#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "DeckPlayer.h"
#include "DjMixer.h"
#include "AudioRecorder.h"
#include "SamplerBank.h"

class AudioEngine : public juce::AudioIODeviceCallback,
                    public juce::ChangeListener,
                    public juce::AsyncUpdater
{
public:
    AudioEngine();
    ~AudioEngine() override;

    bool initialise();
    void shutdown();

    // AudioIODeviceCallback interface
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                          int numInputChannels,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext& context) override;

    // ChangeListener & AsyncUpdater interfaces
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void handleAsyncUpdate() override;

    // Audio device resilience & auto-recovery
    bool ensureAudioDeviceRunning();

    // Deck & Mixer access
    DeckPlayer& getDeck(int deckIndex) { return deckIndex == 0 ? deck1 : deck2; }
    DjMixer& getMixer()                { return djMixer; }
    SamplerBank& getSampler()          { return samplerBank; }
    juce::AudioDeviceManager& getDeviceManager() { return deviceManager; }
    juce::AudioFormatManager& getFormatManager() { return formatManager; }
    AudioRecorder& getRecorder()       { return audioRecorder; }

    // Recording convenience methods
    bool toggleRecording(juce::String& outFilePath);
    bool isRecording() const { return audioRecorder.isRecording(); }
    double getRecordingDuration() const { return audioRecorder.getRecordingDurationSeconds(); }
    juce::File getRecordingDirectory() const;
    void setRecordingDirectory(const juce::File& dir);
    juce::File getDefaultRecordingDirectory() const;
    static juce::File getSettingsFile();

    // Tempo Blend & Crossfader
    void setTempoBlend(bool active);
    bool isTempoBlendActive() const { return tempoBlendActive.load(); }
    // One-shot pitch match: deckIndex takes the current BPM of the other deck.
    // Returns false when the sync is not applicable (other deck unloaded / invalid BPM).
    bool syncDeckTo(int deckIndex);
    void setCrossfader(float val);
    void applyTempoBlend(float crossfaderVal);

    // P3-bis / P3-ter: matematicas puras de alineacion de fase (estaticas y testeables)
    // Error con signo entre dos fases dentro del mismo grid de beat, en [-I/2, +I/2).
    // Positivo = el master va por delante del slave (hay que acelerar el slave).
    static double wrappedPhaseError(double masterPhaseSec, double slavePhaseSec, double interval);

    // Salto minimo SIEMPRE hacia delante que deja la fase del slave sobre la del
    // master: delta = error + (error < 0 ? I : 0). Nunca negativo ni nulo.
    static double quantizedSeekDelta(double errorSec, double interval);

    // Marca el cruce de zona como pendiente; el controlador por bloque lo consume
    // y decide nudge / re-anclaje. masterDeckIndex: 0 = deck1, 1 = deck2.
    void armPhaseAlign(int masterDeckIndex);

    double getSampleRate() const { return currentSampleRate; }
    int getBlockSize() const     { return currentBlockSize; }

    // Audio Output Channel Routing (0 = Ch 1-2, 1 = Ch 3-4)
    void setMasterChannelPair(int pair) { masterChannelPair.store(pair); }
    int getMasterChannelPair() const    { return masterChannelPair.load(); }
    void setCueChannelPair(int pair)    { cueChannelPair.store(pair); }
    int getCueChannelPair() const       { return cueChannelPair.load(); }
    int getNumActiveOutputChannels() const { return currentNumOutputChannels.load(); }
    bool is4ChannelOutputAvailable() const { return currentNumOutputChannels.load() >= 4; }

    // Pitch / Tempo Range
    void setPitchRange(float range) {
        deck1.setPitchRange(range);
        deck2.setPitchRange(range);
    }
    float getPitchRange() const { return deck1.getPitchRange(); }

private:
    juce::AudioDeviceManager deviceManager;
    juce::TimeSliceThread diskReaderThread { "DJ Disk Streaming Thread" };
    juce::AudioFormatManager formatManager;

    DeckPlayer deck1;
    DeckPlayer deck2;
    DjMixer djMixer;
    SamplerBank samplerBank;
    AudioRecorder audioRecorder;

    // Preallocated buffers for real-time safety (no malloc in audio callback)
    juce::AudioBuffer<float> deck1Buffer;
    juce::AudioBuffer<float> deck2Buffer;
    juce::AudioBuffer<float> masterBuffer;
    juce::AudioBuffer<float> cueBuffer;

    double currentSampleRate { 44100.0 };
    int currentBlockSize { 512 };
    std::atomic<int> currentNumOutputChannels { 2 };
    std::atomic<int> masterChannelPair { 0 }; // Pair 0 = Ch 1-2 (Master RCA on DDJ-SB2)
    std::atomic<int> cueChannelPair { 1 };    // Pair 1 = Ch 3-4 (Headphones on DDJ-SB2)

    std::atomic<bool> tempoBlendActive { false };
    std::atomic<float> currentCrossfaderVal { 0.0f };
    enum class CrossfaderZone { Deck1, Center, Deck2 };
    CrossfaderZone lastCrossfaderZone { CrossfaderZone::Center };
    mutable juce::File customRecordDir;

    // P3-ter: controlador de fase por bloque (audio thread). Sin allocs ni locks nuevos.
    enum class PhaseAlignState { Idle, Settle, Measure };
    PhaseAlignState phaseAlignState { PhaseAlignState::Idle };
    int phaseAlignMasterIndexVal { 0 };
    int phaseAlignSettleBlocks { 0 };
    int phaseAlignSeekCount { 0 };
    static constexpr int phaseAlignSettleBlocksCount = 2;
    static constexpr int phaseAlignMaxSeeks = 8;
    static constexpr double phaseAlignDriftRearmSec = 0.025; // ~25 ms para auto re-armado

    bool phaseAlignArmedFlag { false };
    bool phaseNudgeActiveFlag { false };
    float phaseAppliedNudgeVal { 0.0f }; // ultimo bend escrito por el controlador

    void processPhaseAlign();

    std::atomic<bool> isShuttingDown { false };
    std::atomic<bool> isRecoveringDevice { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};

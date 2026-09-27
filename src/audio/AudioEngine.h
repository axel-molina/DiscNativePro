#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "DeckPlayer.h"
#include "DjMixer.h"
#include "AudioRecorder.h"

class AudioEngine : public juce::AudioIODeviceCallback
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

    // Deck & Mixer access
    DeckPlayer& getDeck(int deckIndex) { return deckIndex == 0 ? deck1 : deck2; }
    DjMixer& getMixer()                { return djMixer; }
    juce::AudioDeviceManager& getDeviceManager() { return deviceManager; }
    juce::AudioFormatManager& getFormatManager() { return formatManager; }
    AudioRecorder& getRecorder()       { return audioRecorder; }

    // Recording convenience methods
    bool toggleRecording(juce::String& outFilePath);
    bool isRecording() const { return audioRecorder.isRecording(); }
    double getRecordingDuration() const { return audioRecorder.getRecordingDurationSeconds(); }

    // Tempo Blend & Crossfader
    void setTempoBlend(bool active);
    bool isTempoBlendActive() const { return tempoBlendActive.load(); }
    void setCrossfader(float val);
    void applyTempoBlend(float crossfaderVal);
    void alignBeatPhase(DeckPlayer& masterDeck, DeckPlayer& slaveDeck);

    double getSampleRate() const { return currentSampleRate; }
    int getBlockSize() const     { return currentBlockSize; }

    // Audio Output Channel Routing (0 = Ch 1-2, 1 = Ch 3-4)
    void setMasterChannelPair(int pair) { masterChannelPair.store(pair); }
    int getMasterChannelPair() const    { return masterChannelPair.load(); }
    void setCueChannelPair(int pair)    { cueChannelPair.store(pair); }
    int getCueChannelPair() const       { return cueChannelPair.load(); }
    int getNumActiveOutputChannels() const { return currentNumOutputChannels.load(); }
    bool is4ChannelOutputAvailable() const { return currentNumOutputChannels.load() >= 4; }

private:
    juce::AudioDeviceManager deviceManager;
    juce::TimeSliceThread diskReaderThread { "DJ Disk Streaming Thread" };
    juce::AudioFormatManager formatManager;

    DeckPlayer deck1;
    DeckPlayer deck2;
    DjMixer djMixer;
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

class StretchAdapter; // PIMPL: signalsmith-stretch (pesado) no contamina los TUs que incluyen esto

class DeckPlayer : public juce::AudioSource
{
public:
    DeckPlayer(juce::AudioFormatManager& formatManager, juce::TimeSliceThread& thread);
    ~DeckPlayer() override;

    // Load track from file path using disk streaming
    bool loadFile(const juce::File& audioFile);

    // AudioSource interface
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;

    // Transport controls
    void play();
    void pause();
    void stop();
    void triggerCue();
    void setCuePosition();

    bool isPlaying() const { return playing.load(); }
    bool isLoaded() const  { return loaded.load(); }

    // Navigation & position
    void setPosition(double seconds);
    double getPosition() const;
    double getLengthInSeconds() const { return trackDurationSeconds.load(); }

    // Beat phase (P3)
    double getFirstBeatOffset() const { return firstBeatOffsetSeconds.load(); }
    double getBeatPhase() const;

    // Tempo / Pitch
    void setPitchPercent(float percent); // -8.0 to +8.0, etc.
    float getPitchPercent() const { return pitchPercent.load(); }
    bool isPitchClamped() const { return pitchClamped.load(); }
    void setPitchBend(float bend); // Temporary nudge (e.g. +/- 0.05)
    float getPitchBend() const { return pitchBend.load(); }
    void setPitchRange(float range) { pitchRange.store(juce::jmax(1.0f, range)); }
    float getPitchRange() const     { return pitchRange.load(); }

    // Jog wheel behavior: 0 = Vinyl (center scratch + edge bend), 1 = CDJ (bend all over)
    void setJogMode(int mode) { jogMode.store(mode == 1 ? 1 : 0); }
    int getJogMode() const    { return jogMode.load(); }

    // Key Lock / Master Tempo: pitch cambia tempo sin desafinar (default ON)
    void setKeylock(bool on) { keylock.store(on); updatePlaybackSpeed(); }
    bool isKeylock() const   { return keylock.load(); }

    // Scratch v2: la mano es dueña de la aguja. UI publica velocidad (rev/s
    // con signo); el hilo de audio integra la posición por bloque.
    void beginScratch();
    void endScratch();
    void setScratchVelocity(float revsPerSecond) { scratchVelRevPerSec.store(revsPerSecond); }
    bool isScratching() const { return scratchTouch.load(); }

    // Hot Cues (0 to 3 for 4 hot cues)
    void setHotCue(int index, double positionSeconds);
    void jumpToHotCue(int index);
    void clearHotCue(int index);
    bool hasHotCue(int index) const;
    double getHotCue(int index) const;

    // Loop
    void setLoop(double startSeconds, double lengthSeconds, bool enabled);
    void exitLoop();
    bool isLooping() const { return loopActive.load(); }
    double getLoopStart() const { return loopStartSeconds.load(); }
    double getLoopLength() const { return loopLengthSeconds.load(); }

    // BPM & Beatgrid
    double getBpm() const         { return currentBpm.load(); }
    double getOriginalBpm() const { return originalBpm.load(); }
    void seekRelative(double deltaSeconds);

    // Waveform peak data (3 frequency bands)
    const std::vector<float>& getPeaksLow() const  { return peaksLow; }
    const std::vector<float>& getPeaksMid() const  { return peaksMid; }
    const std::vector<float>& getPeaksHigh() const { return peaksHigh; }
    bool hasWaveformData() const { return !peaksLow.empty(); }

    juce::String getTrackTitle() const;
    juce::String getTrackArtist() const;
    juce::String getTrackKey() const;
    juce::String getTrackCamelot() const;
    juce::Colour getTrackKeyColour() const;
    const juce::File& getCurrentFile() const { return currentFile; }

private:
    void updatePlaybackSpeed();

    juce::AudioFormatManager& formatManager;
    juce::TimeSliceThread& diskThread;

    juce::CriticalSection lock;

    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    std::unique_ptr<juce::BufferingAudioSource> bufferingSource;
    std::unique_ptr<juce::ResamplingAudioSource> resamplingSource;
    std::unique_ptr<StretchAdapter> stretcher; // Key Lock (solo se usa con keylock ON)
    int numSrcChannels { 2 };
    int expectedSamplesPerBlock { 512 };

    juce::File currentFile;
    juce::String trackTitle;
    juce::String trackArtist;
    juce::String trackKey { "--" };
    juce::String trackCamelot { "--" };

    std::atomic<bool> loaded { false };
    std::atomic<bool> playing { false };
    std::atomic<double> trackDurationSeconds { 0.0 };
    std::atomic<double> cuePositionSeconds { 0.0 };
    std::atomic<double> originalBpm { 0.0 };
    std::atomic<double> currentBpm { 0.0 };
    std::atomic<double> firstBeatOffsetSeconds { 0.0 };

    // Seek seguro (sin lock, sin reset en el llamante)
    std::atomic<bool> seekPending { false };
    std::atomic<double> seekTargetSec { 0.0 };
    std::atomic<juce::int64> playbackPosSamples { 0 };

    void applyPendingSeek(double targetSec);

    std::atomic<float> pitchPercent { 0.0f };
    std::atomic<float> pitchBend { 0.0f };
    std::atomic<float> pitchRange { 8.0f };
    std::atomic<bool> pitchClamped { false };
    std::atomic<int> jogMode { 0 };
    std::atomic<bool> keylock { true };

    // Scratch state (atómicos UI<>audio; anclas solo hilo de audio)
    std::atomic<bool> scratchTouch { false };
    std::atomic<float> scratchVelRevPerSec { 0.0f };
    double scratchAnchorSec { 0.0 };
    int scratchDir { 0 }; // +1 adelante, -1 atrás, 0 quieto (solo hilo de audio)
    bool scratchAnchorInit { false };

    // Scratch v3b: anillo de historial (lo último escuchado, device-rate).
    // La reversa lee de aquí: determinista, sin disco ni hilo de fondo.
    static constexpr int scratchRingSize = 768000; // ~8 s estéreo a 48 kHz (~3 MB)
    static constexpr int scratchTailSize = 128;
    static constexpr int scratchXfade = 64;
    juce::AudioBuffer<float> scratchRing;
    juce::AudioBuffer<float> scratchTail;
    int ringWritePos { 0 }; // solo hilo de audio
    int ringValid { 0 };    // samples válidos (solo hilo de audio)
    double revHead { 0.0 }; // cabeza fraccional en samples de anillo (solo hilo de audio)

    void writeScratchRing(const juce::AudioSourceChannelInfo& info);
    float readScratchRing(int ch, double pos) const;

    std::atomic<bool> loopActive { false };
    std::atomic<double> loopStartSeconds { 0.0 };
    std::atomic<double> loopLengthSeconds { 0.0 };

    std::array<std::atomic<double>, 4> hotCues;
    std::array<std::atomic<bool>, 4> hotCuesActive;

    // Los peaks solo se escriben en el hilo de mensajes (los pintores son de
    // mensajes); el worker de análisis nunca toca estos vectores.
    std::vector<float> peaksLow;
    std::vector<float> peaksMid;
    std::vector<float> peaksHigh;

    // P5: análisis en segundo plano. El worker usa su propio reader (un
    // AudioFormatReader no es seguro para lectura concurrente) y publica los
    // resultados en el hilo de mensajes.
    struct AnalysisShared
    {
        std::atomic<bool> alive { true };  // el destructor lo pone a false
        std::atomic<int> generation { 0 }; // ++ en cada loadFile: descarta trabajos viejos
    };

    std::shared_ptr<AnalysisShared> analysis;

    void startAnalysis(std::shared_ptr<juce::AudioFormatReader> analysisReader, int generation);
    static void computeQuickOverview(juce::AudioFormatReader* reader, size_t numPoints,
                                     std::vector<float>& low, std::vector<float>& mid,
                                     std::vector<float>& high);
    static void computeFullOverview(juce::AudioFormatReader* reader, size_t numPoints,
                                    std::vector<float>& low, std::vector<float>& mid,
                                    std::vector<float>& high);
    static void publishIfCurrent(const std::shared_ptr<AnalysisShared>& shared, int generation,
                                 std::function<void()> publishFn);

    double currentSampleRate { 44100.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeckPlayer)
};

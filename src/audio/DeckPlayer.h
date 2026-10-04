#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <array>
#include <atomic>

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

    // Tempo / Pitch
    void setPitchPercent(float percent); // -8.0 to +8.0, etc.
    float getPitchPercent() const { return pitchPercent.load(); }
    void setPitchBend(float bend); // Temporary nudge (e.g. +/- 0.05)
    void setPitchRange(float range) { pitchRange.store(juce::jmax(1.0f, range)); }
    float getPitchRange() const     { return pitchRange.load(); }

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

    // File info
    juce::String getTrackTitle() const;
    juce::String getTrackArtist() const;
    const juce::File& getCurrentFile() const { return currentFile; }

private:
    void updatePlaybackSpeed();

    juce::AudioFormatManager& formatManager;
    juce::TimeSliceThread& diskThread;

    juce::CriticalSection lock;

    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    std::unique_ptr<juce::BufferingAudioSource> bufferingSource;
    std::unique_ptr<juce::ResamplingAudioSource> resamplingSource;

    juce::File currentFile;
    juce::String trackTitle;
    juce::String trackArtist;

    std::atomic<bool> loaded { false };
    std::atomic<bool> playing { false };
    std::atomic<double> trackDurationSeconds { 0.0 };
    std::atomic<double> cuePositionSeconds { 0.0 };
    std::atomic<double> originalBpm { 124.0 };
    std::atomic<double> currentBpm { 124.0 };

    std::atomic<float> pitchPercent { 0.0f };
    std::atomic<float> pitchBend { 0.0f };
    std::atomic<float> pitchRange { 8.0f };

    std::atomic<bool> loopActive { false };
    std::atomic<double> loopStartSeconds { 0.0 };
    std::atomic<double> loopLengthSeconds { 0.0 };

    std::array<std::atomic<double>, 4> hotCues;
    std::array<std::atomic<bool>, 4> hotCuesActive;

    std::vector<float> peaksLow;
    std::vector<float> peaksMid;
    std::vector<float> peaksHigh;
    void extractWaveformOverview(juce::AudioFormatReader* reader);

    double currentSampleRate { 44100.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeckPlayer)
};

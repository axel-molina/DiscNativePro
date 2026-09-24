#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <atomic>

class AudioRecorder : public juce::Thread
{
public:
    AudioRecorder();
    ~AudioRecorder() override;

    bool startRecording(const juce::File& file, double sampleRate, int numChannels = 2);
    void stopRecording();

    bool isRecording() const { return recording.load(std::memory_order_relaxed); }
    double getRecordingDurationSeconds() const { return durationSeconds.load(std::memory_order_relaxed); }
    juce::File getCurrentFile() const { return currentRecordFile; }

    // Real-time safe: pushes audio into lock-free ring buffer (no memory allocation or file I/O)
    void pushAudioBlock(const juce::AudioBuffer<float>& buffer, int numSamples);

    void run() override;

private:
    void flushRemainingBuffer();

    static constexpr int fifoSize = 65536; // ~1.5 sec audio buffer at 44.1kHz
    juce::AbstractFifo fifo { fifoSize };
    juce::AudioBuffer<float> fifoBuffer;

    std::unique_ptr<juce::AudioFormatWriter> writer;
    juce::File currentRecordFile;
    std::atomic<bool> recording { false };
    std::atomic<double> durationSeconds { 0.0 };
    std::atomic<int64_t> totalRecordedSamples { 0 };
    double currentSampleRate { 44100.0 };
    int channels { 2 };

    juce::WaitableEvent wakeEvent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioRecorder)
};

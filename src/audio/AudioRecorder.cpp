#include "AudioRecorder.h"

AudioRecorder::AudioRecorder()
    : juce::Thread("DiscPro Audio Recorder Thread")
{
    fifoBuffer.setSize(2, fifoSize);
}

AudioRecorder::~AudioRecorder()
{
    stopRecording();
}

bool AudioRecorder::startRecording(const juce::File& file, double sampleRate, int numChannels)
{
    stopRecording();

    currentRecordFile = file;
    currentSampleRate = sampleRate;
    channels = numChannels;
    totalRecordedSamples.store(0);
    durationSeconds.store(0.0);

    if (currentRecordFile.exists())
        currentRecordFile.deleteFile();

    auto outStream = currentRecordFile.createOutputStream();
    if (outStream == nullptr)
        return false;

    juce::WavAudioFormat wavFormat;
    writer.reset(wavFormat.createWriterFor(outStream.release(),
                                           sampleRate,
                                           (unsigned int)channels,
                                           24, // 24-bit high quality
                                           {},
                                           0));

    if (writer == nullptr)
        return false;

    fifo.reset();
    fifoBuffer.clear();
    recording.store(true);

    startThread(juce::Thread::Priority::normal);
    return true;
}

void AudioRecorder::stopRecording()
{
    if (recording.exchange(false))
    {
        wakeEvent.signal();
        stopThread(3000);
        flushRemainingBuffer();
        writer.reset();
    }
}

void AudioRecorder::pushAudioBlock(const juce::AudioBuffer<float>& buffer, int numSamples)
{
    if (!recording.load(std::memory_order_relaxed) || numSamples <= 0)
        return;

    int start1, size1, start2, size2;
    fifo.prepareToWrite(numSamples, start1, size1, start2, size2);

    int numChannelsToCopy = juce::jmin(channels, buffer.getNumChannels());

    if (size1 > 0)
    {
        for (int ch = 0; ch < numChannelsToCopy; ++ch)
            fifoBuffer.copyFrom(ch, start1, buffer, ch, 0, size1);
    }

    if (size2 > 0)
    {
        for (int ch = 0; ch < numChannelsToCopy; ++ch)
            fifoBuffer.copyFrom(ch, start2, buffer, ch, size1, size2);
    }

    fifo.finishedWrite(size1 + size2);

    int64_t total = totalRecordedSamples.fetch_add(numSamples, std::memory_order_relaxed) + numSamples;
    durationSeconds.store((double)total / currentSampleRate, std::memory_order_relaxed);

    wakeEvent.signal();
}

void AudioRecorder::run()
{
    juce::AudioBuffer<float> tempBuffer(channels, 2048);

    while (!threadShouldExit())
    {
        wakeEvent.wait(20);

        int numReady = fifo.getNumReady();
        while (numReady > 0 && !threadShouldExit())
        {
            int blockToRead = juce::jmin(numReady, tempBuffer.getNumSamples());
            int start1, size1, start2, size2;
            fifo.prepareToRead(blockToRead, start1, size1, start2, size2);

            if (size1 > 0)
            {
                for (int ch = 0; ch < channels; ++ch)
                    tempBuffer.copyFrom(ch, 0, fifoBuffer, ch, start1, size1);
            }

            if (size2 > 0)
            {
                for (int ch = 0; ch < channels; ++ch)
                    tempBuffer.copyFrom(ch, size1, fifoBuffer, ch, start2, size2);
            }

            fifo.finishedRead(size1 + size2);

            if (writer != nullptr && (size1 + size2) > 0)
            {
                writer->writeFromAudioSampleBuffer(tempBuffer, 0, size1 + size2);
            }

            numReady = fifo.getNumReady();
        }
    }
}

void AudioRecorder::flushRemainingBuffer()
{
    int numReady = fifo.getNumReady();
    if (numReady <= 0 || writer == nullptr)
        return;

    juce::AudioBuffer<float> temp(channels, numReady);
    int start1, size1, start2, size2;
    fifo.prepareToRead(numReady, start1, size1, start2, size2);

    if (size1 > 0)
    {
        for (int ch = 0; ch < channels; ++ch)
            temp.copyFrom(ch, 0, fifoBuffer, ch, start1, size1);
    }
    if (size2 > 0)
    {
        for (int ch = 0; ch < channels; ++ch)
            temp.copyFrom(ch, size1, fifoBuffer, ch, start2, size2);
    }

    fifo.finishedRead(size1 + size2);
    writer->writeFromAudioSampleBuffer(temp, 0, size1 + size2);
}

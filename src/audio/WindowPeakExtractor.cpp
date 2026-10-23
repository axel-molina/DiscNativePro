#include "WindowPeakExtractor.h"

WindowPeakExtractor::WindowPeakExtractor()
{
    formatManager.registerBasicFormats();
}

bool WindowPeakExtractor::open(const juce::File& audioFile)
{
    close();
    reader.reset(formatManager.createReaderFor(audioFile));
    if (reader != nullptr && reader->sampleRate > 0.0)
        bucketSec = (double)samplesPerBucket / reader->sampleRate;
    return reader != nullptr;
}

void WindowPeakExtractor::close()
{
    reader.reset();
    peaks.clear();
    averages.clear();
    dataStart = 0.0;
    dataEnd = 0.0;
}

void WindowPeakExtractor::ensureWindow(double centerSec, double halfWindowSec)
{
    if (reader == nullptr)
        return;

    const double wantStart = centerSec - halfWindowSec;
    const double wantEnd = centerSec + halfWindowSec;
    if (hasData() && wantStart >= dataStart && wantEnd <= dataEnd)
        return;

    const double margin = halfWindowSec + cacheMarginSec;
    extract(centerSec - margin, margin * 2.0);
}

void WindowPeakExtractor::extract(double startSec, double numSecs)
{
    peaks.clear();
    averages.clear();
    dataStart = 0.0;
    dataEnd = 0.0;

    if (reader == nullptr || reader->sampleRate <= 0.0)
        return;

    const double sampleRate = reader->sampleRate;
    const int64_t totalSamples = reader->lengthInSamples;

    int64_t startSample = (int64_t) juce::jmax (0.0, startSec * sampleRate);
    int64_t numSamples = (int64_t) (numSecs * sampleRate);
    if (startSample >= totalSamples || numSamples <= 0)
        return;

    numSamples = juce::jmin (numSamples, totalSamples - startSample);
    const int numBuckets = (int) ((numSamples + samplesPerBucket - 1) / samplesPerBucket);
    if (numBuckets <= 0)
        return;

    peaks.assign ((size_t) numBuckets, 0.0f);
    averages.assign ((size_t) numBuckets, 0.0f);
    std::vector<double> sums ((size_t) numBuckets, 0.0);
    std::vector<int64_t> counts ((size_t) numBuckets, 0);

    const int numCh = juce::jmax (1, (int) reader->numChannels);
    const int chunkSize = 65536;
    juce::AudioBuffer<float> chunkBuf (numCh, chunkSize);

    int64_t pos = startSample;
    while (pos < startSample + numSamples)
    {
        const int n = (int) juce::jmin<int64_t> ((int64_t) chunkSize, startSample + numSamples - pos);
        if (! reader->read (&chunkBuf, 0, n, pos, true, true))
            break;

        const float* left = chunkBuf.getReadPointer (0);
        const float* right = (numCh > 1) ? chunkBuf.getReadPointer (1) : nullptr;

        for (int i = 0; i < n; ++i)
        {
            float v = std::abs (left[i]);
            if (right != nullptr)
                v = (v + std::abs (right[i])) * 0.5f;

            const size_t b = (size_t) ((pos - startSample + i) / samplesPerBucket);
            if (b >= (size_t) numBuckets)
                break;

            if (v > peaks[b])
                peaks[b] = v;
            sums[b] += (double) v;
            counts[b]++;
        }
        pos += n;
    }

    for (size_t b = 0; b < (size_t) numBuckets; ++b)
        averages[b] = counts[b] > 0 ? (float) (sums[b] / (double) counts[b]) : 0.0f;

    // Normalización por ventana con suelo: los picos finos de pistas
    // comprimidas conservan sus diferencias sin despeinar las partes suaves.
    float maxPeak = 1.0e-6f;
    for (float v : peaks)
        maxPeak = juce::jmax (maxPeak, v);
    const float scale = 1.0f / juce::jmax (maxPeak, 0.5f);
    for (size_t b = 0; b < (size_t) numBuckets; ++b)
    {
        peaks[b] *= scale;
        averages[b] *= scale;
    }

    dataStart = (double) startSample / sampleRate;
    dataEnd = dataStart + (double) numSamples / sampleRate;
}

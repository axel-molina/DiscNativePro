#include "BeatDetector.h"
#include <cmath>
#include <vector>
#include <map>
#include <numeric>

BeatAnalysisResult BeatDetector::analyze(juce::AudioFormatReader* reader)
{
    BeatAnalysisResult defaultRes;
    if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate <= 0.0)
        return defaultRes;

    // Analyze up to 60 seconds
    const double maxDuration = 60.0;
    int64_t maxSamples = std::min((int64_t)(maxDuration * reader->sampleRate), reader->lengthInSamples);

    int downsampleFactor = std::max(1, (int)(reader->sampleRate / 4000.0));
    double effectiveRate = reader->sampleRate / (double)downsampleFactor;
    int downsampledLength = (int)(maxSamples / downsampleFactor);
    if (downsampledLength <= 100)
        return defaultRes;

    std::vector<float> downsampled(static_cast<size_t>(downsampledLength), 0.0f);

    // Read audio in blocks of 4096 samples
    const int readChunkSize = 4096;
    juce::AudioBuffer<float> readBuffer(1, readChunkSize);
    int64_t samplesReadTotal = 0;

    while (samplesReadTotal < maxSamples)
    {
        int toRead = (int)std::min((int64_t)readChunkSize, maxSamples - samplesReadTotal);
        readBuffer.clear();
        reader->read(&readBuffer, 0, toRead, samplesReadTotal, true, false);

        const float* raw = readBuffer.getReadPointer(0);
        for (int i = 0; i < toRead; ++i)
        {
            int64_t globalSample = samplesReadTotal + i;
            size_t downIndex = static_cast<size_t>(globalSample / downsampleFactor);
            if (downIndex < downsampled.size())
            {
                downsampled[downIndex] += std::abs(raw[i]);
            }
        }
        samplesReadTotal += toRead;
    }

    for (auto& val : downsampled)
    {
        val /= (float)downsampleFactor;
    }

    // Moving average filter (100ms window) for kick transient energy
    int windowSize = std::max(1, (int)(effectiveRate * 0.1));
    std::vector<float> energy(downsampled.size(), 0.0f);
    float movingSum = 0.0f;

    for (size_t i = 0; i < downsampled.size(); ++i)
    {
        movingSum += downsampled[i];
        if (i >= static_cast<size_t>(windowSize))
        {
            movingSum -= downsampled[i - static_cast<size_t>(windowSize)];
        }
        energy[i] = movingSum / (float)windowSize;
    }

    // Dynamic threshold calculation
    float sumEnergy = std::accumulate(energy.begin(), energy.end(), 0.0f);
    float meanEnergy = sumEnergy / (float)energy.size();
    float threshold = meanEnergy * 1.35f;

    // Detect peaks with minimum distance (200 BPM -> min interval ~0.25s)
    std::vector<int> peaks;
    int minPeakDistance = std::max(1, (int)(effectiveRate * 0.25));

    for (size_t i = 1; i + 1 < energy.size(); ++i)
    {
        if (energy[i] > threshold && energy[i] > energy[i - 1] && energy[i] > energy[i + 1])
        {
            if (peaks.empty() || (static_cast<int>(i) - peaks.back() > minPeakDistance))
            {
                peaks.push_back(static_cast<int>(i));
            }
        }
    }

    if (peaks.size() < 2)
        return defaultRes;

    // Calculate BPM intervals
    std::map<int, int> bpmHistogram;
    for (size_t i = 0; i + 1 < peaks.size(); ++i)
    {
        for (size_t j = 1; j <= 4 && (i + j) < peaks.size(); ++j)
        {
            double diffSec = (peaks[i + j] - peaks[i]) / effectiveRate / (double)j;
            if (diffSec > 0.0)
            {
                double candidate = 60.0 / diffSec;
                if (candidate >= 70.0 && candidate <= 180.0)
                {
                    int rounded = (int)std::round(candidate);
                    bpmHistogram[rounded]++;
                }
                else if (candidate * 2.0 >= 70.0 && candidate * 2.0 <= 180.0)
                {
                    int rounded = (int)std::round(candidate * 2.0);
                    bpmHistogram[rounded]++;
                }
            }
        }
    }

    int bestBpm = 124;
    int maxCount = 0;
    for (const auto& [candidateBpm, count] : bpmHistogram)
    {
        if (count > maxCount)
        {
            maxCount = count;
            bestBpm = candidateBpm;
        }
    }

    BeatAnalysisResult result;
    result.bpm = static_cast<double>(bestBpm);
    result.beatInterval = 60.0 / result.bpm;
    double firstPeakSec = peaks[0] / effectiveRate;
    result.offset = std::fmod(firstPeakSec, result.beatInterval);

    return result;
}

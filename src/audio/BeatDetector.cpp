#include "BeatDetector.h"
#include <cmath>
#include <vector>
#include <map>
#include <numeric>
#include <algorithm>

BeatAnalysisResult BeatDetector::analyze(juce::AudioFormatReader* reader)
{
    BeatAnalysisResult defaultRes;
    defaultRes.bpm = 0.0;
    defaultRes.offset = 0.0;
    defaultRes.beatInterval = 0.0;
    if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate <= 0.0)
        return defaultRes;

    // Analyze up to 60 seconds
    const double maxDuration = 60.0;
    int64_t maxSamples = std::min<int64_t>(static_cast<int64_t>(maxDuration * reader->sampleRate),
                                           static_cast<int64_t>(reader->lengthInSamples));

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
        int toRead = static_cast<int>(std::min<int64_t>(readChunkSize, maxSamples - samplesReadTotal));
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
    std::map<long long, int> bpmHistogram;
    for (size_t i = 0; i + 1 < peaks.size(); ++i)
    {
        for (size_t j = 1; j <= 4 && (i + j) < peaks.size(); ++j)
        {
            double diffSec = (peaks[i + j] - peaks[i]) / effectiveRate / (double)j;
            if (diffSec > 0.0)
            {
                double candidate = 60.0 / diffSec;
                // Normalize candidate tempo into canonical DJ dance range [70.0, 180.0]
                while (candidate > 0.0 && candidate < 70.0)
                    candidate *= 2.0;
                while (candidate > 180.0)
                    candidate /= 2.0;

                if (candidate >= 70.0 && candidate <= 180.0)
                {
                    long long bucket = std::llround(candidate * 10.0);
                    if (bucket >= 700 && bucket <= 1800)
                        bpmHistogram[bucket]++;
                }
            }
        }
    }

    if (bpmHistogram.empty())
        return defaultRes;

    long long bestBucket = 1240;
    int maxCount = 0;
    for (const auto& [bucketVal, count] : bpmHistogram)
    {
        if (count > maxCount)
        {
            maxCount = count;
            bestBucket = bucketVal;
        }
    }

    if (maxCount < 4)
        return defaultRes;

    // Parabolic refinement over neighbors
    auto getCount = [&bpmHistogram](long long b) -> int {
        auto it = bpmHistogram.find(b);
        if (it == bpmHistogram.end()) return 0;
        return it->second;
    };
    int cM1 = getCount(bestBucket - 1);
    int c0  = getCount(bestBucket);
    int cP1 = getCount(bestBucket + 1);
    double refinedBpm = (double) bestBucket / 10.0;
    if (cM1 + cP1 - 2 * c0 != 0)
    {
        // p = 0.5*(cM1 - cP1)/(cM1 - 2*c0 + cP1)
        double p = 0.5 * ((double)cM1 - (double)cP1) / ((double)cM1 - 2.0 * (double)c0 + (double)cP1);
        if (p >= -0.5 && p <= 0.5)
            refinedBpm = ((double) bestBucket + p) / 10.0;
    }

    BeatAnalysisResult result;
    result.bpm = refinedBpm;
    if (result.bpm <= 0.0)
        return defaultRes;
    result.beatInterval = 60.0 / result.bpm;
    double firstPeakSec = peaks[0] / effectiveRate;
    result.offset = std::fmod(firstPeakSec, result.beatInterval);
    if (result.offset < 0.0) result.offset += result.beatInterval;

    return result;
}

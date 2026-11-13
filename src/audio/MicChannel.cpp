#include "MicChannel.h"

#include <cmath>

namespace
{
    constexpr double lowCutHz = 100.0;
    constexpr double duckAttackSec = 0.08;   // bajada de la musica
    constexpr double duckReleaseSec = 0.8;   // vuelta de la musica
    constexpr double duckHoldSec = 0.3;      // hold tras dejar de hablar
    constexpr float voiceOnHysteresisDb = 1.5f; // margen para salir de voz activa
    constexpr float idleReleaseCoef = 0.02f;    // suavizado cuando el canal esta apagado
}

void MicChannel::prepare(double sampleRate, int /*maxBlockSamples*/)
{
    if (sampleRate <= 0.0)
        sampleRate = 44100.0;

    currentSampleRate = sampleRate;
    computeLowCutCoefficients(sampleRate);

    // Bypass: filtro identidad (b0 = 1, resto 0).
    bypass = Biquad();

    holdSamples = (int) std::lround(duckHoldSec * sampleRate);
    holdRemainingSamples = 0;

    duckGain.store(1.0f, std::memory_order_relaxed);
    micLevel.store(0.0f, std::memory_order_relaxed);
    voiceActive.store(false, std::memory_order_relaxed);
}

void MicChannel::computeLowCutCoefficients(double sampleRate)
{
    // High-pass RBJ de 2o orden (12 dB/oct), Q Butterworth.
    const double w0 = juce::MathConstants<double>::twoPi * lowCutHz / sampleRate;
    const double cosW0 = std::cos(w0);
    const double alpha = std::sin(w0) / (2.0 * 0.7071067811865476);
    const double a0 = 1.0 + alpha;

    lowCut.b0 = (float) ((1.0 + cosW0) / 2.0 / a0);
    lowCut.b1 = (float) (-(1.0 + cosW0) / a0);
    lowCut.b2 = lowCut.b0;
    lowCut.a1 = (float) (-2.0 * cosW0 / a0);
    lowCut.a2 = (float) ((1.0 - alpha) / a0);
}

void MicChannel::process(const float* const* inputChannels, int numInputChannels,
                         int numSamples, juce::AudioBuffer<float>& masterBuffer)
{
    const bool on = enabled.load(std::memory_order_relaxed);
    const int numCh = juce::jlimit(0, maxChannels, numInputChannels);

    if (!on || numCh == 0 || numSamples <= 0)
    {
        // Apagado o sin entrada: decaer el VU y soltar el duck suavemente.
        micLevel.store(micLevel.load(std::memory_order_relaxed) * 0.8f,
                       std::memory_order_relaxed);
        voiceActive.store(false, std::memory_order_relaxed);
        holdRemainingSamples = 0;

        float g = duckGain.load(std::memory_order_relaxed);
        if (g != 1.0f)
        {
            const float release = (duckReleaseCoef > 0.0f) ? duckReleaseCoef : idleReleaseCoef;
            duckGain.store(g + (1.0f - g) * release, std::memory_order_relaxed);
        }
        return;
    }

    const float gain = juce::jlimit(-60.0f, 24.0f, gainDb.load(std::memory_order_relaxed));
    const float gainLin = juce::Decibels::decibelsToGain(gain);
    Biquad& f = lowCutEnabled.load(std::memory_order_relaxed) ? lowCut : bypass;

    const int numMasterCh = juce::jmin(2, masterBuffer.getNumChannels());

    // Low-cut + gain + suma al master, con RMS de la senal filtrada para el detector.
    double sumSquares = 0.0;
    size_t counted = 0;
    float peak = 0.0f;

    for (int inCh = 0; inCh < numCh; ++inCh)
    {
        if (inputChannels[inCh] == nullptr)
            continue;

        const int firstOut = (numCh == 1) ? 0 : inCh;
        const int lastOut = (numCh == 1) ? (numMasterCh - 1) : inCh;

        auto* src = inputChannels[inCh];
        const float b0 = f.b0, b1 = f.b1, b2 = f.b2, a1 = f.a1, a2 = f.a2;
        float x1 = f.x1[inCh], x2 = f.x2[inCh], y1 = f.y1[inCh], y2 = f.y2[inCh];

        for (int i = 0; i < numSamples; ++i)
        {
            const float x = src[i];
            const float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x;
            y2 = y1; y1 = y;

            const float v = y * gainLin;

            for (int outCh = firstOut; outCh <= lastOut; ++outCh)
                masterBuffer.getWritePointer(outCh)[i] += v;

            sumSquares += (double) y * (double) y;
            ++counted;
            peak = juce::jmax(peak, std::abs(v));
        }

        f.x1[inCh] = x1; f.x2[inCh] = x2;
        f.y1[inCh] = y1; f.y2[inCh] = y2;
    }

    // Detector de voz: RMS del bloque contra el umbral, con hold e histeresis.
    const float rms = (counted > 0)
                          ? (float) std::sqrt(sumSquares / (double) counted)
                          : 0.0f;
    const float rmsDb = juce::Decibels::gainToDecibels(rms, -90.0f);

    const float threshold = duckThresholdDb.load(std::memory_order_relaxed);
    bool voice = voiceActive.load(std::memory_order_relaxed);

    if (rmsDb >= threshold)
    {
        voice = true;
        holdRemainingSamples = holdSamples;
    }
    else if (voice)
    {
        if (holdRemainingSamples > 0)
        {
            holdRemainingSamples -= numSamples;
        }
        else if (rmsDb < threshold - voiceOnHysteresisDb)
        {
            voice = false;
        }
    }
    voiceActive.store(voice, std::memory_order_relaxed);

    // Duck objetivo y suavizado (ataque rapido, release lento, por bloque).
    const float target = (voice && duckEnabled.load(std::memory_order_relaxed))
                             ? juce::Decibels::decibelsToGain(
                                 juce::jlimit(-60.0f, 0.0f,
                                              duckDepthDb.load(std::memory_order_relaxed)))
                             : 1.0f;

    const float blockSec = (float) ((double) numSamples / currentSampleRate);
    duckAttackCoef = 1.0f - std::exp(-blockSec / (float) duckAttackSec);
    duckReleaseCoef = 1.0f - std::exp(-blockSec / (float) duckReleaseSec);

    float g = duckGain.load(std::memory_order_relaxed);
    g += (target - g) * (target < g ? duckAttackCoef : duckReleaseCoef);
    duckGain.store(g, std::memory_order_relaxed);

    // VU: pico con decaimiento suave.
    float lvl = micLevel.load(std::memory_order_relaxed);
    lvl = juce::jmax(peak, lvl * 0.92f);
    micLevel.store(lvl, std::memory_order_relaxed);
}

#include "MasterLimiter.h"

#include <cmath>

namespace
{
    constexpr double lookaheadSec = 0.005;   // retardo de senal (ventana de pre-accion)
    constexpr double attackSec = 0.0005;     // bajada del gain (por muestra)
    constexpr double releaseSec = 0.1;       // vuelta del gain (por muestra)
    constexpr float grDecayPerBlock = 0.82f; // decaimiento del meter de GR por bloque
    constexpr float lufsSilenceFloor = -70.0f;
    constexpr float lufsOffsetDb = -0.691f;  // offset de calibracion BS.1770

    // Parametros exactos del K-weighting (BS.1770-4 / ITU-R BS.1770).
    constexpr double shelfFreqHz = 1681.9744509555319;
    constexpr double shelfGainDb = 3.999843853973347;
    constexpr double shelfQ = 0.7071752369554196;
    constexpr double highpassFreqHz = 38.13547087602444;
    constexpr double highpassQ = 0.5003270373238773;
}

void MasterLimiter::prepare(double sampleRate, int /*maxBlockSamples*/)
{
    if (sampleRate <= 0.0)
        sampleRate = 44100.0;

    currentSampleRate = sampleRate;

    // Look-ahead: D muestras de retardo por canal.
    delayLength = (int) std::ceil(lookaheadSec * sampleRate);
    for (int ch = 0; ch < maxChannels; ++ch)
    {
        delay[ch].assign((size_t) delayLength, 0.0f);
    }
    delayPos = 0;

    computeKWeightingCoefficients(sampleRate);

    // Ring buffers del medidor LUFS (preasignados fuera del callback).
    ringShortSize = juce::jmax(1, (int) std::lround(3.0 * sampleRate));
    ringMomentSize = juce::jmax(1, (int) std::lround(0.4 * sampleRate));
    ringShort.assign((size_t) ringShortSize, 0.0f);
    ringMoment.assign((size_t) ringMomentSize, 0.0f);
    ringShortPos = 0;
    ringMomentPos = 0;
    sumShort = 0.0;
    sumMoment = 0.0;

    // Coeficientes de suavizado POR MUESTRA del gain computer.
    const double sampleSec = 1.0 / sampleRate;
    attackCoef = (float) (1.0 - std::exp(-sampleSec / attackSec));
    releaseCoef = (float) (1.0 - std::exp(-sampleSec / releaseSec));

    // Ventana deslizante de minimo y envolvente de gain, reseteados.
    winCapacity = delayLength + 2;
    winReq.assign((size_t) winCapacity, 1.0f);
    winIdx.assign((size_t) winCapacity, 0);
    winHead = 0;
    winCount = 0;
    sampleCounter = 0;
    gainEnvelope = 1.0f;

    gainReductionDb.store(0.0f, std::memory_order_relaxed);
    lufsMomentary.store(lufsSilenceFloor, std::memory_order_relaxed);
    lufsShortTerm.store(lufsSilenceFloor, std::memory_order_relaxed);
}

void MasterLimiter::computeKWeightingCoefficients(double sampleRate)
{
    // Etapa 1: high-shelf (+4 dB @ ~1682 Hz).
    {
        const double A = std::pow(10.0, shelfGainDb / 40.0);
        const double w0 = juce::MathConstants<double>::twoPi * shelfFreqHz / sampleRate;
        const double cosW0 = std::cos(w0);
        const double alpha = std::sin(w0) / (2.0 * shelfQ);
        const double sqrtAalpha = 2.0 * std::sqrt(A) * alpha;
        const double a0 = (A + 1.0) - (A - 1.0) * cosW0 + sqrtAalpha;

        shelf.b0 = (float) (A * ((A + 1.0) + (A - 1.0) * cosW0 + sqrtAalpha) / a0);
        shelf.b1 = (float) (-2.0 * A * ((A - 1.0) + (A + 1.0) * cosW0) / a0);
        shelf.b2 = (float) (A * ((A + 1.0) + (A - 1.0) * cosW0 - sqrtAalpha) / a0);
        shelf.a1 = (float) (2.0 * ((A - 1.0) - (A + 1.0) * cosW0) / a0);
        shelf.a2 = (float) (((A + 1.0) - (A - 1.0) * cosW0 - sqrtAalpha) / a0);
    }

    // Etapa 2: high-pass (~38 Hz, Q ~0.5).
    {
        const double w0 = juce::MathConstants<double>::twoPi * highpassFreqHz / sampleRate;
        const double cosW0 = std::cos(w0);
        const double alpha = std::sin(w0) / (2.0 * highpassQ);
        const double a0 = 1.0 + alpha;

        highpass.b0 = (float) ((1.0 + cosW0) / 2.0 / a0);
        highpass.b1 = (float) (-(1.0 + cosW0) / a0);
        highpass.b2 = highpass.b0;
        highpass.a1 = (float) (-2.0 * cosW0 / a0);
        highpass.a2 = (float) ((1.0 - alpha) / a0);
    }
}

void MasterLimiter::process(juce::AudioBuffer<float>& buffer, int numSamples)
{
    const int numCh = juce::jmin(maxChannels, buffer.getNumChannels());
    if (numCh == 0 || numSamples <= 0 || numSamples > buffer.getNumSamples())
        return;

    const float ceilingLin = juce::Decibels::decibelsToGain(
        juce::jlimit(-12.0f, 0.0f, ceilingDb.load(std::memory_order_relaxed)));

    auto* bufL = buffer.getWritePointer(0);
    auto* bufR = (numCh > 1) ? buffer.getWritePointer(1) : nullptr;

    // Biquads del K-weighting (estado local, se escribe de vuelta al final).
    const float sB0 = shelf.b0, sB1 = shelf.b1, sB2 = shelf.b2, sA1 = shelf.a1, sA2 = shelf.a2;
    const float hB0 = highpass.b0, hB1 = highpass.b1, hB2 = highpass.b2,
                hA1 = highpass.a1, hA2 = highpass.a2;
    float sx1[maxChannels] = { shelf.x1[0], shelf.x1[1] };
    float sx2[maxChannels] = { shelf.x2[0], shelf.x2[1] };
    float sy1[maxChannels] = { shelf.y1[0], shelf.y1[1] };
    float sy2[maxChannels] = { shelf.y2[0], shelf.y2[1] };
    float hx1[maxChannels] = { highpass.x1[0], highpass.x1[1] };
    float hx2[maxChannels] = { highpass.x2[0], highpass.x2[1] };
    float hy1[maxChannels] = { highpass.y1[0], highpass.y1[1] };
    float hy2[maxChannels] = { highpass.y2[0], highpass.y2[1] };

    float blockMaxGrDb = 0.0f;
    const int winCap = winCapacity;

    for (int i = 0; i < numSamples; ++i, ++sampleCounter)
    {
        const float xL = bufL[i];
        const float xR = (bufR != nullptr) ? bufR[i] : xL;

        // --- Gain computer sobre la senal SIN retardar ---
        const float peak = juce::jmax(std::abs(xL), std::abs(xR));
        const float req = (peak > ceilingLin) ? ceilingLin / peak : 1.0f;

        // Ventana deslizante de minimo sobre req (deque monotonicO): el minimo
        // de las ultimas delayLength+1 muestras es el objetivo del envolvente,
        // de modo que el pico ya fue "visto" cuando emerge del retardo.
        while (winCount > 0 && winIdx[(size_t) winHead] < sampleCounter - delayLength)
        {
            winHead = (winHead + 1) % winCap;
            --winCount;
        }
        while (winCount > 0 && winReq[(size_t) ((winHead + winCount - 1) % winCap)] >= req)
            --winCount;
        const int tail = (winHead + winCount) % winCap;
        winReq[(size_t) tail] = req;
        winIdx[(size_t) tail] = sampleCounter;
        ++winCount;

        const float reqMin = winReq[(size_t) winHead];
        gainEnvelope += (reqMin - gainEnvelope)
                        * ((reqMin < gainEnvelope) ? attackCoef : releaseCoef);
        const float env = gainEnvelope;

        if (env < 1.0f)
        {
            const float grDb = -20.0f * std::log10(juce::jmax(env, 1.0e-6f));
            blockMaxGrDb = juce::jmax(blockMaxGrDb, grDb);
        }

        // --- Salida: muestra retardada (look-ahead) escalada por el envolvente ---
        float outL, outR;
        if (bufR != nullptr)
        {
            outL = delay[0][(size_t) delayPos];
            outR = delay[1][(size_t) delayPos];
            delay[0][(size_t) delayPos] = xL;
            delay[1][(size_t) delayPos] = xR;
            bufL[i] = outL * env;
            bufR[i] = outR * env;
        }
        else
        {
            outL = delay[0][(size_t) delayPos];
            delay[0][(size_t) delayPos] = xL;
            bufL[i] = outL * env;
        }
        if (++delayPos >= delayLength)
            delayPos = 0;

        // --- Medidor LUFS: K-weighting por canal y cuadrados al ring ---
        double sqSum = 0.0;
        for (int ch = 0; ch < numCh; ++ch)
        {
            const float x = (ch == 0) ? xL : xR;
            const float ys = sB0 * x + sB1 * sx1[ch] + sB2 * sx2[ch] - sA1 * sy1[ch] - sA2 * sy2[ch];
            sx2[ch] = sx1[ch]; sx1[ch] = x;
            sy2[ch] = sy1[ch]; sy1[ch] = ys;

            const float yh = hB0 * ys + hB1 * hx1[ch] + hB2 * hx2[ch] - hA1 * hy1[ch] - hA2 * hy2[ch];
            hx2[ch] = hx1[ch]; hx1[ch] = ys;
            hy2[ch] = hy1[ch]; hy1[ch] = yh;

            sqSum += (double) yh * (double) yh;
        }

        sumShort += sqSum - (double) ringShort[(size_t) ringShortPos];
        ringShort[(size_t) ringShortPos] = (float) sqSum;
        if (++ringShortPos >= ringShortSize)
            ringShortPos = 0;

        sumMoment += sqSum - (double) ringMoment[(size_t) ringMomentPos];
        ringMoment[(size_t) ringMomentPos] = (float) sqSum;
        if (++ringMomentPos >= ringMomentSize)
            ringMomentPos = 0;
    }

    // Devolver el estado de los biquads.
    for (int ch = 0; ch < numCh; ++ch)
    {
        shelf.x1[ch] = sx1[ch]; shelf.x2[ch] = sx2[ch];
        shelf.y1[ch] = sy1[ch]; shelf.y2[ch] = sy2[ch];
        highpass.x1[ch] = hx1[ch]; highpass.x2[ch] = hx2[ch];
        highpass.y1[ch] = hy1[ch]; highpass.y2[ch] = hy2[ch];
    }

    // Publicar GR (con decaimiento suave para el meter).
    gainReductionDb.store(juce::jmax(blockMaxGrDb,
                                     gainReductionDb.load(std::memory_order_relaxed) * grDecayPerBlock),
                          std::memory_order_relaxed);

    // Publicar LUFS: mean square ponderado por canal (G = 1.0 para L/R).
    const float lufsM = (sumMoment > 0.0)
                            ? lufsOffsetDb + 10.0f * (float) std::log10(sumMoment / (double) ringMomentSize)
                            : lufsSilenceFloor;
    const float lufsS = (sumShort > 0.0)
                            ? lufsOffsetDb + 10.0f * (float) std::log10(sumShort / (double) ringShortSize)
                            : lufsSilenceFloor;
    lufsMomentary.store(juce::jmax(lufsM, lufsSilenceFloor), std::memory_order_relaxed);
    lufsShortTerm.store(juce::jmax(lufsS, lufsSilenceFloor), std::memory_order_relaxed);
}

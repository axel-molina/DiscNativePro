#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <vector>

// MasterLimiter: bus master con limiter brickwall look-ahead (siempre activo)
// y medidor de loudness EBU R128 / ITU BS.1770 (LUFS Momentary 400 ms y
// Short-term 3 s con K-weighting). Todo corre en el hilo de audio: sin locks,
// sin allocs; los parametros y lecturas se publican como atomicos.
//
// El limiter procesa in-place: retardo de senal de 5 ms (look-ahead), gain
// computer por muestra sobre la senal SIN retardar y suavizado por muestra
// (attack 1 ms / release 100 ms). El gain se aplica a la muestra retardada,
// de modo que el attack "pre-actua" antes de que el pico llegue a la salida.
//
// El medidor LUFS pondera cada canal con K-weighting (high-shelf +4 dB y
// high-pass 38 Hz) y acumula cuadrados en ring buffers preasignados con sumas
// incrementales O(1) por muestra.
class MasterLimiter
{
public:
    MasterLimiter() = default;

    // Hilo de mensajes o de audio antes de arrancar el callback.
    void prepare(double sampleRate, int maxBlockSamples);

    // Hilo de audio. Limita in-place y actualiza GR y LUFS.
    void process(juce::AudioBuffer<float>& buffer, int numSamples);

    // Reduccion de gain actual del limiter (0 dB = sin limitar), con decaimiento.
    float getGainReductionDb() const { return gainReductionDb.load(std::memory_order_relaxed); }

    // LUFS Momentary (400 ms) y Short-term (3 s); suelo de silencio -70.
    float getLufsMomentary() const  { return lufsMomentary.load(std::memory_order_relaxed); }
    float getLufsShortTerm() const  { return lufsShortTerm.load(std::memory_order_relaxed); }

    // Techo del limiter en dBFS (atomico, escribible desde la UI en cualquier momento).
    std::atomic<float> ceilingDb { -0.3f };

private:
    static constexpr int maxChannels = 2;

    // Biquad RBJ (estado por canal) para el K-weighting.
    struct Biquad
    {
        float b0 { 1.0f }, b1 { 0.0f }, b2 { 0.0f }, a1 { 0.0f }, a2 { 0.0f };
        float x1[maxChannels] { 0.0f, 0.0f };
        float x2[maxChannels] { 0.0f, 0.0f };
        float y1[maxChannels] { 0.0f, 0.0f };
        float y2[maxChannels] { 0.0f, 0.0f };
    };

    void computeKWeightingCoefficients(double sampleRate);

    double currentSampleRate { 44100.0 };

    // Look-ahead: retardo de senal de 5 ms (un slot por muestra, preasignado).
    int delayLength { 221 };
    std::vector<float> delay[maxChannels];
    int delayPos { 0 };

    Biquad shelf;       // high-shelf +4 dB @ ~1682 Hz (etapa 1 BS.1770)
    Biquad highpass;    // high-pass ~38 Hz (etapa 2 BS.1770)

    // Ring buffers de cuadrados K-ponderados (sumas incrementales O(1)).
    std::vector<float> ringShort;   // 3 s
    std::vector<float> ringMoment;  // 400 ms
    int ringShortSize { 0 }, ringShortPos { 0 };
    int ringMomentSize { 0 }, ringMomentPos { 0 };
    double sumShort { 0.0 }, sumMoment { 0.0 };

    // Coeficientes de suavizado por muestra del gain computer.
    float attackCoef { 0.0f };
    float releaseCoef { 0.0f };

    // Envolvente de gain persistente (estado del hilo de audio).
    float gainEnvelope { 1.0f };

    // Ventana deslizante de minimo (deque monotonicO) sobre el req de las
    // ultimas delayLength+1 muestras: el gain aplicado a la muestra que sale
    // del retardo ya vio el pico que viene en camino (look-ahead verdadero).
    std::vector<float> winReq;
    std::vector<long long> winIdx;
    int winCapacity { 0 };
    int winHead { 0 };
    int winCount { 0 };
    long long sampleCounter { 0 };

    std::atomic<float> gainReductionDb { 0.0f };
    std::atomic<float> lufsMomentary { -70.0f };
    std::atomic<float> lufsShortTerm { -70.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MasterLimiter)
};

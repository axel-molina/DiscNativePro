#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

// MicChannel: canal de microfono con low-cut, gain y detector de voz para el
// auto-ducking (talkover). Todo el proceso corre en el hilo de audio (callback):
// sin locks, sin allocs; los parametros se publican como atomicos.
//
// Uso en AudioEngine::audioDeviceIOCallbackWithContext:
//   1. duck = getCurrentDuckGain(); masterBuffer.applyGain(duck)  (musica)
//   2. process(...) suma la voz filtrada y con gain al masterBuffer (post-mezcla,
//      antes de recorder/broadcast, asi hereda grabacion y emision).
class MicChannel
{
public:
    MicChannel() = default;

    // Hilo de mensajes o de audio antes de arrancar el callback.
    void prepare(double sampleRate, int maxBlockSamples);

    // Hilo de audio. inputChannels: 1 o 2 canales activos del dispositivo.
    // Suma la voz (low-cut + gain) al masterBuffer; actualiza detector de voz,
    // duckGain, micLevel y voiceActive.
    void process(const float* const* inputChannels, int numInputChannels,
                 int numSamples, juce::AudioBuffer<float>& masterBuffer);

    // Factor lineal suavizado para la musica (1.0 = sin duck).
    float getCurrentDuckGain() const { return duckGain.load(std::memory_order_relaxed); }

    // Nivel de entrada 0..1 (para el VU).
    float getMicLevel() const { return micLevel.load(std::memory_order_relaxed); }

    bool isVoiceActive() const { return voiceActive.load(std::memory_order_relaxed); }
    bool isEnabled() const { return enabled.load(std::memory_order_relaxed); }

    // Parametros (atomicos, escribibles desde la UI en cualquier momento).
    std::atomic<bool> enabled { false };
    std::atomic<bool> duckEnabled { true };
    std::atomic<bool> lowCutEnabled { true };
    std::atomic<float> gainDb { 0.0f };
    std::atomic<float> duckDepthDb { -12.0f };
    std::atomic<float> duckThresholdDb { -30.0f };

private:
    static constexpr int maxChannels = 2;

    // Biquad high-pass (RBJ, Q = 0.707): estado por canal.
    struct Biquad
    {
        float b0 { 1.0f }, b1 { 0.0f }, b2 { 0.0f }, a1 { 0.0f }, a2 { 0.0f };
        float x1[maxChannels] { 0.0f, 0.0f };
        float x2[maxChannels] { 0.0f, 0.0f };
        float y1[maxChannels] { 0.0f, 0.0f };
        float y2[maxChannels] { 0.0f, 0.0f };
    };

    void computeLowCutCoefficients(double sampleRate);

    double currentSampleRate { 44100.0 };
    Biquad lowCut;         // coeficientes ~100 Hz (recalculados en prepare)
    Biquad bypass;         // identidad (low-cut apagado)

    // Detector de voz (RMS por bloque) con hold y release.
    int holdRemainingSamples { 0 };
    int holdSamples { 0 };
    float duckAttackCoef { 0.0f };   // 1 - exp(-bloque/tau), recalculado por bloque
    float duckReleaseCoef { 0.0f };

    std::atomic<float> duckGain { 1.0f };
    std::atomic<float> micLevel { 0.0f };
    std::atomic<bool> voiceActive { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MicChannel)
};

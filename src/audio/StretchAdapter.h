#pragma once

// Adaptador time-stretch (Key Lock) sobre signalsmith-stretch (MIT, ver thirdparty).
// Sin allocs en tiempo real: buffers preasignados en el constructor.
// Uso: setTempoRate() desde UI; render() solo desde el hilo de audio.

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
// Warnings de conversión propios del third-party (MIT): se silencian aquí,
// no tocan el nivel de warnings del proyecto.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
#pragma clang diagnostic ignored "-Wshadow"
#pragma clang diagnostic ignored "-Wimplicit-int-float-conversion"
#pragma clang diagnostic ignored "-Wfloat-conversion"
#pragma clang diagnostic ignored "-Wfloat-equal"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wfloat-equal"
#endif
#include "thirdparty/signalsmith-stretch.h"
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

class StretchAdapter
{
public:
    static constexpr int maxBlockSamples = 2048;
    static constexpr int tmpSize = 8192; // cubre N*rate con N<=2048 y rate<=4

    StretchAdapter();
    ~StretchAdapter() = default;

    void prepare(double sampleRate); // hilo de mensaje/dispositivo
    void reset();                    // seek: hilo UI (con lock del deck) o audio
    void setTempoRate(double rate)   { tempoRate.store(rate < 0.02 ? 0.02 : rate); }
    // P26: muestras de archivo consumidas por muestra de dispositivo
    // (origFileSampleRate / currentSampleRate). Corrige el tempo cuando el
    // sample rate del archivo no coincide con el del dispositivo.
    void setInputRateFactor(double f) { inputRateFactor.store(f > 0.01 ? f : 0.01); }

    // Key Shift: transposición en semitonos (independiente del tempo). Se aplica
    // en render() solo cuando cambia (el setter es thread-safe vía atómico).
    void setTransposeSemitones(double st) { transposeSemitones.store(st); }

    // Rendere N samples a dst (siempre estéreo) desde src. Solo hilo de audio.
    void render(juce::AudioSource* src, int srcChannels,
                float* const* dst, int dstChannels, int numSamples);

    // Re-anclaje con pre-roll (P3) - solo hilo de audio
    bool reanchorWithPreRoll(juce::AudioSource* src, juce::int64 targetSample, double rate);

    double getLatencySeconds() const { return latencySec.load(); }

private:
    signalsmith::stretch::SignalsmithStretch<float> stretch;
    juce::AudioBuffer<float> tmp;    // entrada (2 x 8192)
    juce::AudioBuffer<float> tmpOut; // salida auxiliar mono (2 x 2048)
    std::atomic<double> tempoRate { 1.0 };
    std::atomic<double> inputRateFactor { 1.0 }; // P26: fileRate/deviceRate
    std::atomic<double> transposeSemitones { 0.0 };
    double appliedSemitones { 0.0 }; // último valor aplicado al stretch (hilo de audio)
    std::atomic<double> latencySec { 0.0 };
    double sampleRate { 44100.0 };
    bool prepared { false };
    juce::int64 cachedSeekLen { 0 };
    double inputCarry { 0.0 }; // resto fraccionario de samples de entrada (P3)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StretchAdapter)
};

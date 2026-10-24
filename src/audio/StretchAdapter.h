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
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wshadow"
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
    std::atomic<double> latencySec { 0.0 };
    double sampleRate { 44100.0 };
    bool prepared { false };
    juce::int64 cachedSeekLen { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StretchAdapter)
};

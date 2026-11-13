#pragma once

#include <juce_core/juce_core.h>
#include <memory>

// P25: motor de separación 2-stem (voz/instrumental) en STREAMING sobre
// MDX-Net (UVR-MDX-NET-Voc_FT, ONNX fp16, entrada [1,4,3072,256] = espectro
// complejo estéreo, 3072 bins, 256 frames). STFT/ISTFT (n_fft 6144, hop 1024,
// Hann) en C++ con KissFFT. Los chunks de 256 frames avanzan 128 (los outputs
// solapados se promedian en el dominio del espectro, igual que el banco de
// referencia) ⇒ latencia fija de 136192 muestras (~3.09 s a 44.1 kHz): cada
// muestra emitida ya recibió el aporte de todos los frames que la cubren.
// El modelo corre a 44.1 kHz — resamplear fuera si el device corre a otro rate.
class TwoStemEngine
{
public:
    static juce::File modelFile();
    static bool isModelAvailable();

    TwoStemEngine();
    ~TwoStemEngine();

    // Carga perezosa de la sesión ORT (CoreML EP con fallback silencioso a CPU).
    bool init(juce::String* error = nullptr);
    bool isInitialised() const;

    // Liberación explícita en el cierre de la app (mismo rationale que
    // OnnxStemEngine: los objetos ORT no se destruyen en el teardown estático).
    void shutdown();

    // Streaming: agrega numIn muestras (planar L/R, 44.1 kHz) y emite hasta
    // outCap muestras finales a voc*/ins* (instrumental = mix - vocals).
    // Devuelve cuántas muestras emitió (0 mientras acumula el primer chunk).
    int process(const float* inL, const float* inR, int numIn,
                float* vocL, float* vocR, float* insL, float* insR, int outCap);

    // Fin del stream: procesa el resto con padding y emite todo lo pendiente.
    // Devuelve muestras emitidas; la pista termina en la posición original.
    int flush(float* vocL, float* vocR, float* insL, float* insR, int outCap);

    // Descarta el estado del stream (pista nueva / seek).
    void reset();

    // Latencia de punta a punta del motor (muestras a 44.1 kHz).
    static constexpr int latencySamples = 136192; // kNeed - kAdvance

private:
    struct Impl;
    std::unique_ptr<Impl> impl;

    JUCE_DECLARE_NON_COPYABLE(TwoStemEngine)
};

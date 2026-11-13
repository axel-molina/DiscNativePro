#pragma once

#include <juce_core/juce_core.h>
#include <functional>
#include <memory>

// P23: motor de separación de stems embebido. Corre htdemucs (ONNX fp32) con
// ONNX Runtime vendorizada, en procesos de chunks fijos de 343980 muestras
// (segmento de entrenamiento, 7.8 s a 44.1 kHz) con solape 50 % y ventana de
// fade (parámetros StemKit: fade 1/10 del chunk, división por la suma de
// ventanas). Los WAV de salida quedan en 44.1 kHz PCM16.
class OnnxStemEngine
{
public:
    using CancelFn = std::function<bool()>;

    // Locator del modelo: bundle Resources/models → App Support/models →
    // junto al exe (Windows/Linux). "" si no hay modelo en la build.
    static juce::File modelFile();
    static bool isModelAvailable();

    OnnxStemEngine();
    ~OnnxStemEngine();

    // Carga perezosa de la sesión ORT. Devuelve false y llena error si no.
    // preferCoreML=false fuerza el camino CPU puro (banco A/B).
    bool init(juce::String* error = nullptr, bool preferCoreML = true);
    bool isInitialised() const;

    // Liberación explícita en el cierre de la app (contexto vivo del hilo de
    // mensajes). Si no se llama, los objetos ORT quedan para el teardown
    // estático, donde ReleaseEnv hace abort() (crash al cerrar).
    void shutdown();

    // Separa audioFile en outDir/{vocals,drums,bass,other}.wav.
    // shouldCancel se consulta entre chunks. false = falla o cancelación.
    bool separate(const juce::File& audioFile, const juce::File& outDir,
                  const CancelFn& shouldCancel = {});

private:
    struct Impl;
    std::unique_ptr<Impl> impl;

    JUCE_DECLARE_NON_COPYABLE(OnnxStemEngine)
};

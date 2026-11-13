#pragma once

#include <juce_core/juce_core.h>
#include <atomic>
#include <deque>
#include <functional>
#include <memory>
#include <set>
#include <vector>

// P23: separación offline de stems con el motor ML embebido (OnnxStemEngine +
// htdemucs ONNX, sin procesos externos), cola FIFO en un thread de baja
// prioridad. La política es "solo al cargar al deck": la library nunca encola
// masivamente. Los WAVs quedan en
// ~/Library/Application Support/DiscNativePro/stems/<firma>/{vocals,drums,bass,other}.wav
class OnnxStemEngine;

class StemSeparationService : private juce::Thread
{
public:
    enum class Status { None, Queued, Processing, Ready, Failed };

    static StemSeparationService& getInstance();
    ~StemSeparationService() override;

    // Encola la pista si corresponde (dedupe interno: ready/queued/processing/
    // failed no se re-encolan; sin modelo embebido no encola). Devuelve true
    // si quedó encolada en esta llamada.
    bool maybeEnqueue(const juce::File& audioFile);

    // P26: encola un lote en un hilo efímero (cada maybeEnqueue lee cabeceras
    // de audio; así el message thread no se bloquea) y reporta el total
    // encolado de vuelta en el message thread.
    void enqueueBatchAsync(const std::vector<juce::File>& files,
                           std::function<void(int enqueued)> onDone);

    // Cantidad de pistas pendientes en la cola (para UI).
    int getQueueCount();

    Status getStatus(const juce::File& audioFile);

    // true si la build incluye el modelo htdemucs (resultado cacheado).
    static bool isModelAvailable();

    // Directorio de stems válidos para la pista (los 4 WAVs presentes); "" si no.
    static juce::String stemsDirFor(const juce::File& audioFile);

    // Cierre ordenado desde el shutdown de la app: detiene el thread y libera
    // la sesión ORT en contexto vivo (evita el abort de ReleaseEnv en el
    // teardown estático).
    void shutdown();

    // Firma de invalidez (path canónico + modTime + size), igual que el caché.
    static juce::String signatureFor(const juce::File& audioFile);

    static juce::File stemsRoot();

    // P25: caché 2-stem (vocals/instrumental) escrita por el streaming de
    // DeckPlayer. Devuelve el directorio si los dos WAVs existen; vacío si no.
    static juce::File stems2DirFor(const juce::File& audioFile);
    // Directorio destino de la caché 2-stem para esta pista (puede no existir).
    static juce::File stems2CacheDirFor(const juce::File& audioFile);

private:
    StemSeparationService();
    void run() override;
    bool separateOne(const juce::File& audioFile, const juce::String& firma);

    std::unique_ptr<OnnxStemEngine> engine;
    std::mutex stateMutex;
    std::deque<juce::File> queue;
    std::set<juce::String> inQueue;   // firmas
    std::set<juce::String> failed;    // firmas (solo esta sesión)
    juce::String processingFirma;

    static std::atomic<int> modelAvailable; // 0 desconocido, 1 sí, 2 no

    JUCE_DECLARE_NON_COPYABLE(StemSeparationService)
};

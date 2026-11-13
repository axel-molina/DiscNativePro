#include "StemSeparationService.h"
#include "LibraryCacheManager.h"
#include "OnnxStemEngine.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>

std::atomic<int> StemSeparationService::modelAvailable { 0 };

StemSeparationService& StemSeparationService::getInstance()
{
    static StemSeparationService instance;
    return instance;
}

StemSeparationService::StemSeparationService()
    : juce::Thread("StemSeparation"), engine(std::make_unique<OnnxStemEngine>())
{
    startThread(juce::Thread::Priority::low);
}

StemSeparationService::~StemSeparationService()
{
    stopThread(3000);
}

juce::File StemSeparationService::stemsRoot()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("Application Support/DiscNativePro/stems");
    dir.createDirectory();
    return dir;
}

juce::String StemSeparationService::signatureFor(const juce::File& audioFile)
{
    const auto canonical = audioFile.getFullPathName();
    const auto modTime = audioFile.getLastModificationTime().toMilliseconds();
    const auto size = audioFile.getSize();
    return juce::String::toHexString(canonical.hashCode64()) + "-"
         + juce::String(modTime) + "-" + juce::String(size);
}

juce::String StemSeparationService::stemsDirFor(const juce::File& audioFile)
{
    if (!audioFile.existsAsFile())
        return {};
    auto dir = stemsRoot().getChildFile(signatureFor(audioFile));
    static const char* names[] = { "vocals.wav", "drums.wav", "bass.wav", "other.wav" };
    for (auto* n : names)
        if (!dir.getChildFile(n).existsAsFile())
            return {};
    return dir.getFullPathName();
}

juce::File StemSeparationService::stems2CacheDirFor(const juce::File& audioFile)
{
    return stemsRoot().getParentDirectory().getChildFile("stems2")
               .getChildFile(signatureFor(audioFile));
}

juce::File StemSeparationService::stems2DirFor(const juce::File& audioFile)
{
    if (!audioFile.existsAsFile())
        return {};
    auto dir = stems2CacheDirFor(audioFile);
    if (!dir.getChildFile("vocals.wav").existsAsFile()
        || !dir.getChildFile("instrumental.wav").existsAsFile())
        return {};
    return dir;
}

bool StemSeparationService::isModelAvailable()
{
    int expected = 0;
    if (modelAvailable.compare_exchange_strong(expected, -1))
        modelAvailable.store(OnnxStemEngine::isModelAvailable() ? 1 : 2);

    while (modelAvailable.load() == -1)
        juce::Thread::sleep(10);
    return modelAvailable.load() == 1;
}

bool StemSeparationService::maybeEnqueue(const juce::File& audioFile)
{
    if (!audioFile.existsAsFile())
        return false;

    // Ya separada: solo asegura el estado en el caché.
    if (stemsDirFor(audioFile).isNotEmpty())
    {
        LibraryCacheManager::getInstance().updateStemsStatus(audioFile, "ready",
                                                             stemsDirFor(audioFile));
        return false;
    }

    // Misma política que DeckPlayer: los stems se cargan a RAM solo para
    // pistas ≤ 900 s; separar una mezcla larga sería trabajo (y ~50 bytes por
    // muestra) que nunca se va a usar.
    {
        juce::AudioFormatManager fmt;
        fmt.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> r(fmt.createReaderFor(audioFile));
        if (r == nullptr)
            return false;
        const auto durSec = (double)r->lengthInSamples / juce::jmax(1.0, r->sampleRate);
        if (durSec <= 0.0 || durSec > 900.0)
            return false;
    }

    if (!isModelAvailable())
        return false; // sin modelo embebido: la cola no encola (Settings muestra el hint)

    const auto firma = signatureFor(audioFile);

    {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (inQueue.count(firma) > 0 || firma == processingFirma || failed.count(firma) > 0)
            return false;
        queue.push_back(audioFile);
        inQueue.insert(firma);
    }

    LibraryCacheManager::getInstance().updateStemsStatus(audioFile, "processing", {});
    notify();
    return true;
}

int StemSeparationService::getQueueCount()
{
    std::lock_guard<std::mutex> lock(stateMutex);
    return (int)queue.size() + (processingFirma.isNotEmpty() ? 1 : 0);
}

void StemSeparationService::enqueueBatchAsync(const std::vector<juce::File>& files,
                                              std::function<void(int)> onDone)
{
    auto pending = std::make_shared<std::vector<juce::File>>(files);
    auto callback = std::make_shared<std::function<void(int)>>(std::move(onDone));
    juce::Thread::launch([this, pending, callback]()
    {
        int enqueued = 0;
        for (const auto& f : *pending)
            if (maybeEnqueue(f))
                ++enqueued;
        juce::MessageManager::callAsync([callback, enqueued]()
        {
            if (*callback)
                (*callback)(enqueued);
        });
    });
}

StemSeparationService::Status StemSeparationService::getStatus(const juce::File& audioFile)
{
    if (stemsDirFor(audioFile).isNotEmpty())
        return Status::Ready;

    const auto firma = signatureFor(audioFile);
    std::lock_guard<std::mutex> lock(stateMutex);
    if (firma == processingFirma)
        return Status::Processing;
    if (inQueue.count(firma) > 0)
        return Status::Queued;
    if (failed.count(firma) > 0)
        return Status::Failed;
    return Status::None;
}

void StemSeparationService::run()
{
    while (!threadShouldExit())
    {
        juce::File next;
        bool haveNext = false;
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            processingFirma = {};
            if (!queue.empty())
            {
                next = queue.front();
                queue.pop_front();
                inQueue.erase(signatureFor(next));
                processingFirma = signatureFor(next);
                haveNext = true;
            }
        }

        // Sin trabajo: esperar SIN retener stateMutex (si se durmiera con el
        // lock tomado, maybeEnqueue del hilo de mensajes se bloquearía antes
        // de poder hacer notify() → deadlock de carga de deck).
        if (!haveNext)
        {
            wait(-1); // hasta notify() de maybeEnqueue
            continue;
        }

        const auto firma = processingFirma;

        const bool ok = separateOne(next, firma);

        {
            std::lock_guard<std::mutex> lock(stateMutex);
            processingFirma = {};
            if (!ok)
                failed.insert(firma);
        }

        // Pequeña pausa entre pistas para no mantener el CPU al 100 % seguido.
        wait(500);
    }
}

void StemSeparationService::shutdown()
{
    stopThread(5000); // la cancelación entre chunks termina en ≤ ~2.5 s
    engine->shutdown();
}

bool StemSeparationService::separateOne(const juce::File& audioFile, const juce::String& firma)
{
    if (threadShouldExit())
        return false;

    auto finalDir = stemsRoot().getChildFile(firma);
    finalDir.createDirectory();

    // El motor escribe los 4 WAVs (44.1 kHz PCM16) directo al caché definitivo.
    if (!engine->separate(audioFile, finalDir, [this]() { return threadShouldExit(); }))
    {
        finalDir.deleteRecursively();
        return false;
    }

    static const char* names[] = { "vocals.wav", "drums.wav", "bass.wav", "other.wav" };
    for (auto* n : names)
        if (!finalDir.getChildFile(n).existsAsFile())
        {
            finalDir.deleteRecursively();
            return false;
        }

    // Marcador de versión (de cara a futuras migraciones de formato).
    finalDir.getChildFile("stems.json").replaceWithText("{ \"version\": 1 }");

    LibraryCacheManager::getInstance().updateStemsStatus(audioFile, "ready",
                                                         finalDir.getFullPathName());
    return true;
}

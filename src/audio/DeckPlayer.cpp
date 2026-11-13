#include "DeckPlayer.h"
#include "BeatDetector.h"
#include "KeyDetector.h"
#include "StretchAdapter.h"
#include "AudioEngine.h" // getSettingsFile (persistencia del modo de stems)
#include "../services/LibraryCacheManager.h"
#include "../services/StemSeparationService.h"
#include "../services/TwoStemEngine.h"
#include <juce_events/juce_events.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace
{
// P5: pool dedicado de 1 hilo. El análisis (decodificación + onsets + key)
// nunca corre en el hilo de mensajes ni compite con el streaming de disco
// (TimeSliceThread) ni con el callback de audio.
juce::ThreadPool& analysisPool()
{
    static juce::ThreadPool pool(1);
    return pool;
}

// Normaliza una banda a su máximo: los temas con master bajo muestran picos
// igual que los altos (beneficia al overview y al vertical).
void normaliseBand(std::vector<float>& band)
{
    float maxV = 1e-6f;
    for (const float v : band)
        maxV = juce::jmax(maxV, v);
    for (float& v : band)
        v /= maxV;
}
} // namespace

DeckPlayer::DeckPlayer(juce::AudioFormatManager& fm, juce::TimeSliceThread& thread)
    : formatManager(fm), diskThread(thread),
      stretcher(std::make_unique<StretchAdapter>()),
      analysis(std::make_shared<AnalysisShared>())
{
    for (size_t i = 0; i < hotCues.size(); ++i)
    {
        hotCues[i].store(0.0);
        hotCuesActive[i].store(false);
    }
    for (auto& g : stemGain)
        g.store(1.0f);
    scratchRing.setSize(2, scratchRingSize);
    scratchRing.clear();
    scratchTail.setSize(2, scratchTailSize);
    scratchTail.clear();
    playbackPosSamples.store(0);
    seekPending.store(false);
    seekTargetSec.store(0.0);
    firstBeatOffsetSeconds.store(0.0);
    stemsPrecompute4.store(loadStemsModeDefault());
}
DeckPlayer::~DeckPlayer()
{
    // Ningún trabajo pendiente vuelve a tocar este deck (ver publishIfCurrent).
    // El DeckPlayer se destruye en el hilo de mensajes, igual que las
    // publicaciones diferidas, así que la bandera basta como barrera.
    if (analysis != nullptr)
        analysis->alive.store(false);

    // El job del pool chequea esta bandera cada pasada (≤ 25 ms + una llamada
    // al motor ~0.35 s); el destructor del pool espera su salida.
    stems2Quit.store(true);

    releaseResources();
}

bool DeckPlayer::loadFile(const juce::File& audioFile)
{
    if (!audioFile.existsAsFile())
        return false;

    auto* reader = formatManager.createReaderFor(audioFile);
    if (reader == nullptr)
        return false;

    // P5: segundo reader, exclusivo del worker de análisis. Los
    // AudioFormatReader no son seguros para lecturas concurrentes con la
    // reproducción, así que ambos caminos nunca comparten handle.
    std::shared_ptr<juce::AudioFormatReader> analysisReader(formatManager.createReaderFor(audioFile));

    // Toda carga invalida el trabajo anterior: sus resultados se descartan.
    const int generation = analysis->generation.fetch_add(1) + 1;

    {
        const juce::ScopedLock sl(lock);

        // Stop and clear previous pipeline
        playing.store(false);
        loaded.store(false);

        resamplingSource.reset();
        bufferingSource.reset();
        readerSource.reset();

        currentFile = audioFile;

        // Metadata extraction
        trackTitle = reader->metadataValues.getValue("title", audioFile.getFileNameWithoutExtension());
        trackArtist = reader->metadataValues.getValue("artist", "Artista Local");
        trackKey = "--";
        trackCamelot = "--";

        double duration = (double)reader->lengthInSamples / reader->sampleRate;
        trackDurationSeconds.store(duration);
        cuePositionSeconds.store(0.0);
        origFileSampleRate = reader->sampleRate > 0.0 ? (double)reader->sampleRate : 44100.0;
        updateRateFactor();

        // P3 sobre P5: la pista nueva no hereda seek pendiente, fase ni
        // posicion de la anterior (el analisis llega despues y repone el offset).
        seekPending.store(false);
        seekTargetSec.store(0.0);
        playbackPosSamples.store(0);
        firstBeatOffsetSeconds.store(0.0);

        // P16: el grid corregido de la pista anterior no se hereda (si la
        // pista tenía corrección persistida, el llamante la reaplica después).
        gridBpmOverride.store(0.0);
        gridOffsetOverride.store(-1.0);
        detectedBpm.store(0.0);
        detectedOffsetSeconds.store(0.0);

        // Fase rápida: sin analizadores ni waveform. BPM/peaks quedan vacíos
        // hasta que el worker publique (la UI muestra "--" y el waveform se
        // rellena en cuanto llega el overview rápido).
        originalBpm.store(0.0);
        currentBpm.store(0.0);
        peaksLow.clear();
        peaksMid.clear();
        peaksHigh.clear();

        // Reset hot cues
        for (size_t i = 0; i < hotCues.size(); ++i)
        {
            hotCues[i].store(0.0);
            hotCuesActive[i].store(false);
        }
        // P27: reaplicar hotcues persistidos por pista (0 = vacío).
        CachedTrackMetadata meta;
        if (LibraryCacheManager::getInstance().getTrack(audioFile, meta))
        {
            for (size_t i = 0; i < hotCues.size(); ++i)
            {
                if (meta.hotCues[i] > 0.0 && meta.hotCues[i] < duration)
                {
                    hotCues[i].store(meta.hotCues[i]);
                    hotCuesActive[i].store(true);
                }
            }
        }
        loopActive.store(false);

        // Build the streaming audio pipeline
        // 1. AudioFormatReaderSource takes ownership of reader
        numSrcChannels = (int)reader->numChannels;
        readerSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);

        // 2. BufferingAudioSource reads ahead in background thread (approx 4 seconds of buffer)
        // Memory footprint is tiny (~1-2MB) even for 2-hour long sets!
        constexpr int bufferSize = 32768 * 4;
        // ponytail: prefill desactivado (6.º argumento). El prefills por defecto
        // duerme ~10 ms en prepareToPlay (2 tandas de 5 ms) y eso es justo el
        // bloqueo que P5 elimina. El hilo de disco llena el buffer en paralelo
        // en cuanto el deck suena; el peor caso es un bloque de silencio si se
        // pulsa PLAY en el mismo milisegundo de la carga.
        bufferingSource = std::make_unique<juce::BufferingAudioSource>(readerSource.get(), diskThread, false, bufferSize, 2, false);

        // 3. ResamplingAudioSource allows smooth pitch / tempo changes (+/- 8%, etc.)
        resamplingSource = std::make_unique<juce::ResamplingAudioSource>(bufferingSource.get(), false, 2);

        if (currentSampleRate > 0.0)
        {
            int blockSize = expectedSamplesPerBlock > 0 ? expectedSamplesPerBlock : 512;
            bufferingSource->prepareToPlay(blockSize, currentSampleRate);
            resamplingSource->prepareToPlay(blockSize, currentSampleRate);
            stretcher->prepare(currentSampleRate);
        }

        stretcher->reset();
        updatePlaybackSpeed();

        // P22: nueva pista = stems de la anterior fuera. Bajo el mismo lock
        // que getNextAudioBlock: sin ventana de carrera con el hilo de audio.
        stemsActive.store(false);
        stemsInRam.store(false);
        stemsReady.store(false);
        for (auto& b : stemBuffers)
            b.setSize(2, 0, false, false, true);

        // P25: el streaming 2-stem de la pista anterior también fuera.
        stems2Active.store(false);
        stems2Engaged.store(false);
        stems2Covered.store(false);
        stems2Cached.store(false);
        stems2CacheDone.store(false); // nueva pista: el worker puede armar caché otra vez
        stems2CacheLen44 = 0;
        for (auto& b : stems2CacheBuf)
            b.setSize(2, 0, false, false, true);

        loaded.store(true);
    }

    // P22: stems. Solo pistas de duración razonable (mezclas largas no caben
    // en RAM). Si ya están separados se cargan a buffers; si no, se encola la
    // separación (queda lista para la próxima carga).
    const auto stemDir = StemSeparationService::stemsDirFor(audioFile);
    const double durSec = trackDurationSeconds.load();
    if (stemDir.isNotEmpty() && durSec > 0.0 && durSec <= 900.0)
    {
        stemsReady.store(true);
        startStemsLoad(stemDir, generation);
    }
    // P25 Fase D: caché 2-stem — la segunda carga de la pista es instantánea.
    else if (loadStems2Cache(StemSeparationService::stems2DirFor(audioFile)))
    {
        // Caché 2-stem en RAM: sin worker, sin modelo. El callback cubre toda
        // la pista desde stems2CacheBuf.
        stems2Engaged.store(true);
    }
    else if (!stemsPrecompute4.load() && TwoStemEngine::isModelAvailable() && durSec > 0.0)
    {
        // P25: modo Rápido (default) — sin 4-stem precalculado el 2-stem corre
        // en streaming durante la reproducción y htdemucs no se encola.
        auto sepReader = std::shared_ptr<juce::AudioFormatReader>(formatManager.createReaderFor(audioFile));
        if (sepReader != nullptr)
        {
            for (auto& ring : stems2Ring)
            {
                ring.setSize(2, stems2RingSamples, false, false, true);
                ring.clear();
            }
            s2Origin44.store(0);
            s2WriteEnd44.store(0);
            stems2Engaged.store(true);
            startStems2Stream(std::move(sepReader), audioFile, 0, generation);
        }
        else
        {
            StemSeparationService::getInstance().maybeEnqueue(audioFile);
        }
    }
    else
    {
        StemSeparationService::getInstance().maybeEnqueue(audioFile);
    }

    // Fase de análisis: worker propio, resultados publicados en el hilo de mensajes.
    startAnalysis(std::move(analysisReader), generation);

    return true;
}

void DeckPlayer::startAnalysis(std::shared_ptr<juce::AudioFormatReader> analysisReader, int generation)
{
    if (analysisReader == nullptr || analysis == nullptr)
        return;

    auto shared = analysis;
    analysisPool().addJob([this, shared, reader = std::move(analysisReader), generation]()
    {
        auto isCurrent = [&shared, generation]()
        {
            return shared->alive.load() && shared->generation.load() == generation;
        };

        // 1) Overview rápido (~2000 puntos, ventanas cortas repartidas): la
        //    bandeja pinta un waveform real en milisegundos.
        {
            std::vector<float> low, mid, high;
            computeQuickOverview(reader.get(), 2000, low, mid, high);
            if (!isCurrent())
                return;
            publishIfCurrent(shared, generation, [this, low, mid, high]() mutable
            {
                peaksLow = std::move(low);
                peaksMid = std::move(mid);
                peaksHigh = std::move(high);
            });
        }

        // 2) Overview completo (8000 puntos, una pasada secuencial).
        {
            std::vector<float> low, mid, high;
            computeFullOverview(reader.get(), 8000, low, mid, high);
            if (!isCurrent())
                return;
            publishIfCurrent(shared, generation, [this, low, mid, high]() mutable
            {
                peaksLow = std::move(low);
                peaksMid = std::move(mid);
                peaksHigh = std::move(high);
            });
        }

        // 3) BPM + offset del primer beat (solo atomos: el label se refresca en
        //    el timer de la UI y el controlador de fase lee el offset).
        const auto beatRes = BeatDetector::analyze(reader.get());
        if (!isCurrent())
            return;
        publishIfCurrent(shared, generation, [this, bpm = beatRes.bpm, offset = beatRes.offset]()
        {
            detectedBpm.store(bpm);
            detectedOffsetSeconds.store(offset);
            // P16: si hay grid corregido a mano, el análisis no lo pisa.
            const double gBpm = gridBpmOverride.load();
            if (gBpm > 0.0)
            {
                originalBpm.store(gBpm);
                currentBpm.store(gBpm * (1.0 + (double)pitchPercent.load() / 100.0));
            }
            else
            {
                originalBpm.store(bpm);
                currentBpm.store(bpm * (1.0 + (double)pitchPercent.load() / 100.0));
            }
            const double gOff = gridOffsetOverride.load();
            firstBeatOffsetSeconds.store(gOff >= 0.0 ? gOff : offset);
        });

        // 4) Tonalidad (Camelot + musical).
        const auto keyRes = KeyDetector::analyze(reader.get());
        if (!isCurrent())
            return;
        publishIfCurrent(shared, generation,
                         [this,
                          key = (keyRes.formattedKey.isNotEmpty() && keyRes.formattedKey != "--")
                                    ? keyRes.formattedKey
                                    : keyRes.camelot,
                          camelot = keyRes.camelot]()
        {
            const juce::ScopedLock sl(lock);
            trackKey = key;
            trackCamelot = camelot;
        });
    });
}

void DeckPlayer::publishIfCurrent(const std::shared_ptr<AnalysisShared>& shared, int generation,
                                  std::function<void()> publishFn)
{
    // callAsync preserva el orden (rápido antes que completo) y los pintores son
    // del hilo de mensajes: allí se asignan peaks, BPM y tonalidad sin carreras.
    juce::MessageManager::callAsync([shared, generation, publish = std::move(publishFn)]() mutable
    {
        if (!shared->alive.load() || shared->generation.load() != generation)
            return; // deck destruido u otra pista cargada: resultado obsoleto
        publish();
    });
}

void DeckPlayer::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    currentSampleRate = sampleRate;
    expectedSamplesPerBlock = samplesPerBlockExpected;

    const juce::ScopedLock sl(lock);

    updateRateFactor();

    if (bufferingSource != nullptr)
        bufferingSource->prepareToPlay(samplesPerBlockExpected, sampleRate);

    if (resamplingSource != nullptr)
    {
        resamplingSource->prepareToPlay(samplesPerBlockExpected, sampleRate);
        stretcher->prepare(sampleRate);
        updatePlaybackSpeed();
    }
    else if (stretcher != nullptr)
    {
        stretcher->prepare(sampleRate);
    }
}

void DeckPlayer::releaseResources()
{
    const juce::ScopedLock sl(lock);
    if (resamplingSource != nullptr)
        resamplingSource->releaseResources();
    if (bufferingSource != nullptr)
        bufferingSource->releaseResources();
}

void DeckPlayer::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    if (!loaded.load())
    {
        bufferToFill.clearActiveBufferRegion();
        return;
    }

    const juce::ScopedLock sl(lock);
    if (resamplingSource == nullptr || bufferingSource == nullptr)
    {
        bufferToFill.clearActiveBufferRegion();
        return;
    }

    // P3: el seek se aplica tambien con el transporte en pausa (la aguja, el
    // tiempo y la onda deben moverse al hacer clic o arrastrar sobre la onda).
    if (seekPending.exchange(false))
        applyPendingSeek(seekTargetSec.load());

    if (!playing.load())
    {
        bufferToFill.clearActiveBufferRegion();
        return;
    }

    // Scratch v3: pitch sigue a la mano. Adelante = resampler existente con
    // ratio dinámico (barrido de pitch real). Atrás = render invertido real
    // con pitch-tracking. Transiciones con crossfade; transporte congelado.
    if (scratchTouch.load())
    {
        double sr = currentSampleRate > 0.0 ? currentSampleRate : 44100.0;
        // P26: el cursor de BufferingAudioSource vive en muestras de ARCHIVO;
        // las conversiones cursor<->segundos del scratch usan fileSr. El anillo
        // de historial y el avance slip siguen en muestras de dispositivo (sr).
        const double fileSr = origFileSampleRate > 0.0 ? origFileSampleRate : 44100.0;
        double dur = trackDurationSeconds.load();
        int N = bufferToFill.numSamples;
        double rate = (double)scratchVelRevPerSec.load() * 1.8; // audio-sec por sec, con signo

        // Slip (P15): bajo la mano el track sigue a tempo nominal en una
        // posicion virtual; al soltar, retoma donde habria estado.
        if (slipEnabled.load())
        {
            if (!slipActive)
            {
                slipActive = true;
                slipVirtualSec.store(getPosition());
            }
            const double tempoRate = juce::jlimit(0.25, 4.0, 1.0 + (double) pitchPercent.load());
            slipVirtualSec.store(slipVirtualSec.load() + (double) N / sr / tempoRate);
        }

        // P3: la aguja y el tiempo siguen a la mano tambien durante el scratch.
        // getPosition() descuenta la latencia del stretcher, asi que se repone
        // aqui para que la posicion publicada sea la que se esta oyendo.
        auto publishScratchPos = [&]()
        {
            const double latency = keylock.load() ? stretcher->getLatencySeconds() : 0.0;
            playbackPosSamples.store((juce::int64)((scratchAnchorSec + latency) * fileSr));
        };

        if (!scratchAnchorInit)
        {
            scratchAnchorSec = (double)bufferingSource->getNextReadPosition() / fileSr;
            scratchDir = 0;
            scratchAnchorInit = true;
            scratchTail.clear();
        }

        int dir = (rate > 0.005) ? 1 : ((rate < -0.005) ? -1 : 0);
        if (dir != scratchDir)
        {
            if (dir >= 0)
                bufferingSource->setNextReadPosition((juce::int64)(scratchAnchorSec * fileSr));
            else
                revHead = (double)ringWritePos; // reversa: cabeza = lo último escuchado
            scratchTail.clear();
            scratchDir = dir;
        }

        if (dir == 0)
        {
            bufferToFill.clearActiveBufferRegion(); // mano quieta = vinilo parado
            return;
        }

        if (dir > 0)
        {
            if (keylock.load())
            {
                // Con Key Lock el tempo lo maneja el stretch (pitch intacto).
                stretcher->setTempoRate(juce::jlimit(0.02, 4.0, rate));
                float* dstPtrs[2] = { nullptr, nullptr };
                auto* fwdBuf = bufferToFill.buffer;
                int fwdCh = fwdBuf->getNumChannels();
                for (int ch = 0; ch < juce::jmin(fwdCh, 2); ++ch)
                    dstPtrs[ch] = fwdBuf->getWritePointer(ch, bufferToFill.startSample);
                stretcher->render(bufferingSource.get(), numSrcChannels, dstPtrs, juce::jmin(fwdCh, 2), N);
            }
            else
            {
                // P26: el ratio es muestras-de-entrada/salida; con rateFactor el
                // scratch avanza fileRate/deviceRate muestras por muestra.
                resamplingSource->setResamplingRatio(juce::jlimit(0.02, 4.0, rate * rateFactor.load()));
                resamplingSource->getNextAudioBlock(bufferToFill);
            }
            writeScratchRing(bufferToFill); // lo escuchado queda en historial
            scratchAnchorSec = (double)bufferingSource->getNextReadPosition() / fileSr;
            if (dur > 0.0 && scratchAnchorSec > dur)
                scratchAnchorSec = dur; // tope sin auto-stop
            publishScratchPos();
            prevReadPos = (juce::int64)(scratchAnchorSec * fileSr);
            return;
        }

        // --- Atrás: consumir del anillo de historial (siempre caliente) ---
        double speed = juce::jmin(-rate, 4.0);
        double step = speed * (double)N; // samples de anillo por bloque
        double oldest = (double)ringWritePos - (double)ringValid;
        double avail = revHead - oldest;
        if (avail < 8.0 || N > scratchRingSize / 2)
        {
            bufferToFill.clearActiveBufferRegion(); // sin historial: silencio limpio
            return;
        }
        if (step > avail)
            step = avail; // tope suave al inicio del historial

        auto* outBuf = bufferToFill.buffer;
        int outCh = outBuf->getNumChannels();
        int ringCh = scratchRing.getNumChannels();
        for (int ch = 0; ch < outCh; ++ch)
        {
            int rc = ch % ringCh;
            float* d = outBuf->getWritePointer(ch, bufferToFill.startSample);
            const float* tail = scratchTail.getReadPointer(rc % scratchTail.getNumChannels());
            for (int i = 0; i < N; ++i)
            {
                // Ventana [revHead - step, revHead] recorrida al revés con pitch-tracking
                double p = revHead - ((double)i * step / (double)juce::jmax(1, N - 1));
                float v = readScratchRing(rc, p);
                if (i < scratchXfade)
                {
                    float t = (float)i / (float)scratchXfade;
                    float g = t * t * (3.0f - 2.0f * t);
                    v = tail[i] * (1.0f - g) + v * g;
                }
                d[i] = v;
            }
            float* t = scratchTail.getWritePointer(rc % scratchTail.getNumChannels());
            const float* tailSrc = outBuf->getReadPointer(ch, bufferToFill.startSample + N - scratchXfade);
            for (int i = 0; i < scratchXfade; ++i)
                t[i] = tailSrc[i];
        }

        revHead -= step;
        scratchAnchorSec -= step / sr; // aguja UI sigue a la mano
        if (scratchAnchorSec < 0.0)
            scratchAnchorSec = 0.0;
        bufferingSource->setNextReadPosition((juce::int64)(scratchAnchorSec * fileSr)); // cursor = aguja
        publishScratchPos();
        prevReadPos = (juce::int64)(scratchAnchorSec * fileSr);
        return; // sin loop-wrap ni auto-stop durante el toque
    }

    // Seek seguro: el pendiente ya se aplico al inicio del bloque (tambien en
    // pausa), antes de decidir si hay audio que renderizar.

    // Process audio block from streaming reader.
    // Key Lock ON: tempo por stretch (pitch intacto). OFF: resample clásico.
    const juce::int64 rpBefore = bufferingSource->getNextReadPosition();
    if (keylock.load())
    {
        float* dstPtrs[2] = { nullptr, nullptr };
        auto* outBuf = bufferToFill.buffer;
        int outCh = outBuf->getNumChannels();
        int useCh = juce::jmin(outCh, 2);
        for (int ch = 0; ch < useCh; ++ch)
            dstPtrs[ch] = outBuf->getWritePointer(ch, bufferToFill.startSample);
        stretcher->render(bufferingSource.get(), numSrcChannels, dstPtrs, useCh, bufferToFill.numSamples);
    }
    else
    {
        resamplingSource->getNextAudioBlock(bufferToFill);
    }

    // P22: modo stems — la salida audible es la suma de stems con sus gains
    // (original muteado). La posición de cada stem se deriva del cursor del
    // master: hereda tempo, keylock, loops y seeks sin pipeline propio.
    if (stemsActive.load() && stemsInRam.load())
    {
        auto* outBuf = bufferToFill.buffer;
        const int outCh = juce::jmin(outBuf->getNumChannels(), 2);
        const int N = bufferToFill.numSamples;
        const juce::int64 rpAfter = bufferingSource->getNextReadPosition();
        const double delta = (double)(rpAfter - rpBefore);

        bool canFollow = delta > 0.0 && delta < origFileSampleRate * 8.0 && N > 0;
        if (canFollow)
        {
            const double latencySec = keylock.load() ? stretcher->getLatencySeconds() : 0.0;
            const double startSec = ((double)rpBefore - latencySec * origFileSampleRate) / origFileSampleRate;
            const double stepSec = (delta / origFileSampleRate) / (double)N;

            for (int ch = 0; ch < outCh; ++ch)
            {
                auto* out = outBuf->getWritePointer(ch, bufferToFill.startSample);
                for (int i = 0; i < N; ++i)
                    out[i] = 0.0f; // original fuera
            }

            for (int s = 0; s < 4; ++s)
            {
                const float g = stemGain[(size_t)s].load();
                if (g <= 0.001f)
                    continue;
                const auto& buf = stemBuffers[(size_t)s];
                if (buf.getNumSamples() <= 0)
                    continue;
                const double stemSr = stemSampleRate[(size_t)s] > 0.0 ? stemSampleRate[(size_t)s] : origFileSampleRate;
                const int stemCh = juce::jmin(buf.getNumChannels(), 2);

                for (int ch = 0; ch < outCh; ++ch)
                {
                    auto* out = outBuf->getWritePointer(ch, bufferToFill.startSample);
                    const float* data = buf.getReadPointer(ch % stemCh);
                    const int len = buf.getNumSamples();
                    for (int i = 0; i < N; ++i)
                        out[i] += g * readStemInterp(data, len, (startSec + stepSec * (double)i) * stemSr);
                }
            }
        }
        else
        {
            // Seek/reversa/loop wrap: silencio limpio hasta que el cursor
            // vuelva a avanzar de forma continua.
            for (int ch = 0; ch < outCh; ++ch)
                outBuf->clear(ch, bufferToFill.startSample, N);
        }
    }
    // P25: modo 2-stem en streaming. Misma matemática de cursor que P22; si el
    // tramo del bloque está dentro de la zona separada, la salida es VOC/INST
    // del anillo (original muteado); si no, la mezcla original sigue sonando
    // sin cortes hasta que el worker la cubra (degradación elegante).
    else if (stems2Engaged.load() && stems2Active.load())
    {
        auto* outBuf = bufferToFill.buffer;
        const int outCh = juce::jmin(outBuf->getNumChannels(), 2);
        const int N = bufferToFill.numSamples;
        const juce::int64 rpAfter = bufferingSource->getNextReadPosition();
        const double delta = (double)(rpAfter - rpBefore);

        const bool canFollow = delta > 0.0 && delta < origFileSampleRate * 8.0 && N > 0;
        if (canFollow)
        {
            const double latencySec = keylock.load() ? stretcher->getLatencySeconds() : 0.0;
            const double startSec = ((double)rpBefore - latencySec * origFileSampleRate) / origFileSampleRate;
            const double stepSec = (delta / origFileSampleRate) / (double)N;

            const double begin44 = startSec * 44100.0;
            const double end44 = (startSec + stepSec * (double)(N - 1)) * 44100.0;
            const bool cached = stems2Cached.load();
            const juce::int64 origin = s2Origin44.load();
            const juce::int64 wEnd = s2WriteEnd44.load();
            const juce::int64 cStart = juce::jmax(origin, wEnd - (juce::int64)stems2RingSamples);
            const bool covered = cached
                ? (end44 <= (double)stems2CacheLen44 - 2.0)
                : (begin44 >= (double)(cStart + 1) && end44 <= (double)(wEnd - 2));
            stems2Covered.store(covered);

            if (covered)
            {
                const float gV = stemGain[0].load();
                const float gI = stemGain[1].load();
                if (cached)
                {
                    // Caché completa en RAM: lectura directa a 44.1 kHz.
                    const int cLen = (int)stems2CacheLen44;
                    for (int ch = 0; ch < outCh; ++ch)
                    {
                        auto* out = outBuf->getWritePointer(ch, bufferToFill.startSample);
                        const auto cIdx = (size_t)juce::jmin(ch, 1);
                        const auto* vD = stems2CacheBuf[0].getReadPointer((int)cIdx);
                        const auto* iD = stems2CacheBuf[1].getReadPointer((int)cIdx);
                        for (int i = 0; i < N; ++i)
                        {
                            const double p44 = (startSec + stepSec * (double)i) * 44100.0;
                            out[i] = gV * readStemInterp(vD, cLen, p44)
                                   + gI * readStemInterp(iD, cLen, p44);
                        }
                    }
                }
                else
                {
                    for (int ch = 0; ch < outCh; ++ch)
                    {
                        auto* out = outBuf->getWritePointer(ch, bufferToFill.startSample);
                        for (int i = 0; i < N; ++i)
                        {
                            const double p44 = (startSec + stepSec * (double)i) * 44100.0;
                            out[i] = gV * readStems2Ring(stems2Ring[0], ch, p44, origin)
                                   + gI * readStems2Ring(stems2Ring[1], ch, p44, origin);
                        }
                    }
                }
            }
        }
        else
        {
            stems2Covered.store(false);
        }
    }

    writeScratchRing(bufferToFill); // historial para la reversa del scratch

    // Actualizar posición atómica sin lock para getPosition
    if (bufferingSource != nullptr && currentSampleRate > 0.0)
    {
        auto rp = bufferingSource->getNextReadPosition();
        playbackPosSamples.store(rp);

        // Slip (P15): durante loop, la posicion virtual avanza con el delta
        // real del cursor (respeta keylock y resample). Al terminar el loop o
        // el scratch (condicion falsa), retoma en la posicion virtual.
        const bool loopOn = loopActive.load();
        if (!slipEnabled.load())
        {
            slipActive = false; // slip apagado en caliente: descarta la virtual
        }
        else
        {
            if (!slipActive && loopOn)
            {
                slipActive = true;
                slipVirtualSec.store(getPosition());
                prevReadPos = rp;
            }
            if (slipActive)
            {
                const juce::int64 delta = rp - prevReadPos;
                // P26: el delta es en muestras de archivo; el avance virtual es
                // en segundos de pista.
                if (delta > 0 && delta < (juce::int64) origFileSampleRate)
                    slipVirtualSec.store(slipVirtualSec.load()
                                         + (double) delta / origFileSampleRate);
                prevReadPos = rp;

                if (!loopOn)
                {
                    // el loop/scratch termino: retomar donde habria estado
                    slipActive = false;
                    seekPending.store(true);
                    seekTargetSec.store(juce::jlimit(0.0, trackDurationSeconds.load(),
                                                     slipVirtualSec.load()));
                }
            }
        }
    }

    // Handle Active Loop
    if (loopActive.load() && !scratchTouch.load() && bufferingSource != nullptr)
    {
        double currentPos = getPosition();
        double start = loopStartSeconds.load();
        double length = loopLengthSeconds.load();

        if (currentPos >= (start + length))
        {
            seekPending.store(true);
            seekTargetSec.store(start);
        }
    }

    // Check End of Track
    if (!scratchTouch.load() && trackDurationSeconds.load() > 0.0 && getPosition() >= trackDurationSeconds.load())
    {
        playing.store(false);
        seekPending.store(true);
        seekTargetSec.store(trackDurationSeconds.load());
    }
}

void DeckPlayer::writeScratchRing(const juce::AudioSourceChannelInfo& info)
{
    int N = info.numSamples;
    if (N <= 0 || N > scratchRingSize || info.buffer == nullptr)
        return;
    int chs = juce::jmin(info.buffer->getNumChannels(), scratchRing.getNumChannels());
    int first = juce::jmin(N, scratchRingSize - ringWritePos);
    for (int ch = 0; ch < chs; ++ch)
    {
        const float* s = info.buffer->getReadPointer(ch, info.startSample);
        float* d = scratchRing.getWritePointer(ch);
        std::memcpy(d + ringWritePos, s, sizeof(float) * (size_t)first);
        if (first < N)
            std::memcpy(d, s + first, sizeof(float) * (size_t)(N - first));
    }
    ringWritePos = (ringWritePos + N) % scratchRingSize;
    ringValid = juce::jmin(ringValid + N, scratchRingSize);
}

float DeckPlayer::readScratchRing(int ch, double pos) const
{
    int sz = scratchRingSize;
    double q = pos - std::floor(pos / (double)sz) * (double)sz; // módulo siempre positivo
    int i0 = (int)q;
    int i1 = (i0 + 1 < sz) ? i0 + 1 : 0;
    double fr = q - (double)i0;
    const float* s = scratchRing.getReadPointer(ch);
    return s[i0] + (float)fr * (s[i1] - s[i0]);
}

void DeckPlayer::beginScratch(){
    scratchVelRevPerSec.store(0.0f);
    scratchAnchorInit = false;
    scratchDir = 0;
    scratchTouch.store(true);
}

void DeckPlayer::endScratch()
{
    scratchTouch.store(false);
    scratchVelRevPerSec.store(0.0f);
    updatePlaybackSpeed(); // restaura ratio de pitch
}

void DeckPlayer::play()
{
    if (loaded.load())
    {
        playing.store(true);
    }
}

void DeckPlayer::pause()
{
    playing.store(false);
}

void DeckPlayer::stop()
{
    playing.store(false);
    setPosition(0.0);
}

void DeckPlayer::triggerCue()
{
    if (!loaded.load())
        return;

    if (playing.load())
    {
        // Pioneer DJ behavior: If playing, pause and jump to cue point
        pause();
        setPosition(cuePositionSeconds.load());
    }
    else
    {
        // If paused, jump to cue point and preview
        setPosition(cuePositionSeconds.load());
    }
}

void DeckPlayer::setCuePosition()
{
    if (loaded.load())
    {
        cuePositionSeconds.store(getPosition());
    }
}

void DeckPlayer::setPosition(double seconds, bool userInitiated)
{
    if (currentSampleRate <= 0.0)
        return;
    double dur = trackDurationSeconds.load();
    double target = seconds;
    if (dur > 0.0)
        target = juce::jlimit(0.0, dur, target);
    else
        target = juce::jmax(0.0, target);
    if (userInitiated)
        userSeekSerial.fetch_add(1);
    seekTargetSec.store(target);
    seekPending.store(true);

    // P25: un salto fuera de la zona ya separada reinicia el motor ahí (el
    // worker se entera por el par seekGen/target). Dentro de la zona cubierta
    // el anillo ya tiene el audio: no hace falta.
    // Con caché 2-stem en RAM toda la pista está cubierta: sin re-sembrado.
    if (stems2Engaged.load() && !stems2Cached.load())
    {
        const juce::int64 target44 = (juce::int64)(target * 44100.0);
        const juce::int64 wEnd = s2WriteEnd44.load();
        const juce::int64 cStart = juce::jmax(s2Origin44.load(),
                                              wEnd - (juce::int64)stems2RingSamples);
        if (target44 >= wEnd - 2 || target44 < cStart)
        {
            s2SeekTarget44.store(target44);
            s2SeekGen.fetch_add(1);
        }
    }
}

void DeckPlayer::applyPendingSeek(double targetSec)
{
    if (bufferingSource == nullptr || currentSampleRate <= 0.0 || stretcher == nullptr)
        return;
    // P26: el cursor vive en muestras de archivo — la conversión usa file-rate.
    double sr = origFileSampleRate > 0.0 ? origFileSampleRate : currentSampleRate;
    double dur = trackDurationSeconds.load();
    double target = targetSec;
    if (dur > 0.0)
        target = juce::jlimit(0.0, dur, target);
    else
        target = juce::jmax(0.0, target);

    // P3: con Key Lock, getPosition() descuenta la latencia del stretcher, asi
    // que el cursor de lectura tiene que compensarla. Sin esto setPosition() y
    // getPosition() no son inversas y cada seek relativo se anula (el
    // controlador de fase queda clavado a la latencia del stretcher).
    if (keylock.load())
        target += stretcher->getLatencySeconds();

    int64_t targetSample = (int64_t)(target * sr);
    targetSample = juce::jmax((int64_t)0, targetSample);
    bufferingSource->setNextReadPosition(targetSample);
    playbackPosSamples.store(targetSample);

    double rate = 1.0;
    if (keylock.load())
    {
        rate = juce::jlimit(0.02, 4.0, (double)1.0f + ((double)pitchPercent.load() / 100.0) + (double)pitchBend.load());
        rate *= rateFactor.load(); // P26: seek() pide el ratio entrada/salida efectivo
    }
    if (stretcher->reanchorWithPreRoll(bufferingSource.get(), targetSample, rate))
    {
    }
    else
    {
        stretcher->reset();
    }
}

double DeckPlayer::getPosition() const
{
    if (currentSampleRate <= 0.0)
        return 0.0;
    // P26: playbackPosSamples son muestras de archivo; el tiempo es sobre el
    // sample rate del archivo (trackDurationSeconds usa el mismo espacio).
    const double fileSr = origFileSampleRate > 0.0 ? origFileSampleRate : currentSampleRate;
    double pos = (double)playbackPosSamples.load() / fileSr;
    if (keylock.load() && stretcher != nullptr)
        pos -= stretcher->getLatencySeconds();
    if (pos < 0.0) pos = 0.0;
    return pos;
}

double DeckPlayer::getBeatPhase() const
{
    // P28: fase sobre el grid del archivo (file-time), igual que beatInterval.
    double bpm = originalBpm.load();
    if (bpm <= 0.0)
        bpm = getBpm();
    if (bpm <= 0.0) bpm = 120.0;
    double interval = 60.0 / bpm;
    if (interval <= 0.0) interval = 0.5;
    double offset = firstBeatOffsetSeconds.load();
    double pos = getPosition();
    double phase = std::fmod(pos - offset, interval);
    if (phase < 0.0) phase += interval;
    return phase / interval;
}

void DeckPlayer::seekRelative(double deltaSeconds, bool userInitiated)
{
    double cur = getPosition();
    double target = cur + deltaSeconds;
    double maxDur = trackDurationSeconds.load();
    if (maxDur > 0.0)
        target = juce::jlimit(0.0, maxDur, target);
    else
        target = juce::jmax(0.0, target);
    setPosition(target, userInitiated);
}

void DeckPlayer::setPitchPercent(float percent)
{
    float range = pitchRange.load();
    float clamped = juce::jlimit(-range, range, percent);
    pitchClamped.store(std::abs(percent - clamped) > 1e-6f);
    pitchPercent.store(clamped);
    currentBpm.store(originalBpm.load() * (1.0 + (double)clamped / 100.0));
    updatePlaybackSpeed();
}

void DeckPlayer::setPitchBend(float bend)
{
    pitchBend.store(bend);
    updatePlaybackSpeed();
}

void DeckPlayer::updatePlaybackSpeed()
{
    // 0% pitch = ratio 1.0; +8% pitch = ratio 1.08; -8% pitch = ratio 0.92
    float totalSpeed = 1.0f + (pitchPercent.load() / 100.0f) + pitchBend.load();
    totalSpeed = juce::jlimit(0.2f, 2.0f, totalSpeed);

    // P20: Key Shift — factor de transposición (±6 semitonos máx.)
    const int st = keyShiftSemitones.load();
    const double shift = std::pow(2.0, (double)st / 12.0);

    if (keylock.load())
    {
        // Key Lock: el tempo lo maneja el stretch, el resampler queda neutro.
        // La transposición la aplica el stretcher (no altera el tempo).
        stretcher->setTempoRate((double)totalSpeed);
        stretcher->setInputRateFactor(rateFactor.load()); // P26
        stretcher->setTransposeSemitones((double)st);
        if (resamplingSource != nullptr)
            resamplingSource->setResamplingRatio(1.0);
    }
    else if (resamplingSource != nullptr)
    {
        // Sin Key Lock la transposición va por resample: cambia pitch y tempo
        // a la vez (comportamiento clásico de giradiscos). P26: el factor
        // file→device corrige el tempo si el archivo no está al rate del device.
        resamplingSource->setResamplingRatio((double)totalSpeed * shift * rateFactor.load());
    }
}

void DeckPlayer::updateRateFactor()
{
    const double dev = currentSampleRate > 0.0 ? currentSampleRate : 44100.0;
    const double file = origFileSampleRate > 0.0 ? origFileSampleRate : 44100.0;
    rateFactor.store(juce::jlimit(0.25, 4.0, file / dev));
}

// ---------------- P22: stems ----------------

void DeckPlayer::startStemsLoad(const juce::String& dir, int generation)
{
    auto shared = analysis;
    analysisPool().addJob([this, shared, dir, generation]()
    {
        auto isCurrent = [&shared, generation]()
        {
            return shared->alive.load() && shared->generation.load() == generation;
        };

        static const char* names[] = { "vocals.wav", "drums.wav", "bass.wav", "other.wav" };
        std::array<juce::AudioBuffer<float>, 4> bufs;
        std::array<double, 4> srs { 0.0, 0.0, 0.0, 0.0 };

        for (int s = 0; s < 4; ++s)
        {
            if (!isCurrent())
                return;
            auto stemFile = juce::File(dir).getChildFile(names[s]);
            std::unique_ptr<juce::AudioFormatReader> r(formatManager.createReaderFor(stemFile));
            if (r == nullptr || r->lengthInSamples <= 0)
                return;
            srs[(size_t)s] = r->sampleRate;
            bufs[(size_t)s].setSize(2, (int)r->lengthInSamples, false, false, true);
            r->read(&bufs[(size_t)s], 0, (int)r->lengthInSamples, 0, true, true);
        }

        if (!isCurrent())
            return;

        publishIfCurrent(shared, generation, [this, local = std::move(bufs), srs]() mutable
        {
            for (int s = 0; s < 4; ++s)
            {
                stemBuffers[s] = std::move(local[(size_t)s]);
                stemSampleRate[s] = srs[(size_t)s];
            }
            stemsInRam.store(true);
        });
    });
}

bool DeckPlayer::tryAttachStemsIfReady()
{
    if (!loaded.load() || stemsReady.load())
        return false;
    const double durSec = trackDurationSeconds.load();
    if (durSec <= 0.0 || durSec > 900.0)
        return false;
    const auto stemDir = StemSeparationService::stemsDirFor(currentFile);
    if (stemDir.isEmpty())
        return false;
    stemsReady.store(true);
    startStemsLoad(stemDir, analysis->generation.load());
    return true;
}

float DeckPlayer::readStemInterp(const float* data, int length, double pos)
{
    if (pos < 0.0 || pos >= (double)(length - 1))
        return 0.0f;
    const int i0 = (int)pos;
    const float f = (float)(pos - (double)i0);
    return data[i0] * (1.0f - f) + data[i0 + 1] * f;
}

// ---------------- P25: stems 2-stem en streaming ----------------

float DeckPlayer::readStems2Ring(const juce::AudioBuffer<float>& ring, int ch, double pos44,
                                 juce::int64 origin44)
{
    const double n = (double)stems2RingSamples;
    double q = std::fmod(pos44 - (double)origin44, n);
    if (q < 0.0)
        q += n;
    const int i0 = (int)q;
    const int i1 = (i0 + 1 < stems2RingSamples) ? i0 + 1 : 0;
    const float f = (float)(q - (double)i0);
    const float* d = ring.getReadPointer(ch % ring.getNumChannels());
    return d[i0] * (1.0f - f) + d[i1] * f;
}

bool DeckPlayer::loadStems2Cache(const juce::File& cacheDir)
{
    if (cacheDir.getFullPathName().isEmpty())
        return false;
    const char* names[2] = { "vocals.wav", "instrumental.wav" };
    juce::AudioBuffer<float> bufs[2];
    for (int i = 0; i < 2; ++i)
    {
        auto reader = std::unique_ptr<juce::AudioFormatReader>(
            formatManager.createReaderFor(cacheDir.getChildFile(names[i])));
        // El engine emite a 44.1 kHz: la caché escrita por nosotros siempre
        // está a esa tasa. Cualquier desvío invalida la caché.
        if (reader == nullptr || std::abs(reader->sampleRate - 44100.0) > 0.5
            || reader->lengthInSamples < 2 || reader->numChannels < 1)
            return false;
        const auto len = (int)reader->lengthInSamples;
        bufs[i].setSize(2, len, false, false, true);
        bufs[i].clear();
        reader->read(&bufs[i], 0, len, 0, true, true);
        if (reader->numChannels < 2)
            bufs[i].copyFrom(1, 0, bufs[i], 0, 0, len);
    }
    stems2CacheBuf[0] = std::move(bufs[0]);
    stems2CacheBuf[1] = std::move(bufs[1]);
    stems2CacheLen44 = stems2CacheBuf[0].getNumSamples();
    stems2Cached.store(true); // publicado al final: el callback ve buffers completos
    return true;
}

bool DeckPlayer::loadStemsModeDefault()
{
    auto file = AudioEngine::getSettingsFile();
    if (file.existsAsFile())
    {
        auto json = juce::JSON::parse(file);
        if (json.isObject() && json.hasProperty("stemsMode"))
            return json["stemsMode"].toString().trim().equalsIgnoreCase("full");
    }
    return false; // Rápido (2-stem en streaming) es el default
}

void DeckPlayer::saveStemsModeDefault(bool precompute4)
{
    auto file = AudioEngine::getSettingsFile();
    juce::var json;
    if (file.existsAsFile())
        json = juce::JSON::parse(file);
    if (!json.isObject())
        json = juce::var(new juce::DynamicObject());
    if (auto* obj = json.getDynamicObject())
        obj->setProperty("stemsMode", precompute4 ? juce::String("full") : juce::String("fast"));
    auto parent = file.getParentDirectory();
    if (!parent.exists())
        parent.createDirectory();
    juce::FileOutputStream out(file);
    if (out.openedOk())
    {
        out.setPosition(0);
        out.truncate();
        out.writeString(juce::JSON::toString(json));
        out.flush();
    }
}

void DeckPlayer::startStems2Stream(std::shared_ptr<juce::AudioFormatReader> reader,
                                   const juce::File& audioFile, juce::int64 startOrigin44,
                                   int generation)
{
    auto shared = analysis;
    stems2Pool.addJob([this, shared, reader, audioFile, startOrigin44, generation]() mutable
    {
        auto isCurrent = [&shared, generation]()
        {
            return shared->alive.load() && shared->generation.load() == generation;
        };
        if (!isCurrent())
            return;

        // Motor persistente por deck (init ~0.1 s con la conversión CoreML ya
        // cacheada) y tocado solo por este worker: el pool de 1 hilo serializa.
        if (stems2Engine == nullptr)
            stems2Engine = std::make_unique<TwoStemEngine>();
        juce::String err;
        if (!stems2Engine->isInitialised() && !stems2Engine->init(&err))
        {
            stems2Engaged.store(false);
            return;
        }
        stems2Engine->reset();

        const double origRate = origFileSampleRate > 0.0 ? origFileSampleRate : 44100.0;
        const juce::int64 totalSrc = reader->lengthInSamples;
        constexpr int kFeed = 16384;                // muestras 44.1k por pasada
        constexpr juce::int64 kAhead44 = 44100 * 8; // ventana de adelanto
        constexpr int kOutCap = kFeed + TwoStemEngine::latencySamples + 4096;

        std::vector<float> inL((size_t)kFeed), inR((size_t)kFeed);
        std::vector<float> oVL((size_t)kOutCap), oVR((size_t)kOutCap),
            oIL((size_t)kOutCap), oIR((size_t)kOutCap);
        juce::AudioBuffer<float> srcBuf(2, kFeed + 8);

        // Lectura del archivo (rate original) con resample lineal a 44.1 kHz
        // solo si hace falta. rd = posición fraccional próxima, en muestras.
        const bool resample = std::abs(origRate - 44100.0) > 0.5;
        const double ratio = 44100.0 / origRate;
        double rd = 0.0;
        juce::int64 winStart = -1;
        juce::int64 winLen = 0;

        auto cursor44 = [&]() -> juce::int64
        {
            return (juce::int64)(((double)playbackPosSamples.load() / origRate) * 44100.0);
        };

        juce::int64 lastSeekGen = s2SeekGen.load();
        // El origen lo fija la carga (0), no playbackPosSamples: leerlo aquí
        // corre una carrera con play() inmediato y siembra en un punto adelantado.
        juce::int64 origin44 = startOrigin44;
        juce::int64 writeEnd44 = origin44;
        rd = (double)origin44 * (origRate / 44100.0);
        s2Origin44.store(origin44);
        s2WriteEnd44.store(writeEnd44);

        // P25 Fase D: escritura de la caché 2-stem. Solo si el stream arranca
        // en 0 y no hay seek: los writers son secuenciales, una re-siembra los
        // invalida. Al cubrir la pista completa se renombra a la caché final.
        const auto cacheDir = StemSeparationService::stems2CacheDirFor(audioFile);
        auto tmpV = cacheDir.getChildFile("vocals.part.wav");
        auto tmpI = cacheDir.getChildFile("instrumental.part.wav");
        juce::WavAudioFormat wavFmt;
        std::unique_ptr<juce::AudioFormatWriter> wV, wI;
        bool cacheDone = stems2CacheDone.load();
        auto killCache = [&]()
        {
            if (!cacheDone)
            {
                wV.reset();
                wI.reset();
                tmpV.deleteFile();
                tmpI.deleteFile();
            }
        };
        if (origin44 == 0 && !cacheDone && cacheDir.createDirectory().wasOk())
        {
            tmpV.deleteFile();
            tmpI.deleteFile();
            wV.reset(wavFmt.createWriterFor(new juce::FileOutputStream(tmpV), 44100.0, 2, 16, {}, 0));
            wI.reset(wavFmt.createWriterFor(new juce::FileOutputStream(tmpI), 44100.0, 2, 16, {}, 0));
            if (wV == nullptr || wI == nullptr)
            {
                wV.reset();
                wI.reset();
            }
        }

        auto fillFeed = [&]() -> int
        {
            if (rd >= (double)(totalSrc - 1))
                return 0;
            if (!resample)
            {
                // jmin con template explícito instanciaría SIMDRegister<int64> en GCC
                // (overload de juce_SIMDRegister_Impl.h) y no compila en Linux.
                const auto toRead = juce::jmin((juce::int64) kFeed, totalSrc - (juce::int64) rd);
                if (toRead <= 0)
                    return 0;
                if (winStart != (juce::int64)rd || winLen < toRead)
                {
                    winStart = (juce::int64)rd;
                    winLen = toRead;
                    srcBuf.setSize(2, (int)toRead, false, false, false);
                    reader->read(&srcBuf, 0, (int)toRead, winStart, true, true);
                }
                std::memcpy(inL.data(), srcBuf.getReadPointer(0), sizeof(float) * (size_t)toRead);
                std::memcpy(inR.data(), srcBuf.getReadPointer(1), sizeof(float) * (size_t)toRead);
                rd += (double)toRead;
                return (int)toRead;
            }
            int n = 0;
            while (n < kFeed && rd < (double)(totalSrc - 1))
            {
                const juce::int64 i0 = (juce::int64)rd;
                if (i0 < winStart || i0 + 1 >= winStart + winLen)
                {
                    winStart = i0;
                    winLen = juce::jmin((juce::int64) 65536, totalSrc - (juce::int64) i0);
                    srcBuf.setSize(2, (int)winLen, false, false, false);
                    reader->read(&srcBuf, 0, (int)winLen, winStart, true, true);
                }
                const float p = (float)(rd - (double)winStart);
                const int j0 = (int)p;
                const float f = p - (float)j0;
                const float* sl = srcBuf.getReadPointer(0);
                const float* srr = srcBuf.getReadPointer(1);
                inL[(size_t)n] = sl[j0] * (1.0f - f) + sl[j0 + 1] * f;
                inR[(size_t)n] = srr[j0] * (1.0f - f) + srr[j0 + 1] * f;
                rd += ratio;
                ++n;
            }
            return n;
        };

        bool flushed = false;
        while (!stems2Quit.load() && isCurrent())
        {
            // Seek fuera de cobertura: reinicia el motor en el destino.
            if (s2SeekGen.load() != lastSeekGen)
            {
                lastSeekGen = s2SeekGen.load();
                killCache(); // writers secuenciales: una re-siembra invalida la caché
                stems2Engine->reset();
                flushed = false;
                winStart = -1;
                winLen = 0;
                origin44 = s2SeekTarget44.load();
                writeEnd44 = origin44;
                rd = (double)origin44 * (origRate / 44100.0);
                s2Origin44.store(origin44);
                s2WriteEnd44.store(writeEnd44);
            }

            const bool allDone = flushed
                && writeEnd44 >= (juce::int64)(((double)totalSrc / origRate) * 44100.0);
            // Caché completa: los writers cubren [0, fin) → renombrar.
            if (allDone && (wV != nullptr || wI != nullptr))
            {
                wV.reset();
                wI.reset();
                tmpV.moveFileTo(cacheDir.getChildFile("vocals.wav"));
                tmpI.moveFileTo(cacheDir.getChildFile("instrumental.wav"));
                cacheDone = true;
                stems2CacheDone.store(true);
            }
            // El flush se drena siempre (el final de pista puede quedar sin
            // cubrir si el adelanto supera la ventana); el feed respeta la
            // ventana de adelanto.
            if (allDone || (!flushed && writeEnd44 - cursor44() >= kAhead44))
            {
                juce::Thread::sleep(25);
                continue;
            }

            int n = 0;
            if (!flushed)
            {
                n = fillFeed();
                if (n <= 0)
                    flushed = true;
            }

            int emitted = 0;
            if (n > 0)
                emitted = stems2Engine->process(inL.data(), inR.data(), n,
                                                oVL.data(), oVR.data(), oIL.data(), oIR.data(), kOutCap);
            else if (flushed)
                emitted = stems2Engine->flush(oVL.data(), oVR.data(), oIL.data(), oIR.data(), kOutCap);

            if (emitted > 0)
            {
                const int ringN = stems2RingSamples;
                const int idx = (int)(((writeEnd44 - origin44) % ringN + ringN) % ringN);
                const int first = juce::jmin(emitted, ringN - idx);
                auto* vL = stems2Ring[0].getWritePointer(0);
                auto* vR = stems2Ring[0].getWritePointer(1);
                auto* iL = stems2Ring[1].getWritePointer(0);
                auto* iR = stems2Ring[1].getWritePointer(1);
                std::memcpy(vL + idx, oVL.data(), sizeof(float) * (size_t)first);
                std::memcpy(vR + idx, oVR.data(), sizeof(float) * (size_t)first);
                std::memcpy(iL + idx, oIL.data(), sizeof(float) * (size_t)first);
                std::memcpy(iR + idx, oIR.data(), sizeof(float) * (size_t)first);
                if (first < emitted)
                {
                    const int rest = emitted - first;
                    std::memcpy(vL, oVL.data() + first, sizeof(float) * (size_t)rest);
                    std::memcpy(vR, oVR.data() + first, sizeof(float) * (size_t)rest);
                    std::memcpy(iL, oIL.data() + first, sizeof(float) * (size_t)rest);
                    std::memcpy(iR, oIR.data() + first, sizeof(float) * (size_t)rest);
                }
                if (wV != nullptr)
                {
                    const float* vc[2] = { oVL.data(), oVR.data() };
                    wV->writeFromFloatArrays(vc, 2, emitted);
                }
                if (wI != nullptr)
                {
                    const float* ic[2] = { oIL.data(), oIR.data() };
                    wI->writeFromFloatArrays(ic, 2, emitted);
                }
                if (!isCurrent())
                {
                    killCache();
                    return;
                }
                writeEnd44 += emitted;
                s2WriteEnd44.store(writeEnd44); // publicar DESPUÉS de escribir
            }
        }
        killCache();
    });
}

void DeckPlayer::setHotCue(int index, double positionSeconds)
{
    if (index >= 0 && index < 4)
    {
        // P28: cuantizar al beat más cercano del grid del archivo si hay BPM.
        // Si no hay BPM (currentBpm/originalBpm en 0) la posición queda cruda.
        if (originalBpm.load() > 0.0)
        {
            positionSeconds = quantizedBeatPos(positionSeconds, getFirstBeatOffset(),
                                               60.0 / originalBpm.load(), 0.0);
            positionSeconds = juce::jlimit(0.0, juce::jmax(0.0, getLengthInSeconds()),
                                           positionSeconds);
        }
        auto uIdx = static_cast<size_t>(index);
        hotCues[uIdx].store(positionSeconds);
        hotCuesActive[uIdx].store(true);
    }
}

void DeckPlayer::jumpToHotCue(int index)
{
    if (index >= 0 && index < 4)
    {
        auto uIdx = static_cast<size_t>(index);
        if (hotCuesActive[uIdx].load())
        {
            setPosition(hotCues[uIdx].load());
            play();
        }
    }
}

void DeckPlayer::clearHotCue(int index)
{
    if (index >= 0 && index < 4)
    {
        auto uIdx = static_cast<size_t>(index);
        hotCuesActive[uIdx].store(false);
    }
}

bool DeckPlayer::hasHotCue(int index) const
{
    if (index >= 0 && index < 4)
    {
        auto uIdx = static_cast<size_t>(index);
        return hotCuesActive[uIdx].load();
    }
    return false;
}

double DeckPlayer::getHotCue(int index) const
{
    if (index >= 0 && index < 4)
    {
        auto uIdx = static_cast<size_t>(index);
        return hotCues[uIdx].load();
    }
    return 0.0;
}

void DeckPlayer::setLoop(double startSeconds, double lengthSeconds, bool enabled)
{
    loopStartSeconds.store(startSeconds);
    loopLengthSeconds.store(lengthSeconds);
    loopActive.store(enabled);
}

void DeckPlayer::exitLoop()
{
    loopActive.store(false);
}

double DeckPlayer::quantizedBeatPos(double pos, double firstBeat, double interval, double beats)
{
    if (interval <= 0.0)
        return pos;
    const double idx = std::round((pos - firstBeat) / interval) + beats;
    return firstBeat + idx * interval;
}

double DeckPlayer::rollStartFor(double pos, double firstBeat, double interval, double beats)
{
    const double length = beats * interval;
    if (length <= 0.0)
        return pos;
    const double k = std::floor((pos - firstBeat) / length);
    return firstBeat + k * length;
}

double DeckPlayer::beatInterval() const
{
    // P28: el grid describe el ARCHIVO y las posiciones están en tiempo de
    // archivo (P26), así que el intervalo debe ser file-time (originalBpm),
    // no el BPM ajustado por pitch — con pitch ≠ 0 beat jump / roll derivaban.
    double bpm = originalBpm.load();
    if (bpm <= 0.0)
        bpm = currentBpm.load();
    if (bpm <= 0.0)
        bpm = 120.0;
    return 60.0 / bpm;
}

void DeckPlayer::jumpBeats(double beats)
{
    const double interval = beatInterval();
    if (interval <= 0.0)
        return;
    setPosition(quantizedBeatPos(getPosition(), getFirstBeatOffset(), interval, beats), true);
}

void DeckPlayer::startLoopRoll(double beats)
{
    const double interval = beatInterval();
    if (interval <= 0.0 || beats <= 0.0)
        return;
    const double start = juce::jmax(0.0, rollStartFor(getPosition(), getFirstBeatOffset(),
                                                      interval, beats));
    setLoop(start, beats * interval, true);
}

void DeckPlayer::releaseLoopRoll()
{
    if (!loopActive.load())
        return;
    const double end = loopStartSeconds.load() + loopLengthSeconds.load();
    loopActive.store(false);
    if (!slipEnabled.load())
        setPosition(end, false); // sin slip: continuar desde el fin del loop
    // con slip: el hilo de audio desengancha y retoma en la posicion virtual
}

void DeckPlayer::setGridBpm(double bpm)
{
    const double clamped = juce::jlimit(40.0, 250.0, bpm);
    gridBpmOverride.store(clamped);
    originalBpm.store(clamped);
    currentBpm.store(clamped * (1.0 + (double)pitchPercent.load() / 100.0));
}

void DeckPlayer::setFirstBeatOffset(double seconds)
{
    const double dur = juce::jmax(0.0, trackDurationSeconds.load());
    const double clamped = juce::jlimit(0.0, dur, seconds);
    gridOffsetOverride.store(clamped);
    firstBeatOffsetSeconds.store(clamped);
}

void DeckPlayer::resetGrid()
{
    gridBpmOverride.store(0.0);
    gridOffsetOverride.store(-1.0);
    const double bpm = detectedBpm.load();
    originalBpm.store(bpm);
    currentBpm.store(bpm * (1.0 + (double)pitchPercent.load() / 100.0));
    firstBeatOffsetSeconds.store(detectedOffsetSeconds.load());
}

juce::String DeckPlayer::getTrackTitle() const
{
    const juce::ScopedLock sl(lock);
    return trackTitle;
}

juce::String DeckPlayer::getTrackArtist() const
{
    const juce::ScopedLock sl(lock);
    return trackArtist;
}

juce::String DeckPlayer::getTrackKey() const
{
    const juce::ScopedLock sl(lock);
    return trackKey;
}

juce::String DeckPlayer::getTrackCamelot() const
{
    const juce::ScopedLock sl(lock);
    return trackCamelot;
}

juce::Colour DeckPlayer::getTrackKeyColour() const
{
    const juce::ScopedLock sl(lock);
    return KeyDetector::getCamelotColour(trackKey);
}

void DeckPlayer::computeQuickOverview(juce::AudioFormatReader* reader, size_t numPoints,
                                     std::vector<float>& low, std::vector<float>& mid,
                                     std::vector<float>& high)
{
    // Overview rápido: ventanas cortas repartidas por la pista. Coste
    // proporcional a numPoints (2000 lecturas de ~21 ms a 48 kHz), no a la
    // duración total: sirve para pintar algo real mientras llega el completo.
    low.assign(numPoints, 0.0f);
    mid.assign(numPoints, 0.0f);
    high.assign(numPoints, 0.0f);

    if (reader == nullptr || reader->lengthInSamples <= 0 || numPoints == 0)
        return;

    const int64_t totalSamples = reader->lengthInSamples;
    const int numCh = juce::jmax(1, (int)reader->numChannels);
    const int window = (int)juce::jmin<int64_t>(1024, totalSamples);
    if (window <= 0)
        return;

    juce::AudioBuffer<float> windowBuf(numCh, window);

    for (size_t p = 0; p < numPoints; ++p)
    {
        const int64_t pos = (int64_t)((double)p / (double)numPoints * (double)totalSamples);
        const int n = (int)juce::jmin<int64_t>((int64_t)window, totalSamples - pos);
        if (n <= 0 || !reader->read(&windowBuf, 0, n, pos, true, true))
            break;

        const float* left = windowBuf.getReadPointer(0);
        const float* right = (numCh > 1) ? windowBuf.getReadPointer(1) : nullptr;

        double peak = 0.0, sum = 0.0;
        for (int i = 0; i < n; ++i)
        {
            double v = std::abs((double)left[i]);
            if (right != nullptr)
                v = (v + std::abs((double)right[i])) * 0.5;
            peak = juce::jmax(peak, v);
            sum += v;
        }
        const double avg = sum / (double)n;

        // Misma matemática de bandas que el overview completo.
        low[p]  = (float)juce::jlimit(0.0, 1.0, peak * 0.95);
        mid[p]  = (float)juce::jlimit(0.0, 1.0, avg * 1.5);
        high[p] = (float)juce::jlimit(0.0, 1.0, (peak - avg) * 1.25);
    }

    normaliseBand(low);
    normaliseBand(mid);
    normaliseBand(high);
}

void DeckPlayer::computeFullOverview(juce::AudioFormatReader* reader, size_t numPoints,
                                     std::vector<float>& low, std::vector<float>& mid,
                                     std::vector<float>& high)
{
    // Alta resolución para el vertical con zoom (8000 pts ≈ 100 KB).
    // Una sola pasada secuencial: sin seeks dispersos, óptimo en MP3.
    low.assign(numPoints, 0.0f);
    mid.assign(numPoints, 0.0f);
    high.assign(numPoints, 0.0f);

    if (reader == nullptr || reader->lengthInSamples <= 0 || numPoints == 0)
        return;

    int64_t totalSamples = reader->lengthInSamples;
    const int numCh = juce::jmax(1, (int)reader->numChannels);

    std::vector<double> ptPeak(numPoints, 0.0);
    std::vector<double> ptSum(numPoints, 0.0);
    std::vector<int64_t> ptCount(numPoints, 0);

    const int chunkSize = 65536;
    juce::AudioBuffer<float> chunkBuf(numCh, chunkSize);
    int64_t pos = 0;
    const double invTotalSamples = (totalSamples > 0) ? ((double)numPoints / (double)totalSamples) : 0.0;

    while (pos < totalSamples)
    {
        int n = (int)juce::jmin<int64_t>((int64_t)chunkSize, totalSamples - pos);
        if (!reader->read(&chunkBuf, 0, n, pos, true, true))
            break;

        const float* left = chunkBuf.getReadPointer(0);
        const float* right = (numCh > 1) ? chunkBuf.getReadPointer(1) : nullptr;

        for (int i = 0; i < n; ++i)
        {
            float v = std::abs(left[i]);
            if (right != nullptr)
                v = (v + std::abs(right[i])) * 0.5f;

            size_t p = static_cast<size_t>(static_cast<double>(pos + i) * invTotalSamples);
            if (p >= numPoints) p = numPoints - 1;

            if ((double)v > ptPeak[p]) ptPeak[p] = (double)v;
            ptSum[p] += (double)v;
            ptCount[p]++;
        }
        pos += n;
    }

    for (size_t p = 0; p < numPoints; ++p)
    {
        float peak = (float)ptPeak[p];
        float avg = ptCount[p] > 0 ? (float)(ptSum[p] / (double)ptCount[p]) : 0.0f;
        low[p]  = juce::jlimit(0.0f, 1.0f, peak * 0.95f);
        mid[p]  = juce::jlimit(0.0f, 1.0f, avg * 1.5f);
        high[p] = juce::jlimit(0.0f, 1.0f, (peak - avg) * 1.25f);
    }

    normaliseBand(low);
    normaliseBand(mid);
    normaliseBand(high);
}




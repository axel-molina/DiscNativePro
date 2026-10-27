#include "DeckPlayer.h"
#include "BeatDetector.h"
#include "KeyDetector.h"
#include "StretchAdapter.h"
#include <juce_events/juce_events.h>
#include <cmath>
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
    scratchRing.setSize(2, scratchRingSize);
    scratchRing.clear();
    scratchTail.setSize(2, scratchTailSize);
    scratchTail.clear();
    playbackPosSamples.store(0);
    seekPending.store(false);
    seekTargetSec.store(0.0);
    firstBeatOffsetSeconds.store(0.0);
}
DeckPlayer::~DeckPlayer()
{
    // Ningún trabajo pendiente vuelve a tocar este deck (ver publishIfCurrent).
    // El DeckPlayer se destruye en el hilo de mensajes, igual que las
    // publicaciones diferidas, así que la bandera basta como barrera.
    if (analysis != nullptr)
        analysis->alive.store(false);

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

        // P3 sobre P5: la pista nueva no hereda seek pendiente, fase ni
        // posicion de la anterior (el analisis llega despues y repone el offset).
        seekPending.store(false);
        seekTargetSec.store(0.0);
        playbackPosSamples.store(0);
        firstBeatOffsetSeconds.store(0.0);

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

        loaded.store(true);
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
            originalBpm.store(bpm);
            currentBpm.store(bpm * (1.0 + (double)pitchPercent.load() / 100.0));
            firstBeatOffsetSeconds.store(offset);
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
        double dur = trackDurationSeconds.load();
        int N = bufferToFill.numSamples;
        double rate = (double)scratchVelRevPerSec.load() * 1.8; // audio-sec por sec, con signo

        // P3: la aguja y el tiempo siguen a la mano tambien durante el scratch.
        // getPosition() descuenta la latencia del stretcher, asi que se repone
        // aqui para que la posicion publicada sea la que se esta oyendo.
        auto publishScratchPos = [&]()
        {
            const double latency = keylock.load() ? stretcher->getLatencySeconds() : 0.0;
            playbackPosSamples.store((juce::int64)((scratchAnchorSec + latency) * sr));
        };

        if (!scratchAnchorInit)
        {
            scratchAnchorSec = (double)bufferingSource->getNextReadPosition() / sr;
            scratchDir = 0;
            scratchAnchorInit = true;
            scratchTail.clear();
        }

        int dir = (rate > 0.005) ? 1 : ((rate < -0.005) ? -1 : 0);
        if (dir != scratchDir)
        {
            if (dir >= 0)
                bufferingSource->setNextReadPosition((juce::int64)(scratchAnchorSec * sr));
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
                resamplingSource->setResamplingRatio(juce::jlimit(0.02, 4.0, rate));
                resamplingSource->getNextAudioBlock(bufferToFill);
            }
            writeScratchRing(bufferToFill); // lo escuchado queda en historial
            scratchAnchorSec = (double)bufferingSource->getNextReadPosition() / sr;
            if (dur > 0.0 && scratchAnchorSec > dur)
                scratchAnchorSec = dur; // tope sin auto-stop
            publishScratchPos();
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
        bufferingSource->setNextReadPosition((juce::int64)(scratchAnchorSec * sr)); // cursor = aguja
        publishScratchPos();
        return; // sin loop-wrap ni auto-stop durante el toque
    }

    // Seek seguro: el pendiente ya se aplico al inicio del bloque (tambien en
    // pausa), antes de decidir si hay audio que renderizar.

    // Process audio block from streaming reader.
    // Key Lock ON: tempo por stretch (pitch intacto). OFF: resample clásico.
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
    writeScratchRing(bufferToFill); // historial para la reversa del scratch

    // Actualizar posición atómica sin lock para getPosition
    if (bufferingSource != nullptr && currentSampleRate > 0.0)
    {
        auto rp = bufferingSource->getNextReadPosition();
        playbackPosSamples.store(rp);
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

void DeckPlayer::setPosition(double seconds)
{
    if (currentSampleRate <= 0.0)
        return;
    double dur = trackDurationSeconds.load();
    double target = seconds;
    if (dur > 0.0)
        target = juce::jlimit(0.0, dur, target);
    else
        target = juce::jmax(0.0, target);
    seekTargetSec.store(target);
    seekPending.store(true);
}

void DeckPlayer::applyPendingSeek(double targetSec)
{
    if (bufferingSource == nullptr || currentSampleRate <= 0.0 || stretcher == nullptr)
        return;
    double sr = currentSampleRate;
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
        rate = juce::jlimit(0.02, 4.0, (double)1.0f + ((double)pitchPercent.load() / 100.0) + (double)pitchBend.load());
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
    double pos = (double)playbackPosSamples.load() / currentSampleRate;
    if (keylock.load() && stretcher != nullptr)
        pos -= stretcher->getLatencySeconds();
    if (pos < 0.0) pos = 0.0;
    return pos;
}

double DeckPlayer::getBeatPhase() const
{
    double bpm = getBpm();
    if (bpm <= 0.0) bpm = 120.0;
    double interval = 60.0 / bpm;
    if (interval <= 0.0) interval = 0.5;
    double offset = firstBeatOffsetSeconds.load();
    double pos = getPosition();
    double phase = std::fmod(pos - offset, interval);
    if (phase < 0.0) phase += interval;
    return phase / interval;
}

void DeckPlayer::seekRelative(double deltaSeconds)
{
    double cur = getPosition();
    double target = cur + deltaSeconds;
    double maxDur = trackDurationSeconds.load();
    if (maxDur > 0.0)
        target = juce::jlimit(0.0, maxDur, target);
    else
        target = juce::jmax(0.0, target);
    setPosition(target);
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
    if (keylock.load())
    {
        // Key Lock: el tempo lo maneja el stretch, el resampler queda neutro.
        stretcher->setTempoRate((double)totalSpeed);
        if (resamplingSource != nullptr)
            resamplingSource->setResamplingRatio(1.0);
    }
    else if (resamplingSource != nullptr)
    {
        resamplingSource->setResamplingRatio((double)totalSpeed);
    }
}

void DeckPlayer::setHotCue(int index, double positionSeconds)
{
    if (index >= 0 && index < 4)
    {
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




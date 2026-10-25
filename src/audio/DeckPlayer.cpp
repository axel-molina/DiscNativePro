#include "DeckPlayer.h"
#include "BeatDetector.h"
#include "KeyDetector.h"
#include "StretchAdapter.h"
#include <cmath>
#include <cstring>
#include <vector>

DeckPlayer::DeckPlayer(juce::AudioFormatManager& fm, juce::TimeSliceThread& thread)
    : formatManager(fm), diskThread(thread),
      stretcher(std::make_unique<StretchAdapter>())
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
    releaseResources();
}

bool DeckPlayer::loadFile(const juce::File& audioFile)
{
    if (!audioFile.existsAsFile())
        return false;

    auto* reader = formatManager.createReaderFor(audioFile);
    if (reader == nullptr)
        return false;

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

    // Native C++ BPM and transient analysis
    auto beatRes = BeatDetector::analyze(reader);
    originalBpm.store(beatRes.bpm);
    currentBpm.store(beatRes.bpm);
    firstBeatOffsetSeconds.store(beatRes.offset);

    // Native C++ Pro Harmonic Key Detection (Camelot + Musical key)
    auto keyRes = KeyDetector::analyze(reader);
    trackKey = (keyRes.formattedKey.isNotEmpty() && keyRes.formattedKey != "--") ? keyRes.formattedKey : keyRes.camelot;
    trackCamelot = keyRes.camelot;

    // Extract 3-band waveform overview peaks
    extractWaveformOverview(reader);

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
    bufferingSource = std::make_unique<juce::BufferingAudioSource>(readerSource.get(), diskThread, false, bufferSize);

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

    return true;
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
    if (!loaded.load() || !playing.load())
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

    // Scratch v3: pitch sigue a la mano. Adelante = resampler existente con
    // ratio dinámico (barrido de pitch real). Atrás = render invertido real
    // con pitch-tracking. Transiciones con crossfade; transporte congelado.
    if (scratchTouch.load())
    {
        double sr = currentSampleRate > 0.0 ? currentSampleRate : 44100.0;
        double dur = trackDurationSeconds.load();
        int N = bufferToFill.numSamples;
        double rate = (double)scratchVelRevPerSec.load() * 1.8; // audio-sec por sec, con signo

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
        return; // sin loop-wrap ni auto-stop durante el toque
    }

    // Seek seguro: aplicar al inicio del bloque si hay uno pendiente
    if (seekPending.exchange(false))
    {
        applyPendingSeek(seekTargetSec.load());
    }

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
    pitchPercent.store(percent);
    currentBpm.store(originalBpm.load() * (1.0 + (double)percent / 100.0));
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

void DeckPlayer::extractWaveformOverview(juce::AudioFormatReader* reader)
{
    // Alta resolución para el vertical con zoom (8000 pts ≈ 100 KB).
    // Una sola pasada secuencial: sin seeks dispersos, óptimo en MP3.
    const size_t numPoints = 8000;
    peaksLow.assign(numPoints, 0.0f);
    peaksMid.assign(numPoints, 0.0f);
    peaksHigh.assign(numPoints, 0.0f);

    if (reader == nullptr || reader->lengthInSamples <= 0)
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
        peaksLow[p]  = juce::jlimit(0.0f, 1.0f, peak * 0.95f);
        peaksMid[p]  = juce::jlimit(0.0f, 1.0f, avg * 1.5f);
        peaksHigh[p] = juce::jlimit(0.0f, 1.0f, (peak - avg) * 1.25f);
    }

    // Normaliza cada banda a su máximo: los temas con master bajo muestran
    // picos igual que los altos (beneficia al overview y al vertical).
    float maxL = 1e-6f, maxM = 1e-6f, maxH = 1e-6f;
    for (size_t p = 0; p < numPoints; ++p)
    {
        maxL = juce::jmax(maxL, peaksLow[p]);
        maxM = juce::jmax(maxM, peaksMid[p]);
        maxH = juce::jmax(maxH, peaksHigh[p]);
    }
    for (size_t p = 0; p < numPoints; ++p)
    {
        peaksLow[p] /= maxL;
        peaksMid[p] /= maxM;
        peaksHigh[p] /= maxH;
    }
}




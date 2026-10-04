#include "DeckPlayer.h"
#include "BeatDetector.h"

DeckPlayer::DeckPlayer(juce::AudioFormatManager& fm, juce::TimeSliceThread& thread)
    : formatManager(fm), diskThread(thread)
{
    for (size_t i = 0; i < hotCues.size(); ++i)
    {
        hotCues[i].store(0.0);
        hotCuesActive[i].store(false);
    }
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

    double duration = (double)reader->lengthInSamples / reader->sampleRate;
    trackDurationSeconds.store(duration);
    cuePositionSeconds.store(0.0);

    // Native C++ BPM and transient analysis
    auto beatRes = BeatDetector::analyze(reader);
    originalBpm.store(beatRes.bpm);
    currentBpm.store(beatRes.bpm);

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
    readerSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);

    // 2. BufferingAudioSource reads ahead in background thread (approx 4 seconds of buffer)
    // Memory footprint is tiny (~1-2MB) even for 2-hour long sets!
    constexpr int bufferSize = 32768 * 4;
    bufferingSource = std::make_unique<juce::BufferingAudioSource>(readerSource.get(), diskThread, false, bufferSize);

    // 3. ResamplingAudioSource allows smooth pitch / tempo changes (+/- 8%, etc.)
    resamplingSource = std::make_unique<juce::ResamplingAudioSource>(bufferingSource.get(), false, 2);

    if (currentSampleRate > 0.0)
    {
        resamplingSource->prepareToPlay(512, currentSampleRate);
    }

    updatePlaybackSpeed();
    loaded.store(true);

    return true;
}

void DeckPlayer::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    currentSampleRate = sampleRate;

    const juce::ScopedLock sl(lock);
    if (resamplingSource != nullptr)
    {
        resamplingSource->prepareToPlay(samplesPerBlockExpected, sampleRate);
        updatePlaybackSpeed();
    }
}

void DeckPlayer::releaseResources()
{
    const juce::ScopedLock sl(lock);
    if (resamplingSource != nullptr)
        resamplingSource->releaseResources();
}

void DeckPlayer::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    if (!loaded.load() || !playing.load())
    {
        bufferToFill.clearActiveBufferRegion();
        return;
    }

    const juce::ScopedLock sl(lock);
    if (resamplingSource == nullptr)
    {
        bufferToFill.clearActiveBufferRegion();
        return;
    }

    // Process audio block from streaming reader
    resamplingSource->getNextAudioBlock(bufferToFill);

    // Handle Active Loop
    if (loopActive.load() && bufferingSource != nullptr)
    {
        double currentPos = getPosition();
        double start = loopStartSeconds.load();
        double length = loopLengthSeconds.load();

        if (currentPos >= (start + length))
        {
            setPosition(start);
        }
    }

    // Check End of Track
    if (getPosition() >= trackDurationSeconds.load())
    {
        playing.store(false);
        setPosition(trackDurationSeconds.load());
    }
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
    const juce::ScopedLock sl(lock);
    if (bufferingSource != nullptr && currentSampleRate > 0.0)
    {
        int64_t samplePos = (int64_t)(seconds * currentSampleRate);
        bufferingSource->setNextReadPosition(juce::jmax((int64_t)0, samplePos));
    }
}

double DeckPlayer::getPosition() const
{
    const juce::ScopedLock sl(lock);
    if (bufferingSource != nullptr && currentSampleRate > 0.0)
    {
        return (double)bufferingSource->getNextReadPosition() / currentSampleRate;
    }
    return 0.0;
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
    if (resamplingSource != nullptr)
    {
        // 0% pitch = ratio 1.0; +8% pitch = ratio 1.08; -8% pitch = ratio 0.92
        float totalSpeed = 1.0f + (pitchPercent.load() / 100.0f) + pitchBend.load();
        totalSpeed = juce::jlimit(0.2f, 2.0f, totalSpeed);
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

void DeckPlayer::extractWaveformOverview(juce::AudioFormatReader* reader)
{
    const size_t numPoints = 600;
    peaksLow.assign(numPoints, 0.0f);
    peaksMid.assign(numPoints, 0.0f);
    peaksHigh.assign(numPoints, 0.0f);

    if (reader == nullptr || reader->lengthInSamples <= 0)
        return;

    int64_t totalSamples = reader->lengthInSamples;
    int samplesPerPoint = (int)(totalSamples / (int64_t)numPoints);
    if (samplesPerPoint <= 0) samplesPerPoint = 1;

    int sliceSize = juce::jmin(256, samplesPerPoint);
    juce::AudioBuffer<float> tempBuffer(1, sliceSize);

    for (size_t p = 0; p < numPoints; ++p)
    {
        int64_t startPos = (int64_t)p * samplesPerPoint;
        tempBuffer.clear();
        reader->read(&tempBuffer, 0, sliceSize, startPos, true, false);

        const float* data = tempBuffer.getReadPointer(0);
        float peak = 0.0f;
        float sum = 0.0f;
        for (int i = 0; i < sliceSize; ++i)
        {
            float val = std::abs(data[i]);
            if (val > peak) peak = val;
            sum += val;
        }
        float avg = sum / (float)sliceSize;

        peaksLow[p]  = juce::jlimit(0.0f, 1.0f, peak * 0.95f);
        peaksMid[p]  = juce::jlimit(0.0f, 1.0f, avg * 1.5f);
        peaksHigh[p] = juce::jlimit(0.0f, 1.0f, (peak - avg) * 1.25f);
    }
}


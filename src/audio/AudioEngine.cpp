#include "AudioEngine.h"
#if JUCE_WINDOWS
#include "MediaFoundationAudioFormat_win.h"
#endif

AudioEngine::AudioEngine()
    : deck1(formatManager, diskReaderThread),
      deck2(formatManager, diskReaderThread)
{
    // Register audio formats (WAV, AIFF, FLAC, OGG, and CoreAudio MP3/AAC/M4A on macOS are all in basic formats)
    formatManager.registerBasicFormats();
#if JUCE_WINDOWS
    formatManager.registerFormat(new MediaFoundationAudioFormat(), false);
#endif
}

AudioEngine::~AudioEngine()
{
    shutdown();
}

bool AudioEngine::initialise()
{
    isShuttingDown.store(false);

    // Start background disk streaming thread (priority normal for smooth file prefetching)
    diskReaderThread.startThread(juce::Thread::Priority::normal);

    // Initialise audio device manager with 0 inputs, up to 4 outputs (Master Ch 1-2, Phones Ch 3-4)
    auto err = deviceManager.initialiseWithDefaultDevices(0, 4);
    if (!err.isEmpty())
    {
        DBG("Audio device error: " << err);
        return false;
    }

    // Ensure all available physical output channels (up to 4) are activated
    auto setup = deviceManager.getAudioDeviceSetup();
    if (auto* currentDev = deviceManager.getCurrentAudioDevice())
    {
        auto outNames = currentDev->getOutputChannelNames();
        int numAvail = outNames.size();
        currentNumOutputChannels.store(numAvail);
        setup.outputChannels.setRange(0, juce::jmin(4, numAvail), true);
        deviceManager.setAudioDeviceSetup(setup, true);
    }

    deviceManager.addAudioCallback(this);
    deviceManager.addChangeListener(this);
    return true;
}

void AudioEngine::shutdown()
{
    isShuttingDown.store(true);
    cancelPendingUpdate();
    audioRecorder.stopRecording();
    deviceManager.removeChangeListener(this);
    deviceManager.removeAudioCallback(this);
    deviceManager.closeAudioDevice();
    diskReaderThread.stopThread(2000);
}

juce::File AudioEngine::getSettingsFile()
{
    auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                       .getChildFile("DiscNativePro");
    if (!appData.exists())
        appData.createDirectory();
    return appData.getChildFile("settings.json");
}

juce::File AudioEngine::getDefaultRecordingDirectory() const
{
    return juce::File::getSpecialLocation(juce::File::userMusicDirectory).getChildFile("DiscPro_Recordings");
}

juce::File AudioEngine::getRecordingDirectory() const
{
    if (customRecordDir.isDirectory())
        return customRecordDir;

    auto file = getSettingsFile();
    if (file.existsAsFile())
    {
        auto json = juce::JSON::parse(file);
        if (json.isObject() && json.hasProperty("recordingDirectory"))
        {
            auto path = json["recordingDirectory"].toString().trim();
            if (path.isNotEmpty())
            {
                juce::File dir(path);
                if (dir.isDirectory() || dir.exists())
                {
                    customRecordDir = dir;
                    return customRecordDir;
                }
            }
        }
    }
    return getDefaultRecordingDirectory();
}

void AudioEngine::setRecordingDirectory(const juce::File& dir)
{
    customRecordDir = dir;
    auto file = getSettingsFile();
    juce::var json;
    if (file.existsAsFile())
        json = juce::JSON::parse(file);
    if (!json.isObject())
        json = juce::var(new juce::DynamicObject());

    if (auto* obj = json.getDynamicObject())
    {
        if (dir == getDefaultRecordingDirectory() || !dir.exists())
            obj->removeProperty("recordingDirectory");
        else
            obj->setProperty("recordingDirectory", dir.getFullPathName());
    }
    file.replaceWithText(juce::JSON::toString(json));
}

bool AudioEngine::toggleRecording(juce::String& outFilePath)
{
    if (audioRecorder.isRecording())
    {
        outFilePath = audioRecorder.getCurrentFile().getFullPathName();
        audioRecorder.stopRecording();
        return false; // Stopped
    }

    auto recDir = getRecordingDirectory();
    if (!recDir.exists())
        recDir.createDirectory();

    auto timestamp = juce::Time::getCurrentTime().formatted("%Y-%m-%d_%H%M%S");
    auto recFile = recDir.getChildFile("DiscPro_Mix_" + timestamp + ".wav");

    bool success = audioRecorder.startRecording(recFile, currentSampleRate, 2);
    if (success)
    {
        outFilePath = recFile.getFullPathName();
        return true; // Started
    }

    return false;
}

void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    if (device == nullptr)
        return;

    currentSampleRate = device->getCurrentSampleRate();
    currentBlockSize = device->getCurrentBufferSizeSamples();
    currentNumOutputChannels.store(device->getActiveOutputChannels().countNumberOfSetBits());

    // Preallocate buffers for zero runtime heap allocation (real-time safe)
    deck1Buffer.setSize(2, currentBlockSize, false, true, true);
    deck2Buffer.setSize(2, currentBlockSize, false, true, true);
    masterBuffer.setSize(2, currentBlockSize, false, true, true);
    cueBuffer.setSize(2, currentBlockSize, false, true, true);

    deck1.prepareToPlay(currentBlockSize, currentSampleRate);
    deck2.prepareToPlay(currentBlockSize, currentSampleRate);
    djMixer.prepare(currentSampleRate, currentBlockSize);
}

void AudioEngine::audioDeviceStopped()
{
    audioRecorder.stopRecording();
    deck1.releaseResources();
    deck2.releaseResources();
    djMixer.reset();

    // Si el dispositivo se detuvo inesperadamente (ej. WebKit o CoreAudio),
    // programar recuperacion asincrona en el hilo de mensajes
    if (!isShuttingDown.load())
    {
        triggerAsyncUpdate();
    }
}

void AudioEngine::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &deviceManager && !isShuttingDown.load())
    {
        ensureAudioDeviceRunning();
    }
}

void AudioEngine::handleAsyncUpdate()
{
    if (!isShuttingDown.load())
    {
        ensureAudioDeviceRunning();
    }
}

bool AudioEngine::ensureAudioDeviceRunning()
{
    if (isShuttingDown.load())
        return false;

    auto* dev = deviceManager.getCurrentAudioDevice();
    if (dev != nullptr && dev->isPlaying())
        return true;

    bool expected = false;
    if (!isRecoveringDevice.compare_exchange_strong(expected, true))
        return false;

    DBG("[AudioEngine] Audio device is not running (dev=" << (dev != nullptr ? dev->getName() : "null")
        << ", playing=" << (dev != nullptr && dev->isPlaying()) << "). Recovering...");

    // 1. Si hay un dispositivo existente detenido, cerrarlo explicitamente primero
    // para que restartLastAudioDevice no sea un no-op en JUCE.
    deviceManager.closeAudioDevice();
    deviceManager.restartLastAudioDevice();

    dev = deviceManager.getCurrentAudioDevice();
    if (dev != nullptr && dev->isPlaying())
    {
        DBG("[AudioEngine] Restored audio device successfully via restartLastAudioDevice");
        isRecoveringDevice.store(false);
        return true;
    }

    // 2. Si restartLastAudioDevice fallo, re-inicializar con dispositivos predeterminados
    auto err = deviceManager.initialiseWithDefaultDevices(0, 4);
    if (err.isEmpty())
    {
        auto setup = deviceManager.getAudioDeviceSetup();
        if (auto* newDev = deviceManager.getCurrentAudioDevice())
        {
            auto outNames = newDev->getOutputChannelNames();
            int numAvail = outNames.size();
            currentNumOutputChannels.store(numAvail);
            setup.outputChannels.setRange(0, juce::jmin(4, numAvail), true);
            deviceManager.setAudioDeviceSetup(setup, true);
        }
        deviceManager.addAudioCallback(this);
        DBG("[AudioEngine] Restored audio device successfully via initialiseWithDefaultDevices");
        isRecoveringDevice.store(false);
        return true;
    }

    DBG("[AudioEngine] Error restoring audio device: " << err);
    isRecoveringDevice.store(false);
    return false;
}

void AudioEngine::audioDeviceIOCallbackWithContext(const float* const* /*inputChannelData*/,
                                                  int /*numInputChannels*/,
                                                  float* const* outputChannelData,
                                                  int numOutputChannels,
                                                  int numSamples,
                                                  const juce::AudioIODeviceCallbackContext& /*context*/)
{
    if (outputChannelData == nullptr || numOutputChannels == 0 || numSamples <= 0)
        return;

    // Ensure our internal buffers match current block samples
    deck1Buffer.clear(0, numSamples);
    deck2Buffer.clear(0, numSamples);
    cueBuffer.clear(0, numSamples);

    // 1. Fetch audio from streaming deck sources
    juce::AudioSourceChannelInfo d1Info(&deck1Buffer, 0, numSamples);
    deck1.getNextAudioBlock(d1Info);

    juce::AudioSourceChannelInfo d2Info(&deck2Buffer, 0, numSamples);
    deck2.getNextAudioBlock(d2Info);

    // 2. Process DSP: Channel EQ, Color Filters, Deck FX Chains, Channel Faders, Crossfader, Master Gain, Cue Preescucha & Meters
    djMixer.setDeckBpm(0, static_cast<float>(deck1.getBpm()));
    djMixer.setDeckBpm(1, static_cast<float>(deck2.getBpm()));
    djMixer.process(deck1Buffer, deck2Buffer, masterBuffer, cueBuffer, numSamples);

    // 2b. Sampler one-shots al master post-crossfader (entran a la grabación)
    samplerBank.process(masterBuffer, numSamples);

    // 3. Push to master recorder (lock-free, zero allocation)
    audioRecorder.pushAudioBlock(masterBuffer, numSamples);

    // 4. Write to physical audio output (DAC / CoreAudio)
    int masterPair = masterChannelPair.load(); // 0 = Ch 1-2, 1 = Ch 3-4
    int cuePair = cueChannelPair.load();       // 0 = Ch 1-2, 1 = Ch 3-4

    if (numOutputChannels >= 4)
    {
        for (int ch = 0; ch < numOutputChannels; ++ch)
        {
            auto* out = outputChannelData[ch];
            if (ch == masterPair * 2)
            {
                std::memcpy(out, masterBuffer.getReadPointer(0), sizeof(float) * (size_t)numSamples);
            }
            else if (ch == masterPair * 2 + 1)
            {
                std::memcpy(out, masterBuffer.getReadPointer(1), sizeof(float) * (size_t)numSamples);
            }
            else if (ch == cuePair * 2)
            {
                std::memcpy(out, cueBuffer.getReadPointer(0), sizeof(float) * (size_t)numSamples);
            }
            else if (ch == cuePair * 2 + 1)
            {
                std::memcpy(out, cueBuffer.getReadPointer(1), sizeof(float) * (size_t)numSamples);
            }
            else
            {
                std::memset(out, 0, sizeof(float) * (size_t)numSamples);
            }
        }
    }
    else
    {
        for (int ch = 0; ch < numOutputChannels; ++ch)
        {
            if (ch < 2)
            {
                auto* out = outputChannelData[ch];
                const auto* src = masterBuffer.getReadPointer(ch);
                std::memcpy(out, src, sizeof(float) * (size_t)numSamples);
            }
            else
            {
                std::memset(outputChannelData[ch], 0, sizeof(float) * (size_t)numSamples);
            }
        }
    }
}

void AudioEngine::setTempoBlend(bool active)
{
    tempoBlendActive.store(active);
    if (active)
    {
        applyTempoBlend(currentCrossfaderVal.load());
    }
}

bool AudioEngine::syncDeckTo(int deckIndex)
{
    auto& self  = getDeck(deckIndex);
    auto& other = getDeck(deckIndex == 0 ? 1 : 0);

    if (!other.isLoaded() || other.getBpm() <= 0.0 || self.getOriginalBpm() <= 0.0)
        return false;

    self.setPitchPercent((float)(((other.getBpm() / self.getOriginalBpm()) - 1.0) * 100.0));
    return true;
}

void AudioEngine::setCrossfader(float val)
{
    val = juce::jlimit(-1.0f, 1.0f, val);
    currentCrossfaderVal.store(val);
    djMixer.setCrossfader(val);

    if (tempoBlendActive.load() && deck1.isLoaded() && deck2.isLoaded())
    {
        CrossfaderZone currentZone = (val <= -0.75f) ? CrossfaderZone::Deck1
                                   : (val >=  0.75f) ? CrossfaderZone::Deck2
                                                     : CrossfaderZone::Center;

        // Check for zone transition into center to align beat phase cleanly
        if (lastCrossfaderZone == CrossfaderZone::Deck1 && currentZone == CrossfaderZone::Center
            && deck1.isPlaying() && deck2.isPlaying())
        {
            alignBeatPhase(deck1, deck2);
        }
        else if (lastCrossfaderZone == CrossfaderZone::Deck2 && currentZone == CrossfaderZone::Center
                 && deck1.isPlaying() && deck2.isPlaying())
        {
            alignBeatPhase(deck2, deck1);
        }
        lastCrossfaderZone = currentZone;

        applyTempoBlend(val);
    }
}

void AudioEngine::applyTempoBlend(float val)
{
    if (!tempoBlendActive.load()) return;
    if (!deck1.isLoaded() || !deck2.isLoaded()) return;

    double b1 = deck1.getOriginalBpm();
    double b2 = deck2.getOriginalBpm();
    if (b1 <= 0.0) b1 = 124.0;
    if (b2 <= 0.0) b2 = 126.0;

    // Normalized crossfader: 0.0 (Deck 1) to 1.0 (Deck 2)
    double t = juce::jlimit(0.0, 1.0, (val + 1.0) / 2.0);

    // Interpolated target BPM for the entire mix
    double targetBpm = b1 * (1.0 - t) + b2 * t;

    // Calculate required pitch percent offsets
    double pitchPercent1 = ((targetBpm / b1) - 1.0) * 100.0;
    double pitchPercent2 = ((targetBpm / b2) - 1.0) * 100.0;

    deck1.setPitchPercent((float)pitchPercent1);
    deck2.setPitchPercent((float)pitchPercent2);
}

void AudioEngine::alignBeatPhase(DeckPlayer& masterDeck, DeckPlayer& slaveDeck)
{
    if (!masterDeck.isLoaded() || !slaveDeck.isLoaded()) return;
    if (masterDeck.isScratching() || slaveDeck.isScratching()) return; // no pelear con la mano

    double bpm = masterDeck.getBpm();
    if (bpm <= 0.0) bpm = 120.0;
    double beatInterval = 60.0 / bpm;
    if (beatInterval <= 0.0) beatInterval = 0.5;

    // Fase real por deck con offset del primer beat
    double masterTime = masterDeck.getPosition();
    double masterOffset = masterDeck.getFirstBeatOffset();
    double masterPhase = std::fmod(masterTime - masterOffset, beatInterval);
    if (masterPhase < 0.0) masterPhase += beatInterval;

    double slaveTime = slaveDeck.getPosition();
    double slaveOffset = slaveDeck.getFirstBeatOffset();
    double slavePhase = std::fmod(slaveTime - slaveOffset, beatInterval);
    if (slavePhase < 0.0) slavePhase += beatInterval;

    double error = masterPhase - slavePhase;
    if (error > beatInterval * 0.5) error -= beatInterval;
    if (error < -beatInterval * 0.5) error += beatInterval;

    double errorMs = std::abs(error) * 1000.0;
    double intervalMs = beatInterval * 1000.0;

    // Híbrido: pequeño desfase -> nudge aditivo (pitchBend), no pisa pitchPercent
    if (intervalMs > 0.0 && errorMs <= 15.0)
    {
        // proporcional hasta ~1%
        double maxNudge = 0.01; // ±1%
        double nudge = (error / (beatInterval * 0.5)) * maxNudge;
        nudge = juce::jlimit(-maxNudge, maxNudge, nudge);
        // deadband ~1ms
        if (errorMs < 1.0)
            nudge = 0.0;
        slaveDeck.setPitchBend((float)nudge);
        return;
    }

    // Desfase grande -> re-anclaje al próximo beat
    if (error > 0.0)
    {
        // slave va retrasado respecto a master -> adelantar al próximo beat
        double nextBeat = slaveOffset + std::ceil((slaveTime - slaveOffset) / beatInterval) * beatInterval;
        if (nextBeat < slaveTime)
            nextBeat += beatInterval;
        double delta = nextBeat - slaveTime;
        if (delta < 0.0) delta = 0.0;
        // límite razonable
        if (delta > beatInterval * 0.6)
            delta = beatInterval * 0.25;
        slaveDeck.seekRelative(delta);
    }
    else if (error < 0.0)
    {
        // slave va adelantado -> ir al beat actual o al anterior más cercano
        double prevBeat = slaveOffset + std::floor((slaveTime - slaveOffset) / beatInterval) * beatInterval;
        if (prevBeat > slaveTime - 1e-6)
            prevBeat -= beatInterval;
        if (prevBeat < 0.0)
            prevBeat = slaveOffset >= 0.0 && slaveOffset < beatInterval ? slaveOffset : 0.0;
        double delta = prevBeat - slaveTime;
        slaveDeck.seekRelative(delta);
    }

    // cancelar nudge al hacer re-anclaje
    slaveDeck.setPitchBend(0.0f);
}

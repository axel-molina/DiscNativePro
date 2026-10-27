#include "AudioEngine.h"
#if JUCE_WINDOWS
#include "MediaFoundationAudioFormat_win.h"
#endif
#include <cmath>

namespace
{
// P3-ter: umbrales del controlador de fase
constexpr double phaseDeadbandSec     = 0.001; // 1 ms -> nudge liberado (convergido)
constexpr double phaseReanchorSec     = 0.015; // 15 ms -> por encima, re-anclaje al beat
constexpr double phaseMaxNudge        = 0.01;  // +/-1 % de pitchBend
constexpr double phaseDriftRearmSec   = 0.025; // ~25 ms para auto re-armado estando enclavado
constexpr double phaseAbortNudgeTol   = 1e-5;

// Grid del master: la alineacion significa que el beat del slave caiga donde cae
// el del master. Con tempos iguales (caso del Tempo Blend) ambas medidas coinciden;
// con tempos muy distintos la alineacion es transitoria por diseno y el controlador
// la re-evalua cada bloque.
double deckBeatInterval(const DeckPlayer& deck)
{
    double bpm = deck.getBpm();
    if (bpm <= 0.0) bpm = 120.0;
    const double interval = 60.0 / bpm;
    return interval > 0.0 ? interval : 0.5;
}

double deckPhaseSeconds(const DeckPlayer& deck, double interval)
{
    double phase = std::fmod(deck.getPosition() - deck.getFirstBeatOffset(), interval);
    if (phase < 0.0) phase += interval;
    return phase;
}

// Nudge proporcional al error, saturado a +/-1 % de pitch (aditivo).
float phaseNudgeFor(double errorSec, double interval)
{
    const double half = interval * 0.5;
    if (half <= 0.0) return 0.0f;
    const double nudge = (errorSec / half) * phaseMaxNudge;
    return static_cast<float>(juce::jlimit(-phaseMaxNudge, phaseMaxNudge, nudge));
}
} // namespace

namespace
{
    constexpr const char* tempoBlendProperty = "tempoBlend";
    constexpr const char* loadLockProperty = "loadLock";
    constexpr const char* autoplayOnLoadProperty = "autoplayOnLoad";

    // Merge sobre el settings.json existente: se respetan las demas claves
    // (p. ej. recordingDirectory) y solo se toca la propiedad propia.
    // defaultValue se devuelve cuando el fichero o la clave no existen
    // (tempoBlend/loadLock: false; autoplayOnLoad: true, activado de fabrica).
    bool readBoolSetting(const char* property, bool defaultValue = false)
    {
        auto file = AudioEngine::getSettingsFile();
        if (!file.existsAsFile())
            return defaultValue;

        auto json = juce::JSON::parse(file);
        if (!json.isObject() || !json.hasProperty(property))
            return defaultValue;

        return json[property];
    }

    void writeBoolSetting(const char* property, bool value)
    {
        auto file = AudioEngine::getSettingsFile();
        juce::var json;
        if (file.existsAsFile())
            json = juce::JSON::parse(file);
        if (!json.isObject())
            json = juce::var(new juce::DynamicObject());

        if (auto* obj = json.getDynamicObject())
            obj->setProperty(property, value);

        file.replaceWithText(juce::JSON::toString(json));
    }
}

AudioEngine::AudioEngine()
    : deck1(formatManager, diskReaderThread),
      deck2(formatManager, diskReaderThread)
{
    // Register audio formats (WAV, AIFF, FLAC, OGG, and CoreAudio MP3/AAC/M4A on macOS are all in basic formats)
    formatManager.registerBasicFormats();
#if JUCE_WINDOWS
    formatManager.registerFormat(new MediaFoundationAudioFormat(), false);
#endif

    // Restaurar el estado guardado del Tempo Blend: arranca "armado" y actua en cuanto
    // ambas bandejas tengan pista y BPM (con decks vacios applyTempoBlend sigue siendo no-op).
    tempoBlendActive.store(readBoolSetting(tempoBlendProperty));

    // Load Lock: con la clave ausente queda desactivado (comportamiento historico).
    loadLockEnabled.store(readBoolSetting(loadLockProperty));

    // Autoplay al cargar: activado de fabrica (default ON) y persistido.
    autoplayOnLoadEnabled.store(readBoolSetting(autoplayOnLoadProperty, true));
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

    // 0. Controlador de fase (P3-bis): una sola evaluacion por bloque, sin allocs.
    //    Escribe pitchBend (aditivo) o publica un seek; los decks ya lo consumen abajo.
    processPhaseAlign();

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
    writeBoolSetting(tempoBlendProperty, active);

    if (active)
    {
        phaseUserOverride.store(false); // reactivar el blend devuelve el mando
        applyTempoBlend(currentCrossfaderVal.load());
    }
}

void AudioEngine::setLoadLockEnabled(bool enabled)
{
    loadLockEnabled.store(enabled);
    writeBoolSetting(loadLockProperty, enabled);
}

void AudioEngine::setAutoplayOnLoadEnabled(bool enabled)
{
    autoplayOnLoadEnabled.store(enabled);
    writeBoolSetting(autoplayOnLoadProperty, enabled);
}

double AudioEngine::normalizeBpmTo(double bpm, double referenceBpm)
{
    if (bpm <= 0.0 || referenceBpm <= 0.0)
        return bpm;
    double best = bpm;
    double bestDiff = std::abs(best - referenceBpm);
    const double candidates[] = { bpm, bpm * 2.0, bpm / 2.0, bpm * 4.0, bpm / 4.0 };
    for (double cand : candidates)
    {
        if (cand < 30.0 || cand > 300.0) continue;
        double diff = std::abs(cand - referenceBpm);
        if (diff < bestDiff - 1e-9)
        {
            bestDiff = diff;
            best = cand;
        }
    }
    return best;
}

bool AudioEngine::syncDeckTo(int deckIndex)
{
    auto& self  = getDeck(deckIndex);
    auto& other = getDeck(deckIndex == 0 ? 1 : 0);

    if (!other.isLoaded() || other.getBpm() <= 0.0 || self.getOriginalBpm() <= 0.0)
        return false;

    double otherBpm = normalizeBpmTo(other.getBpm(), self.getOriginalBpm());
    self.setPitchPercent((float)(((otherBpm / self.getOriginalBpm()) - 1.0) * 100.0));
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
        int masterToAlign = -1;
        if (lastCrossfaderZone == CrossfaderZone::Deck1 && currentZone == CrossfaderZone::Center
            && deck1.isPlaying() && deck2.isPlaying())
        {
            masterToAlign = 0;
        }
        else if (lastCrossfaderZone == CrossfaderZone::Deck2 && currentZone == CrossfaderZone::Center
                 && deck1.isPlaying() && deck2.isPlaying())
        {
            masterToAlign = 1;
        }
        lastCrossfaderZone = currentZone;

        // El blend primero: deja asentado el pitchPercent que sirve de referencia al
        // controlador para detectar una intervencion manual del pitch.
        applyTempoBlend(val);

        if (masterToAlign >= 0)
        {
            // P3-ter: el cruce de zona SOLO arma (armPhaseAlign).
            armPhaseAlign(masterToAlign);
        }
    }
}

void AudioEngine::applyTempoBlend(float val)
{
    if (!tempoBlendActive.load()) return;
    if (!deck1.isLoaded() || !deck2.isLoaded()) return;

    double b1 = deck1.getOriginalBpm();
    double b2 = deck2.getOriginalBpm();
    if (b1 <= 0.0 || b2 <= 0.0)
        return;

    // Normalize to avoid half/double-time jumps
    double b1n = normalizeBpmTo(b1, b2);
    double b2n = normalizeBpmTo(b2, b1n);
    b1 = b1n;
    b2 = b2n;

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

// Helpers puros y testeables (P3-bis)
double AudioEngine::wrappedPhaseError(double masterPhaseSec, double slavePhaseSec, double interval)
{
    if (interval <= 0.0) return 0.0;
    double error = masterPhaseSec - slavePhaseSec;
    if (error > interval * 0.5) error -= interval;
    if (error < -interval * 0.5) error += interval;
    return error;
}

double AudioEngine::quantizedSeekDelta(double errorSec, double interval)
{
    if (interval <= 0.0) return 0.0;
    // error < 0 => slave adelantado: hay que saltar al siguiente ciclo del grid del master
    // para caer exactamente en la fase del master. Siempre hacia delante.
    double delta = errorSec + (errorSec < 0.0 ? interval : 0.0);
    if (delta < 0.0) delta = 0.0;
    return delta;
}

void AudioEngine::armPhaseAlign(int masterDeckIndex)
{
    phaseAlignMasterIndexVal = masterDeckIndex == 0 ? 0 : 1;
    phaseAlignState = PhaseAlignState::Settle;
    phaseAlignSettleBlocks = 0;
    phaseAlignSeekCount = 0;
    phaseAlignArmedFlag = true;
    phaseNudgeActiveFlag = false;
    phaseAppliedNudgeVal = 0.0f;
    phaseUserOverride.store(false); // el cruce al centro vuelve a ceder el mando
}

void AudioEngine::processPhaseAlign()
{
    if (!tempoBlendActive.load()) return;
    if (!deck1.isLoaded() || !deck2.isLoaded()) return;

    // P3 sobre P5: el BPM y el offset llegan del worker de analisis. Hasta que
    // ambos decks los tengan, el grid caeria al fallback de 120 BPM y el
    // controlador podria seekear/nudgear sobre un compas equivocado.
    if (deck1.getBpm() <= 0.0 || deck2.getBpm() <= 0.0) return;

    if (phaseAlignState == PhaseAlignState::Idle && phaseAlignArmedFlag)
    {
        phaseAlignState = PhaseAlignState::Settle;
        phaseAlignSettleBlocks = 0;
        phaseAlignSeekCount = 0;
    }

    const int masterIdx = phaseAlignMasterIndexVal;
    const int slaveIdx  = masterIdx == 0 ? 1 : 0;
    auto& masterDeck = getDeck(masterIdx);
    auto& slaveDeck  = getDeck(slaveIdx);

    // Intervencion manual del transporte: si el usuario mueve una bandeja (onda,
    // MIDI, jog), el controlador se desarma. Antes el rearme por deriva (>25 ms)
    // volvia a alinear y seekeaba la OTRA bandeja al mismo instante, asi que un
    // arrastre en la mitad izquierda movia las dos.
    const juce::uint32 s1 = deck1.getUserSeekSerial();
    const juce::uint32 s2 = deck2.getUserSeekSerial();
    if (s1 != lastUserSeek1 || s2 != lastUserSeek2)
    {
        lastUserSeek1 = s1;
        lastUserSeek2 = s2;
        if (phaseNudgeActiveFlag || std::abs(slaveDeck.getPitchBend()) > 1e-6f)
        {
            slaveDeck.setPitchBend(0.0f);
            phaseAppliedNudgeVal = 0.0f;
            phaseNudgeActiveFlag = false;
        }
        phaseAlignState = PhaseAlignState::Idle;
        phaseAlignArmedFlag = false;
        phaseAlignSeekCount = 0;
        phaseUserOverride.store(true);
        return;
    }

    if (!masterDeck.isPlaying() || !slaveDeck.isPlaying()) return;
    if (masterDeck.isScratching() || slaveDeck.isScratching()) return;

    // Abortar por intervencion del usuario: comparar getPitchBend() con el ultimo nudge escrito por el controlador
    if (phaseNudgeActiveFlag)
    {
        const float pb = slaveDeck.getPitchBend();
        if (std::abs(pb - phaseAppliedNudgeVal) > phaseAbortNudgeTol)
        {
            slaveDeck.setPitchBend(0.0f);
            phaseAppliedNudgeVal = 0.0f;
            phaseNudgeActiveFlag = false;
            phaseAlignState = PhaseAlignState::Idle;
            phaseAlignArmedFlag = false;
            phaseAlignSeekCount = 0;
            return;
        }
    }

    const double beatInterval = deckBeatInterval(masterDeck);
    const double masterPhase  = deckPhaseSeconds(masterDeck, beatInterval);
    const double slavePhase   = deckPhaseSeconds(slaveDeck,  beatInterval);
    const double error        = wrappedPhaseError(masterPhase, slavePhase, beatInterval);
    const double errorAbs     = std::abs(error);

    // Auto re-armado si la deriva supera ~25 ms estando enclavado (Idle).
    // Desactivado mientras el usuario mande (seek manual): su intervencion no
    // se revierte sola.
    if (phaseAlignState == PhaseAlignState::Idle && phaseAlignArmedFlag == false
        && !phaseUserOverride.load())
    {
        if (errorAbs > phaseDriftRearmSec)
        {
            phaseAlignArmedFlag = true;
            phaseAlignState = PhaseAlignState::Settle;
            phaseAlignSettleBlocks = 0;
            phaseAlignSeekCount = 0;
        }
        else
        {
            return;
        }
    }

    if (phaseAlignState == PhaseAlignState::Settle)
    {
        if (phaseNudgeActiveFlag)
        {
            slaveDeck.setPitchBend(0.0f);
            phaseAppliedNudgeVal = 0.0f;
            phaseNudgeActiveFlag = false;
        }
        ++phaseAlignSettleBlocks;
        if (phaseAlignSettleBlocks >= phaseAlignSettleBlocksCount)
            phaseAlignState = PhaseAlignState::Measure;
        return;
    }

    if (phaseAlignState == PhaseAlignState::Measure)
    {
        // Convergido
        if (errorAbs < phaseDeadbandSec)
        {
            if (phaseNudgeActiveFlag || std::abs(slaveDeck.getPitchBend()) > 1e-6f)
            {
                slaveDeck.setPitchBend(0.0f);
                phaseAppliedNudgeVal = 0.0f;
                phaseNudgeActiveFlag = false;
            }
            phaseAlignState = PhaseAlignState::Idle;
            phaseAlignArmedFlag = false;
            phaseAlignSeekCount = 0;
            return;
        }

        // Desfase grande (>15 ms): UN seek por iteracion si |error|>15 ms
        if (errorAbs > phaseReanchorSec)
        {
            if (phaseAlignSeekCount >= phaseAlignMaxSeeks)
            {
                if (phaseNudgeActiveFlag)
                {
                    slaveDeck.setPitchBend(0.0f);
                    phaseAppliedNudgeVal = 0.0f;
                    phaseNudgeActiveFlag = false;
                }
                phaseAlignState = PhaseAlignState::Idle;
                phaseAlignArmedFlag = false;
                phaseAlignSeekCount = 0;
                return;
            }
            if (phaseNudgeActiveFlag)
            {
                slaveDeck.setPitchBend(0.0f);
                phaseAppliedNudgeVal = 0.0f;
                phaseNudgeActiveFlag = false;
            }
            const double delta = quantizedSeekDelta(error, beatInterval);
            if (delta > 0.0)
                slaveDeck.seekRelative(delta, false); // seek del controlador, no del usuario
            ++phaseAlignSeekCount;
            phaseAlignState = PhaseAlignState::Settle;
            phaseAlignSettleBlocks = 0;
            return;
        }

        // Zona intermedia (1 ms < |error| <= 15 ms): nudge aditivo <=1 % si |error|<=15 ms
        if (errorAbs <= phaseReanchorSec)
        {
            float nudge = phaseNudgeFor(error, beatInterval);
            nudge = static_cast<float>(juce::jlimit(-phaseMaxNudge, phaseMaxNudge, (double)nudge));
            if (std::abs((double)nudge - (double)phaseAppliedNudgeVal) > 1e-7)
            {
                slaveDeck.setPitchBend(nudge);
                phaseAppliedNudgeVal = nudge;
            }
            phaseNudgeActiveFlag = std::abs((double)nudge) > 1e-7;
            // seguir midiendo - stay in Measure
            return;
        }
    }
}

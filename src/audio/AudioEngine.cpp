#include "AudioEngine.h"

AudioEngine::AudioEngine()
    : deck1(formatManager, diskReaderThread),
      deck2(formatManager, diskReaderThread)
{
    // Register audio formats (WAV, AIFF, FLAC, OGG, and CoreAudio MP3/AAC/M4A on macOS are all in basic formats)
    formatManager.registerBasicFormats();
}

AudioEngine::~AudioEngine()
{
    shutdown();
}

bool AudioEngine::initialise()
{
    // Start background disk streaming thread (priority normal for smooth file prefetching)
    diskReaderThread.startThread(juce::Thread::Priority::normal);

    // Initialise audio device manager with 0 inputs, 2 stereo outputs (CoreAudio on macOS)
    auto err = deviceManager.initialiseWithDefaultDevices(0, 2);
    if (!err.isEmpty())
    {
        DBG("Audio device error: " << err);
        return false;
    }

    deviceManager.addAudioCallback(this);
    return true;
}

void AudioEngine::shutdown()
{
    audioRecorder.stopRecording();
    deviceManager.removeAudioCallback(this);
    deviceManager.closeAudioDevice();
    diskReaderThread.stopThread(2000);
}

bool AudioEngine::toggleRecording(juce::String& outFilePath)
{
    if (audioRecorder.isRecording())
    {
        outFilePath = audioRecorder.getCurrentFile().getFullPathName();
        audioRecorder.stopRecording();
        return false; // Stopped
    }

    auto recDir = juce::File::getSpecialLocation(juce::File::userMusicDirectory).getChildFile("DiscPro_Recordings");
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

    // Preallocate buffers for zero runtime heap allocation (real-time safe)
    deck1Buffer.setSize(2, currentBlockSize, false, true, true);
    deck2Buffer.setSize(2, currentBlockSize, false, true, true);
    masterBuffer.setSize(2, currentBlockSize, false, true, true);

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

    // 1. Fetch audio from streaming deck sources
    juce::AudioSourceChannelInfo d1Info(&deck1Buffer, 0, numSamples);
    deck1.getNextAudioBlock(d1Info);

    juce::AudioSourceChannelInfo d2Info(&deck2Buffer, 0, numSamples);
    deck2.getNextAudioBlock(d2Info);

    // 2. Process DSP: Channel EQ, Color Filters, Channel Faders, Crossfader, Master Gain & Meters
    djMixer.process(deck1Buffer, deck2Buffer, masterBuffer, numSamples);

    // 3. Push to master recorder (lock-free, zero allocation)
    audioRecorder.pushAudioBlock(masterBuffer, numSamples);

    // 4. Write to physical audio output (DAC / CoreAudio)
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

void AudioEngine::setTempoBlend(bool active)
{
    tempoBlendActive.store(active);
    if (active)
    {
        applyTempoBlend(currentCrossfaderVal.load());
    }
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

    double bpm = masterDeck.getBpm();
    if (bpm <= 0.0) bpm = 120.0;
    double beatInterval = 60.0 / bpm;

    double masterTime = masterDeck.getPosition();
    double masterPhase = std::fmod(masterTime, beatInterval);
    if (masterPhase < 0.0) masterPhase += beatInterval;

    double slaveTime = slaveDeck.getPosition();
    double slavePhase = std::fmod(slaveTime, beatInterval);
    if (slavePhase < 0.0) slavePhase += beatInterval;

    double correction = masterPhase - slavePhase;
    if (correction > beatInterval * 0.5) correction -= beatInterval;
    if (correction < -beatInterval * 0.5) correction += beatInterval;

    slaveDeck.seekRelative(correction);
}

#include "MainComponent.h"
#include "MidiModalComponent.h"
#include "SettingsModalComponent.h"
#include "CoverArtGenerator.h"
#include <juce_audio_utils/juce_audio_utils.h>

MainComponent::MainComponent()
    : midiManager(audioEngine),
      fxPanel(audioEngine.getMixer()),
      deck1(0, audioEngine.getDeck(0), juce::Colour::fromRGB(0, 180, 216)),
      deck2(1, audioEngine.getDeck(1), juce::Colour::fromRGB(148, 163, 184)),
      mixer(audioEngine),
      library(audioEngine.getFormatManager())
{
    // Start CoreAudio low-latency engine
    audioEngine.initialise();

    // Start USB MIDI Manager
    midiManager.initialise();

    // Recording action (WAV export in ~/Music/DiscPro_Recordings/)
    topBar.onRecClicked = [this]() {
        juce::String outPath;
        bool isRec = audioEngine.toggleRecording(outPath);
        if (isRec)
        {
            topBar.setRecordingState(true, 0.0);
        }
        else
        {
            topBar.setRecordingState(false, 0.0);
            if (outPath.isNotEmpty())
            {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::InfoIcon,
                    "Grabacion Guardada",
                    "Sesion de mezcla guardada con exito en:\n\n" + outPath);
            }
        }
    };

    // FX Panel toggle
    topBar.onFxClicked = [this]() {
        isFxPanelVisible = !isFxPanelVisible;
        fxPanel.setVisible(isFxPanelVisible);
        resized();
    };

    // Fullscreen toggle
    topBar.onFullscreenClicked = [this]() {
        if (auto* top = getTopLevelComponent())
        {
            if (auto* win = dynamic_cast<juce::DocumentWindow*>(top))
            {
                win->setFullScreen(!win->isFullScreen());
            }
        }
    };

    // Preferences modal
    topBar.onSettingsClicked = [this]() {
        if (settingsWindow != nullptr)
        {
            settingsWindow->toFront(true);
            return;
        }
        auto* modal = new SettingsModalComponent(audioEngine, midiManager);
        juce::DialogWindow::LaunchOptions opt;
        opt.dialogTitle = "Preferencias de DiscNativePro";
        opt.dialogBackgroundColour = juce::Colour::fromRGB(26, 29, 38);
        opt.content.setOwned(modal);
        opt.resizable = false;
        opt.useNativeTitleBar = true;
        opt.escapeKeyTriggersCloseButton = true;
        settingsWindow = opt.launchAsync();

        modal->onClose = [this]() {
            if (settingsWindow != nullptr)
            {
                settingsWindow->closeButtonPressed();
            }
        };
    };

    addChildComponent(fxPanel);
    addAndMakeVisible(topBar);
    addAndMakeVisible(deck1);
    addAndMakeVisible(mixer);
    addAndMakeVisible(deck2);

    // Connect Library load to Decks
    library.onLoadTrack = [this](int deckIndex, const juce::File& file) {
        if (deckIndex == 0)
            deck1.loadAudioFile(file);
        else
            deck2.loadAudioFile(file);
    };

    library.onLoadYouTubeTrack = [this](int deckIndex, const YouTubeSearchResult& video) {
        if (deckIndex == 0)
            deck1.loadYouTubeVideo(video);
        else
            deck2.loadYouTubeVideo(video);
    };

    library.onStartAutomix = [this](const std::vector<TrackItem>& tracks) {
        startAutomix(tracks);
    };
    library.onStopAutomix = [this]() {
        stopAutomix();
    };

    addAndMakeVisible(library);

    // Preload demo tracks into Deck 1 and Deck 2 to match DiscPro initial state
    auto samplesDir = juce::File::getSpecialLocation(juce::File::userMusicDirectory).getChildFile("DiscPro_Samples");
    auto d1File = samplesDir.getChildFile("DiscPro_Latin_Tech_Groove.wav");
    if (d1File.existsAsFile())
        deck1.loadAudioFile(d1File);

    auto d2File = samplesDir.getChildFile("Sunset_Beach_House_Mix.wav");
    if (d2File.existsAsFile())
        deck2.loadAudioFile(d2File);

    setWantsKeyboardFocus(true);
    startTimerHz(30); // 30Hz for smooth crossfader and video mixing
    setSize(1360, 860);
}

MainComponent::~MainComponent()
{
    stopTimer();
    audioEngine.shutdown();
    CoverArtGenerator::clearCache();
}

void MainComponent::timerCallback()
{
    if (audioEngine.isRecording())
    {
        topBar.setRecordingState(true, audioEngine.getRecordingDuration());
    }

    // Video volume mixing: sync channel faders and crossfader to YouTube video players
    if (deck1.isVideoModeActive())
    {
        float ch1Fader = audioEngine.getMixer().getChannel(0).volumeFader.load();
        float xfader = audioEngine.getMixer().getCrossfader();
        float xfaderGain1 = (xfader <= 0.0f) ? 1.0f : (1.0f - xfader);
        float masterVol = audioEngine.getMixer().getMasterVolume();
        deck1.setVideoVolume(juce::jlimit(0.0f, 1.0f, ch1Fader * xfaderGain1 * masterVol));
    }
    if (deck2.isVideoModeActive())
    {
        float ch2Fader = audioEngine.getMixer().getChannel(1).volumeFader.load();
        float xfader = audioEngine.getMixer().getCrossfader();
        float xfaderGain2 = (xfader >= 0.0f) ? 1.0f : (1.0f + xfader);
        float masterVol = audioEngine.getMixer().getMasterVolume();
        deck2.setVideoVolume(juce::jlimit(0.0f, 1.0f, ch2Fader * xfaderGain2 * masterVol));
    }

    if (!automixActive)
        return;

    if (automixTransitioning)
    {
        // Animate crossfader gradually — each call to setCrossfader triggers
        // applyTempoBlend (BPM sync) and alignBeatPhase (kick alignment) automatically
        float current = audioEngine.getMixer().getCrossfader();
        current += automixCrossfaderStep;

        bool reachedTarget = (automixCrossfaderStep > 0.0f)
                                 ? (current >= automixCrossfaderTarget)
                                 : (current <= automixCrossfaderTarget);

        if (reachedTarget)
        {
            current = automixCrossfaderTarget;
            automixTransitioning = false;

            // Transition complete — the previous deck is now silent
            // Preload next track into the now-free deck
            int freeDeck = (automixActiveDeck == 0) ? 1 : 0;
            int nextIndex = automixCurrentIndex + 1;
            if (nextIndex < (int)automixPlaylist.size())
            {
                if (freeDeck == 0)
                    deck1.loadAudioFile(automixPlaylist[static_cast<size_t>(nextIndex)].file);
                else
                    deck2.loadAudioFile(automixPlaylist[static_cast<size_t>(nextIndex)].file);
            }
        }

        audioEngine.setCrossfader(current);
    }
    else
    {
        // Monitor active deck position — trigger advance when ≤15 seconds remain
        auto& activeDeckPlayer = audioEngine.getDeck(automixActiveDeck);
        if (activeDeckPlayer.isLoaded() && activeDeckPlayer.isPlaying())
        {
            double remaining = activeDeckPlayer.getLengthInSeconds() - activeDeckPlayer.getPosition();
            if (remaining <= 15.0 && remaining > 0.0)
            {
                automixAdvance();
            }
        }
        else if (activeDeckPlayer.isLoaded() && !activeDeckPlayer.isPlaying()
                 && activeDeckPlayer.getPosition() >= activeDeckPlayer.getLengthInSeconds() - 0.1)
        {
            // Track ended naturally without triggering advance (very short tracks)
            automixAdvance();
        }
    }
}

bool MainComponent::keyPressed(const juce::KeyPress& key)
{
    auto text = key.getTextDescription().toLowerCase();
    char c = (text.length() == 1) ? static_cast<char>(text[0]) : '\0';

    // Deck 1 Shortcuts
    if (c == 'q')
    {
        if (deck1.isVideoModeActive())
        {
            if (deck1.isVideoPlayingActive())
                deck1.pauseVideo();
            else
                deck1.playVideo();
        }
        else
        {
            auto& d1 = audioEngine.getDeck(0);
            if (d1.isPlaying()) d1.pause(); else d1.play();
        }
        return true;
    }
    if (c == 'w')
    {
        if (deck1.isVideoModeActive())
            deck1.pauseVideo();
        audioEngine.getDeck(0).triggerCue();
        return true;
    }

    // Deck 2 Shortcuts
    if (c == 'p')
    {
        if (deck2.isVideoModeActive())
        {
            if (deck2.isVideoPlayingActive())
                deck2.pauseVideo();
            else
                deck2.playVideo();
        }
        else
        {
            auto& d2 = audioEngine.getDeck(1);
            if (d2.isPlaying()) d2.pause(); else d2.play();
        }
        return true;
    }
    if (c == 'o')
    {
        if (deck2.isVideoModeActive())
            deck2.pauseVideo();
        audioEngine.getDeck(1).triggerCue();
        return true;
    }

    // Crossfader Shortcuts
    if (c == 'x') { audioEngine.setCrossfader(-1.0f); return true; }
    if (c == 'c') { audioEngine.setCrossfader(0.0f); return true; }
    if (c == 'v') { audioEngine.setCrossfader(1.0f); return true; }

    return false;
}

void MainComponent::paint(juce::Graphics& g)
{
    // DiscPro #0c0d10 dark theme
    g.fillAll(juce::Colour::fromRGB(12, 13, 16));
}

void MainComponent::resized()
{
    auto area = getLocalBounds();

    // 1. Top Bar header (40px)
    topBar.setBounds(area.removeFromTop(40));

    // 2. Optional FX Panel (48px)
    if (isFxPanelVisible)
    {
        fxPanel.setBounds(area.removeFromTop(48).reduced(6, 2));
    }

    // 3. Lower half: Music Library Browser (~38% of remaining window)
    int totalRemainingHeight = area.getHeight();
    int libHeight = juce::jlimit(220, 500, (int)(totalRemainingHeight * 0.38));
    library.setBounds(area.removeFromBottom(libHeight));

    // 4. Center workspace: Deck 1 (left), Center Mixer, Deck 2 (right)
    area.reduce(6, 4);
    int mixerWidth = 430; // 96px Ch1 + 238px Center + 96px Ch2
    int deckWidth = (area.getWidth() - mixerWidth - 12) / 2;

    deck1.setBounds(area.removeFromLeft(deckWidth));
    area.removeFromLeft(6);

    mixer.setBounds(area.removeFromLeft(mixerWidth));
    area.removeFromLeft(6);

    deck2.setBounds(area);
}

void MainComponent::startAutomix(const std::vector<TrackItem>& tracks)
{
    if (tracks.empty())
        return;

    automixPlaylist = tracks;
    automixCurrentIndex = 0;
    automixActiveDeck = 0;
    automixTransitioning = false;
    automixActive = true;

    // Activate Tempo Blend for automatic BPM synchronization
    audioEngine.setTempoBlend(true);

    // Load first track into Deck 1 and start playing
    deck1.loadAudioFile(automixPlaylist[0].file);
    audioEngine.getDeck(0).play();

    // Crossfader fully on Deck 1
    audioEngine.setCrossfader(-1.0f);

    // Preload second track into Deck 2 (if available)
    if (automixPlaylist.size() >= 2)
    {
        deck2.loadAudioFile(automixPlaylist[1].file);
    }

    library.setAutomixRunning(true);
}

void MainComponent::stopAutomix()
{
    automixActive = false;
    automixTransitioning = false;
    automixCurrentIndex = -1;
    automixPlaylist.clear();

    library.setAutomixRunning(false);
    library.clearAutomixQueue();
}

void MainComponent::automixAdvance()
{
    if (!automixActive)
        return;

    // Check if there's a next track to transition to
    int nextTrackIndex = automixCurrentIndex + 1;
    if (nextTrackIndex >= (int)automixPlaylist.size())
    {
        // No more tracks — let the last one finish naturally, then stop
        automixActive = false;
        automixTransitioning = false;

        // Wait for current track to end, then clean up
        juce::Timer::callAfterDelay(1000, [this]() {
            library.setAutomixRunning(false);
            library.clearAutomixQueue();
        });
        return;
    }

    // Determine which deck to transition TO
    int nextDeck = (automixActiveDeck == 0) ? 1 : 0;

    // The next track should already be preloaded in nextDeck — start playing it
    audioEngine.getDeck(nextDeck).play();

    // Configure crossfader animation:
    // ~8 seconds at 10Hz = 80 steps, crossfader range is 2.0 (-1 to +1) so step ≈ 0.025
    if (automixActiveDeck == 0)
    {
        // Transitioning from Deck 1 (-1.0) to Deck 2 (+1.0)
        automixCrossfaderTarget = 1.0f;
        automixCrossfaderStep = 0.025f;
    }
    else
    {
        // Transitioning from Deck 2 (+1.0) to Deck 1 (-1.0)
        automixCrossfaderTarget = -1.0f;
        automixCrossfaderStep = -0.025f;
    }

    automixTransitioning = true;
    automixActiveDeck = nextDeck;
    automixCurrentIndex = nextTrackIndex;
}

#include "MainComponent.h"
#include "MidiModalComponent.h"
#include "SettingsModalComponent.h"
#include "CoverArtGenerator.h"
#include "../services/LanguageManager.h"
#include "../services/AIService.h"
#include "../services/YouTubeService.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <algorithm>
#include <cmath>

MainComponent::MainComponent()
    : midiManager(audioEngine),
      fxPanel(audioEngine.getMixer()),
      samplerPanel(audioEngine.getSampler(), audioEngine.getFormatManager(),
                   [this]() { return audioEngine.getSampleRate(); }),
      deck1(0, audioEngine.getDeck(0), audioEngine, juce::Colour::fromRGB(0, 180, 216)),
      deck2(1, audioEngine.getDeck(1), audioEngine, juce::Colour::fromRGB(148, 163, 184)),
      mixer(audioEngine),
      library(audioEngine.getFormatManager())
{
    // Inicialización pesada diferida a un tick del message loop: la ventana ya
    // está pintada cuando arranca el audio, así un dispositivo que tarde o se
    // cuelgue no deja la app sin ventana visible.
    juce::Component::SafePointer<MainComponent> safeThis(this);
    juce::Timer::callAfterDelay(0, [safeThis]() {
        if (safeThis == nullptr)
            return;

        // Start CoreAudio low-latency engine
        safeThis->audioEngine.initialise();

        // Start USB MIDI Manager
        safeThis->midiManager.initialise();

        // Migrar credenciales legacy (settings.json en claro) al almacén seguro del sistema
        AIService::migrateLegacySecrets();
        YouTubeService::migrateLegacySecret();
    });

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
                    LanguageManager::trUtf8("Grabación Guardada", "Recording Saved"),
                    LanguageManager::trUtf8("Sesión de mezcla guardada con éxito en:\n\n", "Mix session successfully saved to:\n\n") + outPath
                    + LanguageManager::trUtf8("\n\nPuedes abrir la carpeta desde Preferencias > General.", "\n\nYou can open the folder from Preferences > General."));
            }
        }
    };

    // FX Panel toggle
    topBar.onFxClicked = [this]() {
        isFxPanelVisible = !isFxPanelVisible;
        topBar.setFxActive(isFxPanelVisible);
        fxPanel.setVisible(isFxPanelVisible);
        resized();
    };

    // Sampler Panel toggle (8 pads one-shot)
    topBar.onSmpClicked = [this]() {
        isSamplerPanelVisible = !isSamplerPanelVisible;
        topBar.setSmpActive(isSamplerPanelVisible);
        samplerPanel.setVisible(isSamplerPanelVisible);
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
        openSettings(0);
    };

    // Update badge: abre directamente la pestaña de Versión en Preferencias / Ajustes
    topBar.onUpdateClicked = [this]() {
        openSettings(9);
    };

    addChildComponent(fxPanel);
    fxPanel.setVisible(isFxPanelVisible);
    topBar.setFxActive(isFxPanelVisible);
    addChildComponent(samplerPanel);
    samplerPanel.setVisible(isSamplerPanelVisible);
    topBar.setSmpActive(isSamplerPanelVisible);
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

    // Synchronize detected Deck BPM/Key back to Library and Automix queue
    deck1.onFileLoaded = [this](const juce::File& file, double detectedBpm) {
        library.updateTrackBpm(file, detectedBpm);
        library.updateTrackKey(file, deck1.getTrackKey());
    };
    deck2.onFileLoaded = [this](const juce::File& file, double detectedBpm) {
        library.updateTrackBpm(file, detectedBpm);
        library.updateTrackKey(file, deck2.getTrackKey());
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
    library.onSmartAutomix = [this]() {
        startSmartAutomix();
    };
    library.onAutomixQueueChanged = [this](const std::vector<TrackItem>& newQueue) {
        if (!automixActive)
            return;
        automixPlaylist = newQueue;
        if (automixPlaylist.empty())
        {
            stopAutomix();
            return;
        }
        int freeDeck = (automixActiveDeck == 0) ? 1 : 0;
        int nextIndex = automixCurrentIndex + 1;
        if (nextIndex >= 0 && nextIndex < (int)automixPlaylist.size())
        {
            if (freeDeck == 0)
                deck1.loadAudioFile(automixPlaylist[static_cast<size_t>(nextIndex)].file);
            else
                deck2.loadAudioFile(automixPlaylist[static_cast<size_t>(nextIndex)].file);
        }
    };
    library.onOpenSettings = [this](int targetTab) {
        openSettings(targetTab);
    };

    // Connect MIDI hardware browser and deck loader
    midiManager.onBrowseRotate = [this](int delta) {
        library.navigateBrowser(delta);
    };
    midiManager.onBrowseClick = [this]() {
        library.handleBrowseClick();
    };
    midiManager.onLoadTrack = [this](int deckIndex) {
        library.loadSelectedTrack(deckIndex);
    };
    midiManager.onPlayPause = [this](int deckIndex) {
        if (deckIndex == 0)
        {
            deck1.togglePlayPause();
            midiManager.sendMidiMessage(juce::MidiMessage::noteOn(1, 11, (juce::uint8)(deck1.isPlaying() ? 127 : 0)));
        }
        else
        {
            deck2.togglePlayPause();
            midiManager.sendMidiMessage(juce::MidiMessage::noteOn(2, 11, (juce::uint8)(deck2.isPlaying() ? 127 : 0)));
        }
    };
    midiManager.onCue = [this](int deckIndex) {
        if (deckIndex == 0)
        {
            deck1.triggerCue();
            midiManager.sendMidiMessage(juce::MidiMessage::noteOn(1, 11, (juce::uint8)0));
        }
        else
        {
            deck2.triggerCue();
            midiManager.sendMidiMessage(juce::MidiMessage::noteOn(2, 11, (juce::uint8)0));
        }
    };

    addAndMakeVisible(library);

    setWantsKeyboardFocus(true);
    startTimerHz(30); // 30Hz for smooth crossfader and video mixing
    setSize(1360, 860);

    scheduleUpdateCheck();
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

    // Periodically update active track context for AI Match (~0.5s interval)
    static int aiContextTick = 0;
    if (++aiContextTick >= 15)
    {
        aiContextTick = 0;
        CurrentTrackContext ctx;
        float xfader = audioEngine.getMixer().getCrossfader();
        bool d1Playing = deck1.isPlaying() || deck1.isVideoPlayingActive();
        bool d2Playing = deck2.isPlaying() || deck2.isVideoPlayingActive();

        int activeDeckIdx = 0;
        if (d2Playing && !d1Playing)
            activeDeckIdx = 1;
        else if (d1Playing && !d2Playing)
            activeDeckIdx = 0;
        else
            activeDeckIdx = (xfader > 0.0f) ? 1 : 0;

        auto& activeDeckComp = (activeDeckIdx == 0) ? deck1 : deck2;
        if (activeDeckComp.isLoaded())
        {
            ctx.activeDeck = activeDeckIdx;
            ctx.title = activeDeckComp.getTrackTitle();
            ctx.artist = activeDeckComp.getTrackArtist();
            ctx.bpm = activeDeckComp.getBpm();
            ctx.key = activeDeckComp.getTrackKey();
            ctx.isVideo = activeDeckComp.isVideoModeActive();
            ctx.videoId = activeDeckComp.getVideoId();
            library.updateActiveTrackContext(ctx);
        }
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
    // Recording toggle shortcut: Cmd+R (macOS) / Ctrl+R (Windows/Linux)
    if (key.getModifiers().isCommandDown() && (key.getKeyCode() == 'r' || key.getKeyCode() == 'R'))
    {
        if (topBar.onRecClicked)
            topBar.onRecClicked();
        return true;
    }

    auto text = key.getTextDescription().toLowerCase();
    char c = (text.length() == 1) ? static_cast<char>(text[0]) : '\0';

    // Deck 1 Shortcuts
    if (c == 'q')
    {
        deck1.togglePlayPause();
        return true;
    }
    if (c == 'w')
    {
        deck1.triggerCue();
        return true;
    }

    // Deck 2 Shortcuts
    if (c == 'p')
    {
        deck2.togglePlayPause();
        return true;
    }
    if (c == 'o')
    {
        deck2.triggerCue();
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

    float totalRemainingHeight = static_cast<float>(area.getHeight());
    int libHeight = juce::jlimit(190, 480, static_cast<int>(totalRemainingHeight * (isFxPanelVisible ? 0.35f : 0.41f)));
    library.setBounds(area.removeFromBottom(libHeight));

    // 3. Optional FX Strip (54px) directly below Decks/Mixer and above Library (djay Pro style)
    if (isFxPanelVisible)
    {
        fxPanel.setBounds(area.removeFromBottom(54).reduced(6, 2));
    }

    // 3b. Optional Sampler Strip (76px) above Library
    if (isSamplerPanelVisible)
    {
        samplerPanel.setBounds(area.removeFromBottom(76).reduced(6, 2));
    }

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
    automixTransitioning = false;
    automixActive = true;

    // Activate Tempo Blend for automatic BPM synchronization
    audioEngine.setTempoBlend(true);

    bool d1Playing = deck1.isPlaying() || deck1.isVideoPlayingActive() || audioEngine.getDeck(0).isPlaying();
    bool d2Playing = deck2.isPlaying() || deck2.isVideoPlayingActive() || audioEngine.getDeck(1).isPlaying();

    if (d1Playing || d2Playing)
    {
        // A deck is already playing music: do not interrupt or wipe it!
        int playingDeck = 0;
        if (d2Playing && !d1Playing)
            playingDeck = 1;
        else if (d1Playing && !d2Playing)
            playingDeck = 0;
        else
            playingDeck = (audioEngine.getMixer().getCrossfader() > 0.0f) ? 1 : 0;

        int otherDeck = (playingDeck == 0) ? 1 : 0;

        // Ensure crossfader is fully open on the deck currently playing
        audioEngine.setCrossfader((playingDeck == 0) ? -1.0f : 1.0f);

        // Preload first automix track into the other (idle) deck in paused/cue state
        if (otherDeck == 0)
            deck1.loadAudioFile(automixPlaylist[0].file);
        else
            deck2.loadAudioFile(automixPlaylist[0].file);

        automixActiveDeck = playingDeck;
        automixCurrentIndex = -1; // Indicates current playing track is user's original song; next transition will play automixPlaylist[0]
        library.setAutomixPlayingIndex(-1);
    }
    else
    {
        // Cold start from silence: load track 0 in Deck 1 and start immediately
        automixActiveDeck = 0;
        automixCurrentIndex = 0;
        deck1.loadAudioFile(automixPlaylist[0].file);
        audioEngine.getDeck(0).play();
        audioEngine.setCrossfader(-1.0f);

        if (automixPlaylist.size() >= 2)
        {
            deck2.loadAudioFile(automixPlaylist[1].file);
        }

        library.setAutomixPlayingIndex(0);
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
    library.setAutomixPlayingIndex(-1);
    library.clearAutomixQueue();
}

namespace
{
// Camelot Wheel harmonic compatibility:
// Same key, relative major/minor (e.g. 8A <-> 8B), or +/- 1 modulo 12 (e.g. 8A <-> 9A, 12A <-> 1A)
bool areCamelotKeysCompatible(const juce::String& k1, const juce::String& k2)
{
    if (k1.isEmpty() || k2.isEmpty() || k1 == "--" || k2 == "--" || k1 == "YT" || k2 == "YT")
        return false;

    auto s1 = k1.trim().toUpperCase();
    auto s2 = k2.trim().toUpperCase();
    if (s1 == s2)
        return true;

    int num1 = s1.retainCharacters("0123456789").getIntValue();
    int num2 = s2.retainCharacters("0123456789").getIntValue();
    juce::String let1 = s1.retainCharacters("AB");
    juce::String let2 = s2.retainCharacters("AB");

    if (num1 < 1 || num1 > 12 || num2 < 1 || num2 > 12 || let1.isEmpty() || let2.isEmpty())
        return false;

    // Same number, relative major/minor (e.g. 8A <-> 8B)
    if (num1 == num2 && let1 != let2)
        return true;

    // Same letter, +/- 1 modulo 12 (e.g. 12 <-> 1, 8 <-> 9)
    if (let1 == let2)
    {
        int diff = std::abs(num1 - num2);
        if (diff == 1 || diff == 11)
            return true;
    }

    return false;
}

// Scorer for local candidate tracks: prioritizes artist affinity, same folder/genre, BPM proximity, and harmonic key
int scoreTrackCandidate(const TrackItem& candidate,
                       const juce::String& currentTitle,
                       const juce::String& currentArtist,
                       double currentBpm,
                       const juce::String& currentKey,
                       const juce::String& currentFolder)
{
    juce::ignoreUnused(currentTitle);
    int score = 0;
    auto candArtist = candidate.artist.trim().toLowerCase();
    auto curArtist = currentArtist.trim().toLowerCase();

    // 1. Artist affinity
    if (curArtist.isNotEmpty() && candArtist.isNotEmpty())
    {
        if (candArtist == curArtist)
            score += 60;
        else if (candArtist.contains(curArtist) || curArtist.contains(candArtist))
            score += 45;
    }

    // 2. Folder / Genre affinity
    if (currentFolder.isNotEmpty() && candidate.folder.isNotEmpty() && candidate.folder != "Todas las Pistas")
    {
        if (candidate.folder.equalsIgnoreCase(currentFolder))
            score += 35;
    }

    // 3. BPM proximity with Half-Time / Double-Time modular compatibility
    if (currentBpm > 20.0 && candidate.bpm > 20.0)
    {
        double diffDirect = std::abs(candidate.bpm - currentBpm) / currentBpm;
        double diffDouble = std::abs(candidate.bpm - (currentBpm * 2.0)) / (currentBpm * 2.0);
        double diffHalf   = std::abs((candidate.bpm * 2.0) - currentBpm) / currentBpm;
        double minDiff    = std::min({ diffDirect, diffDouble, diffHalf });

        if (minDiff <= 0.04)
            score += 25;
        else if (minDiff <= 0.08)
            score += 18;
        else if (minDiff <= 0.15)
            score += 8;
    }

    // 4. Harmonic Key (Camelot)
    if (areCamelotKeysCompatible(currentKey, candidate.key))
    {
        if (candidate.key.equalsIgnoreCase(currentKey))
            score += 20;
        else
            score += 15;
    }

    return score;
}

// Flexible matching between AI recommendation and candidate track item
bool trackMatchesRecommendation(const TrackItem& track,
                                const juce::String& recTitle,
                                const juce::String& recArtist)
{
    auto rTitle = recTitle.toLowerCase().trim();
    auto rArtist = recArtist.toLowerCase().trim();
    if (rTitle.isEmpty())
        return false;

    auto tTitle = track.title.toLowerCase().trim();
    auto tArtist = track.artist.toLowerCase().trim();
    auto fileName = track.file.getFileNameWithoutExtension().toLowerCase();

    // 1. Direct title contains or filename contains
    if (tTitle.isNotEmpty() && (tTitle.contains(rTitle) || rTitle.contains(tTitle)))
        return true;
    if (fileName.contains(rTitle))
        return true;

    // 2. Artist match + partial title
    if (rArtist.isNotEmpty() && (tArtist.contains(rArtist) || rArtist.contains(tArtist) || fileName.contains(rArtist)))
    {
        auto prefix = rTitle.substring(0, std::min(5, rTitle.length()));
        if (prefix.isNotEmpty() && (tTitle.contains(prefix) || fileName.contains(prefix)))
            return true;
    }

    return false;
}
} // namespace

void MainComponent::startSmartAutomix()
{
    if (smartAutomixBusy)
        return;

    // Deck activo (misma lógica del timer): el que suena manda.
    float xfader = audioEngine.getMixer().getCrossfader();
    bool d1Playing = deck1.isPlaying() || deck1.isVideoPlayingActive();
    bool d2Playing = deck2.isPlaying() || deck2.isVideoPlayingActive();
    int activeDeckIdx = 0;
    if (d2Playing && !d1Playing)
        activeDeckIdx = 1;
    else if (d1Playing && !d2Playing)
        activeDeckIdx = 0;
    else
        activeDeckIdx = (xfader > 0.0f) ? 1 : 0;

    auto& activeDeckComp = (activeDeckIdx == 0) ? deck1 : deck2;
    if (!activeDeckComp.isLoaded())
    {
        library.setQueueMessage(LanguageManager::trUtf8("Pon un tema a sonar y pulsa AutoMix IA",
                                                        "Play a track, then press Smart AutoMix"));
        return;
    }

    auto items = library.allLocalTrackItems();
    if (items.empty())
    {
        library.setQueueMessage(LanguageManager::trUtf8("Tu biblioteca local está vacía (PC o USB)",
                                                        "Your local library is empty (PC or USB)"));
        return;
    }

    const juce::File playingFile = audioEngine.getDeck(activeDeckIdx).getCurrentFile();
    auto currentTitle = activeDeckComp.getTrackTitle();
    auto currentArtist = activeDeckComp.getTrackArtist();
    double currentBpm = activeDeckComp.getBpm();
    auto currentKey = activeDeckComp.getTrackKey();

    // Detect folder of playing track
    juce::String currentFolder;
    for (const auto& it : items)
    {
        if (!playingFile.getFullPathName().isEmpty() && it.file == playingFile)
        {
            currentFolder = it.folder;
            if (currentArtist.isEmpty() || currentArtist == "Artista Local")
                currentArtist = it.artist;
            if (currentTitle.isEmpty())
                currentTitle = it.title;
            break;
        }
    }
    if (currentFolder.isEmpty() && playingFile.exists())
    {
        currentFolder = playingFile.getParentDirectory().getFileName();
    }

    std::vector<TrackItem> validCandidates;
    validCandidates.reserve(items.size());
    for (const auto& item : items)
    {
        if (!playingFile.getFullPathName().isEmpty() && item.file == playingFile)
            continue;

        // Ignorar audios de menos de 1 minuto (60 segundos) para excluir samples, loops y jingles
        double dur = item.duration;
        if (dur <= 0.0 && item.file.existsAsFile())
        {
            if (auto r = std::unique_ptr<juce::AudioFormatReader>(audioEngine.getFormatManager().createReaderFor(item.file)))
                dur = (double)r->lengthInSamples / r->sampleRate;
        }
        if (dur > 0.0 && dur < 60.0)
            continue;

        validCandidates.push_back(item);
    }

    if (validCandidates.empty())
    {
        library.setQueueMessage(LanguageManager::trUtf8("No hay más pistas en tu biblioteca para continuar el AutoMix",
                                                        "No more tracks in your library to continue AutoMix"));
        return;
    }

    // Score and rank all candidate tracks
    struct ScoredTrack {
        int score;
        int originalIndex;
        TrackItem item;
    };
    std::vector<ScoredTrack> scored;
    scored.reserve(validCandidates.size());
    for (size_t i = 0; i < validCandidates.size(); ++i)
    {
        int s = scoreTrackCandidate(validCandidates[i], currentTitle, currentArtist, currentBpm, currentKey, currentFolder);
        scored.push_back({ s, (int)i, validCandidates[i] });
    }

    std::stable_sort(scored.begin(), scored.end(), [](const ScoredTrack& a, const ScoredTrack& b) {
        return a.score > b.score;
    });

    std::vector<TrackItem> fallbackCandidates;
    fallbackCandidates.reserve(10);
    for (size_t i = 0; i < std::min((size_t)10, scored.size()); ++i)
        fallbackCandidates.push_back(scored[i].item);

    // If AI is not configured or disabled, start local harmonic automix immediately
    if (!AIService::isEnabled() || !AIService::isConfigured())
    {
        library.setAutomixQueue(fallbackCandidates);
        library.setQueueMessage(
            LanguageManager::trUtf8("AutoMix Local (IA no configurada): armada por afinidad y armonía",
                                    "Local AutoMix (AI not configured): built by affinity & harmony"));
        startAutomix(fallbackCandidates);
        return;
    }

    CurrentTrackContext ctx;
    ctx.activeDeck = activeDeckIdx;
    ctx.title = currentTitle;
    ctx.artist = currentArtist;
    ctx.bpm = currentBpm;
    ctx.key = currentKey;
    ctx.isVideo = activeDeckComp.isVideoModeActive();
    ctx.videoId = activeDeckComp.getVideoId();

    // Populate top ~30 library candidates for AI
    size_t numToPass = std::min((size_t)30, scored.size());
    ctx.libraryCandidates.reserve(numToPass);
    for (size_t i = 0; i < numToPass; ++i)
    {
        CandidateTrackInfo info;
        info.index = (int)(i + 1);
        info.title = scored[i].item.title;
        info.artist = scored[i].item.artist;
        info.bpm = scored[i].item.bpm;
        info.key = scored[i].item.key;
        info.folder = scored[i].item.folder;
        ctx.libraryCandidates.push_back(std::move(info));
    }

    smartAutomixBusy = true;
    library.setSmartBusy(true);

    juce::Component::SafePointer<MainComponent> safeThis(this);
    AIService::requestRecommendationsAsync(ctx, [safeThis, fallback = std::move(fallbackCandidates), allCandidates = std::move(validCandidates)](
            const std::vector<AIRecommendation>& recs, const juce::String& errorMsg) {
        if (safeThis == nullptr)
            return;
        safeThis->smartAutomixBusy = false;
        safeThis->library.setSmartBusy(false);

        std::vector<TrackItem> matched;

        // If AI succeeded with recommendations, match them against candidates
        if (errorMsg.isEmpty() && !recs.empty())
        {
            for (const auto& r : recs)
            {
                if ((int)matched.size() >= 10)
                    break;

                for (const auto& cand : allCandidates)
                {
                    bool alreadyIn = false;
                    for (const auto& m : matched)
                    {
                        if (m.file == cand.file)
                        {
                            alreadyIn = true;
                            break;
                        }
                    }
                    if (alreadyIn)
                        continue;

                    if (trackMatchesRecommendation(cand, r.title, r.artist))
                    {
                        matched.push_back(cand);
                        break;
                    }
                }
            }
        }

        // Fallback: If AI returned 0 matches, errored or timed out, use top local candidates
        if (matched.empty())
        {
            matched = fallback;
            safeThis->library.setAutomixQueue(matched);
            safeThis->library.setQueueMessage(
                LanguageManager::trUtf8("AutoMix Local: Selección por afinidad, tempo y tonalidad (",
                                        "Local AutoMix: Selected by affinity, tempo & key (")
                + juce::String((int)matched.size())
                + LanguageManager::trUtf8(" pistas)", " tracks)"));
            safeThis->startAutomix(matched);
            return;
        }

        // If AI returned fewer than 10 tracks, complete with top local fallback candidates
        if ((int)matched.size() < 10 && !fallback.empty())
        {
            for (const auto& fb : fallback)
            {
                if ((int)matched.size() >= 10)
                    break;
                bool alreadyIn = false;
                for (const auto& m : matched)
                {
                    if (m.file == fb.file)
                    {
                        alreadyIn = true;
                        break;
                    }
                }
                if (!alreadyIn)
                    matched.push_back(fb);
            }
        }

        safeThis->library.setAutomixQueue(matched);
        safeThis->library.setQueueMessage(
            LanguageManager::trUtf8("AutoMix IA: ", "Smart AutoMix: ")
            + juce::String((int)matched.size())
            + LanguageManager::trUtf8(" pistas seleccionadas", " tracks selected"));
        safeThis->startAutomix(matched);
    });
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
            library.setAutomixPlayingIndex(-1);
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
    library.setAutomixPlayingIndex(automixCurrentIndex);
}

void MainComponent::openSettings(int targetTab)
{
    if (settingsWindow != nullptr)
    {
        if (auto* content = dynamic_cast<SettingsModalComponent*>(settingsWindow->getContentComponent()))
            content->setActiveTab(targetTab);

        settingsWindow->toFront(true);
        return;
    }

    auto* modal = new SettingsModalComponent(audioEngine, midiManager, targetTab);
    juce::DialogWindow::LaunchOptions opt;
    opt.dialogTitle = LanguageManager::trUtf8("Preferencias de DiscNativePro", "DiscNativePro Preferences");
    opt.dialogBackgroundColour = juce::Colour::fromRGB(26, 29, 38);
    opt.content.setOwned(modal);
    opt.resizable = false;
    opt.useNativeTitleBar = true;
    opt.escapeKeyTriggersCloseButton = true;
    settingsWindow = opt.launchAsync();

    modal->onDownloadsFolderChanged = [this]() {
        library.scanDownloadsFolder();
    };

    modal->onUpdateAvailabilityChanged = [this](bool available) {
        topBar.setUpdateAvailable(available);
    };

    modal->onClose = [this]() {
        if (settingsWindow != nullptr)
        {
            settingsWindow->closeButtonPressed();
        }
    };
}

void MainComponent::scheduleUpdateCheck()
{
    // ponytail: diferido 3s para no competir con el arranque del motor de audio;
    // un solo chequeo por lanzamiento (la API anónima permite 60 req/h).
    // Solo enciende el badge: el diálogo nace del clic (orden de ventanas sano,
    // imposible que quede escondido detrás como fantasma).
    juce::Component::SafePointer<MainComponent> safeThis(this);
    juce::Timer::callAfterDelay(3000, [safeThis]() {
        if (safeThis == nullptr)
            return;
        UpdateService::checkForUpdatesAsync([safeThis](const UpdateInfo& info) {
            if (safeThis == nullptr || !info.available)
                return;
            safeThis->pendingUpdate = info;
            safeThis->topBar.setUpdateAvailable(true);
        });
    });
}

#include "DeckComponent.h"
#include "BpmFormat.h"
#include "YouTubeVideoComponent.h"
#include "../services/YouTubeService.h"
#include "../services/YouTubeAudioDownloader.h"
#include "../services/ThumbnailCache.h"
#include "../audio/AudioEngine.h"
#include <cstdio>
#include <cmath>
#include <thread>

DeckComponent::DeckComponent(int deckNum, DeckPlayer& player, AudioEngine& audioEngine, juce::Colour accentColour)
    : deckIndex(deckNum), deck(player), engine(audioEngine), accent(accentColour),
      overviewWaveform(player, accentColour, deckNum),
      jogWheel(player, accentColour)
{
    pitchSlider.setLookAndFeel(&djLookAndFeel);

    juce::String initialTitle = LanguageManager::trUtf8("Bandeja ", "Deck ") + juce::String(deckIndex + 1);
    juce::String initialArtist = LanguageManager::trUtf8("Sin pista cargada", "No track loaded");
    double initialBpm = 0.0;

    coverArtImage = CoverArtGenerator::getCoverForTrack(initialTitle, 256);
    jogWheel.setTrackInfo(initialTitle, initialBpm, accent);

    // Track Title & Artist
    titleLabel.setText(initialTitle, juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(13.5f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    artistLabel.setText(initialArtist, juce::dontSendNotification);
    artistLabel.setFont(juce::FontOptions(11.0f));
    artistLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(142, 149, 165));
    addAndMakeVisible(artistLabel);

    // Right Header Metadata: Time Remaining, Key Badge, BPM
    timeRemainingLabel.setText("00:00.0", juce::dontSendNotification);
    timeRemainingLabel.setFont(juce::FontOptions(13.5f, juce::Font::bold));
    timeRemainingLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    timeRemainingLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(timeRemainingLabel);

    keyBadge.setText("--", juce::dontSendNotification);
    keyBadge.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    keyBadge.setColour(juce::Label::textColourId, juce::Colour::fromRGB(142, 149, 165));
    keyBadge.setColour(juce::Label::backgroundColourId, juce::Colour::fromRGB(142, 149, 165).withAlpha(0.12f));
    keyBadge.setColour(juce::Label::outlineColourId, juce::Colour::fromRGB(142, 149, 165).withAlpha(0.25f));
    keyBadge.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(keyBadge);

    bpmLabel.setText("--", juce::dontSendNotification);
    bpmLabel.setFont(juce::FontOptions(13.5f, juce::Font::bold));
    bpmLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    bpmLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(bpmLabel);

    // Overview Waveform
    addAndMakeVisible(overviewWaveform);
    overviewWaveform.onVideoSeek = [this](double seconds) { seekVideo(seconds); };

    // Pitch Section Controls
    syncButton.setText("SYNC");
    syncButton.setFontSize(10.0f);
    syncButton.setCornerRadius(4.0f);
    syncButton.setCustomColours(juce::Colour::fromRGB(24, 27, 36),
                                juce::Colour::fromRGB(142, 149, 165),
                                juce::Colour::fromRGB(142, 149, 165),
                                juce::Colour::fromRGB(38, 43, 56));
    syncButton.setClickingTogglesState(false);
    syncButton.setToggleState(false, juce::dontSendNotification);
    syncButton.onClick = [this]() {
        const bool applied = engine.syncDeckTo(deckIndex);

        const juce::Colour neutralBg     = juce::Colour::fromRGB(24, 27, 36);
        const juce::Colour neutralFg     = juce::Colour::fromRGB(142, 149, 165);
        const juce::Colour neutralBorder = juce::Colour::fromRGB(38, 43, 56);

        if (applied)
            syncButton.setCustomColours(juce::Colour::fromRGB(37, 99, 235), juce::Colours::white,
                                        juce::Colours::white, juce::Colour::fromRGB(59, 130, 246));
        else
            syncButton.setCustomColours(juce::Colour::fromRGB(180, 118, 20), juce::Colours::white,
                                        juce::Colours::white, juce::Colour::fromRGB(217, 155, 40));

        // One-shot flash: blue when applied (1.5 s), amber when not applicable (0.8 s).
        juce::Component::SafePointer<DeckComponent> safe (this);
        DjButton* btn = &syncButton;
        juce::Timer::callAfterDelay(applied ? 1500 : 800,
            [safe, btn, neutralBg, neutralFg, neutralBorder]() {
                if (safe == nullptr) return;
                btn->setCustomColours(neutralBg, neutralFg, neutralFg, neutralBorder);
            });
    };
    addAndMakeVisible(syncButton);

    // Key Lock / Master Tempo toggle (default ON)
    keyLockButton.setText("KEY");
    keyLockButton.setFontSize(10.0f);
    keyLockButton.setCornerRadius(4.0f);
    keyLockButton.setClickingTogglesState(true);
    keyLockButton.setToggleState(deck.isKeylock(), juce::dontSendNotification);
    updateKeyLockButton();
    keyLockButton.onClick = [this]() {
        deck.setKeylock(keyLockButton.getToggleState());
        updateKeyLockButton();
    };
    keyLockButton.setTooltip(LanguageManager::trUtf8("Key Lock: el pitch no desafina", "Key Lock: pitch keeps key"));
    addAndMakeVisible(keyLockButton);

    pitchBpmLabel.setText(juce::String(initialBpm, 1), juce::dontSendNotification);
    pitchBpmLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    pitchBpmLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    pitchBpmLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(pitchBpmLabel);

    pitchDeltaLabel.setText("0.0%", juce::dontSendNotification);
    pitchDeltaLabel.setFont(juce::FontOptions(10.0f));
    pitchDeltaLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(142, 149, 165));
    pitchDeltaLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(pitchDeltaLabel);

    pitchSlider.setSliderStyle(juce::Slider::LinearVertical);
    pitchSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    pitchSlider.setRange(-8.0, 8.0, 0.05);
    pitchSlider.setValue(0.0);
    pitchSlider.setDoubleClickReturnValue(true, 0.0);
    pitchSlider.onValueChange = [this]() {
        float p = (float)pitchSlider.getValue();
        deck.setPitchPercent(p);
        pitchDeltaLabel.setText((p >= 0.0f ? "+" : "") + juce::String(p, 1) + "%", juce::dontSendNotification);
    };
    addAndMakeVisible(pitchSlider);

    pitchBendDownButton.setIcon(LucideIcons::IconType::Minus, 11.0f);
    pitchBendDownButton.setCornerRadius(4.0f);
    pitchBendDownButton.setCustomColours(juce::Colour::fromRGB(22, 24, 32),
                                         juce::Colour::fromRGB(142, 149, 165),
                                         juce::Colour::fromRGB(142, 149, 165),
                                         juce::Colour::fromRGB(38, 43, 56));
    pitchBendDownButton.onClick = [this]() {
        deck.setPitchBend(-0.04f);
        juce::Timer::callAfterDelay(150, [this]() { deck.setPitchBend(0.0f); });
    };
    addAndMakeVisible(pitchBendDownButton);

    pitchBendUpButton.setIcon(LucideIcons::IconType::Plus, 11.0f);
    pitchBendUpButton.setCornerRadius(4.0f);
    pitchBendUpButton.setCustomColours(juce::Colour::fromRGB(22, 24, 32),
                                       juce::Colour::fromRGB(142, 149, 165),
                                       juce::Colour::fromRGB(142, 149, 165),
                                       juce::Colour::fromRGB(38, 43, 56));
    pitchBendUpButton.onClick = [this]() {
        deck.setPitchBend(0.04f);
        juce::Timer::callAfterDelay(150, [this]() { deck.setPitchBend(0.0f); });
    };
    addAndMakeVisible(pitchBendUpButton);

    // Center Platter
    addAndMakeVisible(jogWheel);

    // Bottom Controls Bar (PLAY, CUE, Loop)
    playButton.setIcon(LucideIcons::IconType::Play, 13.0f);
    playButton.setText("PLAY");
    playButton.setFontSize(11.5f);
    playButton.setCornerRadius(6.0f);
    playButton.setCustomColours(juce::Colour::fromRGB(24, 27, 35),
                                juce::Colours::white,
                                juce::Colour::fromRGB(30, 215, 96),
                                juce::Colour::fromRGB(38, 43, 56));
    playButton.onClick = [this]() {
        togglePlayPause();
    };
    addAndMakeVisible(playButton);

    cueButton.setIcon(LucideIcons::IconType::Disc, 13.0f);
    cueButton.setText("CUE");
    cueButton.setFontSize(11.5f);
    cueButton.setCornerRadius(6.0f);
    cueButton.setCustomColours(juce::Colour::fromRGB(24, 27, 35),
                               juce::Colour::fromRGB(245, 158, 11),
                               juce::Colour::fromRGB(245, 158, 11),
                               juce::Colour::fromRGB(38, 43, 56));
    cueButton.onClick = [this]() {
        triggerCue();
    };
    addAndMakeVisible(cueButton);

    // Hot Cue pads (style matching STEMS/FX; session-RAM only, same store as MIDI pads)
    hotCueRowLabel.setText("HOT CUE", juce::dontSendNotification);
    hotCueRowLabel.setFont(juce::FontOptions(9.5f, juce::Font::bold));
    hotCueRowLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(142, 149, 165));
    hotCueRowLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(hotCueRowLabel);

    for (int i = 0; i < 4; ++i)
    {
        auto& pad = hotCueButtons[static_cast<size_t>(i)];
        pad.setText(juce::String(i + 1));
        pad.setFontSize(10.5f);
        pad.setCornerRadius(4.0f);
        pad.setCustomColours(juce::Colour::fromRGB(24, 26, 32),
                             juce::Colour::fromRGB(148, 163, 184),
                             juce::Colour::fromRGB(148, 163, 184),
                             juce::Colour::fromRGB(45, 50, 65));
        pad.onClick = [this, i]() {
            if (isVideoMode || !deck.isLoaded())
                return;
            if (deck.hasHotCue(i))
            {
                engine.ensureAudioDeviceRunning();
                deck.jumpToHotCue(i);
            }
            else
            {
                deck.setHotCue(i, deck.getPosition());
            }
            refreshHotCueButtons();
        };
        addAndMakeVisible(pad);
    }
    refreshHotCueButtons();

    // Grouped Loop Controls
    loopPrevButton.setIcon(LucideIcons::IconType::ChevronLeft, 11.0f);
    loopPrevButton.setCornerRadius(4.0f);
    loopPrevButton.setCustomColours(juce::Colour::fromRGB(20, 23, 30),
                                    juce::Colour::fromRGB(142, 149, 165),
                                    juce::Colour::fromRGB(142, 149, 165),
                                    juce::Colour::fromRGB(38, 43, 56));
    loopPrevButton.onClick = [this]() {
        if (currentLoopBeats > 1) currentLoopBeats /= 2;
        loopBeatsButton.setText(juce::String(currentLoopBeats));
        if (deck.isLooping())
        {
            double beatSec = 60.0 / deck.getBpm();
            deck.setLoop(deck.getPosition(), currentLoopBeats * beatSec, true);
        }
    };
    addAndMakeVisible(loopPrevButton);

    loopBeatsButton.setIcon(LucideIcons::IconType::Repeat, 12.0f);
    loopBeatsButton.setText(juce::String(currentLoopBeats));
    loopBeatsButton.setFontSize(11.0f);
    loopBeatsButton.setCornerRadius(4.0f);
    loopBeatsButton.setCustomColours(juce::Colour::fromRGB(24, 27, 35),
                                     juce::Colours::white,
                                     juce::Colour::fromRGB(142, 149, 165),
                                     juce::Colour::fromRGB(38, 43, 56));
    loopBeatsButton.setClickingTogglesState(true);
    loopBeatsButton.onClick = [this]() {
        if (deck.isLooping())
        {
            deck.exitLoop();
            loopBeatsButton.setCustomColours(juce::Colour::fromRGB(24, 27, 35),
                                             juce::Colours::white,
                                             juce::Colour::fromRGB(142, 149, 165),
                                             juce::Colour::fromRGB(38, 43, 56));
        }
        else
        {
            double beatSec = 60.0 / deck.getBpm();
            deck.setLoop(deck.getPosition(), currentLoopBeats * beatSec, true);
            loopBeatsButton.setCustomColours(juce::Colour::fromRGB(37, 99, 235),
                                             juce::Colours::white,
                                             juce::Colours::white,
                                             juce::Colour::fromRGB(59, 130, 246));
        }
    };
    addAndMakeVisible(loopBeatsButton);

    loopNextButton.setIcon(LucideIcons::IconType::ChevronRight, 11.0f);
    loopNextButton.setCornerRadius(4.0f);
    loopNextButton.setCustomColours(juce::Colour::fromRGB(20, 23, 30),
                                    juce::Colour::fromRGB(142, 149, 165),
                                    juce::Colour::fromRGB(142, 149, 165),
                                    juce::Colour::fromRGB(38, 43, 56));
    loopNextButton.onClick = [this]() {
        if (currentLoopBeats < 32) currentLoopBeats *= 2;
        loopBeatsButton.setText(juce::String(currentLoopBeats));
        if (deck.isLooping())
        {
            double beatSec = 60.0 / deck.getBpm();
            deck.setLoop(deck.getPosition(), currentLoopBeats * beatSec, true);
        }
    };
    addAndMakeVisible(loopNextButton);

    LanguageManager::getInstance().addListener(this);
    startTimerHz(60);
}

DeckComponent::~DeckComponent()
{
    LanguageManager::getInstance().removeListener(this);
    stopTimer();
    pitchSlider.setLookAndFeel(nullptr);
}

void DeckComponent::languageChanged()
{
    if (!isLoaded())
    {
        juce::String initialTitle = LanguageManager::trUtf8("Bandeja ", "Deck ") + juce::String(deckIndex + 1);
        juce::String initialArtist = LanguageManager::trUtf8("Sin pista cargada", "No track loaded");
        titleLabel.setText(initialTitle, juce::dontSendNotification);
        artistLabel.setText(initialArtist, juce::dontSendNotification);
        jogWheel.setTrackInfo(initialTitle, 0.0, accent);
    }
    repaint();
}

bool DeckComponent::shouldBlockLoad(LoadOrigin origin) const
{
    // AutoMix siempre carga en la bandeja libre: exento.
    // Solo se bloquea si la bandeja de audio esta sonando ahora mismo (en pausa se carga).
    return origin == LoadOrigin::User
        && engine.isLoadLockEnabled()
        && deck.isPlaying();
}

void DeckComponent::showLoadLockWarning(std::function<void(DeckComponent&)> forcedLoad)
{
    auto message = LanguageManager::trUtf8("La bandeja ", "Deck ")
                 + juce::String(deckIndex + 1)
                 + LanguageManager::trUtf8(" está reproduciendo. ¿Cargar de todos modos?",
                                           " is playing. Load anyway?");

    // AlertWindow manual: el LookAndFeel asigna Return al boton primario, asi que solo
    // "Cancelar" declara atajos (Return y Escape). Enter jamas fuerza la carga.
    auto* alert = new juce::AlertWindow(LanguageManager::trUtf8("Protección de carga", "Load Lock"),
                                       message,
                                       juce::MessageBoxIconType::WarningIcon,
                                       this);
    alert->addButton(LanguageManager::trUtf8("Forzar carga", "Force load"), 1);
    alert->addButton(LanguageManager::trUtf8("Cancelar", "Cancel"), 0,
                     juce::KeyPress(juce::KeyPress::returnKey),
                     juce::KeyPress(juce::KeyPress::escapeKey));

    // El AlertWindow da foco al primer boton: sin foco de teclado, "Forzar carga" solo responde al raton.
    alert->getButton(0)->setWantsKeyboardFocus(false);

    alert->enterModalState(true,
                           juce::ModalCallbackFunction::create(
                               [safe = juce::Component::SafePointer<DeckComponent>(this), forcedLoad](int result) {
                                   if (safe != nullptr && result == 1)
                                       forcedLoad(*safe);
                               }),
                           true);
}

void DeckComponent::loadAudioFile(const juce::File& file, LoadOrigin origin)
{
    if (shouldBlockLoad(origin))
    {
        showLoadLockWarning([file](DeckComponent& self) { self.loadAudioFileNow(file); });
        return;
    }

    loadAudioFileNow(file);
}

void DeckComponent::loadAudioFileNow(const juce::File& file)
{
    if (deck.loadFile(file))
    {
        isVideoMode = false;
        isVideoPlaying = false;
        isExtractingAudio = false;
        extractionProgress = 0.0f;
        loadedVideoId.clear();
        videoPositionSeconds = 0.0;
        videoDurationSeconds = 0.0;
        overviewWaveform.setVideoMode(false);
        overviewWaveform.setVideoProgress(0.0, 0.0);
        if (videoPlayer != nullptr)
        {
            videoPlayer->stop();
            videoPlayer->setVisible(false);
        }
        jogWheel.setVisible(true);

        // Garantizar que CoreAudio este activo para el streaming del deck
        engine.ensureAudioDeviceRunning();

        auto cleanTitle = deck.getTrackTitle().replaceCharacter('_', ' ');
        auto trackArtist = deck.getTrackArtist();
        // ponytail: sin metadata, partir "Artista - Título" del filename antes que "Artista Local"
        juce::String splitArtist, splitTitle;
        if ((trackArtist.isEmpty() || trackArtist == "Artista Local" || trackArtist == "Local Artist")
            && splitArtistTitle(cleanTitle, splitArtist, splitTitle))
        {
            trackArtist = splitArtist;
            cleanTitle = splitTitle;
        }
        titleLabel.setText(cleanTitle, juce::dontSendNotification);
        artistLabel.setText(trackArtist, juce::dontSendNotification);
        double bpm = deck.getBpm();
        bpmLabel.setText(formatBpm(bpm), juce::dontSendNotification);
        pitchBpmLabel.setText(formatBpm(bpm), juce::dontSendNotification);
        keyBadge.setText(deck.getTrackKey(), juce::dontSendNotification);
        auto keyCol = deck.getTrackKeyColour();
        keyBadge.setColour(juce::Label::textColourId, keyCol);
        keyBadge.setColour(juce::Label::backgroundColourId, keyCol.withAlpha(0.15f));
        keyBadge.setColour(juce::Label::outlineColourId, keyCol.withAlpha(0.35f));
        pitchSlider.setValue(0.0);
        overviewWaveform.updateWaveform();

        coverArtImage = CoverArtGenerator::getCoverForTrack(cleanTitle, 256);
        jogWheelBpmShown = -1.0; // P5: el BPM real llega del worker de análisis
        jogWheel.setTrackInfo(cleanTitle, bpm, accent);
        refreshHotCueButtons();

        repaint();

        if (onFileLoaded)
            onFileLoaded(file, bpm);
    }
}

bool DeckComponent::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& f : files)
    {
        juce::File file(f);
        auto ext = file.getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".mp3" || ext == ".flac" || ext == ".aiff" ||
            ext == ".aif" || ext == ".m4a" || ext == ".ogg" || ext == ".aac")
            return true;
    }
    return false;
}

void DeckComponent::fileDragEnter(const juce::StringArray&, int, int)
{
    isDragOver = true;
    if (!isVideoMode)
        jogWheel.setVisible(false);
    repaint();
}

void DeckComponent::fileDragExit(const juce::StringArray&)
{
    isDragOver = false;
    if (!isVideoMode)
        jogWheel.setVisible(true);
    repaint();
}

void DeckComponent::filesDropped(const juce::StringArray& files, int /*x*/, int /*y*/)
{
    isDragOver = false;
    if (!isVideoMode)
        jogWheel.setVisible(true);
    for (const auto& f : files)
    {
        juce::File file(f);
        if (file.existsAsFile())
        {
            loadAudioFile(file);
            break;
        }
    }
    repaint();
}

bool DeckComponent::isInterestedInDragSource(const SourceDetails& dragSourceDetails)
{
    return dragSourceDetails.description.toString().isNotEmpty();
}

void DeckComponent::itemDragEnter(const SourceDetails&)
{
    isDragOver = true;
    if (!isVideoMode)
        jogWheel.setVisible(false);
    repaint();
}

void DeckComponent::itemDragExit(const SourceDetails&)
{
    isDragOver = false;
    if (!isVideoMode)
        jogWheel.setVisible(true);
    repaint();
}

void DeckComponent::itemDropped(const SourceDetails& dragSourceDetails)
{
    isDragOver = false;
    if (!isVideoMode)
        jogWheel.setVisible(true);
    juce::String desc = dragSourceDetails.description.toString();
    if (desc.startsWith("yt:"))
    {
        auto parts = juce::StringArray::fromTokens(desc.substring(3), "\t", "");
        YouTubeSearchResult res;
        res.id = parts.size() > 0 ? parts[0] : "";
        res.title = parts.size() > 1 && parts[1].isNotEmpty() ? parts[1] : ("YouTube (" + res.id + ")");
        res.artist = parts.size() > 2 && parts[2].isNotEmpty() ? parts[2] : "YouTube";
        res.thumbnailUrl = parts.size() > 3 ? parts[3] : "";
        res.durationSeconds = parts.size() > 4 ? parts[4].getIntValue() : 180;
        res.durationText = parts.size() > 5 ? parts[5] : "3:00";

        loadYouTubeVideo(res);
        repaint();
        return;
    }
    juce::File file(desc.upToFirstOccurrenceOf("\n", false, false).trim());
    if (file.existsAsFile())
    {
        loadAudioFile(file);
    }
    repaint();
}

void DeckComponent::loadYouTubeVideo(const YouTubeSearchResult& result, LoadOrigin origin)
{
    if (shouldBlockLoad(origin))
    {
        showLoadLockWarning([result](DeckComponent& self) { self.loadYouTubeVideoNow(result); });
        return;
    }

    loadYouTubeVideoNow(result);
}

void DeckComponent::loadYouTubeVideoNow(const YouTubeSearchResult& result)
{
    const bool autoplay = YouTubeService::isVideoAutoplayEnabled();
    const bool audioMode = YouTubeService::isAudioPlaybackMode();

    if (!audioMode)
    {
        // Modo Video: reproductor web WKWebView
        isVideoMode = true;
        isVideoPlaying = autoplay;
        isExtractingAudio = false;
        extractionProgress = 0.0f;
        loadedVideoId = result.id;
        videoPositionSeconds = 0.0;
        videoDurationSeconds = result.durationSeconds > 0 ? (double)result.durationSeconds : 0.0;
        videoLastTickMs = juce::Time::getMillisecondCounter();
        overviewWaveform.setVideoMode(true);
        overviewWaveform.setVideoProgress(0.0, videoDurationSeconds);
        deck.stop();

        if (videoPlayer == nullptr)
        {
            videoPlayer = std::make_unique<YouTubeVideoComponent>(deckIndex);
            addAndMakeVisible(*videoPlayer);
        }

        videoPlayer->loadVideo(result.id, result.title, autoplay);
        videoPlayer->setVisible(true);
        jogWheel.setVisible(false);

        auto displayTitle = result.title.isNotEmpty() ? result.title : ("YouTube Video (" + result.id + ")");
        auto displayArtist = result.artist.isNotEmpty() ? result.artist : "YouTube DJ";
        juce::String splitArtist, splitTitle;
        if (splitArtistTitle(displayTitle, splitArtist, splitTitle)
            && (displayArtist == "YouTube DJ" || displayArtist == "YouTube" || displayArtist == "YouTube Creator"
                || displayArtist.isEmpty() || !splitArtist.equalsIgnoreCase(displayArtist)))
        {
            displayArtist = splitArtist;
            displayTitle = splitTitle;
        }

        titleLabel.setText(displayTitle, juce::dontSendNotification);
        artistLabel.setText(displayArtist, juce::dontSendNotification);
        bpmLabel.setText(formatBpm(128.0), juce::dontSendNotification);
        pitchBpmLabel.setText(formatBpm(128.0), juce::dontSendNotification);
        timeRemainingLabel.setText(result.durationText.isNotEmpty() ? result.durationText : "--:--", juce::dontSendNotification);
        keyBadge.setText("YT", juce::dontSendNotification);

        if (displayTitle.startsWith("YouTube Video ("))
            resolveVideoMetadataAsync(result.id);

        refreshHotCueButtons();
        resized();
        repaint();
        return;
    }

    // Modo Audio nativo: Jog Wheel, CoreAudio y extracción
    loadedVideoId = result.id;
    isVideoMode = false;
    isVideoPlaying = false;
    if (videoPlayer != nullptr)
    {
        videoPlayer->stop();
        videoPlayer->setVisible(false);
    }
    jogWheel.setVisible(true);

    auto displayTitle = result.title.isNotEmpty() ? result.title : ("YouTube Video (" + result.id + ")");
    auto displayArtist = result.artist.isNotEmpty() ? result.artist : "YouTube DJ";
    juce::String splitArtist, splitTitle;
    if (splitArtistTitle(displayTitle, splitArtist, splitTitle)
        && (displayArtist == "YouTube DJ" || displayArtist == "YouTube" || displayArtist == "YouTube Creator"
            || displayArtist.isEmpty() || !splitArtist.equalsIgnoreCase(displayArtist)))
    {
        displayArtist = splitArtist;
        displayTitle = splitTitle;
    }

    titleLabel.setText(displayTitle, juce::dontSendNotification);
    artistLabel.setText(displayArtist, juce::dontSendNotification);
    timeRemainingLabel.setText(result.durationText.isNotEmpty() ? result.durationText : "--:--", juce::dontSendNotification);

    // 1. Check if audio is already in local cache
    auto cachedFile = YouTubeAudioDownloader::getInstance().getCachedAudioFile(result.id);
    if (cachedFile.existsAsFile())
    {
        isExtractingAudio = false;
        extractionProgress = 0.0f;
        loadAudioFileNow(cachedFile);

        // Keep clean YouTube title & artist
        titleLabel.setText(displayTitle, juce::dontSendNotification);
        artistLabel.setText(displayArtist, juce::dontSendNotification);
        jogWheel.setTrackInfo(displayTitle, deck.getBpm(), accent);

        // Asynchronously load thumbnail if not yet in memory
        auto thumb = ThumbnailCache::getInstance().getThumbnail(result.id, result.thumbnailUrl,
            [safe = juce::Component::SafePointer<DeckComponent>(this), vid = result.id](const juce::String& readyId) {
                if (safe == nullptr || safe->loadedVideoId != vid) return;
                auto img = ThumbnailCache::getInstance().getThumbnail(readyId, "");
                if (img.isValid())
                {
                    safe->coverArtImage = img;
                    safe->jogWheel.setCustomCoverArt(img);
                    safe->repaint();
                }
            });
        if (thumb.isValid())
        {
            coverArtImage = thumb;
            jogWheel.setCustomCoverArt(thumb);
        }

        if (autoplay)
            deck.play();

        resized();
        repaint();
        return;
    }

    // 2. Audio needs extraction: stop previous sound and show progress banner on deck
    deck.stop();
    isExtractingAudio = true;
    extractionProgress = 0.05f;
    extractionStatus = LanguageManager::trUtf8("Descargando audio de YouTube...", "Downloading YouTube audio...");
    overviewWaveform.setVideoMode(false);
    overviewWaveform.setVideoProgress(0.0, 0.0);
    jogWheel.setTrackInfo(displayTitle, 0.0, accent);

    // Prefetch thumbnail so it's ready when extraction finishes
    auto thumb = ThumbnailCache::getInstance().getThumbnail(result.id, result.thumbnailUrl,
        [safe = juce::Component::SafePointer<DeckComponent>(this), vid = result.id](const juce::String& readyId) {
            if (safe == nullptr || safe->loadedVideoId != vid) return;
            auto img = ThumbnailCache::getInstance().getThumbnail(readyId, "");
            if (img.isValid())
            {
                safe->coverArtImage = img;
                safe->jogWheel.setCustomCoverArt(img);
                safe->repaint();
            }
        });
    if (thumb.isValid())
    {
        coverArtImage = thumb;
        jogWheel.setCustomCoverArt(thumb);
    }

    if (displayTitle.startsWith("YouTube Video ("))
        resolveVideoMetadataAsync(result.id);

    resized();
    repaint();

    YouTubeAudioDownloader::getInstance().downloadAudioAsync(
        result.id,
        displayTitle,
        displayArtist,
        [safe = juce::Component::SafePointer<DeckComponent>(this), vid = result.id](float progress, const juce::String& status) {
            if (safe == nullptr || safe->loadedVideoId != vid) return;
            safe->extractionProgress = progress;
            safe->extractionStatus = status;
            safe->repaint();
        },
        [safe = juce::Component::SafePointer<DeckComponent>(this), vid = result.id, displayTitle, displayArtist, autoplay](const juce::File& file, bool success, const juce::String& error) {
            if (safe == nullptr || safe->loadedVideoId != vid) return;
            safe->isExtractingAudio = false;

            if (success && file.existsAsFile())
            {
                safe->loadAudioFileNow(file);
                safe->titleLabel.setText(displayTitle, juce::dontSendNotification);
                safe->artistLabel.setText(displayArtist, juce::dontSendNotification);
                safe->jogWheel.setTrackInfo(displayTitle, safe->deck.getBpm(), safe->accent);

                auto img = ThumbnailCache::getInstance().getThumbnail(vid, "");
                if (img.isValid())
                {
                    safe->coverArtImage = img;
                    safe->jogWheel.setCustomCoverArt(img);
                }

                if (autoplay)
                    safe->deck.play();
            }
            else
            {
                safe->extractionStatus = error.isNotEmpty() ? error : LanguageManager::trUtf8("Fallo al descargar audio", "Download failed");
            }
            safe->repaint();
        }
    );
}

juce::String DeckComponent::sanitizeTrackString(const juce::String& text)
{
    auto s = text.trim();

    // 1. Prefijos comunes de convertidores y descargadores web
    static const char* webPrefixes[] = {
        "y2mate.is - ", "y2mate.com - ", "y2mate - ",
        "yt1s.com - ", "yt1s - ",
        "snaptube - ", "snaptube.com - ",
        "ytmp3 - ", "ytmp3.cc - ",
        "convert2mp3 - ",
        "[MP3] ", "(MP3) ", "MP3 - ",
        nullptr
    };
    for (int i = 0; webPrefixes[i] != nullptr; ++i)
    {
        if (s.startsWithIgnoreCase(webPrefixes[i]))
        {
            s = s.substring((int)strlen(webPrefixes[i])).trimStart();
            i = -1; // re-check in case multiple prefixes exist
        }
    }

    // 2. Coletillas y sufijos de calidad / ripeo
    static const char* suffixes[] = {
        " (Official Video)", " (Official Music Video)", " [Official Video]",
        " (Official Audio)", " (Lyric Video)", " (Lyrics)", " [Lyrics]", " (Visualizer)",
        " (Official HD Video)", " (Official Video HD)", " (HD)", " [Official Audio]",
        " (Official Lyric Video)", " (Remastered)", " (Remaster)",
        " (Live)", " (Official Live Video)",
        " [320kbps]", " (320kbps)", " [320 kbps]", " (320 kbps)",
        " [128kbps]", " (128kbps)", " [256kbps]",
        " [HQ]", " (HQ)", " [FLAC]", " (FLAC)",
        " (Audio Oficial)", " [Audio Oficial]", " (Video Oficial)", " [Video Oficial]",
        " (Videoclip Oficial)", " [Videoclip Oficial]",
        nullptr
    };
    for (int i = 0; suffixes[i] != nullptr; ++i)
    {
        auto suf = juce::String::fromUTF8(suffixes[i]);
        if (s.endsWithIgnoreCase(suf))
        {
            s = s.dropLastCharacters(suf.length()).trimEnd();
            i = -1; // re-check
        }
    }

    // 3. Quitar IDs de YouTube de 11 caracteres al final: [dQw4w9WgXcQ] o (dQw4w9WgXcQ)
    if (s.endsWith("]") || s.endsWith(")"))
    {
        auto openBracket = s.lastIndexOfChar('[');
        if (openBracket < 0) openBracket = s.lastIndexOfChar('(');
        if (openBracket >= 0)
        {
            auto inner = s.substring(openBracket + 1, s.length() - 1).trim();
            if (inner.length() == 11)
            {
                bool isYtId = true;
                for (int c = 0; c < inner.length(); ++c)
                {
                    auto ch = inner[c];
                    if (!juce::CharacterFunctions::isLetterOrDigit(ch) && ch != '_' && ch != '-')
                    {
                        isYtId = false;
                        break;
                    }
                }
                if (isYtId)
                {
                    s = s.substring(0, openBracket).trimEnd();
                }
            }
        }
    }

    return s.trim();
}

juce::String DeckComponent::stripTrackNumberPrefix(const juce::String& filename)
{
    auto s = sanitizeTrackString(filename).trimStart();
    int i = 0;
    while (i < s.length() && juce::CharacterFunctions::isDigit(s[i]))
        ++i;

    if (i > 0 && i < s.length())
    {
        auto rest = s.substring(i).trimStart();
        if (rest.startsWith("- ") || rest.startsWith(juce::String::fromUTF8("– ")) || rest.startsWith(juce::String::fromUTF8("— ")))
            return sanitizeTrackString(rest.substring(2).trimStart());
        if (rest.startsWith(". ") || rest.startsWith("._"))
            return sanitizeTrackString(rest.substring(2).trimStart());
        if (rest.startsWith(".") || rest.startsWith("-"))
            return sanitizeTrackString(rest.substring(1).trimStart());
        if (s[i] == ' ')
            return sanitizeTrackString(rest);
    }
    return s;
}

bool DeckComponent::splitArtistTitle(const juce::String& in, juce::String& outArtist, juce::String& outTitle)
{
    auto t = sanitizeTrackString(stripTrackNumberPrefix(in));

    // Separadores artista-título: guion, en-dash (–, típico de YouTube) y em-dash (—)
    int sep = t.indexOf(" - ");
    int sepLen = 3;
    if (sep < 0)
    {
        sep = t.indexOf(juce::String::fromUTF8(" – "));
        sepLen = 3;
    }
    if (sep < 0)
    {
        sep = t.indexOf(juce::String::fromUTF8(" — "));
        sepLen = 3;
    }
    if (sep <= 0)
        return false;

    outArtist = sanitizeTrackString(t.substring(0, sep));
    // Cola "| ..." en vivo: "Song | Live in X" -> "Song"
    outTitle = t.substring(sep + sepLen).trim();
    int pipe = outTitle.indexOf(" | ");
    if (pipe > 0 && outTitle.substring(pipe).toLowerCase().contains("live"))
        outTitle = outTitle.substring(0, pipe).trim();

    outTitle = sanitizeTrackString(outTitle);
    return outArtist.isNotEmpty() && outTitle.isNotEmpty();
}

void DeckComponent::resolveVideoMetadataAsync(const juce::String& videoId)
{
    if (videoId.isEmpty())
        return;

    juce::String idCopy = videoId;
    std::thread([this, idCopy]() {
        auto details = YouTubeService::getVideoDetails(idCopy);
        juce::MessageManager::callAsync([this, idCopy, details]() {
            if (!isVideoMode || loadedVideoId != idCopy)
                return; // el usuario ya cargó otra cosa

            auto title = details.title;
            auto artist = details.artist;
            juce::String splitArtist, splitTitle;
            if (splitArtistTitle(title, splitArtist, splitTitle)
                && (artist.isEmpty() || artist == "YouTube" || artist == "YouTube DJ" || artist == "YouTube Creator"
                    || !splitArtist.equalsIgnoreCase(artist)))
            {
                artist = splitArtist;
                title = splitTitle;
            }
            if (title.isNotEmpty() && !title.startsWith("YouTube Video ("))
                titleLabel.setText(title, juce::dontSendNotification);
            if (artist.isNotEmpty())
                artistLabel.setText(artist, juce::dontSendNotification);

            // Actualizar duración real del video y sincronizar la barra de progreso
            if (details.durationSeconds > 0)
            {
                videoDurationSeconds = (double)details.durationSeconds;
                overviewWaveform.setVideoProgress(videoPositionSeconds, videoDurationSeconds);
            }
            repaint();
        });
    }).detach();
}

void DeckComponent::setVideoVolume(float effectiveVolume)
{
    if (isVideoMode && videoPlayer != nullptr)
    {
        videoPlayer->setVolume(effectiveVolume);
    }
}

void DeckComponent::playVideo()
{
    if (isVideoMode && videoPlayer != nullptr)
    {
        videoPlayer->play();
        isVideoPlaying = true;
    }
}

void DeckComponent::pauseVideo()
{
    if (isVideoMode && videoPlayer != nullptr)
    {
        videoPlayer->pause();
        isVideoPlaying = false;
    }
}

void DeckComponent::togglePlayPause()
{
    if (isVideoMode && videoPlayer != nullptr)
    {
        // Silence any background local audio track in video mode
        if (deck.isPlaying())
            deck.pause();

        if (isVideoPlaying)
            pauseVideo();
        else
            playVideo();
    }
    else
    {
        if (deck.isPlaying())
        {
            deck.pause();
        }
        else
        {
            engine.ensureAudioDeviceRunning();
            deck.play();
        }
    }
}

void DeckComponent::triggerCue()
{
    if (isVideoMode && videoPlayer != nullptr)
    {
        if (deck.isPlaying())
            deck.pause();

        pauseVideo();
        seekVideo(0.0);
    }
    else
    {
        engine.ensureAudioDeviceRunning();
        deck.triggerCue();
    }
}

void DeckComponent::refreshHotCueButtons()
{
    // ponytail: pads are stateless triggers over DeckPlayer RAM store; no disk persist (YAGNI).
    // Disabled (grey) in YouTube video mode where hot cues don't apply.
    for (int i = 0; i < 4; ++i)
    {
        auto& pad = hotCueButtons[static_cast<size_t>(i)];
        bool videoDisabled = isVideoMode;
        bool enabled = !videoDisabled && deck.isLoaded();
        bool active = !videoDisabled && deck.hasHotCue(i);
        int state = (enabled ? 2 : 0) + (active ? 1 : 0);
        if (state == hotCueButtonShown[static_cast<size_t>(i)])
            continue;
        hotCueButtonShown[static_cast<size_t>(i)] = state;
        pad.setEnabled(enabled);
        if (active)
            pad.setCustomColours(juce::Colour::fromRGB(245, 158, 11),
                                 juce::Colours::black,
                                 juce::Colours::black,
                                 juce::Colour::fromRGB(251, 191, 36));
        else
            pad.setCustomColours(juce::Colour::fromRGB(24, 26, 32),
                                 juce::Colour::fromRGB(148, 163, 184),
                                 juce::Colour::fromRGB(148, 163, 184),
                                 juce::Colour::fromRGB(45, 50, 65));
    }
}

void DeckComponent::seekVideo(double seconds)
{
    if (!isVideoMode || videoPlayer == nullptr)
        return;

    double target = juce::jlimit(0.0, videoDurationSeconds > 0.0 ? videoDurationSeconds : seconds, seconds);
    videoPositionSeconds = target;
    videoLastTickMs = juce::Time::getMillisecondCounter();
    videoPlayer->seekTo(target);
    overviewWaveform.setVideoProgress(videoPositionSeconds, videoDurationSeconds);
}

bool DeckComponent::isPlaying() const
{
    return isVideoMode ? isVideoPlaying : deck.isPlaying();
}

void DeckComponent::formatTime(double seconds, char* buffer, size_t bufferSize)
{
    if (seconds < 0.0) seconds = 0.0;
    int totalTenths = (int)(seconds * 10.0);
    int tenths = totalTenths % 10;
    int totalSecs = totalTenths / 10;
    int mins = totalSecs / 60;
    int secs = totalSecs % 60;
    std::snprintf(buffer, bufferSize, "%02d:%02d.%1d", mins, secs, tenths);
}

void DeckComponent::updateKeyLockButton()
{
    bool on = keyLockButton.getToggleState();
    // Verde = lock activo (no desafina), gris = pitch clásico
    keyLockButton.setCustomColours(on ? juce::Colour::fromRGB(20, 60, 45) : juce::Colour::fromRGB(24, 27, 36),
                                   on ? juce::Colour::fromRGB(74, 222, 128) : juce::Colour::fromRGB(142, 149, 165),
                                   on ? juce::Colour::fromRGB(74, 222, 128) : juce::Colour::fromRGB(142, 149, 165),
                                   on ? juce::Colour::fromRGB(34, 197, 94).withAlpha(0.5f) : juce::Colour::fromRGB(38, 43, 56));
}

void DeckComponent::updateLabels()
{
    if (deck.isLoaded() || isVideoMode)
    {
        double pos = isVideoMode ? videoPositionSeconds : deck.getPosition();
        double total = isVideoMode ? videoDurationSeconds : deck.getLengthInSeconds();
        double remaining = std::max(0.0, total - pos);

        char buf[32];
        formatTime(remaining, buf, sizeof(buf));
        timeRemainingLabel.setText("-" + juce::String(buf), juce::dontSendNotification);
    }
    else
    {
        timeRemainingLabel.setText("00:00.0", juce::dontSendNotification);
    }

    // Update Play/Cue glowing states
    bool playing = isVideoMode ? isVideoPlaying : deck.isPlaying();
    if (playing)
    {
        playButton.setIcon(LucideIcons::IconType::Pause, 13.0f);
        playButton.setText("PAUSE");
        playButton.setCustomColours(juce::Colour::fromRGB(30, 215, 96),
                                    juce::Colours::black,
                                    juce::Colours::black,
                                    juce::Colour::fromRGB(38, 230, 108));
    }
    else
    {
        playButton.setIcon(LucideIcons::IconType::Play, 13.0f);
        playButton.setText("PLAY");
        playButton.setCustomColours(juce::Colour::fromRGB(24, 27, 35),
                                    juce::Colours::white,
                                    juce::Colour::fromRGB(30, 215, 96),
                                    juce::Colour::fromRGB(38, 43, 56));
    }

    if (deck.isLooping())
    {
        loopBeatsButton.setCustomColours(juce::Colour::fromRGB(37, 99, 235),
                                         juce::Colours::white,
                                         juce::Colours::white,
                                         juce::Colour::fromRGB(59, 130, 246));
    }
    else
    {
        loopBeatsButton.setCustomColours(juce::Colour::fromRGB(24, 27, 35),
                                         juce::Colours::white,
                                         juce::Colour::fromRGB(142, 149, 165),
                                         juce::Colour::fromRGB(38, 43, 56));
    }

    // Dynamic BPM and pitch reflection (supports DDJ-SB2 MIDI, Tempo Blend and Pitch Bend in real-time)
    if (deck.isLoaded())
    {
        double curBpm = deck.getBpm();
        juce::String bpmText = formatBpm(curBpm);
        if (deck.isPitchClamped())
        {
            bpmText += " ! SYNC fuera de rango";
            bpmLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(245, 158, 11));
            pitchBpmLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(245, 158, 11));
        }
        else
        {
            bpmLabel.setColour(juce::Label::textColourId, juce::Colours::white);
            pitchBpmLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        }
        bpmLabel.setText(bpmText, juce::dontSendNotification);
        pitchBpmLabel.setText(formatBpm(curBpm), juce::dontSendNotification);

        // P5: la tonalidad se detecta en segundo plano; el badge se refresca
        // cuando llega (solo si cambió, sin repintar de más).
        const juce::String currentKey = deck.getTrackKey();
        if (keyBadge.getText() != currentKey)
        {
            keyBadge.setText(currentKey, juce::dontSendNotification);
            const auto keyCol = deck.getTrackKeyColour();
            keyBadge.setColour(juce::Label::textColourId, keyCol);
            keyBadge.setColour(juce::Label::backgroundColourId, keyCol.withAlpha(0.15f));
            keyBadge.setColour(juce::Label::outlineColourId, keyCol.withAlpha(0.35f));
        }

        // P5: el jog wheel se pintó con bpm=0 al cargar (análisis pendiente);
        // se refresca una sola vez cuando llega el BPM detectado.
        const double detectedBpm = deck.getOriginalBpm();
        if (detectedBpm > 0.0 && std::abs(jogWheelBpmShown - detectedBpm) > 1e-9)
        {
            jogWheelBpmShown = detectedBpm;
            jogWheel.setTrackInfo(titleLabel.getText(), deck.getBpm(), accent);
        }
    }
    else
    {
        bpmLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        pitchBpmLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        bpmLabel.setText(formatBpm(0.0), juce::dontSendNotification);
        pitchBpmLabel.setText(formatBpm(0.0), juce::dontSendNotification);
    }

    float currentRange = deck.getPitchRange();
    if (std::abs(pitchSlider.getMaximum() - (double)currentRange) > 0.01)
    {
        pitchSlider.setRange(-currentRange, currentRange, 0.05);
    }

    float currentPitchPercent = deck.getPitchPercent();
    if (!pitchSlider.isMouseButtonDown())
    {
        pitchSlider.setValue(currentPitchPercent, juce::dontSendNotification);
        pitchDeltaLabel.setText((currentPitchPercent >= 0.0f ? "+" : "") + juce::String(currentPitchPercent, 1) + "%", juce::dontSendNotification);
    }
}

void DeckComponent::timerCallback()
{
    jogWheel.updateAngle();
    if (isVideoMode)
    {
        juce::uint32 now = juce::Time::getMillisecondCounter();
        double dt = (now - videoLastTickMs) / 1000.0;
        videoLastTickMs = now;
        if (isVideoPlaying && dt > 0.0 && dt < 5.0)
        {
            videoPositionSeconds += dt;
            if (videoDurationSeconds > 0.0 && videoPositionSeconds >= videoDurationSeconds)
            {
                videoPositionSeconds = videoDurationSeconds;
                isVideoPlaying = false;
            }
            overviewWaveform.setVideoProgress(videoPositionSeconds, videoDurationSeconds);
        }
    }
    updateLabels();
    overviewWaveform.updateWaveform();
    refreshHotCueButtons();
}

void DeckComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Dark deck panel background #0f1116
    g.setColour(juce::Colour::fromRGB(15, 17, 22));
    g.fillRoundedRectangle(bounds.reduced(2.0f), 8.0f);

    g.setColour(juce::Colour::fromRGB(30, 34, 44));
    g.drawRoundedRectangle(bounds.reduced(2.0f), 8.0f, 1.2f);

    // Album Artwork Box (40x40px with rounded corners)
    juce::Rectangle<float> artBox(12.0f, 10.0f, 40.0f, 40.0f);
    if (coverArtImage.isValid())
    {
        juce::Path clipPath;
        clipPath.addRoundedRectangle(artBox, 6.0f);
        juce::Graphics::ScopedSaveState state(g);
        g.reduceClipRegion(clipPath);
        g.drawImage(coverArtImage, artBox, juce::RectanglePlacement::centred | juce::RectanglePlacement::fillDestination);
    }
    else
    {
        juce::ColourGradient artGrad(accent, artBox.getX(), artBox.getY(),
                                     juce::Colour::fromRGB(20, 24, 34), artBox.getRight(), artBox.getBottom(), false);
        g.setGradientFill(artGrad);
        g.fillRoundedRectangle(artBox, 6.0f);
    }

    g.setColour(juce::Colour::fromRGB(48, 54, 70));
    g.drawRoundedRectangle(artBox, 6.0f, 1.0f);

}

void DeckComponent::paintOverChildren(juce::Graphics& g)
{
    // Visual Drag Over Indicator (drawn in foreground on top of all controls)
    if (isDragOver)
    {
        auto bounds = getLocalBounds().toFloat();

        g.setColour(accent.withAlpha(0.18f));
        g.fillRoundedRectangle(bounds.reduced(2.0f), 8.0f);

        g.setColour(accent);
        g.drawRoundedRectangle(bounds.reduced(2.0f), 8.0f, 2.5f);

        juce::Rectangle<float> banner(bounds.getCentreX() - 130.0f, bounds.getCentreY() - 32.0f, 260.0f, 64.0f);
        g.setColour(juce::Colour::fromRGB(12, 14, 18).withAlpha(0.96f));
        g.fillRoundedRectangle(banner, 8.0f);
        g.setColour(accent);
        g.drawRoundedRectangle(banner, 8.0f, 1.5f);

        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        g.drawText(juce::String(LanguageManager::trUtf8("SOLTAR EN DECK ", "DROP ON DECK ")) + juce::String(deckIndex + 1),
                   banner.removeFromTop(36.0f), juce::Justification::centred, false);

        g.setColour(accent);
        g.setFont(juce::FontOptions(10.5f));
        g.drawText(LanguageManager::trUtf8("Pista de Biblioteca o Archivo MP3/WAV", "Library Track or MP3/WAV File"),
                   banner, juce::Justification::centred, false);
    }

    // Audio Extraction Progress Indicator (YouTube / Stream download)
    if (isExtractingAudio)
    {
        auto bounds = getLocalBounds().toFloat();
        juce::Rectangle<float> banner(bounds.getCentreX() - 145.0f, bounds.getCentreY() - 36.0f, 290.0f, 72.0f);
        g.setColour(juce::Colour::fromRGB(12, 14, 18).withAlpha(0.95f));
        g.fillRoundedRectangle(banner, 8.0f);
        g.setColour(accent);
        g.drawRoundedRectangle(banner, 8.0f, 1.5f);

        auto textRect = banner.removeFromTop(38.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(12.5f, juce::Font::bold));
        g.drawText(extractionStatus.isNotEmpty() ? extractionStatus : LanguageManager::trUtf8("Descargando audio de YouTube...", "Downloading YouTube audio..."),
                   textRect, juce::Justification::centred, true);

        auto pBar = banner.reduced(24.0f, 8.0f);
        g.setColour(juce::Colour::fromRGB(32, 36, 48));
        g.fillRoundedRectangle(pBar, 3.0f);
        if (extractionProgress > 0.0f)
        {
            auto fillBar = pBar.withWidth(pBar.getWidth() * juce::jlimit(0.0f, 1.0f, extractionProgress));
            g.setColour(accent);
            g.fillRoundedRectangle(fillBar, 3.0f);
        }
    }
}

void DeckComponent::resized()
{
    auto area = getLocalBounds().reduced(8, 6);

    // 1. Header (44px)
    auto headerArea = area.removeFromTop(44);

    // Left artwork takes 52px
    headerArea.removeFromLeft(52);

    // Right metadata block (Time, Key, BPM)
    auto rightMeta = headerArea.removeFromRight(186);
    bpmLabel.setBounds(rightMeta.removeFromRight(46));
    keyBadge.setBounds(rightMeta.removeFromRight(68).reduced(2, 9));
    timeRemainingLabel.setBounds(rightMeta);

    // Title and Artist fill the middle header space
    titleLabel.setBounds(headerArea.removeFromTop(22));
    artistLabel.setBounds(headerArea);

    area.removeFromTop(6);

    // 2. Overview Waveform (28px)
    overviewWaveform.setBounds(area.removeFromTop(28));
    area.removeFromTop(6);

    // 2b. Hot Cue pads row (28px)
    auto hotCueRow = area.removeFromBottom(28);
    hotCueRowLabel.setBounds(hotCueRow.removeFromLeft(62).reduced(2, 0));
    hotCueRow.removeFromLeft(4);
    int padW = (hotCueRow.getWidth() - 18) / 4;
    for (int i = 0; i < 4; ++i)
    {
        hotCueButtons[static_cast<size_t>(i)].setBounds(hotCueRow.removeFromLeft(padW).reduced(2, 0));
        if (i < 3)
            hotCueRow.removeFromLeft(6);
    }
    area.removeFromBottom(6);

    // 3. Bottom Transport Controls Bar (36px)
    auto bottomBar = area.removeFromBottom(36);
    int thirdW = (bottomBar.getWidth() - 16) / 3;

    playButton.setBounds(bottomBar.removeFromLeft(thirdW).reduced(2, 0));
    bottomBar.removeFromLeft(8);

    cueButton.setBounds(bottomBar.removeFromLeft(thirdW).reduced(2, 0));
    bottomBar.removeFromLeft(8);

    // Grouped Loop Controls
    auto loopArea = bottomBar.reduced(2, 0);
    loopPrevButton.setBounds(loopArea.removeFromLeft(28));
    loopNextButton.setBounds(loopArea.removeFromRight(28));
    loopBeatsButton.setBounds(loopArea);

    area.removeFromBottom(8);

    // 4. Center Workspace: Jog Wheel + Pitch Section
    int pitchW = 58;
    juce::Rectangle<int> pitchArea;
    if (deckIndex == 0)
    {
        // Deck 1: Pitch on left edge
        pitchArea = area.removeFromLeft(pitchW);
        area.removeFromLeft(12);
        jogWheel.setBounds(area);
        if (videoPlayer != nullptr)
        {
            videoPlayer->setBounds(area);
            videoPlayer->setVisible(isVideoMode);
        }
        jogWheel.setVisible(!isVideoMode);
    }
    else
    {
        // Deck 2: Pitch on right edge
        pitchArea = area.removeFromRight(pitchW);
        area.removeFromRight(12);
        jogWheel.setBounds(area);
        if (videoPlayer != nullptr)
        {
            videoPlayer->setBounds(area);
            videoPlayer->setVisible(isVideoMode);
        }
        jogWheel.setVisible(!isVideoMode);
    }

    // Layout Pitch Area
    syncButton.setBounds(pitchArea.removeFromTop(22).reduced(4, 0));
    pitchArea.removeFromTop(4);
    keyLockButton.setBounds(pitchArea.removeFromTop(22).reduced(4, 0));
    pitchArea.removeFromTop(4);
    pitchBpmLabel.setBounds(pitchArea.removeFromTop(16));
    pitchDeltaLabel.setBounds(pitchArea.removeFromTop(14));
    pitchArea.removeFromTop(4);

    // Bottom +/- buttons
    auto bendArea = pitchArea.removeFromBottom(22).reduced(2, 0);
    pitchBendDownButton.setBounds(bendArea.removeFromLeft(24));
    pitchBendUpButton.setBounds(bendArea.removeFromRight(24));

    pitchArea.removeFromBottom(4);
    pitchSlider.setBounds(pitchArea);
}

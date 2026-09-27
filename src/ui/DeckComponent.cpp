#include "DeckComponent.h"
#include "YouTubeVideoComponent.h"
#include "../services/YouTubeService.h"
#include <cstdio>
#include <cmath>

DeckComponent::DeckComponent(int deckNum, DeckPlayer& player, juce::Colour accentColour)
    : deckIndex(deckNum), deck(player), accent(accentColour),
      overviewWaveform(player, accentColour),
      jogWheel(player, accentColour)
{
    pitchSlider.setLookAndFeel(&djLookAndFeel);

    juce::String initialTitle = (deckIndex == 0) ? "DiscPro Latin Tech Groove" : "Sunset Beach House Mix";
    juce::String initialArtist = (deckIndex == 0) ? "Axel Molina & DiscPro Sound" : "Deep Horizon";
    double initialBpm = (deckIndex == 0) ? 124.0 : 126.0;

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
    timeRemainingLabel.setText(deckIndex == 0 ? "-01:01.9" : "-01:00.9", juce::dontSendNotification);
    timeRemainingLabel.setFont(juce::FontOptions(13.5f, juce::Font::bold));
    timeRemainingLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    timeRemainingLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(timeRemainingLabel);

    keyBadge.setText(deckIndex == 0 ? "8A" : "11A", juce::dontSendNotification);
    keyBadge.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    keyBadge.setColour(juce::Label::textColourId, juce::Colour::fromRGB(0, 229, 255));
    keyBadge.setColour(juce::Label::backgroundColourId, juce::Colour::fromRGB(0, 229, 255).withAlpha(0.15f));
    keyBadge.setColour(juce::Label::outlineColourId, juce::Colour::fromRGB(0, 229, 255).withAlpha(0.35f));
    keyBadge.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(keyBadge);

    bpmLabel.setText(juce::String(initialBpm, 1), juce::dontSendNotification);
    bpmLabel.setFont(juce::FontOptions(13.5f, juce::Font::bold));
    bpmLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    bpmLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(bpmLabel);

    // Overview Waveform
    addAndMakeVisible(overviewWaveform);

    // Pitch Section Controls
    syncButton.setText("SYNC");
    syncButton.setFontSize(10.0f);
    syncButton.setCornerRadius(4.0f);
    syncButton.setCustomColours(juce::Colour::fromRGB(24, 27, 36),
                                juce::Colour::fromRGB(142, 149, 165),
                                juce::Colour::fromRGB(142, 149, 165),
                                juce::Colour::fromRGB(38, 43, 56));
    syncButton.setClickingTogglesState(true);
    syncButton.onClick = [this]() {
        bool on = syncButton.getToggleState();
        syncButton.setCustomColours(on ? juce::Colour::fromRGB(37, 99, 235) : juce::Colour::fromRGB(24, 27, 36),
                                    on ? juce::Colours::white : juce::Colour::fromRGB(142, 149, 165),
                                    on ? juce::Colours::white : juce::Colour::fromRGB(142, 149, 165),
                                    on ? juce::Colour::fromRGB(59, 130, 246) : juce::Colour::fromRGB(38, 43, 56));
    };
    addAndMakeVisible(syncButton);

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
        if (isVideoMode && videoPlayer != nullptr)
        {
            if (isVideoPlaying)
            {
                videoPlayer->pause();
                isVideoPlaying = false;
            }
            else
            {
                videoPlayer->play();
                isVideoPlaying = true;
            }
        }
        else
        {
            if (deck.isPlaying()) deck.pause();
            else deck.play();
        }
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
        if (isVideoMode && videoPlayer != nullptr)
        {
            videoPlayer->pause();
            videoPlayer->seekTo(0.0);
            isVideoPlaying = false;
        }
        else
        {
            deck.triggerCue();
        }
    };
    addAndMakeVisible(cueButton);

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

    startTimerHz(60);
}

DeckComponent::~DeckComponent()
{
    stopTimer();
    pitchSlider.setLookAndFeel(nullptr);
}

void DeckComponent::loadAudioFile(const juce::File& file)
{
    if (deck.loadFile(file))
    {
        isVideoMode = false;
        isVideoPlaying = false;
        if (videoPlayer != nullptr)
        {
            videoPlayer->pause();
            videoPlayer->setVisible(false);
        }
        jogWheel.setVisible(true);

        auto cleanTitle = deck.getTrackTitle().replaceCharacter('_', ' ');
        titleLabel.setText(cleanTitle, juce::dontSendNotification);
        artistLabel.setText(deck.getTrackArtist(), juce::dontSendNotification);
        double bpm = deck.getBpm();
        bpmLabel.setText(juce::String(bpm, 1), juce::dontSendNotification);
        pitchBpmLabel.setText(juce::String(bpm, 1), juce::dontSendNotification);
        pitchSlider.setValue(0.0);
        overviewWaveform.updateWaveform();

        coverArtImage = CoverArtGenerator::getCoverForTrack(cleanTitle, 256);
        jogWheel.setTrackInfo(cleanTitle, bpm, accent);

        repaint();
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
    repaint();
}

void DeckComponent::fileDragExit(const juce::StringArray&)
{
    isDragOver = false;
    repaint();
}

void DeckComponent::filesDropped(const juce::StringArray& files, int /*x*/, int /*y*/)
{
    isDragOver = false;
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
    repaint();
}

void DeckComponent::itemDragExit(const SourceDetails&)
{
    isDragOver = false;
    repaint();
}

void DeckComponent::itemDropped(const SourceDetails& dragSourceDetails)
{
    isDragOver = false;
    juce::String desc = dragSourceDetails.description.toString();
    if (desc.startsWith("yt:"))
    {
        juce::String videoId = desc.substring(3);
        YouTubeSearchResult res;
        res.id = videoId;
        res.title = "YouTube Video (" + videoId + ")";
        res.artist = "YouTube DJ";
        loadYouTubeVideo(res);
        repaint();
        return;
    }
    juce::File file(desc);
    if (file.existsAsFile())
    {
        loadAudioFile(file);
    }
    repaint();
}

void DeckComponent::loadYouTubeVideo(const YouTubeSearchResult& result)
{
    isVideoMode = true;
    isVideoPlaying = false;
    deck.stop();

    if (videoPlayer == nullptr)
    {
        videoPlayer = std::make_unique<YouTubeVideoComponent>(deckIndex);
        addAndMakeVisible(*videoPlayer);
    }

    videoPlayer->loadVideo(result.id, result.title);
    videoPlayer->setVisible(true);
    jogWheel.setVisible(false);

    auto displayTitle = result.title.isNotEmpty() ? result.title : ("YouTube Video (" + result.id + ")");
    titleLabel.setText(displayTitle, juce::dontSendNotification);
    artistLabel.setText(result.artist.isNotEmpty() ? result.artist : "YouTube DJ", juce::dontSendNotification);
    bpmLabel.setText("128.0", juce::dontSendNotification);
    pitchBpmLabel.setText("128.0", juce::dontSendNotification);
    timeRemainingLabel.setText(result.durationText.isNotEmpty() ? result.durationText : "--:--", juce::dontSendNotification);
    keyBadge.setText("YT", juce::dontSendNotification);

    resized();
    repaint();
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

void DeckComponent::updateLabels()
{
    double pos = deck.getPosition();
    double total = deck.getLengthInSeconds();
    double remaining = std::max(0.0, total - pos);

    char buf[32];
    formatTime(remaining, buf, sizeof(buf));
    timeRemainingLabel.setText("-" + juce::String(buf), juce::dontSendNotification);

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

    // Dynamic BPM and pitch reflection (supports Tempo Blend and Pitch Bend in real-time)
    double curBpm = deck.getBpm();
    bpmLabel.setText(juce::String(curBpm, 1), juce::dontSendNotification);
    pitchBpmLabel.setText(juce::String(curBpm, 1), juce::dontSendNotification);

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
    updateLabels();
    overviewWaveform.updateWaveform();
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

    // Visual Drag Over Indicator
    if (isDragOver)
    {
        g.setColour(accent.withAlpha(0.18f));
        g.fillRoundedRectangle(bounds.reduced(2.0f), 8.0f);

        g.setColour(accent);
        g.drawRoundedRectangle(bounds.reduced(2.0f), 8.0f, 2.5f);

        juce::Rectangle<float> banner(bounds.getCentreX() - 130.0f, bounds.getCentreY() - 32.0f, 260.0f, 64.0f);
        g.setColour(juce::Colour::fromRGB(12, 14, 18).withAlpha(0.94f));
        g.fillRoundedRectangle(banner, 8.0f);
        g.setColour(accent);
        g.drawRoundedRectangle(banner, 8.0f, 1.5f);

        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        g.drawText(juce::String("SOLTAR EN DECK ") + juce::String(deckIndex + 1),
                   banner.removeFromTop(36.0f), juce::Justification::centred, false);

        g.setColour(accent);
        g.setFont(juce::FontOptions(10.5f));
        g.drawText(juce::String::fromUTF8("Pista de Biblioteca o Archivo MP3/WAV"),
                   banner, juce::Justification::centred, false);
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
    auto rightMeta = headerArea.removeFromRight(150);
    bpmLabel.setBounds(rightMeta.removeFromRight(46));
    keyBadge.setBounds(rightMeta.removeFromRight(30).reduced(0, 10));
    timeRemainingLabel.setBounds(rightMeta);

    // Title and Artist fill the middle header space
    titleLabel.setBounds(headerArea.removeFromTop(22));
    artistLabel.setBounds(headerArea);

    area.removeFromTop(6);

    // 2. Overview Waveform (28px)
    overviewWaveform.setBounds(area.removeFromTop(28));
    area.removeFromTop(8);

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

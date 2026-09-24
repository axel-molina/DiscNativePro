#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "../audio/DeckPlayer.h"
#include "JogWheel.h"
#include "OverviewWaveform.h"
#include "DjLookAndFeel.h"
#include "CoverArtGenerator.h"
#include "DjButton.h"

class DeckComponent : public juce::Component,
                      public juce::FileDragAndDropTarget,
                      public juce::DragAndDropTarget,
                      public juce::Timer
{
public:
    DeckComponent(int deckNumber, DeckPlayer& player, juce::Colour accentColour);
    ~DeckComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    // File drag & drop from macOS Finder
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    // Internal app drag & drop from Library
    bool isInterestedInDragSource(const SourceDetails& dragSourceDetails) override;
    void itemDragEnter(const SourceDetails& dragSourceDetails) override;
    void itemDragExit(const SourceDetails& dragSourceDetails) override;
    void itemDropped(const SourceDetails& dragSourceDetails) override;

    void loadAudioFile(const juce::File& file);
    void loadYouTubeVideo(const struct YouTubeSearchResult& result);
    bool isVideoModeActive() const { return isVideoMode; }
    void setVideoVolume(float effectiveVolume);
    void playVideo();
    void pauseVideo();

private:
    void updateLabels();
    void formatTime(double seconds, char* buffer, size_t bufferSize);

    int deckIndex;
    DeckPlayer& deck;
    juce::Colour accent;
    bool isDragOver { false };
    bool isVideoMode { false };
    std::unique_ptr<class YouTubeVideoComponent> videoPlayer;

    // Header elements (Matching DiscPro DeckHeader.tsx)
    juce::Label titleLabel;
    juce::Label artistLabel;
    juce::Label timeRemainingLabel;
    juce::Label keyBadge;
    juce::Label bpmLabel;

    // Overview Waveform
    OverviewWaveform overviewWaveform;

    // Center Platter Turntable
    JogWheel jogWheel;

    // Pitch Section (Side edge)
    DjButton syncButton { "SYNC" };
    juce::Label pitchBpmLabel;
    juce::Label pitchDeltaLabel;
    juce::Slider pitchSlider;
    DjButton pitchBendDownButton;
    DjButton pitchBendUpButton;

    // Lower Transport Controls Bar (3 clean groups matching screenshot)
    DjButton playButton;
    DjButton cueButton;
    DjButton loopPrevButton;
    DjButton loopBeatsButton;
    DjButton loopNextButton;

    int currentLoopBeats { 4 };
    juce::Image coverArtImage;
    DjLookAndFeel djLookAndFeel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeckComponent)
};

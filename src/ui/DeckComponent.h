#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "../audio/DeckPlayer.h"
#include "JogWheel.h"
#include "OverviewWaveform.h"
#include "DjLookAndFeel.h"
#include "CoverArtGenerator.h"
#include "DjButton.h"
#include "../services/LanguageManager.h"
#include <array>

class AudioEngine;

class DeckComponent : public juce::Component,
                      public juce::FileDragAndDropTarget,
                      public juce::DragAndDropTarget,
                      public juce::Timer,
                      public LanguageManager::Listener
{
public:
    DeckComponent(int deckNumber, DeckPlayer& player, AudioEngine& engine, juce::Colour accentColour);
    ~DeckComponent() override;

    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;
    void languageChanged() override;

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

    // P6: origen explicito de la carga. AutoMix queda exento del Load Lock.
    enum class LoadOrigin { User, Automix };

    void loadAudioFile(const juce::File& file, LoadOrigin origin = LoadOrigin::User);
    void loadYouTubeVideo(const struct YouTubeSearchResult& result, LoadOrigin origin = LoadOrigin::User);
    bool isVideoModeActive() const { return isVideoMode; }
    bool isVideoPlayingActive() const { return isVideoPlaying; }
    void setVideoVolume(float effectiveVolume);
    void playVideo();
    void pauseVideo();
    void togglePlayPause();
    void triggerCue();
    bool isPlaying() const;
    double getVideoPosition() const { return videoPositionSeconds; }
    double getVideoDuration() const { return videoDurationSeconds; }
    void seekVideo(double seconds);

    double getOriginalBpm() const { return deck.getOriginalBpm(); }
    double getBpm() const { return deck.getBpm(); }
    juce::String getTrackTitle() const { return titleLabel.getText(); }
    juce::String getTrackArtist() const { return artistLabel.getText(); }
    juce::String getTrackKey() const { return keyBadge.getText(); }
    juce::String getVideoId() const { return loadedVideoId; }
    bool isLoaded() const { return deck.isLoaded() || isVideoMode; }

    static juce::String sanitizeTrackString(const juce::String& text);
    static juce::String stripTrackNumberPrefix(const juce::String& filename);
    static bool splitArtistTitle(const juce::String& in, juce::String& outArtist, juce::String& outTitle);

    std::function<void(const juce::File& file, double detectedBpm)> onFileLoaded;

private:
    // P6: guarda centralizada del Load Lock + carga real separada de la publica.
    bool shouldBlockLoad(LoadOrigin origin) const;
    void showLoadLockWarning(std::function<void(DeckComponent&)> forcedLoad);
    void loadAudioFileNow(const juce::File& file);
    void loadYouTubeVideoNow(const struct YouTubeSearchResult& result);

    void updateLabels();
    void updateKeyLockButton();
    void formatTime(double seconds, char* buffer, size_t bufferSize);
    // Resuelve título/autor reales cuando solo tenemos el ID (drag yt:ID).
    // No bloquea: carga inmediata + actualización async si el video sigue cargado.
    void resolveVideoMetadataAsync(const juce::String& videoId);

    int deckIndex;
    DeckPlayer& deck;
    AudioEngine& engine;
    juce::Colour accent;
    bool isDragOver { false };
    bool isVideoMode { false };
    bool isVideoPlaying { false };
    bool isExtractingAudio { false };
    float extractionProgress { 0.0f };
    juce::String extractionStatus;
    juce::String loadedVideoId;
    std::unique_ptr<class YouTubeVideoComponent> videoPlayer;
    // ponytail: reloj virtual para modo video — el iframe YT no reporta
    // posición (control unidireccional), así que se integra el tiempo en el
    // timer de UI. Puede derivar si la red buferiza; sin puente JS no hay
    // forma barata de resincronizar.
    double videoPositionSeconds { 0.0 };
    double videoDurationSeconds { 0.0 };
    juce::uint32 videoLastTickMs { 0 };

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
    DjButton keyLockButton { "KEY" };
    juce::Label pitchBpmLabel;
    juce::Label pitchDeltaLabel;
    juce::Slider pitchSlider;
    DjButton pitchBendDownButton;
    DjButton pitchBendUpButton;
    // P5: el BPM se detecta en segundo plano; caché para refrescar el jog wheel
    // una sola vez por pista (setTrackInfo regenera la portada).
    double jogWheelBpmShown { -1.0 };

    // Lower Transport Controls Bar (3 clean groups matching screenshot)
    DjButton playButton;
    DjButton cueButton;
    DjButton loopPrevButton;
    DjButton loopBeatsButton;
    DjButton loopNextButton;

    // Hot Cue row (4 pads per deck, session-RAM only)
    juce::Label hotCueRowLabel;
    std::array<DjButton, 4> hotCueButtons;
    std::array<int, 4> hotCueButtonShown { -1, -1, -1, -1 }; // cached (enabled,active) to avoid 60Hz repaint spam
    void refreshHotCueButtons();

    int currentLoopBeats { 4 };
    juce::Image coverArtImage;
    DjLookAndFeel djLookAndFeel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeckComponent)
};

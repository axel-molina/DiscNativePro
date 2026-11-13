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
                      public juce::Button::Listener,
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
    void buttonStateChanged(juce::Button*) override;
    void buttonClicked(juce::Button*) override {}
    void mouseDown(const juce::MouseEvent& e) override; // reset Key Shift al clicar su label

    // P15: acciones de performance (UI y MIDI comparten estas entradas)
    void beatJump(int direction);   // -1 atras, +1 adelante (usa el size selector)
    void startRoll();               // hold: loop roll momentaneo
    void releaseRoll();
    void toggleSlip();
    double getJumpBeats() const { return jumpSizes[static_cast<size_t>(jumpSizeIdx)]; }
    double getRollBeats() const  { return rollSizes[static_cast<size_t>(rollSizeIdx)]; }

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
    // P16: abre el editor de beat grid de este deck (MainComponent lo aloja).
    std::function<void(int deckIndex)> onEditGrid;

private:
    // P6: guarda centralizada del Load Lock + carga real separada de la publica.
    bool shouldBlockLoad(LoadOrigin origin) const;
    void showLoadLockWarning(std::function<void(DeckComponent&)> forcedLoad);
    void loadAudioFileNow(const juce::File& file, LoadOrigin origin);
    void loadYouTubeVideoNow(const struct YouTubeSearchResult& result, LoadOrigin origin);

    void updateLabels();
    void updateKeyLockButton();
    void updateKeyShiftDisplay();
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
    // P26: banner de error persistente (fallo de carga/descarga) hasta la
    // próxima carga — antes el error se escribía tras apagar el banner y
    // nadie lo veía.
    bool loadErrorVisible { false };
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
    DjButton keyShiftDownButton; // P20: Key Shift -1 st
    DjButton keyShiftUpButton;   // P20: Key Shift +1 st
    juce::Label keyShiftLabel;   // muestra "+N"/"0" (clic = reset)
    DjButton gridButton { "GRID" };
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
    void persistHotCues(); // P27: guarda los 4 hotcues en la caché de biblioteca

    int currentLoopBeats { 4 };

    // P15: fila de performance bajo los hot cues
    DjButton jumpBackButton;    // ‹ salto cuantizado atras
    DjButton jumpSizeButton;    // cicla 1/2→1→2→4→8→16 beats
    DjButton jumpFwdButton;     // › salto cuantizado adelante
    DjButton rollPadButton;     // ROLL (hold)
    DjButton rollSizeButton;    // cicla 1/16→1/8→1/4→1/2→1→2 beats
    DjButton slipToggleButton;  // SLIP on/off
    static constexpr double jumpSizes[] = { 0.5, 1.0, 2.0, 4.0, 8.0, 16.0 };
    static constexpr double rollSizes[] = { 1.0 / 16.0, 1.0 / 8.0, 0.25, 0.5, 1.0, 2.0 };
    int jumpSizeIdx { 1 }; // 1 beat
    int rollSizeIdx { 2 }; // 1/4 beat
    bool rollHeld { false };
    int rollPadShown { -1 }; // cache (looping) para no repintar a 60 Hz
    int slipShown { -1 };    // cache (slip on)
    juce::String jumpSizeLabel(int idx) const;
    juce::String rollSizeLabel(int idx) const;
    void refreshPerformanceButtons();

    // P22: stems (solo visibles con stems listos para la pista actual)
    DjButton stemsMasterButton;                  // STEMS on/off
    std::array<DjButton, 4> stemButtons;         // VOC / DRM / BAS / OTH
    int stemsMasterShown { -1 };                 // cache (0 off, 1 listo, 2 activo)
    std::array<int, 4> stemButtonShown { -1, -1, -1, -1 }; // cache (gain > 0.5)
    bool stemsRowShown { false };                // cache de visibilidad (evita resized() por tick)
    bool stems2ModeShown { false };              // P25: modo VOC/INST vs VOC/DRM/BAS/OTH
    int stemsPollTick { 0 };                     // P23f: throttle 1 s del chequeo de adopción de stems
    void refreshStemButtons();

    juce::Image coverArtImage;
    DjLookAndFeel djLookAndFeel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeckComponent)
};

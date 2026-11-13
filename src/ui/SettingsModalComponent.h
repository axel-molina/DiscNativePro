#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "../audio/AudioEngine.h"
#include "../midi/MidiManager.h"
#include "../services/AIService.h"
#include "../services/LanguageManager.h"
#include "../services/UpdateService.h"
#include "LucideIcons.h"
#include "DjButton.h"
#include "MidiMappingTableComponent.h"

class SettingsModalComponent : public juce::Component,
                               public LanguageManager::Listener,
                               public juce::ChangeListener,
                               public juce::Timer
{
public:
    SettingsModalComponent(AudioEngine& engine, MidiManager& midiMgr, int initialTab = 0);
    ~SettingsModalComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void languageChanged() override;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void timerCallback() override;
    void refreshLanguage();
    void setActiveTab(int tabIndex);

    std::function<void()> onClose;
    std::function<void()> onDownloadsFolderChanged;
    std::function<void(bool)> onUpdateAvailabilityChanged;

private:
    void setupGeneralTab();
    void setupDevicesTab();
    void setupSoundTab();
    void setupMidiTab();
    void setupStreamingTab();
    void setupTransmitTab();
    void setupAITab();
    void setupDonateTab();
    void setupDesignTab();
    void setupVersionTab();
    void checkUpdatesFromUI();
    void openColourPicker(int band); // 0 = graves, 1 = medios, 2 = agudos
    void refreshDesignSwatches();
    void refreshMidiDevices();
    void updateTabVisibility();
    void applySettings();
    void updateBroadcastRuntime();
    void refreshBroadcastStatus();
    void persistBroadcastConfig();
    void updateTransmitDestinationUI();
    void refreshFfmpegStatus();
    void refreshPodcastRecordings();
    void exportPodcastMp3();
    void updateRecordingFolderUI();
    void updateStemsPrecalcButton();
    void refreshAiModelOptions();

    AudioEngine& audioEngine;
    MidiManager& midi;
    int activeTab = 0; // 0: General, 1: Dispositivos, 2: Sonido, 3: MIDI, 4: Shortcuts, 5: Streaming, 6: IA, 7: Donar, 8: Diseño, 9: Versión

    // Tab buttons
    DjButton tabGeneralBtn;
    DjButton tabDevicesBtn;
    DjButton tabSoundBtn;
    DjButton tabMidiBtn;
    DjButton tabShortcutsBtn;
    DjButton tabStreamingBtn;
    DjButton tabAIBtn;
    DjButton tabDonateBtn;
    DjButton tabDesignBtn;
    DjButton tabVersionBtn;
    DjButton tabTransmitBtn;

    // --- Tab 0: General Controls ---
    juce::Label languageSectionLabel;
    juce::Label languageSelectLabel;
    juce::ComboBox languageCombo;

    juce::Label generalSectionDeck;
    juce::ToggleButton loadLockToggle;
    juce::ToggleButton autoplayOnLoadToggle;
    juce::Label jogModeLabel;
    juce::ComboBox jogModeCombo;
    juce::Label pitchRangeLabel;
    juce::ComboBox pitchRangeCombo;
    juce::ToggleButton autoCueToggle;
    juce::ToggleButton ytAutoplayToggle;

    juce::Label generalSectionDisplay;
    juce::Label waveformFpsLabel;
    juce::ComboBox waveformFpsCombo;

    juce::Label recSectionLabel;
    juce::Label recFolderDescLabel;

    // --- Tab 0: Stems (separación automática) — estado del binario demucs ---
    juce::Label stemsSectionLabel;
    juce::Label stemsStatusDescLabel;
    juce::Label stemsModeLabel;
    juce::ComboBox stemsModeCombo;
    DjButton stemsPrecalcBtn;
    juce::TextEditor recFolderEditor;
    DjButton recBrowseBtn;
    DjButton recOpenBtn;
    DjButton recResetBtn;

    // --- Tab 0: Sección Descargas de YouTube y Archivos Locales ---
    juce::Label downloadsSectionLabel;
    juce::ToggleButton saveDownloadsToggle;
    juce::Label downloadsFolderDescLabel;
    juce::TextEditor downloadsFolderEditor;
    DjButton downloadsBrowseBtn;
    DjButton downloadsOpenBtn;
    DjButton downloadsResetBtn;
    void updateDownloadsFolderUI();

    std::unique_ptr<juce::FileChooser> fileChooser;

    // --- Tab 1: Dispositivos Controls ---
    std::unique_ptr<juce::AudioDeviceSelectorComponent> audioDeviceSelector;
    juce::Label routingSectionLabel;
    juce::Label masterRoutingLabel;
    juce::ComboBox masterRoutingCombo;
    juce::Label cueRoutingLabel;
    juce::ComboBox cueRoutingCombo;
    juce::Label controllerDetectedBadge;

    // --- Tab 2: Sonido Controls ---
    juce::Label soundSectionEngine;
    juce::Label sampleRateInfoLabel;
    juce::Label bufferSizeLabel;
    juce::ComboBox bufferSizeCombo;

    juce::Label soundSectionMixer;
    juce::Label eqModeLabel;
    juce::ComboBox eqModeCombo;
    juce::Label crossfaderCurveLabel;
    juce::ComboBox crossfaderCurveCombo;

    juce::Label soundSectionPhones;
    juce::Label cueMixLabel;
    juce::Slider cueMixSlider;
    juce::Label cueVolumeLabel;
    juce::Slider cueVolumeSlider;

    juce::Label soundSectionMaster;
    juce::Label headroomLabel;
    juce::ComboBox headroomCombo;
    juce::ToggleButton masterLimiterToggle;
    juce::Label ceilingLabel;
    juce::Slider ceilingSlider;

    // --- Tab 2: Micrófono (Talkover) — control completo (P18d) ---
    juce::Label soundSectionMic;
    juce::Label micGainLabel;
    juce::Slider micGainSlider;
    juce::ToggleButton micEnableToggle;
    juce::Label micDuckLabel;
    juce::Slider micDuckSlider;
    juce::ToggleButton micDuckAutoToggle;

    // --- Tab 3: MIDI Controls ---
    juce::Label midiTitleLabel;
    juce::Label midiDescLabel;
    juce::Label midiDevicesHeader;
    juce::TextButton midiScanBtn { "Escanear Puertos" };
    juce::TextButton midiImportMixxxBtn { "Importar Mixxx" }; // P26/P29: presets de comunidad
    juce::TextButton midiImportVdjBtn { "Importar VDJ" };     // P29: definition + mapper VDJ
    juce::StringArray midiConnectedDevices;
    juce::Label midiMonitorHeader;
    juce::Label midiMonitorText;
    juce::Label midiMappingHeader;
    std::unique_ptr<MidiMappingTableComponent> midiMappingTable;
    void importMixxxPreset();
    void importVdjMapping();

    // --- Tab 4: Shortcuts Controls ---
    // (Rendered directly in paint/custom cards)

    // --- Tab 5: Streaming / YouTube Controls ---
    juce::Label streamingSectionTitle;
    juce::Label streamingDescLabel;
    juce::Label apiKeyLabel;
    juce::TextEditor apiKeyEditor;
    juce::Label apiStatusBadge;
    juce::Label apiInfoCard;
    DjButton clearApiKeyBtn;

    // --- Tab 5: Broadcast (servidor propio Icecast/Shoutcast) ---
    juce::Label broadcastSectionTitle;
    juce::Label broadcastDescLabel;
    juce::ToggleButton broadcastEnabledToggle;
    juce::Label broadcastHostLabel;
    juce::TextEditor broadcastHostEditor;
    juce::Label broadcastPortLabel;
    juce::TextEditor broadcastPortEditor;
    juce::Label broadcastMountLabel;
    juce::TextEditor broadcastMountEditor;
    juce::Label broadcastPasswordLabel;
    juce::TextEditor broadcastPasswordEditor;
    juce::Label broadcastNameLabel;
    juce::TextEditor broadcastNameEditor;
    juce::Label broadcastBitrateLabel;
    juce::ComboBox broadcastBitrateCombo;
    juce::Label broadcastStatusBadge;

    // --- Tab 10: Transmitir (radio + RTMP + podcasts) ---
    juce::Label transmitTitleLabel;
    juce::Label transmitDescLabel;

    juce::Label broadcastDestLabel;
    juce::ComboBox broadcastDestCombo;

    juce::Label rtmpUrlLabel;
    juce::TextEditor rtmpUrlEditor;
    juce::Label rtmpKeyLabel;
    juce::TextEditor rtmpKeyEditor;
    juce::Label rtmpBitrateLabel;
    juce::ComboBox rtmpBitrateCombo;
    DjButton installFfmpegBtn;
    juce::Label ffmpegStatusBadge;

    juce::Label podcastSectionLabel;
    juce::Label podcastDescLabel;
    juce::ComboBox podcastRecCombo;
    DjButton podcastRefreshBtn;
    juce::Label podcastBitrateLabel;
    juce::ComboBox podcastBitrateCombo;
    DjButton podcastExportBtn;
    juce::Label podcastStatusBadge;
    bool isExportingPodcast { false };

    // --- Tab 6: Inteligencia Artificial (AI Match) Controls ---
    juce::Label aiSectionTitle;
    juce::Label aiDescLabel;
    juce::ToggleButton aiEnabledToggle;
    juce::Label aiProviderLabel;
    juce::ComboBox aiProviderCombo;
    juce::Label aiModelLabel;
    juce::ComboBox aiModelCombo;
    juce::Label aiApiKeyLabel;
    juce::TextEditor aiApiKeyEditor;
    juce::Label aiStatusBadge;
    juce::Label aiInfoCard;
    DjButton clearAiApiKeyBtn;

    // --- Tab 7: Donaciones (Cafecito) Controls ---
    juce::Label donateTitleLabel;
    juce::Label donateDescLabel;
    DjButton donateBtn;
    juce::Label donateHintLabel;
    juce::Label donateInfoCard;

    // --- Tab 8: Diseño (colores de waveforms) Controls ---
    juce::Label designTitleLabel;
    juce::Label designDescLabel;
    juce::Label deckThemeLabel;
    DjButton deckTheme1Btn;
    DjButton deckTheme2Btn;
    juce::Label waveLowLabel;
    juce::Label waveMidLabel;
    juce::Label waveHighLabel;
    DjButton waveLowSwatch;
    DjButton waveMidSwatch;
    DjButton waveHighSwatch;
    juce::Label designPresetLabel;
    DjButton presetRgbBtn;
    DjButton presetNeonBtn;
    DjButton presetSilverBtn;
    juce::Component::SafePointer<juce::ColourSelector> activeColourSelector;
    int activeColourBand { -1 };
    int designDeckIndex { 0 };  // bandeja editada en el tab Diseño (0 = Deck 1, 1 = Deck 2)
    int activeColourDeck { 0 }; // bandeja destino congelada al abrir el selector de color

    // --- Tab 9: Versión & Actualizaciones ---
    juce::Label versionSectionTitle;
    juce::Label versionAppTitle;
    juce::Label versionStatusLabel;
    DjButton checkUpdatesBtn;

    juce::Label versionChangeTitle;
    juce::Label versionHelpIcon;
    juce::ComboBox versionSelectCombo;
    DjButton installVersionBtn;
    juce::Label downloadStatusLabel;
    double downloadProgressValue { -1.0 };
    juce::ProgressBar downloadProgressBar { downloadProgressValue };
    bool isDownloading { false };
    bool isCheckingUpdates { false };
    std::vector<ReleaseItem> availableReleases;

    // --- Footer Controls ---
    juce::TextButton undoButton { "Deshacer" };
    juce::TextButton applyButton { "Aplicar" };
    juce::Label appInfoLabel;

    // Cuerpo con scroll vertical (solo hace scroll si la tab desborda)
    juce::Viewport bodyViewport;
    juce::Component bodyContent;

    // Sidebar vertical de pestañas (icono + etiqueta), con scroll si no caben
    juce::Viewport sideViewport;
    juce::Component sideContent;

    // Dynamic card layout boxes computed in resized()
    juce::Rectangle<int> midiDeviceListBox;
    juce::Rectangle<int> streamingInfoCardBox;
    juce::Rectangle<int> aiInfoCardBox;
    juce::Rectangle<int> donateCardBox;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsModalComponent)
};

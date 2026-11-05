#include "SettingsModalComponent.h"
#include "../services/YouTubeService.h"
#include "../services/WaveformTheme.h"
#include "../services/SecretStore.h"
#include "../services/FfmpegLocator.h"
#include "../audio/Mp3Exporter.h"

namespace
{
    // Ajustes de broadcast (strings/numeros) en el mismo settings.json del resto de la app.
    // La contraseña NUNCA va al JSON: se guarda en el llavero via SecretStore.
    constexpr const char* broadcastHostProperty = "broadcastHost";
    constexpr const char* broadcastPortProperty = "broadcastPort";
    constexpr const char* broadcastMountProperty = "broadcastMount";
    constexpr const char* broadcastNameProperty = "broadcastName";
    constexpr const char* broadcastBitrateProperty = "broadcastBitrate";
    constexpr const char* broadcastPasswordAccount = "broadcast.password";

    juce::String readStringSetting(const char* property, const juce::String& defaultValue = {})
    {
        auto file = AudioEngine::getSettingsFile();
        if (!file.existsAsFile())
            return defaultValue;

        auto json = juce::JSON::parse(file);
        if (!json.isObject() || !json.hasProperty(property))
            return defaultValue;

        return json[property].toString();
    }

    void writeStringSetting(const char* property, const juce::String& value)
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

    int bitrateFromComboId(int comboId)
    {
        switch (comboId)
        {
            case 1: return 128;
            case 3: return 256;
            case 4: return 320;
            case 2:
            default: return 192;
        }
    }

    int comboIdFromBitrate(int kbps)
    {
        switch (kbps)
        {
            case 128: return 1;
            case 256: return 3;
            case 320: return 4;
            case 192:
            default: return 2;
        }
    }

    // --- Destinos de transmision ---
    // ids del combo: 1 radio propia (Icecast), 2 YouTube, 3 Facebook, 4 Twitch, 5 personalizado
    constexpr const char* broadcastDestProperty = "broadcastDest";
    constexpr const char* rtmpBitrateProperty = "rtmpBitrate";
    constexpr const char* rtmpCustomUrlProperty = "rtmpCustomUrl";
    constexpr const char* podcastBitrateProperty = "podcastBitrate";

    bool isRtmpDestination(int comboId) { return comboId >= 2; }

    juce::String defaultRtmpUrlForDestination(int comboId)
    {
        switch (comboId)
        {
            case 2: return "rtmp://a.rtmp.youtube.com/live2/";
            case 3: return "rtmps://live-api-s.facebook.com:443/rtmp/";
            case 4: return "rtmp://live.twitch.tv/app/";
            default: return {};
        }
    }

    juce::String rtmpKeyAccountForDestination(int comboId)
    {
        switch (comboId)
        {
            case 2: return "rtmp.key.youtube";
            case 3: return "rtmp.key.facebook";
            case 4: return "rtmp.key.twitch";
            default: return "rtmp.key.custom";
        }
    }

    juce::String composeRtmpIngestUrl(const juce::String& baseUrl, const juce::String& streamKey)
    {
        auto base = baseUrl.trim();
        auto key = streamKey.trim();
        if (key.isEmpty())
            return base;
        if (base.endsWithChar('/'))
            return base + key;
        return base + "/" + key;
    }
} // namespace

SettingsModalComponent::SettingsModalComponent(AudioEngine& engine, MidiManager& midiMgr, int initialTab)
    : audioEngine(engine), midi(midiMgr), activeTab(initialTab)
{
    setSize(740, 540);

    // 0. Sidebar vertical de pestañas (icono + etiqueta, scroll si no caben)
    sideViewport.setViewedComponent(&sideContent, false);
    sideViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(sideViewport);

    // 1. Tab Buttons (10 tabs) — botones apilados dentro del sidebar
    auto setupTabBtn = [this](DjButton& btn, LucideIcons::IconType icon, const juce::String& text, int tabIdx) {
        btn.setIcon(icon, 14.0f);
        btn.setText(text);
        btn.setFontSize(9.5f);
        btn.setStackedLayout(true);
        btn.setCornerRadius(6.0f);
        btn.onClick = [this, tabIdx]() {
            activeTab = tabIdx;
            updateTabVisibility();
            bodyViewport.setViewPosition({ 0, 0 });
            resized();
            repaint();
        };
        sideContent.addAndMakeVisible(btn);
    };

    setupTabBtn(tabGeneralBtn, LucideIcons::IconType::Settings, "General", 0);
    setupTabBtn(tabDevicesBtn, LucideIcons::IconType::Headphones, "Dispositivos", 1);
    setupTabBtn(tabSoundBtn, LucideIcons::IconType::Volume2, "Sonido", 2);
    setupTabBtn(tabMidiBtn, LucideIcons::IconType::Zap, "MIDI", 3);
    setupTabBtn(tabShortcutsBtn, LucideIcons::IconType::Keyboard, "Shortcuts", 4);
    setupTabBtn(tabStreamingBtn, LucideIcons::IconType::Tv, "Streaming", 5);
    setupTabBtn(tabAIBtn, LucideIcons::IconType::Sparkles, "IA Match", 6);
    setupTabBtn(tabDonateBtn, LucideIcons::IconType::Coffee, "Donar", 7);
    setupTabBtn(tabDesignBtn, LucideIcons::IconType::Sliders, "Diseño", 8);
    setupTabBtn(tabVersionBtn, LucideIcons::IconType::Tag, "Versión", 9);
    setupTabBtn(tabTransmitBtn, LucideIcons::IconType::Radio, "Transmitir", 10);

    // 3. Setup Tab Contents
    bodyViewport.setViewedComponent(&bodyContent, false);
    bodyViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(bodyViewport);

    setupGeneralTab();
    setupDevicesTab();
    setupSoundTab();
    setupMidiTab();
    setupStreamingTab();
    setupTransmitTab();
    setupAITab();
    setupDonateTab();
    setupDesignTab();
    setupVersionTab();

    // 4. Footer controls
    undoButton.setButtonText("Deshacer");
    undoButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(37, 41, 56));
    undoButton.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(220, 225, 235));
    undoButton.onClick = [this]() { if (onClose) onClose(); };
    addAndMakeVisible(undoButton);

    applyButton.setButtonText("Aplicar");
    applyButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(37, 99, 235));
    applyButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    applyButton.onClick = [this]() { applySettings(); };
    addAndMakeVisible(applyButton);

    LanguageManager::getInstance().addListener(this);
    refreshLanguage();
    updateTabVisibility();
    startTimerHz(2); // refresco del estado del broadcast (solo pinta si la tab esta activa)
}

SettingsModalComponent::~SettingsModalComponent()
{
    stopTimer();
    LanguageManager::getInstance().removeListener(this);
    midi.onMidiActivity = nullptr;
}

void SettingsModalComponent::setActiveTab(int tabIndex)
{
    activeTab = tabIndex;
    updateTabVisibility();
    bodyViewport.setViewPosition({ 0, 0 });
    resized();
    repaint();
}

void SettingsModalComponent::languageChanged()
{
    refreshLanguage();
}


void SettingsModalComponent::setupGeneralTab()
{
    // Section 0: Idioma / Language
    languageSectionLabel.setText(LanguageManager::trUtf8("Idioma de la Interfaz", "Interface Language"), juce::dontSendNotification);
    languageSectionLabel.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    languageSectionLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(0, 180, 216));
    bodyContent.addChildComponent(languageSectionLabel);

    languageSelectLabel.setText(LanguageManager::trUtf8("Idioma:", "Language:"), juce::dontSendNotification);
    languageSelectLabel.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    languageSelectLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(160, 165, 180));
    bodyContent.addChildComponent(languageSelectLabel);

    languageCombo.addItem(LanguageManager::trUtf8("Automático (Sistema)", "Automatic (System)"), 1);
    languageCombo.addItem(juce::String::fromUTF8("Español (Spanish)"), 2);
    languageCombo.addItem(juce::String::fromUTF8("English (Inglés)"), 3);

    auto setting = LanguageManager::getInstance().getLanguageSetting();
    if (setting == LanguageSetting::System)
        languageCombo.setSelectedId(1, juce::dontSendNotification);
    else if (setting == LanguageSetting::Spanish)
        languageCombo.setSelectedId(2, juce::dontSendNotification);
    else
        languageCombo.setSelectedId(3, juce::dontSendNotification);

    languageCombo.onChange = [this]() {
        int id = languageCombo.getSelectedId();
        if (id == 1)
            LanguageManager::getInstance().setLanguageSetting(LanguageSetting::System);
        else if (id == 2)
            LanguageManager::getInstance().setLanguageSetting(LanguageSetting::Spanish);
        else if (id == 3)
            LanguageManager::getInstance().setLanguageSetting(LanguageSetting::English);
    };
    bodyContent.addChildComponent(languageCombo);

    // Section 1: Comportamiento de Decks
    generalSectionDeck.setText(juce::String::fromUTF8("Comportamiento de Decks"), juce::dontSendNotification);
    generalSectionDeck.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    generalSectionDeck.setColour(juce::Label::textColourId, juce::Colour::fromRGB(0, 180, 216));
    bodyContent.addChildComponent(generalSectionDeck);

    loadLockToggle.setButtonText(juce::String::fromUTF8("Protección de carga (Load Lock): Bloquear carga si el deck reproduce en vivo"));
    loadLockToggle.setToggleState(audioEngine.isLoadLockEnabled(), juce::dontSendNotification);
    loadLockToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::white.withAlpha(0.85f));
    loadLockToggle.onClick = [this]() {
        audioEngine.setLoadLockEnabled(loadLockToggle.getToggleState());
    };
    bodyContent.addChildComponent(loadLockToggle);

    autoplayOnLoadToggle.setButtonText(juce::String::fromUTF8("Reproducir al cargar (Autoplay en bandeja)"));
    autoplayOnLoadToggle.setToggleState(audioEngine.isAutoplayOnLoadEnabled(), juce::dontSendNotification);
    autoplayOnLoadToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::white.withAlpha(0.85f));
    autoplayOnLoadToggle.onClick = [this]() {
        audioEngine.setAutoplayOnLoadEnabled(autoplayOnLoadToggle.getToggleState());
    };
    bodyContent.addChildComponent(autoplayOnLoadToggle);

    jogModeLabel.setText(juce::String::fromUTF8("Modo Jog Wheel:"), juce::dontSendNotification);
    jogModeLabel.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    jogModeLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(160, 165, 180));
    bodyContent.addChildComponent(jogModeLabel);

    jogModeCombo.addItem(juce::String::fromUTF8("Vinilo (Scratch & Pitch Bend al borde)"), 1);
    jogModeCombo.addItem(juce::String::fromUTF8("CDJ (Pitch Bend continuo)"), 2);
    jogModeCombo.setSelectedId(audioEngine.getDeck(0).getJogMode() == 1 ? 2 : 1, juce::dontSendNotification);
    jogModeCombo.onChange = [this]() {
        int mode = (jogModeCombo.getSelectedId() == 2) ? 1 : 0;
        audioEngine.getDeck(0).setJogMode(mode);
        audioEngine.getDeck(1).setJogMode(mode);
    };
    bodyContent.addChildComponent(jogModeCombo);

    pitchRangeLabel.setText(juce::String::fromUTF8("Rango de Pitch por defecto:"), juce::dontSendNotification);
    pitchRangeLabel.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    pitchRangeLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(160, 165, 180));
    bodyContent.addChildComponent(pitchRangeLabel);

    pitchRangeCombo.addItem(juce::String::fromUTF8("±8% (Precisión estándar Club)"), 1);
    pitchRangeCombo.addItem(juce::String::fromUTF8("±16% (Rango extendido)"), 2);
    pitchRangeCombo.addItem(juce::String::fromUTF8("±50% (Extremo / Transiciones abiertas)"), 3);

    float curRange = audioEngine.getPitchRange();
    if (curRange > 30.0f) pitchRangeCombo.setSelectedId(3, juce::dontSendNotification);
    else if (curRange > 12.0f) pitchRangeCombo.setSelectedId(2, juce::dontSendNotification);
    else pitchRangeCombo.setSelectedId(1, juce::dontSendNotification);

    pitchRangeCombo.onChange = [this]() {
        int id = pitchRangeCombo.getSelectedId();
        float r = 8.0f;
        if (id == 2) r = 16.0f;
        else if (id == 3) r = 50.0f;
        audioEngine.setPitchRange(r);
    };
    bodyContent.addChildComponent(pitchRangeCombo);

    autoCueToggle.setButtonText(juce::String::fromUTF8("Auto-CUE: Posicionar cabezal en el primer beat al cargar pista"));
    autoCueToggle.setToggleState(true, juce::dontSendNotification);
    autoCueToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::white.withAlpha(0.85f));
    bodyContent.addChildComponent(autoCueToggle);

    ytAutoplayToggle.setToggleState(YouTubeService::isVideoAutoplayEnabled(), juce::dontSendNotification);
    ytAutoplayToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::white.withAlpha(0.85f));
    bodyContent.addChildComponent(ytAutoplayToggle);

    // Section 2: Visualización y Rendimiento
    generalSectionDisplay.setText(juce::String::fromUTF8("Visualización y Rendimiento"), juce::dontSendNotification);
    generalSectionDisplay.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    generalSectionDisplay.setColour(juce::Label::textColourId, juce::Colour::fromRGB(0, 180, 216));
    bodyContent.addChildComponent(generalSectionDisplay);

    waveformFpsLabel.setText(juce::String::fromUTF8("Tasa de refresco de formas de onda:"), juce::dontSendNotification);
    waveformFpsLabel.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    waveformFpsLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(160, 165, 180));
    bodyContent.addChildComponent(waveformFpsLabel);

    waveformFpsCombo.addItem(juce::String::fromUTF8("60 FPS (Fluidez ultra suave - Recomendado)"), 1);
    waveformFpsCombo.addItem(juce::String::fromUTF8("30 FPS (Ahorro de batería y recursos)"), 2);
    waveformFpsCombo.setSelectedId(1, juce::dontSendNotification);
    bodyContent.addChildComponent(waveformFpsCombo);

    // Section 3: Grabación de Sesiones
    recSectionLabel.setText(juce::String::fromUTF8("Grabación de Sesiones"), juce::dontSendNotification);
    recSectionLabel.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    recSectionLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(0, 180, 216));
    bodyContent.addChildComponent(recSectionLabel);

    recFolderDescLabel.setText(juce::String::fromUTF8("Carpeta de destino para archivos grabados (.wav 24-bit):"), juce::dontSendNotification);
    recFolderDescLabel.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    recFolderDescLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(160, 165, 180));
    bodyContent.addChildComponent(recFolderDescLabel);

    recFolderEditor.setReadOnly(true);
    recFolderEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour::fromRGB(17, 19, 26));
    recFolderEditor.setColour(juce::TextEditor::textColourId, juce::Colour::fromRGB(220, 225, 235));
    recFolderEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour::fromRGB(38, 43, 58));
    recFolderEditor.setFont(juce::FontOptions("Menlo", 10.5f, juce::Font::plain));
    bodyContent.addChildComponent(recFolderEditor);

    recBrowseBtn.setIcon(LucideIcons::IconType::Folder, 12.0f);
    recBrowseBtn.setText(juce::String::fromUTF8("Examinar..."));
    recBrowseBtn.setFontSize(10.5f);
    recBrowseBtn.setCornerRadius(5.0f);
    recBrowseBtn.setCustomColours(juce::Colour::fromRGB(32, 35, 48),
                                 juce::Colour::fromRGB(200, 205, 220),
                                 juce::Colour::fromRGB(0, 180, 216),
                                 juce::Colour::fromRGB(45, 50, 68));
    recBrowseBtn.onClick = [this]() {
        fileChooser = std::make_unique<juce::FileChooser>(
            LanguageManager::trUtf8("Seleccionar Carpeta para Grabaciones", "Select Recording Folder"),
            audioEngine.getRecordingDirectory(),
            "");
        auto folderFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories;
        fileChooser->launchAsync(folderFlags, [this](const juce::FileChooser& fc) {
            auto result = fc.getResult();
            if (result.isDirectory())
            {
                audioEngine.setRecordingDirectory(result);
                updateRecordingFolderUI();
            }
        });
    };
    bodyContent.addChildComponent(recBrowseBtn);

    recOpenBtn.setIcon(LucideIcons::IconType::Folder, 12.0f);
    recOpenBtn.setText(juce::String::fromUTF8("Abrir Carpeta"));
    recOpenBtn.setFontSize(10.5f);
    recOpenBtn.setCornerRadius(5.0f);
    recOpenBtn.setCustomColours(juce::Colour::fromRGB(32, 35, 48),
                               juce::Colour::fromRGB(200, 205, 220),
                               juce::Colour::fromRGB(74, 222, 128),
                               juce::Colour::fromRGB(45, 50, 68));
    recOpenBtn.onClick = [this]() {
        auto dir = audioEngine.getRecordingDirectory();
        if (!dir.exists())
            dir.createDirectory();
        dir.revealToUser();
    };
    bodyContent.addChildComponent(recOpenBtn);

    recResetBtn.setText(juce::String::fromUTF8("Por defecto"));
    recResetBtn.setFontSize(10.5f);
    recResetBtn.setCornerRadius(5.0f);
    recResetBtn.setCustomColours(juce::Colour::fromRGB(26, 28, 36),
                                juce::Colour::fromRGB(140, 145, 160),
                                juce::Colour::fromRGB(200, 205, 220),
                                juce::Colour::fromRGB(40, 44, 58));
    recResetBtn.onClick = [this]() {
        audioEngine.setRecordingDirectory(audioEngine.getDefaultRecordingDirectory());
        updateRecordingFolderUI();
    };
    bodyContent.addChildComponent(recResetBtn);

    updateRecordingFolderUI();

    // Section 4: Descargas de YouTube y Archivos Locales
    downloadsSectionLabel.setText(juce::String::fromUTF8("Descargas de YouTube y Archivos Locales"), juce::dontSendNotification);
    downloadsSectionLabel.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    downloadsSectionLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(0, 180, 216));
    bodyContent.addChildComponent(downloadsSectionLabel);

    saveDownloadsToggle.setButtonText(juce::String::fromUTF8("Guardar canciones descargadas en disco local (persistente)"));
    saveDownloadsToggle.setToggleState(YouTubeService::isSaveDownloadsEnabled(), juce::dontSendNotification);
    saveDownloadsToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::white.withAlpha(0.85f));
    saveDownloadsToggle.onClick = [this]() {
        bool enabled = saveDownloadsToggle.getToggleState();
        YouTubeService::setSaveDownloadsEnabled(enabled);
        updateDownloadsFolderUI();
    };
    bodyContent.addChildComponent(saveDownloadsToggle);

    downloadsFolderDescLabel.setText(juce::String::fromUTF8("Carpeta de destino para canciones descargadas (.m4a AAC):"), juce::dontSendNotification);
    downloadsFolderDescLabel.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    downloadsFolderDescLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(160, 165, 180));
    bodyContent.addChildComponent(downloadsFolderDescLabel);

    downloadsFolderEditor.setReadOnly(true);
    downloadsFolderEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour::fromRGB(17, 19, 26));
    downloadsFolderEditor.setColour(juce::TextEditor::textColourId, juce::Colour::fromRGB(220, 225, 235));
    downloadsFolderEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour::fromRGB(38, 43, 58));
    downloadsFolderEditor.setFont(juce::FontOptions("Menlo", 10.5f, juce::Font::plain));
    bodyContent.addChildComponent(downloadsFolderEditor);

    downloadsBrowseBtn.setIcon(LucideIcons::IconType::Folder, 12.0f);
    downloadsBrowseBtn.setText(juce::String::fromUTF8("Examinar..."));
    downloadsBrowseBtn.setFontSize(10.5f);
    downloadsBrowseBtn.setCornerRadius(5.0f);
    downloadsBrowseBtn.setCustomColours(juce::Colour::fromRGB(32, 35, 48),
                                      juce::Colour::fromRGB(200, 205, 220),
                                      juce::Colour::fromRGB(0, 180, 216),
                                      juce::Colour::fromRGB(45, 50, 68));
    downloadsBrowseBtn.onClick = [this]() {
        fileChooser = std::make_unique<juce::FileChooser>(
            LanguageManager::trUtf8("Seleccionar Carpeta para Descargas", "Select Downloads Folder"),
            YouTubeService::getDownloadsFolder(),
            "");
        auto folderFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories;
        fileChooser->launchAsync(folderFlags, [this](const juce::FileChooser& fc) {
            auto result = fc.getResult();
            if (result.isDirectory())
            {
                YouTubeService::setDownloadsFolder(result);
                updateDownloadsFolderUI();
                if (onDownloadsFolderChanged)
                    onDownloadsFolderChanged();
            }
        });
    };
    bodyContent.addChildComponent(downloadsBrowseBtn);

    downloadsOpenBtn.setIcon(LucideIcons::IconType::Folder, 12.0f);
    downloadsOpenBtn.setText(juce::String::fromUTF8("Abrir Carpeta"));
    downloadsOpenBtn.setFontSize(10.5f);
    downloadsOpenBtn.setCornerRadius(5.0f);
    downloadsOpenBtn.setCustomColours(juce::Colour::fromRGB(32, 35, 48),
                                    juce::Colour::fromRGB(200, 205, 220),
                                    juce::Colour::fromRGB(74, 222, 128),
                                    juce::Colour::fromRGB(45, 50, 68));
    downloadsOpenBtn.onClick = []() {
        auto dir = YouTubeService::getDownloadsFolder();
        if (!dir.exists())
            dir.createDirectory();
        dir.revealToUser();
    };
    bodyContent.addChildComponent(downloadsOpenBtn);

    downloadsResetBtn.setText(juce::String::fromUTF8("Por defecto"));
    downloadsResetBtn.setFontSize(10.5f);
    downloadsResetBtn.setCornerRadius(5.0f);
    downloadsResetBtn.setCustomColours(juce::Colour::fromRGB(26, 28, 36),
                                     juce::Colour::fromRGB(140, 145, 160),
                                     juce::Colour::fromRGB(200, 205, 220),
                                     juce::Colour::fromRGB(40, 44, 58));
    downloadsResetBtn.onClick = [this]() {
        YouTubeService::setDownloadsFolder(YouTubeService::getDefaultDownloadsFolder());
        updateDownloadsFolderUI();
        if (onDownloadsFolderChanged)
            onDownloadsFolderChanged();
    };
    bodyContent.addChildComponent(downloadsResetBtn);

    updateDownloadsFolderUI();
}

void SettingsModalComponent::updateRecordingFolderUI()
{
    auto dir = audioEngine.getRecordingDirectory();
    recFolderEditor.setText(dir.getFullPathName(), juce::dontSendNotification);
    recFolderEditor.setCaretPosition(recFolderEditor.getText().length());
}

void SettingsModalComponent::updateDownloadsFolderUI()
{
    auto dir = YouTubeService::getDownloadsFolder();
    downloadsFolderEditor.setText(dir.getFullPathName(), juce::dontSendNotification);
    downloadsFolderEditor.setCaretPosition(downloadsFolderEditor.getText().length());

    bool isEnabled = saveDownloadsToggle.getToggleState();
    downloadsFolderEditor.setEnabled(isEnabled);
    downloadsBrowseBtn.setEnabled(isEnabled);
    downloadsOpenBtn.setEnabled(isEnabled);
    downloadsResetBtn.setEnabled(isEnabled);
    downloadsFolderDescLabel.setAlpha(isEnabled ? 1.0f : 0.5f);
}

void SettingsModalComponent::refreshLanguage()
{
    tabGeneralBtn.setText(LanguageManager::trUtf8("General", "General"));
    tabDevicesBtn.setText(LanguageManager::trUtf8("Dispositivos", "Devices"));
    tabSoundBtn.setText(LanguageManager::trUtf8("Sonido", "Sound"));
    tabMidiBtn.setText(LanguageManager::trUtf8("MIDI", "MIDI"));
    tabShortcutsBtn.setText(LanguageManager::trUtf8("Shortcuts", "Shortcuts"));
    tabStreamingBtn.setText(LanguageManager::trUtf8("Streaming", "Streaming"));
    tabAIBtn.setText(LanguageManager::trUtf8("IA Match", "AI Match"));
    tabDonateBtn.setText(LanguageManager::trUtf8("Donar", "Donate"));
    tabDesignBtn.setText(LanguageManager::trUtf8("Diseño", "Design"));

    undoButton.setButtonText(LanguageManager::trUtf8("Deshacer", "Undo"));
    applyButton.setButtonText(LanguageManager::trUtf8("Aplicar", "Apply"));

    // Tab 0: General
    languageSectionLabel.setText(LanguageManager::trUtf8("Idioma de la Interfaz", "Interface Language"), juce::dontSendNotification);
    languageSelectLabel.setText(LanguageManager::trUtf8("Idioma:", "Language:"), juce::dontSendNotification);
    int curLangId = languageCombo.getSelectedId();
    languageCombo.clear(juce::dontSendNotification);
    languageCombo.addItem(LanguageManager::trUtf8("Automático (Sistema)", "Automatic (System)"), 1);
    languageCombo.addItem(juce::String::fromUTF8("Español (Spanish)"), 2);
    languageCombo.addItem(juce::String::fromUTF8("English (Inglés)"), 3);
    languageCombo.setSelectedId(curLangId > 0 ? curLangId : 1, juce::dontSendNotification);

    generalSectionDeck.setText(LanguageManager::trUtf8("Comportamiento de Decks", "Deck Behavior"), juce::dontSendNotification);
    loadLockToggle.setButtonText(LanguageManager::trUtf8("Protección de carga (Load Lock): Bloquear carga si el deck reproduce en vivo", "Load Lock: Prevent loading tracks into playing decks"));
    autoplayOnLoadToggle.setButtonText(LanguageManager::trUtf8("Reproducir al cargar (Autoplay en bandeja)", "Autoplay on load"));
    jogModeLabel.setText(LanguageManager::trUtf8("Modo Jog Wheel:", "Jog Wheel Mode:"), juce::dontSendNotification);
    int curJog = jogModeCombo.getSelectedId();
    jogModeCombo.clear(juce::dontSendNotification);
    jogModeCombo.addItem(LanguageManager::trUtf8("Vinilo (Scratch & Pitch Bend al borde)", "Vinyl (Scratch & Edge Pitch Bend)"), 1);
    jogModeCombo.addItem(LanguageManager::trUtf8("CDJ (Pitch Bend continuo)", "CDJ (Continuous Pitch Bend)"), 2);
    jogModeCombo.setSelectedId(curJog > 0 ? curJog : 1, juce::dontSendNotification);

    pitchRangeLabel.setText(LanguageManager::trUtf8("Rango de Pitch por defecto:", "Default Pitch Range:"), juce::dontSendNotification);
    int curPitch = pitchRangeCombo.getSelectedId();
    pitchRangeCombo.clear(juce::dontSendNotification);
    pitchRangeCombo.addItem(LanguageManager::trUtf8("±8% (Precisión estándar Club)", "±8% (Standard Club Precision)"), 1);
    pitchRangeCombo.addItem(LanguageManager::trUtf8("±16% (Rango extendido)", "±16% (Extended Range)"), 2);
    pitchRangeCombo.addItem(LanguageManager::trUtf8("±50% (Extremo / Transiciones abiertas)", "±50% (Extreme / Open Transitions)"), 3);
    pitchRangeCombo.setSelectedId(curPitch > 0 ? curPitch : 1, juce::dontSendNotification);

    autoCueToggle.setButtonText(LanguageManager::trUtf8("Auto-CUE: Posicionar cabezal en el primer beat al cargar pista", "Auto-CUE: Position playhead at first beat on track load"));
    ytAutoplayToggle.setButtonText(LanguageManager::trUtf8("Autoplay YouTube: Reproducir automáticamente el video al cargarlo en bandeja",
                                                            "YouTube autoplay: Start playing the video automatically when loaded on a deck"));

    generalSectionDisplay.setText(LanguageManager::trUtf8("Visualización y Rendimiento", "Display & Performance"), juce::dontSendNotification);
    waveformFpsLabel.setText(LanguageManager::trUtf8("Tasa de refresco de formas de onda:", "Waveform Refresh Rate:"), juce::dontSendNotification);
    int curFps = waveformFpsCombo.getSelectedId();
    waveformFpsCombo.clear(juce::dontSendNotification);
    waveformFpsCombo.addItem(LanguageManager::trUtf8("60 FPS (Fluidez ultra suave - Recomendado)", "60 FPS (Ultra Smooth - Recommended)"), 1);
    waveformFpsCombo.addItem(LanguageManager::trUtf8("30 FPS (Ahorro de batería y recursos)", "30 FPS (Battery & Resource Saver)"), 2);
    waveformFpsCombo.setSelectedId(curFps > 0 ? curFps : 1, juce::dontSendNotification);

    recSectionLabel.setText(LanguageManager::trUtf8("Grabación de Sesiones", "Session Recording"), juce::dontSendNotification);
    recFolderDescLabel.setText(LanguageManager::trUtf8("Carpeta de destino para archivos grabados (.wav 24-bit):", "Destination folder for recorded files (.wav 24-bit):"), juce::dontSendNotification);
    recBrowseBtn.setText(LanguageManager::trUtf8("Examinar...", "Browse..."));
    recOpenBtn.setText(LanguageManager::trUtf8("Abrir Carpeta", "Open Folder"));
    recResetBtn.setText(LanguageManager::trUtf8("Por defecto", "Default"));

    downloadsSectionLabel.setText(LanguageManager::trUtf8("Descargas de YouTube y Archivos Locales", "YouTube Downloads & Local Files"), juce::dontSendNotification);
    saveDownloadsToggle.setButtonText(LanguageManager::trUtf8("Guardar canciones descargadas en disco local (persistente)", "Save downloaded songs to local storage (persistent)"));
    downloadsFolderDescLabel.setText(LanguageManager::trUtf8("Carpeta de destino para canciones descargadas (.m4a AAC):", "Destination folder for downloaded tracks (.m4a AAC):"), juce::dontSendNotification);
    downloadsBrowseBtn.setText(LanguageManager::trUtf8("Examinar...", "Browse..."));
    downloadsOpenBtn.setText(LanguageManager::trUtf8("Abrir Carpeta", "Open Folder"));
    downloadsResetBtn.setText(LanguageManager::trUtf8("Por defecto", "Default"));

    // La versión nunca se hardcodea: se lee de la fuente runtime única
    // (UpdateService::getCurrentVersion -> JUCEApplication::getApplicationVersion,
    // alimentada por APP_VERSION en CMake). Un build 1.0.0 muestra 1.0.0; 1.3.0, 1.3.0.
    const juce::String appVersion = UpdateService::getCurrentVersion();
    appInfoLabel.setText(LanguageManager::tr("DiscNativePro DJ Workstation v" + appVersion + " • Motor C++20 nativo con JUCE 8, CoreAudio y SIMD DSP.",
                                             "DiscNativePro DJ Workstation v" + appVersion + " • Native C++20 engine with JUCE 8, CoreAudio & SIMD DSP."), juce::dontSendNotification);

    // Tab 1: Dispositivos / Devices
    routingSectionLabel.setText(LanguageManager::trUtf8("Enrutamiento de Salidas de Audio (Pioneer DDJ / Multi-Canal)", "Audio Output Routing (Pioneer DDJ / Multi-Channel)"), juce::dontSendNotification);
    masterRoutingLabel.setText(LanguageManager::trUtf8("Salida Master (Altavoces / Club):", "Master Output (Speakers / Club):"), juce::dontSendNotification);
    int curMR = masterRoutingCombo.getSelectedId();
    masterRoutingCombo.clear(juce::dontSendNotification);
    masterRoutingCombo.addItem(LanguageManager::trUtf8("Canales 1 & 2 (Salida RCA Master DDJ-SB2)", "Channels 1 & 2 (DDJ-SB2 Master RCA)"), 1);
    masterRoutingCombo.addItem(LanguageManager::trUtf8("Canales 3 & 4 (Salida Frontal DDJ-SB2)", "Channels 3 & 4 (DDJ-SB2 Front Output)"), 2);
    masterRoutingCombo.setSelectedId(curMR > 0 ? curMR : 1, juce::dontSendNotification);

    cueRoutingLabel.setText(LanguageManager::trUtf8("Salida Auriculares (Preescucha CUE):", "Headphones Output (CUE Cueing):"), juce::dontSendNotification);
    int curCR = cueRoutingCombo.getSelectedId();
    cueRoutingCombo.clear(juce::dontSendNotification);
    cueRoutingCombo.addItem(LanguageManager::trUtf8("Canales 3 & 4 (Jack Frontal Auriculares DDJ-SB2)", "Channels 3 & 4 (DDJ-SB2 Front Headphone Jack)"), 1);
    cueRoutingCombo.addItem(LanguageManager::trUtf8("Canales 1 & 2 (Salida Compartida Master)", "Channels 1 & 2 (Shared Master Output)"), 2);
    cueRoutingCombo.setSelectedId(curCR > 0 ? curCR : 1, juce::dontSendNotification);

    // Tab 2: Sonido / Sound
    soundSectionEngine.setText(LanguageManager::trUtf8("Motor de Audio CoreAudio", "CoreAudio Sound Engine"), juce::dontSendNotification);
    int sr = (int)audioEngine.getSampleRate();
    if (sr <= 0) sr = 44100;
    sampleRateInfoLabel.setText(LanguageManager::trUtf8("Frecuencia de Muestreo (Sample Rate): ", "Sample Rate: ") + juce::String(sr) + " Hz (CoreAudio)", juce::dontSendNotification);
    bufferSizeLabel.setText(LanguageManager::trUtf8("Tamaño de Buffer de Audio / Latencia:", "Audio Buffer Size / Latency:"), juce::dontSendNotification);
    int curBuf = bufferSizeCombo.getSelectedId();
    bufferSizeCombo.clear(juce::dontSendNotification);
    bufferSizeCombo.addItem(LanguageManager::trUtf8("128 muestras (2.9 ms • Ultra baja latencia interactiva)", "128 samples (2.9 ms • Ultra-low interactive latency)"), 1);
    bufferSizeCombo.addItem(LanguageManager::trUtf8("256 muestras (5.8 ms • Baja latencia recomendada)", "256 samples (5.8 ms • Recommended low latency)"), 2);
    bufferSizeCombo.addItem(LanguageManager::trUtf8("512 muestras (11.6 ms • Equilibrado estándar)", "512 samples (11.6 ms • Standard balanced)"), 3);
    bufferSizeCombo.addItem(LanguageManager::trUtf8("1024 muestras (23.2 ms • Máxima estabilidad)", "1024 samples (23.2 ms • Maximum stability)"), 4);
    bufferSizeCombo.setSelectedId(curBuf > 0 ? curBuf : 2, juce::dontSendNotification);

    soundSectionMixer.setText(LanguageManager::trUtf8("Ecualizador & Crossfader", "Equalizer & Crossfader"), juce::dontSendNotification);
    eqModeLabel.setText(LanguageManager::trUtf8("Curva de Ecualizador (EQ Mode):", "Equalizer Curve (EQ Mode):"), juce::dontSendNotification);
    int curEq = eqModeCombo.getSelectedId();
    eqModeCombo.clear(juce::dontSendNotification);
    eqModeCombo.addItem(LanguageManager::trUtf8("Isolator (-∞ dB Full Kill • Corte total por banda)", "Isolator (-∞ dB Full Kill • Total band cut)"), 1);
    eqModeCombo.addItem(LanguageManager::trUtf8("Clásico (-24 dB a +6 dB • Curva analógica suave)", "Classic (-24 dB to +6 dB • Smooth analog curve)"), 2);
    eqModeCombo.setSelectedId(curEq > 0 ? curEq : 1, juce::dontSendNotification);

    crossfaderCurveLabel.setText(LanguageManager::trUtf8("Curva de Crossfader por defecto:", "Default Crossfader Curve:"), juce::dontSendNotification);
    int curXf = crossfaderCurveCombo.getSelectedId();
    crossfaderCurveCombo.clear(juce::dontSendNotification);
    crossfaderCurveCombo.addItem(LanguageManager::trUtf8("Suave (Smooth • Transiciones fluidas)", "Smooth (Fluid transitions)"), 1);
    crossfaderCurveCombo.addItem(LanguageManager::trUtf8("Lineal (Linear • Mezcla progresiva)", "Linear (Progressive mix)"), 2);
    crossfaderCurveCombo.addItem(LanguageManager::trUtf8("Corte Rápido (Scratch • Corte instantáneo)", "Fast Cut (Scratch • Instant cut)"), 3);
    crossfaderCurveCombo.setSelectedId(curXf > 0 ? curXf : 1, juce::dontSendNotification);

    soundSectionMaster.setText(LanguageManager::trUtf8("Margen Dinámico y Limitador Master", "Dynamic Headroom & Master Limiter"), juce::dontSendNotification);
    headroomLabel.setText(LanguageManager::trUtf8("Margen Dinámico Master (Headroom):", "Master Headroom:"), juce::dontSendNotification);
    int curHr = headroomCombo.getSelectedId();
    headroomCombo.clear(juce::dontSendNotification);
    headroomCombo.addItem(LanguageManager::trUtf8("-3 dB (Recomendado para clubs y grabación)", "-3 dB (Recommended for clubs and recording)"), 1);
    headroomCombo.addItem(LanguageManager::trUtf8("-6 dB (Margen amplio para mezclas intensas)", "-6 dB (Wide headroom for intense mixing)"), 2);
    headroomCombo.addItem(LanguageManager::trUtf8("0 dB (Sin margen / límite exacto)", "0 dB (No headroom / exact limit)"), 3);
    headroomCombo.setSelectedId(curHr > 0 ? curHr : 1, juce::dontSendNotification);

    masterLimiterToggle.setButtonText(LanguageManager::trUtf8("Limitador Master de Seguridad (Evita distorsión digital y clipping)", "Safety Master Limiter (Prevents digital distortion and clipping)"));

    soundSectionPhones.setText(LanguageManager::trUtf8("Monitoreo y Auriculares (Preescucha CUE)", "Monitoring & Headphones (CUE Cueing)"), juce::dontSendNotification);
    cueMixLabel.setText(LanguageManager::trUtf8("Mezcla Auriculares (CUE a la izquierda / MASTER a la derecha):", "Headphone Mix (CUE left / MASTER right):"), juce::dontSendNotification);
    cueVolumeLabel.setText(LanguageManager::trUtf8("Volumen Master de Auriculares:", "Headphone Master Volume:"), juce::dontSendNotification);

    // Tab 3: MIDI
    midiTitleLabel.setText(LanguageManager::trUtf8("Controladores USB MIDI", "USB MIDI Controllers"), juce::dontSendNotification);
    midiDescLabel.setText(LanguageManager::trUtf8("DiscNativePro detecta automáticamente cualquier controladora DJ física conectada por USB (Pioneer DDJ, Hercules, Numark, Native Instruments). Mapea jog wheels, perillas de EQ, faders y pads.", "DiscNativePro automatically detects any USB physical DJ controller (Pioneer DDJ, Hercules, Numark, Native Instruments). Maps jog wheels, EQ knobs, faders and pads."), juce::dontSendNotification);
    midiScanBtn.setButtonText(LanguageManager::trUtf8("Escanear Puertos", "Scan Ports"));
    midiMonitorHeader.setText(LanguageManager::trUtf8("MONITOR DE SEÑALES MIDI EN VIVO", "LIVE MIDI SIGNAL MONITOR"), juce::dontSendNotification);
    midiMonitorText.setText(LanguageManager::trUtf8("Esperando movimiento de fader, jog o pad...", "Waiting for fader, jog or pad movement..."), juce::dontSendNotification);
    midiMappingHeader.setText(LanguageManager::trUtf8("MAPEO PERSONALIZADO (MIDI LEARN)", "CUSTOM MAPPING (MIDI LEARN)"), juce::dontSendNotification);
    if (midiMappingTable != nullptr)
        midiMappingTable->refreshLanguage();

    // Tab 5: Streaming
    streamingSectionTitle.setText(LanguageManager::trUtf8("Integración de YouTube Streaming", "YouTube Streaming Integration"), juce::dontSendNotification);
    streamingDescLabel.setText(LanguageManager::trUtf8("DiscNativePro permite buscar, mezclar y reproducir videos de YouTube en vivo directamente en las bandejas.\nPor defecto se utiliza Scraping Público para buscar sin necesidad de API Key.", "DiscNativePro lets you search, mix and play live YouTube videos directly on the decks.\nBy default, Public Scraping is used without needing an API Key."), juce::dontSendNotification);
    apiKeyLabel.setText(LanguageManager::trUtf8("YouTube Data API v3 Key (Opcional - Para búsqueda oficial sin restricciones):", "YouTube Data API v3 Key (Optional - For official search without limits):"), juce::dontSendNotification);
    apiKeyEditor.setTextToShowWhenEmpty(LanguageManager::trUtf8("Pega tu clave AIzaSy...", "Paste your AIzaSy key..."), juce::Colour::fromRGB(100, 108, 125));
    clearApiKeyBtn.setText(LanguageManager::trUtf8("Borrar Key", "Clear Key"));

    // Tab 10: Transmitir (radio + RTMP + podcasts)
    transmitTitleLabel.setText(LanguageManager::trUtf8("Transmitir — Emisiones en vivo y podcasts",
                                                      "Transmit — Live streams and podcasts"), juce::dontSendNotification);
    transmitDescLabel.setText(LanguageManager::trUtf8(
        "Emite la mezcla master en vivo a tu radio propia o a tus redes (YouTube, Facebook, Twitch). Los oyentes se conectan a la plataforma, no a tu app.",
        "Streams the master mix live to your own radio server or to your networks (YouTube, Facebook, Twitch). Listeners connect to the platform, not to your app."), juce::dontSendNotification);
    broadcastDestLabel.setText(LanguageManager::trUtf8("Destino de la emisión:", "Destination:"), juce::dontSendNotification);
    broadcastSectionTitle.setText(LanguageManager::trUtf8("Servidor de radio (Icecast/Shoutcast)",
                                                          "Radio server (Icecast/Shoutcast)"), juce::dontSendNotification);
    broadcastDescLabel.setText(LanguageManager::trUtf8("Datos de tu servidor propio de streaming.",
                                                       "Details of your own streaming server."), juce::dontSendNotification);
    broadcastEnabledToggle.setButtonText(LanguageManager::trUtf8("Emitir mezcla en vivo", "Broadcast mix live"));
    broadcastHostLabel.setText(LanguageManager::trUtf8("Servidor (host o IP):", "Server (host or IP):"), juce::dontSendNotification);
    broadcastPortLabel.setText(LanguageManager::trUtf8("Puerto:", "Port:"), juce::dontSendNotification);
    broadcastPasswordLabel.setText(LanguageManager::trUtf8("Contraseña:", "Password:"), juce::dontSendNotification);
    broadcastNameLabel.setText(LanguageManager::trUtf8("Nombre de la emisión:", "Station name:"), juce::dontSendNotification);
    broadcastBitrateLabel.setText(LanguageManager::trUtf8("Bitrate MP3:", "MP3 bitrate:"), juce::dontSendNotification);
    rtmpUrlLabel.setText(LanguageManager::trUtf8("URL de ingesta RTMP:", "RTMP ingest URL:"), juce::dontSendNotification);
    rtmpKeyLabel.setText(LanguageManager::trUtf8("Stream key:", "Stream key:"), juce::dontSendNotification);
    rtmpBitrateLabel.setText(LanguageManager::trUtf8("Bitrate AAC:", "AAC bitrate:"), juce::dontSendNotification);
    installFfmpegBtn.setText(LanguageManager::trUtf8("Instalar ffmpeg", "Install ffmpeg"));
    podcastSectionLabel.setText(LanguageManager::trUtf8("Podcast (exportar grabación a MP3)",
                                                        "Podcast (export recording to MP3)"), juce::dontSendNotification);
    podcastDescLabel.setText(LanguageManager::trUtf8("Convierte una grabación de sesión (WAV) a MP3 listo para publicar.",
                                                    "Converts a session recording (WAV) into a publish-ready MP3."), juce::dontSendNotification);
    podcastRecCombo.setTextWhenNothingSelected(LanguageManager::trUtf8("(sin grabaciones)", "(no recordings)"));
    podcastRefreshBtn.setText(LanguageManager::trUtf8("Refrescar", "Refresh"));
    podcastBitrateLabel.setText(LanguageManager::trUtf8("Bitrate de exportación:", "Export bitrate:"), juce::dontSendNotification);
    podcastExportBtn.setText(LanguageManager::trUtf8("Exportar a MP3", "Export to MP3"));
    refreshFfmpegStatus();
    apiInfoCard.setText(LanguageManager::trUtf8("Modo Video en Bandejas:\n• Al soltar un video de YouTube en el Deck 1 o Deck 2, el Jog Wheel se transforma en reproductor de video de alta definición (WebKit).\n• Los faders de volumen y el Crossfader central mezclan fluidamente el audio y video entre bandejas.\n• Iniciar sesión en YouTube en Safari comparte tu sesión con WKWebView para reproducción Premium sin publicidad.", "Video Mode in Decks:\n• Dropping a YouTube video onto Deck 1 or 2 turns the Jog Wheel into an HD video player (WebKit).\n• Volume faders and Crossfader smoothly mix audio and video between decks.\n• Logging in to YouTube in Safari shares your session with WKWebView for ad-free Premium playback."), juce::dontSendNotification);

    // Tab 6: IA
    aiSectionTitle.setText(LanguageManager::trUtf8("Inteligencia Artificial (AI Smart Match)", "Artificial Intelligence (AI Smart Match)"), juce::dontSendNotification);
    aiDescLabel.setText(LanguageManager::trUtf8("Analiza la pista en reproducción (local o YouTube) y sugiere mezclas armónicas por género, tempo y Camelot Key.\nElige tu proveedor preferido e ingresa tu clave API personal.", "Analyzes the currently playing track (local or YouTube) and suggests harmonic transitions by genre, tempo and Camelot Key.\nChoose your provider and enter your personal API key."), juce::dontSendNotification);
    aiEnabledToggle.setButtonText(LanguageManager::trUtf8("Habilitar Recomendaciones Inteligentes de IA", "Enable AI Smart Recommendations"));
    aiProviderLabel.setText(LanguageManager::trUtf8("Proveedor de Inteligencia Artificial:", "AI Provider:"), juce::dontSendNotification);
    aiModelLabel.setText(LanguageManager::trUtf8("Modelo (editable: elige o pega uno nuevo):", "Model (editable: pick or paste a new one):"), juce::dontSendNotification);
    aiApiKeyLabel.setText(LanguageManager::trUtf8("Clave API (API Key) del Proveedor Seleccionado:", "API Key for Selected Provider:"), juce::dontSendNotification);
    aiApiKeyEditor.setTextToShowWhenEmpty(LanguageManager::trUtf8("Pega tu API Key...", "Paste your API Key..."), juce::Colour::fromRGB(100, 108, 125));
    clearAiApiKeyBtn.setText(LanguageManager::trUtf8("Borrar Key", "Clear Key"));
    aiInfoCard.setText(LanguageManager::trUtf8("Obtén tu API Key gratuita:\n• Google Gemini: aistudio.google.com (15 req/min sin costo)\n• OpenAI: platform.openai.com\n• Anthropic: console.anthropic.com\n• DeepSeek: platform.deepseek.com\n• OpenRouter: openrouter.ai (una key, 200+ modelos)", "Get your free API Key:\n• Google Gemini: aistudio.google.com (15 req/min free)\n• OpenAI: platform.openai.com\n• Anthropic: console.anthropic.com\n• DeepSeek: platform.deepseek.com\n• OpenRouter: openrouter.ai (one key, 200+ models)"), juce::dontSendNotification);

    // Tab 7: Donar
    donateTitleLabel.setText(LanguageManager::trUtf8("Apoya DiscNativePro", "Support DiscNativePro"), juce::dontSendNotification);    donateDescLabel.setText(LanguageManager::trUtf8("¿Estás disfrutando de DiscNativePro? Puedes apoyar el desarrollo y las futuras funcionalidades regalándome un café.", "Enjoying DiscNativePro? You can support development and upcoming features by buying me a coffee."), juce::dontSendNotification);
    donateBtn.setText(LanguageManager::trUtf8("Donar un café en Cafecito", "Buy me a coffee on Cafecito"));
    donateHintLabel.setText(LanguageManager::trUtf8("Se abrirá tu navegador. Aporte voluntario, ¡gracias por apoyar el proyecto!", "Your browser will open. Voluntary contribution — thanks for supporting the project!"), juce::dontSendNotification);
    donateInfoCard.setText(LanguageManager::trUtf8("Tu aporte ayuda a:\n• Mantener el desarrollo activo y corregir errores.\n• Financiar hosting, licencias y pruebas en equipos reales.\n• Priorizar las funcionalidades que más pide la comunidad.", "Your contribution helps to:\n• Keep active development and bug fixes going.\n• Cover hosting, licenses and testing on real hardware.\n• Prioritize the features the community requests most."), juce::dontSendNotification);

    // Tab 8: Diseño
    designTitleLabel.setText(LanguageManager::trUtf8("Personalización de Waveforms", "Waveform Personalization"), juce::dontSendNotification);
    designDescLabel.setText(LanguageManager::trUtf8("Elige los colores de Graves, Medios y Agudos. Se aplican en vivo al wave de la bandeja seleccionada y se guardan automáticamente.", "Choose the Low, Mid and High colours. They apply live to the selected deck's waveform and save automatically."), juce::dontSendNotification);
    deckThemeLabel.setText(LanguageManager::trUtf8("Bandeja:", "Deck:"), juce::dontSendNotification);
    deckTheme1Btn.setText(LanguageManager::trUtf8("Bandeja 1", "Deck 1"));
    deckTheme2Btn.setText(LanguageManager::trUtf8("Bandeja 2", "Deck 2"));
    waveLowLabel.setText(LanguageManager::trUtf8("Graves (Low):", "Lows (Low):"), juce::dontSendNotification);
    waveMidLabel.setText(LanguageManager::trUtf8("Medios (Mid):", "Mids (Mid):"), juce::dontSendNotification);
    waveHighLabel.setText(LanguageManager::trUtf8("Agudos (High):", "Highs (High):"), juce::dontSendNotification);
    designPresetLabel.setText(LanguageManager::trUtf8("Esquemas predefinidos:", "Preset schemes:"), juce::dontSendNotification);
    presetRgbBtn.setText(LanguageManager::trUtf8("RGB Multicolor", "RGB Multicolor"));
    presetNeonBtn.setText(LanguageManager::trUtf8("Azul Neón", "Neon Blue"));
    presetSilverBtn.setText(LanguageManager::trUtf8("Plata Mono", "Mono Silver"));
    refreshDesignSwatches();

    // Tab 9: Versión / Version
    tabVersionBtn.setText(LanguageManager::trUtf8("Versión", "Version"));
    tabTransmitBtn.setText(LanguageManager::trUtf8("Transmitir", "Transmit"));
    versionSectionTitle.setText(LanguageManager::trUtf8("Versión", "Version"), juce::dontSendNotification);
    versionChangeTitle.setText(LanguageManager::trUtf8("Cambiar de versión", "Change version"), juce::dontSendNotification);
    checkUpdatesBtn.setText(LanguageManager::trUtf8("Buscar actualizaciones", "Check for updates"));
    installVersionBtn.setText(LanguageManager::trUtf8("Instalar", "Install"));
    versionHelpIcon.setTooltip(LanguageManager::trUtf8(
        "Selecciona cualquier versión oficial disponible en GitHub Releases para instalarla o cambiar de versión.",
        "Select any official version available in GitHub Releases to install or switch versions."));

    updateTabVisibility();
    repaint();
}

void SettingsModalComponent::setupDevicesTab()
{
    // Embedded AudioDeviceSelectorComponent with up to 4 output channels
    audioDeviceSelector = std::make_unique<juce::AudioDeviceSelectorComponent>(
        audioEngine.getDeviceManager(), 0, 0, 2, 4, false, false, true, false);
    bodyContent.addChildComponent(*audioDeviceSelector);

    // Section: Enrutamiento de Canales de Audio
    routingSectionLabel.setText(juce::String::fromUTF8("Enrutamiento de Salidas de Audio (Pioneer DDJ / Multi-Canal)"), juce::dontSendNotification);
    routingSectionLabel.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    routingSectionLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(0, 180, 216));
    bodyContent.addChildComponent(routingSectionLabel);

    // Master Routing
    masterRoutingLabel.setText(juce::String::fromUTF8("Salida Master (Altavoces / Club):"), juce::dontSendNotification);
    masterRoutingLabel.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    masterRoutingLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(160, 165, 180));
    bodyContent.addChildComponent(masterRoutingLabel);

    masterRoutingCombo.addItem(juce::String::fromUTF8("Canales 1 & 2 (Salida RCA Master DDJ-SB2)"), 1);
    masterRoutingCombo.addItem(juce::String::fromUTF8("Canales 3 & 4 (Salida Frontal DDJ-SB2)"), 2);
    masterRoutingCombo.setSelectedId(audioEngine.getMasterChannelPair() == 0 ? 1 : 2, juce::dontSendNotification);
    masterRoutingCombo.onChange = [this]() {
        audioEngine.setMasterChannelPair(masterRoutingCombo.getSelectedId() == 1 ? 0 : 1);
    };
    bodyContent.addChildComponent(masterRoutingCombo);

    // CUE Routing
    cueRoutingLabel.setText(juce::String::fromUTF8("Salida Auriculares (Preescucha CUE):"), juce::dontSendNotification);
    cueRoutingLabel.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    cueRoutingLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(160, 165, 180));
    bodyContent.addChildComponent(cueRoutingLabel);

    cueRoutingCombo.addItem(juce::String::fromUTF8("Canales 3 & 4 (Jack Frontal Auriculares DDJ-SB2)"), 1);
    cueRoutingCombo.addItem(juce::String::fromUTF8("Canales 1 & 2 (Salida Compartida Master)"), 2);
    cueRoutingCombo.setSelectedId(audioEngine.getCueChannelPair() == 1 ? 1 : 2, juce::dontSendNotification);
    cueRoutingCombo.onChange = [this]() {
        audioEngine.setCueChannelPair(cueRoutingCombo.getSelectedId() == 1 ? 1 : 0);
    };
    bodyContent.addChildComponent(cueRoutingCombo);

    // Status Badge
    controllerDetectedBadge.setText(juce::String::fromUTF8("● Controladora DJ 4 Canales Detectada (Master: Ch 1-2 • Auriculares: Ch 3-4)"), juce::dontSendNotification);
    controllerDetectedBadge.setColour(juce::Label::textColourId, juce::Colour::fromRGB(74, 222, 128));
    controllerDetectedBadge.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    bodyContent.addChildComponent(controllerDetectedBadge);
}

void SettingsModalComponent::setupSoundTab()
{
    // Section 1: Motor de Audio
    soundSectionEngine.setText(juce::String::fromUTF8("Motor de Audio CoreAudio"), juce::dontSendNotification);
    soundSectionEngine.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    soundSectionEngine.setColour(juce::Label::textColourId, juce::Colour::fromRGB(0, 180, 216));
    bodyContent.addChildComponent(soundSectionEngine);

    int sr = (int)audioEngine.getSampleRate();
    if (sr <= 0) sr = 44100;
    sampleRateInfoLabel.setText(juce::String::fromUTF8("Frecuencia de Muestreo (Sample Rate): ") + juce::String(sr) + " Hz (CoreAudio)",
                                juce::dontSendNotification);
    sampleRateInfoLabel.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    sampleRateInfoLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(180, 185, 200));
    bodyContent.addChildComponent(sampleRateInfoLabel);

    bufferSizeLabel.setText(juce::String::fromUTF8("Tamaño de Buffer de Audio / Latencia:"), juce::dontSendNotification);
    bufferSizeLabel.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    bufferSizeLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(160, 165, 180));
    bodyContent.addChildComponent(bufferSizeLabel);

    bufferSizeCombo.addItem(juce::String::fromUTF8("128 muestras (2.9 ms • Ultra baja latencia interactiva)"), 1);
    bufferSizeCombo.addItem(juce::String::fromUTF8("256 muestras (5.8 ms • Baja latencia recomendada)"), 2);
    bufferSizeCombo.addItem(juce::String::fromUTF8("512 muestras (11.6 ms • Equilibrado estándar)"), 3);
    bufferSizeCombo.addItem(juce::String::fromUTF8("1024 muestras (23.2 ms • Máxima estabilidad)"), 4);

    int curBlock = audioEngine.getBlockSize();
    if (curBlock <= 128) bufferSizeCombo.setSelectedId(1, juce::dontSendNotification);
    else if (curBlock <= 256) bufferSizeCombo.setSelectedId(2, juce::dontSendNotification);
    else if (curBlock <= 512) bufferSizeCombo.setSelectedId(3, juce::dontSendNotification);
    else bufferSizeCombo.setSelectedId(4, juce::dontSendNotification);

    bodyContent.addChildComponent(bufferSizeCombo);

    // Section 2: Mezclador & Curvas
    soundSectionMixer.setText(juce::String::fromUTF8("Ecualizador & Crossfader"), juce::dontSendNotification);
    soundSectionMixer.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    soundSectionMixer.setColour(juce::Label::textColourId, juce::Colour::fromRGB(0, 180, 216));
    bodyContent.addChildComponent(soundSectionMixer);

    eqModeLabel.setText(juce::String::fromUTF8("Curva de Ecualizador (EQ Mode):"), juce::dontSendNotification);
    eqModeLabel.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    eqModeLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(160, 165, 180));
    bodyContent.addChildComponent(eqModeLabel);

    eqModeCombo.addItem(juce::String::fromUTF8("Isolator (-∞ dB Full Kill • Corte total por banda)"), 1);
    eqModeCombo.addItem(juce::String::fromUTF8("Clásico (-24 dB a +6 dB • Curva analógica suave)"), 2);
    eqModeCombo.setSelectedId(1, juce::dontSendNotification);
    bodyContent.addChildComponent(eqModeCombo);

    crossfaderCurveLabel.setText(juce::String::fromUTF8("Curva de Crossfader por defecto:"), juce::dontSendNotification);
    crossfaderCurveLabel.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    crossfaderCurveLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(160, 165, 180));
    bodyContent.addChildComponent(crossfaderCurveLabel);

    crossfaderCurveCombo.addItem(juce::String::fromUTF8("Suave (Smooth • Transiciones fluidas)"), 1);
    crossfaderCurveCombo.addItem(juce::String::fromUTF8("Lineal (Linear • Mezcla progresiva)"), 2);
    crossfaderCurveCombo.addItem(juce::String::fromUTF8("Corte Rápido (Scratch • Corte instantáneo)"), 3);
    crossfaderCurveCombo.setSelectedId(1, juce::dontSendNotification);
    bodyContent.addChildComponent(crossfaderCurveCombo);

    // Section 3: Margen Dinámico & Limitador
    soundSectionMaster.setText(juce::String::fromUTF8("Margen Dinámico y Limitador Master"), juce::dontSendNotification);
    soundSectionMaster.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    soundSectionMaster.setColour(juce::Label::textColourId, juce::Colour::fromRGB(0, 180, 216));
    bodyContent.addChildComponent(soundSectionMaster);

    headroomLabel.setText(juce::String::fromUTF8("Margen Dinámico Master (Headroom):"), juce::dontSendNotification);
    headroomLabel.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    headroomLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(160, 165, 180));
    bodyContent.addChildComponent(headroomLabel);

    headroomCombo.addItem(juce::String::fromUTF8("-3 dB (Recomendado para clubs y grabación)"), 1);
    headroomCombo.addItem(juce::String::fromUTF8("-6 dB (Margen amplio para mezclas intensas)"), 2);
    headroomCombo.addItem(juce::String::fromUTF8("0 dB (Sin margen / límite exacto)"), 3);
    headroomCombo.setSelectedId(1, juce::dontSendNotification);
    bodyContent.addChildComponent(headroomCombo);

    masterLimiterToggle.setButtonText(juce::String::fromUTF8("Limitador Master de Seguridad (Evita distorsión digital y clipping)"));
    masterLimiterToggle.setToggleState(true, juce::dontSendNotification);
    masterLimiterToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::white.withAlpha(0.85f));
    bodyContent.addChildComponent(masterLimiterToggle);

    // Section 4: Monitoreo & Auriculares (CUE)
    soundSectionPhones.setText(juce::String::fromUTF8("Monitoreo y Auriculares (Preescucha CUE)"), juce::dontSendNotification);
    soundSectionPhones.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    soundSectionPhones.setColour(juce::Label::textColourId, juce::Colour::fromRGB(0, 180, 216));
    bodyContent.addChildComponent(soundSectionPhones);

    cueMixLabel.setText(juce::String::fromUTF8("Mezcla Auriculares (CUE a la izquierda / MASTER a la derecha):"), juce::dontSendNotification);
    cueMixLabel.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    cueMixLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(160, 165, 180));
    bodyContent.addChildComponent(cueMixLabel);

    cueMixSlider.setRange(0.0, 1.0, 0.01);
    cueMixSlider.setValue(audioEngine.getMixer().getCueMix(), juce::dontSendNotification);
    cueMixSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    cueMixSlider.onValueChange = [this]() {
        audioEngine.getMixer().setCueMix((float)cueMixSlider.getValue());
    };
    bodyContent.addChildComponent(cueMixSlider);

    cueVolumeLabel.setText(juce::String::fromUTF8("Volumen Master de Auriculares:"), juce::dontSendNotification);
    cueVolumeLabel.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    cueVolumeLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(160, 165, 180));
    bodyContent.addChildComponent(cueVolumeLabel);

    cueVolumeSlider.setRange(0.0, 1.5, 0.01);
    cueVolumeSlider.setValue(audioEngine.getMixer().getPhonesVolume(), juce::dontSendNotification);
    cueVolumeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    cueVolumeSlider.onValueChange = [this]() {
        audioEngine.getMixer().setPhonesVolume((float)cueVolumeSlider.getValue());
    };
    bodyContent.addChildComponent(cueVolumeSlider);
}

void SettingsModalComponent::setupMidiTab()
{
    midiTitleLabel.setText(juce::String::fromUTF8("Controladores USB MIDI"), juce::dontSendNotification);
    midiTitleLabel.setFont(juce::FontOptions(12.5f, juce::Font::bold));
    midiTitleLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(46, 196, 182));
    bodyContent.addChildComponent(midiTitleLabel);

    midiDescLabel.setText(juce::String::fromUTF8("DiscNativePro detecta automáticamente cualquier controladora DJ física conectada por USB (Pioneer DDJ, Hercules, Numark, Native Instruments). Mapea jog wheels, perillas de EQ, faders y pads."),
                          juce::dontSendNotification);
    midiDescLabel.setFont(juce::FontOptions(11.0f));
    midiDescLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(142, 149, 165));
    bodyContent.addChildComponent(midiDescLabel);

    midiDevicesHeader.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    midiDevicesHeader.setColour(juce::Label::textColourId, juce::Colours::white);
    bodyContent.addChildComponent(midiDevicesHeader);

    midiScanBtn.setButtonText(juce::String::fromUTF8("Escanear Puertos"));
    midiScanBtn.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(24, 28, 36));
    midiScanBtn.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(0, 180, 216));
    midiScanBtn.onClick = [this]() {
        midi.rescanDevices();
        refreshMidiDevices();
    };
    bodyContent.addChildComponent(midiScanBtn);

    midiMonitorHeader.setText(juce::String::fromUTF8("MONITOR DE SEÑALES MIDI EN VIVO"), juce::dontSendNotification);
    midiMonitorHeader.setFont(juce::FontOptions(9.5f, juce::Font::bold));
    midiMonitorHeader.setColour(juce::Label::textColourId, juce::Colour::fromRGB(100, 108, 125));
    bodyContent.addChildComponent(midiMonitorHeader);

    midiMonitorText.setText(juce::String::fromUTF8("Esperando movimiento de fader, jog o pad..."), juce::dontSendNotification);
    midiMonitorText.setFont(juce::FontOptions("Menlo", 11.5f, juce::Font::plain));
    midiMonitorText.setColour(juce::Label::textColourId, juce::Colour::fromRGB(0, 229, 255));
    bodyContent.addChildComponent(midiMonitorText);

    midi.onMidiActivity = [this](const juce::String& desc) {
        midiMonitorText.setText(desc, juce::dontSendNotification);
    };

    midiMappingHeader.setText(juce::String::fromUTF8("MAPEO PERSONALIZADO (MIDI LEARN)"), juce::dontSendNotification);
    midiMappingHeader.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    midiMappingHeader.setColour(juce::Label::textColourId, juce::Colour::fromRGB(46, 196, 182));
    bodyContent.addChildComponent(midiMappingHeader);

    midiMappingTable = std::make_unique<MidiMappingTableComponent>(midi);
    bodyContent.addChildComponent(*midiMappingTable);

    refreshMidiDevices();
}

void SettingsModalComponent::setupStreamingTab()
{
    streamingSectionTitle.setText(juce::String::fromUTF8("Integración de YouTube Streaming"), juce::dontSendNotification);
    streamingSectionTitle.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    streamingSectionTitle.setColour(juce::Label::textColourId, juce::Colour::fromRGB(0, 180, 216));
    bodyContent.addChildComponent(streamingSectionTitle);

    streamingDescLabel.setText(juce::String::fromUTF8("DiscNativePro permite buscar, mezclar y reproducir videos de YouTube en vivo directamente en las bandejas.\nPor defecto se utiliza Scraping Público para buscar sin necesidad de API Key."), juce::dontSendNotification);
    streamingDescLabel.setFont(juce::FontOptions(10.5f));
    streamingDescLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(160, 165, 180));
    bodyContent.addChildComponent(streamingDescLabel);

    apiKeyLabel.setText(juce::String::fromUTF8("YouTube Data API v3 Key (Opcional - Para búsqueda oficial sin restricciones):"), juce::dontSendNotification);
    apiKeyLabel.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    apiKeyLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(180, 185, 200));
    bodyContent.addChildComponent(apiKeyLabel);

    apiKeyEditor.setTextToShowWhenEmpty("Pega tu clave AIzaSy...", juce::Colour::fromRGB(100, 108, 125));
    apiKeyEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour::fromRGB(17, 19, 26));
    apiKeyEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    apiKeyEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour::fromRGB(38, 43, 58));
    apiKeyEditor.setPasswordCharacter((juce::juce_wchar)'*');
    apiKeyEditor.setText(YouTubeService::getApiKey(), juce::dontSendNotification);
    bodyContent.addChildComponent(apiKeyEditor);

    clearApiKeyBtn.setText("Borrar Key");
    clearApiKeyBtn.setFontSize(10.5f);
    clearApiKeyBtn.setCornerRadius(5.0f);
    clearApiKeyBtn.setCustomColours(juce::Colour::fromRGB(32, 35, 48),
                                   juce::Colour::fromRGB(244, 63, 94),
                                   juce::Colour::fromRGB(244, 63, 94),
                                   juce::Colour::fromRGB(45, 50, 65));
    clearApiKeyBtn.onClick = [this]() {
        apiKeyEditor.setText("", juce::dontSendNotification);
        YouTubeService::setApiKey("");
        updateTabVisibility();
    };
    bodyContent.addChildComponent(clearApiKeyBtn);

    apiStatusBadge.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    bodyContent.addChildComponent(apiStatusBadge);

    apiInfoCard.setText(juce::String::fromUTF8("Modo Video en Bandejas:\n• Al soltar un video de YouTube en el Deck 1 o Deck 2, el Jog Wheel se transforma en reproductor de video de alta definición (WebKit).\n• Los faders de volumen y el Crossfader central mezclan fluidamente el audio y video entre bandejas.\n• Iniciar sesión en YouTube en Safari comparte tu sesión con WKWebView para reproducción Premium sin publicidad."), juce::dontSendNotification);
    apiInfoCard.setFont(juce::FontOptions(10.0f));
    apiInfoCard.setColour(juce::Label::textColourId, juce::Colour::fromRGB(140, 145, 160));
    apiInfoCard.setJustificationType(juce::Justification::topLeft);
    bodyContent.addChildComponent(apiInfoCard);

}

void SettingsModalComponent::setupTransmitTab()
{
    auto styleEditor = [](juce::TextEditor& ed) {
        ed.setFont(juce::FontOptions(11.5f));
        ed.setColour(juce::TextEditor::backgroundColourId, juce::Colour::fromRGB(17, 19, 26));
        ed.setColour(juce::TextEditor::textColourId, juce::Colours::white);
        ed.setColour(juce::TextEditor::outlineColourId, juce::Colour::fromRGB(38, 43, 58));
    };
    auto styleLabel = [](juce::Label& lb) {
        lb.setFont(juce::FontOptions(11.0f, juce::Font::plain));
        lb.setColour(juce::Label::textColourId, juce::Colour::fromRGB(180, 185, 200));
        lb.setJustificationType(juce::Justification::centredLeft);
    };

    transmitTitleLabel.setText(juce::String::fromUTF8("Transmitir — Emisiones en vivo y podcasts"), juce::dontSendNotification);
    transmitTitleLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    transmitTitleLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(0, 180, 216));
    bodyContent.addChildComponent(transmitTitleLabel);

    transmitDescLabel.setText(juce::String::fromUTF8("Emite la mezcla master en vivo a tu radio propia o a tus redes (YouTube, Facebook, Twitch). Los oyentes se conectan a la plataforma, no a tu app."), juce::dontSendNotification);
    transmitDescLabel.setFont(juce::FontOptions(10.5f));
    transmitDescLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(160, 165, 180));
    bodyContent.addChildComponent(transmitDescLabel);

    // --- Destino ---
    styleLabel(broadcastDestLabel);
    broadcastDestLabel.setText(juce::String::fromUTF8("Destino de la emisión:"), juce::dontSendNotification);
    bodyContent.addChildComponent(broadcastDestLabel);

    broadcastDestCombo.addItem(juce::String::fromUTF8("Servidor de radio propio (Icecast/Shoutcast)"), 1);
    broadcastDestCombo.addItem("YouTube Live", 2);
    broadcastDestCombo.addItem("Facebook Live", 3);
    broadcastDestCombo.addItem("Twitch", 4);
    broadcastDestCombo.addItem("RTMP personalizado", 5);
    broadcastDestCombo.setSelectedId(juce::jmax(1, juce::jmin(5, readStringSetting(broadcastDestProperty, "1").getIntValue())),
                                     juce::dontSendNotification);
    broadcastDestCombo.onChange = [this]() {
        // Cambiar de destino con emisión activa reinicia el servicio en el nuevo destino
        if (audioEngine.getBroadcastService().isBroadcasting())
        {
            if (isRtmpDestination(broadcastDestCombo.getSelectedId()))
                rtmpUrlEditor.setText(defaultRtmpUrlForDestination(broadcastDestCombo.getSelectedId()),
                                      juce::dontSendNotification);
            rtmpKeyEditor.setText(SecretStore::read(rtmpKeyAccountForDestination(broadcastDestCombo.getSelectedId())),
                                  juce::dontSendNotification);
            updateTransmitDestinationUI();
            updateBroadcastRuntime();
        }
        else
        {
            updateTransmitDestinationUI();
        }
        persistBroadcastConfig();
        resized();
        repaint();
    };
    bodyContent.addChildComponent(broadcastDestCombo);

    // --- Radio propia (Icecast) ---
    broadcastSectionTitle.setText(juce::String::fromUTF8("Servidor de radio (Icecast/Shoutcast)"), juce::dontSendNotification);
    broadcastSectionTitle.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    broadcastSectionTitle.setColour(juce::Label::textColourId, juce::Colour::fromRGB(0, 180, 216));
    bodyContent.addChildComponent(broadcastSectionTitle);

    broadcastDescLabel.setText(juce::String::fromUTF8("Datos de tu servidor propio de streaming."), juce::dontSendNotification);
    broadcastDescLabel.setFont(juce::FontOptions(10.5f));
    broadcastDescLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(160, 165, 180));
    bodyContent.addChildComponent(broadcastDescLabel);

    styleLabel(broadcastHostLabel);
    broadcastHostLabel.setText(juce::String::fromUTF8("Servidor (host o IP):"), juce::dontSendNotification);
    bodyContent.addChildComponent(broadcastHostLabel);
    styleEditor(broadcastHostEditor);
    broadcastHostEditor.setTextToShowWhenEmpty("radio.midominio.com", juce::Colour::fromRGB(100, 108, 125));
    bodyContent.addChildComponent(broadcastHostEditor);

    styleLabel(broadcastPortLabel);
    broadcastPortLabel.setText(juce::String::fromUTF8("Puerto:"), juce::dontSendNotification);
    bodyContent.addChildComponent(broadcastPortLabel);
    styleEditor(broadcastPortEditor);
    broadcastPortEditor.setInputRestrictions(0, "0123456789");
    broadcastPortEditor.setTextToShowWhenEmpty("8000", juce::Colour::fromRGB(100, 108, 125));
    bodyContent.addChildComponent(broadcastPortEditor);

    styleLabel(broadcastMountLabel);
    broadcastMountLabel.setText(juce::String::fromUTF8("Mount:"), juce::dontSendNotification);
    bodyContent.addChildComponent(broadcastMountLabel);
    styleEditor(broadcastMountEditor);
    broadcastMountEditor.setTextToShowWhenEmpty("/stream", juce::Colour::fromRGB(100, 108, 125));
    bodyContent.addChildComponent(broadcastMountEditor);

    styleLabel(broadcastPasswordLabel);
    broadcastPasswordLabel.setText(juce::String::fromUTF8("Contraseña:"), juce::dontSendNotification);
    bodyContent.addChildComponent(broadcastPasswordLabel);
    styleEditor(broadcastPasswordEditor);
    broadcastPasswordEditor.setPasswordCharacter((juce::juce_wchar)'*');
    bodyContent.addChildComponent(broadcastPasswordEditor);

    styleLabel(broadcastNameLabel);
    broadcastNameLabel.setText(juce::String::fromUTF8("Nombre de la emisión:"), juce::dontSendNotification);
    bodyContent.addChildComponent(broadcastNameLabel);
    styleEditor(broadcastNameEditor);
    broadcastNameEditor.setTextToShowWhenEmpty("Mi radio DJ", juce::Colour::fromRGB(100, 108, 125));
    bodyContent.addChildComponent(broadcastNameEditor);

    styleLabel(broadcastBitrateLabel);
    broadcastBitrateLabel.setText(juce::String::fromUTF8("Bitrate MP3:"), juce::dontSendNotification);
    bodyContent.addChildComponent(broadcastBitrateLabel);
    broadcastBitrateCombo.addItem("128 kbps", 1);
    broadcastBitrateCombo.addItem("192 kbps", 2);
    broadcastBitrateCombo.addItem("256 kbps", 3);
    broadcastBitrateCombo.addItem("320 kbps", 4);
    broadcastBitrateCombo.setSelectedId(2, juce::dontSendNotification);
    bodyContent.addChildComponent(broadcastBitrateCombo);

    // --- RTMP (YouTube / Facebook / Twitch / personalizado) ---
    rtmpUrlLabel.setText(juce::String::fromUTF8("URL de ingesta RTMP:"), juce::dontSendNotification);
    styleLabel(rtmpUrlLabel);
    bodyContent.addChildComponent(rtmpUrlLabel);
    styleEditor(rtmpUrlEditor);
    rtmpUrlEditor.setTextToShowWhenEmpty("rtmp://...", juce::Colour::fromRGB(100, 108, 125));
    bodyContent.addChildComponent(rtmpUrlEditor);

    rtmpKeyLabel.setText(juce::String::fromUTF8("Stream key:"), juce::dontSendNotification);
    styleLabel(rtmpKeyLabel);
    bodyContent.addChildComponent(rtmpKeyLabel);
    styleEditor(rtmpKeyEditor);
    rtmpKeyEditor.setPasswordCharacter((juce::juce_wchar)'*');
    bodyContent.addChildComponent(rtmpKeyEditor);

    rtmpBitrateLabel.setText(juce::String::fromUTF8("Bitrate AAC:"), juce::dontSendNotification);
    styleLabel(rtmpBitrateLabel);
    bodyContent.addChildComponent(rtmpBitrateLabel);
    rtmpBitrateCombo.addItem("128 kbps", 1);
    rtmpBitrateCombo.addItem("192 kbps", 2);
    rtmpBitrateCombo.addItem("256 kbps", 3);
    rtmpBitrateCombo.setSelectedId(2, juce::dontSendNotification);
    bodyContent.addChildComponent(rtmpBitrateCombo);

    installFfmpegBtn.setText("Instalar ffmpeg");
    installFfmpegBtn.setFontSize(10.5f);
    installFfmpegBtn.setCornerRadius(5.0f);
    installFfmpegBtn.setCustomColours(juce::Colour::fromRGB(32, 35, 48),
                                      juce::Colour::fromRGB(0, 180, 216),
                                      juce::Colour::fromRGB(0, 180, 216),
                                      juce::Colour::fromRGB(45, 50, 65));
    installFfmpegBtn.onClick = [this]() {
        installFfmpegBtn.setEnabled(false);
        ffmpegStatusBadge.setText(LanguageManager::trUtf8("Descargando ffmpeg...", "Downloading ffmpeg..."),
                                  juce::dontSendNotification);
        juce::Thread::launch([this]() {
            const bool ok = FfmpegLocator::ensureInstalled();
            juce::MessageManager::callAsync([this, ok]() {
                installFfmpegBtn.setEnabled(true);
                refreshFfmpegStatus();
                if (!ok)
                    ffmpegStatusBadge.setColour(juce::Label::textColourId, juce::Colour::fromRGB(248, 113, 113));
            });
        });
    };
    bodyContent.addChildComponent(installFfmpegBtn);

    ffmpegStatusBadge.setFont(juce::FontOptions(10.0f));
    ffmpegStatusBadge.setColour(juce::Label::textColourId, juce::Colour::fromRGB(140, 145, 160));
    bodyContent.addChildComponent(ffmpegStatusBadge);

    // --- Emisión (toggle + estado, compartido por radio y RTMP) ---
    broadcastEnabledToggle.setButtonText(juce::String::fromUTF8("Emitir mezcla en vivo"));
    broadcastEnabledToggle.setColour(juce::ToggleButton::textColourId, juce::Colour::fromRGB(220, 225, 235));
    broadcastEnabledToggle.onClick = [this]() { updateBroadcastRuntime(); };
    bodyContent.addChildComponent(broadcastEnabledToggle);

    broadcastStatusBadge.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    bodyContent.addChildComponent(broadcastStatusBadge);

    // --- Podcast: exportar grabación a MP3 ---
    podcastSectionLabel.setText(juce::String::fromUTF8("Podcast (exportar grabación a MP3)"), juce::dontSendNotification);
    podcastSectionLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    podcastSectionLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(0, 180, 216));
    bodyContent.addChildComponent(podcastSectionLabel);

    podcastDescLabel.setText(juce::String::fromUTF8("Convierte una grabación de sesión (WAV) a MP3 listo para publicar."), juce::dontSendNotification);
    podcastDescLabel.setFont(juce::FontOptions(10.5f));
    podcastDescLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(160, 165, 180));
    bodyContent.addChildComponent(podcastDescLabel);

    podcastRecCombo.setTextWhenNothingSelected(
        LanguageManager::trUtf8("(sin grabaciones)", "(no recordings)"));
    bodyContent.addChildComponent(podcastRecCombo);

    podcastRefreshBtn.setText("Refrescar");
    podcastRefreshBtn.setFontSize(10.5f);
    podcastRefreshBtn.setCornerRadius(5.0f);
    podcastRefreshBtn.setCustomColours(juce::Colour::fromRGB(32, 35, 48),
                                       juce::Colour::fromRGB(0, 180, 216),
                                       juce::Colour::fromRGB(0, 180, 216),
                                       juce::Colour::fromRGB(45, 50, 65));
    podcastRefreshBtn.onClick = [this]() { refreshPodcastRecordings(); };
    bodyContent.addChildComponent(podcastRefreshBtn);

    podcastBitrateLabel.setText(juce::String::fromUTF8("Bitrate de exportación:"), juce::dontSendNotification);
    styleLabel(podcastBitrateLabel);
    bodyContent.addChildComponent(podcastBitrateLabel);
    podcastBitrateCombo.addItem("128 kbps", 1);
    podcastBitrateCombo.addItem("192 kbps", 2);
    podcastBitrateCombo.addItem("256 kbps", 3);
    podcastBitrateCombo.addItem("320 kbps", 4);
    podcastBitrateCombo.setSelectedId(2, juce::dontSendNotification);
    bodyContent.addChildComponent(podcastBitrateCombo);

    podcastExportBtn.setText("Exportar a MP3");
    podcastExportBtn.setFontSize(10.5f);
    podcastExportBtn.setCornerRadius(5.0f);
    podcastExportBtn.setCustomColours(juce::Colour::fromRGB(37, 99, 235),
                                      juce::Colours::white,
                                      juce::Colours::white,
                                      juce::Colour::fromRGB(45, 50, 65));
    podcastExportBtn.onClick = [this]() { exportPodcastMp3(); };
    bodyContent.addChildComponent(podcastExportBtn);

    podcastStatusBadge.setFont(juce::FontOptions(10.5f));
    podcastStatusBadge.setColour(juce::Label::textColourId, juce::Colour::fromRGB(140, 145, 160));
    bodyContent.addChildComponent(podcastStatusBadge);

    // --- Cargar configuración persistida ---
    broadcastHostEditor.setText(readStringSetting(broadcastHostProperty), juce::dontSendNotification);
    broadcastPortEditor.setText(readStringSetting(broadcastPortProperty, "8000"), juce::dontSendNotification);
    broadcastMountEditor.setText(readStringSetting(broadcastMountProperty, "/stream"), juce::dontSendNotification);
    broadcastNameEditor.setText(readStringSetting(broadcastNameProperty), juce::dontSendNotification);
    broadcastPasswordEditor.setText(SecretStore::read(broadcastPasswordAccount), juce::dontSendNotification);
    broadcastBitrateCombo.setSelectedId(comboIdFromBitrate(
        readStringSetting(broadcastBitrateProperty, "192").getIntValue()), juce::dontSendNotification);

    const int destId = broadcastDestCombo.getSelectedId();
    if (destId == 5)
        rtmpUrlEditor.setText(readStringSetting(rtmpCustomUrlProperty), juce::dontSendNotification);
    else
        rtmpUrlEditor.setText(defaultRtmpUrlForDestination(destId), juce::dontSendNotification);
    rtmpKeyEditor.setText(SecretStore::read(rtmpKeyAccountForDestination(destId)), juce::dontSendNotification);

    const int rtmpBitrateStored = readStringSetting(rtmpBitrateProperty, "192").getIntValue();
    rtmpBitrateCombo.setSelectedId(rtmpBitrateStored == 128 ? 1 : (rtmpBitrateStored == 256 ? 3 : 2),
                                   juce::dontSendNotification);
    podcastBitrateCombo.setSelectedId(comboIdFromBitrate(
        readStringSetting(podcastBitrateProperty, "256").getIntValue()), juce::dontSendNotification);

    // Si la emision ya estaba activa (configurado desde otra sesion del modal)
    broadcastEnabledToggle.setToggleState(audioEngine.getBroadcastService().isBroadcasting(), juce::dontSendNotification);

    updateTransmitDestinationUI();
    refreshFfmpegStatus();
    refreshPodcastRecordings();
}

void SettingsModalComponent::updateBroadcastRuntime()
{
    auto& svc = audioEngine.getBroadcastService();
    const int destId = broadcastDestCombo.getSelectedId();

    if (broadcastEnabledToggle.getToggleState())
    {
        BroadcastService::Config cfg;

        if (isRtmpDestination(destId))
        {
            cfg.mode = BroadcastService::Mode::Rtmp;
            cfg.ingestUrl = composeRtmpIngestUrl(rtmpUrlEditor.getText(), rtmpKeyEditor.getText());
            cfg.bitrateKbps = rtmpBitrateCombo.getSelectedId() == 1 ? 128
                           : (rtmpBitrateCombo.getSelectedId() == 3 ? 256 : 192);
        }
        else
        {
            cfg.mode = BroadcastService::Mode::Icecast;
            cfg.host = broadcastHostEditor.getText().trim();
            cfg.port = juce::jlimit(1, 65535, broadcastPortEditor.getText().trim().getIntValue());
            if (cfg.port == 0)
                cfg.port = 8000;
            cfg.mount = broadcastMountEditor.getText().trim();
            cfg.password = broadcastPasswordEditor.getText();
            cfg.stationName = broadcastNameEditor.getText().trim();
            cfg.bitrateKbps = bitrateFromComboId(broadcastBitrateCombo.getSelectedId());
        }

        persistBroadcastConfig();

        if (!svc.startBroadcast(cfg, audioEngine.getSampleRate(), 2))
            broadcastEnabledToggle.setToggleState(false, juce::dontSendNotification);
    }
    else
    {
        svc.stopBroadcast();
    }

    refreshBroadcastStatus();
}

void SettingsModalComponent::updateTransmitDestinationUI()
{
    const int destId = broadcastDestCombo.getSelectedId();
    const bool radio = (destId == 1);
    const bool rtmp = isRtmpDestination(destId);

    broadcastSectionTitle.setVisible(radio);
    broadcastDescLabel.setVisible(radio);
    broadcastHostLabel.setVisible(radio);
    broadcastHostEditor.setVisible(radio);
    broadcastPortLabel.setVisible(radio);
    broadcastPortEditor.setVisible(radio);
    broadcastMountLabel.setVisible(radio);
    broadcastMountEditor.setVisible(radio);
    broadcastPasswordLabel.setVisible(radio);
    broadcastPasswordEditor.setVisible(radio);
    broadcastNameLabel.setVisible(radio);
    broadcastNameEditor.setVisible(radio);
    broadcastBitrateLabel.setVisible(radio);
    broadcastBitrateCombo.setVisible(radio);

    rtmpUrlLabel.setVisible(rtmp);
    rtmpUrlEditor.setVisible(rtmp);
    rtmpKeyLabel.setVisible(rtmp);
    rtmpKeyEditor.setVisible(rtmp);
    rtmpBitrateLabel.setVisible(rtmp);
    rtmpBitrateCombo.setVisible(rtmp);
    installFfmpegBtn.setVisible(rtmp);
    ffmpegStatusBadge.setVisible(rtmp);

    // La URL solo es editable en el destino personalizado
    rtmpUrlEditor.setReadOnly(destId != 5);

    if (rtmp)
        refreshFfmpegStatus();
}

void SettingsModalComponent::refreshFfmpegStatus()
{
    const auto ffmpeg = FfmpegLocator::locateFfmpeg();
    if (ffmpeg.existsAsFile())
    {
        ffmpegStatusBadge.setText(LanguageManager::trUtf8("ffmpeg listo: ", "ffmpeg ready: ")
                                      + ffmpeg.getFullPathName(), juce::dontSendNotification);
        ffmpegStatusBadge.setColour(juce::Label::textColourId, juce::Colour::fromRGB(74, 222, 128));
        installFfmpegBtn.setVisible(false);
    }
    else
    {
        ffmpegStatusBadge.setText(LanguageManager::trUtf8(
            "ffmpeg no está instalado (necesario para emitir a redes)",
            "ffmpeg is not installed (required to stream to networks)"), juce::dontSendNotification);
        ffmpegStatusBadge.setColour(juce::Label::textColourId, juce::Colour::fromRGB(250, 204, 21));
        installFfmpegBtn.setVisible(isRtmpDestination(broadcastDestCombo.getSelectedId()));
    }
}

void SettingsModalComponent::refreshPodcastRecordings()
{
    podcastRecCombo.clear(juce::dontSendNotification);

    auto dir = audioEngine.getRecordingDirectory();
    if (!dir.isDirectory())
        return;

    auto files = dir.findChildFiles(juce::File::findFiles, false, "*.wav");
    files.sort();
    for (int i = 0; i < files.size(); ++i)
        podcastRecCombo.addItem(files.getReference(i).getFileName(), i + 1);

    if (!files.isEmpty())
        podcastRecCombo.setSelectedId(files.size(), juce::dontSendNotification); // la más reciente
}

void SettingsModalComponent::exportPodcastMp3()
{
    if (isExportingPodcast)
        return;

    auto dir = audioEngine.getRecordingDirectory();
    auto wav = dir.getChildFile(podcastRecCombo.getText());

    if (podcastRecCombo.getText().isEmpty() || !wav.existsAsFile())
    {
        podcastStatusBadge.setText(LanguageManager::trUtf8("Selecciona una grabación válida",
                                                          "Select a valid recording"), juce::dontSendNotification);
        podcastStatusBadge.setColour(juce::Label::textColourId, juce::Colour::fromRGB(248, 113, 113));
        return;
    }

    const int bitrate = bitrateFromComboId(podcastBitrateCombo.getSelectedId());
    auto outMp3 = wav.withFileExtension("mp3");

    isExportingPodcast = true;
    podcastExportBtn.setEnabled(false);
    podcastStatusBadge.setText(LanguageManager::trUtf8("Exportando a MP3...", "Exporting to MP3..."),
                               juce::dontSendNotification);
    podcastStatusBadge.setColour(juce::Label::textColourId, juce::Colour::fromRGB(0, 229, 255));

    juce::Thread::launch([this, wav, outMp3, bitrate]() {
        const auto error = Mp3Exporter::encodeWavToMp3(wav, outMp3, bitrate);
        juce::MessageManager::callAsync([this, error, outMp3]() {
            isExportingPodcast = false;
            podcastExportBtn.setEnabled(true);

            if (error.isEmpty())
            {
                podcastStatusBadge.setText(LanguageManager::trUtf8("MP3 creado: ", "MP3 created: ")
                                               + outMp3.getFileName(), juce::dontSendNotification);
                podcastStatusBadge.setColour(juce::Label::textColourId, juce::Colour::fromRGB(74, 222, 128));
            }
            else
            {
                podcastStatusBadge.setText(error, juce::dontSendNotification);
                podcastStatusBadge.setColour(juce::Label::textColourId, juce::Colour::fromRGB(248, 113, 113));
            }
        });
    });
}

void SettingsModalComponent::persistBroadcastConfig()
{
    writeStringSetting(broadcastDestProperty, juce::String(broadcastDestCombo.getSelectedId()));
    writeStringSetting(broadcastHostProperty, broadcastHostEditor.getText().trim());
    writeStringSetting(broadcastPortProperty, broadcastPortEditor.getText().trim());
    writeStringSetting(broadcastMountProperty, broadcastMountEditor.getText().trim());
    writeStringSetting(broadcastNameProperty, broadcastNameEditor.getText().trim());
    writeStringSetting(broadcastBitrateProperty,
                       juce::String(bitrateFromComboId(broadcastBitrateCombo.getSelectedId())));
    SecretStore::write(broadcastPasswordAccount, broadcastPasswordEditor.getText());

    const int destId = broadcastDestCombo.getSelectedId();
    if (destId == 5)
        writeStringSetting(rtmpCustomUrlProperty, rtmpUrlEditor.getText().trim());
    writeStringSetting(rtmpBitrateProperty, juce::String(rtmpBitrateCombo.getSelectedId() == 1 ? 128
                                                                                               : (rtmpBitrateCombo.getSelectedId() == 3 ? 256 : 192)));
    SecretStore::write(rtmpKeyAccountForDestination(destId), rtmpKeyEditor.getText());
    writeStringSetting(podcastBitrateProperty,
                       juce::String(bitrateFromComboId(podcastBitrateCombo.getSelectedId())));
}

void SettingsModalComponent::refreshBroadcastStatus()
{
    auto& svc = audioEngine.getBroadcastService();

    if (svc.isBroadcasting())
    {
        if (svc.isConnected())
        {
            const double mb = (double) svc.getBytesSent() / (1024.0 * 1024.0);
            broadcastStatusBadge.setText(
                juce::String::fromUTF8("● ") + LanguageManager::trUtf8("Emitiendo en vivo", "Broadcasting live")
                    + " — " + juce::String::formatted("%.1f MB", mb),
                juce::dontSendNotification);
            broadcastStatusBadge.setColour(juce::Label::textColourId, juce::Colour::fromRGB(74, 222, 128));
        }
        else
        {
            const auto err = svc.getLastError();
            broadcastStatusBadge.setText(
                juce::String::fromUTF8("● ") + LanguageManager::trUtf8("Reconectando", "Reconnecting")
                    + (err.isNotEmpty() ? " — " + err : juce::String()),
                juce::dontSendNotification);
            broadcastStatusBadge.setColour(juce::Label::textColourId, juce::Colour::fromRGB(250, 204, 21));
        }
    }
    else
    {
        broadcastStatusBadge.setText(
            juce::String::fromUTF8("● ") + LanguageManager::trUtf8("Emisión apagada", "Broadcast off"),
            juce::dontSendNotification);
        broadcastStatusBadge.setColour(juce::Label::textColourId, juce::Colour::fromRGB(120, 125, 140));
    }
}

void SettingsModalComponent::timerCallback()
{
    if (activeTab == 10)
        refreshBroadcastStatus();
}

namespace
{
AIProvider providerFromComboId(int id)
{
    switch (id)
    {
        case 1: return AIProvider::Gemini;
        case 2: return AIProvider::OpenAI;
        case 3: return AIProvider::Claude;
        case 4: return AIProvider::DeepSeek;
        case 5: return AIProvider::OpenRouter;
        default: return AIProvider::Gemini;
    }
}

int comboIdFromProvider(AIProvider p)
{
    switch (p)
    {
        case AIProvider::Gemini: return 1;
        case AIProvider::OpenAI: return 2;
        case AIProvider::Claude: return 3;
        case AIProvider::DeepSeek: return 4;
        case AIProvider::OpenRouter: return 5;
    }
    return 1;
}
} // namespace

void SettingsModalComponent::setupAITab()
{
    aiSectionTitle.setText(juce::String::fromUTF8("Inteligencia Artificial (AI Smart Match)"), juce::dontSendNotification);
    aiSectionTitle.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    aiSectionTitle.setColour(juce::Label::textColourId, juce::Colour::fromRGB(192, 132, 252));
    bodyContent.addChildComponent(aiSectionTitle);

    aiDescLabel.setText(juce::String::fromUTF8("Analiza la pista en reproducción (local o YouTube) y sugiere mezclas armónicas por género, tempo y Camelot Key.\nElige tu proveedor preferido e ingresa tu clave API personal."), juce::dontSendNotification);
    aiDescLabel.setFont(juce::FontOptions(10.5f));
    aiDescLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(160, 165, 180));
    bodyContent.addChildComponent(aiDescLabel);

    aiEnabledToggle.setButtonText(juce::String::fromUTF8("Habilitar Recomendaciones Inteligentes de IA"));
    aiEnabledToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    aiEnabledToggle.setColour(juce::ToggleButton::tickColourId, juce::Colour::fromRGB(168, 85, 247));
    aiEnabledToggle.setToggleState(AIService::isEnabled(), juce::dontSendNotification);
    aiEnabledToggle.onClick = [this]() { updateTabVisibility(); };
    bodyContent.addChildComponent(aiEnabledToggle);

    aiProviderLabel.setText("Proveedor de Inteligencia Artificial:", juce::dontSendNotification);
    aiProviderLabel.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    aiProviderLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(180, 185, 200));
    bodyContent.addChildComponent(aiProviderLabel);

    aiProviderCombo.addItem("Google Gemini (Flash - Gratuito en AI Studio)", 1);
    aiProviderCombo.addItem("OpenAI (ChatGPT / GPT-4o mini)", 2);
    aiProviderCombo.addItem("Anthropic Claude (Claude 3.5 Haiku)", 3);
    aiProviderCombo.addItem("DeepSeek (DeepSeek V3 / R1)", 4);
    aiProviderCombo.addItem("OpenRouter (Key sk-or-v1...)", 5);

    auto curProv = AIService::getProvider();
    int provId = comboIdFromProvider(curProv);
    aiProviderCombo.setSelectedId(provId, juce::dontSendNotification);
    aiProviderCombo.onChange = [this]() {
        AIProvider p = providerFromComboId(aiProviderCombo.getSelectedId());
        aiApiKeyEditor.setText(AIService::getApiKey(p), juce::dontSendNotification);
        refreshAiModelOptions();
        updateTabVisibility();
    };
    bodyContent.addChildComponent(aiProviderCombo);

    // Modelo configurable por proveedor: ComboBox editable = presets + texto libre.
    // ponytail: un solo control cubre selector y campo custom; evita un TextEditor extra.
    aiModelLabel.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    aiModelLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(180, 185, 200));
    bodyContent.addChildComponent(aiModelLabel);

    aiModelCombo.setEditableText(true);
    aiModelCombo.setJustificationType(juce::Justification::centredLeft);
    refreshAiModelOptions();
    bodyContent.addChildComponent(aiModelCombo);

    aiApiKeyLabel.setText("Clave API (API Key) del Proveedor Seleccionado:", juce::dontSendNotification);
    aiApiKeyLabel.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    aiApiKeyLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(180, 185, 200));
    bodyContent.addChildComponent(aiApiKeyLabel);

    aiApiKeyEditor.setTextToShowWhenEmpty("Pega tu API Key...", juce::Colour::fromRGB(100, 108, 125));
    aiApiKeyEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour::fromRGB(17, 19, 26));
    aiApiKeyEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    aiApiKeyEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour::fromRGB(38, 43, 58));
    aiApiKeyEditor.setPasswordCharacter((juce::juce_wchar)'*');
    aiApiKeyEditor.setText(AIService::getApiKey(curProv), juce::dontSendNotification);
    aiApiKeyEditor.onTextChange = [this]() { updateTabVisibility(); };
    bodyContent.addChildComponent(aiApiKeyEditor);

    clearAiApiKeyBtn.setText("Borrar Key");
    clearAiApiKeyBtn.setFontSize(10.5f);
    clearAiApiKeyBtn.setCornerRadius(5.0f);
    clearAiApiKeyBtn.setCustomColours(juce::Colour::fromRGB(32, 35, 48),
                                      juce::Colour::fromRGB(244, 63, 94),
                                      juce::Colour::fromRGB(244, 63, 94),
                                      juce::Colour::fromRGB(45, 50, 65));
    clearAiApiKeyBtn.onClick = [this]() {
        AIProvider p = providerFromComboId(aiProviderCombo.getSelectedId());
        aiApiKeyEditor.setText("", juce::dontSendNotification);
        AIService::setApiKey(p, "");
        updateTabVisibility();
    };
    bodyContent.addChildComponent(clearAiApiKeyBtn);

    aiStatusBadge.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    bodyContent.addChildComponent(aiStatusBadge);

    aiInfoCard.setText(juce::String::fromUTF8("Obtén tu API Key gratuita:\n• Google Gemini: aistudio.google.com (15 req/min sin costo)\n• OpenAI: platform.openai.com\n• Anthropic: console.anthropic.com\n• DeepSeek: platform.deepseek.com\n• OpenRouter: openrouter.ai (una key, 200+ modelos)"), juce::dontSendNotification);
    aiInfoCard.setFont(juce::FontOptions(10.5f));
    aiInfoCard.setColour(juce::Label::textColourId, juce::Colour::fromRGB(140, 145, 160));
    aiInfoCard.setJustificationType(juce::Justification::topLeft);
    bodyContent.addChildComponent(aiInfoCard);
}

void SettingsModalComponent::setupDonateTab()
{
    donateTitleLabel.setText(juce::String::fromUTF8("Apoya DiscNativePro"), juce::dontSendNotification);
    donateTitleLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    donateTitleLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(251, 191, 36));
    bodyContent.addChildComponent(donateTitleLabel);

    donateDescLabel.setText(juce::String::fromUTF8("¿Estás disfrutando de DiscNativePro? Puedes apoyar el desarrollo y las futuras funcionalidades regalándome un café."), juce::dontSendNotification);
    donateDescLabel.setFont(juce::FontOptions(11.0f));
    donateDescLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(200, 205, 215));
    donateDescLabel.setJustificationType(juce::Justification::topLeft);
    bodyContent.addChildComponent(donateDescLabel);

    donateBtn.setIcon(LucideIcons::IconType::Coffee, 15.0f);
    donateBtn.setText(juce::String::fromUTF8("Donar un café en Cafecito"));
    donateBtn.setFontSize(12.0f);
    donateBtn.setCornerRadius(8.0f);
    donateBtn.setCustomColours(juce::Colour::fromRGB(120, 53, 15),
                               juce::Colours::white,
                               juce::Colour::fromRGB(251, 191, 36),
                               juce::Colour::fromRGB(251, 191, 36).withAlpha(0.6f));
    donateBtn.onClick = []() {
        juce::URL("https://cafecito.app/discnativepro").launchInDefaultBrowser();
    };
    bodyContent.addChildComponent(donateBtn);

    donateHintLabel.setFont(juce::FontOptions(10.0f));
    donateHintLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(140, 145, 160));
    donateHintLabel.setJustificationType(juce::Justification::centred);
    bodyContent.addChildComponent(donateHintLabel);

    donateInfoCard.setFont(juce::FontOptions(10.5f));
    donateInfoCard.setColour(juce::Label::textColourId, juce::Colour::fromRGB(140, 145, 160));
    donateInfoCard.setJustificationType(juce::Justification::topLeft);
    bodyContent.addChildComponent(donateInfoCard);
}

void SettingsModalComponent::setupDesignTab()
{
    designTitleLabel.setText(juce::String::fromUTF8("Personalización de Waveforms"), juce::dontSendNotification);
    designTitleLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    designTitleLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(0, 180, 216));
    bodyContent.addChildComponent(designTitleLabel);

    designDescLabel.setText(juce::String::fromUTF8("Elige los colores de Graves, Medios y Agudos. Se aplican en vivo al wave de la bandeja seleccionada y se guardan automáticamente."), juce::dontSendNotification);
    designDescLabel.setFont(juce::FontOptions(10.5f));
    designDescLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(160, 165, 180));
    designDescLabel.setJustificationType(juce::Justification::topLeft);
    bodyContent.addChildComponent(designDescLabel);

    // Selector de bandeja (Deck 1 / Deck 2): cada bandeja tiene su propio tema
    deckThemeLabel.setText(juce::String::fromUTF8("Bandeja:"), juce::dontSendNotification);
    deckThemeLabel.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    deckThemeLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(180, 185, 200));
    bodyContent.addChildComponent(deckThemeLabel);

    auto setupDeckToggle = [this](DjButton& btn, int deck) {
        btn.setFontSize(10.5f);
        btn.setCornerRadius(6.0f);
        btn.onClick = [this, deck]() {
            designDeckIndex = deck;
            refreshDesignSwatches();
        };
        bodyContent.addChildComponent(btn);
    };

    deckTheme1Btn.setText(juce::String::fromUTF8("Bandeja 1"));
    deckTheme2Btn.setText(juce::String::fromUTF8("Bandeja 2"));
    setupDeckToggle(deckTheme1Btn, 0);
    setupDeckToggle(deckTheme2Btn, 1);

    auto setupRow = [this](juce::Label& label, DjButton& swatch, const char* text, int band) {
        label.setText(juce::String::fromUTF8(text), juce::dontSendNotification);
        label.setFont(juce::FontOptions(11.0f, juce::Font::plain));
        label.setColour(juce::Label::textColourId, juce::Colour::fromRGB(180, 185, 200));
        bodyContent.addChildComponent(label);

        swatch.setFontSize(10.5f);
        swatch.setCornerRadius(6.0f);
        swatch.onClick = [this, band]() { openColourPicker(band); };
        bodyContent.addChildComponent(swatch);
    };

    setupRow(waveLowLabel, waveLowSwatch, "Graves (Low):", 0);
    setupRow(waveMidLabel, waveMidSwatch, "Medios (Mid):", 1);
    setupRow(waveHighLabel, waveHighSwatch, "Agudos (High):", 2);

    designPresetLabel.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    designPresetLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(180, 185, 200));
    bodyContent.addChildComponent(designPresetLabel);

    auto setupPreset = [this](DjButton& btn, int preset) {
        btn.setFontSize(10.5f);
        btn.setCornerRadius(6.0f);
        btn.setCustomColours(juce::Colour::fromRGB(32, 35, 48),
                             juce::Colour::fromRGB(200, 205, 220),
                             juce::Colour::fromRGB(0, 180, 216),
                             juce::Colour::fromRGB(45, 50, 68));
        btn.onClick = [this, preset]() {
            WaveformTheme::applyPreset(designDeckIndex, preset);
            refreshDesignSwatches();
        };
        bodyContent.addChildComponent(btn);
    };

    setupPreset(presetRgbBtn, WaveformTheme::RgbMulticolor);
    setupPreset(presetNeonBtn, WaveformTheme::NeonBlue);
    setupPreset(presetSilverBtn, WaveformTheme::SilverMono);

    refreshDesignSwatches();
}

void SettingsModalComponent::setupVersionTab()
{
    // Section: Versión
    versionSectionTitle.setText(LanguageManager::trUtf8("Versión", "Version"), juce::dontSendNotification);
    versionSectionTitle.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    versionSectionTitle.setColour(juce::Label::textColourId, juce::Colour::fromRGB(142, 149, 165));
    bodyContent.addChildComponent(versionSectionTitle);

    versionAppTitle.setText("DiscNativePro " + UpdateService::getCurrentVersion(), juce::dontSendNotification);
    versionAppTitle.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    versionAppTitle.setColour(juce::Label::textColourId, juce::Colours::white);
    bodyContent.addChildComponent(versionAppTitle);

    versionStatusLabel.setText("DiscNativePro " + UpdateService::getCurrentVersion() +
                               LanguageManager::trUtf8(" es la última versión.", " is the latest version."),
                               juce::dontSendNotification);
    versionStatusLabel.setFont(juce::FontOptions(11.5f));
    versionStatusLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(148, 163, 184));
    bodyContent.addChildComponent(versionStatusLabel);

    checkUpdatesBtn.setText(LanguageManager::trUtf8("Buscar actualizaciones", "Check for updates"));
    checkUpdatesBtn.setFontSize(11.0f);
    checkUpdatesBtn.setCornerRadius(6.0f);
    checkUpdatesBtn.setCustomColours(juce::Colour::fromRGB(32, 35, 48),
                                    juce::Colours::white,
                                    juce::Colours::white,
                                    juce::Colour::fromRGB(55, 60, 78));
    checkUpdatesBtn.onClick = [this]() { checkUpdatesFromUI(); };
    bodyContent.addChildComponent(checkUpdatesBtn);

    // Section: Cambiar de versión
    versionChangeTitle.setText(LanguageManager::trUtf8("Cambiar de versión", "Change version"), juce::dontSendNotification);
    versionChangeTitle.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    versionChangeTitle.setColour(juce::Label::textColourId, juce::Colours::white);
    bodyContent.addChildComponent(versionChangeTitle);

    versionHelpIcon.setText("(?)", juce::dontSendNotification);
    versionHelpIcon.setFont(juce::FontOptions(11.0f));
    versionHelpIcon.setColour(juce::Label::textColourId, juce::Colour::fromRGB(140, 145, 160));
    versionHelpIcon.setTooltip(LanguageManager::trUtf8(
        "Selecciona cualquier versión oficial disponible en GitHub Releases para instalarla o cambiar de versión.",
        "Select any official version available in GitHub Releases to install or switch versions."));
    bodyContent.addChildComponent(versionHelpIcon);

    versionSelectCombo.setTextWhenNothingSelected(LanguageManager::trUtf8("Buscar versiones disponibles...", "Check available versions..."));
    versionSelectCombo.setColour(juce::ComboBox::backgroundColourId, juce::Colour::fromRGB(22, 25, 34));
    versionSelectCombo.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    versionSelectCombo.setColour(juce::ComboBox::outlineColourId, juce::Colour::fromRGB(45, 50, 68));
    bodyContent.addChildComponent(versionSelectCombo);

    installVersionBtn.setIcon(LucideIcons::IconType::Download, 13.0f);
    installVersionBtn.setText(LanguageManager::trUtf8("Instalar", "Install"));
    installVersionBtn.setFontSize(11.5f);
    installVersionBtn.setCornerRadius(6.0f);
    installVersionBtn.setCustomColours(juce::Colour::fromRGB(14, 165, 233),
                                       juce::Colours::white,
                                       juce::Colours::white,
                                       juce::Colour::fromRGB(2, 132, 199));
    installVersionBtn.onClick = [this]() {
        if (isDownloading)
            return;

        int selIdx = versionSelectCombo.getSelectedId() - 1;
        if (selIdx < 0 || selIdx >= (int)availableReleases.size())
        {
            juce::URL("https://github.com/axel-molina/DiscNativePro/releases").launchInDefaultBrowser();
            return;
        }

        const auto& chosen = availableReleases[(size_t)selIdx];
        juce::String url = chosen.downloadUrl.isNotEmpty() ? chosen.downloadUrl : chosen.pageUrl;
        juce::String assetName = chosen.assetName;

        isDownloading = true;
        installVersionBtn.setEnabled(false);
        checkUpdatesBtn.setEnabled(false);
        downloadProgressValue = 0.0;
        downloadProgressBar.setVisible(true);
        downloadStatusLabel.setVisible(true);
        downloadStatusLabel.setText(LanguageManager::trUtf8("Iniciando descarga...", "Starting download..."), juce::dontSendNotification);
        downloadStatusLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(56, 189, 248));
        resized();

        juce::Component::SafePointer<SettingsModalComponent> safeThis(this);
        UpdateService::downloadAndOpenReleaseAsset(
            url,
            assetName,
            [safeThis](float progress, const juce::String& statusText) {
                if (safeThis == nullptr) return;
                safeThis->downloadProgressValue = progress;
                safeThis->downloadStatusLabel.setText(
                    LanguageManager::trUtf8("Descargando: ", "Downloading: ") + statusText,
                    juce::dontSendNotification);
                safeThis->downloadProgressBar.repaint();
            },
            [safeThis](bool success, const juce::String& errMsg, const juce::File& /*file*/) {
                if (safeThis == nullptr) return;
                safeThis->isDownloading = false;
                safeThis->installVersionBtn.setEnabled(true);
                safeThis->checkUpdatesBtn.setEnabled(true);

                if (success)
                {
                    safeThis->downloadProgressValue = 1.0;
                    safeThis->downloadStatusLabel.setText(
                        LanguageManager::trUtf8("¡Descarga completada! Abriendo instalador...", "Download complete! Opening installer..."),
                        juce::dontSendNotification);
                    safeThis->downloadStatusLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(74, 222, 128));
                }
                else
                {
                    safeThis->downloadProgressValue = -1.0;
                    safeThis->downloadStatusLabel.setText(
                        LanguageManager::trUtf8("Error en la descarga: ", "Download error: ") + errMsg,
                        juce::dontSendNotification);
                    safeThis->downloadStatusLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(244, 63, 94));
                }
                safeThis->downloadProgressBar.setVisible(false);
                safeThis->resized();
            });
    };
    bodyContent.addChildComponent(installVersionBtn);

    downloadStatusLabel.setFont(juce::FontOptions(11.0f));
    downloadStatusLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(56, 189, 248));
    bodyContent.addChildComponent(downloadStatusLabel);

    downloadProgressBar.setPercentageDisplay(false);
    downloadProgressBar.setColour(juce::ProgressBar::foregroundColourId, juce::Colour::fromRGB(14, 165, 233));
    downloadProgressBar.setColour(juce::ProgressBar::backgroundColourId, juce::Colour::fromRGB(26, 29, 38));
    bodyContent.addChildComponent(downloadProgressBar);
}

void SettingsModalComponent::checkUpdatesFromUI()
{
    if (isCheckingUpdates || isDownloading)
        return;

    isCheckingUpdates = true;
    checkUpdatesBtn.setEnabled(false);
    versionStatusLabel.setText(LanguageManager::trUtf8("Buscando actualizaciones en GitHub...", "Checking for updates on GitHub..."), juce::dontSendNotification);
    versionStatusLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(148, 163, 184));

    juce::Component::SafePointer<SettingsModalComponent> safeThis(this);
    UpdateService::checkForUpdatesAsync([safeThis](const UpdateInfo& info) {
        if (safeThis == nullptr)
            return;

        safeThis->isCheckingUpdates = false;
        safeThis->checkUpdatesBtn.setEnabled(true);
        safeThis->availableReleases = info.allReleases;

        safeThis->versionSelectCombo.clear(juce::dontSendNotification);
        int id = 1;
        for (const auto& rel : safeThis->availableReleases)
        {
            juce::String label = rel.tagName;
            if (id == 1)
                label += LanguageManager::trUtf8(" (Última versión)", " (Latest release)");
            safeThis->versionSelectCombo.addItem(label, id++);
        }
        if (safeThis->versionSelectCombo.getNumItems() > 0)
            safeThis->versionSelectCombo.setSelectedId(1, juce::dontSendNotification);

        if (info.available)
        {
            safeThis->versionStatusLabel.setText(
                LanguageManager::trUtf8("¡Hay una nueva versión disponible! (", "A new version is available! (") + info.latestTag + ")",
                juce::dontSendNotification);
            safeThis->versionStatusLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(245, 158, 11)); // Ámbar
        }
        else
        {
            safeThis->versionStatusLabel.setText(
                "DiscNativePro " + info.currentVersion + LanguageManager::trUtf8(" es la última versión.", " is the latest version."),
                juce::dontSendNotification);
            safeThis->versionStatusLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(74, 222, 128)); // Verde
        }

        if (safeThis->onUpdateAvailabilityChanged)
            safeThis->onUpdateAvailabilityChanged(info.available);
    });
}

void SettingsModalComponent::refreshDesignSwatches()
{
    auto paintSwatch = [](DjButton& btn, juce::Colour c) {
        btn.setCustomColours(c,
                             c.contrasting(),
                             c.contrasting(),
                             juce::Colours::white.withAlpha(0.4f));
        btn.setText("#" + c.toString().substring(2).toUpperCase());
    };

    const int deck = designDeckIndex;
    paintSwatch(waveLowSwatch, WaveformTheme::low(deck));
    paintSwatch(waveMidSwatch, WaveformTheme::mid(deck));
    paintSwatch(waveHighSwatch, WaveformTheme::high(deck));

    // Estado visual del toggle de bandeja
    auto styleDeckToggle = [](DjButton& btn, bool isActive) {
        if (isActive)
        {
            btn.setCustomColours(juce::Colour::fromRGB(0, 180, 216).withAlpha(0.22f),
                                 juce::Colour::fromRGB(0, 229, 255),
                                 juce::Colour::fromRGB(0, 229, 255),
                                 juce::Colour::fromRGB(0, 180, 216));
        }
        else
        {
            btn.setCustomColours(juce::Colour::fromRGB(32, 35, 48),
                                 juce::Colour::fromRGB(160, 165, 180),
                                 juce::Colour::fromRGB(160, 165, 180),
                                 juce::Colour::fromRGB(45, 50, 68));
        }
    };
    styleDeckToggle(deckTheme1Btn, designDeckIndex == 0);
    styleDeckToggle(deckTheme2Btn, designDeckIndex == 1);

    repaint();
}

void SettingsModalComponent::openColourPicker(int band)
{
    auto selector = std::make_unique<juce::ColourSelector>(
        juce::ColourSelector::showAlphaChannel
        | juce::ColourSelector::showColourAtTop
        | juce::ColourSelector::editableColour
        | juce::ColourSelector::showSliders);
    juce::Colour current = (band == 0) ? WaveformTheme::low(designDeckIndex)
                         : (band == 1) ? WaveformTheme::mid(designDeckIndex)
                                       : WaveformTheme::high(designDeckIndex);
    selector->setCurrentColour(current);
    selector->setSize(320, 420);
    selector->addChangeListener(this);
    activeColourSelector = selector.get();
    activeColourBand = band;
    activeColourDeck = designDeckIndex; // congela la bandeja destino del picker

    juce::DialogWindow::LaunchOptions opts;
    opts.content.setOwned(selector.release());
    opts.dialogTitle = LanguageManager::trUtf8("Elegir color de waveform", "Choose waveform colour");
    opts.useNativeTitleBar = true;
    opts.resizable = false;
    opts.escapeKeyTriggersCloseButton = true;
    opts.launchAsync();
}

void SettingsModalComponent::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    // SafePointer: nulo automático si el diálogo ya se cerró; solo compara.
    auto* selector = activeColourSelector.getComponent();
    if (selector != nullptr && source == selector && activeColourBand >= 0)
    {
        juce::Colour c = selector->getCurrentColour();
        if (activeColourBand == 0)
            WaveformTheme::setLow(activeColourDeck, c);
        else if (activeColourBand == 1)
            WaveformTheme::setMid(activeColourDeck, c);
        else
            WaveformTheme::setHigh(activeColourDeck, c);
        refreshDesignSwatches();
    }
}

void SettingsModalComponent::refreshAiModelOptions()
{
    AIProvider p = providerFromComboId(aiProviderCombo.getSelectedId());

    auto models = AIService::getAvailableModels(p);
    auto current = AIService::getModel(p);

    aiModelCombo.clear(juce::dontSendNotification);
    for (int i = 0; i < models.size(); ++i)
        aiModelCombo.addItem(models[i], i + 1);

    bool found = false;
    for (int i = 0; i < models.size(); ++i)
    {
        if (models[i].trim().equalsIgnoreCase(current.trim()))
        {
            aiModelCombo.setSelectedId(i + 1, juce::dontSendNotification);
            found = true;
            break;
        }
    }
    if (!found)
    {
        // Modelo custom guardado (o nuevo): mostrarlo como texto libre
        aiModelCombo.setText(current, juce::dontSendNotification);
    }
}

void SettingsModalComponent::refreshMidiDevices()
{
    midiConnectedDevices = midi.getConnectedDeviceNames();
    midiDevicesHeader.setText(juce::String::formatted("DISPOSITIVOS CONECTADOS (%d)", midiConnectedDevices.size()),
                              juce::dontSendNotification);
    repaint();
}

void SettingsModalComponent::updateTabVisibility()
{
    // General components (Tab 0)
    bool isGeneral = (activeTab == 0);
    languageSectionLabel.setVisible(isGeneral);
    languageSelectLabel.setVisible(isGeneral);
    languageCombo.setVisible(isGeneral);
    generalSectionDeck.setVisible(isGeneral);
    loadLockToggle.setVisible(isGeneral);
    autoplayOnLoadToggle.setVisible(isGeneral);
    jogModeLabel.setVisible(isGeneral);
    jogModeCombo.setVisible(isGeneral);
    pitchRangeLabel.setVisible(isGeneral);
    pitchRangeCombo.setVisible(isGeneral);
    autoCueToggle.setVisible(isGeneral);
    ytAutoplayToggle.setVisible(isGeneral);
    generalSectionDisplay.setVisible(isGeneral);
    waveformFpsLabel.setVisible(isGeneral);
    waveformFpsCombo.setVisible(isGeneral);
    recSectionLabel.setVisible(isGeneral);
    recFolderDescLabel.setVisible(isGeneral);
    recFolderEditor.setVisible(isGeneral);
    recBrowseBtn.setVisible(isGeneral);
    recOpenBtn.setVisible(isGeneral);
    recResetBtn.setVisible(isGeneral);

    downloadsSectionLabel.setVisible(isGeneral);
    saveDownloadsToggle.setVisible(isGeneral);
    downloadsFolderDescLabel.setVisible(isGeneral);
    downloadsFolderEditor.setVisible(isGeneral);
    downloadsBrowseBtn.setVisible(isGeneral);
    downloadsOpenBtn.setVisible(isGeneral);
    downloadsResetBtn.setVisible(isGeneral);

    if (isGeneral)
    {
        updateRecordingFolderUI();
        updateDownloadsFolderUI();
    }

    // Devices component (Tab 1)
    bool isDevices = (activeTab == 1);
    if (audioDeviceSelector != nullptr)
        audioDeviceSelector->setVisible(isDevices);
    routingSectionLabel.setVisible(isDevices);
    masterRoutingLabel.setVisible(isDevices);
    masterRoutingCombo.setVisible(isDevices);
    cueRoutingLabel.setVisible(isDevices);
    cueRoutingCombo.setVisible(isDevices);
    controllerDetectedBadge.setVisible(isDevices);
    if (isDevices)
    {
        bool is4Ch = audioEngine.is4ChannelOutputAvailable();
        if (is4Ch)
        {
            controllerDetectedBadge.setText(LanguageManager::trUtf8("● Controladora DJ 4 Canales Detectada (Master: Ch 1-2 • Auriculares: Ch 3-4)",
                                                                    "● 4-Channel DJ Controller Detected (Master: Ch 1-2 • Headphones: Ch 3-4)"), juce::dontSendNotification);
            controllerDetectedBadge.setColour(juce::Label::textColourId, juce::Colour::fromRGB(74, 222, 128));
        }
        else
        {
            controllerDetectedBadge.setText(LanguageManager::trUtf8("● Dispositivo estéreo (2 canales). Para preescucha independiente selecciona una tarjeta de 4 canales como DDJ-SB2",
                                                                    "● Stereo 2-channel device. For independent cueing select a 4-channel soundcard like DDJ-SB2"), juce::dontSendNotification);
            controllerDetectedBadge.setColour(juce::Label::textColourId, juce::Colour::fromRGB(148, 163, 184));
        }
    }

    // Sound components (Tab 2)
    bool isSound = (activeTab == 2);
    soundSectionEngine.setVisible(isSound);
    sampleRateInfoLabel.setVisible(isSound);
    bufferSizeLabel.setVisible(isSound);
    bufferSizeCombo.setVisible(isSound);
    soundSectionPhones.setVisible(isSound);
    cueMixLabel.setVisible(isSound);
    cueMixSlider.setVisible(isSound);
    cueVolumeLabel.setVisible(isSound);
    cueVolumeSlider.setVisible(isSound);
    soundSectionMixer.setVisible(isSound);
    eqModeLabel.setVisible(isSound);
    eqModeCombo.setVisible(isSound);
    crossfaderCurveLabel.setVisible(isSound);
    crossfaderCurveCombo.setVisible(isSound);
    soundSectionMaster.setVisible(isSound);
    headroomLabel.setVisible(isSound);
    headroomCombo.setVisible(isSound);
    masterLimiterToggle.setVisible(isSound);

    // MIDI components (Tab 3)
    bool isMidi = (activeTab == 3);
    midiTitleLabel.setVisible(isMidi);
    midiDescLabel.setVisible(isMidi);
    midiDevicesHeader.setVisible(isMidi);
    midiScanBtn.setVisible(isMidi);
    midiMonitorHeader.setVisible(isMidi);
    midiMonitorText.setVisible(isMidi);
    midiMappingHeader.setVisible(isMidi);
    if (midiMappingTable != nullptr)
        midiMappingTable->setVisible(isMidi);

    // Streaming components (Tab 5)
    bool isStreaming = (activeTab == 5);
    streamingSectionTitle.setVisible(isStreaming);
    streamingDescLabel.setVisible(isStreaming);
    apiKeyLabel.setVisible(isStreaming);
    apiKeyEditor.setVisible(isStreaming);
    apiStatusBadge.setVisible(isStreaming);
    apiInfoCard.setVisible(isStreaming);
    clearApiKeyBtn.setVisible(isStreaming);

    // Transmitir components (Tab 10)
    bool isTransmit = (activeTab == 10);
    transmitTitleLabel.setVisible(isTransmit);
    transmitDescLabel.setVisible(isTransmit);
    broadcastDestLabel.setVisible(isTransmit);
    broadcastDestCombo.setVisible(isTransmit);
    broadcastEnabledToggle.setVisible(isTransmit);
    broadcastStatusBadge.setVisible(isTransmit);
    podcastSectionLabel.setVisible(isTransmit);
    podcastDescLabel.setVisible(isTransmit);
    podcastRecCombo.setVisible(isTransmit);
    podcastRefreshBtn.setVisible(isTransmit);
    podcastBitrateLabel.setVisible(isTransmit);
    podcastBitrateCombo.setVisible(isTransmit);
    podcastExportBtn.setVisible(isTransmit);
    podcastStatusBadge.setVisible(isTransmit);

    if (isTransmit)
    {
        updateTransmitDestinationUI();
        refreshBroadcastStatus();
        refreshPodcastRecordings();
    }

    if (isStreaming)
    {
        bool hasKey = apiKeyEditor.getText().trim().isNotEmpty();
        if (hasKey)
        {
            apiStatusBadge.setText(LanguageManager::trUtf8("● YouTube Data API v3 Clave Guardada (Sin Restricciones)",
                                                          "● YouTube Data API v3 Key Saved (Unrestricted)"), juce::dontSendNotification);
            apiStatusBadge.setColour(juce::Label::textColourId, juce::Colour::fromRGB(74, 222, 128));
        }
        else
        {
            apiStatusBadge.setText(LanguageManager::trUtf8("● Scraping Público Activo (Sin Clave API)",
                                                          "● Public Scraping Active (No API Key)"), juce::dontSendNotification);
            apiStatusBadge.setColour(juce::Label::textColourId, juce::Colour::fromRGB(0, 229, 255));
        }
    }

    // AI Components (Tab 6)
    bool isAi = (activeTab == 6);
    aiSectionTitle.setVisible(isAi);
    aiDescLabel.setVisible(isAi);
    aiEnabledToggle.setVisible(isAi);
    aiProviderLabel.setVisible(isAi);
    aiProviderCombo.setVisible(isAi);
    aiModelLabel.setVisible(isAi);
    aiModelCombo.setVisible(isAi);
    aiApiKeyLabel.setVisible(isAi);
    aiApiKeyEditor.setVisible(isAi);
    aiStatusBadge.setVisible(isAi);
    aiInfoCard.setVisible(isAi);
    clearAiApiKeyBtn.setVisible(isAi);

    if (isAi)
    {
        bool hasKey = aiApiKeyEditor.getText().trim().isNotEmpty();
        bool enabled = aiEnabledToggle.getToggleState();
        if (hasKey && enabled)
        {
            auto badge = LanguageManager::trUtf8("● Recomendaciones IA Activas (", "● AI Recommendations Active (") + aiProviderCombo.getText() + " / " + aiModelCombo.getText().trim() + ")";
            if (!SecretStore::isAvailable())
                badge += LanguageManager::trUtf8(" · guardado local (sin almacén seguro)", " · local storage (no secure store)");
            aiStatusBadge.setText(badge, juce::dontSendNotification);
            aiStatusBadge.setColour(juce::Label::textColourId, juce::Colour::fromRGB(74, 222, 128));
        }
        else if (hasKey && !enabled)
        {
            aiStatusBadge.setText(LanguageManager::trUtf8("● Clave Guardada pero IA Desactivada", "● Key Saved but AI Disabled"), juce::dontSendNotification);
            aiStatusBadge.setColour(juce::Label::textColourId, juce::Colour::fromRGB(251, 191, 36));
        }
        else
        {
            aiStatusBadge.setText(LanguageManager::trUtf8("● Sin Clave API (Recomendaciones Desactivadas)", "● No API Key (Recommendations Disabled)"), juce::dontSendNotification);
            aiStatusBadge.setColour(juce::Label::textColourId, juce::Colour::fromRGB(148, 163, 184));
        }
    }

    // Donate components (Tab 7)
    bool isDonate = (activeTab == 7);
    donateTitleLabel.setVisible(isDonate);
    donateDescLabel.setVisible(isDonate);
    donateBtn.setVisible(isDonate);
    donateHintLabel.setVisible(isDonate);
    donateInfoCard.setVisible(isDonate);

    // Design components (Tab 8)
    bool isDesign = (activeTab == 8);
    designTitleLabel.setVisible(isDesign);
    designDescLabel.setVisible(isDesign);
    deckThemeLabel.setVisible(isDesign);
    deckTheme1Btn.setVisible(isDesign);
    deckTheme2Btn.setVisible(isDesign);
    waveLowLabel.setVisible(isDesign);
    waveMidLabel.setVisible(isDesign);
    waveHighLabel.setVisible(isDesign);
    waveLowSwatch.setVisible(isDesign);
    waveMidSwatch.setVisible(isDesign);
    waveHighSwatch.setVisible(isDesign);
    designPresetLabel.setVisible(isDesign);
    presetRgbBtn.setVisible(isDesign);
    presetNeonBtn.setVisible(isDesign);
    presetSilverBtn.setVisible(isDesign);

    // Version components (Tab 9)
    bool isVersion = (activeTab == 9);
    versionSectionTitle.setVisible(isVersion);
    versionAppTitle.setVisible(isVersion);
    versionStatusLabel.setVisible(isVersion);
    checkUpdatesBtn.setVisible(isVersion);
    versionChangeTitle.setVisible(isVersion);
    versionHelpIcon.setVisible(isVersion);
    versionSelectCombo.setVisible(isVersion);
    installVersionBtn.setVisible(isVersion);
    downloadStatusLabel.setVisible(isVersion && downloadStatusLabel.getText().isNotEmpty());
    downloadProgressBar.setVisible(isVersion && isDownloading);

    if (isVersion && availableReleases.empty() && !isCheckingUpdates)
    {
        checkUpdatesFromUI();
    }

    // Tab buttons styling
    auto updateBtnStyle = [](DjButton& btn, bool active) {
        if (active)
        {
            btn.setCustomColours(juce::Colour::fromRGB(34, 40, 56),
                                 juce::Colour::fromRGB(0, 180, 216),
                                 juce::Colour::fromRGB(0, 180, 216),
                                 juce::Colour::fromRGB(0, 180, 216).withAlpha(0.6f));
        }
        else
        {
            btn.setCustomColours(juce::Colours::transparentBlack,
                                 juce::Colour::fromRGB(140, 145, 160),
                                 juce::Colour::fromRGB(140, 145, 160));
        }
    };

    updateBtnStyle(tabGeneralBtn, activeTab == 0);
    updateBtnStyle(tabDevicesBtn, activeTab == 1);
    updateBtnStyle(tabSoundBtn, activeTab == 2);
    updateBtnStyle(tabMidiBtn, activeTab == 3);
    updateBtnStyle(tabShortcutsBtn, activeTab == 4);
    updateBtnStyle(tabStreamingBtn, activeTab == 5);
    updateBtnStyle(tabAIBtn, activeTab == 6);
    updateBtnStyle(tabDonateBtn, activeTab == 7);
    updateBtnStyle(tabDesignBtn, activeTab == 8);
    updateBtnStyle(tabVersionBtn, activeTab == 9);
    updateBtnStyle(tabTransmitBtn, activeTab == 10);
}

void SettingsModalComponent::applySettings()
{
    // Apply YouTube API key + video autoplay
    YouTubeService::setApiKey(apiKeyEditor.getText().trim());
    YouTubeService::setVideoAutoplayEnabled(ytAutoplayToggle.getToggleState());

    // Persistir la configuración de broadcast (la emisión en sí se controla con el toggle)
    persistBroadcastConfig();

    // Apply AI Settings
    AIService::setEnabled(aiEnabledToggle.getToggleState());
    AIProvider prov = providerFromComboId(aiProviderCombo.getSelectedId());
    AIService::setProvider(prov);
    AIService::setApiKey(prov, aiApiKeyEditor.getText().trim());
    AIService::setModel(prov, aiModelCombo.getText().trim());

    // Apply selected buffer size to audio engine device manager
    int id = bufferSizeCombo.getSelectedId();
    int targetBuffer = 512;
    if (id == 1) targetBuffer = 128;
    else if (id == 2) targetBuffer = 256;
    else if (id == 3) targetBuffer = 512;
    else if (id == 4) targetBuffer = 1024;

    auto setup = audioEngine.getDeviceManager().getAudioDeviceSetup();
    if (setup.bufferSize != targetBuffer)
    {
        setup.bufferSize = targetBuffer;
        audioEngine.getDeviceManager().setAudioDeviceSetup(setup, true);
    }

    if (onClose)
        onClose();
}

void SettingsModalComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // 1. Modal background and frame
    g.fillAll(juce::Colour::fromRGB(26, 29, 38));

    // Outer subtle border
    g.setColour(juce::Colour::fromRGB(45, 50, 66));
    g.drawRect(bounds, 1.0f);

    // 2. Sidebar (fondo más oscuro) + divisor vertical
    g.setColour(juce::Colour::fromRGB(20, 22, 31));
    g.fillRect(0.0f, 0.0f, 96.0f, bounds.getHeight());
    g.setColour(juce::Colour::fromRGB(38, 43, 58));
    g.drawVerticalLine(96, 0.0f, bounds.getHeight());

    // 3. Footer Bar (#14161f)
    auto footerArea = getLocalBounds().removeFromBottom(48);
    g.setColour(juce::Colour::fromRGB(20, 22, 31));
    g.fillRect(footerArea);

    g.setColour(juce::Colour::fromRGB(38, 43, 58));
    g.drawHorizontalLine(footerArea.getY(), 0.0f, bounds.getWidth());

    // 5. If activeTab == 3 (MIDI), paint the device list box
    // (cajas en coords de bodyContent -> compensar scroll del viewport)
    const auto contentOffset = bodyViewport.getPosition() - bodyViewport.getViewPosition();
    if (activeTab == 3 && !midiDeviceListBox.isEmpty())
    {
        auto devBox = (midiDeviceListBox + contentOffset).toFloat();
        if (midiConnectedDevices.isEmpty())
        {
            g.setColour(juce::Colour::fromRGB(17, 18, 25));
            g.fillRoundedRectangle(devBox, 8.0f);
            g.setColour(juce::Colour::fromRGB(40, 44, 60));
            g.drawRoundedRectangle(devBox, 8.0f, 1.0f);

            g.setColour(juce::Colour::fromRGB(255, 159, 28));
            g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
            g.drawText(LanguageManager::trUtf8("No se detectó ningún controlador", "No controller detected"),
                       devBox.removeFromTop(75.0f), juce::Justification::centred, false);

            g.setColour(juce::Colour::fromRGB(120, 125, 140));
            g.setFont(juce::FontOptions(10.5f, juce::Font::plain));
            g.drawText(LanguageManager::trUtf8("Conecta un dispositivo USB DJ y pulsa 'Escanear Puertos'", "Connect a USB DJ device and click 'Scan Ports'"),
                       devBox, juce::Justification::centred, false);
        }
        else
        {
            g.setColour(juce::Colour::fromRGB(17, 18, 25));
            g.fillRoundedRectangle(devBox, 8.0f);
            g.setColour(juce::Colour::fromRGB(40, 44, 60));
            g.drawRoundedRectangle(devBox, 8.0f, 1.0f);

            auto listArea = devBox.reduced(10.0f);
            for (int i = 0; i < midiConnectedDevices.size(); ++i)
            {
                auto row = listArea.removeFromTop(24.0f);
                g.setColour(juce::Colour::fromRGB(46, 196, 182));
                LucideIcons::draw(g, LucideIcons::IconType::CircleDot,
                                  juce::Rectangle<float>(row.getX(), row.getY() + 4.0f, 12.0f, 12.0f),
                                  juce::Colour::fromRGB(46, 196, 182), 1.6f);
                g.setColour(juce::Colours::white);
                g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
                g.drawText(midiConnectedDevices[i], row.removeFromRight(row.getWidth() - 20.0f),
                           juce::Justification::centredLeft, false);
            }
        }
    }

    // 6. If activeTab == 4 (Shortcuts), paint the keyboard shortcuts cards
    if (activeTab == 4)
    {
        auto contentArea = getLocalBounds();
        contentArea.removeFromTop(10);
        contentArea.removeFromBottom(54);
        contentArea.removeFromLeft(96); // sidebar
        contentArea.reduce(20, 10);

        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        g.drawText(LanguageManager::trUtf8("Atajos de Teclado Principales de DiscNativePro:", "DiscNativePro Primary Keyboard Shortcuts:"),
                   contentArea.removeFromTop(24), juce::Justification::centredLeft, false);

        contentArea.removeFromTop(8);

        // Deck 1 & Deck 2 cards row
        auto decksRow = contentArea.removeFromTop(160);
        int cardWidth = (decksRow.getWidth() - 14) / 2;

        auto d1Card = decksRow.removeFromLeft(cardWidth);
        decksRow.removeFromLeft(14);
        auto d2Card = decksRow;

        auto drawCard = [&g](juce::Rectangle<int> r, const juce::String& title, juce::Colour col,
                             const std::vector<std::pair<juce::String, juce::String>>& keys) {
            g.setColour(juce::Colour::fromRGB(18, 20, 26));
            g.fillRoundedRectangle(r.toFloat(), 6.0f);
            g.setColour(juce::Colour::fromRGB(37, 40, 52));
            g.drawRoundedRectangle(r.toFloat(), 6.0f, 1.0f);

            auto inner = r.reduced(10, 8);
            g.setColour(col);
            g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
            g.drawText(title, inner.removeFromTop(18), juce::Justification::centredLeft, false);
            inner.removeFromTop(4);

            g.setFont(juce::FontOptions(11.0f, juce::Font::plain));
            for (const auto& item : keys)
            {
                auto row = inner.removeFromTop(22);
                juce::GlyphArrangement ga;
                ga.addLineOfText(g.getCurrentFont(), item.first, 0.0f, 0.0f);
                int kbdW = juce::jmax(36, static_cast<int>(ga.getBoundingBox(0, -1, true).getWidth()) + 14);
                auto kbdRect = row.removeFromLeft(kbdW).reduced(0, 2);
                g.setColour(juce::Colour::fromRGB(36, 39, 53));
                g.fillRoundedRectangle(kbdRect.toFloat(), 3.0f);
                g.setColour(juce::Colours::white);
                g.drawText(item.first, kbdRect, juce::Justification::centred, false);

                row.removeFromLeft(8);
                g.setColour(juce::Colour::fromRGB(200, 205, 215));
                g.drawText(item.second, row, juce::Justification::centredLeft, false);
            }
        };

        drawCard(d1Card, LanguageManager::trUtf8("Deck 1 (Canal Izquierdo):", "Deck 1 (Left Channel):"), juce::Colour::fromRGB(0, 180, 216), {
            { "Q", "Play / Pause" },
            { "W", LanguageManager::trUtf8("CUE (Pre-escucha)", "CUE (Cueing)") },
            { "E", LanguageManager::trUtf8("SYNC (Sincronizar BPM)", "SYNC (Sync BPM)") },
            { "1-4", LanguageManager::trUtf8("Hot Cues 1 a 4", "Hot Cues 1 to 4") }
        });

        drawCard(d2Card, LanguageManager::trUtf8("Deck 2 (Canal Derecho):", "Deck 2 (Right Channel):"), juce::Colour::fromRGB(148, 163, 184), {
            { "P", "Play / Pause" },
            { "O", LanguageManager::trUtf8("CUE (Pre-escucha)", "CUE (Cueing)") },
            { "I", LanguageManager::trUtf8("SYNC (Sincronizar BPM)", "SYNC (Sync BPM)") },
            { "7-0", LanguageManager::trUtf8("Hot Cues 1 a 4", "Hot Cues 1 to 4") }
        });

        contentArea.removeFromTop(12);

        // Mixer & General Card
        auto mixerCard = contentArea.removeFromTop(122);
        drawCard(mixerCard, LanguageManager::trUtf8("Mezclador & Control Global:", "Mixer & Global Control:"), juce::Colours::white, {
            { juce::String::fromUTF8("⌘R / Ctrl+R"), LanguageManager::trUtf8("Iniciar / Detener Grabación de Sesión (REC)", "Start / Stop Session Recording (REC)") },
            { "X | C | V",   LanguageManager::trUtf8("Crossfader: Izquierda | Centro | Derecha", "Crossfader: Left | Center | Right") },
            { "Espacio",     LanguageManager::trUtf8("Activar / Desactivar Scratch en Deck activo", "Toggle Scratch on Active Deck") }
        });
    }

    // 7. If activeTab == 5 (Streaming), paint info card background
    if (activeTab == 5 && !streamingInfoCardBox.isEmpty())
    {
        auto cardArea = (streamingInfoCardBox + contentOffset).toFloat();
        g.setColour(juce::Colour::fromRGB(18, 20, 27));
        g.fillRoundedRectangle(cardArea, 8.0f);
        g.setColour(juce::Colour::fromRGB(40, 44, 60));
        g.drawRoundedRectangle(cardArea, 8.0f, 1.0f);
    }

    // 8. If activeTab == 6 (AI Match), paint info card background
    if (activeTab == 6 && !aiInfoCardBox.isEmpty())
    {
        auto cardArea = (aiInfoCardBox + contentOffset).toFloat();
        g.setColour(juce::Colour::fromRGB(18, 20, 27));
        g.fillRoundedRectangle(cardArea, 8.0f);
        g.setColour(juce::Colour::fromRGB(40, 44, 60));
        g.drawRoundedRectangle(cardArea, 8.0f, 1.0f);
    }

    // 9. If activeTab == 7 (Donar), paint info card background
    if (activeTab == 7 && !donateCardBox.isEmpty())
    {
        auto cardArea = (donateCardBox + contentOffset).toFloat();
        g.setColour(juce::Colour::fromRGB(18, 20, 27));
        g.fillRoundedRectangle(cardArea, 8.0f);
        g.setColour(juce::Colour::fromRGB(40, 44, 60));
        g.drawRoundedRectangle(cardArea, 8.0f, 1.0f);
    }
}

void SettingsModalComponent::resized()
{
    // 1. Sidebar vertical de pestañas (icono arriba + etiqueta abajo).
    //    Botones 80x44 apilados; scroll vertical solo si no caben en la altura.
    constexpr int sidebarW = 96;
    constexpr int tabBtnW = 80;
    constexpr int tabBtnH = 44;
    constexpr int tabGap = 4;

    auto sidebarRect = getLocalBounds();
    sidebarRect.removeFromBottom(52); // footer
    sidebarRect = sidebarRect.removeFromLeft(sidebarW); // el sidebar ES la franja izquierda
    sideViewport.setBounds(sidebarRect);

    const int tabsContentH = 4 + 11 * tabBtnH + 10 * tabGap + 4;
    sideContent.setBounds(0, 0, sidebarW, juce::jmax(tabsContentH, sidebarRect.getHeight()));

    const int tabBtnX = (sidebarW - tabBtnW) / 2;
    tabGeneralBtn.setBounds(tabBtnX, 4, tabBtnW, tabBtnH);
    tabDevicesBtn.setBounds(tabBtnX, 4 + (tabBtnH + tabGap) * 1, tabBtnW, tabBtnH);
    tabSoundBtn.setBounds(tabBtnX, 4 + (tabBtnH + tabGap) * 2, tabBtnW, tabBtnH);
    tabMidiBtn.setBounds(tabBtnX, 4 + (tabBtnH + tabGap) * 3, tabBtnW, tabBtnH);
    tabShortcutsBtn.setBounds(tabBtnX, 4 + (tabBtnH + tabGap) * 4, tabBtnW, tabBtnH);
    tabStreamingBtn.setBounds(tabBtnX, 4 + (tabBtnH + tabGap) * 5, tabBtnW, tabBtnH);
    tabAIBtn.setBounds(tabBtnX, 4 + (tabBtnH + tabGap) * 6, tabBtnW, tabBtnH);
    tabDonateBtn.setBounds(tabBtnX, 4 + (tabBtnH + tabGap) * 7, tabBtnW, tabBtnH);
    tabDesignBtn.setBounds(tabBtnX, 4 + (tabBtnH + tabGap) * 8, tabBtnW, tabBtnH);
    tabVersionBtn.setBounds(tabBtnX, 4 + (tabBtnH + tabGap) * 9, tabBtnW, tabBtnH);
    tabTransmitBtn.setBounds(tabBtnX, 4 + (tabBtnH + tabGap) * 10, tabBtnW, tabBtnH);

    // 2. Tab Body Area (derecha del sidebar) con scroll (el contenido vive en bodyContent)
    auto bodyRect = getLocalBounds();
    bodyRect.removeFromLeft(sidebarW);
    bodyRect.removeFromBottom(52);
    bodyViewport.setBounds(bodyRect);

    // Layout a un ancho dado. La altura necesaria no depende del ancho (filas fijas),
    // así que se mide una vez y se re-ejecuta solo si aparece la scrollbar.
    auto layoutTabs = [&](int contentW) {
        juce::Rectangle<int> body(0, 0, contentW, 3000);
        body.reduce(20, 10);

    // --- Tab 0: General Layout (Unconditionally set bounds so they are always ready) ---
    {
        auto gArea = body;

        // Section 0: Idioma / Language
        languageSectionLabel.setBounds(gArea.removeFromTop(18));
        gArea.removeFromTop(4);
        auto langRow = gArea.removeFromTop(24);
        languageSelectLabel.setBounds(langRow.removeFromLeft(160));
        languageCombo.setBounds(langRow);
        gArea.removeFromTop(8);

        generalSectionDeck.setBounds(gArea.removeFromTop(18));
        gArea.removeFromTop(4);
        loadLockToggle.setBounds(gArea.removeFromTop(22));
        gArea.removeFromTop(4);
        autoplayOnLoadToggle.setBounds(gArea.removeFromTop(22));
        gArea.removeFromTop(4);

        auto row1 = gArea.removeFromTop(24);
        jogModeLabel.setBounds(row1.removeFromLeft(160));
        jogModeCombo.setBounds(row1);
        gArea.removeFromTop(4);

        auto row2 = gArea.removeFromTop(24);
        pitchRangeLabel.setBounds(row2.removeFromLeft(160));
        pitchRangeCombo.setBounds(row2);
        gArea.removeFromTop(4);

        autoCueToggle.setBounds(gArea.removeFromTop(22));
        gArea.removeFromTop(4);

        ytAutoplayToggle.setBounds(gArea.removeFromTop(22));
        gArea.removeFromTop(8);

        generalSectionDisplay.setBounds(gArea.removeFromTop(18));
        gArea.removeFromTop(4);

        auto row3 = gArea.removeFromTop(24);
        waveformFpsLabel.setBounds(row3.removeFromLeft(220));
        waveformFpsCombo.setBounds(row3);
        gArea.removeFromTop(8);

        // Section 3: Grabación de Sesiones
        recSectionLabel.setBounds(gArea.removeFromTop(18));
        gArea.removeFromTop(4);

        recFolderDescLabel.setBounds(gArea.removeFromTop(16));
        gArea.removeFromTop(4);

        auto recRow = gArea.removeFromTop(24);
        recResetBtn.setBounds(recRow.removeFromRight(82));
        recRow.removeFromRight(6);
        recOpenBtn.setBounds(recRow.removeFromRight(102));
        recRow.removeFromRight(6);
        recBrowseBtn.setBounds(recRow.removeFromRight(96));
        recRow.removeFromRight(6);
        recFolderEditor.setBounds(recRow);
        gArea.removeFromTop(12);

        // Section 4: Descargas de YouTube y Archivos Locales
        downloadsSectionLabel.setBounds(gArea.removeFromTop(18));
        gArea.removeFromTop(4);

        saveDownloadsToggle.setBounds(gArea.removeFromTop(22));
        gArea.removeFromTop(4);

        downloadsFolderDescLabel.setBounds(gArea.removeFromTop(16));
        gArea.removeFromTop(4);

        auto dlRow = gArea.removeFromTop(24);
        downloadsResetBtn.setBounds(dlRow.removeFromRight(82));
        dlRow.removeFromRight(6);
        downloadsOpenBtn.setBounds(dlRow.removeFromRight(102));
        dlRow.removeFromRight(6);
        downloadsBrowseBtn.setBounds(dlRow.removeFromRight(96));
        dlRow.removeFromRight(6);
        downloadsFolderEditor.setBounds(dlRow);
        gArea.removeFromTop(12);

        appInfoLabel.setBounds(gArea.removeFromTop(18));
    }

    // --- Tab 1: Devices Layout (Unconditionally set bounds) ---
    {
        auto dArea = body;
        if (audioDeviceSelector != nullptr)
        {
            audioDeviceSelector->setBounds(dArea.removeFromTop(200));
        }
        dArea.removeFromTop(8);

        routingSectionLabel.setBounds(dArea.removeFromTop(18));
        dArea.removeFromTop(4);

        auto mRow = dArea.removeFromTop(24);
        masterRoutingLabel.setBounds(mRow.removeFromLeft(220));
        masterRoutingCombo.setBounds(mRow);
        dArea.removeFromTop(4);

        auto cRow = dArea.removeFromTop(24);
        cueRoutingLabel.setBounds(cRow.removeFromLeft(220));
        cueRoutingCombo.setBounds(cRow);
        dArea.removeFromTop(8);

        controllerDetectedBadge.setBounds(dArea.removeFromTop(20));
    }

    // --- Tab 2: Sound Layout (Unconditionally set bounds) ---
    {
        auto sArea = body;

        soundSectionEngine.setBounds(sArea.removeFromTop(18));
        sArea.removeFromTop(2);
        sampleRateInfoLabel.setBounds(sArea.removeFromTop(18));
        sArea.removeFromTop(4);

        auto bRow = sArea.removeFromTop(22);
        bufferSizeLabel.setBounds(bRow.removeFromLeft(220));
        bufferSizeCombo.setBounds(bRow);
        sArea.removeFromTop(8);

        soundSectionPhones.setBounds(sArea.removeFromTop(18));
        sArea.removeFromTop(4);

        auto cmRow = sArea.removeFromTop(22);
        cueMixLabel.setBounds(cmRow.removeFromLeft(220));
        cueMixSlider.setBounds(cmRow);
        sArea.removeFromTop(4);

        auto cvRow = sArea.removeFromTop(22);
        cueVolumeLabel.setBounds(cvRow.removeFromLeft(220));
        cueVolumeSlider.setBounds(cvRow);
        sArea.removeFromTop(8);

        soundSectionMixer.setBounds(sArea.removeFromTop(18));
        sArea.removeFromTop(4);

        auto eqRow = sArea.removeFromTop(22);
        eqModeLabel.setBounds(eqRow.removeFromLeft(220));
        eqModeCombo.setBounds(eqRow);
        sArea.removeFromTop(4);

        auto cfRow = sArea.removeFromTop(22);
        crossfaderCurveLabel.setBounds(cfRow.removeFromLeft(220));
        crossfaderCurveCombo.setBounds(cfRow);
        sArea.removeFromTop(8);

        soundSectionMaster.setBounds(sArea.removeFromTop(18));
        sArea.removeFromTop(4);

        auto hdRow = sArea.removeFromTop(22);
        headroomLabel.setBounds(hdRow.removeFromLeft(220));
        headroomCombo.setBounds(hdRow);
        sArea.removeFromTop(4);

        masterLimiterToggle.setBounds(sArea.removeFromTop(22));
    }

    // --- Tab 3: MIDI Layout (Unconditionally set bounds) ---
    {
        auto mArea = body;
        midiTitleLabel.setBounds(mArea.removeFromTop(22));
        mArea.removeFromTop(4);
        midiDescLabel.setBounds(mArea.removeFromTop(36));
        mArea.removeFromTop(10);

        auto devHeaderRow = mArea.removeFromTop(26);
        midiDevicesHeader.setBounds(devHeaderRow.removeFromLeft(250));
        midiScanBtn.setBounds(devHeaderRow.removeFromRight(130));

        mArea.removeFromTop(8);
        midiDeviceListBox = mArea.removeFromTop(110);
        mArea.removeFromTop(12);

        midiMonitorHeader.setBounds(mArea.removeFromTop(18));
        mArea.removeFromTop(2);
        midiMonitorText.setBounds(mArea.removeFromTop(24));
        mArea.removeFromTop(16);

        midiMappingHeader.setBounds(mArea.removeFromTop(20));
        mArea.removeFromTop(8);
        if (midiMappingTable != nullptr)
            midiMappingTable->setBounds(mArea.removeFromTop(380));
    }

    // --- Tab 5: Streaming Layout (Unconditionally set bounds) ---
    {
        auto stArea = body;
        streamingSectionTitle.setBounds(stArea.removeFromTop(22));
        stArea.removeFromTop(4);
        streamingDescLabel.setBounds(stArea.removeFromTop(38));
        stArea.removeFromTop(12);

        apiKeyLabel.setBounds(stArea.removeFromTop(20));
        stArea.removeFromTop(4);

        auto keyRow = stArea.removeFromTop(28);
        clearApiKeyBtn.setBounds(keyRow.removeFromRight(90));
        keyRow.removeFromRight(8);
        apiKeyEditor.setBounds(keyRow);

        stArea.removeFromTop(8);
        apiStatusBadge.setBounds(stArea.removeFromTop(20));
        stArea.removeFromTop(12);

        streamingInfoCardBox = stArea.removeFromTop(120);
        apiInfoCard.setBounds(streamingInfoCardBox.reduced(12, 10));
    }

    // --- Tab 10: Transmitir Layout (radio + RTMP + podcasts) ---
    {
        auto tArea = body;
        transmitTitleLabel.setBounds(tArea.removeFromTop(22));
        tArea.removeFromTop(4);
        transmitDescLabel.setBounds(tArea.removeFromTop(30));
        tArea.removeFromTop(10);

        auto destRow = tArea.removeFromTop(26);
        broadcastDestLabel.setBounds(destRow.removeFromLeft(170));
        destRow.removeFromLeft(8);
        broadcastDestCombo.setBounds(destRow);
        tArea.removeFromTop(10);

        // Radio propia (Icecast)
        broadcastSectionTitle.setBounds(tArea.removeFromTop(20));
        tArea.removeFromTop(2);
        broadcastDescLabel.setBounds(tArea.removeFromTop(16));
        tArea.removeFromTop(4);

        auto r1 = tArea.removeFromTop(26);
        broadcastHostLabel.setBounds(r1.removeFromLeft(170));
        r1.removeFromLeft(8);
        broadcastHostEditor.setBounds(r1);
        tArea.removeFromTop(6);

        auto r2 = tArea.removeFromTop(28);
        broadcastPortLabel.setBounds(r2.removeFromLeft(170));
        r2.removeFromLeft(8);
        broadcastPortEditor.setBounds(r2.removeFromLeft(100));
        r2.removeFromLeft(16);
        broadcastMountLabel.setBounds(r2.removeFromLeft(100));
        r2.removeFromLeft(8);
        broadcastMountEditor.setBounds(r2);
        tArea.removeFromTop(6);

        auto r3 = tArea.removeFromTop(28);
        broadcastPasswordLabel.setBounds(r3.removeFromLeft(170));
        r3.removeFromLeft(8);
        broadcastPasswordEditor.setBounds(r3.removeFromLeft(200));
        r3.removeFromLeft(16);
        broadcastNameLabel.setBounds(r3.removeFromLeft(120));
        r3.removeFromLeft(8);
        broadcastNameEditor.setBounds(r3);
        tArea.removeFromTop(6);

        auto r4 = tArea.removeFromTop(24);
        broadcastBitrateLabel.setBounds(r4.removeFromLeft(170));
        r4.removeFromLeft(8);
        broadcastBitrateCombo.setBounds(r4.removeFromLeft(120));

        // RTMP (YouTube / Facebook / Twitch / personalizado)
        auto rt1 = tArea.removeFromTop(26);
        rtmpUrlLabel.setBounds(rt1.removeFromLeft(170));
        rt1.removeFromLeft(8);
        rtmpUrlEditor.setBounds(rt1);
        tArea.removeFromTop(6);

        auto rt2 = tArea.removeFromTop(26);
        rtmpKeyLabel.setBounds(rt2.removeFromLeft(170));
        rt2.removeFromLeft(8);
        rtmpKeyEditor.setBounds(rt2);
        tArea.removeFromTop(6);

        auto rt3 = tArea.removeFromTop(26);
        rtmpBitrateLabel.setBounds(rt3.removeFromLeft(170));
        rt3.removeFromLeft(8);
        rtmpBitrateCombo.setBounds(rt3.removeFromLeft(120));
        rt3.removeFromLeft(16);
        installFfmpegBtn.setBounds(rt3.removeFromLeft(130));
        tArea.removeFromTop(4);
        ffmpegStatusBadge.setBounds(tArea.removeFromTop(16));
        tArea.removeFromTop(10);

        // Emisión (toggle + estado)
        broadcastEnabledToggle.setBounds(tArea.removeFromTop(24));
        tArea.removeFromTop(6);
        broadcastStatusBadge.setBounds(tArea.removeFromTop(20));
        tArea.removeFromTop(14);

        // Podcast (exportar grabación a MP3)
        podcastSectionLabel.setBounds(tArea.removeFromTop(20));
        tArea.removeFromTop(2);
        podcastDescLabel.setBounds(tArea.removeFromTop(16));
        tArea.removeFromTop(4);

        auto p1 = tArea.removeFromTop(26);
        podcastRefreshBtn.setBounds(p1.removeFromRight(110));
        p1.removeFromRight(8);
        podcastRecCombo.setBounds(p1);
        tArea.removeFromTop(6);

        auto p2 = tArea.removeFromTop(26);
        podcastBitrateLabel.setBounds(p2.removeFromLeft(170));
        p2.removeFromLeft(8);
        podcastBitrateCombo.setBounds(p2.removeFromLeft(120));
        p2.removeFromLeft(16);
        podcastExportBtn.setBounds(p2.removeFromLeft(150));
        tArea.removeFromTop(6);
        podcastStatusBadge.setBounds(tArea.removeFromTop(18));
    }

    // --- Tab 6: AI Match Layout (Unconditionally set bounds) ---
    {
        auto aiArea = body;
        aiSectionTitle.setBounds(aiArea.removeFromTop(22));
        aiArea.removeFromTop(4);
        aiDescLabel.setBounds(aiArea.removeFromTop(34));
        aiArea.removeFromTop(8);

        aiEnabledToggle.setBounds(aiArea.removeFromTop(24));
        aiArea.removeFromTop(8);

        auto provRow = aiArea.removeFromTop(26);
        aiProviderLabel.setBounds(provRow.removeFromLeft(210));
        aiProviderCombo.setBounds(provRow);
        aiArea.removeFromTop(8);

        auto modelRow = aiArea.removeFromTop(26);
        aiModelLabel.setBounds(modelRow.removeFromLeft(210));
        aiModelCombo.setBounds(modelRow);
        aiArea.removeFromTop(8);

        aiApiKeyLabel.setBounds(aiArea.removeFromTop(20));
        aiArea.removeFromTop(4);

        auto keyRow = aiArea.removeFromTop(28);
        clearAiApiKeyBtn.setBounds(keyRow.removeFromRight(90));
        keyRow.removeFromRight(8);
        aiApiKeyEditor.setBounds(keyRow);

        aiArea.removeFromTop(8);
        aiStatusBadge.setBounds(aiArea.removeFromTop(20));
        aiArea.removeFromTop(12);

        aiInfoCardBox = aiArea.removeFromTop(84);
        aiInfoCard.setBounds(aiInfoCardBox.reduced(12, 10));
    }

    // --- Tab 7: Donar Layout (Unconditionally set bounds) ---
    {
        auto dArea = body;
        donateTitleLabel.setBounds(dArea.removeFromTop(22));
        dArea.removeFromTop(4);
        donateDescLabel.setBounds(dArea.removeFromTop(44));
        dArea.removeFromTop(12);

        auto btnRow = dArea.removeFromTop(36);
        int btnW = 300;
        donateBtn.setBounds(btnRow.withSizeKeepingCentre(btnW, 36));
        dArea.removeFromTop(6);

        donateHintLabel.setBounds(dArea.removeFromTop(18));
        dArea.removeFromTop(12);

        donateCardBox = dArea.removeFromTop(110);
        donateInfoCard.setBounds(donateCardBox.reduced(12, 10));
    }

    // --- Tab 8: Diseño Layout (Unconditionally set bounds) ---
    {
        auto tArea = body;
        designTitleLabel.setBounds(tArea.removeFromTop(22));
        tArea.removeFromTop(4);
        designDescLabel.setBounds(tArea.removeFromTop(34));
        tArea.removeFromTop(12);

        auto deckRow = tArea.removeFromTop(28);
        deckThemeLabel.setBounds(deckRow.removeFromLeft(210));
        deckTheme1Btn.setBounds(deckRow.removeFromLeft(140));
        deckRow.removeFromLeft(8);
        deckTheme2Btn.setBounds(deckRow.removeFromLeft(140));
        tArea.removeFromTop(8);

        auto lowRow = tArea.removeFromTop(28);
        waveLowLabel.setBounds(lowRow.removeFromLeft(210));
        waveLowSwatch.setBounds(lowRow);
        tArea.removeFromTop(8);

        auto midRow = tArea.removeFromTop(28);
        waveMidLabel.setBounds(midRow.removeFromLeft(210));
        waveMidSwatch.setBounds(midRow);
        tArea.removeFromTop(8);

        auto highRow = tArea.removeFromTop(28);
        waveHighLabel.setBounds(highRow.removeFromLeft(210));
        waveHighSwatch.setBounds(highRow);
        tArea.removeFromTop(12);

        designPresetLabel.setBounds(tArea.removeFromTop(20));
        tArea.removeFromTop(4);

        auto presetRow = tArea.removeFromTop(28);
        int presetW = (presetRow.getWidth() - 16) / 3;
        presetRgbBtn.setBounds(presetRow.removeFromLeft(presetW));
        presetRow.removeFromLeft(8);
        presetNeonBtn.setBounds(presetRow.removeFromLeft(presetW));
        presetRow.removeFromLeft(8);
        presetSilverBtn.setBounds(presetRow);
    }

    // --- Tab 9: Versión Layout (Unconditionally set bounds) ---
    {
        auto vArea = body;
        versionSectionTitle.setBounds(vArea.removeFromTop(20));
        vArea.removeFromTop(4);

        versionAppTitle.setBounds(vArea.removeFromTop(32));
        vArea.removeFromTop(2);

        versionStatusLabel.setBounds(vArea.removeFromTop(22));
        vArea.removeFromTop(12);

        checkUpdatesBtn.setBounds(vArea.removeFromTop(32).removeFromLeft(180));
        vArea.removeFromTop(24);

        // Section: Cambiar de versión
        auto titleRow = vArea.removeFromTop(22);
        versionChangeTitle.setBounds(titleRow.removeFromLeft(160));
        versionHelpIcon.setBounds(titleRow.removeFromLeft(30));
        vArea.removeFromTop(8);

        auto actionRow = vArea.removeFromTop(34);
        int installBtnW = 120;
        int comboW = juce::jmax(200, actionRow.getWidth() - installBtnW - 12);
        versionSelectCombo.setBounds(actionRow.removeFromLeft(comboW));
        actionRow.removeFromLeft(12);
        installVersionBtn.setBounds(actionRow.removeFromLeft(installBtnW));
        vArea.removeFromTop(12);

        downloadProgressBar.setBounds(vArea.removeFromTop(8));
        vArea.removeFromTop(4);
        downloadStatusLabel.setBounds(vArea.removeFromTop(20));
    }

    }; // layoutTabs

    int availW = juce::jmax(100, bodyViewport.getWidth());
    int availH = juce::jmax(100, bodyViewport.getHeight());
    layoutTabs(availW);
    int neededH = 20;
    for (auto* c : bodyContent.getChildren())
        if (c->isVisible())
            neededH = juce::jmax(neededH, c->getBottom() + 12);
    int contentW = availW;
    if (neededH > availH)
    {
        contentW = juce::jmax(100, availW - bodyViewport.getScrollBarThickness());
        layoutTabs(contentW);
    }
    bodyContent.setBounds(0, 0, contentW, juce::jmax(neededH, availH));

    // 3. Footer buttons
    int footerY = getHeight() - 38;
    applyButton.setBounds(getWidth() - 96, footerY, 80, 26);
    undoButton.setBounds(getWidth() - 186, footerY, 80, 26);
}

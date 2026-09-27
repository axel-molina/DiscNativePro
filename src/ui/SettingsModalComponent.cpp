#include "SettingsModalComponent.h"
#include "../services/YouTubeService.h"

SettingsModalComponent::SettingsModalComponent(AudioEngine& engine, MidiManager& midiMgr)
    : audioEngine(engine), midi(midiMgr)
{
    setSize(640, 540);

    // 1. Close traffic light button
    closeDotButton.setButtonText("");
    closeDotButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(255, 95, 86));
    closeDotButton.onClick = [this]() { if (onClose) onClose(); };
    addAndMakeVisible(closeDotButton);

    headerTitleLabel.setText("Preferencias de DiscNativePro", juce::dontSendNotification);
    headerTitleLabel.setFont(juce::FontOptions(12.5f, juce::Font::bold));
    headerTitleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(headerTitleLabel);

    // 2. Tab Buttons (6 tabs)
    auto setupTabBtn = [this](DjButton& btn, LucideIcons::IconType icon, const juce::String& text, int tabIdx) {
        btn.setIcon(icon, 14.0f);
        btn.setText(text);
        btn.setFontSize(10.5f);
        btn.setCornerRadius(6.0f);
        btn.onClick = [this, tabIdx]() {
            activeTab = tabIdx;
            updateTabVisibility();
            resized();
            repaint();
        };
        addAndMakeVisible(btn);
    };

    setupTabBtn(tabGeneralBtn, LucideIcons::IconType::Settings, "General", 0);
    setupTabBtn(tabDevicesBtn, LucideIcons::IconType::Headphones, "Dispositivos", 1);
    setupTabBtn(tabSoundBtn, LucideIcons::IconType::Volume2, "Sonido", 2);
    setupTabBtn(tabMidiBtn, LucideIcons::IconType::Zap, "MIDI", 3);
    setupTabBtn(tabShortcutsBtn, LucideIcons::IconType::Keyboard, "Shortcuts", 4);
    setupTabBtn(tabStreamingBtn, LucideIcons::IconType::Tv, "Streaming", 5);

    // 3. Setup Tab Contents
    setupGeneralTab();
    setupDevicesTab();
    setupSoundTab();
    setupMidiTab();
    setupStreamingTab();

    // 4. Footer controls
    helpButton.setText("?");
    helpButton.setFontSize(11.0f);
    helpButton.setCornerRadius(4.0f);
    helpButton.setCustomColours(juce::Colour::fromRGB(32, 35, 48),
                                juce::Colour::fromRGB(150, 155, 170),
                                juce::Colour::fromRGB(150, 155, 170));
    addAndMakeVisible(helpButton);

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

    updateTabVisibility();
}

SettingsModalComponent::~SettingsModalComponent()
{
    midi.onMidiActivity = nullptr;
}

void SettingsModalComponent::setupGeneralTab()
{
    // Section 1: Comportamiento de Decks
    generalSectionDeck.setText(juce::String::fromUTF8("Comportamiento de Decks"), juce::dontSendNotification);
    generalSectionDeck.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    generalSectionDeck.setColour(juce::Label::textColourId, juce::Colour::fromRGB(0, 180, 216));
    addChildComponent(generalSectionDeck);

    loadLockToggle.setButtonText(juce::String::fromUTF8("Protección de carga (Load Lock): Bloquear carga si el deck reproduce en vivo"));
    loadLockToggle.setToggleState(false, juce::dontSendNotification);
    loadLockToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::white.withAlpha(0.85f));
    addChildComponent(loadLockToggle);

    jogModeLabel.setText(juce::String::fromUTF8("Modo Jog Wheel:"), juce::dontSendNotification);
    jogModeLabel.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    jogModeLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(160, 165, 180));
    addChildComponent(jogModeLabel);

    jogModeCombo.addItem(juce::String::fromUTF8("Vinilo (Scratch & Pitch Bend al borde)"), 1);
    jogModeCombo.addItem(juce::String::fromUTF8("CDJ (Pitch Bend continuo)"), 2);
    jogModeCombo.setSelectedId(1, juce::dontSendNotification);
    addChildComponent(jogModeCombo);

    pitchRangeLabel.setText(juce::String::fromUTF8("Rango de Pitch por defecto:"), juce::dontSendNotification);
    pitchRangeLabel.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    pitchRangeLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(160, 165, 180));
    addChildComponent(pitchRangeLabel);

    pitchRangeCombo.addItem(juce::String::fromUTF8("±8% (Precisión estándar Club)"), 1);
    pitchRangeCombo.addItem(juce::String::fromUTF8("±16% (Rango extendido)"), 2);
    pitchRangeCombo.addItem(juce::String::fromUTF8("±50% (Extremo / Transiciones abiertas)"), 3);
    pitchRangeCombo.setSelectedId(1, juce::dontSendNotification);
    addChildComponent(pitchRangeCombo);

    autoCueToggle.setButtonText(juce::String::fromUTF8("Auto-CUE: Posicionar cabezal en el primer beat al cargar pista"));
    autoCueToggle.setToggleState(true, juce::dontSendNotification);
    autoCueToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::white.withAlpha(0.85f));
    addChildComponent(autoCueToggle);

    // Section 2: Visualización y Rendimiento
    generalSectionDisplay.setText(juce::String::fromUTF8("Visualización y Rendimiento"), juce::dontSendNotification);
    generalSectionDisplay.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    generalSectionDisplay.setColour(juce::Label::textColourId, juce::Colour::fromRGB(0, 180, 216));
    addChildComponent(generalSectionDisplay);

    waveformFpsLabel.setText(juce::String::fromUTF8("Tasa de refresco de formas de onda:"), juce::dontSendNotification);
    waveformFpsLabel.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    waveformFpsLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(160, 165, 180));
    addChildComponent(waveformFpsLabel);

    waveformFpsCombo.addItem(juce::String::fromUTF8("60 FPS (Fluidez ultra suave - Recomendado)"), 1);
    waveformFpsCombo.addItem(juce::String::fromUTF8("30 FPS (Ahorro de batería y recursos)"), 2);
    waveformFpsCombo.setSelectedId(1, juce::dontSendNotification);
    addChildComponent(waveformFpsCombo);

    waveformColorLabel.setText(juce::String::fromUTF8("Esquema de color de Waveform:"), juce::dontSendNotification);
    waveformColorLabel.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    waveformColorLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(160, 165, 180));
    addChildComponent(waveformColorLabel);

    waveformColorCombo.addItem(juce::String::fromUTF8("RGB Multicolor (Frecuencias Agudos/Medios/Graves)"), 1);
    waveformColorCombo.addItem(juce::String::fromUTF8("Azul Neón Clásico DiscPro"), 2);
    waveformColorCombo.addItem(juce::String::fromUTF8("Monocromo Plata"), 3);
    waveformColorCombo.setSelectedId(1, juce::dontSendNotification);
    addChildComponent(waveformColorCombo);

    appInfoLabel.setText(juce::String::fromUTF8("DiscNativePro DJ Workstation v1.0.0 • Motor C++20 nativo con JUCE 8, CoreAudio y SIMD DSP."),
                         juce::dontSendNotification);
    appInfoLabel.setFont(juce::FontOptions(10.0f, juce::Font::italic));
    appInfoLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(110, 115, 130));
    addChildComponent(appInfoLabel);
}

void SettingsModalComponent::setupDevicesTab()
{
    // Embedded AudioDeviceSelectorComponent with up to 4 output channels
    audioDeviceSelector = std::make_unique<juce::AudioDeviceSelectorComponent>(
        audioEngine.getDeviceManager(), 0, 0, 2, 4, false, false, true, false);
    addChildComponent(*audioDeviceSelector);

    // Section: Enrutamiento de Canales de Audio
    routingSectionLabel.setText(juce::String::fromUTF8("Enrutamiento de Salidas de Audio (Pioneer DDJ / Multi-Canal)"), juce::dontSendNotification);
    routingSectionLabel.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    routingSectionLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(0, 180, 216));
    addChildComponent(routingSectionLabel);

    // Master Routing
    masterRoutingLabel.setText(juce::String::fromUTF8("Salida Master (Altavoces / Club):"), juce::dontSendNotification);
    masterRoutingLabel.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    masterRoutingLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(160, 165, 180));
    addChildComponent(masterRoutingLabel);

    masterRoutingCombo.addItem(juce::String::fromUTF8("Canales 1 & 2 (Salida RCA Master DDJ-SB2)"), 1);
    masterRoutingCombo.addItem(juce::String::fromUTF8("Canales 3 & 4 (Salida Frontal DDJ-SB2)"), 2);
    masterRoutingCombo.setSelectedId(audioEngine.getMasterChannelPair() == 0 ? 1 : 2, juce::dontSendNotification);
    masterRoutingCombo.onChange = [this]() {
        audioEngine.setMasterChannelPair(masterRoutingCombo.getSelectedId() == 1 ? 0 : 1);
    };
    addChildComponent(masterRoutingCombo);

    // CUE Routing
    cueRoutingLabel.setText(juce::String::fromUTF8("Salida Auriculares (Preescucha CUE):"), juce::dontSendNotification);
    cueRoutingLabel.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    cueRoutingLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(160, 165, 180));
    addChildComponent(cueRoutingLabel);

    cueRoutingCombo.addItem(juce::String::fromUTF8("Canales 3 & 4 (Jack Frontal Auriculares DDJ-SB2)"), 1);
    cueRoutingCombo.addItem(juce::String::fromUTF8("Canales 1 & 2 (Salida Compartida Master)"), 2);
    cueRoutingCombo.setSelectedId(audioEngine.getCueChannelPair() == 1 ? 1 : 2, juce::dontSendNotification);
    cueRoutingCombo.onChange = [this]() {
        audioEngine.setCueChannelPair(cueRoutingCombo.getSelectedId() == 1 ? 1 : 0);
    };
    addChildComponent(cueRoutingCombo);

    // Status Badge
    controllerDetectedBadge.setText(juce::String::fromUTF8("● Controladora DJ 4 Canales Detectada (Master: Ch 1-2 • Auriculares: Ch 3-4)"), juce::dontSendNotification);
    controllerDetectedBadge.setColour(juce::Label::textColourId, juce::Colour::fromRGB(74, 222, 128));
    controllerDetectedBadge.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    addChildComponent(controllerDetectedBadge);
}

void SettingsModalComponent::setupSoundTab()
{
    // Section 1: Motor de Audio
    soundSectionEngine.setText(juce::String::fromUTF8("Motor de Audio CoreAudio"), juce::dontSendNotification);
    soundSectionEngine.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    soundSectionEngine.setColour(juce::Label::textColourId, juce::Colour::fromRGB(0, 180, 216));
    addChildComponent(soundSectionEngine);

    int sr = (int)audioEngine.getSampleRate();
    if (sr <= 0) sr = 44100;
    sampleRateInfoLabel.setText(juce::String::fromUTF8("Frecuencia de Muestreo (Sample Rate): ") + juce::String(sr) + " Hz (CoreAudio)",
                                juce::dontSendNotification);
    sampleRateInfoLabel.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    sampleRateInfoLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(180, 185, 200));
    addChildComponent(sampleRateInfoLabel);

    bufferSizeLabel.setText(juce::String::fromUTF8("Tamaño de Buffer de Audio / Latencia:"), juce::dontSendNotification);
    bufferSizeLabel.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    bufferSizeLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(160, 165, 180));
    addChildComponent(bufferSizeLabel);

    bufferSizeCombo.addItem(juce::String::fromUTF8("128 muestras (2.9 ms • Ultra baja latencia interactiva)"), 1);
    bufferSizeCombo.addItem(juce::String::fromUTF8("256 muestras (5.8 ms • Baja latencia recomendada)"), 2);
    bufferSizeCombo.addItem(juce::String::fromUTF8("512 muestras (11.6 ms • Equilibrado estándar)"), 3);
    bufferSizeCombo.addItem(juce::String::fromUTF8("1024 muestras (23.2 ms • Máxima estabilidad)"), 4);

    int curBlock = audioEngine.getBlockSize();
    if (curBlock <= 128) bufferSizeCombo.setSelectedId(1, juce::dontSendNotification);
    else if (curBlock <= 256) bufferSizeCombo.setSelectedId(2, juce::dontSendNotification);
    else if (curBlock <= 512) bufferSizeCombo.setSelectedId(3, juce::dontSendNotification);
    else bufferSizeCombo.setSelectedId(4, juce::dontSendNotification);

    addChildComponent(bufferSizeCombo);

    // Section 2: Mezclador & Curvas
    soundSectionMixer.setText(juce::String::fromUTF8("Ecualizador & Crossfader"), juce::dontSendNotification);
    soundSectionMixer.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    soundSectionMixer.setColour(juce::Label::textColourId, juce::Colour::fromRGB(0, 180, 216));
    addChildComponent(soundSectionMixer);

    eqModeLabel.setText(juce::String::fromUTF8("Curva de Ecualizador (EQ Mode):"), juce::dontSendNotification);
    eqModeLabel.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    eqModeLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(160, 165, 180));
    addChildComponent(eqModeLabel);

    eqModeCombo.addItem(juce::String::fromUTF8("Isolator (-∞ dB Full Kill • Corte total por banda)"), 1);
    eqModeCombo.addItem(juce::String::fromUTF8("Clásico (-24 dB a +6 dB • Curva analógica suave)"), 2);
    eqModeCombo.setSelectedId(1, juce::dontSendNotification);
    addChildComponent(eqModeCombo);

    crossfaderCurveLabel.setText(juce::String::fromUTF8("Curva de Crossfader por defecto:"), juce::dontSendNotification);
    crossfaderCurveLabel.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    crossfaderCurveLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(160, 165, 180));
    addChildComponent(crossfaderCurveLabel);

    crossfaderCurveCombo.addItem(juce::String::fromUTF8("Suave (Smooth • Transiciones fluidas)"), 1);
    crossfaderCurveCombo.addItem(juce::String::fromUTF8("Lineal (Linear • Mezcla progresiva)"), 2);
    crossfaderCurveCombo.addItem(juce::String::fromUTF8("Corte Rápido (Scratch • Corte instantáneo)"), 3);
    crossfaderCurveCombo.setSelectedId(1, juce::dontSendNotification);
    addChildComponent(crossfaderCurveCombo);

    // Section 3: Margen Dinámico & Limitador
    soundSectionMaster.setText(juce::String::fromUTF8("Margen Dinámico y Limitador Master"), juce::dontSendNotification);
    soundSectionMaster.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    soundSectionMaster.setColour(juce::Label::textColourId, juce::Colour::fromRGB(0, 180, 216));
    addChildComponent(soundSectionMaster);

    headroomLabel.setText(juce::String::fromUTF8("Margen Dinámico Master (Headroom):"), juce::dontSendNotification);
    headroomLabel.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    headroomLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(160, 165, 180));
    addChildComponent(headroomLabel);

    headroomCombo.addItem(juce::String::fromUTF8("-3 dB (Recomendado para clubs y grabación)"), 1);
    headroomCombo.addItem(juce::String::fromUTF8("-6 dB (Margen amplio para mezclas intensas)"), 2);
    headroomCombo.addItem(juce::String::fromUTF8("0 dB (Sin margen / límite exacto)"), 3);
    headroomCombo.setSelectedId(1, juce::dontSendNotification);
    addChildComponent(headroomCombo);

    masterLimiterToggle.setButtonText(juce::String::fromUTF8("Limitador Master de Seguridad (Evita distorsión digital y clipping)"));
    masterLimiterToggle.setToggleState(true, juce::dontSendNotification);
    masterLimiterToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::white.withAlpha(0.85f));
    addChildComponent(masterLimiterToggle);

    // Section 4: Monitoreo & Auriculares (CUE)
    soundSectionPhones.setText(juce::String::fromUTF8("Monitoreo y Auriculares (Preescucha CUE)"), juce::dontSendNotification);
    soundSectionPhones.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    soundSectionPhones.setColour(juce::Label::textColourId, juce::Colour::fromRGB(0, 180, 216));
    addChildComponent(soundSectionPhones);

    cueMixLabel.setText(juce::String::fromUTF8("Mezcla Auriculares (CUE a la izquierda / MASTER a la derecha):"), juce::dontSendNotification);
    cueMixLabel.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    cueMixLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(160, 165, 180));
    addChildComponent(cueMixLabel);

    cueMixSlider.setRange(0.0, 1.0, 0.01);
    cueMixSlider.setValue(audioEngine.getMixer().getCueMix(), juce::dontSendNotification);
    cueMixSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    cueMixSlider.onValueChange = [this]() {
        audioEngine.getMixer().setCueMix((float)cueMixSlider.getValue());
    };
    addChildComponent(cueMixSlider);

    cueVolumeLabel.setText(juce::String::fromUTF8("Volumen Master de Auriculares:"), juce::dontSendNotification);
    cueVolumeLabel.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    cueVolumeLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(160, 165, 180));
    addChildComponent(cueVolumeLabel);

    cueVolumeSlider.setRange(0.0, 1.5, 0.01);
    cueVolumeSlider.setValue(audioEngine.getMixer().getPhonesVolume(), juce::dontSendNotification);
    cueVolumeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    cueVolumeSlider.onValueChange = [this]() {
        audioEngine.getMixer().setPhonesVolume((float)cueVolumeSlider.getValue());
    };
    addChildComponent(cueVolumeSlider);
}

void SettingsModalComponent::setupMidiTab()
{
    midiTitleLabel.setText(juce::String::fromUTF8("Controladores USB MIDI"), juce::dontSendNotification);
    midiTitleLabel.setFont(juce::FontOptions(12.5f, juce::Font::bold));
    midiTitleLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(46, 196, 182));
    addChildComponent(midiTitleLabel);

    midiDescLabel.setText(juce::String::fromUTF8("DiscNativePro detecta automáticamente cualquier controladora DJ física conectada por USB (Pioneer DDJ, Hercules, Numark, Native Instruments). Mapea jog wheels, perillas de EQ, faders y pads."),
                          juce::dontSendNotification);
    midiDescLabel.setFont(juce::FontOptions(11.0f));
    midiDescLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(142, 149, 165));
    addChildComponent(midiDescLabel);

    midiDevicesHeader.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    midiDevicesHeader.setColour(juce::Label::textColourId, juce::Colours::white);
    addChildComponent(midiDevicesHeader);

    midiScanBtn.setButtonText(juce::String::fromUTF8("Escanear Puertos"));
    midiScanBtn.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(24, 28, 36));
    midiScanBtn.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(0, 180, 216));
    midiScanBtn.onClick = [this]() {
        midi.rescanDevices();
        refreshMidiDevices();
    };
    addChildComponent(midiScanBtn);

    midiMonitorHeader.setText(juce::String::fromUTF8("MONITOR DE SEÑALES MIDI EN VIVO"), juce::dontSendNotification);
    midiMonitorHeader.setFont(juce::FontOptions(9.5f, juce::Font::bold));
    midiMonitorHeader.setColour(juce::Label::textColourId, juce::Colour::fromRGB(100, 108, 125));
    addChildComponent(midiMonitorHeader);

    midiMonitorText.setText(juce::String::fromUTF8("Esperando movimiento de fader, jog o pad..."), juce::dontSendNotification);
    midiMonitorText.setFont(juce::FontOptions("Menlo", 11.5f, juce::Font::plain));
    midiMonitorText.setColour(juce::Label::textColourId, juce::Colour::fromRGB(0, 229, 255));
    addChildComponent(midiMonitorText);

    midi.onMidiActivity = [this](const juce::String& desc) {
        midiMonitorText.setText(desc, juce::dontSendNotification);
    };

    refreshMidiDevices();
}

void SettingsModalComponent::setupStreamingTab()
{
    streamingSectionTitle.setText(juce::String::fromUTF8("Integración de YouTube Streaming"), juce::dontSendNotification);
    streamingSectionTitle.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    streamingSectionTitle.setColour(juce::Label::textColourId, juce::Colour::fromRGB(0, 180, 216));
    addChildComponent(streamingSectionTitle);

    streamingDescLabel.setText(juce::String::fromUTF8("DiscNativePro permite buscar, mezclar y reproducir videos de YouTube en vivo directamente en las bandejas.\nPor defecto se utiliza Scraping Público para buscar sin necesidad de API Key."), juce::dontSendNotification);
    streamingDescLabel.setFont(juce::FontOptions(10.5f));
    streamingDescLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(160, 165, 180));
    addChildComponent(streamingDescLabel);

    apiKeyLabel.setText(juce::String::fromUTF8("YouTube Data API v3 Key (Opcional - Para búsqueda oficial sin restricciones):"), juce::dontSendNotification);
    apiKeyLabel.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    apiKeyLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(180, 185, 200));
    addChildComponent(apiKeyLabel);

    apiKeyEditor.setTextToShowWhenEmpty("Pega tu clave AIzaSy...", juce::Colour::fromRGB(100, 108, 125));
    apiKeyEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour::fromRGB(17, 19, 26));
    apiKeyEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    apiKeyEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour::fromRGB(38, 43, 58));
    apiKeyEditor.setText(YouTubeService::getApiKey(), juce::dontSendNotification);
    addChildComponent(apiKeyEditor);

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
    addChildComponent(clearApiKeyBtn);

    apiStatusBadge.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    addChildComponent(apiStatusBadge);

    apiInfoCard.setText(juce::String::fromUTF8("Modo Video en Bandejas:\n• Al soltar un video de YouTube en el Deck 1 o Deck 2, el Jog Wheel se transforma en reproductor de video de alta definición (WebKit).\n• Los faders de volumen y el Crossfader central mezclan fluidamente el audio y video entre bandejas.\n• Iniciar sesión en YouTube en Safari comparte tu sesión con WKWebView para reproducción Premium sin publicidad."), juce::dontSendNotification);
    apiInfoCard.setFont(juce::FontOptions(10.0f));
    apiInfoCard.setColour(juce::Label::textColourId, juce::Colour::fromRGB(140, 145, 160));
    addChildComponent(apiInfoCard);
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
    generalSectionDeck.setVisible(isGeneral);
    loadLockToggle.setVisible(isGeneral);
    jogModeLabel.setVisible(isGeneral);
    jogModeCombo.setVisible(isGeneral);
    pitchRangeLabel.setVisible(isGeneral);
    pitchRangeCombo.setVisible(isGeneral);
    autoCueToggle.setVisible(isGeneral);
    generalSectionDisplay.setVisible(isGeneral);
    waveformFpsLabel.setVisible(isGeneral);
    waveformFpsCombo.setVisible(isGeneral);
    waveformColorLabel.setVisible(isGeneral);
    waveformColorCombo.setVisible(isGeneral);
    appInfoLabel.setVisible(isGeneral);

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
            controllerDetectedBadge.setText(juce::String::fromUTF8("● Controladora DJ 4 Canales Detectada (Master: Ch 1-2 • Auriculares: Ch 3-4)"), juce::dontSendNotification);
            controllerDetectedBadge.setColour(juce::Label::textColourId, juce::Colour::fromRGB(74, 222, 128));
        }
        else
        {
            controllerDetectedBadge.setText(juce::String::fromUTF8("● Dispositivo estéreo (2 canales). Para preescucha independiente selecciona una tarjeta de 4 canales como DDJ-SB2"), juce::dontSendNotification);
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

    // Streaming components (Tab 5)
    bool isStreaming = (activeTab == 5);
    streamingSectionTitle.setVisible(isStreaming);
    streamingDescLabel.setVisible(isStreaming);
    apiKeyLabel.setVisible(isStreaming);
    apiKeyEditor.setVisible(isStreaming);
    apiStatusBadge.setVisible(isStreaming);
    apiInfoCard.setVisible(isStreaming);
    clearApiKeyBtn.setVisible(isStreaming);

    if (isStreaming)
    {
        bool hasKey = apiKeyEditor.getText().trim().isNotEmpty();
        if (hasKey)
        {
            apiStatusBadge.setText(juce::String::fromUTF8("● YouTube Data API v3 Clave Guardada (Sin Restricciones)"), juce::dontSendNotification);
            apiStatusBadge.setColour(juce::Label::textColourId, juce::Colour::fromRGB(74, 222, 128));
        }
        else
        {
            apiStatusBadge.setText(juce::String::fromUTF8("● Scraping Público Activo (Sin Clave API)"), juce::dontSendNotification);
            apiStatusBadge.setColour(juce::Label::textColourId, juce::Colour::fromRGB(0, 229, 255));
        }
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
}

void SettingsModalComponent::applySettings()
{
    // Apply YouTube API key
    YouTubeService::setApiKey(apiKeyEditor.getText().trim());

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

    // 2. Top Title Bar (#14161f)
    auto titleBarArea = bounds.removeFromTop(80.0f);
    g.setColour(juce::Colour::fromRGB(20, 22, 31));
    g.fillRect(titleBarArea);

    g.setColour(juce::Colour::fromRGB(38, 43, 58));
    g.drawHorizontalLine((int)titleBarArea.getBottom(), 0.0f, bounds.getWidth());

    // 3. Tab Divider line
    g.drawHorizontalLine(34, 0.0f, bounds.getWidth());

    // 4. Footer Bar (#14161f)
    auto footerArea = getLocalBounds().removeFromBottom(48);
    g.setColour(juce::Colour::fromRGB(20, 22, 31));
    g.fillRect(footerArea);

    g.setColour(juce::Colour::fromRGB(38, 43, 58));
    g.drawHorizontalLine(footerArea.getY(), 0.0f, bounds.getWidth());

    // 5. If activeTab == 3 (MIDI), paint the device list box
    if (activeTab == 3)
    {
        juce::Rectangle<float> devBox(24.0f, 185.0f, (float)getWidth() - 48.0f, 130.0f);
        if (midiConnectedDevices.isEmpty())
        {
            g.setColour(juce::Colour::fromRGB(17, 18, 25));
            g.fillRoundedRectangle(devBox, 8.0f);
            g.setColour(juce::Colour::fromRGB(40, 44, 60));
            g.drawRoundedRectangle(devBox, 8.0f, 1.0f);

            g.setColour(juce::Colour::fromRGB(255, 159, 28));
            g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
            g.drawText(juce::String::fromUTF8("No se detectó ningún controlador"),
                       devBox.removeFromTop(75.0f), juce::Justification::centred, false);

            g.setColour(juce::Colour::fromRGB(120, 125, 140));
            g.setFont(juce::FontOptions(10.5f, juce::Font::plain));
            g.drawText(juce::String::fromUTF8("Conecta un dispositivo USB DJ y pulsa 'Escanear Puertos'"),
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
        contentArea.removeFromTop(88);
        contentArea.removeFromBottom(54);
        contentArea.reduce(20, 10);

        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        g.drawText(juce::String::fromUTF8("Atajos de Teclado Principales de DiscNativePro:"),
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
                auto kbdRect = row.removeFromLeft(36).reduced(0, 2);
                g.setColour(juce::Colour::fromRGB(36, 39, 53));
                g.fillRoundedRectangle(kbdRect.toFloat(), 3.0f);
                g.setColour(juce::Colours::white);
                g.drawText(item.first, kbdRect, juce::Justification::centred, false);

                row.removeFromLeft(8);
                g.setColour(juce::Colour::fromRGB(200, 205, 215));
                g.drawText(item.second, row, juce::Justification::centredLeft, false);
            }
        };

        drawCard(d1Card, juce::String::fromUTF8("Deck 1 (Canal Izquierdo):"), juce::Colour::fromRGB(0, 180, 216), {
            { "Q", "Play / Pause" },
            { "W", "CUE (Pre-escucha)" },
            { "E", "SYNC (Sincronizar BPM)" },
            { "1-4", "Hot Cues 1 a 4" }
        });

        drawCard(d2Card, juce::String::fromUTF8("Deck 2 (Canal Derecho):"), juce::Colour::fromRGB(148, 163, 184), {
            { "P", "Play / Pause" },
            { "O", "CUE (Pre-escucha)" },
            { "I", "SYNC (Sincronizar BPM)" },
            { "7-0", "Hot Cues 1 a 4" }
        });

        contentArea.removeFromTop(12);

        // Mixer Card
        auto mixerCard = contentArea.removeFromTop(100);
        drawCard(mixerCard, juce::String::fromUTF8("Mezclador & Crossfader:"), juce::Colours::white, {
            { "X | C | V", juce::String::fromUTF8("Crossfader: Izquierda | Centro | Derecha") },
            { "Espacio",   juce::String::fromUTF8("Activar / Desactivar Scratch en Deck activo") }
        });
    }

    // 7. If activeTab == 5 (Streaming), paint info card background
    if (activeTab == 5)
    {
        juce::Rectangle<float> cardArea(24.0f, 240.0f, (float)getWidth() - 48.0f, 160.0f);
        g.setColour(juce::Colour::fromRGB(18, 20, 27));
        g.fillRoundedRectangle(cardArea, 8.0f);
        g.setColour(juce::Colour::fromRGB(40, 44, 60));
        g.drawRoundedRectangle(cardArea, 8.0f, 1.0f);
    }
}

void SettingsModalComponent::resized()
{
    // 1. Title bar (height 34px)
    closeDotButton.setBounds(14, 11, 12, 12);
    headerTitleLabel.setBounds(34, 6, 320, 22);

    // 2. Tab buttons row (y: 38 to 76)
    int tabW = 96;
    int tabH = 34;
    int spacing = 5;
    int totalTabsW = tabW * 6 + spacing * 5;
    int startTabX = (getWidth() - totalTabsW) / 2;

    tabGeneralBtn.setBounds(startTabX, 40, tabW, tabH);
    tabDevicesBtn.setBounds(startTabX + (tabW + spacing) * 1, 40, tabW, tabH);
    tabSoundBtn.setBounds(startTabX + (tabW + spacing) * 2, 40, tabW, tabH);
    tabMidiBtn.setBounds(startTabX + (tabW + spacing) * 3, 40, tabW, tabH);
    tabShortcutsBtn.setBounds(startTabX + (tabW + spacing) * 4, 40, tabW, tabH);
    tabStreamingBtn.setBounds(startTabX + (tabW + spacing) * 5, 40, tabW, tabH);

    // 3. Tab Body Area
    auto body = getLocalBounds();
    body.removeFromTop(88);
    body.removeFromBottom(52);
    body.reduce(20, 10);

    // --- Tab 0: General Layout (Unconditionally set bounds so they are always ready) ---
    {
        auto gArea = body;

        generalSectionDeck.setBounds(gArea.removeFromTop(20));
        gArea.removeFromTop(4);
        loadLockToggle.setBounds(gArea.removeFromTop(24));
        gArea.removeFromTop(6);

        auto row1 = gArea.removeFromTop(24);
        jogModeLabel.setBounds(row1.removeFromLeft(160));
        jogModeCombo.setBounds(row1);
        gArea.removeFromTop(6);

        auto row2 = gArea.removeFromTop(24);
        pitchRangeLabel.setBounds(row2.removeFromLeft(160));
        pitchRangeCombo.setBounds(row2);
        gArea.removeFromTop(6);

        autoCueToggle.setBounds(gArea.removeFromTop(24));
        gArea.removeFromTop(14);

        generalSectionDisplay.setBounds(gArea.removeFromTop(20));
        gArea.removeFromTop(6);

        auto row3 = gArea.removeFromTop(24);
        waveformFpsLabel.setBounds(row3.removeFromLeft(220));
        waveformFpsCombo.setBounds(row3);
        gArea.removeFromTop(6);

        auto row4 = gArea.removeFromTop(24);
        waveformColorLabel.setBounds(row4.removeFromLeft(220));
        waveformColorCombo.setBounds(row4);
        gArea.removeFromTop(18);

        appInfoLabel.setBounds(gArea.removeFromTop(20));
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

        mArea.removeFromTop(142); // space occupied by device list box painted in paint()

        midiMonitorHeader.setBounds(mArea.removeFromTop(18));
        mArea.removeFromTop(2);
        midiMonitorText.setBounds(mArea.removeFromTop(26));
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

        stArea.removeFromTop(6);
        apiStatusBadge.setBounds(stArea.removeFromTop(20));
        stArea.removeFromTop(16);

        apiInfoCard.setBounds(stArea.removeFromTop(140).reduced(8, 6));
    }

    // 4. Footer buttons
    int footerY = getHeight() - 38;
    helpButton.setBounds(18, footerY, 28, 26);
    applyButton.setBounds(getWidth() - 96, footerY, 80, 26);
    undoButton.setBounds(getWidth() - 186, footerY, 80, 26);
}

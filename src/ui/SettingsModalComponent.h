#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "../audio/AudioEngine.h"
#include "../midi/MidiManager.h"
#include "LucideIcons.h"
#include "DjButton.h"

class SettingsModalComponent : public juce::Component
{
public:
    SettingsModalComponent(AudioEngine& engine, MidiManager& midiMgr);
    ~SettingsModalComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    std::function<void()> onClose;

private:
    void setupGeneralTab();
    void setupDevicesTab();
    void setupSoundTab();
    void setupMidiTab();
    void setupStreamingTab();
    void refreshMidiDevices();
    void updateTabVisibility();
    void applySettings();

    AudioEngine& audioEngine;
    MidiManager& midi;
    int activeTab = 0; // 0: General, 1: Dispositivos, 2: Sonido, 3: MIDI, 4: Shortcuts, 5: Streaming

    // Header controls
    juce::TextButton closeDotButton;
    juce::Label headerTitleLabel;

    // Tab buttons
    DjButton tabGeneralBtn;
    DjButton tabDevicesBtn;
    DjButton tabSoundBtn;
    DjButton tabMidiBtn;
    DjButton tabShortcutsBtn;
    DjButton tabStreamingBtn;

    // --- Tab 0: General Controls ---
    juce::Label generalSectionDeck;
    juce::ToggleButton loadLockToggle;
    juce::Label jogModeLabel;
    juce::ComboBox jogModeCombo;
    juce::Label pitchRangeLabel;
    juce::ComboBox pitchRangeCombo;
    juce::ToggleButton autoCueToggle;

    juce::Label generalSectionDisplay;
    juce::Label waveformFpsLabel;
    juce::ComboBox waveformFpsCombo;
    juce::Label waveformColorLabel;
    juce::ComboBox waveformColorCombo;

    juce::Label appInfoLabel;

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

    // --- Tab 3: MIDI Controls ---
    juce::Label midiTitleLabel;
    juce::Label midiDescLabel;
    juce::Label midiDevicesHeader;
    juce::TextButton midiScanBtn { "Escanear Puertos" };
    juce::StringArray midiConnectedDevices;
    juce::Label midiMonitorHeader;
    juce::Label midiMonitorText;

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

    // --- Footer Controls ---
    DjButton helpButton;
    juce::TextButton undoButton { "Deshacer" };
    juce::TextButton applyButton { "Aplicar" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsModalComponent)
};

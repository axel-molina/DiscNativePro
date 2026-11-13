#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include "../midi/MidiManager.h"
#include "../services/LanguageManager.h"
#include "LucideIcons.h"

class MidiModalComponent : public juce::Component,
                           public LanguageManager::Listener
{
public:
    MidiModalComponent(MidiManager& midiMgr);
    ~MidiModalComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void languageChanged() override;
    void refreshLanguage();

private:
    void refreshDeviceList();
    void importMixxxPreset();

    MidiManager& midi;

    juce::Label headerTitle;
    juce::Label descLabel;
    juce::Label devicesHeader;
    juce::TextButton scanBtn { "Escanear" };
    juce::TextButton mixxxBtn { "Importar Mixxx" };
    std::atomic<bool> importing { false };

    // Device list container
    juce::StringArray currentDevices;

    // Monitor box
    juce::Label monitorHeader;
    juce::Label monitorText;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiModalComponent)
};

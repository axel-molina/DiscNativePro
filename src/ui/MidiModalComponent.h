#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../midi/MidiManager.h"
#include "LucideIcons.h"

class MidiModalComponent : public juce::Component
{
public:
    MidiModalComponent(MidiManager& midiMgr);
    ~MidiModalComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void refreshDeviceList();

    MidiManager& midi;

    juce::Label headerTitle;
    juce::Label descLabel;
    juce::Label devicesHeader;
    juce::TextButton scanBtn { "Escanear" };

    // Device list container
    juce::StringArray currentDevices;

    // Monitor box
    juce::Label monitorHeader;
    juce::Label monitorText;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiModalComponent)
};

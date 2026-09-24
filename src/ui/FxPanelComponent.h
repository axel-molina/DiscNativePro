#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../audio/DjMixer.h"
#include "DjKnob.h"

class FxPanelComponent : public juce::Component
{
public:
    FxPanelComponent(DjMixer& djMixer);
    ~FxPanelComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    DjMixer& mixer;

    // Delay / Echo
    juce::TextButton echoToggleBtn { "ECHO" };
    juce::TextButton beat1_4Btn    { "1/4" };
    juce::TextButton beat1_2Btn    { "1/2" };
    juce::TextButton beat3_4Btn    { "3/4" };
    juce::TextButton beat1_1Btn    { "1" };
    DjKnob echoFeedbackKnob        { "FEEDBACK", juce::Colour::fromRGB(96, 165, 250) };
    DjKnob echoMixKnob             { "ECHO WET", juce::Colour::fromRGB(96, 165, 250) };

    // Reverb
    juce::TextButton reverbToggleBtn { "REVERB" };
    DjKnob reverbMixKnob             { "REV WET", juce::Colour::fromRGB(255, 159, 28) };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FxPanelComponent)
};

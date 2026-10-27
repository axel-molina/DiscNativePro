#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../audio/DjMixer.h"
#include "DjKnob.h"
#include "VuMeter.h"
#include "DjLookAndFeel.h"
#include "DjButton.h"

class ChannelStripComponent : public juce::Component,
                             public juce::Timer
{
public:
    ChannelStripComponent(int channelIdx, ChannelStrip& chStrip, juce::Colour accent);
    ~ChannelStripComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    // Modo Ondas: el fader de volumen se reubica en el centro del mixer y el
    // VU desaparece; la columna solo lleva EQ y queda más estrecha.
    juce::Slider& getVolumeFader() { return volumeFader; }
    void setWavesMode(bool waves);

private:
    int index;
    ChannelStrip& strip;
    juce::Colour deckColor;

    juce::Label headerBadge;

    DjKnob highKnob   { "HIGH",   juce::Colour::fromRGB(156, 163, 175) };
    DjKnob midKnob    { "MID",    juce::Colour::fromRGB(156, 163, 175) };
    DjKnob lowKnob    { "LOW",    juce::Colour::fromRGB(156, 163, 175) };
    DjKnob filterKnob { "FILTER", juce::Colour::fromRGB(156, 163, 175) };

    juce::Slider volumeFader;
    VuMeter vuMeter;
    DjButton cueButton { "CUE" };
    DjLookAndFeel djLookAndFeel;
    float faderSeparatorY { 0.0f };
    bool wavesMode { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChannelStripComponent)
};

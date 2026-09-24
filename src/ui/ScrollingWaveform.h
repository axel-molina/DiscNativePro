#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../audio/DeckPlayer.h"

class ScrollingWaveform : public juce::Component,
                         public juce::Timer
{
public:
    ScrollingWaveform(DeckPlayer& deck1Player, DeckPlayer& deck2Player);
    ~ScrollingWaveform() override;

    void paint(juce::Graphics& g) override;
    void timerCallback() override;

private:
    void drawDeckWaveform(juce::Graphics& g, DeckPlayer& player, juce::Rectangle<float> area, juce::Colour accent, bool invert);

    DeckPlayer& deck1;
    DeckPlayer& deck2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScrollingWaveform)
};

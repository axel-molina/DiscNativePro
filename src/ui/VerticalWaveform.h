#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../audio/DeckPlayer.h"

class VerticalWaveform : public juce::Component,
                         public juce::Timer
{
public:
    VerticalWaveform(DeckPlayer& player1, DeckPlayer& player2);
    ~VerticalWaveform() override;

    void paint(juce::Graphics& g) override;
    void timerCallback() override;

private:
    void renderDeckHalf(juce::Graphics& g,
                        DeckPlayer& player,
                        float startX,
                        float deckWidth,
                        float centerY,
                        float totalHeight,
                        bool isLeft);

    DeckPlayer& deck1;
    DeckPlayer& deck2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VerticalWaveform)
};

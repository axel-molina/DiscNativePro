#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../audio/DeckPlayer.h"

class OverviewWaveform : public juce::Component
{
public:
    OverviewWaveform(DeckPlayer& player, juce::Colour deckAccent);
    ~OverviewWaveform() override;

    void setAccentColour(juce::Colour c) { accent = c; repaint(); }

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

    void updateWaveform();

private:
    void seekToMouse(float mouseX);

    DeckPlayer& deck;
    juce::Colour accent;

    std::vector<float> peaksLow;
    std::vector<float> peaksMid;
    std::vector<float> peaksHigh;
    bool hasData { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OverviewWaveform)
};

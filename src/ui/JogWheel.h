#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../audio/DeckPlayer.h"

class JogWheel : public juce::Component
{
public:
    JogWheel(DeckPlayer& player, juce::Colour accentColour = juce::Colour::fromRGB(0, 210, 255));
    ~JogWheel() override;

    void setTrackInfo(const juce::String& title, double bpm, juce::Colour deckColor);
    void setAccentColour(juce::Colour c) { accent = c; repaint(); }
    void updateAngle();

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

private:
    float getAngleFromPoint(juce::Point<float> pt) const;

    DeckPlayer& deck;
    juce::Colour accent;
    juce::String trackTitle;
    double trackBpm { 120.0 };
    juce::Image coverArtImage;

    float currentAngleRadians { 0.0f };
    float lastMouseAngle { 0.0f };
    bool wasPlayingBeforeDrag { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JogWheel)
};

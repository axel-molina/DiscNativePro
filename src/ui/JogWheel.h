#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../audio/DeckPlayer.h"
#include "UiPalette.h"

class JogWheel : public juce::Component
{
public:
    JogWheel(DeckPlayer& player, juce::Colour accentColour = UiPalette::accentBright);
    ~JogWheel() override;

    void setTrackInfo(const juce::String& title, double bpm, juce::Colour deckColor);
    void setCustomCoverArt(const juce::Image& image);
    void setAccentColour(juce::Colour c) { accent = c; repaint(); }
    void updateAngle();

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

private:
    float getAngleFromPoint(juce::Point<float> pt) const;
    bool isEdgeZone(juce::Point<float> pt) const;

    DeckPlayer& deck;
    juce::Colour accent;
    juce::String trackTitle;
    double trackBpm { 120.0 };
    juce::Image coverArtImage;

    float currentAngleRadians { 0.0f };
    float lastMouseAngle { 0.0f };
    bool auditioning { false }; // tocado en pausa: play temporal para que el scratch suene
    bool bending { false };
    bool scratching { false }; // zona scratch tomada: publico velocidad, no saltos
    float smoothRevPerSec { 0.0f };
    juce::uint32 lastMoveMs { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JogWheel)
};

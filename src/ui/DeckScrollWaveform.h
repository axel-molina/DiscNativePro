#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../audio/DeckPlayer.h"

// Wave scrolling de una bandeja (±3s alrededor del playhead) para las tiras del
// modo Ondas: misma técnica que el wave central de 2 Bandejas (resolución fina
// de picos para mezclas precisas) con cuadrícula de beats al BPM real de la
// pista. Click/drag sobre la onda hace seek.
class DeckScrollWaveform : public juce::Component,
                           public juce::Timer
{
public:
    explicit DeckScrollWaveform(DeckPlayer& player, juce::Colour deckAccent);
    ~DeckScrollWaveform() override;

    void paint(juce::Graphics& g) override;
    void timerCallback() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

private:
    void seekToMouse(float mouseX);

    DeckPlayer& deck;
    juce::Colour accent;

    static constexpr double visibleWindowSecs = 6.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeckScrollWaveform)
};

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../audio/DeckPlayer.h"
#include "../audio/WindowPeakExtractor.h"

// Wave scrolling de una bandeja (±3s alrededor del playhead) para las tiras del
// modo Ondas. Picos finos por ventana (~6ms/punto, estilo Serato) con cuerpo
// RMS multibanda, línea de pico fino y curva de amplitud tipo dB para
// distinguir transientes incluso en pistas comprimidas. Cuadrícula de beats al
// BPM real de la pista; click/drag hace seek.
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
    void ensureFinePeaks();
    void drawFineWave(juce::Graphics& g, juce::Rectangle<float> area, double currentPos);
    void drawOverviewFallback(juce::Graphics& g, juce::Rectangle<float> area, double currentPos);
    void seekToMouse(float mouseX);

    DeckPlayer& deck;
    juce::Colour accent;

    WindowPeakExtractor extractor;
    juce::File extractorFile;

    static constexpr double visibleWindowSecs = 6.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeckScrollWaveform)
};

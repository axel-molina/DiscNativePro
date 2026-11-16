#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../audio/DeckPlayer.h"
#include <cmath>

class VerticalWaveform : public juce::Component,
                         public juce::Timer
{
public:
    VerticalWaveform(DeckPlayer& player1, DeckPlayer& player2);
    ~VerticalWaveform() override;

    void paint(juce::Graphics& g) override;
    void timerCallback() override;

    // Clic = salto a ese instante; arrastre vertical = adelantar/atrasar;
    // rueda = zoom (solo en memoria, por sesion).
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

private:
    void renderDeckHalf(juce::Graphics& g,
                        DeckPlayer& player,
                        float startX,
                        float deckWidth,
                        float centerY,
                        float totalHeight,
                        bool isLeft);

    // Mitad izquierda = Deck 1, derecha = Deck 2 (igual que el pintado).
    DeckPlayer& deckForX(float x) const;
    // Instante bajo una coordenada Y (el tiempo crece hacia arriba).
    double timeAtY(const DeckPlayer& player, float y) const;
    void applyDragPosition(const juce::MouseEvent& e);

    DeckPlayer& deck1;
    DeckPlayer& deck2;

    int dragDeck { -1 };            // -1 = sin arrastre, 0 = Deck 1, 1 = Deck 2
    double dragAnchorPos { 0.0 };
    float dragAnchorY { 0.0f };
    float pixelsPerSecond { 70.0f }; // zoom de la vista (20..300)
    double lastPos1 { -1.0 };
    double lastPos2 { -1.0 };

    // Estado del loop de cada deck. Se compara con el del DeckPlayer para
    // repintar al activarlo o desactivarlo aunque el transporte este parado
    // (P40): si no, la region del loop no aparecia hasta que volvia a sonar.
    struct LoopState
    {
        bool   active { false };
        double start  { -1.0 };
        double length { -1.0 };

        bool differsFrom (const DeckPlayer& d) const
        {
            // Tolerancia en vez de == (regla del repo: 0 warnings).
            return active != d.isLooping()
                || std::abs (start  - d.getLoopStart())  > 1e-6
                || std::abs (length - d.getLoopLength()) > 1e-6;
        }
    };

    LoopState lastLoop1;
    LoopState lastLoop2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VerticalWaveform)
};

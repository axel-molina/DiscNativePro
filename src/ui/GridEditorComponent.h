#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../audio/DeckPlayer.h"
#include "DjButton.h"

class AudioEngine;

// P16: editor de beat grid modal. Canvas con peaks zoomados centrados en el
// marcador del primer beat; el arrastre mueve el grid, los botones ajustan
// offset/BPM. Los cambios se aplican en vivo al DeckPlayer y se persisten
// por pista en LibraryCacheManager.
class GridEditorComponent : public juce::Component,
                            public juce::Timer
{
public:
    GridEditorComponent(DeckPlayer& player, AudioEngine& engineRef, int deckNumber);
    ~GridEditorComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void timerCallback() override;

    std::function<void()> onClosed; // MainComponent cierra la DialogWindow

private:
    void persistGrid();
    void refreshLabels();
    juce::Rectangle<int> canvasRect() const;

    DeckPlayer& deck;
    AudioEngine& engine;
    int deckIdx;

    static constexpr double pixelsPerSecond = 60.0;

    // Botones
    DjButton nudgeBackFarButton;   // -20 ms
    DjButton nudgeBackButton;      // -5 ms
    DjButton nudgeFwdButton;       // +5 ms
    DjButton nudgeFwdFarButton;    // +20 ms
    DjButton bpmDownFarButton;     // -0.5
    DjButton bpmDownButton;        // -0.05
    DjButton bpmUpButton;          // +0.05
    DjButton bpmUpFarButton;       // +0.5
    DjButton bpmHalfButton;        // x1/2
    DjButton bpmDoubleButton;      // x2
    DjButton resetButton;
    DjButton playButton;
    DjButton closeButton;

    juce::Label offsetValueLabel;
    juce::Label bpmValueLabel;
    juce::Label helpLabel;

    bool playAudition { false };
    bool wasPlayingOnOpen { false }; // el deck ya venía reproduciendo: cerrar no debe pausarlo
    int lastDragX { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GridEditorComponent)
};

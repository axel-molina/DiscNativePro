#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "../audio/DeckPlayer.h"

class OverviewWaveform : public juce::Component
{
public:
    OverviewWaveform(DeckPlayer& player, juce::Colour deckAccent, int deckIndex);
    ~OverviewWaveform() override;

    void setAccentColour(juce::Colour c) { accent = c; repaint(); }

    // Progreso de video YouTube (reloj virtual de DeckComponent).
    // dur <= 0 desactiva el modo video.
    void setVideoProgress(double posSeconds, double durSeconds)
    {
        videoPos = posSeconds;
        videoDur = durSeconds;
    }

    void setVideoMode(bool active)
    {
        videoModeActive = active;
    }

    std::function<void(double seconds)> onVideoSeek;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

    void updateWaveform();

private:
    void seekToMouse(float mouseX);
    bool isVideoProgressActive() const { return videoModeActive && videoDur > 0.0; }

    DeckPlayer& deck;
    juce::Colour accent;
    int deckIdx { 0 }; // 0 = Deck 1, 1 = Deck 2 (tema de waveform propio)

    std::vector<float> peaksLow;
    std::vector<float> peaksMid;
    std::vector<float> peaksHigh;

    double videoPos { 0.0 };
    double videoDur { 0.0 };
    bool videoModeActive { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OverviewWaveform)
};

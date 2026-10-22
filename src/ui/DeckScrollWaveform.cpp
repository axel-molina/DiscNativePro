#include "DeckScrollWaveform.h"
#include "../services/WaveformTheme.h"
#include <cmath>

DeckScrollWaveform::DeckScrollWaveform(DeckPlayer& player, juce::Colour deckAccent)
    : deck(player), accent(deckAccent)
{
    startTimerHz(60);
}

DeckScrollWaveform::~DeckScrollWaveform()
{
    stopTimer();
}

void DeckScrollWaveform::timerCallback()
{
    if (deck.isPlaying())
        repaint();
}

void DeckScrollWaveform::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour(juce::Colour::fromRGB(14, 15, 19));
    g.fillRect(bounds);

    const double totalDur = deck.getLengthInSeconds();
    if (!deck.isLoaded() || totalDur <= 0.0)
        return;

    const auto& low = deck.getPeaksLow();
    const auto& mid = deck.getPeaksMid();
    const auto& high = deck.getPeaksHigh();
    if (low.empty())
        return;

    const size_t numPeaks = low.size();
    const float height = bounds.getHeight();
    const float centerX = bounds.getCentreX();
    const float baselineY = bounds.getBottom();
    const double pixelsPerSec = (double)bounds.getWidth() / visibleWindowSecs;
    const double currentPos = deck.getPosition();
    const double halfWindow = visibleWindowSecs * 0.5;

    // Cuadrícula de beats al BPM real de la pista (fallback 124)
    const double bpm = deck.getBpm() > 1.0 ? deck.getBpm() : 124.0;
    const double beatInterval = 60.0 / bpm;
    const double firstBeatTime = std::floor((currentPos - halfWindow) / beatInterval) * beatInterval;

    g.setColour(juce::Colour::fromRGB(45, 52, 68));
    for (double bt = firstBeatTime; bt <= currentPos + halfWindow; bt += beatInterval)
    {
        if (bt >= 0.0 && bt <= totalDur)
        {
            float gridX = centerX + (float)((bt - currentPos) * pixelsPerSec);
            g.drawVerticalLine((int)gridX, bounds.getY(), bounds.getBottom());
        }
    }

    // Barras multibanda desde la línea base (mismo estilo que el wave central)
    const int stepPixels = 2;
    for (float x = bounds.getX(); x < bounds.getRight(); x += (float)stepPixels)
    {
        const double trackTime = currentPos + (double)(x - centerX) / pixelsPerSec;
        if (trackTime < 0.0 || trackTime > totalDur)
            continue;

        size_t idx = static_cast<size_t>((trackTime / totalDur) * (double)numPeaks);
        if (idx >= numPeaks)
            idx = numPeaks - 1;

        const float lAmp = low[idx] * height * 0.9f;
        const float mAmp = mid[idx] * height * 0.9f;
        const float hAmp = high[idx] * height * 0.9f;

        g.setColour(WaveformTheme::low());
        g.fillRect(x, baselineY, (float)stepPixels, -lAmp * 0.85f);
        g.setColour(WaveformTheme::mid());
        g.fillRect(x, baselineY, (float)stepPixels, -mAmp * 0.75f);
        g.setColour(WaveformTheme::high());
        g.fillRect(x, baselineY, (float)stepPixels, -hAmp * 0.65f);
    }

    // Playhead centrado
    g.setColour(accent.withAlpha(0.95f));
    g.drawVerticalLine((int)centerX, bounds.getY(), bounds.getBottom());
}

void DeckScrollWaveform::mouseDown(const juce::MouseEvent& e)
{
    seekToMouse((float)e.x);
}

void DeckScrollWaveform::mouseDrag(const juce::MouseEvent& e)
{
    seekToMouse((float)e.x);
}

void DeckScrollWaveform::seekToMouse(float mouseX)
{
    const double totalDur = deck.getLengthInSeconds();
    if (!deck.isLoaded() || totalDur <= 0.0)
        return;

    const double pixelsPerSec = (double)getWidth() / visibleWindowSecs;
    const double target = deck.getPosition() + ((double)mouseX - (double)getWidth() * 0.5) / pixelsPerSec;
    deck.setPosition(juce::jlimit(0.0, totalDur, target));
}

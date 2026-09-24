#include "ScrollingWaveform.h"
#include <cmath>

ScrollingWaveform::ScrollingWaveform(DeckPlayer& d1, DeckPlayer& d2)
    : deck1(d1), deck2(d2)
{
    startTimerHz(60);
}

ScrollingWaveform::~ScrollingWaveform()
{
    stopTimer();
}

void ScrollingWaveform::timerCallback()
{
    if (deck1.isPlaying() || deck2.isPlaying())
    {
        repaint();
    }
}

void ScrollingWaveform::drawDeckWaveform(juce::Graphics& g, DeckPlayer& player, juce::Rectangle<float> area, juce::Colour accent, bool invert)
{
    g.setColour(juce::Colour::fromRGB(14, 15, 19));
    g.fillRect(area);

    double totalDur = player.getLengthInSeconds();
    if (!player.isLoaded() || totalDur <= 0.0)
    {
        return;
    }

    const auto& low = player.getPeaksLow();
    const auto& mid = player.getPeaksMid();
    const auto& high = player.getPeaksHigh();
    if (low.empty()) return;

    size_t numPeaks = low.size();
    float width = area.getWidth();
    float height = area.getHeight();
    float centerX = area.getCentreX();
    float baselineY = invert ? area.getY() : area.getBottom();

    // Time window visible on screen: +/- 3.0 seconds
    const double visibleWindowSecs = 6.0;
    const double pixelsPerSec = width / visibleWindowSecs;

    double currentPos = player.getPosition();

    // 1. Draw Beatgrid Lines
    // Default 124 BPM -> beat interval ~0.484s
    double beatInterval = 60.0 / 124.0;
    double firstBeatTime = std::floor((currentPos - 3.0) / beatInterval) * beatInterval;

    g.setColour(juce::Colour::fromRGB(45, 52, 68));
    for (double bt = firstBeatTime; bt <= currentPos + 3.0; bt += beatInterval)
    {
        if (bt >= 0.0 && bt <= totalDur)
        {
            float gridX = centerX + (float)((bt - currentPos) * pixelsPerSec);
            g.drawVerticalLine((int)gridX, area.getY(), area.getBottom());
        }
    }

    // 2. Draw Scrolling Multi-Band Frequency Waveform Bars
    int stepPixels = 2; // Crisp 2px resolution
    for (float x = area.getX(); x < area.getRight(); x += (float)stepPixels)
    {
        double timeOffset = (x - centerX) / pixelsPerSec;
        double trackTime = currentPos + timeOffset;

        if (trackTime >= 0.0 && trackTime <= totalDur)
        {
            size_t idx = static_cast<size_t>((trackTime / totalDur) * (double)numPeaks);
            if (idx >= numPeaks) idx = numPeaks - 1;

            float lAmp = low[idx] * height * 0.9f;
            float mAmp = mid[idx] * height * 0.9f;
            float hAmp = high[idx] * height * 0.9f;

            float dir = invert ? 1.0f : -1.0f;

            // Low frequency (Red)
            g.setColour(juce::Colour::fromRGB(230, 57, 70));
            g.fillRect(x, baselineY, (float)stepPixels, dir * lAmp * 0.85f);

            // Mid frequency (Amber)
            g.setColour(juce::Colour::fromRGB(255, 159, 28));
            g.fillRect(x, baselineY, (float)stepPixels, dir * mAmp * 0.75f);

            // High frequency (Deck Accent)
            g.setColour(accent);
            g.fillRect(x, baselineY, (float)stepPixels, dir * hAmp * 0.65f);
        }
    }
}

void ScrollingWaveform::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    float halfH = bounds.getHeight() * 0.5f;

    // Deck 1 (top half, waveform grows downward toward center)
    drawDeckWaveform(g, deck1, bounds.removeFromTop(halfH), juce::Colour::fromRGB(0, 180, 216), true);

    // Deck 2 (bottom half, waveform grows upward toward center)
    drawDeckWaveform(g, deck2, bounds, juce::Colour::fromRGB(0, 229, 255), false);

    // Center divider
    g.setColour(juce::Colour::fromRGB(36, 40, 52));
    g.drawHorizontalLine((int)halfH, 0.0f, (float)getWidth());

    // Center stationary Playhead Needle (Red)
    float centerX = bounds.getCentreX();
    float totalH = (float)getHeight();
    g.setColour(juce::Colour::fromRGB(255, 51, 75).withAlpha(0.35f));
    g.drawLine(centerX, 0.0f, centerX, totalH, 5.0f);

    g.setColour(juce::Colour::fromRGB(255, 51, 75));
    g.drawLine(centerX, 0.0f, centerX, totalH, 2.0f);
}

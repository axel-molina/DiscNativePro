#include "VerticalWaveform.h"
#include <cmath>

VerticalWaveform::VerticalWaveform(DeckPlayer& p1, DeckPlayer& p2)
    : deck1(p1), deck2(p2)
{
    startTimerHz(60);
}

VerticalWaveform::~VerticalWaveform()
{
    stopTimer();
}

void VerticalWaveform::timerCallback()
{
    if (deck1.isPlaying() || deck2.isPlaying())
    {
        repaint();
    }
}

void VerticalWaveform::renderDeckHalf(juce::Graphics& g,
                                      DeckPlayer& player,
                                      float startX,
                                      float deckWidth,
                                      float centerY,
                                      float totalHeight,
                                      bool isLeft)
{
    float deckCenterX = std::floor(startX + deckWidth * 0.5f);

    if (!player.isLoaded() || player.getLengthInSeconds() <= 0.0)
    {
        // Subtle vertical centerline guide when no track is loaded
        g.setColour(juce::Colour::fromRGB(20, 22, 31));
        g.drawVerticalLine(static_cast<int>(deckCenterX), 0.0f, totalHeight);
        return;
    }

    const auto& low = player.getPeaksLow();
    const auto& mid = player.getPeaksMid();
    const auto& high = player.getPeaksHigh();
    if (low.empty()) return;

    size_t numPeaks = low.size();
    double totalDur = player.getLengthInSeconds();
    double currentTime = player.getPosition();
    double bpm = player.getBpm();
    if (bpm <= 20.0) bpm = 124.0;
    double beatInterval = 60.0 / bpm;

    const float pixelsPerSecond = 140.0f;
    float maxHalfAmp = deckWidth * 0.44f;
    float barHeight = 2.0f;

    // Time window visible on screen (scrolling downwards into centerY)
    float pastTimeSpan = (totalHeight - centerY + 10.0f) / pixelsPerSecond;
    float futureTimeSpan = (centerY + 10.0f) / pixelsPerSecond;

    double minVisibleTime = std::max(0.0, currentTime - pastTimeSpan);
    double maxVisibleTime = std::min(totalDur, currentTime + futureTimeSpan);

    // Render 3-band stepped frequency bars
    for (float y = 0.0f; y < totalHeight; y += barHeight)
    {
        // Exact time offset for this Y coordinate
        double sliceTime = currentTime + (centerY - y) / pixelsPerSecond;
        if (sliceTime < 0.0 || sliceTime > totalDur)
            continue;

        size_t idx = static_cast<size_t>((sliceTime / totalDur) * (double)numPeaks);
        if (idx >= numPeaks) idx = numPeaks - 1;

        float lowVal = low[idx];
        float midVal = mid[idx];
        float highVal = high[idx];

        float lowW = std::max(1.0f, lowVal * maxHalfAmp);
        float midW = std::max(1.0f, midVal * maxHalfAmp);
        float highW = std::max(1.0f, highVal * maxHalfAmp);

        // 1. High frequency band (Electric Cyan #00e5ff) - Transients / hats shoot wide
        if (highVal > 0.08f)
        {
            g.setColour(juce::Colour::fromRGB(0, 229, 255));
            g.fillRect(deckCenterX - highW, y, highW * 2.0f, barHeight);
        }

        // 2. Mid frequency band (Amber Orange #ff9f1c) - Vocals / body
        if (midVal > 0.06f)
        {
            g.setColour(juce::Colour::fromRGB(255, 159, 28));
            g.fillRect(deckCenterX - midW, y, midW * 2.0f, barHeight);
        }

        // 3. Low frequency band (Crimson Red #ff2a4b) - Kick / sub core
        if (lowVal > 0.04f)
        {
            g.setColour(juce::Colour::fromRGB(255, 42, 75));
            g.fillRect(deckCenterX - lowW, y, lowW * 2.0f, barHeight);
        }
    }

    // Beatgrid lines and downbeat measure numbers
    int firstBeat = static_cast<int>(std::floor(minVisibleTime / beatInterval));
    int lastBeat = static_cast<int>(std::ceil(maxVisibleTime / beatInterval));

    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));

    for (int b = firstBeat; b <= lastBeat; ++b)
    {
        double beatTime = (double)b * beatInterval;
        float y = centerY - static_cast<float>((beatTime - currentTime) * (double)pixelsPerSecond);

        if (y >= 0.0f && y <= totalHeight)
        {
            bool isBar = (b % 4 == 0);
            int measureNum = (b / 4) + 1;

            if (isBar)
            {
                g.setColour(juce::Colours::white.withAlpha(0.45f));
                g.drawHorizontalLine(static_cast<int>(y), startX + (isLeft ? 8.0f : 0.0f), startX + deckWidth - (isLeft ? 0.0f : 8.0f));

                // Measure number text along the center line
                g.setColour(juce::Colours::white.withAlpha(0.85f));
                float textX = isLeft ? (startX + deckWidth - 14.0f) : (startX + 4.0f);
                g.drawText(juce::String(measureNum),
                           static_cast<int>(textX), static_cast<int>(y - 12.0f), 16, 12,
                           isLeft ? juce::Justification::centredRight : juce::Justification::centredLeft, false);
            }
            else
            {
                g.setColour(juce::Colours::white.withAlpha(0.12f));
                g.drawHorizontalLine(static_cast<int>(y), startX + (isLeft ? 16.0f : 0.0f), startX + deckWidth - (isLeft ? 0.0f : 16.0f));
            }
        }
    }
}

void VerticalWaveform::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    float width = bounds.getWidth();
    float height = bounds.getHeight();
    float halfWidth = std::floor(width * 0.5f);
    float centerY = std::floor(height * 0.5f);

    // 1. Deep dark background #090a0e
    g.fillAll(juce::Colour::fromRGB(9, 10, 14));

    // 2. Render Deck 1 (Left Half)
    renderDeckHalf(g, deck1, 0.0f, halfWidth, centerY, height, true);

    // 3. Render Deck 2 (Right Half)
    renderDeckHalf(g, deck2, halfWidth, halfWidth, centerY, height, false);

    // 4. Center Divider line #1a1d26
    g.setColour(juce::Colour::fromRGB(26, 29, 38));
    g.drawVerticalLine(static_cast<int>(halfWidth), 0.0f, height);

    // 5. Horizontal Center Playhead Needle (Glowing Red #ff2a4b)
    g.setColour(juce::Colour::fromRGB(255, 42, 75).withAlpha(0.35f));
    g.drawLine(0.0f, centerY, width, centerY, 4.0f);

    g.setColour(juce::Colour::fromRGB(255, 42, 75));
    g.drawLine(0.0f, centerY, width, centerY, 2.0f);

    // 6. Top Deck Badges (Cyan "1" on left, Cyan "2" on right)
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.setColour(juce::Colour::fromRGB(0, 229, 255));
    g.drawText("1", 0, 4, static_cast<int>(halfWidth), 16, juce::Justification::centred, false);
    g.drawText("2", static_cast<int>(halfWidth), 4, static_cast<int>(halfWidth), 16, juce::Justification::centred, false);
}

#include "OverviewWaveform.h"

OverviewWaveform::OverviewWaveform(DeckPlayer& player, juce::Colour deckAccent)
    : deck(player), accent(deckAccent)
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

OverviewWaveform::~OverviewWaveform()
{
}

void OverviewWaveform::seekToMouse(float mouseX)
{
    double dur = deck.getLengthInSeconds();
    if (dur > 0.0 && getWidth() > 0)
    {
        float norm = juce::jlimit(0.0f, 1.0f, mouseX / (float)getWidth());
        deck.setPosition((double)norm * dur);
        repaint();
    }
}

void OverviewWaveform::mouseDown(const juce::MouseEvent& e)
{
    seekToMouse(e.position.x);
}

void OverviewWaveform::mouseDrag(const juce::MouseEvent& e)
{
    seekToMouse(e.position.x);
}

void OverviewWaveform::updateWaveform()
{
    repaint();
}

void OverviewWaveform::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    float width = bounds.getWidth();
    float height = bounds.getHeight();
    float halfHeight = height * 0.5f;

    // Background #111317
    g.setColour(juce::Colour::fromRGB(17, 19, 23));
    g.fillRoundedRectangle(bounds, 4.0f);

    g.setColour(juce::Colour::fromRGB(36, 40, 50));
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

    // Center baseline
    g.setColour(juce::Colour::fromRGB(35, 39, 48));
    g.drawHorizontalLine((int)halfHeight, bounds.getX(), bounds.getRight());

    double dur = deck.getLengthInSeconds();
    if (!deck.isLoaded() || dur <= 0.0)
    {
        return;
    }

    const auto& low = deck.getPeaksLow();
    const auto& mid = deck.getPeaksMid();
    const auto& high = deck.getPeaksHigh();

    if (!low.empty())
    {
        size_t numPeaks = low.size();

        for (int x = 0; x < (int)width; ++x)
        {
            size_t idx = static_cast<size_t>((float)x / width * (float)numPeaks);
            if (idx >= numPeaks) idx = numPeaks - 1;

            float lAmp = low[idx] * halfHeight;
            float mAmp = mid[idx] * halfHeight;
            float hAmp = high[idx] * halfHeight;

            // 1. Low frequency bass base (Red #e63946)
            g.setColour(juce::Colour::fromRGB(230, 57, 70));
            g.fillRect((float)x, halfHeight - lAmp * 0.8f, 1.0f, lAmp * 1.6f);

            // 2. Mid frequency vocals/leads (Amber #ff9f1c)
            g.setColour(juce::Colour::fromRGB(255, 159, 28));
            g.fillRect((float)x, halfHeight - mAmp * 0.9f, 1.0f, mAmp * 1.8f);

            // 3. High frequency transients (Deck Accent)
            g.setColour(accent);
            g.fillRect((float)x, halfHeight - hAmp, 1.0f, hAmp * 2.0f);
        }
    }

    // Active Loop Region
    if (deck.isLooping())
    {
        float loopStartX = (float)(deck.getLoopStart() / dur) * width;
        float loopWidthX = (float)(deck.getLoopLength() / dur) * width;

        g.setColour(juce::Colour::fromRGB(37, 99, 235).withAlpha(0.35f));
        g.fillRect(loopStartX, 0.0f, loopWidthX, height);
        g.setColour(juce::Colour::fromRGB(96, 165, 250));
        g.drawRect(loopStartX, 0.0f, loopWidthX, height, 1.0f);
    }

    // Hot Cue Markers
    juce::Colour cueColours[4] = {
        juce::Colour::fromRGB(255, 51, 75),
        juce::Colour::fromRGB(255, 159, 28),
        juce::Colour::fromRGB(46, 196, 182),
        juce::Colour::fromRGB(0, 210, 255)
    };

    for (int i = 0; i < 4; ++i)
    {
        if (deck.hasHotCue(i))
        {
            float cueX = (float)(deck.getHotCue(i) / dur) * width;
            g.setColour(cueColours[i]);
            g.drawVerticalLine((int)cueX, 0.0f, height);

            // Cue flag marker
            juce::Path flag;
            flag.startNewSubPath(cueX, 0.0f);
            flag.lineTo(cueX + 8.0f, 0.0f);
            flag.lineTo(cueX, 7.0f);
            flag.closeSubPath();
            g.fillPath(flag);
        }
    }

    // Playhead Needle
    double currentPos = deck.getPosition();
    float needleX = (float)(currentPos / dur) * width;

    // Darkened played portion
    g.setColour(juce::Colours::black.withAlpha(0.25f));
    g.fillRect(0.0f, 0.0f, needleX, height);

    // Glowing needle line
    g.setColour(juce::Colour::fromRGB(255, 51, 75).withAlpha(0.4f));
    g.drawLine(needleX, 0.0f, needleX, height, 4.0f);

    g.setColour(juce::Colour::fromRGB(255, 51, 75));
    g.drawLine(needleX, 0.0f, needleX, height, 1.5f);
}

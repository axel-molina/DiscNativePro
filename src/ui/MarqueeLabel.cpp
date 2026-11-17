#include "MarqueeLabel.h"

MarqueeLabel::MarqueeLabel()
{
    setOpaque(false);
    setInterceptsMouseClicks(false, false);
}

void MarqueeLabel::setText(const juce::String& newText, juce::NotificationType /*notificationType*/)
{
    if (text == newText)
        return;

    text = newText;
    scrollOffset = 0.0f;
    phase = Phase::Hold;
    phaseElapsedMs = 0;

    updateOverflow();

    if (overflows)
        startTimerIfNeeded();
    else
        stopTimer();

    repaint();
}

void MarqueeLabel::setFont(const juce::Font& newFont)
{
    font = newFont;
    updateOverflow();
    repaint();
}

void MarqueeLabel::setFont(const juce::FontOptions& newFontOptions)
{
    font = juce::Font(newFontOptions);
    updateOverflow();
    repaint();
}

void MarqueeLabel::setJustificationType(juce::Justification newJustification)
{
    if (justification == newJustification)
        return;

    justification = newJustification;
    repaint();
}

void MarqueeLabel::setColour(juce::Label::ColourIds colourId, const juce::Colour& colour)
{
    if (colourId == juce::Label::textColourId && textColour != colour)
    {
        textColour = colour;
        repaint();
    }
}

void MarqueeLabel::resized()
{
    updateOverflow();
    repaint();
}

void MarqueeLabel::updateOverflow()
{
    const float textWidth = juce::GlyphArrangement::getStringWidth(font, text);
    const float availableWidth = (float)getWidth() - 4.0f;
    const bool nowOverflowing = text.isNotEmpty() && textWidth > availableWidth;

    if (nowOverflowing != overflows)
    {
        overflows = nowOverflowing;
        scrollOffset = 0.0f;
        phase = Phase::Hold;
        phaseElapsedMs = 0;

        if (overflows)
            startTimerIfNeeded();
        else
            stopTimer();
    }
}

void MarqueeLabel::startTimerIfNeeded()
{
    if (!isTimerRunning())
        startTimer(timerPeriodMs);
}

void MarqueeLabel::visibilityChanged()
{
    if (!isShowing() || !overflows)
        stopTimer();
    else
        startTimerIfNeeded();
}

void MarqueeLabel::timerCallback()
{
    if (!overflows || !isShowing())
    {
        stopTimer();
        return;
    }

    const float step = speedPxPerSecond * (float)timerPeriodMs / 1000.0f;

    if (phase == Phase::Hold)
    {
        phaseElapsedMs += timerPeriodMs;
        if (phaseElapsedMs >= holdMilliseconds)
        {
            phase = Phase::Scroll;
            phaseElapsedMs = 0;
        }
        repaint();
        return;
    }

    scrollOffset += step;

    const float firstCopyWidth = juce::GlyphArrangement::getStringWidth(font, text) + textSpacingPx;
    if (firstCopyWidth > 0.0f && scrollOffset >= firstCopyWidth)
    {
        scrollOffset = 0.0f;
        phase = Phase::Hold;
        phaseElapsedMs = 0;
    }

    repaint();
}

void MarqueeLabel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const float textWidth = juce::GlyphArrangement::getStringWidth(font, text);

    g.setColour(textColour);
    g.setFont(font);

    if (!overflows)
    {
        g.drawText(text, getLocalBounds(), justification, true);
        return;
    }

    const float y = bounds.getCentreY() - font.getAscent() * 0.5f;
    const float firstCopyWidth = textWidth + textSpacingPx;
    const float startX = 2.0f - scrollOffset;

    g.saveState();
    g.reduceClipRegion(getLocalBounds());

    g.drawText(text, juce::Rectangle<float> { startX, y, textWidth, font.getHeight() }, juce::Justification::topLeft, false);
    g.drawText(text, juce::Rectangle<float> { startX + firstCopyWidth, y, textWidth, font.getHeight() }, juce::Justification::topLeft, false);

    g.restoreState();
}

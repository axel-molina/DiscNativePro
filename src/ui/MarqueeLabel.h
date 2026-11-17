#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class MarqueeLabel : public juce::Component, private juce::Timer
{
public:
    MarqueeLabel();

    void setText(const juce::String& newText, juce::NotificationType notificationType = juce::sendNotification);
    juce::String getText() const { return text; }

    void setFont(const juce::Font& newFont);
    void setFont(const juce::FontOptions& newFontOptions);

    void setJustificationType(juce::Justification newJustification);
    juce::Justification getJustificationType() const { return justification; }

    void setColour(juce::Label::ColourIds colourId, const juce::Colour& colour);

    void resized() override;
    void paint(juce::Graphics& g) override;
    void visibilityChanged() override;

private:
    static constexpr int timerPeriodMs = 33;
    static constexpr float speedPxPerSecond = 38.0f;
    static constexpr float textSpacingPx = 48.0f;
    static constexpr int holdMilliseconds = 700;

    enum class Phase { Hold, Scroll };

    void timerCallback() override;
    void updateOverflow();
    void startTimerIfNeeded();

    juce::String text;
    juce::Font font { juce::FontOptions {} };
    juce::Justification justification { juce::Justification::centredLeft };
    juce::Colour textColour { juce::Colours::white };

    bool overflows { false };
    Phase phase { Phase::Hold };
    float scrollOffset { 0.0f };
    int phaseElapsedMs { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MarqueeLabel)
};

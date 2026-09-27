#include "ChannelStripComponent.h"

ChannelStripComponent::ChannelStripComponent(int channelIdx, ChannelStrip& chStrip, juce::Colour accent)
    : index(channelIdx), strip(chStrip), deckColor(accent)
{
    // 1. Header Badge: "CH 1" or "CH 2"
    headerBadge.setText(index == 0 ? "CH 1" : "CH 2", juce::dontSendNotification);
    headerBadge.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    headerBadge.setColour(juce::Label::textColourId, deckColor);
    headerBadge.setColour(juce::Label::backgroundColourId, juce::Colour::fromRGB(24, 27, 35));
    headerBadge.setColour(juce::Label::outlineColourId, juce::Colour::fromRGB(38, 43, 56));
    headerBadge.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(headerBadge);

    // 2. EQ Knobs & Filter
    highKnob.setRange(0.0, 1.0, 0.01);
    highKnob.setValue(0.5);
    highKnob.onValueChange = [this]() { strip.eqHigh.store((float)highKnob.getValue()); };
    addAndMakeVisible(highKnob);

    midKnob.setRange(0.0, 1.0, 0.01);
    midKnob.setValue(0.5);
    midKnob.onValueChange = [this]() { strip.eqMid.store((float)midKnob.getValue()); };
    addAndMakeVisible(midKnob);

    lowKnob.setRange(0.0, 1.0, 0.01);
    lowKnob.setValue(0.5);
    lowKnob.onValueChange = [this]() { strip.eqLow.store((float)lowKnob.getValue()); };
    addAndMakeVisible(lowKnob);

    filterKnob.setRange(-1.0, 1.0, 0.01);
    filterKnob.setValue(0.0);
    filterKnob.onValueChange = [this]() { strip.filterKnob.store((float)filterKnob.getValue()); };
    addAndMakeVisible(filterKnob);

    // 3. Channel Fader & Meter
    volumeFader.setLookAndFeel(&djLookAndFeel);
    volumeFader.setSliderStyle(juce::Slider::LinearVertical);
    volumeFader.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    volumeFader.setRange(0.0, 1.0, 0.01);
    volumeFader.setValue(0.85);
    volumeFader.onValueChange = [this]() { strip.volumeFader.store((float)volumeFader.getValue()); };
    addAndMakeVisible(volumeFader);

    addAndMakeVisible(vuMeter);

    // 4. CUE Button
    cueButton.setIcon(LucideIcons::IconType::Headphones, 12.0f);
    cueButton.setText("CUE");
    cueButton.setFontSize(10.5f);
    cueButton.setCornerRadius(4.0f);
    cueButton.setCustomColours(juce::Colour::fromRGB(24, 26, 34),
                               juce::Colour::fromRGB(113, 118, 132),
                               juce::Colour::fromRGB(113, 118, 132),
                               juce::Colour::fromRGB(38, 43, 56));
    cueButton.setClickingTogglesState(true);
    cueButton.onClick = [this]() {
        bool on = cueButton.getToggleState();
        cueButton.setCustomColours(on ? juce::Colour::fromRGB(245, 158, 11) : juce::Colour::fromRGB(24, 26, 34),
                                   on ? juce::Colours::black : juce::Colour::fromRGB(113, 118, 132),
                                   on ? juce::Colours::black : juce::Colour::fromRGB(113, 118, 132),
                                   on ? juce::Colour::fromRGB(251, 191, 36) : juce::Colour::fromRGB(38, 43, 56));
    };
    addAndMakeVisible(cueButton);

    startTimerHz(60);
}

ChannelStripComponent::~ChannelStripComponent()
{
    stopTimer();
    volumeFader.setLookAndFeel(nullptr);
}

void ChannelStripComponent::timerCallback()
{
    vuMeter.setLevels(strip.meterPeakLeft.load(), strip.meterPeakRight.load());

    // Sincronización visual en tiempo real de faders y perillas (MIDI / Automix / Shortcuts)
    if (!volumeFader.isMouseButtonDown())
        volumeFader.setValue(strip.volumeFader.load(), juce::dontSendNotification);

    if (!highKnob.isMouseButtonDown())
        highKnob.setValue(strip.eqHigh.load(), juce::dontSendNotification);

    if (!midKnob.isMouseButtonDown())
        midKnob.setValue(strip.eqMid.load(), juce::dontSendNotification);

    if (!lowKnob.isMouseButtonDown())
        lowKnob.setValue(strip.eqLow.load(), juce::dontSendNotification);

    if (!filterKnob.isMouseButtonDown())
        filterKnob.setValue(strip.filterKnob.load(), juce::dontSendNotification);
}

void ChannelStripComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Dark Channel Strip background #111318 with border #1e222d
    g.fillAll(juce::Colour::fromRGB(17, 19, 24));

    g.setColour(juce::Colour::fromRGB(30, 34, 45));
    g.drawVerticalLine(0, 0.0f, bounds.getHeight());
    g.drawVerticalLine(getWidth() - 1, 0.0f, bounds.getHeight());

    // Separator below header badge
    g.drawHorizontalLine(32, 4.0f, (float)getWidth() - 4.0f);

    // Separator between FILTER knob and Volume Fader (cleanly raised with ample fader headroom)
    if (faderSeparatorY > 0.0f)
    {
        g.setColour(juce::Colour::fromRGB(36, 42, 56));
        g.drawHorizontalLine((int)faderSeparatorY, 4.0f, (float)getWidth() - 4.0f);
    }
}

void ChannelStripComponent::resized()
{
    auto totalArea = getLocalBounds().reduced(4, 3);

    // 1. Header Badge (24px)
    headerBadge.setBounds(totalArea.removeFromTop(24).reduced(6, 2));
    totalArea.removeFromTop(8); // leaves gap at y = 32 for the top separator

    // 2. Bottom CUE button (24px)
    cueButton.setBounds(totalArea.removeFromBottom(24).reduced(4, 1));
    totalArea.removeFromBottom(8);

    // 3. Divide remaining height between EQ section and Fader section
    // Allocate an extended, generous height for the volume fader (130-180px, ~44% of available space)
    int availH = totalArea.getHeight();
    int faderH = juce::jlimit(130, 180, (int)((float)availH * 0.44f));
    auto faderArea = totalArea.removeFromBottom(faderH);

    // Separator line position (raised nicely above the fader)
    faderSeparatorY = (float)faderArea.getY() - 4.0f;
    faderArea.removeFromTop(4); // padding below separator line

    // 4. EQ Knobs section: perfectly centered vertically in the middle of remaining upper area
    int knobH = juce::jlimit(36, 44, (totalArea.getHeight() - 24) / 4);
    int knobSpacing = 3;
    int totalKnobsHeight = (knobH * 4) + (knobSpacing * 3);
    int topOffset = juce::jmax(2, (totalArea.getHeight() - totalKnobsHeight) / 2);

    totalArea.removeFromTop(topOffset);

    highKnob.setBounds(totalArea.removeFromTop(knobH).reduced(2, 1));
    totalArea.removeFromTop(knobSpacing);
    midKnob.setBounds(totalArea.removeFromTop(knobH).reduced(2, 1));
    totalArea.removeFromTop(knobSpacing);
    lowKnob.setBounds(totalArea.removeFromTop(knobH).reduced(2, 1));
    totalArea.removeFromTop(knobSpacing);
    filterKnob.setBounds(totalArea.removeFromTop(knobH).reduced(2, 1));

    // 5. Volume Fader & VU Meter: takes the extended fader height
    auto faderContent = faderArea.reduced(2, 0);
    vuMeter.setBounds(faderContent.removeFromRight(12).reduced(0, 2));
    faderContent.removeFromRight(4);
    volumeFader.setBounds(faderContent);
}

#include "CenterColumnComponent.h"

CenterColumnComponent::CenterColumnComponent(AudioEngine& engine)
    : audioEngine(engine),
      mixer(engine.getMixer()),
      verticalWaveform(engine.getDeck(0), engine.getDeck(1))
{
    // Top Controls: Master, Meter, Phones
    masterKnob.setRange(0.0, 1.5, 0.01);
    masterKnob.setValue(1.0);
    masterKnob.onValueChange = [this]() { mixer.setMasterVolume((float)masterKnob.getValue()); };
    addAndMakeVisible(masterKnob);

    addAndMakeVisible(masterMeter);

    phonesKnob.setRange(0.0, 1.5, 0.01);
    phonesKnob.setValue(1.0);
    addAndMakeVisible(phonesKnob);

    // Middle: Dual Vertical Waveform
    addAndMakeVisible(verticalWaveform);

    // Bottom: Crossfader header controls
    deck1Label.setText("1", juce::dontSendNotification);
    deck1Label.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    deck1Label.setColour(juce::Label::textColourId, juce::Colour::fromRGB(59, 130, 246));
    deck1Label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(deck1Label);

    curveButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(24, 27, 35));
    curveButton.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(142, 149, 165));
    curveButton.onClick = [this]() {
        if (curveButton.getButtonText() == "SMOOTH") curveButton.setButtonText("SCRATCH");
        else if (curveButton.getButtonText() == "SCRATCH") curveButton.setButtonText("LINEAR");
        else curveButton.setButtonText("SMOOTH");
    };
    addAndMakeVisible(curveButton);

    tempoBlendButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(24, 27, 35));
    tempoBlendButton.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(142, 149, 165));
    tempoBlendButton.setClickingTogglesState(true);
    tempoBlendButton.onClick = [this]() {
        bool on = tempoBlendButton.getToggleState();
        audioEngine.setTempoBlend(on);
        tempoBlendButton.setColour(juce::TextButton::buttonColourId, on ? juce::Colour::fromRGB(37, 99, 235).withAlpha(0.25f) : juce::Colour::fromRGB(24, 27, 35));
        tempoBlendButton.setColour(juce::TextButton::textColourOffId, on ? juce::Colour::fromRGB(147, 197, 253) : juce::Colour::fromRGB(142, 149, 165));
    };
    addAndMakeVisible(tempoBlendButton);

    deck2Label.setText("2", juce::dontSendNotification);
    deck2Label.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    deck2Label.setColour(juce::Label::textColourId, juce::Colour::fromRGB(203, 213, 225));
    deck2Label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(deck2Label);

    // Crossfader
    crossfader.setLookAndFeel(&djLookAndFeel);
    crossfader.setSliderStyle(juce::Slider::LinearHorizontal);
    crossfader.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    crossfader.setRange(-1.0, 1.0, 0.01);
    crossfader.setValue(0.0);
    crossfader.setDoubleClickReturnValue(true, 0.0);
    crossfader.onValueChange = [this]() {
        audioEngine.setCrossfader((float)crossfader.getValue());
    };
    addAndMakeVisible(crossfader);

    startTimerHz(60);
}

CenterColumnComponent::~CenterColumnComponent()
{
    stopTimer();
    crossfader.setLookAndFeel(nullptr);
}

void CenterColumnComponent::timerCallback()
{
    masterMeter.setLevels(mixer.getMasterPeakLeft(), mixer.getMasterPeakRight());

    // Synchronize crossfader if nudged via shortcuts / MIDI
    if (!crossfader.isMouseButtonDown())
    {
        crossfader.setValue(mixer.getCrossfader(), juce::dontSendNotification);
    }
}

void CenterColumnComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Dark base background #0c0d12 with borders #1c202a
    g.fillAll(juce::Colour::fromRGB(12, 13, 18));

    g.setColour(juce::Colour::fromRGB(28, 32, 42));
    g.drawVerticalLine(0, 0.0f, bounds.getHeight());
    g.drawVerticalLine(getWidth() - 1, 0.0f, bounds.getHeight());

    // Top section background #12141a
    g.setColour(juce::Colour::fromRGB(18, 20, 26));
    g.fillRect(0, 0, getWidth(), 46);
    g.setColour(juce::Colour::fromRGB(31, 35, 46));
    g.drawHorizontalLine(46, 0.0f, (float)getWidth());

    // Bottom section background #12141a
    int bottomY = getHeight() - 64;
    g.setColour(juce::Colour::fromRGB(18, 20, 26));
    g.fillRect(0, bottomY, getWidth(), 64);
    g.setColour(juce::Colour::fromRGB(31, 35, 46));
    g.drawHorizontalLine(bottomY, 0.0f, (float)getWidth());
}

void CenterColumnComponent::resized()
{
    auto area = getLocalBounds();

    // 1. Top Section (46px)
    auto topArea = area.removeFromTop(46).reduced(6, 2);
    masterKnob.setBounds(topArea.removeFromLeft(48).reduced(0, 1));
    phonesKnob.setBounds(topArea.removeFromRight(48).reduced(0, 1));
    masterMeter.setBounds(topArea.reduced(8, 4));

    // 2. Bottom Section (64px)
    auto bottomArea = area.removeFromBottom(64).reduced(6, 4);

    // Crossfader header controls (1, SMOOTH, TEMPO BLEND, 2)
    auto ctrlRow = bottomArea.removeFromTop(20);
    deck1Label.setBounds(ctrlRow.removeFromLeft(16));
    deck2Label.setBounds(ctrlRow.removeFromRight(16));
    ctrlRow.reduce(4, 0);
    curveButton.setBounds(ctrlRow.removeFromLeft(54).reduced(0, 1));
    ctrlRow.removeFromLeft(4);
    tempoBlendButton.setBounds(ctrlRow.reduced(0, 1));

    bottomArea.removeFromTop(4);
    crossfader.setBounds(bottomArea.reduced(4, 0));

    // 3. Middle Section: Dual Vertical Waveform fills all remaining space!
    verticalWaveform.setBounds(area);
}

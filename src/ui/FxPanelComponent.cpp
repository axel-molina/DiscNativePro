#include "FxPanelComponent.h"

FxPanelComponent::FxPanelComponent(DjMixer& djMixer)
    : mixer(djMixer)
{
    // ECHO Section
    echoToggleBtn.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(24, 28, 36));
    echoToggleBtn.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(96, 165, 250));
    echoToggleBtn.setClickingTogglesState(true);
    echoToggleBtn.onClick = [this]() {
        bool on = echoToggleBtn.getToggleState();
        mixer.setFxEnabled(on);
        echoToggleBtn.setColour(juce::TextButton::buttonColourId, on ? juce::Colour::fromRGB(37, 99, 235) : juce::Colour::fromRGB(24, 28, 36));
        echoToggleBtn.setColour(juce::TextButton::textColourOffId, on ? juce::Colours::white : juce::Colour::fromRGB(96, 165, 250));
    };
    addAndMakeVisible(echoToggleBtn);

    auto setupBeatBtn = [this](juce::TextButton& btn, float timeSec) {
        btn.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(20, 23, 30));
        btn.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(142, 149, 165));
        btn.onClick = [this, timeSec, &btn]() {
            mixer.setFxDelayTime(timeSec);
            beat1_4Btn.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(142, 149, 165));
            beat1_2Btn.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(142, 149, 165));
            beat3_4Btn.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(142, 149, 165));
            beat1_1Btn.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(142, 149, 165));
            btn.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(96, 165, 250));
        };
        addAndMakeVisible(btn);
    };

    setupBeatBtn(beat1_4Btn, 0.125f);
    setupBeatBtn(beat1_2Btn, 0.250f);
    setupBeatBtn(beat3_4Btn, 0.375f);
    setupBeatBtn(beat1_1Btn, 0.500f);
    beat1_2Btn.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(96, 165, 250)); // default

    echoFeedbackKnob.setRange(0.0, 0.85, 0.01);
    echoFeedbackKnob.setValue(0.35);
    echoFeedbackKnob.onValueChange = [this]() { mixer.setFxFeedback((float)echoFeedbackKnob.getValue()); };
    addAndMakeVisible(echoFeedbackKnob);

    echoMixKnob.setRange(0.0, 1.0, 0.01);
    echoMixKnob.setValue(0.30);
    echoMixKnob.onValueChange = [this]() { mixer.setFxMix((float)echoMixKnob.getValue()); };
    addAndMakeVisible(echoMixKnob);

    // REVERB Section
    reverbToggleBtn.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(24, 28, 36));
    reverbToggleBtn.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(255, 159, 28));
    reverbToggleBtn.setClickingTogglesState(true);
    reverbToggleBtn.onClick = [this]() {
        bool on = reverbToggleBtn.getToggleState();
        mixer.setReverbEnabled(on);
        reverbToggleBtn.setColour(juce::TextButton::buttonColourId, on ? juce::Colour::fromRGB(217, 119, 6) : juce::Colour::fromRGB(24, 28, 36));
        reverbToggleBtn.setColour(juce::TextButton::textColourOffId, on ? juce::Colours::white : juce::Colour::fromRGB(255, 159, 28));
    };
    addAndMakeVisible(reverbToggleBtn);

    reverbMixKnob.setRange(0.0, 1.0, 0.01);
    reverbMixKnob.setValue(0.25);
    reverbMixKnob.onValueChange = [this]() { mixer.setReverbMix((float)reverbMixKnob.getValue()); };
    addAndMakeVisible(reverbMixKnob);
}

FxPanelComponent::~FxPanelComponent()
{
}

void FxPanelComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Dark sleek FX bar #121419 with border #232733
    g.setColour(juce::Colour::fromRGB(18, 20, 25));
    g.fillRoundedRectangle(bounds.reduced(4.0f, 2.0f), 6.0f);

    g.setColour(juce::Colour::fromRGB(35, 39, 51));
    g.drawRoundedRectangle(bounds.reduced(4.0f, 2.0f), 6.0f, 1.0f);

    // Section dividers
    g.setColour(juce::Colour::fromRGB(32, 36, 46));
    g.drawVerticalLine(340, 6.0f, (float)getHeight() - 6.0f);
}

void FxPanelComponent::resized()
{
    auto area = getLocalBounds().reduced(8, 3);

    // Delay section (left)
    echoToggleBtn.setBounds(area.removeFromLeft(64).reduced(0, 4));
    area.removeFromLeft(8);

    beat1_4Btn.setBounds(area.removeFromLeft(30).reduced(0, 7));
    area.removeFromLeft(2);
    beat1_2Btn.setBounds(area.removeFromLeft(30).reduced(0, 7));
    area.removeFromLeft(2);
    beat3_4Btn.setBounds(area.removeFromLeft(30).reduced(0, 7));
    area.removeFromLeft(2);
    beat1_1Btn.setBounds(area.removeFromLeft(30).reduced(0, 7));
    area.removeFromLeft(12);

    echoFeedbackKnob.setBounds(area.removeFromLeft(48).reduced(0, 1));
    area.removeFromLeft(4);
    echoMixKnob.setBounds(area.removeFromLeft(48).reduced(0, 1));

    // Move to Reverb section (right of divider)
    area.removeFromLeft(24);
    reverbToggleBtn.setBounds(area.removeFromLeft(74).reduced(0, 4));
    area.removeFromLeft(8);
    reverbMixKnob.setBounds(area.removeFromLeft(48).reduced(0, 1));
}

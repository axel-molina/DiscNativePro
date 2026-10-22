#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../audio/AudioEngine.h"
#include "VerticalWaveform.h"
#include "DjKnob.h"
#include "VuMeter.h"
#include "DjLookAndFeel.h"

class CenterColumnComponent : public juce::Component,
                              public juce::Timer
{
public:
    CenterColumnComponent(AudioEngine& engine);
    ~CenterColumnComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    // Modo Ondas: la onda vertical se oculta y el centro aloja los faders de
    // volumen de cada bandeja (re-padreados desde MixerComponent).
    void setWavesMode(bool active, juce::Slider* fader1, juce::Slider* fader2);

private:
    AudioEngine& audioEngine;
    DjMixer& mixer;

    // Top section: Master & Phones
    DjKnob masterKnob { "MASTER", juce::Colour::fromRGB(156, 163, 175) };
    VuMeter masterMeter;
    DjKnob phonesKnob { "PHONES", juce::Colour::fromRGB(156, 163, 175) };

    // Middle section: Dual Vertical Waveform (u ondas: faders de volumen externos)
    VerticalWaveform verticalWaveform;
    bool wavesMode { false };
    juce::Slider* externalFader1 { nullptr };
    juce::Slider* externalFader2 { nullptr };

    // Bottom section: Crossfader Controls
    juce::Label deck1Label;
    juce::TextButton curveButton       { "SMOOTH" };
    juce::TextButton tempoBlendButton  { "TEMPO BLEND" };
    juce::Label deck2Label;
    juce::Slider crossfader;
    DjLookAndFeel djLookAndFeel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CenterColumnComponent)
};

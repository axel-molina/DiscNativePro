#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../audio/AudioEngine.h"
#include "ChannelStripComponent.h"
#include "CenterColumnComponent.h"

class MixerComponent : public juce::Component
{
public:
    MixerComponent(AudioEngine& engine);
    ~MixerComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Modo Ondas: los faders de volumen se reubican en el centro del mixer
    // (dónde estaba la onda vertical); las columnas quedan con EQ + VU.
    void setWavesMode(bool waves);

private:
    ChannelStripComponent channel1;
    CenterColumnComponent centerColumn;
    ChannelStripComponent channel2;
    bool wavesMode { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixerComponent)
};

#include "MixerComponent.h"

MixerComponent::MixerComponent(AudioEngine& engine)
    : channel1(0, engine.getMixer().getChannel(0), juce::Colour::fromRGB(0, 180, 216)),
      centerColumn(engine),
      channel2(1, engine.getMixer().getChannel(1), juce::Colour::fromRGB(148, 163, 184))
{
    addAndMakeVisible(channel1);
    addAndMakeVisible(centerColumn);
    addAndMakeVisible(channel2);
}

MixerComponent::~MixerComponent()
{
}

void MixerComponent::setWavesMode(bool waves)
{
    if (wavesMode == waves)
        return;

    wavesMode = waves;

    if (waves)
    {
        centerColumn.setWavesMode(true, &channel1.getVolumeFader(), &channel2.getVolumeFader());
        channel1.setWavesMode(true);
        channel2.setWavesMode(true);
    }
    else
    {
        centerColumn.setWavesMode(false, nullptr, nullptr);
        channel1.setWavesMode(false);
        channel2.setWavesMode(false);
    }

    resized();
}

void MixerComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(12, 13, 18));
}

void MixerComponent::resized()
{
    auto area = getLocalBounds();

    // Ondas: columnas más estrechas (solo EQ, sin VU ni fader)
    int stripWidth = wavesMode ? 72 : 96;
    channel1.setBounds(area.removeFromLeft(stripWidth));
    channel2.setBounds(area.removeFromRight(stripWidth));
    centerColumn.setBounds(area);
}

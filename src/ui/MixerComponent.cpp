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
        channel1.setFaderHostedElsewhere(true);
        channel2.setFaderHostedElsewhere(true);
    }
    else
    {
        centerColumn.setWavesMode(false, nullptr, nullptr);
        channel1.setFaderHostedElsewhere(false);
        channel2.setFaderHostedElsewhere(false);
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

    int stripWidth = 96;
    channel1.setBounds(area.removeFromLeft(stripWidth));
    channel2.setBounds(area.removeFromRight(stripWidth));
    centerColumn.setBounds(area);
}

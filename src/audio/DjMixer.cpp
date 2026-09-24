#include "DjMixer.h"
#include <cmath>

void ChannelStrip::prepare(double sampleRate, int /*blockSize*/)
{
    reset();

    // Prepare default coefficients
    auto defaultCoeffs = juce::dsp::IIR::Coefficients<float>::makeAllPass(sampleRate, 1000.0f);
    for (int ch = 0; ch < 2; ++ch)
    {
        lowFilter[ch].coefficients = defaultCoeffs;
        midFilter[ch].coefficients = defaultCoeffs;
        highFilter[ch].coefficients = defaultCoeffs;
        colorFilter[ch].coefficients = defaultCoeffs;
    }
}

void ChannelStrip::reset()
{
    for (int ch = 0; ch < 2; ++ch)
    {
        lowFilter[ch].reset();
        midFilter[ch].reset();
        highFilter[ch].reset();
        colorFilter[ch].reset();
    }
    meterPeakLeft.store(0.0f);
    meterPeakRight.store(0.0f);
}

void ChannelStrip::process(juce::AudioBuffer<float>& buffer, int numSamples, double sampleRate)
{
    if (numSamples <= 0 || sampleRate <= 0.0)
        return;

    // 1. Calculate 3-band EQ coefficients
    float lowKnob = juce::jlimit(0.0f, 1.0f, eqLow.load());
    float midKnob = juce::jlimit(0.0f, 1.0f, eqMid.load());
    float highKnob = juce::jlimit(0.0f, 1.0f, eqHigh.load());

    auto calcGain = [](float knob) -> float {
        if (knob <= 0.02f) return 0.0001f; // Kill (-80dB)
        if (knob < 0.5f)
            return juce::Decibels::decibelsToGain(juce::jmap(knob, 0.02f, 0.5f, -36.0f, 0.0f));
        return juce::Decibels::decibelsToGain(juce::jmap(knob, 0.5f, 1.0f, 0.0f, 6.0f));
    };

    auto lowCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowShelf(sampleRate, 250.0f, 0.707f, calcGain(lowKnob));
    auto midCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, 1000.0f, 0.8f, calcGain(midKnob));
    auto highCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate, 2500.0f, 0.707f, calcGain(highKnob));

    // 2. Color Filter (Bipolar Knob: Left = LPF, Center = Bypass, Right = HPF)
    float colKnob = juce::jlimit(-1.0f, 1.0f, filterKnob.load());
    juce::ReferenceCountedObjectPtr<juce::dsp::IIR::Coefficients<float>> colCoeffs = nullptr;

    if (colKnob < -0.03f)
    {
        // Low Pass Filter sweep (from 20 kHz down to 120 Hz)
        float norm = -colKnob;
        float cutoff = juce::jlimit(120.0f, 20000.0f, 20000.0f * std::pow(0.006f, norm));
        colCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, cutoff, 0.85f);
    }
    else if (colKnob > 0.03f)
    {
        // High Pass Filter sweep (from 20 Hz up to 8000 Hz)
        float norm = colKnob;
        float cutoff = juce::jlimit(20.0f, 8000.0f, 20.0f * std::pow(400.0f, norm));
        colCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, cutoff, 0.85f);
    }

    int numChannels = juce::jmin(2, buffer.getNumChannels());
    for (int ch = 0; ch < numChannels; ++ch)
    {
        lowFilter[ch].coefficients = lowCoeffs;
        midFilter[ch].coefficients = midCoeffs;
        highFilter[ch].coefficients = highCoeffs;

        auto* channelData = buffer.getWritePointer(ch);

        if (colCoeffs != nullptr)
        {
            colorFilter[ch].coefficients = colCoeffs;
            for (int i = 0; i < numSamples; ++i)
            {
                float s = channelData[i];
                s = lowFilter[ch].processSample(s);
                s = midFilter[ch].processSample(s);
                s = highFilter[ch].processSample(s);
                s = colorFilter[ch].processSample(s);
                channelData[i] = s;
            }
        }
        else
        {
            for (int i = 0; i < numSamples; ++i)
            {
                float s = channelData[i];
                s = lowFilter[ch].processSample(s);
                s = midFilter[ch].processSample(s);
                s = highFilter[ch].processSample(s);
                channelData[i] = s;
            }
        }
    }

    // 3. Apply Channel Gain & Volume Fader
    float totalChannelGain = gain.load() * volumeFader.load();
    buffer.applyGain(totalChannelGain);

    // 4. Update VU Meters with smooth decay
    float peakL = buffer.getMagnitude(0, 0, numSamples);
    float peakR = numChannels > 1 ? buffer.getMagnitude(1, 0, numSamples) : peakL;

    float prevL = meterPeakLeft.load();
    float prevR = meterPeakRight.load();
    meterPeakLeft.store(peakL > prevL ? peakL : (prevL * 0.92f));
    meterPeakRight.store(peakR > prevR ? peakR : (prevR * 0.92f));
}

DjMixer::DjMixer()
{
    delayBufferL.assign(maxDelaySamples, 0.0f);
    delayBufferR.assign(maxDelaySamples, 0.0f);
}

DjMixer::~DjMixer()
{
}

void DjMixer::prepare(double sampleRate, int blockSize)
{
    currentSampleRate = sampleRate;
    ch1.prepare(sampleRate, blockSize);
    ch2.prepare(sampleRate, blockSize);

    delayBufferL.assign(maxDelaySamples, 0.0f);
    delayBufferR.assign(maxDelaySamples, 0.0f);
    delayWritePos = 0;

    reverb.setSampleRate(sampleRate);
}

void DjMixer::reset()
{
    ch1.reset();
    ch2.reset();
    masterPeakLeft.store(0.0f);
    masterPeakRight.store(0.0f);

    std::fill(delayBufferL.begin(), delayBufferL.end(), 0.0f);
    std::fill(delayBufferR.begin(), delayBufferR.end(), 0.0f);
    delayWritePos = 0;
    reverb.reset();
}

void DjMixer::process(juce::AudioBuffer<float>& deck1Buffer,
                      juce::AudioBuffer<float>& deck2Buffer,
                      juce::AudioBuffer<float>& masterOutBuffer,
                      int numSamples)
{
    // Process individual channel strips
    ch1.process(deck1Buffer, numSamples, currentSampleRate);
    ch2.process(deck2Buffer, numSamples, currentSampleRate);

    // Crossfader calculation: constant power curve
    float xf = crossfader.load(); // -1.0 to 1.0
    float norm = (xf + 1.0f) * 0.5f; // 0.0 to 1.0
    float gain1 = std::cos(norm * juce::MathConstants<float>::halfPi);
    float gain2 = std::sin(norm * juce::MathConstants<float>::halfPi);

    masterOutBuffer.clear();

    int numChannels = juce::jmin(2, masterOutBuffer.getNumChannels());
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* masterData = masterOutBuffer.getWritePointer(ch);
        const auto* d1Data = deck1Buffer.getReadPointer(ch);
        const auto* d2Data = deck2Buffer.getReadPointer(ch);

        for (int i = 0; i < numSamples; ++i)
        {
            masterData[i] = (d1Data[i] * gain1) + (d2Data[i] * gain2);
        }
    }

    // Apply master volume
    float masterVol = masterVolume.load();
    masterOutBuffer.applyGain(masterVol);

    // Master FX Processing: Delay
    if (fxEnabled.load() && fxMix.load() > 0.001f)
    {
        float mix = fxMix.load();
        float fb = fxFeedback.load();
        int delaySamples = juce::jlimit(1, maxDelaySamples - 1, (int)(fxDelayTime.load() * currentSampleRate));
        auto* l = masterOutBuffer.getWritePointer(0);
        auto* r = numChannels > 1 ? masterOutBuffer.getWritePointer(1) : l;

        for (int i = 0; i < numSamples; ++i)
        {
            float dryL = l[i];
            float dryR = r[i];
            int readPos = (delayWritePos - delaySamples + maxDelaySamples) % maxDelaySamples;
            float delayedL = delayBufferL[static_cast<size_t>(readPos)];
            float delayedR = delayBufferR[static_cast<size_t>(readPos)];

            delayBufferL[static_cast<size_t>(delayWritePos)] = dryL + delayedL * fb;
            delayBufferR[static_cast<size_t>(delayWritePos)] = dryR + delayedR * fb;
            delayWritePos = (delayWritePos + 1) % maxDelaySamples;

            l[i] = dryL * (1.0f - mix) + delayedL * mix;
            r[i] = dryR * (1.0f - mix) + delayedR * mix;
        }
    }

    // Master FX Processing: Reverb
    if (reverbEnabled.load() && reverbMix.load() > 0.001f)
    {
        float rMix = reverbMix.load();
        auto* l = masterOutBuffer.getWritePointer(0);
        auto* r = numChannels > 1 ? masterOutBuffer.getWritePointer(1) : l;

        juce::Reverb::Parameters p;
        p.roomSize = 0.65f;
        p.damping = 0.35f;
        p.wetLevel = rMix;
        p.dryLevel = 1.0f - (rMix * 0.4f);
        p.width = 1.0f;
        reverb.setParameters(p);
        reverb.processStereo(l, r, numSamples);
    }

    // Update master meters
    float peakL = masterOutBuffer.getMagnitude(0, 0, numSamples);
    float peakR = numChannels > 1 ? masterOutBuffer.getMagnitude(1, 0, numSamples) : peakL;

    float prevL = masterPeakLeft.load();
    float prevR = masterPeakRight.load();
    masterPeakLeft.store(peakL > prevL ? peakL : (prevL * 0.92f));
    masterPeakRight.store(peakR > prevR ? peakR : (prevR * 0.92f));
}

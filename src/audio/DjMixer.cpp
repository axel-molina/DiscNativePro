#include "DjMixer.h"
#include <cmath>

void ChannelStrip::prepare(double sampleRate, int blockSize)
{
    reset();

    // Persistent EQ coefficient objects: allocated ONCE here (prepare is not the
    // audio thread). From now on process() only overwrites their values in place.
    lowEqCoeffs  = new juce::dsp::IIR::Coefficients<float>();
    midEqCoeffs  = new juce::dsp::IIR::Coefficients<float>();
    highEqCoeffs = new juce::dsp::IIR::Coefficients<float>();

    // Neutral (unity gain) coefficients, assigned in place. This also reserves the
    // coefficient storage so later in-place updates never grow the array.
    *lowEqCoeffs  = juce::dsp::IIR::ArrayCoefficients<float>::makeLowShelf(sampleRate, 250.0f, 0.707f, 1.0f);
    *midEqCoeffs  = juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter(sampleRate, 1000.0f, 0.8f, 1.0f);
    *highEqCoeffs = juce::dsp::IIR::ArrayCoefficients<float>::makeHighShelf(sampleRate, 2500.0f, 0.707f, 1.0f);

    lastAppliedLowGain  = 1.0f;
    lastAppliedMidGain  = 1.0f;
    lastAppliedHighGain = 1.0f;

    const juce::dsp::ProcessSpec spec { sampleRate,
                                        static_cast<juce::uint32>(juce::jmax(1, blockSize)),
                                        static_cast<juce::uint32>(2) };

    for (int ch = 0; ch < 2; ++ch)
    {
        lowFilter[ch].coefficients  = lowEqCoeffs;
        midFilter[ch].coefficients  = midEqCoeffs;
        highFilter[ch].coefficients = highEqCoeffs;

        colorLowFilter[ch].prepare(spec);
        colorLowFilter[ch].setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        colorLowFilter[ch].setResonance(0.707f);
        colorLowFilter[ch].setCutoffFrequency(juce::jmin(20000.0f, static_cast<float>(sampleRate) * 0.45f));

        colorHighFilter[ch].prepare(spec);
        colorHighFilter[ch].setType(juce::dsp::StateVariableTPTFilterType::highpass);
        colorHighFilter[ch].setResonance(0.707f);
        colorHighFilter[ch].setCutoffFrequency(juce::jmin(20.0f, static_cast<float>(sampleRate) * 0.45f));
    }

    // ~25 ms smoothing ramps, seeded with the current knob values (no jump on start).
    colorSmooth.reset(sampleRate, 0.025);
    eqLowSmooth.reset(sampleRate, 0.025);
    eqMidSmooth.reset(sampleRate, 0.025);
    eqHighSmooth.reset(sampleRate, 0.025);

    colorSmooth.setCurrentAndTargetValue(juce::jlimit(-1.0f, 1.0f, filterKnob.load()));
    eqLowSmooth.setCurrentAndTargetValue(juce::jlimit(0.0f, 1.0f, eqLow.load()));
    eqMidSmooth.setCurrentAndTargetValue(juce::jlimit(0.0f, 1.0f, eqMid.load()));
    eqHighSmooth.setCurrentAndTargetValue(juce::jlimit(0.0f, 1.0f, eqHigh.load()));
}

void ChannelStrip::reset()
{
    for (int ch = 0; ch < 2; ++ch)
    {
        lowFilter[ch].reset();
        midFilter[ch].reset();
        highFilter[ch].reset();
        colorLowFilter[ch].reset();
        colorHighFilter[ch].reset();
    }

    // Snap every smoother to its last target so a seek never replays a stale ramp.
    colorSmooth.setCurrentAndTargetValue(colorSmooth.getTargetValue());
    eqLowSmooth.setCurrentAndTargetValue(eqLowSmooth.getTargetValue());
    eqMidSmooth.setCurrentAndTargetValue(eqMidSmooth.getTargetValue());
    eqHighSmooth.setCurrentAndTargetValue(eqHighSmooth.getTargetValue());

    meterPeakLeft.store(0.0f);
    meterPeakRight.store(0.0f);
}

void ChannelStrip::process(juce::AudioBuffer<float>& buffer, int numSamples, double sampleRate)
{
    juce::AudioBuffer<float> dummy;
    process(buffer, dummy, numSamples, sampleRate);
}

void ChannelStrip::process(juce::AudioBuffer<float>& buffer, juce::AudioBuffer<float>& pflBuffer, int numSamples, double sampleRate)
{
    if (numSamples <= 0 || sampleRate <= 0.0)
        return;

    // The coefficient objects are created in prepare(); without them the strip is silent.
    if (lowEqCoeffs == nullptr || midEqCoeffs == nullptr || highEqCoeffs == nullptr)
        return;

    // 1. One smoothed value per block (~25 ms ramp): continuous across blocks, no zipper.
    eqLowSmooth.setTargetValue(juce::jlimit(0.0f, 1.0f, eqLow.load()));
    eqMidSmooth.setTargetValue(juce::jlimit(0.0f, 1.0f, eqMid.load()));
    eqHighSmooth.setTargetValue(juce::jlimit(0.0f, 1.0f, eqHigh.load()));
    colorSmooth.setTargetValue(juce::jlimit(-1.0f, 1.0f, filterKnob.load()));

    const float lowKnob  = eqLowSmooth.skip(numSamples);
    const float midKnob  = eqMidSmooth.skip(numSamples);
    const float highKnob = eqHighSmooth.skip(numSamples);
    const float colKnob  = colorSmooth.skip(numSamples);

    // Continuous taper: -80 dB kill at 0.0 -> 0 dB at 0.5 -> +6 dB at 1.0 (no jump).
    const auto gainForKnob = [](float knob) -> float
    {
        if (knob < 0.5f)
            return juce::Decibels::decibelsToGain(juce::jmap(knob, 0.0f, 0.5f, -80.0f, 0.0f));

        return juce::Decibels::decibelsToGain(juce::jmap(knob, 0.5f, 1.0f, 0.0f, 6.0f));
    };

    // 2. EQ coefficients assigned in place on the persistent objects (no allocation),
    //    only when the smoothed gain actually changed.
    const float lowGain  = gainForKnob(lowKnob);
    const float midGain  = gainForKnob(midKnob);
    const float highGain = gainForKnob(highKnob);

    if (std::abs(lowGain - lastAppliedLowGain) > 1e-4f)
    {
        *lowEqCoeffs = juce::dsp::IIR::ArrayCoefficients<float>::makeLowShelf(sampleRate, 250.0f, 0.707f, lowGain);
        lastAppliedLowGain = lowGain;
    }

    if (std::abs(midGain - lastAppliedMidGain) > 1e-4f)
    {
        *midEqCoeffs = juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter(sampleRate, 1000.0f, 0.8f, midGain);
        lastAppliedMidGain = midGain;
    }

    if (std::abs(highGain - lastAppliedHighGain) > 1e-4f)
    {
        *highEqCoeffs = juce::dsp::IIR::ArrayCoefficients<float>::makeHighShelf(sampleRate, 2500.0f, 0.707f, highGain);
        lastAppliedHighGain = highGain;
    }

    // 3. Color filter: dual SVF (LPF/HPF) with smoothstep crossfade at centre
    //    to avoid cutoff jumps when crossing zero.
    const float lpCutoffTarget = colKnob <= 0.0f
        ? 20000.0f * std::pow(0.006f, -colKnob)
        : 20000.0f;
    const float hpCutoffTarget = colKnob >= 0.0f
        ? 20.0f * std::pow(400.0f, colKnob)
        : 20.0f;

    const float lpCutoff = juce::jlimit(120.0f, 20000.0f, juce::jmin(lpCutoffTarget, static_cast<float>(sampleRate) * 0.45f));
    const float hpCutoff = juce::jlimit(20.0f, 8000.0f, juce::jmin(hpCutoffTarget, static_cast<float>(sampleRate) * 0.45f));

    for (int ch = 0; ch < 2; ++ch)
    {
        colorLowFilter[ch].setCutoffFrequency(lpCutoff);
        colorHighFilter[ch].setCutoffFrequency(hpCutoff);
    }

    // smoothstep in |knob| < 0.06
    float w = 0.0f;
    if (colKnob >= 0.0f)
    {
        const float t = juce::jlimit(0.0f, 1.0f, colKnob / 0.06f);
        w = t * t * (3.0f - 2.0f * t); // smoothstep
    }
    else
    {
        w = 0.0f;
    }
    const float wLow = 1.0f - w;

    int numChannels = juce::jmin(2, buffer.getNumChannels());
    for (int ch = 0; ch < numChannels; ++ch)
    {
        lowFilter[ch].coefficients  = lowEqCoeffs;
        midFilter[ch].coefficients  = midEqCoeffs;
        highFilter[ch].coefficients = highEqCoeffs;

        auto* channelData = buffer.getWritePointer(ch);

        for (int i = 0; i < numSamples; ++i)
        {
            float s = channelData[i];
            s = lowFilter[ch].processSample(s);
            s = midFilter[ch].processSample(s);
            s = highFilter[ch].processSample(s);
            float yLow = colorLowFilter[ch].processSample(ch, s);
            float yHigh = colorHighFilter[ch].processSample(ch, s);
            s = wLow * yLow + w * yHigh;
            channelData[i] = s;
        }

        colorLowFilter[ch].snapToZero();
        colorHighFilter[ch].snapToZero();
    }

    // 4. Apply Trim / Gain (PFL Signal)
    float channelGain = gain.load();
    buffer.applyGain(channelGain);

    // 4. Update VU Meters with smooth decay (monitoring PFL/Gain level)
    float peakL = buffer.getMagnitude(0, 0, numSamples);
    float peakR = numChannels > 1 ? buffer.getMagnitude(1, 0, numSamples) : peakL;

    float prevL = meterPeakLeft.load();
    float prevR = meterPeakRight.load();
    meterPeakLeft.store(peakL > prevL ? peakL : (prevL * 0.92f));
    meterPeakRight.store(peakR > prevR ? peakR : (prevR * 0.92f));

    // 5. If CUE is enabled, accumulate PFL signal into cue output buffer
    if (cueActive.load() && pflBuffer.getNumChannels() > 0)
    {
        int numPflCh = juce::jmin(buffer.getNumChannels(), pflBuffer.getNumChannels());
        for (int ch = 0; ch < numPflCh; ++ch)
        {
            pflBuffer.addFrom(ch, 0, buffer, ch, 0, numSamples);
        }
    }

    // 6. Apply Channel Volume Fader for Master Mix
    buffer.applyGain(volumeFader.load());
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
    fxChain1.prepare(sampleRate, blockSize);
    fxChain2.prepare(sampleRate, blockSize);

    delayBufferL.assign(maxDelaySamples, 0.0f);
    delayBufferR.assign(maxDelaySamples, 0.0f);
    delayWritePos = 0;

    reverb.setSampleRate(sampleRate);
}

void DjMixer::reset()
{
    ch1.reset();
    ch2.reset();
    fxChain1.reset();
    fxChain2.reset();
    masterPeakLeft.store(0.0f);
    masterPeakRight.store(0.0f);
    cuePeakLeft.store(0.0f);
    cuePeakRight.store(0.0f);

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
    juce::AudioBuffer<float> dummyCue;
    process(deck1Buffer, deck2Buffer, masterOutBuffer, dummyCue, numSamples);
}

void DjMixer::process(juce::AudioBuffer<float>& deck1Buffer,
                      juce::AudioBuffer<float>& deck2Buffer,
                      juce::AudioBuffer<float>& masterOutBuffer,
                      juce::AudioBuffer<float>& cueOutBuffer,
                      int numSamples)
{
    if (cueOutBuffer.getNumChannels() > 0)
        cueOutBuffer.clear(0, numSamples);

    // Process individual channel strips (accumulates into cueOutBuffer if PFL CUE is active)
    ch1.process(deck1Buffer, cueOutBuffer, numSamples, currentSampleRate);
    ch2.process(deck2Buffer, cueOutBuffer, numSamples, currentSampleRate);

    // Process Deck multi-FX chains before crossfader
    fxChain1.process(deck1Buffer, numSamples, deck1Bpm.load());
    fxChain2.process(deck2Buffer, numSamples, deck2Bpm.load());

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

    // Process CUE Output (Headphones / Preescucha)
    if (cueOutBuffer.getNumChannels() > 0)
    {
        float mixVal = cueMix.load(); // 0.0 = Pure Cue, 1.0 = Pure Master
        if (mixVal > 0.001f)
        {
            int cueChannels = juce::jmin(cueOutBuffer.getNumChannels(), masterOutBuffer.getNumChannels());
            for (int ch = 0; ch < cueChannels; ++ch)
            {
                auto* cueData = cueOutBuffer.getWritePointer(ch);
                const auto* mData = masterOutBuffer.getReadPointer(ch);
                for (int i = 0; i < numSamples; ++i)
                {
                    cueData[i] = (cueData[i] * (1.0f - mixVal)) + (mData[i] * mixVal);
                }
            }
        }

        // Apply Headphone Volume
        float pVol = phonesVolume.load();
        cueOutBuffer.applyGain(pVol);

        // Update CUE meters
        float cPeakL = cueOutBuffer.getMagnitude(0, 0, numSamples);
        float cPeakR = cueOutBuffer.getNumChannels() > 1 ? cueOutBuffer.getMagnitude(1, 0, numSamples) : cPeakL;
        float prevCL = cuePeakLeft.load();
        float prevCR = cuePeakRight.load();
        cuePeakLeft.store(cPeakL > prevCL ? cPeakL : (prevCL * 0.92f));
        cuePeakRight.store(cPeakR > prevCR ? cPeakR : (prevCR * 0.92f));
    }
}

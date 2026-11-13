#include "DeckFxChain.h"
#include <cmath>
#include <algorithm>

void FxSlot::prepare(double sampleRate, int blockSize)
{
    delayBufferL.assign(maxDelaySamples, 0.0f);
    delayBufferR.assign(maxDelaySamples, 0.0f);
    delayWritePos = 0;

    reverb.setSampleRate(sampleRate);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(blockSize);
    spec.numChannels = 2;

    chorus.prepare(spec);
    phaser.prepare(spec);

    crushHoldCount = 0;
    crushHeldL = 0.0f;
    crushHeldR = 0.0f;
}

void FxSlot::reset()
{
    std::fill(delayBufferL.begin(), delayBufferL.end(), 0.0f);
    std::fill(delayBufferR.begin(), delayBufferR.end(), 0.0f);
    delayWritePos = 0;

    reverb.reset();
    chorus.reset();
    phaser.reset();

    crushHoldCount = 0;
    crushHeldL = 0.0f;
    crushHeldR = 0.0f;
}

void FxSlot::process(juce::AudioBuffer<float>& buffer,
                     juce::AudioBuffer<float>& scratch,
                     int numSamples,
                     double sampleRate,
                     double currentBpm)
{
    if (!enabled.load())
        return;

    float wet = wetDry.load();
    if (wet <= 0.001f)
        return;

    int numChannels = juce::jmin(2, buffer.getNumChannels());

    // 1. Copy dry input into pre-allocated scratch buffer
    for (int ch = 0; ch < numChannels; ++ch)
    {
        scratch.copyFrom(ch, 0, buffer, ch, 0, numSamples);
    }

    float param = parameter.load();
    auto fx = static_cast<FxType>(type.load());

    // 2. Process DSP into buffer
    switch (fx)
    {
        case FxType::Echo:
        {
            // P21: retardo = división de beat seleccionada (tabla de 5 divisiones)
            static constexpr float divTable[] = { 0.25f, 0.5f, 0.75f, 1.0f, 2.0f };
            float beatSec = (currentBpm > 30.0) ? static_cast<float>(60.0 / currentBpm) : 0.5f;
            float delaySec = beatSec * divTable[juce::jlimit(0, 4, beatDivision.load())];

            int delaySamples = juce::jlimit(1, maxDelaySamples - 1, static_cast<int>(delaySec * sampleRate));
            float feedback = 0.48f;

            auto* l = buffer.getWritePointer(0);
            auto* r = numChannels > 1 ? buffer.getWritePointer(1) : l;

            for (int i = 0; i < numSamples; ++i)
            {
                float dryL = l[i];
                float dryR = r[i];

                int readPos = (delayWritePos - delaySamples + maxDelaySamples) % maxDelaySamples;
                float delayedL = delayBufferL[static_cast<size_t>(readPos)];
                float delayedR = delayBufferR[static_cast<size_t>(readPos)];

                delayBufferL[static_cast<size_t>(delayWritePos)] = dryL + delayedL * feedback;
                delayBufferR[static_cast<size_t>(delayWritePos)] = dryR + delayedR * feedback;

                delayWritePos = (delayWritePos + 1) % maxDelaySamples;

                l[i] = delayedL;
                if (numChannels > 1)
                    r[i] = delayedR;
            }
            break;
        }

        case FxType::Reverb:
        {
            juce::Reverb::Parameters p;
            p.roomSize = juce::jlimit(0.15f, 0.95f, param);
            p.damping = 0.4f;
            p.wetLevel = 1.0f;
            p.dryLevel = 0.0f;
            p.width = 1.0f;
            reverb.setParameters(p);

            float* l = buffer.getWritePointer(0);
            float* r = numChannels > 1 ? buffer.getWritePointer(1) : l;
            reverb.processStereo(l, r, numSamples);
            break;
        }

        case FxType::Flanger:
        {
            float rate = 0.2f + param * 2.8f;
            chorus.setRate(rate);
            chorus.setDepth(0.85f);
            chorus.setCentreDelay(3.5f);
            chorus.setFeedback(0.55f);
            chorus.setMix(1.0f);

            juce::dsp::AudioBlock<float> block(buffer);
            auto subBlock = block.getSubBlock(0, static_cast<size_t>(numSamples));
            juce::dsp::ProcessContextReplacing<float> context(subBlock);
            chorus.process(context);
            break;
        }

        case FxType::Phaser:
        {
            float rate = 0.2f + param * 3.5f;
            phaser.setRate(rate);
            phaser.setDepth(0.9f);
            phaser.setFeedback(0.5f);
            phaser.setMix(1.0f);

            juce::dsp::AudioBlock<float> block(buffer);
            auto subBlock = block.getSubBlock(0, static_cast<size_t>(numSamples));
            juce::dsp::ProcessContextReplacing<float> context(subBlock);
            phaser.process(context);
            break;
        }

        case FxType::BitCrusher:
        {
            float bits = juce::jmap(param, 0.0f, 1.0f, 12.0f, 3.0f);
            float steps = std::pow(2.0f, bits);
            int holdSamples = 1 + static_cast<int>(param * 7.0f);

            auto* l = buffer.getWritePointer(0);
            auto* r = numChannels > 1 ? buffer.getWritePointer(1) : l;

            for (int i = 0; i < numSamples; ++i)
            {
                crushHoldCount++;
                if (crushHoldCount >= holdSamples)
                {
                    crushHoldCount = 0;
                    crushHeldL = std::round(l[i] * steps) / steps;
                    crushHeldR = std::round(r[i] * steps) / steps;
                }
                l[i] = crushHeldL;
                if (numChannels > 1)
                    r[i] = crushHeldR;
            }
            break;
        }

        case FxType::Gate:
        {
            // P21: pulso rítmico con período = división de beat; si hay fase de
            // beat publicada por el deck, el ciclo queda anclado al downbeat.
            static constexpr float divTable[] = { 0.25f, 0.5f, 0.75f, 1.0f, 2.0f };
            const float div = divTable[juce::jlimit(0, 4, beatDivision.load())];
            float beatSec = (currentBpm > 30.0) ? static_cast<float>(60.0 / currentBpm) : 0.5f;
            double periodSec = static_cast<double>(beatSec) * div;
            double phaseStep = 1.0 / (periodSec * sampleRate);

            double phase = beatPhase.load();
            bool anchored = (phase >= 0.0 && phase < 1.0);
            double gatePhase = anchored ? phase / div : 0.0;

            auto* l = buffer.getWritePointer(0);
            auto* r = numChannels > 1 ? buffer.getWritePointer(1) : l;

            for (int i = 0; i < numSamples; ++i)
            {
                gatePhase += phaseStep;
                if (gatePhase >= 1.0)
                    gatePhase -= std::floor(gatePhase);

                // Square pulse with soft cosine transitions to eliminate clicks
                float rawPulse = std::sin(gatePhase * juce::MathConstants<double>::twoPi) > 0.0 ? 1.0f : 0.0f;
                float mod = rawPulse * 0.95f + 0.05f;

                l[i] *= mod;
                if (numChannels > 1)
                    r[i] *= mod;
            }
            break;
        }
    }

    // 3. Dry/Wet Crossfade: buffer = dry * (1 - wet) + wetSample * wet
    float dryGain = 1.0f - wet;
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* out = buffer.getWritePointer(ch);
        const auto* inDry = scratch.getReadPointer(ch);
        for (int i = 0; i < numSamples; ++i)
        {
            out[i] = (inDry[i] * dryGain) + (out[i] * wet);
        }
    }
}

DeckFxChain::DeckFxChain()
{
    // Default FX assignments matching DJ software standard
    slots[0].type.store(static_cast<int>(FxType::Echo));
    slots[1].type.store(static_cast<int>(FxType::Flanger));
    slots[2].type.store(static_cast<int>(FxType::Gate));
}

void DeckFxChain::prepare(double sampleRate, int blockSize)
{
    currentSampleRate = sampleRate;
    currentBlockSize = blockSize;

    scratchBuffer.setSize(2, blockSize, false, false, true);

    for (auto& slot : slots)
    {
        slot.prepare(sampleRate, blockSize);
    }
}

void DeckFxChain::reset()
{
    scratchBuffer.clear();
    for (auto& slot : slots)
    {
        slot.reset();
    }
}

void DeckFxChain::process(juce::AudioBuffer<float>& buffer, int numSamples, double currentBpm)
{
    if (scratchBuffer.getNumSamples() < numSamples)
        scratchBuffer.setSize(2, numSamples, false, false, true);

    for (auto& slot : slots)
    {
        if (slot.enabled.load())
        {
            slot.process(buffer, scratchBuffer, numSamples, currentSampleRate, currentBpm);
        }
    }
}

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <array>
#include <vector>

enum class FxType : int
{
    Echo = 0,
    Reverb = 1,
    Flanger = 2,
    Phaser = 3,
    BitCrusher = 4,
    Gate = 5
};

inline const char* getFxName(FxType type)
{
    switch (type)
    {
        case FxType::Echo:       return "Echo";
        case FxType::Reverb:     return "Reverb";
        case FxType::Flanger:    return "Flanger";
        case FxType::Phaser:     return "Phaser";
        case FxType::BitCrusher: return "Bit Crusher";
        case FxType::Gate:       return "Gate";
    }
    return "None";
}

struct FxSlot
{
    std::atomic<int> type { static_cast<int>(FxType::Echo) };
    std::atomic<bool> enabled { false };
    std::atomic<float> wetDry { 0.5f };      // 0.0 to 1.0
    std::atomic<float> parameter { 0.5f };   // Parameter (rate, room size, etc.)

    // P21: división rítmica para Echo/Gate (0=1/4, 1=1/2, 2=3/4, 3=1/1, 4=2/1)
    std::atomic<int> beatDivision { 2 };
    // Fase de beat del deck (0..1); -1.0 = sin anclaje (el mixer la publica por bloque)
    std::atomic<double> beatPhase { -1.0 };

    // Circular delay buffer for Echo (pre-allocated up to 96000 samples)
    static constexpr int maxDelaySamples = 96000;
    std::vector<float> delayBufferL;
    std::vector<float> delayBufferR;
    int delayWritePos { 0 };

    // Native JUCE DSP processors
    juce::Reverb reverb;
    juce::dsp::Chorus<float> chorus;
    juce::dsp::Phaser<float> phaser;

    // BitCrusher state
    int crushHoldCount { 0 };
    float crushHeldL { 0.0f };
    float crushHeldR { 0.0f };

    void prepare(double sampleRate, int blockSize);
    void reset();
    void process(juce::AudioBuffer<float>& buffer,
                 juce::AudioBuffer<float>& scratch,
                 int numSamples,
                 double sampleRate,
                 double currentBpm);
};

class DeckFxChain
{
public:
    DeckFxChain();
    ~DeckFxChain() = default;

    void prepare(double sampleRate, int blockSize);
    void reset();
    void process(juce::AudioBuffer<float>& buffer, int numSamples, double currentBpm);

    static constexpr int numSlots = 3;
    FxSlot& getSlot(int index) { return slots[static_cast<size_t>(juce::jlimit(0, numSlots - 1, index))]; }
    const FxSlot& getSlot(int index) const { return slots[static_cast<size_t>(juce::jlimit(0, numSlots - 1, index))]; }

private:
    double currentSampleRate { 44100.0 };
    int currentBlockSize { 512 };
    std::array<FxSlot, numSlots> slots;

    // Pre-allocated scratch buffer for real-time wet/dry mixing
    juce::AudioBuffer<float> scratchBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeckFxChain)
};

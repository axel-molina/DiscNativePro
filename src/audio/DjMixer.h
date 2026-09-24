#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <vector>

struct ChannelStrip
{
    std::atomic<float> gain { 1.0f };       // Linear gain (1.0 = 0 dB)
    std::atomic<float> eqHigh { 0.5f };     // 0.0 = Kill (-30dB), 0.5 = 0dB, 1.0 = +6dB
    std::atomic<float> eqMid { 0.5f };      // 0.0 = Kill (-30dB), 0.5 = 0dB, 1.0 = +6dB
    std::atomic<float> eqLow { 0.5f };      // 0.0 = Kill (-30dB), 0.5 = 0dB, 1.0 = +6dB
    std::atomic<float> filterKnob { 0.0f }; // -1.0 = LPF, 0.0 = Bypass, +1.0 = HPF
    std::atomic<float> volumeFader { 1.0f };// 0.0 to 1.0

    // Meter levels (0.0 to 1.0 peak)
    std::atomic<float> meterPeakLeft { 0.0f };
    std::atomic<float> meterPeakRight { 0.0f };

    // DSP filters for 2 channels (Left & Right)
    juce::dsp::IIR::Filter<float> lowFilter[2];
    juce::dsp::IIR::Filter<float> midFilter[2];
    juce::dsp::IIR::Filter<float> highFilter[2];
    juce::dsp::IIR::Filter<float> colorFilter[2];

    void prepare(double sampleRate, int blockSize);
    void reset();
    void process(juce::AudioBuffer<float>& buffer, int numSamples, double sampleRate);
};

class DjMixer
{
public:
    DjMixer();
    ~DjMixer();

    void prepare(double sampleRate, int blockSize);
    void reset();

    // Process both decks into master output buffer
    void process(juce::AudioBuffer<float>& deck1Buffer,
                 juce::AudioBuffer<float>& deck2Buffer,
                 juce::AudioBuffer<float>& masterOutBuffer,
                 int numSamples);

    // Channel access
    ChannelStrip& getChannel(int deckIndex) { return deckIndex == 0 ? ch1 : ch2; }

    // Master & Crossfader controls
    void setCrossfader(float value) { crossfader.store(juce::jlimit(-1.0f, 1.0f, value)); }
    float getCrossfader() const     { return crossfader.load(); }

    void setMasterVolume(float vol) { masterVolume.store(juce::jlimit(0.0f, 2.0f, vol)); }
    float getMasterVolume() const   { return masterVolume.load(); }

    // Master Peak Meter
    float getMasterPeakLeft() const  { return masterPeakLeft.load(); }
    float getMasterPeakRight() const { return masterPeakRight.load(); }

    // Master FX controls
    void setFxEnabled(bool enabled)        { fxEnabled.store(enabled); }
    bool isFxEnabled() const               { return fxEnabled.load(); }
    void setFxDelayTime(float seconds)     { fxDelayTime.store(juce::jlimit(0.05f, 1.5f, seconds)); }
    float getFxDelayTime() const           { return fxDelayTime.load(); }
    void setFxFeedback(float fb)           { fxFeedback.store(juce::jlimit(0.0f, 0.9f, fb)); }
    float getFxFeedback() const            { return fxFeedback.load(); }
    void setFxMix(float mix)               { fxMix.store(juce::jlimit(0.0f, 1.0f, mix)); }
    float getFxMix() const                 { return fxMix.load(); }

    void setReverbEnabled(bool enabled)    { reverbEnabled.store(enabled); }
    bool isReverbEnabled() const           { return reverbEnabled.load(); }
    void setReverbMix(float mix)           { reverbMix.store(juce::jlimit(0.0f, 1.0f, mix)); }
    float getReverbMix() const             { return reverbMix.load(); }

private:
    double currentSampleRate { 44100.0 };
    ChannelStrip ch1;
    ChannelStrip ch2;

    std::atomic<float> crossfader { 0.0f };    // -1.0 = Deck 1 full, 0.0 = Center, +1.0 = Deck 2 full
    std::atomic<float> masterVolume { 1.0f };

    std::atomic<float> masterPeakLeft { 0.0f };
    std::atomic<float> masterPeakRight { 0.0f };

    // FX section
    std::atomic<bool> fxEnabled { false };
    std::atomic<float> fxDelayTime { 0.25f }; // default ~120 BPM 1/2 beat
    std::atomic<float> fxFeedback { 0.35f };
    std::atomic<float> fxMix { 0.0f };

    std::atomic<bool> reverbEnabled { false };
    std::atomic<float> reverbMix { 0.0f };

    static constexpr int maxDelaySamples = 96000;
    std::vector<float> delayBufferL;
    std::vector<float> delayBufferR;
    int delayWritePos { 0 };

    juce::Reverb reverb;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DjMixer)
};

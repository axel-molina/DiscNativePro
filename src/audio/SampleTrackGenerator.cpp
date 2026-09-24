#include "SampleTrackGenerator.h"
#include <cmath>
#include <random>

juce::File SampleTrackGenerator::generateTrack(const juce::File& directory,
                                              const juce::String& filename,
                                              const juce::String& title,
                                              const juce::String& artist,
                                              double bpm,
                                              int bars,
                                              const juce::String& style)
{
    directory.createDirectory();
    auto targetFile = directory.getChildFile(filename);
    if (targetFile.existsAsFile() && targetFile.getSize() > 1000)
    {
        return targetFile;
    }

    double sampleRate = 44100.0;
    double secondsPerBeat = 60.0 / bpm;
    int totalBeats = bars * 4;
    int totalSamples = (int)(totalBeats * secondsPerBeat * sampleRate);

    juce::AudioBuffer<float> buffer(2, totalSamples);
    buffer.clear();

    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getWritePointer(1);

    std::mt19937 rng(1337);
    std::uniform_real_distribution<float> noiseDist(-1.0f, 1.0f);

    for (int beat = 0; beat < totalBeats; ++beat)
    {
        double beatStartTime = (double)beat * secondsPerBeat;
        int startSample = (int)(beatStartTime * sampleRate);

        // 1. Kick Drum (4-on-the-floor)
        int kickSamples = (int)(0.25 * sampleRate);
        for (int i = 0; i < kickSamples && startSample + i < totalSamples; ++i)
        {
            double t = (double)i / sampleRate;
            double freq = 130.0 * std::exp(-t * 28.0) + 40.0;
            double env = std::exp(-t * 14.0);
            float kickVal = (float)(std::sin(2.0 * juce::MathConstants<double>::pi * freq * t) * env * 0.75);
            left[startSample + i] += kickVal;
            right[startSample + i] += kickVal;
        }

        // 2. Offbeat Hi-Hat (on the "&")
        int offbeatSample = (int)((beatStartTime + secondsPerBeat * 0.5) * sampleRate);
        int hatSamples = (int)(0.08 * sampleRate);
        for (int i = 0; i < hatSamples && offbeatSample + i < totalSamples; ++i)
        {
            double t = (double)i / sampleRate;
            double env = std::exp(-t * 45.0);
            float noise = noiseDist(rng) * (float)env * 0.35f;
            left[offbeatSample + i] += noise * 0.8f;
            right[offbeatSample + i] += noise * 1.2f; // stereo width
        }

        // 3. Snare / Clap on beats 2 and 4
        if (beat % 2 == 1)
        {
            int snareLen = (int)(0.18 * sampleRate);
            for (int i = 0; i < snareLen && startSample + i < totalSamples; ++i)
            {
                double t = (double)i / sampleRate;
                double tone = std::sin(2.0 * juce::MathConstants<double>::pi * 180.0 * t) * std::exp(-t * 25.0) * 0.3;
                double noise = (double)noiseDist(rng) * std::exp(-t * 18.0) * 0.4;
                float snareVal = (float)(tone + noise);
                left[startSample + i] += snareVal;
                right[startSample + i] += snareVal;
            }
        }

        // 4. Bassline & Melodic Groove
        double latinFreqs[4] = { 110.0, 146.8, 164.8, 130.8 };
        double houseFreqs[4] = { 65.4, 65.4, 77.78, 87.3 };
        double bassFreq = (style == "latin") ? latinFreqs[beat % 4] : houseFreqs[(beat / 4) % 4];

        int bassLen = (int)(secondsPerBeat * 0.8 * sampleRate);
        for (int i = 0; i < bassLen && startSample + i < totalSamples; ++i)
        {
            double t = (double)i / sampleRate;
            double bassEnv = std::exp(-t * 6.0);
            double tone = (std::sin(2.0 * juce::MathConstants<double>::pi * bassFreq * t)
                         + 0.3 * std::sin(4.0 * juce::MathConstants<double>::pi * bassFreq * t)) * bassEnv * 0.3;
            left[startSample + i] += (float)tone;
            right[startSample + i] += (float)tone;
        }

        // 5. Synth Stabs (every 4 beats)
        if (beat % 4 == 0)
        {
            double chordFreqs[3] = { 261.6, 329.6, 392.0 }; // C Major
            int chordLen = (int)(secondsPerBeat * 1.5 * sampleRate);
            for (int i = 0; i < chordLen && startSample + i < totalSamples; ++i)
            {
                double t = (double)i / sampleRate;
                double env = std::exp(-t * 3.5);
                double chordVal = 0.0;
                for (double cf : chordFreqs)
                    chordVal += std::sin(2.0 * juce::MathConstants<double>::pi * cf * t) * 0.12;

                left[startSample + i] += (float)(chordVal * env * 0.9);
                right[startSample + i] += (float)(chordVal * env * 1.1);
            }
        }
    }

    // Soft-limit to prevent clipping
    for (int i = 0; i < totalSamples; ++i)
    {
        left[i] = std::tanh(left[i]);
        right[i] = std::tanh(right[i]);
    }

    // Write WAV file
    juce::WavAudioFormat wavFormat;
    auto outStream = targetFile.createOutputStream();
    if (outStream != nullptr)
    {
        juce::StringPairArray metadata;
        metadata.set("title", title);
        metadata.set("artist", artist);

        std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(
            outStream.release(), sampleRate, 2, 16, metadata, 0));

        if (writer != nullptr)
        {
            writer->writeFromAudioSampleBuffer(buffer, 0, totalSamples);
        }
    }

    return targetFile;
}

void SampleTrackGenerator::ensureDefaultDemoTracks(const juce::File& targetFolder,
                                                  std::vector<juce::File>& createdFiles)
{
    createdFiles.clear();

    auto f1 = generateTrack(targetFolder,
                            "DiscPro_Latin_Tech_Groove.wav",
                            "DiscPro Latin Tech Groove",
                            "Axel Molina & DiscPro Sound",
                            124.0,
                            16, // 16 bars = ~31 sec
                            "latin");
    if (f1.existsAsFile()) createdFiles.push_back(f1);

    auto f2 = generateTrack(targetFolder,
                            "Sunset_Beach_House_Mix.wav",
                            "Sunset Beach House Mix",
                            "Deep Horizon",
                            126.0,
                            16,
                            "house");
    if (f2.existsAsFile()) createdFiles.push_back(f2);
}

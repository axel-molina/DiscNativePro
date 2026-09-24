#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

struct BeatAnalysisResult
{
    double bpm { 124.0 };
    double offset { 0.0 };
    double beatInterval { 0.4838 };
};

class BeatDetector
{
public:
    static BeatAnalysisResult analyze(juce::AudioFormatReader* reader);
};

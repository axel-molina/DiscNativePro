#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>

class SampleTrackGenerator
{
public:
    static juce::File generateTrack(const juce::File& directory,
                                    const juce::String& filename,
                                    const juce::String& title,
                                    const juce::String& artist,
                                    double bpm,
                                    int bars,
                                    const juce::String& style);

    static void ensureDefaultDemoTracks(const juce::File& targetFolder,
                                        std::vector<juce::File>& createdFiles);
};

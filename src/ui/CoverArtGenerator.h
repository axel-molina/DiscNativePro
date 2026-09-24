#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <map>

class CoverArtGenerator
{
public:
    static juce::Image getCoverForTrack(const juce::String& title, int size = 256);
    static void clearCache();

private:
    static juce::Image createOceanTechCover(int size);
    static juce::Image createSunsetHouseCover(int size);
    static juce::Image createCarnivalCover(int size);
    static juce::Image createPiratasCover(int size);
    static juce::Image createCyberfunkCover(int size);
    static juce::Image createDefaultCover(const juce::String& title, int size);
};

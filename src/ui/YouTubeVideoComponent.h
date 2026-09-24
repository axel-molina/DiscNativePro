#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "LucideIcons.h"
#include <memory>

class YouTubeVideoComponent : public juce::Component
{
public:
    YouTubeVideoComponent(int deckIndex);
    ~YouTubeVideoComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void loadVideo(const juce::String& videoId, const juce::String& title = {});
    void play();
    void pause();
    void seekTo(double seconds);
    void setVolume(float volume0to1); // 0.0f to 1.0f

    bool isVideoLoaded() const { return currentVideoId.isNotEmpty(); }
    juce::String getVideoId() const { return currentVideoId; }
    juce::String getVideoTitle() const { return currentVideoTitle; }
    float getVolume() const { return currentVolume; }

private:
    int deckIndex;
    juce::String currentVideoId;
    juce::String currentVideoTitle;
    float currentVolume { 1.0f };

    struct Pimpl;
    std::unique_ptr<Pimpl> pimpl;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(YouTubeVideoComponent)
};

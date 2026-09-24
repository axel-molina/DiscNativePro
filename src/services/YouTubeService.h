#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <vector>
#include <functional>

struct YouTubeSearchResult
{
    juce::String id;
    juce::String title;
    juce::String artist;
    juce::String thumbnailUrl;
    int durationSeconds { 180 };
    juce::String durationText { "3:00" };
};

class YouTubeService
{
public:
    static juce::String extractVideoId(const juce::String& input);
    static juce::String getApiKey();
    static void setApiKey(const juce::String& key);
    static bool hasApiKey();

    // Synchronous search (safe to call on background thread)
    static std::vector<YouTubeSearchResult> search(const juce::String& query);

    // Asynchronous search (triggers onComplete on Message Thread)
    static void searchAsync(const juce::String& query,
                            std::function<void(const std::vector<YouTubeSearchResult>&)> onComplete);

    // Fast details lookup from direct ID / URL
    static YouTubeSearchResult getVideoDetails(const juce::String& videoIdOrUrl);

private:
    static std::vector<YouTubeSearchResult> searchPublic(const juce::String& query);
    static std::vector<YouTubeSearchResult> searchOfficialApi(const juce::String& query, const juce::String& apiKey);

    static juce::File getSettingsFile();
};

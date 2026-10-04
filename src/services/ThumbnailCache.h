#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_events/juce_events.h>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <functional>

class ThumbnailCache
{
public:
    static ThumbnailCache& getInstance();

    juce::Image getThumbnail(const juce::String& videoId,
                             const juce::String& url,
                             std::function<void(const juce::String&)> onReady = nullptr);

    void clearMemoryCache();

private:
    ThumbnailCache();
    ~ThumbnailCache() = default;

    juce::File getCacheFolder() const;
    void downloadAsync(const juce::String& videoId,
                       const juce::String& url,
                       std::function<void(const juce::String&)> onReady);

    std::mutex cacheMutex;
    std::unordered_map<std::string, juce::Image> memoryCache;
    std::unordered_set<std::string> pendingRequests;

    JUCE_DECLARE_NON_COPYABLE(ThumbnailCache)
};

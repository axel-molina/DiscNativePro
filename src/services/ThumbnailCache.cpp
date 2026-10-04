#include "ThumbnailCache.h"

ThumbnailCache& ThumbnailCache::getInstance()
{
    static ThumbnailCache instance;
    return instance;
}

ThumbnailCache::ThumbnailCache()
{
    getCacheFolder().createDirectory();
}

juce::File ThumbnailCache::getCacheFolder() const
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("DiscNativePro/Thumbnails");
}

void ThumbnailCache::clearMemoryCache()
{
    std::scoped_lock lock(cacheMutex);
    memoryCache.clear();
}

juce::Image ThumbnailCache::getThumbnail(const juce::String& videoId,
                                        const juce::String& url,
                                        std::function<void(const juce::String&)> onReady)
{
    auto safeKey = videoId.isNotEmpty() ? videoId : url;
    if (safeKey.isEmpty())
        return {};

    std::string key = safeKey.toStdString();

    {
        std::scoped_lock lock(cacheMutex);
        auto it = memoryCache.find(key);
        if (it != memoryCache.end())
            return it->second;

        if (pendingRequests.find(key) != pendingRequests.end())
            return {};
    }

    // Check disk cache
    auto cacheFile = getCacheFolder().getChildFile(juce::File::createLegalFileName(safeKey) + ".jpg");
    if (cacheFile.existsAsFile() && cacheFile.getSize() > 200)
    {
        auto img = juce::ImageFileFormat::loadFrom(cacheFile);
        if (img.isValid())
        {
            auto scaled = img.rescaled(160, 90, juce::Graphics::mediumResamplingQuality);
            auto finalImg = scaled.isValid() ? scaled : img;

            std::scoped_lock lock(cacheMutex);
            memoryCache[key] = finalImg;
            return finalImg;
        }
    }

    // Need to download
    juce::String downloadUrl = url;
    if (videoId.isNotEmpty() && (downloadUrl.isEmpty() || downloadUrl.contains(".webp") || !downloadUrl.contains(".jpg")))
    {
        downloadUrl = "https://i.ytimg.com/vi/" + videoId + "/mqdefault.jpg";
    }

    if (downloadUrl.isNotEmpty())
    {
        {
            std::scoped_lock lock(cacheMutex);
            pendingRequests.insert(key);
        }
        downloadAsync(videoId, downloadUrl, std::move(onReady));
    }

    return {};
}

void ThumbnailCache::downloadAsync(const juce::String& videoId,
                                   const juce::String& url,
                                   std::function<void(const juce::String&)> onReadyCallback)
{
    auto safeKey = videoId.isNotEmpty() ? videoId : url;
    auto cacheFile = getCacheFolder().getChildFile(juce::File::createLegalFileName(safeKey) + ".jpg");
    auto tmpFile = getCacheFolder().getChildFile(juce::File::createLegalFileName(safeKey) + ".tmp");

    juce::Thread::launch([this, videoId, url, safeKey, cacheFile, tmpFile, cb = std::move(onReadyCallback)]() mutable {
        bool downloadOk = false;

        // Try downloading via curl
        juce::String escapedUrl = url;
        escapedUrl = escapedUrl.replace("'", "'\\''");
        juce::String cmd = "curl -s -L --max-time 6 -o '" + tmpFile.getFullPathName() + "' '" + escapedUrl + "'";

        int ret = system(cmd.toRawUTF8());
        if (ret == 0 && tmpFile.existsAsFile() && tmpFile.getSize() > 200)
        {
            cacheFile.deleteFile();
            tmpFile.moveFileTo(cacheFile);
            downloadOk = true;
        }

        // Fallback to JUCE URL if curl failed
        if (!downloadOk)
        {
            juce::URL juceUrl(url);
            std::unique_ptr<juce::InputStream> stream(juceUrl.createInputStream(
                juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                    .withConnectionTimeoutMs(6000)));

            if (stream != nullptr)
            {
                cacheFile.deleteFile();
                juce::FileOutputStream fos(cacheFile);
                if (fos.openedOk())
                {
                    fos.writeFromInputStream(*stream, -1);
                    fos.flush();
                    downloadOk = (cacheFile.existsAsFile() && cacheFile.getSize() > 200);
                }
            }
        }

        juce::Image finalImg;
        if (cacheFile.existsAsFile())
        {
            auto rawImg = juce::ImageFileFormat::loadFrom(cacheFile);
            if (!rawImg.isValid())
            {
                // Fallback: convert via macOS sips in case server sent WebP
                juce::String sipsCmd = "sips -s format jpeg '" + cacheFile.getFullPathName() + "' --out '" + cacheFile.getFullPathName() + "' 2>/dev/null";
                system(sipsCmd.toRawUTF8());
                rawImg = juce::ImageFileFormat::loadFrom(cacheFile);
            }

            if (rawImg.isValid())
            {
                auto scaled = rawImg.rescaled(160, 90, juce::Graphics::mediumResamplingQuality);
                finalImg = scaled.isValid() ? scaled : rawImg;
            }
        }

        juce::MessageManager::callAsync([this, safeKey, videoId, finalImg, readyCb = std::move(cb)]() {
            std::string key = safeKey.toStdString();
            {
                std::scoped_lock lock(cacheMutex);
                if (finalImg.isValid())
                    memoryCache[key] = finalImg;
                pendingRequests.erase(key);
            }

            if (readyCb)
                readyCb(videoId);
        });
    });
}

#include "YouTubeService.h"
#include "SecretStore.h"
#include <cstring>
#include <string>
#include <thread>
#include <mutex>
#include <map>

juce::File YouTubeService::getSettingsFile()
{
    auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                       .getChildFile("DiscNativePro");
    if (!appData.exists())
        appData.createDirectory();
    return appData.getChildFile("settings.json");
}

juce::String YouTubeService::getApiKey()
{
    const auto secureValue = SecretStore::read("youtube");
    if (secureValue.isNotEmpty())
        return secureValue.trim();

    auto file = getSettingsFile();
    if (file.existsAsFile())
    {
        auto json = juce::JSON::parse(file);
        if (json.isObject() && json.hasProperty("youtubeApiKey"))
        {
            auto legacyValue = json["youtubeApiKey"].toString().trim();
            if (legacyValue.isNotEmpty() && SecretStore::write("youtube", legacyValue))
            {
                if (auto* obj = json.getDynamicObject())
                {
                    obj->removeProperty("youtubeApiKey");
                    file.replaceWithText(juce::JSON::toString(json));
                }
            }
            return legacyValue;
        }
    }
    return {};
}

void YouTubeService::setApiKey(const juce::String& key)
{
    const auto trimmedKey = key.trim();
    auto file = getSettingsFile();
    juce::var json;
    if (file.existsAsFile())
    {
        json = juce::JSON::parse(file);
    }
    if (!json.isObject())
    {
        json = juce::var(new juce::DynamicObject());
    }

    if (auto* obj = json.getDynamicObject())
    {
        if (trimmedKey.isNotEmpty())
        {
            if (SecretStore::write("youtube", trimmedKey))
                obj->removeProperty("youtubeApiKey");
            else
                obj->setProperty("youtubeApiKey", trimmedKey);
        }
        else
        {
            SecretStore::remove("youtube");
            obj->removeProperty("youtubeApiKey");
        }
    }

    file.replaceWithText(juce::JSON::toString(json));
}

void YouTubeService::migrateLegacySecret()
{
    auto file = getSettingsFile();
    if (!file.existsAsFile())
        return;

    auto json = juce::JSON::parse(file);
    if (!json.isObject())
        return;

    auto* obj = json.getDynamicObject();
    if (obj == nullptr)
        return;

    const auto account = juce::String("youtube");
    const auto legacy = json.hasProperty("youtubeApiKey") ? json["youtubeApiKey"].toString().trim() : juce::String();

    bool changed = false;
    if (legacy.isNotEmpty())
    {
        if (SecretStore::read(account).isNotEmpty() || SecretStore::write(account, legacy))
        {
            obj->removeProperty("youtubeApiKey");
            changed = true;
        }
    }
    else if (json.hasProperty("youtubeApiKey"))
    {
        obj->removeProperty("youtubeApiKey");
        changed = true;
    }

    if (changed)
        file.replaceWithText(juce::JSON::toString(json));
}

bool YouTubeService::hasApiKey()
{
    return getApiKey().isNotEmpty();
}

bool YouTubeService::isVideoAutoplayEnabled()
{
    auto file = getSettingsFile();
    if (file.existsAsFile())
    {
        auto json = juce::JSON::parse(file);
        if (json.isObject() && json.hasProperty("youtubeAutoplay"))
            return (bool)json["youtubeAutoplay"];
    }
    return true; // default ON: conserva el comportamiento actual
}

void YouTubeService::setVideoAutoplayEnabled(bool enabled)
{
    auto file = getSettingsFile();
    juce::var json;
    if (file.existsAsFile())
        json = juce::JSON::parse(file);
    if (!json.isObject())
        json = juce::var(new juce::DynamicObject());

    if (auto* obj = json.getDynamicObject())
        obj->setProperty("youtubeAutoplay", enabled);

    file.replaceWithText(juce::JSON::toString(json));
}

bool YouTubeService::isAudioPlaybackMode()
{
    auto file = getSettingsFile();
    if (file.existsAsFile())
    {
        auto json = juce::JSON::parse(file);
        if (json.isObject() && json.hasProperty("youtubePlaybackMode"))
            return json["youtubePlaybackMode"].toString().equalsIgnoreCase("audio");
    }
    return true; // default ON: modo Audio nativo con descargas y control total
}

void YouTubeService::setAudioPlaybackMode(bool audioMode)
{
    auto file = getSettingsFile();
    juce::var json;
    if (file.existsAsFile())
        json = juce::JSON::parse(file);
    if (!json.isObject())
        json = juce::var(new juce::DynamicObject());

    if (auto* obj = json.getDynamicObject())
        obj->setProperty("youtubePlaybackMode", audioMode ? "audio" : "video");

    file.replaceWithText(juce::JSON::toString(json));
}

bool YouTubeService::isSaveDownloadsEnabled()
{
    auto file = getSettingsFile();
    if (file.existsAsFile())
    {
        auto json = juce::JSON::parse(file);
        if (json.isObject() && json.hasProperty("saveDownloadedTracks"))
            return (bool)json["saveDownloadedTracks"];
    }
    return false; // Por defecto desactivado
}

void YouTubeService::setSaveDownloadsEnabled(bool enabled)
{
    auto file = getSettingsFile();
    juce::var json;
    if (file.existsAsFile())
        json = juce::JSON::parse(file);
    if (!json.isObject())
        json = juce::var(new juce::DynamicObject());

    if (auto* obj = json.getDynamicObject())
        obj->setProperty("saveDownloadedTracks", enabled);

    file.replaceWithText(juce::JSON::toString(json));
}

juce::File YouTubeService::getDefaultDownloadsFolder()
{
    return juce::File::getSpecialLocation(juce::File::userMusicDirectory)
               .getChildFile("DiscNativePro")
               .getChildFile("Downloads");
}

juce::File YouTubeService::getDownloadsFolder()
{
    auto file = getSettingsFile();
    if (file.existsAsFile())
    {
        auto json = juce::JSON::parse(file);
        if (json.isObject() && json.hasProperty("downloadsFolder"))
        {
            auto path = json["downloadsFolder"].toString().trim();
            if (path.isNotEmpty())
                return juce::File(path);
        }
    }
    return getDefaultDownloadsFolder();
}

void YouTubeService::setDownloadsFolder(const juce::File& folder)
{
    auto file = getSettingsFile();
    juce::var json;
    if (file.existsAsFile())
        json = juce::JSON::parse(file);
    if (!json.isObject())
        json = juce::var(new juce::DynamicObject());

    if (auto* obj = json.getDynamicObject())
        obj->setProperty("downloadsFolder", folder.getFullPathName());

    file.replaceWithText(juce::JSON::toString(json));
}

juce::String YouTubeService::sanitizeFilename(const juce::String& name)
{
    juce::String safe = name;
    safe = safe.replaceCharacter('/', '-');
    safe = safe.replaceCharacter(':', '-');
    safe = safe.replaceCharacter('\\', '-');
    safe = safe.replaceCharacter('*', ' ');
    safe = safe.replaceCharacter('?', ' ');
    safe = safe.replaceCharacter('"', '\'');
    safe = safe.replaceCharacter('<', ' ');
    safe = safe.replaceCharacter('>', ' ');
    safe = safe.replaceCharacter('|', '-');
    safe = safe.trim();
    if (safe.isEmpty())
        safe = "Track";
    return safe;
}

juce::String YouTubeService::extractVideoId(const juce::String& input)
{
    auto clean = input.trim();
    if (clean.isEmpty()) return {};

    // 1. Direct 11-character ID
    if (clean.length() == 11 && clean.containsOnly("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-"))
        return clean;

    // 2. Standard ?v= or &v=
    int vIdx = clean.indexOf("v=");
    if (vIdx >= 0)
    {
        auto sub = clean.substring(vIdx + 2);
        int amp = sub.indexOfAnyOf("&?/");
        if (amp >= 0) sub = sub.substring(0, amp);
        if (sub.length() >= 11) return sub.substring(0, 11);
    }

    // 3. Shortened youtu.be/
    int beIdx = clean.indexOf("youtu.be/");
    if (beIdx >= 0)
    {
        auto sub = clean.substring(beIdx + 9);
        int q = sub.indexOfAnyOf("?&/");
        if (q >= 0) sub = sub.substring(0, q);
        if (sub.length() >= 11) return sub.substring(0, 11);
    }

    // 4. Shorts / Embed / Live
    const char* prefixes[] = { "/shorts/", "/embed/", "/live/" };
    for (const char* prefix : prefixes)
    {
        int pIdx = clean.indexOf(prefix);
        if (pIdx >= 0)
        {
            auto sub = clean.substring(pIdx + (int)strlen(prefix));
            int q = sub.indexOfAnyOf("?&/");
            if (q >= 0) sub = sub.substring(0, q);
            if (sub.length() >= 11) return sub.substring(0, 11);
        }
    }

    return {};
}

YouTubeSearchResult YouTubeService::getVideoDetails(const juce::String& videoIdOrUrl)
{
    auto vidId = extractVideoId(videoIdOrUrl);
    if (vidId.isEmpty())
        vidId = videoIdOrUrl.trim();

    YouTubeSearchResult result;
    result.id = vidId;
    result.title = "YouTube Video (" + vidId + ")";
    result.artist = "YouTube";
    result.thumbnailUrl = "https://i.ytimg.com/vi/" + vidId + "/hqdefault.jpg";
    result.durationSeconds = 210;
    result.durationText = "3:30";

    // Attempt lightweight oembed lookup
    auto oembedUrl = "https://noembed.com/embed?url=https://www.youtube.com/watch?v=" + vidId;
    juce::URL url(oembedUrl);
    std::unique_ptr<juce::InputStream> stream(url.createInputStream(
        juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs(3000)));

    if (stream != nullptr)
    {
        auto json = juce::JSON::parse(stream->readEntireStreamAsString());
        if (json.isObject())
        {
            auto t = json["title"].toString();
            auto a = json["author_name"].toString();
            if (t.isNotEmpty()) result.title = t;
            if (a.isNotEmpty()) result.artist = a;
        }
    }

    result.thumbnailUrl = "https://i.ytimg.com/vi/" + vidId + "/mqdefault.jpg";
    return result;
}

namespace
{
// Descarga una página de YouTube (watch o results) con la misma técnica que searchPublic
juce::String downloadYouTubePage(const juce::String& pageUrl)
{
    juce::String html;

#if !JUCE_WINDOWS
    // curl sin shell (argumentos separados: sin interpolación de comandos).
    juce::ChildProcess curlProc;
    juce::StringArray curlArgs { "curl", "-s", "-L", "--max-time", "6",
                                 "-H", "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
                                 "-H", "Accept-Language: es,en-US;q=0.9,en;q=0.8",
                                 pageUrl };
    if (curlProc.start(curlArgs))
        html = curlProc.readAllProcessOutput();
#endif

    if (html.isEmpty())
    {
        juce::URL url(pageUrl);
        juce::String extraHeaders =
#if JUCE_WINDOWS
            "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/122.0.0.0 Safari/537.36\r\n"
#else
            "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36\r\n"
#endif
            "Accept-Language: es,en-US;q=0.9,en;q=0.8\r\n";

        std::unique_ptr<juce::InputStream> stream(url.createInputStream(
            juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(8000)
                .withExtraHeaders(extraHeaders)));

        if (stream != nullptr)
            html = stream->readEntireStreamAsString();
    }

    return html;
}

// Extrae el JSON de ytInitialData del HTML (misma técnica que searchPublic)
juce::var parseInitialData(const juce::String& html)
{
    if (html.isEmpty())
        return {};

    const char* raw = html.toRawUTF8();
    const char* pData = strstr(raw, "var ytInitialData = {");
    if (pData == nullptr)
        pData = strstr(raw, "ytInitialData = {");
    if (pData == nullptr)
        return {};

    const char* pStart = strchr(pData, '{');
    if (pStart == nullptr)
        return {};

    const char* pEndTag = strstr(pStart, "</script>");
    if (pEndTag == nullptr)
        return {};

    const char* pEnd = pEndTag;
    while (pEnd > pStart && (*(pEnd - 1) == ';' || *(pEnd - 1) == ' ' || *(pEnd - 1) == '\n' || *(pEnd - 1) == '\r' || *(pEnd - 1) == '\t'))
        pEnd--;

    if (pEnd <= pStart)
        return {};

    return juce::JSON::parse(juce::String::fromUTF8(pStart, static_cast<int>(pEnd - pStart)));
}

juce::String rendererTitle(const juce::var& vr)
{
    auto runs = vr["title"]["runs"];
    if (runs.isArray() && runs.size() > 0)
        return runs[0]["text"].toString();
    return vr["title"]["simpleText"].toString();
}

juce::String rendererChannel(const juce::var& vr)
{
    auto byline = vr["longBylineText"]["runs"];
    if (byline.isArray() && byline.size() > 0)
        return byline[0]["text"].toString();
    auto owner = vr["ownerText"]["runs"];
    if (owner.isArray() && owner.size() > 0)
        return owner[0]["text"].toString();
    auto shortByline = vr["shortBylineText"]["runs"];
    if (shortByline.isArray() && shortByline.size() > 0)
        return shortByline[0]["text"].toString();
    return {};
}

// ponytail: caché de sesión (sin TTL; Up Next es estable durante un set).
// Evita re-scrapear al regenerar sobre la misma pista.
struct SignalsCache
{
    std::mutex mutex;
    std::map<std::string, juce::StringArray> related;
    std::map<std::string, juce::StringArray> tags;
};

SignalsCache& signalsCache()
{
    static SignalsCache cache;
    return cache;
}
} // namespace

juce::StringArray YouTubeService::getRelatedVideos(const juce::String& videoId, int maxResults)
{
    juce::StringArray related;
    auto vid = extractVideoId(videoId);
    if (vid.isEmpty() || maxResults <= 0)
        return related;

    {
        std::lock_guard<std::mutex> lock(signalsCache().mutex);
        auto it = signalsCache().related.find(vid.toStdString());
        if (it != signalsCache().related.end())
        {
            for (int i = 0; i < it->second.size() && related.size() < maxResults; ++i)
                related.add(it->second[i]);
            return related;
        }
    }

    auto data = parseInitialData(downloadYouTubePage("https://www.youtube.com/watch?v=" + vid));
    if (!data.isObject())
        return related;

    auto results = data["contents"]["twoColumnWatchNextResults"]["secondaryResults"]["secondaryResults"]["results"];
    if (!results.isArray())
        return related;

    for (int i = 0; i < results.size() && related.size() < maxResults; ++i)
    {
        auto vr = results[i]["compactVideoRenderer"];
        if (!vr.isObject())
            continue; // playlists, ads, shorts shelves: se ignoran

        auto title = rendererTitle(vr);
        if (title.isEmpty())
            continue;

        auto channel = rendererChannel(vr);
        related.add(channel.isNotEmpty() ? title + juce::String::fromUTF8(" — ") + channel : title);
    }

    {
        std::lock_guard<std::mutex> lock(signalsCache().mutex);
        signalsCache().related[vid.toStdString()] = related;
    }

    return related;
}

juce::StringArray YouTubeService::getVideoTags(const juce::String& videoId)
{
    juce::StringArray tags;
    auto vid = extractVideoId(videoId);
    auto apiKey = getApiKey();
    if (vid.isEmpty() || apiKey.isEmpty())
        return tags;

    {
        std::lock_guard<std::mutex> lock(signalsCache().mutex);
        auto it = signalsCache().tags.find(vid.toStdString());
        if (it != signalsCache().tags.end())
            return it->second;
    }

    juce::URL url("https://www.googleapis.com/youtube/v3/videos?part=snippet&id=" + vid + "&key=" + apiKey);
    std::unique_ptr<juce::InputStream> stream(url.createInputStream(
        juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs(8000)));
    if (stream == nullptr)
        return tags;

    auto json = juce::JSON::parse(stream->readEntireStreamAsString());
    if (!json.isObject())
        return tags;

    auto items = json["items"];
    if (!items.isArray() || items.size() == 0)
        return tags;

    auto tagList = items[0]["snippet"]["tags"];
    if (tagList.isArray())
    {
        for (int i = 0; i < tagList.size() && tags.size() < 10; ++i)
        {
            auto t = tagList[i].toString().trim();
            if (t.isNotEmpty())
                tags.add(t);
        }
    }

    {
        std::lock_guard<std::mutex> lock(signalsCache().mutex);
        signalsCache().tags[vid.toStdString()] = tags;
    }

    return tags;
}

YouTubeService::AudienceSignals YouTubeService::getAudienceSignals(const juce::String& videoId)
{
    AudienceSignals signals;

    // Related (scrape ≤6s) y tags (API ≤8s) son independientes: en paralelo
    std::thread relatedThread([&signals, videoId]() {
        signals.related = getRelatedVideos(videoId, 8);
    });
    std::thread tagsThread([&signals, videoId]() {
        signals.tags = getVideoTags(videoId);
    });
    relatedThread.join();
    tagsThread.join();

    return signals;
}

void YouTubeService::prefetchAudienceSignals(const juce::String& videoId)
{
    auto vid = extractVideoId(videoId);
    if (vid.isEmpty())
        return;

    {
        std::lock_guard<std::mutex> lock(signalsCache().mutex);
        if (signalsCache().related.find(vid.toStdString()) != signalsCache().related.end())
            return; // ya en caché: evita spawn de hilos en cada tick de 0.5s
    }

    juce::Thread::launch([vid]() {
        getAudienceSignals(vid);
    });
}

std::vector<YouTubeSearchResult> YouTubeService::search(const juce::String& query)
{
    auto q = query.trim();
    if (q.isEmpty()) return {};

    // Check if input is a direct video ID or URL
    auto directId = extractVideoId(q);
    if (directId.isNotEmpty())
    {
        return { getVideoDetails(directId) };
    }

    auto apiKey = getApiKey();
    if (apiKey.isNotEmpty())
    {
        auto results = searchOfficialApi(q, apiKey);
        if (!results.empty())
            return results;
    }

    return searchPublic(q);
}

void YouTubeService::searchAsync(const juce::String& query,
                                 std::function<void(const std::vector<YouTubeSearchResult>&)> onComplete)
{
    juce::Thread::launch([query, onComplete]() {
        auto res = search(query);
        juce::MessageManager::callAsync([onComplete, res]() {
            if (onComplete) onComplete(res);
        });
    });
}

std::vector<YouTubeSearchResult> YouTubeService::searchPublic(const juce::String& query)
{
    std::vector<YouTubeSearchResult> results;

    juce::String html;

#if !JUCE_WINDOWS
    // curl sin shell (argumentos separados: sin interpolación de comandos ni quoting manual).
    juce::ChildProcess curlProc;
    juce::StringArray curlArgs { "curl", "-s", "-L", "--max-time", "6",
                                 "-H", "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
                                 "-H", "Accept-Language: es,en-US;q=0.9,en;q=0.8",
                                 "https://www.youtube.com/results?search_query=" + juce::URL::addEscapeChars(query, true) };
    if (curlProc.start(curlArgs))
        html = curlProc.readAllProcessOutput();
#endif

    // On Windows (or fallback on macOS), use native juce::URL in-memory streaming
    // to avoid spawning visible cmd.exe console windows
    if (html.isEmpty())
    {
        juce::String urlStr = "https://www.youtube.com/results?search_query=" + juce::URL::addEscapeChars(query, true);
        juce::URL url(urlStr);
        juce::String extraHeaders =
#if JUCE_WINDOWS
            "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/122.0.0.0 Safari/537.36\r\n"
#else
            "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36\r\n"
#endif
            "Accept-Language: es,en-US;q=0.9,en;q=0.8\r\n";

        std::unique_ptr<juce::InputStream> stream(url.createInputStream(
            juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(8000)
                .withExtraHeaders(extraHeaders)));

        if (stream != nullptr)
            html = stream->readEntireStreamAsString();
    }

    if (html.isEmpty())
        return results;

    const char* raw = html.toRawUTF8();
    const char* pData = strstr(raw, "var ytInitialData = {");
    if (pData == nullptr)
        pData = strstr(raw, "ytInitialData = {");

    if (pData == nullptr)
        return results;

    const char* pStart = strchr(pData, '{');
    if (pStart == nullptr)
        return results;

    const char* pEndTag = strstr(pStart, "</script>");
    if (pEndTag == nullptr)
        return results;

    const char* pEnd = pEndTag;
    while (pEnd > pStart && (*(pEnd - 1) == ';' || *(pEnd - 1) == ' ' || *(pEnd - 1) == '\n' || *(pEnd - 1) == '\r' || *(pEnd - 1) == '\t'))
    {
        pEnd--;
    }

    if (pEnd <= pStart)
        return results;

    auto jsonString = juce::String::fromUTF8(pStart, static_cast<int>(pEnd - pStart));
    auto data = juce::JSON::parse(jsonString);

    if (!data.isObject())
        return results;

    auto sectionList = data["contents"]["twoColumnSearchResultsRenderer"]["primaryContents"]["sectionListRenderer"]["contents"];
    if (!sectionList.isArray())
        return results;

    for (int s = 0; s < sectionList.size() && results.size() < 20; ++s)
    {
        auto itemSection = sectionList[s]["itemSectionRenderer"]["contents"];
        if (!itemSection.isArray())
            continue;

        for (int i = 0; i < itemSection.size() && results.size() < 20; ++i)
        {
            auto item = itemSection[i];
            if (item.hasProperty("videoRenderer"))
            {
                auto vr = item["videoRenderer"];
                auto vidId = vr["videoId"].toString();
                if (vidId.isEmpty()) continue;

                YouTubeSearchResult res;
                res.id = vidId;

                // Title
                auto runs = vr["title"]["runs"];
                if (runs.isArray() && runs.size() > 0)
                    res.title = runs[0]["text"].toString();
                else
                    res.title = vr["title"]["simpleText"].toString();

                if (res.title.isEmpty())
                    res.title = "YouTube Video (" + vidId + ")";

                // Channel / Artist
                auto ownerRuns = vr["ownerText"]["runs"];
                if (ownerRuns.isArray() && ownerRuns.size() > 0)
                    res.artist = ownerRuns[0]["text"].toString();
                else
                {
                    auto bylineRuns = vr["longBylineText"]["runs"];
                    if (bylineRuns.isArray() && bylineRuns.size() > 0)
                        res.artist = bylineRuns[0]["text"].toString();
                    else
                        res.artist = "YouTube Creator";
                }

                // Duration
                res.durationText = vr["lengthText"]["simpleText"].toString();
                if (res.durationText.isNotEmpty())
                {
                    auto parts = juce::StringArray::fromTokens(res.durationText, ":", "");
                    if (parts.size() == 2)
                        res.durationSeconds = parts[0].getIntValue() * 60 + parts[1].getIntValue();
                    else if (parts.size() == 3)
                        res.durationSeconds = parts[0].getIntValue() * 3600 + parts[1].getIntValue() * 60 + parts[2].getIntValue();
                    else
                        res.durationSeconds = 180;
                }
                else
                {
                    res.durationSeconds = 180;
                    res.durationText = "3:00";
                }

                // Fecha de publicación: los resultados de búsqueda solo traen el
                // texto relativo ("hace 2 años"), que es justo lo que se pinta.
                res.publishedText = vr["publishedTimeText"]["simpleText"].toString();
                if (res.publishedText.isEmpty())
                {
                    auto pubRuns = vr["publishedTimeText"]["runs"];
                    if (pubRuns.isArray() && pubRuns.size() > 0)
                        res.publishedText = pubRuns[0]["text"].toString();
                }

                // Thumbnail (always clean 16:9 JPEG)
                res.thumbnailUrl = "https://i.ytimg.com/vi/" + vidId + "/mqdefault.jpg";

                results.push_back(res);
            }
        }
    }

    return results;
}

std::vector<YouTubeSearchResult> YouTubeService::searchOfficialApi(const juce::String& query, const juce::String& apiKey)
{
    std::vector<YouTubeSearchResult> results;

    juce::String urlStr = "https://www.googleapis.com/youtube/v3/search?part=snippet&type=video&maxResults=20&q="
                        + juce::URL::addEscapeChars(query, true)
                        + "&key=" + apiKey;

    juce::URL url(urlStr);
    std::unique_ptr<juce::InputStream> stream(url.createInputStream(
        juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs(8000)));

    if (stream == nullptr)
        return results;

    auto json = juce::JSON::parse(stream->readEntireStreamAsString());
    if (!json.isObject())
        return results;

    auto items = json["items"];
    if (!items.isArray())
        return results;

    for (int i = 0; i < items.size(); ++i)
    {
        auto item = items[i];
        auto vidId = item["id"]["videoId"].toString();
        if (vidId.isEmpty()) continue;

        YouTubeSearchResult res;
        res.id = vidId;
        res.title = item["snippet"]["title"].toString();
        res.artist = item["snippet"]["channelTitle"].toString();
        res.durationSeconds = 210;
        res.durationText = "3:30";
        res.thumbnailUrl = "https://i.ytimg.com/vi/" + vidId + "/mqdefault.jpg";

        // Salvaguarda: esta ruta (solo con API key configurada) sí trae la fecha
        // absoluta. Se pinta en ISO para no dejar la columna vacía.
        auto publishedAt = item["snippet"]["publishedAt"].toString();
        if (publishedAt.isNotEmpty())
            res.publishedText = juce::Time::fromISO8601(publishedAt).formatted("%Y-%m-%d");

        results.push_back(res);
    }

    return results;
}

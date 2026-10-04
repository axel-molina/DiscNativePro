#include "YouTubeService.h"
#include <cstdio>
#include <cstring>
#include <string>

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
    auto file = getSettingsFile();
    if (file.existsAsFile())
    {
        auto json = juce::JSON::parse(file);
        if (json.isObject() && json.hasProperty("youtubeApiKey"))
        {
            return json["youtubeApiKey"].toString().trim();
        }
    }
    return {};
}

void YouTubeService::setApiKey(const juce::String& key)
{
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
        if (key.trim().isNotEmpty())
            obj->setProperty("youtubeApiKey", key.trim());
        else
            obj->removeProperty("youtubeApiKey");
    }

    file.replaceWithText(juce::JSON::toString(json));
}

bool YouTubeService::hasApiKey()
{
    return getApiKey().isNotEmpty();
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

    // Use curl on macOS for ultra-fast, robust response with HTTP/2 and modern TLS
    juce::String escapedQuery = query;
    escapedQuery = escapedQuery.replace("'", "'\\''");
    juce::String cmd = "curl -s -L --max-time 6 "
                       "-H 'User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36' "
                       "-H 'Accept-Language: es,en-US;q=0.9,en;q=0.8' "
                       "'https://www.youtube.com/results?search_query=" + juce::URL::addEscapeChars(escapedQuery, true) + "'";

    FILE* pipe = popen(cmd.toRawUTF8(), "r");
    if (pipe != nullptr)
    {
        char buffer[16384];
        std::string outStr;
        while (size_t bytes = fread(buffer, 1, sizeof(buffer), pipe))
        {
            outStr.append(buffer, bytes);
        }
        pclose(pipe);
        html = juce::String::fromUTF8(outStr.data(), (int)outStr.size());
    }

    if (html.isEmpty())
    {
        juce::String urlStr = "https://www.youtube.com/results?search_query=" + juce::URL::addEscapeChars(query, true);
        juce::URL url(urlStr);
        juce::String extraHeaders =
            "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36\r\n"
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
        results.push_back(res);
    }

    return results;
}

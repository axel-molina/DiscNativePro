#include "LibraryCacheManager.h"

LibraryCacheManager& LibraryCacheManager::getInstance()
{
    static LibraryCacheManager instance;
    return instance;
}

LibraryCacheManager::LibraryCacheManager()
{
    loadCache();
}

LibraryCacheManager::~LibraryCacheManager()
{
    saveCache();
}

juce::File LibraryCacheManager::getCacheFile() const
{
    auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("DiscNativePro");
    if (!appData.isDirectory())
        appData.createDirectory();
    return appData.getChildFile("library_cache.json");
}

size_t LibraryCacheManager::getCachedCount()
{
    std::lock_guard<std::mutex> lock(cacheMutex);
    return cache.size();
}

std::vector<juce::File> LibraryCacheManager::getCachedTracks()
{
    std::lock_guard<std::mutex> lock(cacheMutex);
    std::vector<juce::File> out;
    out.reserve(cache.size());
    for (const auto& kv : cache)
        out.emplace_back(kv.first);
    return out;
}

void LibraryCacheManager::clear()
{
    std::lock_guard<std::mutex> lock(cacheMutex);
    cache.clear();
    isDirty = true;
    saveCache();
}

bool LibraryCacheManager::getTrack(const juce::File& file, CachedTrackMetadata& outMeta)
{
    auto path = file.getFullPathName();
    auto modTime = file.getLastModificationTime().toMilliseconds();
    auto fileSize = file.getSize();

    std::lock_guard<std::mutex> lock(cacheMutex);
    auto it = cache.find(path);
    if (it != cache.end())
    {
        // Check if file has changed on disk
        if (it->second.lastModifiedTime == modTime && it->second.fileSizeBytes == fileSize)
        {
            outMeta = it->second;
            return true;
        }
    }
    return false;
}

void LibraryCacheManager::storeTrack(const juce::File& file, const CachedTrackMetadata& meta, bool autoSave)
{
    auto path = file.getFullPathName();

    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        cache[path] = meta;
        isDirty = true;
    }

    if (autoSave)
    {
        auto now = juce::Time::getMillisecondCounter();
        if (now - lastSaveTick > 5000)
        {
            saveCache();
            lastSaveTick = now;
        }
    }
}

void LibraryCacheManager::updateBpmAndKey(const juce::File& file, double bpm, const juce::String& key)
{
    auto path = file.getFullPathName();
    bool updated = false;

    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        auto it = cache.find(path);
        if (it != cache.end())
        {
            if (bpm > 0.0) it->second.bpm = bpm;
            if (key.isNotEmpty() && key != "--") it->second.key = key;
            isDirty = true;
            updated = true;
        }
    }

    if (updated)
    {
        auto now = juce::Time::getMillisecondCounter();
        if (now - lastSaveTick > 3000)
        {
            saveCache();
            lastSaveTick = now;
        }
    }
}

void LibraryCacheManager::updateGrid(const juce::File& file, double gridBpm, double gridFirstBeatOffset)
{
    auto path = file.getFullPathName();
    bool updated = false;

    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        auto it = cache.find(path);
        if (it == cache.end())
        {
            // Pista nunca escaneada por la biblioteca: entrada mínima con la
            // misma firma de validez (modTime/size) que usa getTrack().
            CachedTrackMetadata meta;
            meta.title = file.getFileNameWithoutExtension();
            meta.duration = 0.0;
            meta.lastModifiedTime = file.getLastModificationTime().toMilliseconds();
            meta.fileSizeBytes = file.getSize();
            it = cache.emplace(path, std::move(meta)).first;
        }
        it->second.gridBpm = gridBpm;
        it->second.gridFirstBeatOffset = gridFirstBeatOffset;
        isDirty = true;
        updated = true;
    }

    if (updated)
    {
        auto now = juce::Time::getMillisecondCounter();
        if (now - lastSaveTick > 3000)
        {
            saveCache();
            lastSaveTick = now;
        }
    }
}

void LibraryCacheManager::updateHotCues(const juce::File& file, const std::array<double, 4>& hotCues)
{
    auto path = file.getFullPathName();
    bool updated = false;

    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        auto it = cache.find(path);
        if (it == cache.end())
        {
            // Entrada mínima con la misma firma de validez que usa getTrack().
            CachedTrackMetadata meta;
            meta.title = file.getFileNameWithoutExtension();
            meta.lastModifiedTime = file.getLastModificationTime().toMilliseconds();
            meta.fileSizeBytes = file.getSize();
            it = cache.emplace(path, std::move(meta)).first;
        }
        it->second.hotCues = hotCues;
        isDirty = true;
        updated = true;
    }

    if (updated)
    {
        auto now = juce::Time::getMillisecondCounter();
        if (now - lastSaveTick > 3000)
        {
            saveCache();
            lastSaveTick = now;
        }
    }
}

void LibraryCacheManager::updateStemsStatus(const juce::File& file, const juce::String& status, const juce::String& dir)
{
    auto path = file.getFullPathName();
    bool updated = false;

    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        auto it = cache.find(path);
        if (it == cache.end())
        {
            // Entrada mínima con la misma firma de validez que usa getTrack().
            CachedTrackMetadata meta;
            meta.title = file.getFileNameWithoutExtension();
            meta.lastModifiedTime = file.getLastModificationTime().toMilliseconds();
            meta.fileSizeBytes = file.getSize();
            it = cache.emplace(path, std::move(meta)).first;
        }
        it->second.stemsStatus = status;
        it->second.stemsDir = dir;
        isDirty = true;
        updated = true;
    }

    if (updated)
    {
        auto now = juce::Time::getMillisecondCounter();
        if (now - lastSaveTick > 3000)
        {
            saveCache();
            lastSaveTick = now;
        }
    }
}

juce::String LibraryCacheManager::getStemsStatus(const juce::File& file)
{
    std::lock_guard<std::mutex> lock(cacheMutex);
    auto it = cache.find(file.getFullPathName());
    if (it == cache.end())
        return juce::String();
    return it->second.stemsStatus;
}

bool LibraryCacheManager::hasStemsJobInProgress()
{
    std::lock_guard<std::mutex> lock(cacheMutex);
    for (const auto& [path, meta] : cache)
        if (meta.stemsStatus == "processing")
            return true;
    return false;
}

void LibraryCacheManager::loadCache()
{
    auto file = getCacheFile();
    if (!file.existsAsFile())
        return;

    auto jsonContent = file.loadFileAsString();
    if (jsonContent.isEmpty())
        return;

    auto parsed = juce::JSON::parse(jsonContent);
    if (!parsed.isObject())
        return;

    auto* obj = parsed.getDynamicObject();
    if (obj == nullptr)
        return;

    std::lock_guard<std::mutex> lock(cacheMutex);
    cache.clear();

    for (const auto& prop : obj->getProperties())
    {
        auto path = prop.name.toString();
        const auto& val = prop.value;
        if (val.isObject())
        {
            auto* itemObj = val.getDynamicObject();
            if (itemObj != nullptr)
            {
                CachedTrackMetadata meta;
                meta.title = itemObj->getProperty("title").toString();
                meta.artist = itemObj->getProperty("artist").toString();
                meta.album = itemObj->getProperty("album").toString();
                meta.duration = static_cast<double>(itemObj->getProperty("duration"));
                meta.bpm = static_cast<double>(itemObj->getProperty("bpm"));
                meta.key = itemObj->getProperty("key").toString();
                meta.lastModifiedTime = static_cast<juce::int64>(itemObj->getProperty("modTime"));
                meta.fileSizeBytes = static_cast<juce::int64>(itemObj->getProperty("size"));
                meta.gridBpm = static_cast<double>(itemObj->getProperty("gridBpm"));
                meta.gridFirstBeatOffset = static_cast<double>(itemObj->getProperty("gridOffset"));
                if (auto* hcArr = itemObj->getProperty("hotCues").getArray())
                {
                    for (int i = 0; i < 4 && i < (int)hcArr->size(); ++i)
                        meta.hotCues[static_cast<size_t>(i)] = static_cast<double>((*hcArr)[i]);
                }
                meta.stemsStatus = itemObj->getProperty("stemsStatus").toString();
                meta.stemsDir = itemObj->getProperty("stemsDir").toString();

                cache[path] = meta;
            }
        }
    }
    isDirty = false;
}

void LibraryCacheManager::saveCache()
{
    std::unordered_map<juce::String, CachedTrackMetadata> snapshot;
    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        if (!isDirty && getCacheFile().existsAsFile())
            return;
        snapshot = cache;
        isDirty = false;
        lastSaveTick = juce::Time::getMillisecondCounter();
    }

    auto rootObj = std::make_unique<juce::DynamicObject>();
    for (const auto& [path, meta] : snapshot)
    {
        auto itemObj = std::make_unique<juce::DynamicObject>();
        itemObj->setProperty("title", meta.title);
        itemObj->setProperty("artist", meta.artist);
        itemObj->setProperty("album", meta.album);
        itemObj->setProperty("duration", meta.duration);
        itemObj->setProperty("bpm", meta.bpm);
        itemObj->setProperty("key", meta.key);
        itemObj->setProperty("modTime", meta.lastModifiedTime);
        itemObj->setProperty("size", meta.fileSizeBytes);
        itemObj->setProperty("gridBpm", meta.gridBpm);
        itemObj->setProperty("gridOffset", meta.gridFirstBeatOffset);
        juce::Array<juce::var> hcArr;
        for (const auto hc : meta.hotCues)
            hcArr.add(hc);
        itemObj->setProperty("hotCues", hcArr);
        itemObj->setProperty("stemsStatus", meta.stemsStatus);
        itemObj->setProperty("stemsDir", meta.stemsDir);

        rootObj->setProperty(path, juce::var(itemObj.release()));
    }

    auto jsonString = juce::JSON::toString(juce::var(rootObj.release()), true);
    auto file = getCacheFile();
    juce::TemporaryFile tempFile(file);
    if (tempFile.getFile().replaceWithText(jsonString))
    {
        tempFile.overwriteTargetFileWithTemporary();
    }
}

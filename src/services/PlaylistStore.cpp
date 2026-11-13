#include "PlaylistStore.h"

PlaylistStore& PlaylistStore::getInstance()
{
    static PlaylistStore instance;
    return instance;
}

PlaylistStore::PlaylistStore()
{
    load();
}

PlaylistStore::~PlaylistStore()
{
    std::lock_guard<std::mutex> lock(mutex);
    saveUnlocked();
}

juce::File PlaylistStore::getStoreFile() const
{
    auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("DiscNativePro");
    if (!appData.isDirectory())
        appData.createDirectory();
    return appData.getChildFile("playlists.json");
}

void PlaylistStore::load()
{
    auto file = getStoreFile();
    if (!file.existsAsFile())
        return;

    auto jsonContent = file.loadFileAsString();
    if (jsonContent.isEmpty())
        return;

    auto parsed = juce::JSON::parse(jsonContent);
    if (!parsed.isArray())
        return;

    std::lock_guard<std::mutex> lock(mutex);
    playlists.clear();

    for (const auto& val : *parsed.getArray())
    {
        if (!val.isObject())
            continue;
        auto* obj = val.getDynamicObject();
        if (obj == nullptr)
            continue;

        Playlist pl;
        pl.id = obj->getProperty("id").toString();
        pl.name = obj->getProperty("name").toString();
        if (pl.id.isEmpty() || pl.name.isEmpty())
            continue;

        if (auto* arr = obj->getProperty("paths").getArray())
        {
            for (const auto& p : *arr)
                if (p.toString().isNotEmpty())
                    pl.paths.push_back(p.toString());
        }
        playlists.push_back(std::move(pl));
    }
    dirty = false;
}

void PlaylistStore::saveUnlocked()
{
    if (!dirty && getStoreFile().existsAsFile())
        return;
    auto snapshot = playlists;
    dirty = false;

    juce::Array<juce::var> rootArr;
    for (const auto& pl : snapshot)
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty("id", pl.id);
        obj->setProperty("name", pl.name);

        juce::Array<juce::var> pathsArr;
        for (const auto& p : pl.paths)
            pathsArr.add(juce::var(p));
        obj->setProperty("paths", pathsArr);

        rootArr.add(juce::var(obj.release()));
    }

    auto jsonString = juce::JSON::toString(juce::var(rootArr), true);
    auto file = getStoreFile();
    juce::TemporaryFile tempFile(file);
    if (tempFile.getFile().replaceWithText(jsonString))
        tempFile.overwriteTargetFileWithTemporary();
}

int PlaylistStore::getPlaylistCount() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return (int) playlists.size();
}

std::vector<Playlist> PlaylistStore::getPlaylists() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return playlists;
}

juce::String PlaylistStore::nameOf(const juce::String& id) const
{
    for (const auto& pl : playlists)
        if (pl.id == id)
            return pl.name;
    return {};
}

bool PlaylistStore::createPlaylist(const juce::String& name)
{
    auto trimmed = name.trim();
    if (trimmed.isEmpty())
        return false;

    std::lock_guard<std::mutex> lock(mutex);
    for (const auto& pl : playlists)
        if (pl.name.equalsIgnoreCase(trimmed))
            return false;

    Playlist pl;
    pl.id = juce::Uuid().toString();
    pl.name = trimmed;
    playlists.push_back(std::move(pl));
    dirty = true;
    saveUnlocked();
    return true;
}

bool PlaylistStore::renamePlaylist(const juce::String& id, const juce::String& newName)
{
    auto trimmed = newName.trim();
    if (trimmed.isEmpty())
        return false;

    std::lock_guard<std::mutex> lock(mutex);
    for (const auto& pl : playlists)
        if (pl.id != id && pl.name.equalsIgnoreCase(trimmed))
            return false;

    for (auto& pl : playlists)
    {
        if (pl.id == id)
        {
            pl.name = trimmed;
            dirty = true;
            saveUnlocked();
            return true;
        }
    }
    return false;
}

bool PlaylistStore::deletePlaylist(const juce::String& id)
{
    std::lock_guard<std::mutex> lock(mutex);
    for (auto it = playlists.begin(); it != playlists.end(); ++it)
    {
        if (it->id == id)
        {
            playlists.erase(it);
            dirty = true;
            saveUnlocked();
            return true;
        }
    }
    return false;
}

bool PlaylistStore::addTrack(const juce::String& id, const juce::String& canonicalPath)
{
    if (canonicalPath.isEmpty())
        return false;

    std::lock_guard<std::mutex> lock(mutex);
    for (auto& pl : playlists)
    {
        if (pl.id != id)
            continue;
        for (const auto& p : pl.paths)
            if (p == canonicalPath)
                return false;
        pl.paths.push_back(canonicalPath);
        dirty = true;
        saveUnlocked();
        return true;
    }
    return false;
}

bool PlaylistStore::removeTrack(const juce::String& id, const juce::String& canonicalPath)
{
    std::lock_guard<std::mutex> lock(mutex);
    for (auto& pl : playlists)
    {
        if (pl.id != id)
            continue;
        for (auto it = pl.paths.begin(); it != pl.paths.end(); ++it)
        {
            if (*it == canonicalPath)
            {
                pl.paths.erase(it);
                dirty = true;
                saveUnlocked();
                return true;
            }
        }
        return false;
    }
    return false;
}

bool PlaylistStore::reorderTrack(const juce::String& id, const juce::String& path, const juce::String& beforePath)
{
    if (path.isEmpty() || path == beforePath)
        return false;

    std::lock_guard<std::mutex> lock(mutex);
    for (auto& pl : playlists)
    {
        if (pl.id != id)
            continue;

        int pathIdx = -1;
        for (int i = 0; i < (int) pl.paths.size(); ++i)
        {
            if (pl.paths[(size_t) i] == path)
            {
                pathIdx = i;
                break;
            }
        }
        if (pathIdx < 0)
            return false;

        auto moved = pl.paths[(size_t) pathIdx];
        pl.paths.erase(pl.paths.begin() + pathIdx);

        if (beforePath.isEmpty())
        {
            pl.paths.push_back(moved);
        }
        else
        {
            int beforeIdx = -1;
            for (int i = 0; i < (int) pl.paths.size(); ++i)
            {
                if (pl.paths[(size_t) i] == beforePath)
                {
                    beforeIdx = i;
                    break;
                }
            }
            if (beforeIdx < 0)
                pl.paths.push_back(moved);
            else
                pl.paths.insert(pl.paths.begin() + beforeIdx, moved);
        }
        dirty = true;
        saveUnlocked();
        return true;
    }
    return false;
}

juce::StringArray PlaylistStore::playlistPaths(const juce::String& id) const
{
    juce::StringArray out;
    std::lock_guard<std::mutex> lock(mutex);
    for (const auto& pl : playlists)
    {
        if (pl.id != id)
            continue;
        for (const auto& p : pl.paths)
            out.add(p);
        break;
    }
    return out;
}

bool PlaylistStore::playlistContains(const juce::String& id, const juce::String& canonicalPath) const
{
    std::lock_guard<std::mutex> lock(mutex);
    for (const auto& pl : playlists)
    {
        if (pl.id != id)
            continue;
        for (const auto& p : pl.paths)
            if (p == canonicalPath)
                return true;
        break;
    }
    return false;
}

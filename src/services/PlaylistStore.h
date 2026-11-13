#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <mutex>

// P17: playlists persistentes. JSON simple en el directorio de datos de la
// app; las pistas se guardan por ruta canónica y en orden manual.
struct Playlist
{
    juce::String id;
    juce::String name;
    std::vector<juce::String> paths;
};

class PlaylistStore
{
public:
    static PlaylistStore& getInstance();

    std::vector<Playlist> getPlaylists() const;
    int getPlaylistCount() const;

    bool createPlaylist(const juce::String& name);
    bool renamePlaylist(const juce::String& id, const juce::String& newName);
    bool deletePlaylist(const juce::String& id);
    bool addTrack(const juce::String& id, const juce::String& canonicalPath);
    bool removeTrack(const juce::String& id, const juce::String& canonicalPath);
    // Mueve `path` a la posición previa a `beforePath` (vacío = al final).
    bool reorderTrack(const juce::String& id, const juce::String& path, const juce::String& beforePath);
    juce::StringArray playlistPaths(const juce::String& id) const;
    bool playlistContains(const juce::String& id, const juce::String& canonicalPath) const;

private:
    PlaylistStore();
    ~PlaylistStore();

    juce::File getStoreFile() const;
    void load();
    // Requiere el mutex ya tomado (se llama dentro de las mutaciones).
    void saveUnlocked();

    juce::String nameOf(const juce::String& id) const;

    mutable std::mutex mutex;
    std::vector<Playlist> playlists;
    bool dirty { false };

    JUCE_DECLARE_NON_COPYABLE(PlaylistStore)
};

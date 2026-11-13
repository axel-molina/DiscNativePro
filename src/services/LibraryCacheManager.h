#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <unordered_map>
#include <mutex>
#include <vector>

struct CachedTrackMetadata
{
    juce::String title;
    juce::String artist;
    juce::String album;
    double duration { 0.0 };
    double bpm { 0.0 };
    juce::String key;
    // P16: beat grid corregido a mano. gridBpm <= 0 = sin corrección.
    double gridBpm { 0.0 };
    double gridFirstBeatOffset { 0.0 };
    // P27: hotcues persistentes por pista (segundos; 0 = vacío).
    std::array<double, 4> hotCues { 0.0, 0.0, 0.0, 0.0 };
    juce::int64 lastModifiedTime { 0 };
    juce::int64 fileSizeBytes { 0 };
    // P22: separación de stems. "none" / "processing" / "ready".
    juce::String stemsStatus { "none" };
    juce::String stemsDir;
};

class LibraryCacheManager
{
public:
    static LibraryCacheManager& getInstance();

    bool getTrack(const juce::File& file, CachedTrackMetadata& outMeta);
    void storeTrack(const juce::File& file, const CachedTrackMetadata& meta, bool autoSave = false);
    void updateBpmAndKey(const juce::File& file, double bpm, const juce::String& key);
    void updateGrid(const juce::File& file, double gridBpm, double gridFirstBeatOffset);
    void updateHotCues(const juce::File& file, const std::array<double, 4>& hotCues);
    void updateStemsStatus(const juce::File& file, const juce::String& status, const juce::String& dir);
    juce::String getStemsStatus(const juce::File& file); // "none"/"processing"/"ready" ("" = sin entrada)
    bool hasStemsJobInProgress(); // algún track con separación en curso

    void loadCache();
    void saveCache();

    size_t getCachedCount();
    // P26: todas las pistas indexadas (para el precálculo en lote de stems).
    std::vector<juce::File> getCachedTracks();
    void clear();

private:
    LibraryCacheManager();
    ~LibraryCacheManager();

    juce::File getCacheFile() const;

    std::mutex cacheMutex;
    std::unordered_map<juce::String, CachedTrackMetadata> cache; // Key: Canonical Full Path
    bool isDirty { false };
    juce::uint32 lastSaveTick { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LibraryCacheManager)
};

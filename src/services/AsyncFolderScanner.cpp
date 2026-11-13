#include "AsyncFolderScanner.h"
#include "../ui/DeckComponent.h"
#include "../audio/KeyDetector.h"

AsyncFolderScanner::AsyncFolderScanner(juce::AudioFormatManager& fm)
    : juce::Thread("AsyncFolderScanner"), formatManager(fm)
{
}

AsyncFolderScanner::~AsyncFolderScanner()
{
    cancelScan();
}

bool AsyncFolderScanner::isValidAudioFile(const juce::File& file)
{
    if (file.isDirectory() || file.isHidden())
        return false;

    auto fileName = file.getFileName();
    if (fileName.startsWith("._") || fileName.startsWith("."))
        return false;

    auto ext = file.getFileExtension().toLowerCase();
    return (ext == ".mp3"  || ext == ".wav"  || ext == ".aif"  ||
            ext == ".aiff" || ext == ".flac" || ext == ".m4a"  ||
            ext == ".aac"  || ext == ".ogg");
}

void AsyncFolderScanner::startScan(const juce::File& rootFolder, const juce::String& baseFolderTag)
{
    cancelScan();

    rootToScan = rootFolder;
    rootBaseTag = baseFolderTag.isNotEmpty() ? baseFolderTag : rootFolder.getFileName();
    cancelRequested = false;
    tracksFoundCount = 0;
    batchBuffer.clear();
    lastBatchDispatchTick = juce::Time::getMillisecondCounter();

    ++currentScanId;

    startThread(juce::Thread::Priority::normal);
}

void AsyncFolderScanner::cancelScan()
{
    cancelRequested = true;
    ++currentScanId;
    signalThreadShouldExit();
    stopThread(2000);
    batchBuffer.clear();
}

void AsyncFolderScanner::run()
{
    uint64_t scanId = currentScanId.load();

    if (!rootToScan.isDirectory() || threadShouldExit() || cancelRequested.load())
        return;

    scanDirectoryRecursive(rootToScan, rootToScan, rootBaseTag, scanId);

    if (!threadShouldExit() && !cancelRequested.load() && scanId == currentScanId.load())
    {
        flushBatch(scanId, true);
        LibraryCacheManager::getInstance().saveCache();
    }
}

void AsyncFolderScanner::scanDirectoryRecursive(const juce::File& dir, const juce::File& rootDir, const juce::String& baseFolderTag, uint64_t scanId)
{
    if (threadShouldExit() || cancelRequested.load() || scanId != currentScanId.load())
        return;

    std::vector<juce::File> subDirs;

    for (const auto& entry : juce::RangedDirectoryIterator(dir, false, "*", juce::File::findFilesAndDirectories))
    {
        if (threadShouldExit() || cancelRequested.load() || scanId != currentScanId.load())
            return;

        auto f = entry.getFile();
        if (f.isDirectory())
        {
            auto dirName = f.getFileName();
            if (dirName.startsWith(".") ||
                dirName.equalsIgnoreCase("System Volume Information") ||
                dirName.equalsIgnoreCase("$RECYCLE.BIN") ||
                dirName.equalsIgnoreCase(".Trashes") ||
                dirName.equalsIgnoreCase(".Spotlight-V100") ||
                dirName.equalsIgnoreCase(".fseventsd"))
            {
                continue;
            }
            subDirs.push_back(f);
        }
        else if (isValidAudioFile(f))
        {
            TrackItem item;
            if (processAudioFile(f, rootDir, baseFolderTag, item))
            {
                batchBuffer.push_back(std::move(item));
                tracksFoundCount++;

                auto now = juce::Time::getMillisecondCounter();
                if (batchBuffer.size() >= 60 || (now - lastBatchDispatchTick >= 120))
                {
                    flushBatch(scanId, false);
                    lastBatchDispatchTick = now;

                    if (onProgress)
                    {
                        int currentCount = tracksFoundCount.load();
                        auto folderName = dir.getFileName();
                        juce::MessageManager::callAsync([this, currentCount, folderName, scanId]() {
                            if (scanId == currentScanId.load() && onProgress)
                                onProgress(currentCount, folderName, scanId);
                        });
                    }
                }
            }
        }
    }

    for (const auto& subDir : subDirs)
    {
        if (threadShouldExit() || cancelRequested.load() || scanId != currentScanId.load())
            return;
        scanDirectoryRecursive(subDir, rootDir, baseFolderTag, scanId);
    }
}

bool AsyncFolderScanner::processAudioFile(const juce::File& file, const juce::File& rootDir, const juce::String& baseFolderTag, TrackItem& outItem)
{
    outItem.file = file;
    outItem.id = juce::String::toHexString(file.hashCode64());
    outItem.rootFolderPath = rootDir.getFullPathName();
    outItem.isDemo = false;

    // Folder tag formatting relative to root
    auto parentDir = file.getParentDirectory();
    if (parentDir == rootDir)
    {
        outItem.folder = baseFolderTag;
    }
    else
    {
        auto relDirPath = parentDir.getRelativePathFrom(rootDir);
        juce::String cleanRel = relDirPath.replaceCharacter('\\', '/');
        juce::StringArray subParts;
        subParts.addTokens(cleanRel, "/", "\"");
        subParts.removeEmptyStrings();

        if (!subParts.isEmpty())
            outItem.folder = baseFolderTag + " / " + subParts.joinIntoString(" / ");
        else
            outItem.folder = baseFolderTag;
    }

    // 1. Check persistent cache first (instant hit)
    CachedTrackMetadata cachedMeta;
    if (LibraryCacheManager::getInstance().getTrack(file, cachedMeta))
    {
        outItem.title = cachedMeta.title;
        outItem.artist = cachedMeta.artist;
        outItem.album = cachedMeta.album;
        outItem.duration = cachedMeta.duration;
        outItem.bpm = cachedMeta.bpm;
        outItem.key = cachedMeta.key;
        return true;
    }

    // 2. Cache miss: extract basic metadata via format reader
    outItem.title = file.getFileNameWithoutExtension().replaceCharacter('_', ' ');
    outItem.artist = "Artista Local";
    outItem.album = parentDir.getFileName();
    outItem.duration = 0.0;
    outItem.bpm = 0.0;
    outItem.key = "--";

    juce::String splitArtist, splitTitle;
    if (DeckComponent::splitArtistTitle(outItem.title, splitArtist, splitTitle))
    {
        outItem.artist = splitArtist;
        outItem.title = splitTitle;
    }
    else
    {
        outItem.title = DeckComponent::stripTrackNumberPrefix(outItem.title);
    }

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader != nullptr)
    {
        if (reader->sampleRate > 0)
            outItem.duration = (double)reader->lengthInSamples / reader->sampleRate;

        auto metaTitle = reader->metadataValues.getValue("title", "").trim();
        if (metaTitle.isNotEmpty())
            outItem.title = DeckComponent::sanitizeTrackString(metaTitle);

        auto metaArtist = reader->metadataValues.getValue("artist", "").trim();
        if (metaArtist.isNotEmpty())
            outItem.artist = DeckComponent::sanitizeTrackString(metaArtist);

        auto metaAlbum = reader->metadataValues.getValue("album", "").trim();
        if (metaAlbum.isNotEmpty())
            outItem.album = DeckComponent::sanitizeTrackString(metaAlbum);

        if (outItem.artist.isEmpty() || outItem.artist == "Artista Local" || outItem.artist == "Local Artist")
        {
            if (DeckComponent::splitArtistTitle(outItem.title, splitArtist, splitTitle))
            {
                outItem.artist = splitArtist;
                outItem.title = splitTitle;
            }
        }

        // Try reading BPM from metadata
        auto bpmStr = reader->metadataValues.getValue("bpm", "");
        if (bpmStr.isEmpty()) bpmStr = reader->metadataValues.getValue("TBPM", "");
        if (bpmStr.isEmpty()) bpmStr = reader->metadataValues.getValue("tempo", "");
        if (bpmStr.isNotEmpty())
        {
            double bVal = bpmStr.getDoubleValue();
            if (bVal >= 40.0 && bVal <= 250.0)
                outItem.bpm = bVal;
        }

        // Try reading Key from metadata
        auto keyStr = reader->metadataValues.getValue("initialkey", "");
        if (keyStr.isEmpty()) keyStr = reader->metadataValues.getValue("TKEY", "");
        if (keyStr.isEmpty()) keyStr = reader->metadataValues.getValue("key", "");
        if (keyStr.isNotEmpty())
        {
            auto formatted = KeyDetector::formatKeyString(keyStr);
            if (formatted.isNotEmpty() && formatted != "--")
                outItem.key = formatted;
        }
    }

    // Store in cache for subsequent lightning-fast loads
    CachedTrackMetadata toCache;
    toCache.title = outItem.title;
    toCache.artist = outItem.artist;
    toCache.album = outItem.album;
    toCache.duration = outItem.duration;
    toCache.bpm = outItem.bpm;
    toCache.key = outItem.key;
    toCache.lastModifiedTime = file.getLastModificationTime().toMilliseconds();
    toCache.fileSizeBytes = file.getSize();

    LibraryCacheManager::getInstance().storeTrack(file, toCache, false);

    return true;
}

void AsyncFolderScanner::flushBatch(uint64_t scanId, bool isFinal)
{
    if (batchBuffer.empty() && !isFinal)
        return;

    auto batchCopy = std::move(batchBuffer);
    batchBuffer.clear();

    int totalFound = tracksFoundCount.load();

    juce::MessageManager::callAsync([this, batch = std::move(batchCopy), totalFound, scanId, isFinal]() {
        if (scanId != currentScanId.load())
            return;

        if (!batch.empty() && onTracksDiscovered)
            onTracksDiscovered(batch, scanId);

        if (isFinal && onScanFinished)
            onScanFinished(totalFound, scanId);
    });
}

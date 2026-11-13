#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "DjButton.h"
#include "LucideIcons.h"
#include "FolderTreeComponent.h"
#include "AIMatchView.h"
#include "../services/LanguageManager.h"
#include <vector>
#include <functional>

struct TrackItem
{
    juce::String id;
    juce::String title;
    juce::String artist;
    juce::String album { "Club Sessions Vol. 1" };
    juce::String key { "8A" };
    double bpm { 124.0 };
    double duration { 0.0 };
    juce::File file;
    juce::String folder { "Todas las Pistas" };
    juce::String rootFolderPath;
    bool isDemo { false };
};

class LibraryComponent : public juce::Component,
                         public juce::TableListBoxModel,
                         public juce::DragAndDropTarget,
                         public juce::Timer,
                         public LanguageManager::Listener
{
public:
    LibraryComponent(juce::AudioFormatManager& formatManager);
    ~LibraryComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;
    void languageChanged() override;
    void refreshLanguage();

    // TableListBoxModel overrides
    int getNumRows() override;
    void paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected) override;
    void paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override;
    juce::Component* refreshComponentForCell(int rowNumber, int columnId, bool isRowSelected, juce::Component* existingComponentToUpdate) override;
    void cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent&) override;
    void cellClicked(int rowNumber, int columnId, const juce::MouseEvent& e) override;
    juce::var getDragSourceDescription(const juce::SparseSet<int>& currentlySelectedRows) override;
    void sortOrderChanged(int newSortColumnId, bool isForwards) override;

    // DragAndDropTarget overrides (Automix panel drop zone)
    bool isInterestedInDragSource(const SourceDetails& dragSourceDetails) override;
    void itemDragEnter(const SourceDetails& dragSourceDetails) override;
    void itemDragMove(const SourceDetails& dragSourceDetails) override;
    void itemDragExit(const SourceDetails& dragSourceDetails) override;
    void itemDropped(const SourceDetails& dragSourceDetails) override;

    // Keyboard handling (Cmd+A select all)
    bool keyPressed(const juce::KeyPress& key) override;

    // Methods
    void importFiles();
    void importFolder();
    void scanFolderRecursive(const juce::File& folder);
    void scanDownloadsFolder();
    void addTrack(const juce::File& file, const juce::String& folderTag = "Todas las Pistas", const juce::String& rootFolder = {}, bool isDemo = false);
    void filterTracks(const juce::String& query);
    void unsyncFolder(const juce::String& folderPath);

    // Automix queue management
    void addToAutomixQueue(const std::vector<TrackItem>& tracks);
    void clearAutomixQueue();
    void setAutomixQueue(const std::vector<TrackItem>& tracks);
    void setAutomixRunning(bool running);
    void setAutomixPlayingIndex(int index);
    void deleteTrackFromAutomixQueue(int index);
    void reorderAutomixQueue(int fromIndex, int toIndex);
    void handleAutomixDropTracks(const juce::String& description);
    const std::vector<TrackItem>& getAutomixQueue() const { return automixQueue; }
    void setQueueMessage(const juce::String& message);
    void setSmartBusy(bool busy);
    bool isSmartBusy() const { return smartBusy; }
    std::vector<TrackItem> allLocalTrackItems() const;
    void updateTrackBpm(const juce::File& file, double bpm);
    void updateTrackKey(const juce::File& file, const juce::String& key);
    void updateTrackBpmAndKey(const juce::File& file, double bpm, const juce::String& key);

    // P17: playlists
    void refreshPlaylists();
    bool isPlaylistView() const { return currentSelectedFolder.startsWith("playlist:"); }
    juce::String currentPlaylistId() const { return currentSelectedFolder.substring((int) juce::String("playlist:").length()); }

    // MIDI / Hardware Browser Navigation
    void navigateBrowser(int delta);
    void handleBrowseClick();
    void loadSelectedTrack(int deckIndex);
    juce::File getSelectedTrackFile() const;
    bool isFocusOnFolders() const { return isBrowsingFolders; }

    enum class LibraryMode { Local, USB, YouTube, AIMatch, SoundCloud };
    void setLibraryMode(LibraryMode mode);
    LibraryMode getLibraryMode() const { return currentMode; }
    void updateActiveTrackContext(const CurrentTrackContext& ctx);

    std::function<void(int deckIndex, const juce::File& file)> onLoadTrack;
    std::function<void(int deckIndex, const struct YouTubeSearchResult& video)> onLoadYouTubeTrack;
    std::function<void(const std::vector<TrackItem>&)> onStartAutomix;
    std::function<void()> onStopAutomix;
    std::function<void()> onSmartAutomix;
    std::function<void(const std::vector<TrackItem>&)> onAutomixQueueChanged;
    std::function<void(int targetTab)> onOpenSettings;

private:
    juce::AudioFormatManager& formatManager;

    LibraryMode currentMode { LibraryMode::Local };
    std::unique_ptr<class YouTubeLibraryView> youtubeView;
    std::unique_ptr<AIMatchView> aiMatchView;
    std::unique_ptr<class UsbLibraryView> usbView;
    std::unique_ptr<class SoundCloudLibraryView> soundcloudView;

    // 1. Far-Left Vertical Icon Rail
    DjButton railLocalBtn;    // Music (cyan)
    DjButton railUsbBtn;      // USB (emerald green / white)
    DjButton railAiBtn;       // AI Match ✨ (purple)
    DjButton railYtBtn;       // YouTube / TV (red/slate)
    DjButton railScBtn;       // SoundCloud (orange)

    // 2. "Mis Archivos" Hierarchical Folder Tree
    FolderTreeComponent folderTree;
    juce::String currentSelectedFolder { "Todas las Pistas" };

    // 3. Center Table Area
    juce::Label tableHeaderTitle;
    juce::TextEditor searchBox;
    DjButton addBtn;
    juce::TableListBox table;

    // 4. Right Automix Panel
    DjButton automixHeader;
    DjButton startAutomixBtn;
    DjButton smartAutomixBtn;
    DjButton clearQueueBtn;
    juce::Label queueTitle;

    std::vector<TrackItem> allTracks;
    std::vector<TrackItem> filteredTracks;
    void applySorting();
    int currentSortColumnId { 0 };
    bool currentSortForwards { true };

    // P17: playlists (drag reorder dentro de la vista de playlist)
    int playlistDropRow { -1 };
    void showTrackContextMenu(int rowNumber);
    void showPlaylistNameDialog(const juce::String& title, const juce::String& initialName,
                                const std::function<void(const juce::String&)>& onAccepted);

    // Automix state
    std::vector<TrackItem> automixQueue;
    juce::Rectangle<float> automixDropArea;
    bool automixDropHover { false };
    bool automixRunning { false };
    bool smartBusy { false };
    int automixPlayingIndex { -1 };

    class AutomixQueueView;
    std::unique_ptr<AutomixQueueView> automixQueueView;

    class DownloadsEmptyOverlay;
    std::unique_ptr<DownloadsEmptyOverlay> downloadsEmptyOverlay;
    void updateDownloadsEmptyState();

    class BackgroundBpmScanner;
    std::unique_ptr<BackgroundBpmScanner> bpmScanner;

    std::unique_ptr<class AsyncFolderScanner> folderScanner;
    std::unordered_set<juce::String> knownTrackPaths;
    bool isFolderSyncing { false };
    bool tableNeedsRepaint { false };
    bool stemsRepaintLatch { false };
    void updateSyncStatus(int count, bool isScanning, const juce::String& currentFolder = {});

    // Hardware browsing state
    bool isBrowsingFolders { false };

    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LibraryComponent)
};

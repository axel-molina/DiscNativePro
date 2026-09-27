#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "DjButton.h"
#include "LucideIcons.h"
#include "FolderTreeComponent.h"
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
                         public juce::DragAndDropTarget
{
public:
    LibraryComponent(juce::AudioFormatManager& formatManager);
    ~LibraryComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // TableListBoxModel overrides
    int getNumRows() override;
    void paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected) override;
    void paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override;
    juce::Component* refreshComponentForCell(int rowNumber, int columnId, bool isRowSelected, juce::Component* existingComponentToUpdate) override;
    void cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent&) override;
    juce::var getDragSourceDescription(const juce::SparseSet<int>& currentlySelectedRows) override;

    // DragAndDropTarget overrides (Automix panel drop zone)
    bool isInterestedInDragSource(const SourceDetails& dragSourceDetails) override;
    void itemDragEnter(const SourceDetails& dragSourceDetails) override;
    void itemDragExit(const SourceDetails& dragSourceDetails) override;
    void itemDropped(const SourceDetails& dragSourceDetails) override;

    // Keyboard handling (Cmd+A select all)
    bool keyPressed(const juce::KeyPress& key) override;

    // Methods
    void importFiles();
    void importFolder();
    void scanFolderRecursive(const juce::File& folder);
    void addTrack(const juce::File& file, const juce::String& folderTag = "Todas las Pistas", const juce::String& rootFolder = {}, bool isDemo = false);
    void filterTracks(const juce::String& query);
    void unsyncFolder(const juce::String& folderPath);

    // Automix queue management
    void addToAutomixQueue(const std::vector<TrackItem>& tracks);
    void clearAutomixQueue();
    void setAutomixRunning(bool running);

    // MIDI / Hardware Browser Navigation
    void navigateBrowser(int delta);
    void handleBrowseClick();
    void loadSelectedTrack(int deckIndex);
    juce::File getSelectedTrackFile() const;
    bool isFocusOnFolders() const { return isBrowsingFolders; }

    enum class LibraryMode { Local, YouTube };
    void setLibraryMode(LibraryMode mode);
    LibraryMode getLibraryMode() const { return currentMode; }

    std::function<void(int deckIndex, const juce::File& file)> onLoadTrack;
    std::function<void(int deckIndex, const struct YouTubeSearchResult& video)> onLoadYouTubeTrack;
    std::function<void(const std::vector<TrackItem>&)> onStartAutomix;
    std::function<void()> onStopAutomix;

private:
    juce::AudioFormatManager& formatManager;

    LibraryMode currentMode { LibraryMode::Local };
    std::unique_ptr<class YouTubeLibraryView> youtubeView;

    // 1. Far-Left Vertical Icon Rail
    DjButton railIcon1; // Music (cyan)
    DjButton railIcon2; // Radio (coral/red)
    DjButton railIcon3; // Disc (turntable)
    DjButton railIcon4; // Tv
    DjButton railIcon5; // MoreHorizontal

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
    DjButton clearQueueBtn;
    juce::Label queueTitle;

    std::vector<TrackItem> allTracks;
    std::vector<TrackItem> filteredTracks;

    // Automix state
    std::vector<TrackItem> automixQueue;
    bool automixDropHover { false };
    bool automixRunning { false };

    // Hardware browsing state
    bool isBrowsingFolders { false };

    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LibraryComponent)
};

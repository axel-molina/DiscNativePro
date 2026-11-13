#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "LucideIcons.h"
#include "DjButton.h"
#include "../services/LanguageManager.h"
#include <vector>
#include <set>
#include <map>
#include <memory>
#include <functional>

// Forward declaration of TrackItem
struct TrackItem;

struct FolderNode
{
    juce::String name;
    juce::String fullPath;
    int depth { 0 };
    int directCount { 0 };
    int totalCount { 0 };
    bool isSystem { false };
    std::vector<FolderNode> children;
};

struct FlatFolderRow
{
    juce::String name;
    juce::String fullPath;
    int depth { 0 };
    int directCount { 0 };
    int totalCount { 0 };
    bool isSystem { false };
    bool hasChildren { false };
    bool isExpanded { false };
    bool isPlaylistHeader { false };
    bool isPlaylist { false };
};

// P17: fila mínima para la sección Playlists del árbol.
struct PlaylistRowInfo
{
    juce::String id;
    juce::String name;
    int trackCount { 0 };
};

class FolderTreeComponent : public juce::Component,
                            public LanguageManager::Listener
{
public:
    FolderTreeComponent();
    ~FolderTreeComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void languageChanged() override;
    void refreshLanguage();

    void updateTree(const std::vector<TrackItem>& tracks);
    void setPlaylists(const std::vector<PlaylistRowInfo>& playlists);
    void setSelectedFolder(const juce::String& folderPath);
    juce::String getSelectedFolder() const { return selectedFolder; }
    void expandFolder(const juce::String& folderPath);
    void navigateFolders(int delta);
    bool toggleExpandCurrentFolder(); // Returns true if it had children and expanded/collapsed, false if leaf folder

    std::function<void(const juce::String& folderPath)> onSelectFolder;
    std::function<void()> onAddFolder;
    std::function<void(const juce::String& folderPath)> onUnsyncFolder;
    std::function<void(const juce::String& folderPath)> onPrecalcFolder;
    std::function<void()> onCreatePlaylist;
    std::function<void(const juce::String& playlistId)> onRenamePlaylist;
    std::function<void(const juce::String& playlistId)> onDeletePlaylist;
    // P17: soltar pistas (rutas "\n"-separadas) sobre una fila de playlist las añade
    std::function<void(const juce::String& playlistId, const juce::StringArray& paths)> onTracksDroppedIntoPlaylist;

private:
    class TreeViewInternal : public juce::Component,
                             public juce::DragAndDropTarget
    {
    public:
        TreeViewInternal(FolderTreeComponent& owner);

        void paint(juce::Graphics& g) override;
        void mouseMove(const juce::MouseEvent& e) override;
        void mouseExit(const juce::MouseEvent& e) override;
        void mouseDown(const juce::MouseEvent& e) override;

        bool isInterestedInDragSource(const SourceDetails&) override { return true; }
        void itemDragEnter(const SourceDetails& d) override { itemDragMove(d); }
        void itemDragMove(const SourceDetails& d) override;
        void itemDragExit(const SourceDetails&) override;
        void itemDropped(const SourceDetails& d) override;

        int getRowHeight() const { return 28; }
        int getTotalContentHeight() const;

    private:
        FolderTreeComponent& owner;
        int hoveredRow { -1 };
    };

    void rebuildFlatRows();

    juce::Label headerTitleLabel;
    DjButton addFolderBtn;

    juce::Viewport viewport;
    std::unique_ptr<TreeViewInternal> treeView;

    std::vector<FolderNode> rootNodes;
    std::vector<FlatFolderRow> flatRows;
    std::vector<PlaylistRowInfo> playlistsInfo;
    bool playlistRowsVisible { true };
    int dragHoverRow { -1 };
    std::set<juce::String> expandedFolders;
    juce::String selectedFolder { "Todas las Pistas" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FolderTreeComponent)
};

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "LucideIcons.h"
#include "DjButton.h"
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
};

class FolderTreeComponent : public juce::Component
{
public:
    FolderTreeComponent();
    ~FolderTreeComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void updateTree(const std::vector<TrackItem>& tracks);
    void setSelectedFolder(const juce::String& folderPath);
    juce::String getSelectedFolder() const { return selectedFolder; }
    void expandFolder(const juce::String& folderPath);

    std::function<void(const juce::String& folderPath)> onSelectFolder;
    std::function<void()> onAddFolder;
    std::function<void(const juce::String& folderPath)> onUnsyncFolder;

private:
    class TreeViewInternal : public juce::Component
    {
    public:
        TreeViewInternal(FolderTreeComponent& owner);

        void paint(juce::Graphics& g) override;
        void mouseMove(const juce::MouseEvent& e) override;
        void mouseExit(const juce::MouseEvent& e) override;
        void mouseDown(const juce::MouseEvent& e) override;

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
    std::set<juce::String> expandedFolders;
    juce::String selectedFolder { "Todas las Pistas" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FolderTreeComponent)
};

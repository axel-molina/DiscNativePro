#include "FolderTreeComponent.h"
#include "LibraryComponent.h"

namespace
{
    struct TempNode
    {
        juce::String name;
        juce::String fullPath;
        int depth { 0 };
        int directCount { 0 };
        std::map<juce::String, std::shared_ptr<TempNode>> childrenMap;
    };
}

FolderTreeComponent::TreeViewInternal::TreeViewInternal(FolderTreeComponent& o)
    : owner(o)
{
    setInterceptsMouseClicks(true, false);
}

int FolderTreeComponent::TreeViewInternal::getTotalContentHeight() const
{
    return juce::jmax(10, static_cast<int>(owner.flatRows.size()) * getRowHeight() + 8);
}

void FolderTreeComponent::TreeViewInternal::paint(juce::Graphics& g)
{
    for (int i = 0; i < static_cast<int>(owner.flatRows.size()); ++i)
    {
        const auto& row = owner.flatRows[static_cast<size_t>(i)];
        int y = i * getRowHeight();
        auto rowRect = juce::Rectangle<int>(4, y + 1, getWidth() - 8, getRowHeight() - 2);
        bool isSelected = (row.fullPath == owner.selectedFolder);

        // Row background
        if (isSelected)
        {
            g.setColour(juce::Colour::fromRGB(59, 62, 71)); // #3b3e47 active selection
            g.fillRoundedRectangle(rowRect.toFloat(), 5.0f);
        }
        else if (i == hoveredRow)
        {
            g.setColour(juce::Colour::fromRGB(255, 255, 255).withAlpha(0.04f));
            g.fillRoundedRectangle(rowRect.toFloat(), 5.0f);
        }

        int indent = rowRect.getX() + row.depth * 14;

        // 1. Expand / Collapse Chevron Button (if it has children)
        if (row.hasChildren)
        {
            auto chevBox = juce::Rectangle<float>((float)indent, (float)(y + 5), 18.0f, 18.0f);
            g.setColour(juce::Colour::fromRGB(24, 27, 36));
            g.fillRoundedRectangle(chevBox, 3.5f);
            g.setColour(juce::Colour::fromRGB(45, 50, 68));
            g.drawRoundedRectangle(chevBox, 3.5f, 1.0f);

            if (row.isExpanded)
            {
                LucideIcons::draw(g, LucideIcons::IconType::ChevronDown,
                                  chevBox.reduced(3.5f),
                                  juce::Colour::fromRGB(0, 229, 255), 1.8f);
            }
            else
            {
                LucideIcons::draw(g, LucideIcons::IconType::ChevronRight,
                                  chevBox.reduced(3.5f),
                                  juce::Colour::fromRGB(142, 149, 165), 1.8f);
            }
            indent += 22;
        }
        else
        {
            if (row.depth > 0)
                indent += 18;
            else
                indent += 4;
        }

        // 2. Folder Icon
        auto folderRect = juce::Rectangle<float>((float)indent, (float)(y + 7), 14.0f, 14.0f);
        auto folderCol = isSelected ? juce::Colours::white : juce::Colour::fromRGB(148, 163, 184);
        LucideIcons::draw(g, LucideIcons::IconType::Folder, folderRect, folderCol, 1.5f);
        indent += 19;

        // 3. Count badge
        int count = row.isSystem ? row.totalCount : row.directCount;
        juce::String countStr = (count > 0) ? juce::String(count) : juce::String();
        int badgeWidth = countStr.isNotEmpty() ? 30 : 0;

        if (countStr.isNotEmpty())
        {
            auto badgeRect = juce::Rectangle<int>(rowRect.getRight() - badgeWidth - 4, y, badgeWidth, getRowHeight());
            g.setColour(isSelected ? juce::Colour::fromRGB(200, 205, 215) : juce::Colour::fromRGB(100, 108, 125));
            g.setFont(juce::FontOptions("Menlo", 10.0f, juce::Font::plain));
            g.drawText(countStr, badgeRect, juce::Justification::centredRight, false);
        }

        // 4. Folder Name
        int textWidth = (rowRect.getRight() - badgeWidth - 6) - indent;
        if (textWidth > 10)
        {
            auto textRect = juce::Rectangle<int>(indent, y, textWidth, getRowHeight());
            g.setColour(isSelected ? juce::Colours::white : juce::Colour::fromRGB(203, 213, 225));
            g.setFont(juce::FontOptions(11.5f, isSelected ? juce::Font::bold : juce::Font::plain));
            g.drawText(row.name, textRect, juce::Justification::centredLeft, true);
        }
    }
}

void FolderTreeComponent::TreeViewInternal::mouseMove(const juce::MouseEvent& e)
{
    int newHover = e.y / getRowHeight();
    if (newHover < 0 || newHover >= static_cast<int>(owner.flatRows.size()))
        newHover = -1;

    if (newHover != hoveredRow)
    {
        hoveredRow = newHover;
        repaint();
    }
}

void FolderTreeComponent::TreeViewInternal::mouseExit(const juce::MouseEvent&)
{
    if (hoveredRow != -1)
    {
        hoveredRow = -1;
        repaint();
    }
}

void FolderTreeComponent::TreeViewInternal::mouseDown(const juce::MouseEvent& e)
{
    int rowIndex = e.y / getRowHeight();
    if (rowIndex < 0 || rowIndex >= static_cast<int>(owner.flatRows.size()))
        return;

    const auto& row = owner.flatRows[static_cast<size_t>(rowIndex)];

    // Right Click Context Menu
    if (e.mods.isPopupMenu())
    {
        if (!row.isSystem)
        {
            juce::PopupMenu m;
            m.addItem(1, juce::String::fromUTF8("Desincronizar carpeta '") + row.name + juce::String::fromUTF8("' (") + juce::String(row.totalCount) + juce::String::fromUTF8(" canciones)"), true, false);
            m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
                [this, path = row.fullPath](int res) {
                    if (res == 1 && owner.onUnsyncFolder)
                    {
                        owner.onUnsyncFolder(path);
                    }
                });
        }
        return;
    }

    // Left Click: Check if click was on Chevron Button
    if (row.hasChildren)
    {
        int indent = 4 + row.depth * 14;
        juce::Rectangle<int> chevHitBox(indent - 2, rowIndex * getRowHeight() + 2, 24, 24);
        if (chevHitBox.contains(e.x, e.y))
        {
            if (owner.expandedFolders.count(row.fullPath) > 0)
                owner.expandedFolders.erase(row.fullPath);
            else
                owner.expandedFolders.insert(row.fullPath);

            owner.rebuildFlatRows();
            setSize(owner.viewport.getWidth(), getTotalContentHeight());
            repaint();
            return;
        }
    }

    // Click on row selects the folder
    owner.selectedFolder = row.fullPath;
    repaint();

    if (owner.onSelectFolder)
    {
        owner.onSelectFolder(row.fullPath);
    }
}

// --- FolderTreeComponent Implementation ---

FolderTreeComponent::FolderTreeComponent()
{
    headerTitleLabel.setText("Mis Archivos", juce::dontSendNotification);
    headerTitleLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    headerTitleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(headerTitleLabel);

    addFolderBtn.setIcon(LucideIcons::IconType::Plus, 13.0f);
    addFolderBtn.setCornerRadius(4.0f);
    addFolderBtn.setCustomColours(juce::Colour::fromRGB(20, 24, 32),
                                  juce::Colour::fromRGB(0, 229, 255),
                                  juce::Colour::fromRGB(0, 229, 255),
                                  juce::Colour::fromRGB(38, 43, 56));
    addFolderBtn.setTooltip(juce::String::fromUTF8("Importar carpeta o archivos con subcarpetas"));
    addFolderBtn.onClick = [this]() {
        if (onAddFolder) onAddFolder();
    };
    addAndMakeVisible(addFolderBtn);

    treeView = std::make_unique<TreeViewInternal>(*this);
    viewport.setViewedComponent(treeView.get(), false);
    viewport.setScrollBarsShown(true, false);
    viewport.setScrollBarThickness(6);
    addAndMakeVisible(viewport);
}

FolderTreeComponent::~FolderTreeComponent()
{
}

void FolderTreeComponent::paint(juce::Graphics& g)
{
    // Clean header border #1b1e26
    g.setColour(juce::Colour::fromRGB(27, 30, 38));
    g.drawHorizontalLine(32, 0.0f, (float)getWidth());
}

void FolderTreeComponent::resized()
{
    auto area = getLocalBounds();
    auto headerArea = area.removeFromTop(32).reduced(4, 3);
    headerTitleLabel.setBounds(headerArea.removeFromLeft(getWidth() - 36));
    addFolderBtn.setBounds(headerArea.removeFromRight(24));

    viewport.setBounds(area);
    if (treeView != nullptr)
    {
        treeView->setSize(viewport.getWidth(), treeView->getTotalContentHeight());
    }
}

void FolderTreeComponent::setSelectedFolder(const juce::String& folderPath)
{
    selectedFolder = folderPath;
    if (treeView != nullptr)
    {
        treeView->repaint();
    }
}

void FolderTreeComponent::expandFolder(const juce::String& folderPath)
{
    expandedFolders.insert(folderPath);
    rebuildFlatRows();
    if (treeView != nullptr)
    {
        treeView->setSize(viewport.getWidth(), treeView->getTotalContentHeight());
        treeView->repaint();
    }
}

void FolderTreeComponent::navigateFolders(int delta)
{
    if (flatRows.empty()) return;
    int currentIndex = -1;
    for (size_t i = 0; i < flatRows.size(); ++i)
    {
        if (flatRows[i].fullPath == selectedFolder)
        {
            currentIndex = static_cast<int>(i);
            break;
        }
    }
    if (currentIndex == -1)
        currentIndex = 0;
    else
        currentIndex = juce::jlimit(0, static_cast<int>(flatRows.size()) - 1, currentIndex + delta);

    setSelectedFolder(flatRows[static_cast<size_t>(currentIndex)].fullPath);
    if (onSelectFolder)
        onSelectFolder(selectedFolder);

    // Scroll viewport to ensure selected folder is visible
    if (treeView != nullptr)
    {
        int rowY = currentIndex * treeView->getRowHeight();
        viewport.setViewPosition(viewport.getViewPositionX(), juce::jmax(0, rowY - 50));
    }
}

bool FolderTreeComponent::toggleExpandCurrentFolder()
{
    for (const auto& row : flatRows)
    {
        if (row.fullPath == selectedFolder)
        {
            if (row.hasChildren)
            {
                if (expandedFolders.count(row.fullPath) > 0)
                    expandedFolders.erase(row.fullPath);
                else
                    expandedFolders.insert(row.fullPath);

                rebuildFlatRows();
                if (treeView != nullptr)
                {
                    treeView->setSize(viewport.getWidth(), treeView->getTotalContentHeight());
                    treeView->repaint();
                }
                return true;
            }
            break;
        }
    }
    return false;
}

void FolderTreeComponent::updateTree(const std::vector<TrackItem>& tracks)
{
    rootNodes.clear();

    // 1. Root Node: Todas las Pistas
    FolderNode allPistas;
    allPistas.name = "Todas las Pistas";
    allPistas.fullPath = "Todas las Pistas";
    allPistas.depth = 0;
    allPistas.directCount = static_cast<int>(tracks.size());
    allPistas.totalCount = static_cast<int>(tracks.size());
    allPistas.isSystem = true;
    rootNodes.push_back(allPistas);

    // 2. Root Node: Demos DiscPro
    FolderNode demos;
    demos.name = "Demos DiscPro";
    demos.fullPath = "Demos DiscPro";
    demos.depth = 0;
    demos.isSystem = true;
    int demoCount = 0;
    for (const auto& t : tracks)
    {
        if (t.isDemo || t.folder == "Demos DiscPro")
            demoCount++;
    }
    demos.directCount = demoCount;
    demos.totalCount = demoCount;
    rootNodes.push_back(demos);

    // 3. Hierarchical User Folders
    std::map<juce::String, std::shared_ptr<TempNode>> rootChildrenMap;

    for (const auto& t : tracks)
    {
        if (t.isDemo || t.folder == "Demos DiscPro" || t.folder == "Todas las Pistas")
            continue;

        juce::String rawFolder = t.folder.trim();
        if (rawFolder.isEmpty())
            rawFolder = "Mis Archivos";

        juce::StringArray segments;
        int start = 0;
        while (true)
        {
            int next = rawFolder.indexOf(start, " / ");
            if (next < 0)
            {
                auto token = rawFolder.substring(start).trim();
                if (token.isNotEmpty()) segments.add(token);
                break;
            }
            auto token = rawFolder.substring(start, next).trim();
            if (token.isNotEmpty()) segments.add(token);
            start = next + 3;
        }

        if (segments.isEmpty() && rawFolder.contains("/"))
        {
            segments.addTokens(rawFolder, "/", "\"");
            for (auto& s : segments) s = s.trim();
            segments.removeEmptyStrings();
        }

        if (segments.isEmpty())
        {
            segments.add(rawFolder);
        }

        auto* currentMap = &rootChildrenMap;
        juce::String accumulatedPath;
        for (int i = 0; i < segments.size(); ++i)
        {
            auto seg = segments[i];
            accumulatedPath = accumulatedPath.isNotEmpty() ? (accumulatedPath + " / " + seg) : seg;
            bool isLeaf = (i == segments.size() - 1);

            auto it = currentMap->find(seg);
            std::shared_ptr<TempNode> node;
            if (it == currentMap->end())
            {
                node = std::make_shared<TempNode>();
                node->name = seg;
                node->fullPath = accumulatedPath;
                node->depth = i;
                node->directCount = 0;
                (*currentMap)[seg] = node;
            }
            else
            {
                node = it->second;
            }

            if (isLeaf)
            {
                node->directCount++;
            }

            currentMap = &(node->childrenMap);
        }
    }

    std::function<FolderNode(const TempNode&)> convertTemp = [&](const TempNode& temp) -> FolderNode {
        FolderNode fn;
        fn.name = temp.name;
        fn.fullPath = temp.fullPath;
        fn.depth = temp.depth;
        fn.directCount = temp.directCount;
        fn.isSystem = false;
        int subSum = 0;
        for (const auto& pair : temp.childrenMap)
        {
            auto childNode = convertTemp(*pair.second);
            subSum += childNode.totalCount;
            fn.children.push_back(childNode);
        }
        fn.totalCount = fn.directCount + subSum;
        return fn;
    };

    for (const auto& pair : rootChildrenMap)
    {
        rootNodes.push_back(convertTemp(*pair.second));
    }

    rebuildFlatRows();

    if (treeView != nullptr)
    {
        treeView->setSize(viewport.getWidth(), treeView->getTotalContentHeight());
        treeView->repaint();
    }
}

void FolderTreeComponent::rebuildFlatRows()
{
    flatRows.clear();

    std::function<void(const FolderNode&)> flatten = [&](const FolderNode& node) {
        FlatFolderRow row;
        row.name = node.name;
        row.fullPath = node.fullPath;
        row.depth = node.depth;
        row.directCount = node.directCount;
        row.totalCount = node.totalCount;
        row.isSystem = node.isSystem;
        row.hasChildren = !node.children.empty();
        row.isExpanded = (expandedFolders.count(node.fullPath) > 0);
        flatRows.push_back(row);

        if (row.hasChildren && row.isExpanded)
        {
            for (const auto& child : node.children)
            {
                flatten(child);
            }
        }
    };

    for (const auto& root : rootNodes)
    {
        flatten(root);
    }
}

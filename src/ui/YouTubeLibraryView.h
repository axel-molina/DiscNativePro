#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../services/YouTubeService.h"
#include "DjButton.h"
#include "LucideIcons.h"
#include <vector>
#include <functional>

class YouTubeLibraryView : public juce::Component,
                           public juce::TableListBoxModel
{
public:
    YouTubeLibraryView();
    ~YouTubeLibraryView() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // TableListBoxModel overrides
    int getNumRows() override;
    void paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected) override;
    void paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override;
    juce::Component* refreshComponentForCell(int rowNumber, int columnId, bool isRowSelected, juce::Component* existingComponentToUpdate) override;
    void cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent&) override;
    juce::var getDragSourceDescription(const juce::SparseSet<int>& currentlySelectedRows) override;

    void performSearch(const juce::String& query);
    void handlePasteUrl();
    void updateApiStatus();

    std::function<void(int deckIndex, const YouTubeSearchResult& video)> onLoadTrack;
    std::function<void()> onOpenSettings;

private:
    juce::Label headerTitle;
    juce::TextEditor searchBox;
    DjButton searchBtn;
    DjButton pasteUrlBtn;
    juce::Label apiStatusBadge;
    juce::Label statusLabel;

    // Quick genre tag buttons
    std::vector<std::unique_ptr<DjButton>> quickTagButtons;

    juce::TableListBox table;
    std::vector<YouTubeSearchResult> searchResults;
    bool isSearching { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(YouTubeLibraryView)
};

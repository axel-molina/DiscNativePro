#include "SoundCloudLibraryView.h"

namespace
{
    class SoundCloudLoadButtonsComponent : public juce::Component
    {
    public:
        SoundCloudLoadButtonsComponent(std::function<void(int)> onClickCallback)
            : onDeckLoad(std::move(onClickCallback))
        {
            btn1.setButtonText("1");
            btn1.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(255, 85, 0).withAlpha(0.2f));
            btn1.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(255, 140, 60));
            btn1.onClick = [this]() { if (onDeckLoad) onDeckLoad(0); };
            addAndMakeVisible(btn1);

            btn2.setButtonText("2");
            btn2.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(255, 85, 0).withAlpha(0.2f));
            btn2.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(255, 140, 60));
            btn2.onClick = [this]() { if (onDeckLoad) onDeckLoad(1); };
            addAndMakeVisible(btn2);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced(2, 2);
            btn1.setBounds(area.removeFromLeft(30).reduced(1, 1));
            area.removeFromLeft(4);
            btn2.setBounds(area.removeFromLeft(30).reduced(1, 1));
        }

    private:
        juce::TextButton btn1;
        juce::TextButton btn2;
        std::function<void(int)> onDeckLoad;
    };
}

SoundCloudLibraryView::SoundCloudLibraryView()
{
    headerTitle.setText("SoundCloud DJ Streaming", juce::dontSendNotification);
    headerTitle.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    headerTitle.setColour(juce::Label::textColourId, juce::Colour::fromRGB(255, 85, 0));
    addAndMakeVisible(headerTitle);

    searchBox.setFont(juce::FontOptions(12.5f));
    searchBox.setColour(juce::TextEditor::backgroundColourId, juce::Colour::fromRGB(16, 18, 24));
    searchBox.setColour(juce::TextEditor::outlineColourId, juce::Colour::fromRGB(36, 40, 56));
    searchBox.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    searchBox.onReturnKey = [this]() { performSearch(searchBox.getText()); };
    addAndMakeVisible(searchBox);

    searchBtn.setIcon(LucideIcons::IconType::Search, 13.0f);
    searchBtn.setFontSize(11.0f);
    searchBtn.setCornerRadius(0.0f);
    searchBtn.setCustomColours(juce::Colour::fromRGB(255, 85, 0).withAlpha(0.25f),
                               juce::Colours::white,
                               juce::Colours::white,
                               juce::Colour::fromRGB(255, 85, 0).withAlpha(0.5f));
    searchBtn.onClick = [this]() { performSearch(searchBox.getText()); };
    searchBtn.setTooltip(LanguageManager::trUtf8("Busca en SoundCloud el texto del campo", "Search SoundCloud for the query"));
    addAndMakeVisible(searchBtn);

    statusLabel.setFont(juce::FontOptions(11.0f));
    statusLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(142, 149, 165));
    addAndMakeVisible(statusLabel);

    table.setModel(this);
    table.setColour(juce::ListBox::backgroundColourId, juce::Colour::fromRGB(12, 13, 16));
    table.getHeader().setColour(juce::TableHeaderComponent::backgroundColourId, juce::Colour::fromRGB(16, 18, 23));
    table.getHeader().setColour(juce::TableHeaderComponent::textColourId, juce::Colour::fromRGB(148, 163, 184));
    table.getHeader().addColumn("", 1, 46, 46, 46);
    table.getHeader().addColumn("TÍTULO", 2, 360, 180, 600);
    table.getHeader().addColumn("ARTISTA / CREADOR", 3, 240, 140, 400);
    table.getHeader().addColumn("DURACIÓN", 4, 80, 60, 100);
    table.getHeader().addColumn("CARGAR", 5, 76, 76, 76);
    table.setRowHeight(44);
    table.setMultipleSelectionEnabled(false);
    addAndMakeVisible(table);

    LanguageManager::getInstance().addListener(this);
    refreshLanguage();

    performSearch("Tech House DJ Mix");
}

SoundCloudLibraryView::~SoundCloudLibraryView()
{
    LanguageManager::getInstance().removeListener(this);
}

void SoundCloudLibraryView::languageChanged()
{
    refreshLanguage();
}

void SoundCloudLibraryView::refreshLanguage()
{
    searchBtn.setText(LanguageManager::trUtf8("Buscar", "Search"));
    searchBox.setTextToShowWhenEmpty(LanguageManager::trUtf8("Buscar tracks, remixes o sets en SoundCloud...",
                                                             "Search tracks, remixes or sets on SoundCloud..."),
                                     juce::Colour::fromRGB(100, 108, 125));

    table.getHeader().setColumnName(2, LanguageManager::trUtf8("TÍTULO", "TITLE"));
    table.getHeader().setColumnName(3, LanguageManager::trUtf8("ARTISTA / CREADOR", "ARTIST / CREATOR"));
    table.getHeader().setColumnName(4, LanguageManager::trUtf8("DURACIÓN", "DURATION"));
    table.getHeader().setColumnName(5, LanguageManager::trUtf8("CARGAR", "LOAD"));

    table.repaint();
    repaint();
}

void SoundCloudLibraryView::performSearch(const juce::String& query)
{
    auto q = query.trim();
    if (q.isEmpty())
        return;

    isSearching = true;
    statusLabel.setText(LanguageManager::trUtf8("Buscando en SoundCloud...", "Searching on SoundCloud..."), juce::dontSendNotification);
    repaint();

    SoundCloudService::searchAsync(q, [this, q](const std::vector<SoundCloudTrackItem>& results) {
        isSearching = false;
        searchResults = results;
        table.updateContent();
        table.repaint();

        if (searchResults.empty())
        {
            statusLabel.setText(LanguageManager::trUtf8("No se encontraron resultados para '", "No results found for '") + q + "'.", juce::dontSendNotification);
        }
        else
        {
            statusLabel.setText(LanguageManager::trUtf8("Mostrando ", "Showing ") + juce::String((int)searchResults.size())
                                + LanguageManager::trUtf8(" tracks encontrados en SoundCloud (MP3 directo para mezclar)", " tracks found on SoundCloud (Direct MP3 for mixing)"),
                                juce::dontSendNotification);
        }
    });
}

int SoundCloudLibraryView::getNumRows()
{
    return static_cast<int>(searchResults.size());
}

void SoundCloudLibraryView::paintRowBackground(juce::Graphics& g, int rowNumber, int /*width*/, int /*height*/, bool rowIsSelected)
{
    if (rowIsSelected)
        g.fillAll(juce::Colour::fromRGB(45, 30, 20));
    else if (rowNumber % 2 == 1)
        g.fillAll(juce::Colour::fromRGB(15, 17, 23));
    else
        g.fillAll(juce::Colour::fromRGB(12, 13, 16));
}

void SoundCloudLibraryView::paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool /*rowIsSelected*/)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(searchResults.size()))
        return;

    const auto& track = searchResults[static_cast<size_t>(rowNumber)];

    if (columnId == 1) // Cover Thumbnail
    {
        auto iconBox = juce::Rectangle<float>(6.0f, (static_cast<float>(height) - 32.0f) * 0.5f, 32.0f, 32.0f);
        g.setColour(juce::Colour::fromRGB(28, 20, 16));
        g.fillRoundedRectangle(iconBox, 0.0f);

        LucideIcons::draw(g, LucideIcons::IconType::SoundCloud, iconBox.reduced(6.0f), juce::Colour::fromRGB(255, 85, 0));
    }
    else if (columnId == 2) // Title
    {
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.drawText(track.title, 8, 0, width - 12, height, juce::Justification::centredLeft, true);
    }
    else if (columnId == 3) // Artist / Uploader
    {
        g.setColour(juce::Colour::fromRGB(255, 140, 60));
        g.setFont(juce::FontOptions(11.5f));
        g.drawText(track.user, 6, 0, width - 12, height, juce::Justification::centredLeft, true);
    }
    else if (columnId == 4) // Duration
    {
        g.setColour(juce::Colour::fromRGB(148, 163, 184));
        g.setFont(juce::FontOptions("Menlo", 11.0f, juce::Font::plain));
        g.drawText(track.durationText, 4, 0, width - 8, height, juce::Justification::centredLeft, false);
    }
}

juce::Component* SoundCloudLibraryView::refreshComponentForCell(int rowNumber, int columnId, bool /*isRowSelected*/, juce::Component* existingComponentToUpdate)
{
    if (columnId == 5) // CARGAR
    {
        auto* comp = static_cast<SoundCloudLoadButtonsComponent*>(existingComponentToUpdate);
        if (comp == nullptr)
        {
            comp = new SoundCloudLoadButtonsComponent([this, rowNumber](int deckIndex) {
                if (rowNumber >= 0 && rowNumber < static_cast<int>(searchResults.size()))
                {
                    const auto& track = searchResults[static_cast<size_t>(rowNumber)];
                    statusLabel.setText(LanguageManager::trUtf8("Descargando track de SoundCloud para Deck ", "Downloading SoundCloud track for Deck ")
                                        + juce::String(deckIndex + 1) + "...", juce::dontSendNotification);

                    SoundCloudService::downloadTrackAsync(track, [this, deckIndex, track](const juce::File& file) {
                        if (file.existsAsFile())
                        {
                            statusLabel.setText(LanguageManager::trUtf8("Track cargado: ", "Track loaded: ") + track.title, juce::dontSendNotification);
                            if (onLoadTrack)
                                onLoadTrack(deckIndex, file, track.title, track.user);
                        }
                        else
                        {
                            statusLabel.setText(LanguageManager::trUtf8("Error al descargar stream de SoundCloud", "Error downloading SoundCloud stream"), juce::dontSendNotification);
                        }
                    });
                }
            });
        }
        return comp;
    }

    return nullptr;
}

void SoundCloudLibraryView::cellDoubleClicked(int rowNumber, int /*columnId*/, const juce::MouseEvent&)
{
    if (rowNumber >= 0 && rowNumber < static_cast<int>(searchResults.size()))
    {
        const auto& track = searchResults[static_cast<size_t>(rowNumber)];
        SoundCloudService::downloadTrackAsync(track, [this, track](const juce::File& file) {
            if (file.existsAsFile() && onLoadTrack)
                onLoadTrack(0, file, track.title, track.user);
        });
    }
}

juce::var SoundCloudLibraryView::getDragSourceDescription(const juce::SparseSet<int>& currentlySelectedRows)
{
    juce::ignoreUnused(currentlySelectedRows);
    return {};
}

void SoundCloudLibraryView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(12, 13, 16));

    // Top Header background (36px)
    auto headerRect = getLocalBounds().removeFromTop(36).toFloat();
    g.setColour(juce::Colour::fromRGB(16, 18, 23));
    g.fillRect(headerRect);
    g.setColour(juce::Colour::fromRGB(28, 31, 40));
    g.drawHorizontalLine(35, 0.0f, (float)getWidth());

    // SoundCloud brand icon
    auto iconRect = juce::Rectangle<float>(12.0f, 9.0f, 18.0f, 18.0f);
    LucideIcons::draw(g, LucideIcons::IconType::SoundCloud, iconRect, juce::Colour::fromRGB(255, 85, 0));
}

void SoundCloudLibraryView::resized()
{
    auto area = getLocalBounds();

    // 1. Header (36px)
    auto header = area.removeFromTop(36).reduced(8, 4);
    header.removeFromLeft(28); // icon space
    headerTitle.setBounds(header.removeFromLeft(200));

    statusLabel.setBounds(header.removeFromRight(360));
    searchBtn.setBounds(header.removeFromRight(76).reduced(0, 1));
    header.removeFromRight(6);
    searchBox.setBounds(header.removeFromRight(juce::jmin(300, header.getWidth())));

    // 2. Table Area
    table.setBounds(area);
}

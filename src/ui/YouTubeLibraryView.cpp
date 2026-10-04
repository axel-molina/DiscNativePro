#include "YouTubeLibraryView.h"

namespace
{
    class YtLoadButtonsComponent : public juce::Component
    {
    public:
        YtLoadButtonsComponent()
        {
            btn1.setButtonText("1");
            btn1.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(0, 180, 216).withAlpha(0.15f));
            btn1.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(0, 210, 255));
            btn1.onClick = [this]() { if (onDeckLoad) onDeckLoad(0); };
            addAndMakeVisible(btn1);

            btn2.setButtonText("2");
            btn2.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(0, 229, 255).withAlpha(0.15f));
            btn2.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(0, 229, 255));
            btn2.onClick = [this]() { if (onDeckLoad) onDeckLoad(1); };
            addAndMakeVisible(btn2);
        }

        void setCallback(std::function<void(int)> onClickCallback)
        {
            onDeckLoad = std::move(onClickCallback);
        }

        void resized() override
        {
            auto area = getLocalBounds();
            int btnH = 28;
            int btnW = 32;
            int y = (area.getHeight() - btnH) / 2;
            int startX = (area.getWidth() - (btnW * 2 + 4)) / 2;
            btn1.setBounds(startX, y, btnW, btnH);
            btn2.setBounds(startX + btnW + 4, y, btnW, btnH);
        }

    private:
        juce::TextButton btn1;
        juce::TextButton btn2;
        std::function<void(int)> onDeckLoad;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(YtLoadButtonsComponent)
    };
}

YouTubeLibraryView::YouTubeLibraryView()
{
    // 1. Header Title
    headerTitle.setText("YouTube DJ Video Streaming", juce::dontSendNotification);
    headerTitle.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    headerTitle.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(headerTitle);

    // 2. Search Box
    searchBox.setTextToShowWhenEmpty("Buscar canciones, mixes o pegar enlace de YouTube...", juce::Colour::fromRGB(110, 118, 135));
    searchBox.setColour(juce::TextEditor::backgroundColourId, juce::Colour::fromRGB(16, 18, 25));
    searchBox.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    searchBox.setColour(juce::TextEditor::outlineColourId, juce::Colour::fromRGB(38, 43, 58));
    searchBox.onReturnKey = [this]() { performSearch(searchBox.getText()); };
    addAndMakeVisible(searchBox);

    // 3. Search Button
    searchBtn.setIcon(LucideIcons::IconType::Search, 13.0f);
    searchBtn.setText("Buscar");
    searchBtn.setFontSize(11.0f);
    searchBtn.setCornerRadius(5.0f);
    searchBtn.setCustomColours(juce::Colour::fromRGB(22, 26, 36),
                               juce::Colour::fromRGB(0, 229, 255),
                               juce::Colour::fromRGB(0, 229, 255),
                               juce::Colour::fromRGB(38, 43, 56));
    searchBtn.onClick = [this]() { performSearch(searchBox.getText()); };
    addAndMakeVisible(searchBtn);

    // 4. Paste URL Button
    pasteUrlBtn.setIcon(LucideIcons::IconType::Plus, 12.0f);
    pasteUrlBtn.setText("Pegar URL");
    pasteUrlBtn.setFontSize(11.0f);
    pasteUrlBtn.setCornerRadius(5.0f);
    pasteUrlBtn.setCustomColours(juce::Colour::fromRGB(22, 26, 36),
                                 juce::Colour::fromRGB(245, 158, 11),
                                 juce::Colour::fromRGB(245, 158, 11),
                                 juce::Colour::fromRGB(38, 43, 56));
    pasteUrlBtn.onClick = [this]() { handlePasteUrl(); };
    addAndMakeVisible(pasteUrlBtn);

    // 5. API Status Badge
    apiStatusBadge.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    apiStatusBadge.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(apiStatusBadge);
    updateApiStatus();

    // 6. Status text
    statusLabel.setFont(juce::FontOptions(11.0f));
    statusLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(142, 149, 165));
    addAndMakeVisible(statusLabel);

    // 7. Quick Search Genre Pills
    const juce::StringArray tags = { "Tech House Mix", "Afro House", "Ibiza Club 2026", "Nu Disco", "Melodic Techno", "Latin Tech Groove", "Festival EDM" };
    for (const auto& tag : tags)
    {
        auto btn = std::make_unique<DjButton>();
        btn->setText(tag);
        btn->setFontSize(10.0f);
        btn->setCornerRadius(12.0f);
        btn->setCustomColours(juce::Colour::fromRGB(20, 23, 32),
                              juce::Colour::fromRGB(160, 168, 185),
                              juce::Colour::fromRGB(0, 229, 255),
                              juce::Colour::fromRGB(35, 40, 52));
        btn->onClick = [this, tag]() {
            searchBox.setText(tag, juce::dontSendNotification);
            performSearch(tag);
        };
        addAndMakeVisible(*btn);
        quickTagButtons.push_back(std::move(btn));
    }

    // 8. Results Table
    table.setModel(this);
    table.setColour(juce::TableListBox::backgroundColourId, juce::Colour::fromRGB(12, 13, 18));
    table.getHeader().addColumn("#", 1, 36, 28, 45, juce::TableHeaderComponent::notSortable);
    table.getHeader().addColumn("VISTA PREVIA", 2, 96, 85, 110, juce::TableHeaderComponent::notSortable);
    table.getHeader().addColumn(juce::String::fromUTF8("TÍTULO DEL VIDEO"), 3, 360, 180, 600);
    table.getHeader().addColumn("CANAL / ARTISTA", 4, 180, 100, 300);
    table.getHeader().addColumn(juce::String::fromUTF8("DURACIÓN"), 5, 80, 60, 100);
    table.getHeader().addColumn("CARGAR", 6, 84, 75, 95, juce::TableHeaderComponent::notSortable);
    table.getHeader().setColour(juce::TableHeaderComponent::backgroundColourId, juce::Colour::fromRGB(16, 18, 24));
    table.getHeader().setColour(juce::TableHeaderComponent::textColourId, juce::Colour::fromRGB(142, 149, 165));
    table.setRowHeight(56);
    table.setMultipleSelectionEnabled(false);
    addAndMakeVisible(table);

    // Initial search for popular mixes
    performSearch("Tech House DJ Mix");
}

YouTubeLibraryView::~YouTubeLibraryView()
{
}

void YouTubeLibraryView::updateApiStatus()
{
    if (YouTubeService::hasApiKey())
    {
        apiStatusBadge.setText(juce::String::fromUTF8("● YouTube API v3: Conectada (Sin Anuncios)"), juce::dontSendNotification);
        apiStatusBadge.setColour(juce::Label::textColourId, juce::Colour::fromRGB(74, 222, 128));
    }
    else
    {
        apiStatusBadge.setText(juce::String::fromUTF8("● Modo Scraping Público Activo"), juce::dontSendNotification);
        apiStatusBadge.setColour(juce::Label::textColourId, juce::Colour::fromRGB(0, 229, 255));
    }
}

void YouTubeLibraryView::performSearch(const juce::String& query)
{
    auto q = query.trim();
    if (q.isEmpty())
        return;

    // Direct video ID or full URL check
    juce::String extracted = YouTubeService::extractVideoId(q);
    if (extracted.length() == 11 && !q.contains(" "))
    {
        statusLabel.setText("Cargando video directo...", juce::dontSendNotification);
        auto details = YouTubeService::getVideoDetails(extracted);
        searchResults.clear();
        searchResults.push_back(details);
        table.updateContent();
        table.repaint();
        statusLabel.setText("Video listo para cargar o arrastrar", juce::dontSendNotification);
        return;
    }

    isSearching = true;
    statusLabel.setText("Buscando en YouTube...", juce::dontSendNotification);
    repaint();

    YouTubeService::searchAsync(q, [this, q](const std::vector<YouTubeSearchResult>& results) {
        isSearching = false;
        searchResults = results;
        table.updateContent();
        table.repaint();

        if (searchResults.empty())
        {
            statusLabel.setText(juce::String::fromUTF8("No se encontraron resultados para '") + q + "'.", juce::dontSendNotification);
        }
        else
        {
            statusLabel.setText(juce::String::formatted("Mostrando %d videos encontrados en YouTube", (int)searchResults.size()),
                                juce::dontSendNotification);
        }
    });
}

void YouTubeLibraryView::handlePasteUrl()
{
    juce::String clip = juce::SystemClipboard::getTextFromClipboard().trim();
    if (clip.isEmpty())
    {
        statusLabel.setText("El portapapeles está vacío", juce::dontSendNotification);
        return;
    }

    juce::String videoId = YouTubeService::extractVideoId(clip);
    if (videoId.isNotEmpty())
    {
        searchBox.setText(clip, juce::dontSendNotification);
        statusLabel.setText("Obteniendo información del video...", juce::dontSendNotification);
        auto item = YouTubeService::getVideoDetails(videoId);
        searchResults.clear();
        searchResults.push_back(item);
        table.updateContent();
        table.repaint();
        statusLabel.setText(juce::String::fromUTF8("Video listo: ") + item.title, juce::dontSendNotification);
    }
    else
    {
        // Treat as normal search text
        searchBox.setText(clip, juce::dontSendNotification);
        performSearch(clip);
    }
}

int YouTubeLibraryView::getNumRows()
{
    return static_cast<int>(searchResults.size());
}

void YouTubeLibraryView::paintRowBackground(juce::Graphics& g, int rowNumber, int /*width*/, int /*height*/, bool rowIsSelected)
{
    if (rowIsSelected)
    {
        g.fillAll(juce::Colour::fromRGB(45, 50, 65));
    }
    else if (rowNumber % 2 == 1)
    {
        g.fillAll(juce::Colour::fromRGB(15, 17, 23));
    }
    else
    {
        g.fillAll(juce::Colour::fromRGB(12, 13, 18));
    }
}

void YouTubeLibraryView::paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool /*rowIsSelected*/)
{
    if (rowNumber < 0 || rowNumber >= (int)searchResults.size())
        return;

    const auto& item = searchResults[static_cast<size_t>(rowNumber)];

    switch (columnId)
    {
        case 1: // #
            g.setColour(juce::Colour::fromRGB(110, 118, 135));
            g.setFont(juce::FontOptions(11.0f));
            g.drawText(juce::String(rowNumber + 1), 0, 0, width, height, juce::Justification::centred, false);
            break;

        case 2: // Panoramic 16:9 Thumbnail (80x45)
        {
            float thumbW = 80.0f;
            float thumbH = 45.0f;
            float thumbX = ((float)width - thumbW) * 0.5f;
            float thumbY = ((float)height - thumbH) * 0.5f;
            auto thumbRect = juce::Rectangle<float>(thumbX, thumbY, thumbW, thumbH);

            auto img = ThumbnailCache::getInstance().getThumbnail(item.id, item.thumbnailUrl, [this](const juce::String&) {
                table.repaint();
            });

            // Base background
            g.setColour(juce::Colour::fromRGB(22, 25, 34));
            g.fillRoundedRectangle(thumbRect, 4.0f);

            if (img.isValid())
            {
                juce::Graphics::ScopedSaveState state(g);
                juce::Path clipPath;
                clipPath.addRoundedRectangle(thumbRect, 4.0f);
                g.reduceClipRegion(clipPath);

                g.drawImage(img, thumbRect, juce::RectanglePlacement::centred | juce::RectanglePlacement::fillDestination);
            }
            else
            {
                // Placeholder TV / Video icon
                auto iconArea = juce::Rectangle<float>(thumbRect.getCentreX() - 10.0f,
                                                       thumbRect.getCentreY() - 10.0f,
                                                       20.0f, 20.0f);
                LucideIcons::draw(g, LucideIcons::IconType::Tv, iconArea, juce::Colour::fromRGB(75, 82, 100), 1.5f);
            }

            // Sleek outer border
            g.setColour(juce::Colour::fromRGB(42, 47, 62));
            g.drawRoundedRectangle(thumbRect, 4.0f, 1.0f);

            // Duration badge overlaid in bottom-right corner of thumbnail
            if (item.durationText.isNotEmpty())
            {
                juce::Font badgeFont(juce::FontOptions(9.5f, juce::Font::bold));
                juce::GlyphArrangement ga;
                ga.addLineOfText(badgeFont, item.durationText, 0.0f, 0.0f);
                float badgeW = ga.getBoundingBox(0, -1, true).getWidth() + 8.0f;
                float badgeH = 14.0f;
                auto badgeRect = juce::Rectangle<float>(thumbRect.getRight() - badgeW - 3.0f,
                                                        thumbRect.getBottom() - badgeH - 3.0f,
                                                        badgeW, badgeH);

                g.setColour(juce::Colours::black.withAlpha(0.82f));
                g.fillRoundedRectangle(badgeRect, 2.5f);

                g.setColour(juce::Colours::white);
                g.setFont(badgeFont);
                g.drawText(item.durationText, badgeRect, juce::Justification::centred, false);
            }
            break;
        }

        case 3: // TÍTULO
            g.setColour(juce::Colours::white);
            g.setFont(juce::FontOptions(12.5f, juce::Font::bold));
            g.drawText(item.title, 10, 0, width - 20, height, juce::Justification::centredLeft, true);
            break;

        case 4: // CANAL
            g.setColour(juce::Colour::fromRGB(150, 158, 175));
            g.setFont(juce::FontOptions(11.5f, juce::Font::plain));
            g.drawText(item.artist, 6, 0, width - 12, height, juce::Justification::centredLeft, true);
            break;

        case 5: // DURACIÓN
            g.setColour(juce::Colour::fromRGB(0, 229, 255));
            g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
            g.drawText(item.durationText, 4, 0, width - 8, height, juce::Justification::centred, false);
            break;

        default:
            break;
    }
}

juce::Component* YouTubeLibraryView::refreshComponentForCell(int rowNumber, int columnId, bool /*isRowSelected*/, juce::Component* existingComponentToUpdate)
{
    if (columnId == 6) // CARGAR 1 / 2
    {
        auto* comp = dynamic_cast<YtLoadButtonsComponent*>(existingComponentToUpdate);
        if (comp == nullptr)
            comp = new YtLoadButtonsComponent();

        comp->setCallback([this, rowNumber](int deckIndex) {
            if (rowNumber >= 0 && rowNumber < (int)searchResults.size() && onLoadTrack)
            {
                onLoadTrack(deckIndex, searchResults[static_cast<size_t>(rowNumber)]);
            }
        });
        return comp;
    }
    delete existingComponentToUpdate;
    return nullptr;
}

void YouTubeLibraryView::cellDoubleClicked(int rowNumber, int /*columnId*/, const juce::MouseEvent&)
{
    if (rowNumber >= 0 && rowNumber < (int)searchResults.size() && onLoadTrack)
    {
        onLoadTrack(0, searchResults[static_cast<size_t>(rowNumber)]);
    }
}

juce::var YouTubeLibraryView::getDragSourceDescription(const juce::SparseSet<int>& currentlySelectedRows)
{
    if (currentlySelectedRows.isEmpty())
        return {};

    int firstRow = currentlySelectedRows[0];
    if (firstRow >= 0 && firstRow < (int)searchResults.size())
    {
        return juce::var("yt:" + searchResults[static_cast<size_t>(firstRow)].id);
    }
    return {};
}

void YouTubeLibraryView::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colour::fromRGB(12, 13, 18));
    g.fillRoundedRectangle(bounds, 6.0f);
}

void YouTubeLibraryView::resized()
{
    auto area = getLocalBounds().reduced(10, 8);

    // Row 1: Header + API status badge (28px)
    auto topRow = area.removeFromTop(28);
    headerTitle.setBounds(topRow.removeFromLeft(240));
    apiStatusBadge.setBounds(topRow);

    area.removeFromTop(6);

    // Row 2: Search input + Search button + Paste URL button (32px)
    auto searchRow = area.removeFromTop(32);
    searchBox.setBounds(searchRow.removeFromLeft(searchRow.getWidth() - 190).reduced(0, 1));
    searchRow.removeFromLeft(6);
    searchBtn.setBounds(searchRow.removeFromLeft(84).reduced(0, 1));
    searchRow.removeFromLeft(6);
    pasteUrlBtn.setBounds(searchRow.reduced(0, 1));

    area.removeFromTop(6);

    // Row 3: Quick Genre Tag Buttons (24px)
    auto tagRow = area.removeFromTop(24);
    int tagSpacing = 6;
    for (size_t i = 0; i < quickTagButtons.size(); ++i)
    {
        auto& btn = quickTagButtons[i];
        int w = 110;
        btn->setBounds(tagRow.removeFromLeft(w));
        tagRow.removeFromLeft(tagSpacing);
    }

    area.removeFromTop(6);

    // Row 4: Status text (18px)
    statusLabel.setBounds(area.removeFromTop(18));
    area.removeFromTop(4);

    // Table fills remaining space
    table.setBounds(area);
}

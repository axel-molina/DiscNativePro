#include "YouTubeLibraryView.h"
#include "../services/YouTubeAudioDownloader.h"

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

    class YtDownloadButtonComponent : public juce::Component
    {
    public:
        YtDownloadButtonComponent()
        {
            btn.setFontSize(10.5f);
            btn.setCornerRadius(4.0f);
            btn.onClick = [this]() {
                if (videoId.isEmpty()) return;
                if (YouTubeAudioDownloader::getInstance().getCachedAudioFile(videoId).existsAsFile())
                    return;
                if (YouTubeAudioDownloader::getInstance().isDownloading(videoId))
                    return;

                updateState(0.05f, LanguageManager::trUtf8("Descargando...", "Downloading..."), true);
                YouTubeAudioDownloader::getInstance().downloadAudioAsync(
                    videoId,
                    trackTitle,
                    trackArtist,
                    [safe = juce::Component::SafePointer<YtDownloadButtonComponent>(this), id = videoId](float progress, const juce::String& /*status*/) {
                        if (safe == nullptr || safe->videoId != id) return;
                        safe->updateState(progress, juce::String((int)(progress * 100)) + "%", true);
                    },
                    [safe = juce::Component::SafePointer<YtDownloadButtonComponent>(this), id = videoId](const juce::File& file, bool success, const juce::String& /*error*/) {
                        if (safe == nullptr || safe->videoId != id) return;
                        if (success && file.existsAsFile())
                            safe->updateState(1.0f, LanguageManager::trUtf8("Listo", "Ready"), false);
                        else
                            safe->updateState(0.0f, LanguageManager::trUtf8("Error", "Error"), false);
                    }
                );
            };
            addAndMakeVisible(btn);
        }

        void setTrack(const YouTubeSearchResult& item)
        {
            videoId = item.id;
            trackTitle = item.title;
            trackArtist = item.artist;
            bool isCached = YouTubeAudioDownloader::getInstance().getCachedAudioFile(videoId).existsAsFile();
            bool isDown = YouTubeAudioDownloader::getInstance().isDownloading(videoId);

            if (isCached)
                updateState(1.0f, LanguageManager::trUtf8("Listo", "Ready"), false);
            else if (isDown)
                updateState(0.5f, LanguageManager::trUtf8("Descargando...", "Downloading..."), true);
            else
                updateState(0.0f, LanguageManager::trUtf8("Descargar", "Download"), false);
        }

        void updateState(float progress, const juce::String& text, bool downloading)
        {
            juce::ignoreUnused(progress);
            bool isCached = YouTubeAudioDownloader::getInstance().getCachedAudioFile(videoId).existsAsFile();

            if (isCached)
            {
                btn.setIcon(LucideIcons::IconType::Check, 11.0f);
                btn.setText(LanguageManager::trUtf8("Listo", "Ready"));
                btn.setCustomColours(juce::Colour::fromRGB(16, 185, 129).withAlpha(0.2f),
                                     juce::Colour::fromRGB(52, 211, 153),
                                     juce::Colour::fromRGB(52, 211, 153),
                                     juce::Colour::fromRGB(16, 185, 129).withAlpha(0.35f));
            }
            else if (downloading)
            {
                btn.setIcon(LucideIcons::IconType::Download, 11.0f);
                btn.setText(text);
                btn.setCustomColours(juce::Colour::fromRGB(245, 158, 11).withAlpha(0.2f),
                                     juce::Colour::fromRGB(251, 191, 36),
                                     juce::Colour::fromRGB(251, 191, 36),
                                     juce::Colour::fromRGB(245, 158, 11).withAlpha(0.35f));
            }
            else
            {
                btn.setIcon(LucideIcons::IconType::Download, 11.0f);
                btn.setText(LanguageManager::trUtf8("Descargar", "Download"));
                btn.setCustomColours(juce::Colour::fromRGB(20, 24, 34),
                                     juce::Colour::fromRGB(0, 210, 255),
                                     juce::Colour::fromRGB(0, 210, 255),
                                     juce::Colour::fromRGB(38, 43, 58));
            }
        }

        void resized() override
        {
            auto area = getLocalBounds();
            int btnH = 26;
            int y = (area.getHeight() - btnH) / 2;
            btn.setBounds(area.reduced(4, y));
        }

    private:
        DjButton btn;
        juce::String videoId;
        juce::String trackTitle;
        juce::String trackArtist;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(YtDownloadButtonComponent)
    };
}

YouTubeLibraryView::YouTubeLibraryView()
{
    // 1. Header Title
    headerTitle.setText("YouTube DJ Streaming", juce::dontSendNotification);
    headerTitle.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    headerTitle.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(headerTitle);

    // 1b. Playback Mode Toggle: Audio (Native) vs Video (Web)
    audioModeBtn.setIcon(LucideIcons::IconType::Music, 11.0f);
    audioModeBtn.setFontSize(11.0f);
    audioModeBtn.setCornerRadius(4.0f);
    audioModeBtn.onClick = [this]() {
        YouTubeService::setAudioPlaybackMode(true);
        updateModeButtons();
    };
    addAndMakeVisible(audioModeBtn);

    videoModeBtn.setIcon(LucideIcons::IconType::Video, 11.0f);
    videoModeBtn.setFontSize(11.0f);
    videoModeBtn.setCornerRadius(4.0f);
    videoModeBtn.onClick = [this]() {
        YouTubeService::setAudioPlaybackMode(false);
        updateModeButtons();
    };
    addAndMakeVisible(videoModeBtn);
    updateModeButtons();

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

    // 7. Results Table
    table.setModel(this);
    table.setColour(juce::TableListBox::backgroundColourId, juce::Colour::fromRGB(12, 13, 18));
    table.getHeader().addColumn("#", 1, 36, 28, 45, juce::TableHeaderComponent::notSortable);
    table.getHeader().addColumn("VISTA PREVIA", 2, 96, 85, 110, juce::TableHeaderComponent::notSortable);
    table.getHeader().addColumn(juce::String::fromUTF8("TÍTULO DEL VIDEO"), 3, 300, 180, 600);
    table.getHeader().addColumn("CANAL / ARTISTA", 4, 150, 100, 300);
    // insertIndex 5: la fecha va entre CANAL y DURACIÓN, pegada al resto de
    // metadatos del vídeo. notSortable porque sortOrderChanged no está
    // implementado en esta vista (las flechas de las otras columnas son mudas).
    table.getHeader().addColumn("FECHA", 8, 105, 80, 140, juce::TableHeaderComponent::notSortable, 5);
    table.getHeader().addColumn(juce::String::fromUTF8("DURACIÓN"), 5, 75, 55, 95);
    table.getHeader().addColumn("CARGAR", 6, 80, 70, 95, juce::TableHeaderComponent::notSortable);
    table.getHeader().addColumn(juce::String::fromUTF8("DESCARGAR"), 7, 95, 80, 115, juce::TableHeaderComponent::notSortable);
    table.getHeader().setColour(juce::TableHeaderComponent::backgroundColourId, juce::Colour::fromRGB(16, 18, 24));
    table.getHeader().setColour(juce::TableHeaderComponent::textColourId, juce::Colour::fromRGB(142, 149, 165));
    table.setRowHeight(56);
    table.setMultipleSelectionEnabled(false);
    addAndMakeVisible(table);

    LanguageManager::getInstance().addListener(this);
    refreshLanguage();
}

YouTubeLibraryView::~YouTubeLibraryView()
{
    LanguageManager::getInstance().removeListener(this);
}

void YouTubeLibraryView::languageChanged()
{
    refreshLanguage();
}

void YouTubeLibraryView::updateModeButtons()
{
    bool audioMode = YouTubeService::isAudioPlaybackMode();
    if (audioMode)
    {
        audioModeBtn.setCustomColours(juce::Colour::fromRGB(0, 180, 216).withAlpha(0.25f),
                                      juce::Colour::fromRGB(0, 229, 255),
                                      juce::Colour::fromRGB(0, 229, 255),
                                      juce::Colour::fromRGB(0, 180, 216).withAlpha(0.45f));
        videoModeBtn.setCustomColours(juce::Colour::fromRGB(20, 23, 30),
                                      juce::Colour::fromRGB(142, 149, 165),
                                      juce::Colour::fromRGB(142, 149, 165),
                                      juce::Colour::fromRGB(38, 43, 56));
    }
    else
    {
        audioModeBtn.setCustomColours(juce::Colour::fromRGB(20, 23, 30),
                                      juce::Colour::fromRGB(142, 149, 165),
                                      juce::Colour::fromRGB(142, 149, 165),
                                      juce::Colour::fromRGB(38, 43, 56));
        videoModeBtn.setCustomColours(juce::Colour::fromRGB(239, 68, 68).withAlpha(0.22f),
                                      juce::Colour::fromRGB(248, 113, 113),
                                      juce::Colour::fromRGB(248, 113, 113),
                                      juce::Colour::fromRGB(239, 68, 68).withAlpha(0.45f));
    }
}

void YouTubeLibraryView::refreshLanguage()
{
    searchBtn.setText(LanguageManager::trUtf8("Buscar", "Search"));
    pasteUrlBtn.setText(LanguageManager::trUtf8("Pegar URL", "Paste URL"));
    searchBox.setTextToShowWhenEmpty(LanguageManager::trUtf8("Buscar canciones, mixes o pegar enlace de YouTube...",
                                                             "Search songs, mixes or paste YouTube link..."),
                                     juce::Colour::fromRGB(110, 118, 135));

    audioModeBtn.setText(LanguageManager::trUtf8("Audio", "Audio"));
    videoModeBtn.setText(LanguageManager::trUtf8("Video", "Video"));

    table.getHeader().setColumnName(2, LanguageManager::trUtf8("VISTA PREVIA", "PREVIEW"));
    table.getHeader().setColumnName(3, LanguageManager::trUtf8("TÍTULO DEL VIDEO", "VIDEO TITLE"));
    table.getHeader().setColumnName(4, LanguageManager::trUtf8("CANAL / ARTISTA", "CHANNEL / ARTIST"));
    table.getHeader().setColumnName(8, LanguageManager::trUtf8("FECHA", "DATE"));
    table.getHeader().setColumnName(5, LanguageManager::trUtf8("DURACIÓN", "DURATION"));
    table.getHeader().setColumnName(6, LanguageManager::trUtf8("CARGAR", "LOAD"));
    table.getHeader().setColumnName(7, LanguageManager::trUtf8("DESCARGAR", "DOWNLOAD"));

    updateApiStatus();
    updateModeButtons();
    table.repaint();
    repaint();
}

void YouTubeLibraryView::updateApiStatus()
{
    if (YouTubeService::hasApiKey())
    {
        apiStatusBadge.setText(LanguageManager::trUtf8("● YouTube API v3: Conectada (Sin Anuncios)", "● YouTube API v3: Connected (No Ads)"), juce::dontSendNotification);
        apiStatusBadge.setColour(juce::Label::textColourId, juce::Colour::fromRGB(74, 222, 128));
    }
    else
    {
        apiStatusBadge.setText(LanguageManager::trUtf8("● Modo Scraping Público Activo", "● Public Scraping Active"), juce::dontSendNotification);
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
        statusLabel.setText(LanguageManager::trUtf8("Cargando video directo...", "Loading direct video..."), juce::dontSendNotification);
        auto details = YouTubeService::getVideoDetails(extracted);
        searchResults.clear();
        searchResults.push_back(details);
        table.updateContent();
        table.repaint();
        statusLabel.setText(LanguageManager::trUtf8("Video listo para cargar o arrastrar", "Video ready to load or drag"), juce::dontSendNotification);
        return;
    }

    isSearching = true;
    statusLabel.setText(LanguageManager::trUtf8("Buscando en YouTube...", "Searching YouTube..."), juce::dontSendNotification);
    repaint();

    YouTubeService::searchAsync(q, [this, q](const std::vector<YouTubeSearchResult>& results) {
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
                                + LanguageManager::trUtf8(" videos encontrados en YouTube", " videos found on YouTube"),
                                juce::dontSendNotification);
        }
    });
}

void YouTubeLibraryView::handlePasteUrl()
{
    juce::String clip = juce::SystemClipboard::getTextFromClipboard().trim();
    if (clip.isEmpty())
    {
        statusLabel.setText(LanguageManager::trUtf8("El portapapeles está vacío", "Clipboard is empty"), juce::dontSendNotification);
        return;
    }

    juce::String videoId = YouTubeService::extractVideoId(clip);
    if (videoId.isNotEmpty())
    {
        searchBox.setText(clip, juce::dontSendNotification);
        statusLabel.setText(LanguageManager::trUtf8("Obteniendo información del video...", "Getting video information..."), juce::dontSendNotification);
        auto item = YouTubeService::getVideoDetails(videoId);
        searchResults.clear();
        searchResults.push_back(item);
        table.updateContent();
        table.repaint();
        statusLabel.setText(LanguageManager::trUtf8("Video listo: ", "Video ready: ") + item.title, juce::dontSendNotification);
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

        case 8: // FECHA DE PUBLICACIÓN (texto relativo de YouTube)
            if (item.publishedText.isEmpty())
            {
                g.setColour(juce::Colour::fromRGB(80, 86, 100));
                g.setFont(juce::FontOptions(11.5f, juce::Font::plain));
                g.drawText(juce::String::fromUTF8("—"), 4, 0, width - 8, height,
                           juce::Justification::centred, false);
            }
            else
            {
                g.setColour(juce::Colour::fromRGB(150, 158, 175));
                g.setFont(juce::FontOptions(11.5f, juce::Font::plain));
                g.drawText(item.publishedText, 4, 0, width - 8, height,
                           juce::Justification::centred, true);
            }
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
    else if (columnId == 7) // DESCARGAR
    {
        auto* comp = dynamic_cast<YtDownloadButtonComponent*>(existingComponentToUpdate);
        if (comp == nullptr)
            comp = new YtDownloadButtonComponent();

        if (rowNumber >= 0 && rowNumber < (int)searchResults.size())
        {
            comp->setTrack(searchResults[static_cast<size_t>(rowNumber)]);
        }
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
        const auto& it = searchResults[static_cast<size_t>(firstRow)];
        return juce::var("yt:" + it.id + "\t" + it.title + "\t" + it.artist + "\t"
                         + it.thumbnailUrl + "\t" + juce::String(it.durationSeconds) + "\t" + it.durationText);
    }
    return {};
}

void YouTubeLibraryView::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colour::fromRGB(12, 13, 18));
    g.fillRoundedRectangle(bounds, 6.0f);

    // YouTube brand icon in top header row
    auto iconRect = juce::Rectangle<float>(10.0f, 13.0f, 20.0f, 18.0f);
    LucideIcons::draw(g, LucideIcons::IconType::YouTube, iconRect, juce::Colour::fromRGB(239, 68, 68));
}

void YouTubeLibraryView::resized()
{
    auto area = getLocalBounds().reduced(10, 8);

    // Row 1: Header + Mode Switch + API status badge (28px)
    auto topRow = area.removeFromTop(28);
    topRow.removeFromLeft(26); // space for YouTube icon
    headerTitle.setBounds(topRow.removeFromLeft(180));
    topRow.removeFromLeft(8);
    audioModeBtn.setBounds(topRow.removeFromLeft(74).reduced(0, 2));
    topRow.removeFromLeft(4);
    videoModeBtn.setBounds(topRow.removeFromLeft(74).reduced(0, 2));
    topRow.removeFromLeft(10);
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

    // Row 3: Status text (18px)
    statusLabel.setBounds(area.removeFromTop(18));
    area.removeFromTop(4);

    // Table fills remaining space
    table.setBounds(area);
}

#include "AIMatchView.h"
#include "CoverArtGenerator.h"
#include "../services/YouTubeService.h"

class RecommendationCardComponent : public juce::Component
{
public:
    RecommendationCardComponent(const AIRecommendation& rec, int targetDeck,
                                std::function<void(const juce::File&, int)> loadLocalCb,
                                std::function<void(const juce::String&, int)> searchYtCb)
        : recommendation(rec),
          targetDeckIndex(targetDeck),
          onLoadLocal(std::move(loadLocalCb)),
          onSearchYt(std::move(searchYtCb))
    {
        if (rec.isLocal)
        {
            actionButton.setText(LanguageManager::trUtf8("Cargar Deck ", "Load Deck ") + juce::String(targetDeck + 1));
            actionButton.setIcon(LucideIcons::IconType::Music, 11.0f);
            actionButton.setCustomColours(juce::Colour::fromRGB(16, 185, 129),
                                          juce::Colours::white,
                                          juce::Colours::white,
                                          juce::Colour::fromRGB(5, 150, 105));
            actionButton.onClick = [this]() {
                if (onLoadLocal && recommendation.localFile.existsAsFile())
                    onLoadLocal(recommendation.localFile, targetDeckIndex);
            };
        }
        else
        {
            actionButton.setText(LanguageManager::trUtf8("Buscar en YouTube", "Search YouTube"));
            actionButton.setIcon(LucideIcons::IconType::Tv, 11.0f);
            actionButton.setCustomColours(juce::Colour::fromRGB(220, 38, 38),
                                          juce::Colours::white,
                                          juce::Colours::white,
                                          juce::Colour::fromRGB(185, 28, 28));
            actionButton.onClick = [this]() {
                if (onSearchYt)
                    onSearchYt(recommendation.artist + " " + recommendation.title, targetDeckIndex);
            };
        }

        actionButton.setFontSize(11.0f);
        actionButton.setCornerRadius(0.0f);
        addAndMakeVisible(actionButton);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(4.0f, 3.0f);

        // Dark card background
        g.setColour(juce::Colour::fromRGB(18, 20, 26));
        g.fillRoundedRectangle(bounds, 0.0f);

        // Border
        g.setColour(recommendation.isLocal ? juce::Colour::fromRGB(16, 185, 129).withAlpha(0.35f)
                                           : juce::Colour::fromRGB(38, 43, 56));
        g.drawRoundedRectangle(bounds, 0.0f, 1.0f);

        auto contentArea = bounds.reduced(10.0f, 8.0f);

        // 1. Left Icon Thumbnail (36x36)
        auto iconRect = contentArea.removeFromLeft(36.0f).withHeight(36.0f);
        g.setColour(juce::Colour::fromRGB(28, 32, 44));
        g.fillRoundedRectangle(iconRect, 0.0f);

        LucideIcons::draw(g, recommendation.isLocal ? LucideIcons::IconType::Disc : LucideIcons::IconType::Sparkles,
                          iconRect.reduced(8.0f),
                          recommendation.isLocal ? juce::Colour::fromRGB(16, 185, 129)
                                                 : juce::Colour::fromRGB(147, 51, 234),
                          1.8f);

        contentArea.removeFromLeft(12.0f);

        // Leave room for right action button
        contentArea.removeFromRight(150.0f);

        // 2. Track Title & Artist
        auto textRect = contentArea;
        auto titleRect = textRect.removeFromTop(18.0f);

        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        juce::String fullTitle = recommendation.title;
        if (recommendation.artist.isNotEmpty())
            fullTitle += juce::String::fromUTF8(" — ") + recommendation.artist;
        g.drawText(fullTitle, titleRect, juce::Justification::centredLeft, true);

        // 3. Badges Row (Genre, BPM, Key, Source)
        auto badgesRect = textRect.removeFromTop(16.0f);
        float bx = badgesRect.getX();

        auto drawBadge = [&](const juce::String& text, juce::Colour bg, juce::Colour fg, float w) {
            juce::Rectangle<float> r(bx, badgesRect.getY(), w, 15.0f);
            g.setColour(bg);
            g.fillRoundedRectangle(r, 0.0f);
            g.setColour(fg);
            g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
            g.drawText(text, r, juce::Justification::centred, false);
            bx += w + 6.0f;
        };

        if (recommendation.bpm > 0.0)
        {
            drawBadge(juce::String(recommendation.bpm, 0) + " BPM",
                      juce::Colour::fromRGB(30, 41, 59),
                      juce::Colour::fromRGB(148, 163, 184), 58.0f);
        }

        if (recommendation.key.isNotEmpty())
        {
            drawBadge(recommendation.key,
                      juce::Colour::fromRGB(30, 58, 138).withAlpha(0.4f),
                      juce::Colour::fromRGB(96, 165, 250), 38.0f);
        }

        if (recommendation.genre.isNotEmpty())
        {
            drawBadge(recommendation.genre,
                      juce::Colour::fromRGB(55, 48, 163).withAlpha(0.35f),
                      juce::Colour::fromRGB(192, 132, 252), 75.0f);
        }

        if (recommendation.isLocal)
        {
            drawBadge("LOCAL",
                      juce::Colour::fromRGB(6, 78, 59).withAlpha(0.4f),
                      juce::Colour::fromRGB(52, 211, 153), 48.0f);
        }

        // Señal de afinidad: lo que más importa (género/crowd) va primero visualmente
        auto mt = recommendation.matchType.toLowerCase().trim();
        if (mt == "crowd")
        {
            drawBadge("CROWD",
                      juce::Colour::fromRGB(120, 53, 15).withAlpha(0.45f),
                      juce::Colour::fromRGB(251, 191, 36), 52.0f);
        }
        else if (mt == "harmonic")
        {
            drawBadge(LanguageManager::trUtf8("ARMONÍA", "HARMONY"),
                      juce::Colour::fromRGB(30, 58, 138).withAlpha(0.4f),
                      juce::Colour::fromRGB(96, 165, 250), 62.0f);
        }
        else
        {
            drawBadge(LanguageManager::trUtf8("GÉNERO", "GENRE"),
                      juce::Colour::fromRGB(55, 48, 163).withAlpha(0.35f),
                      juce::Colour::fromRGB(192, 132, 252), 56.0f);
        }

        // 4. Reason Subtitle
        if (recommendation.reason.isNotEmpty())
        {
            auto reasonRect = textRect;
            g.setColour(juce::Colour::fromRGB(142, 149, 165));
            g.setFont(juce::FontOptions(10.5f, juce::Font::italic));
            g.drawText(recommendation.reason,
                       reasonRect, juce::Justification::centredLeft, true);
        }
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(12, 14);
        actionButton.setBounds(bounds.removeFromRight(140).withHeight(30).withY(bounds.getCentreY() - 15));
    }

private:
    AIRecommendation recommendation;
    int targetDeckIndex;
    std::function<void(const juce::File&, int)> onLoadLocal;
    std::function<void(const juce::String&, int)> onSearchYt;
    DjButton actionButton;
};

class AIMatchView::CardListContainer : public juce::Component
{
public:
    CardListContainer() = default;

    void clear()
    {
        cards.clear();
        resized();
    }

    void updateWidth(int newWidth)
    {
        setSize(newWidth, getHeight());
        resized();
    }

    void addCard(std::unique_ptr<RecommendationCardComponent> card)
    {
        addAndMakeVisible(*card);
        cards.push_back(std::move(card));
        resized();
    }

    void resized() override
    {
        int y = 0;
        int cardHeight = 68;
        int w = getWidth();

        for (auto& c : cards)
        {
            c->setBounds(0, y, w, cardHeight);
            y += cardHeight + 4;
        }

        setSize(w, std::max(y, 10));
    }

private:
    std::vector<std::unique_ptr<RecommendationCardComponent>> cards;
};

AIMatchView::AIMatchView()
    : listContainer(std::make_unique<CardListContainer>())
{
    // 1. Header Buttons
    refreshButton.setIcon(LucideIcons::IconType::Sparkles, 12.0f);
    refreshButton.setFontSize(11.0f);
    refreshButton.setCornerRadius(0.0f);
    refreshButton.setCustomColours(juce::Colour::fromRGB(88, 28, 135),
                                   juce::Colours::white,
                                   juce::Colours::white,
                                   juce::Colour::fromRGB(126, 34, 206));
    refreshButton.onClick = [this]() { refreshRecommendations(); };
    addAndMakeVisible(refreshButton);

    settingsShortcutButton.setIcon(LucideIcons::IconType::Sliders, 12.0f);
    settingsShortcutButton.setFontSize(11.0f);
    settingsShortcutButton.setCornerRadius(0.0f);
    settingsShortcutButton.setCustomColours(juce::Colour::fromRGB(24, 27, 36),
                                            juce::Colour::fromRGB(142, 149, 165),
                                            juce::Colours::white,
                                            juce::Colour::fromRGB(45, 52, 70));
    settingsShortcutButton.onClick = [this]() {
        if (onOpenSettings) onOpenSettings();
    };
    addAndMakeVisible(settingsShortcutButton);

    // 2. Viewport for scrollable cards
    viewport.setViewedComponent(listContainer.get(), false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);

    LanguageManager::getInstance().addListener(this);
    refreshLanguage();
}

AIMatchView::~AIMatchView()
{
    LanguageManager::getInstance().removeListener(this);
}

void AIMatchView::languageChanged()
{
    refreshLanguage();
}

void AIMatchView::refreshLanguage()
{
    refreshButton.setText(LanguageManager::trUtf8("Generar Recomendaciones", "Generate Recommendations"));
    settingsShortcutButton.setText(LanguageManager::trUtf8("Configurar IA", "Configure AI"));
    updateCardList();
    repaint();
}

void AIMatchView::updateActiveTrack(const CurrentTrackContext& ctx)
{
    currentContext = ctx;

    // Precarga de señal de audiencia al cambiar de video: cuando el usuario
    // pulse Generar, el contexto suele estar ya en caché (updateActiveTrack late ~0.5s)
    if (ctx.isVideo && ctx.videoId.isNotEmpty() && ctx.videoId != lastPrefetchedVideoId)
    {
        lastPrefetchedVideoId = ctx.videoId;
        YouTubeService::prefetchAudienceSignals(ctx.videoId);
    }

    repaint();
}

void AIMatchView::setLocalLibraryFiles(const std::vector<juce::File>& files)
{
    localFiles = files;
}

void AIMatchView::refreshRecommendations()
{
    if (!AIService::isEnabled() || !AIService::isConfigured())
    {
        errorMessage = LanguageManager::trUtf8("La Inteligencia Artificial no está configurada o activada. Configura tu API Key en Ajustes.",
                                               "AI is not configured or enabled. Configure your API Key in Settings.");
        resultsUncertain = false;
        listContainer->clear();
        repaint();
        return;
    }

    if (currentContext.title.isEmpty() && currentContext.artist.isEmpty())
    {
        errorMessage = LanguageManager::trUtf8("Carga y reproduce una pista primero en cualquier bandeja para generar recomendaciones.",
                                               "Load and play a track first on any deck to generate recommendations.");
        resultsUncertain = false;
        listContainer->clear();
        repaint();
        return;
    }

    isLoading = true;
    errorMessage = {};
    statusMessage = LanguageManager::trUtf8("Analizando pista y generando mezclas armónicas...",
                                           "Analyzing track and generating harmonic transitions...");
    listContainer->clear();
    repaint();

    AIService::requestRecommendationsAsync(currentContext, [this](const std::vector<AIRecommendation>& recs, const juce::String& err) {
        isLoading = false;
        if (err.isNotEmpty() && recs.empty())
        {
            errorMessage = err;
            resultsUncertain = false;
            repaint();
            return;
        }

        recommendations = recs;
        // Solo marca LOCAL como badge; el orden del modelo manda (género/crowd primero)
        AIService::resolveLocalMatches(recommendations, localFiles);
        resultsUncertain = !recommendations.empty();
        for (const auto& r : recommendations)
        {
            if (r.known)
            {
                resultsUncertain = false;
                break;
            }
        }
        updateCardList();
        repaint();
    });
}

void AIMatchView::updateCardList()
{
    listContainer->clear();

    // The target deck is the opposite deck (0 if currently 1, 1 if currently 0)
    int targetDeck = (currentContext.activeDeck == 0) ? 1 : 0;

    for (const auto& r : recommendations)
    {
        auto card = std::make_unique<RecommendationCardComponent>(r, targetDeck, onLoadLocalTrack, onSearchYouTube);
        listContainer->addCard(std::move(card));
    }

    listContainer->resized();
}

void AIMatchView::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Background #0c0d10
    g.fillAll(juce::Colour::fromRGB(12, 13, 16));

    // Top Header bar background (36px)
    auto headerRect = bounds.removeFromTop(36.0f);
    g.setColour(juce::Colour::fromRGB(16, 18, 23));
    g.fillRect(headerRect);
    g.setColour(juce::Colour::fromRGB(28, 31, 40));
    g.drawHorizontalLine(35, 0.0f, (float)getWidth());

    // Context label in header
    auto infoArea = headerRect.reduced(10.0f, 6.0f);
    infoArea.removeFromRight(280.0f); // Space for buttons

    juce::String contextText = LanguageManager::trUtf8("Pista Activa: ", "Active Track: ");
    if (currentContext.title.isNotEmpty())
    {
        contextText << "\"" << currentContext.title << "\"";
        if (currentContext.artist.isNotEmpty())
            contextText << " - " + currentContext.artist;
        if (currentContext.bpm > 0.0)
            contextText << " (" << juce::String(currentContext.bpm, 1) << " BPM";
        if (currentContext.key.isNotEmpty() && currentContext.key != "--")
            contextText << " / " << currentContext.key;
        if (currentContext.bpm > 0.0)
            contextText << ")";
    }
    else
    {
        contextText << LanguageManager::trUtf8("Ninguna (Reproduce en Deck 1 o Deck 2)", "None (Play on Deck 1 or Deck 2)");
    }

    if (resultsUncertain && !recommendations.empty())
        contextText << LanguageManager::trUtf8("  •  track poco conocido (por género)", "  •  less known track (by genre)");

    g.setColour(juce::Colour::fromRGB(192, 132, 252));
    LucideIcons::draw(g, LucideIcons::IconType::Sparkles,
                      juce::Rectangle<float>(infoArea.getX(), infoArea.getY() + 3.0f, 15.0f, 15.0f),
                      juce::Colour::fromRGB(192, 132, 252), 1.6f);

    infoArea.removeFromLeft(22.0f);
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.drawText(contextText, infoArea, juce::Justification::centredLeft, true);

    // Empty / Status / Error states
    if (isLoading)
    {
        auto center = getLocalBounds().reduced(20).withTrimmedTop(36);
        g.setColour(juce::Colour::fromRGB(192, 132, 252));
        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        g.drawText(statusMessage, center, juce::Justification::centred, false);
    }
    else if (errorMessage.isNotEmpty())
    {
        auto center = getLocalBounds().reduced(20).withTrimmedTop(36);
        g.setColour(juce::Colour::fromRGB(239, 68, 68));
        g.setFont(juce::FontOptions(12.5f, juce::Font::plain));
        g.drawText(errorMessage, center, juce::Justification::centred, true);
    }
    else if (recommendations.empty())
    {
        auto center = getLocalBounds().reduced(20).withTrimmedTop(36);
        g.setColour(juce::Colour::fromRGB(142, 149, 165));
        g.setFont(juce::FontOptions(12.5f, juce::Font::plain));
        g.drawText(LanguageManager::trUtf8("Haz clic en 'Generar Recomendaciones' para obtener mezclas sugeridas por IA.",
                                           "Click 'Generate Recommendations' to get AI suggested transitions."),
                   center, juce::Justification::centred, false);
    }
}

void AIMatchView::resized()
{
    auto area = getLocalBounds();
    auto header = area.removeFromTop(36).reduced(8, 5);

    settingsShortcutButton.setBounds(header.removeFromRight(100));
    header.removeFromRight(6);
    refreshButton.setBounds(header.removeFromRight(175));

    viewport.setBounds(area.reduced(6, 4));
    if (listContainer)
    {
        listContainer->updateWidth(viewport.getWidth() - 14);
    }
}

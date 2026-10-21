#pragma once

#include <juce_core/juce_core.h>
#include <functional>
#include <vector>

enum class AIProvider
{
    Gemini,
    OpenAI,
    Claude,
    DeepSeek,
    OpenRouter
};

struct AIRecommendation
{
    juce::String title;
    juce::String artist;
    double bpm { 0.0 };
    juce::String key;
    juce::String genre;
    juce::String reason;

    // Affinity signal: "crowd" (el público la pone junto a esta) > "genre" > "harmonic"
    juce::String matchType { "genre" };
    int energy { 0 }; // 1-10, 0 = desconocido
    bool known { true }; // false = el modelo no reconoce la pista -> fallback por género

    // Hybrid library resolution (badge only, never reorders)
    bool isLocal { false };
    juce::File localFile;
};

struct CandidateTrackInfo
{
    int index { 0 };
    juce::String title;
    juce::String artist;
    double bpm { 0.0 };
    juce::String key;
    juce::String folder;
};

struct CurrentTrackContext
{
    juce::String title;
    juce::String artist;
    double bpm { 0.0 };
    juce::String key;
    bool isVideo { false };
    int activeDeck { 1 };
    juce::String videoId; // YouTube ID cuando isVideo (para señal de audiencia)
    juce::StringArray audienceAlsoPlays; // "la audiencia también ve/mezcla" (scrape Up Next)
    juce::StringArray videoTags; // tags de videos.list (solo con API key oficial)
    std::vector<CandidateTrackInfo> libraryCandidates; // Pistas locales disponibles en la biblioteca
};

class AIService
{
public:
    static juce::File getSettingsFile();

    // Provider & Configuration
    static AIProvider getProvider();
    static void setProvider(AIProvider provider);
    static juce::String getProviderName(AIProvider provider);

    static juce::String getApiKey(AIProvider provider);
    static void setApiKey(AIProvider provider, const juce::String& key);

    // Migración única: mueve todas las keys legacy de settings.json al almacén seguro.
    static void migrateLegacySecrets();

    // Model per provider (user-configurable, persisted; never hardcoded at call sites)
    static juce::String getDefaultModel(AIProvider provider);
    static juce::StringArray getAvailableModels(AIProvider provider);
    static juce::String getModel(AIProvider provider);
    static void setModel(AIProvider provider, const juce::String& model);

    static bool isEnabled();
    static void setEnabled(bool enabled);

    static bool isConfigured();

    // Asynchronous Recommendation Generation
    // Returns results on the JUCE Message Thread
    static void requestRecommendationsAsync(
        const CurrentTrackContext& context,
        std::function<void(const std::vector<AIRecommendation>& recommendations, const juce::String& errorMsg)> onComplete);

    // Helper to resolve whether a recommendation matches any track in local library
    static void resolveLocalMatches(std::vector<AIRecommendation>& recs,
                                    const std::vector<juce::File>& localLibraryFiles);

private:
    static juce::String getProviderKeyName(AIProvider provider);
    static bool isValidModelName(const juce::String& model);
    static juce::String buildPrompt(const CurrentTrackContext& context);
    static std::vector<AIRecommendation> parseResponse(AIProvider provider, const juce::String& rawJson, juce::String& outError);
    // Fallback determinístico desde Up Next (cero IA): garantiza cards aunque la IA falle
    static std::vector<AIRecommendation> buildCrowdFallback(const CurrentTrackContext& context);
};

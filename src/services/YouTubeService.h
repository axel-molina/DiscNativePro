#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <vector>
#include <functional>

struct YouTubeSearchResult
{
    juce::String id;
    juce::String title;
    juce::String artist;
    juce::String thumbnailUrl;
    int durationSeconds { 180 };
    juce::String durationText { "3:00" };
    // Fecha de publicación ya formateada para la tabla. YouTube solo expone el
    // texto relativo en los resultados ("hace 2 años"), así que se guarda tal
    // cual; queda vacío si no viene (directos, o la ruta de oembed).
    juce::String publishedText;
};

class YouTubeService
{
public:
    static juce::String extractVideoId(const juce::String& input);
    static juce::String getApiKey();
    static void setApiKey(const juce::String& key);
    static bool hasApiKey();

    // Migración única: mueve la key legacy de settings.json al almacén seguro.
    static void migrateLegacySecret();

    // Reproducción automática al cargar un video en bandeja (default: true)
    static bool isVideoAutoplayEnabled();
    static void setVideoAutoplayEnabled(bool enabled);

    // Modo de reproducción de YouTube: Audio nativo (true) vs Video Web (false)
    static bool isAudioPlaybackMode();
    static void setAudioPlaybackMode(bool audioMode);

    // Guardado persistente de canciones descargadas en disco local (default: false)
    static bool isSaveDownloadsEnabled();
    static void setSaveDownloadsEnabled(bool enabled);

    // Carpeta local de descargas (por defecto: ~/Music/DiscNativePro/Downloads)
    static juce::File getDefaultDownloadsFolder();
    static juce::File getDownloadsFolder();
    static void setDownloadsFolder(const juce::File& folder);

    // Sanitización de nombres de archivo para el sistema de archivos
    static juce::String sanitizeFilename(const juce::String& name);

    // Synchronous search (safe to call on background thread)
    static std::vector<YouTubeSearchResult> search(const juce::String& query);

    // Asynchronous search (triggers onComplete on Message Thread)
    static void searchAsync(const juce::String& query,
                            std::function<void(const std::vector<YouTubeSearchResult>&)> onComplete);

    // Fast details lookup from direct ID / URL
    static YouTubeSearchResult getVideoDetails(const juce::String& videoIdOrUrl);

    // "Up Next" de la watch page: lo que la audiencia ve junto a este video.
    // Scrape (la API oficial ya no expone relacionados). Devuelve "Título — Canal".
    static juce::StringArray getRelatedVideos(const juce::String& videoId, int maxResults = 8);

    // Tags oficiales via videos.list (vacío sin API key o si falla)
    static juce::StringArray getVideoTags(const juce::String& videoId);

    struct AudienceSignals
    {
        juce::StringArray related;
        juce::StringArray tags;
    };

    // Related + tags en paralelo con caché de sesión. Llamar desde hilo de fondo.
    static AudienceSignals getAudienceSignals(const juce::String& videoId);

    // Precarga fire-and-forget (llamar al cambiar de pista; no hace nada si ya está en caché)
    static void prefetchAudienceSignals(const juce::String& videoId);

private:
    static std::vector<YouTubeSearchResult> searchPublic(const juce::String& query);
    static std::vector<YouTubeSearchResult> searchOfficialApi(const juce::String& query, const juce::String& apiKey);

    static juce::File getSettingsFile();
};

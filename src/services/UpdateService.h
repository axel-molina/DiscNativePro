#pragma once

#include <juce_core/juce_core.h>
#include <functional>
#include "InstallContext.h"

struct ReleaseItem
{
    juce::String tagName;      // p.ej. "v1.2.0"
    juce::String version;      // "1.2.0"
    juce::String name;
    juce::String releaseNotes;
    juce::String downloadUrl;  // Asset directo para el SO actual o pageUrl
    juce::String pageUrl;      // html_url del release
    juce::String assetName;    // Nombre del archivo a descargar
};

// Resultado del chequeo de actualización contra GitHub Releases.
// Todo el chequeo corre en background thread; el callback llega en Message Thread.
struct UpdateInfo
{
    bool available { false };
    juce::String currentVersion;
    juce::String latestTag;      // p.ej. "v1.2.0" (tal cual viene de la API)
    juce::String latestVersion;  // sin 'v': "1.2.0"
    juce::String releaseName;
    juce::String releaseNotes;   // body del release (puede estar vacío)
    juce::String downloadUrl;    // asset de esta plataforma, o html_url como fallback
    juce::String pageUrl;        // html_url del release
    juce::String assetName;
    std::vector<ReleaseItem> allReleases; // Lista de versiones disponibles en GitHub
};

class UpdateService
{
public:
    using UpdateCallback = std::function<void(const UpdateInfo&)>;

    // La UI debe consultar esto antes de ofrecer updater: en WinStore (MSIX)
    // y LinuxDeb el updater interno está deshabilitado (Store/apt gestionan).
    static bool isInternalUpdaterActive();
    static InstallContext::Kind getInstallContext();

    // Lanza el chequeo en background y responde en Message Thread.
    // Nunca lanza: sin red, timeout, rate-limit o tag omitido => available=false.
    static void checkForUpdatesAsync(UpdateCallback onComplete);

    // Descarga el asset (a Descargas en macOS/portable-Linux, a %TEMP% staging
    // en Windows Setup) con progreso y lo abre/ejecuta al terminar.
    static void downloadAndOpenReleaseAsset(
        const juce::String& downloadUrl,
        const juce::String& assetName,
        std::function<void(float progress, const juce::String& statusText)> onProgress,
        std::function<void(bool success, const juce::String& errorMessage, const juce::File& file)> onComplete);

    // P30 (Windows Setup): installer con update pendiente que se ejecuta
    // silenciosamente al cerrar la app. Ruta vacía = sin pendiente.
    static void setPendingInstallOnQuit(const juce::String& installerPath);
    static juce::String getPendingInstallOnQuit();

    // Llama al salir (JUCEApplication::shutdown): si hay installer pendiente y
    // el contexto es WinSetup, lo lanza /VERYSILENT apuntando al dir actual.
    static void runPendingInstaller();

    static juce::String getCurrentVersion();
    static bool isNewerVersion(const juce::String& latest, const juce::String& current);

    static juce::String getSkippedTag();
    static void setSkippedTag(const juce::String& tag);

private:
    static juce::File getSettingsFile();
    static int compareVersions(const juce::String& a, const juce::String& b);
};

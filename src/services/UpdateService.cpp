#include "UpdateService.h"
#include <juce_events/juce_events.h>

#if JUCE_LINUX
    #include <unistd.h> // getpid() para el script de auto-reemplazo
#endif

namespace
{
constexpr const char* kLatestReleaseApi =
    "https://api.github.com/repos/axel-molina/DiscNativePro/releases/latest";
constexpr const char* kReleasesApi =
    "https://api.github.com/repos/axel-molina/DiscNativePro/releases?per_page=15";

#if JUCE_MAC
constexpr const char* kAssetName = "DiscNativePro-macOS-Universal.zip";
#elif JUCE_WINDOWS
constexpr const char* kAssetName = "DiscNativePro-Windows-x64-Setup.exe";
#else
constexpr const char* kAssetName = "DiscNativePro-Linux-x64.tar.gz";
#endif

ReleaseItem parseReleaseItem(const juce::var& releaseObj)
{
    ReleaseItem item;
    if (!releaseObj.isObject())
        return item;

    item.tagName = releaseObj["tag_name"].toString().trim();
    item.version = item.tagName;
    if (item.version.startsWithChar('v') || item.version.startsWithChar('V'))
        item.version = item.version.substring(1);
    item.name = releaseObj["name"].toString().trim();
    if (item.name.isEmpty())
        item.name = item.tagName;
    item.releaseNotes = releaseObj["body"].toString();
    item.pageUrl = releaseObj["html_url"].toString();
    item.downloadUrl = item.pageUrl;

    if (auto* assets = releaseObj["assets"].getArray())
    {
        for (auto& a : *assets)
        {
            auto name = a["name"].toString();
            auto dl = a["browser_download_url"].toString();
            if (name == kAssetName && dl.isNotEmpty())
            {
                item.downloadUrl = dl;
                item.assetName = name;
                break;
            }
        }

        if (item.assetName.isEmpty())
        {
            for (auto& a : *assets)
            {
                auto name = a["name"].toString();
                auto dl = a["browser_download_url"].toString();
               #if JUCE_MAC
                if (name.containsIgnoreCase("mac") || name.endsWithIgnoreCase(".dmg") || name.endsWithIgnoreCase(".zip"))
               #elif JUCE_WINDOWS
                if (name.containsIgnoreCase("win") || name.endsWithIgnoreCase(".exe") || name.endsWithIgnoreCase(".zip"))
               #else
                if (name.containsIgnoreCase("linux") || name.endsWithIgnoreCase(".tar.gz"))
               #endif
                {
                    item.downloadUrl = dl;
                    item.assetName = name;
                    break;
                }
            }
        }
    }

    return item;
}
} // namespace

juce::String UpdateService::getCurrentVersion()
{
    // Fuente runtime única: la versión la decide JUCEApplication
    // (getApplicationVersion()), alimentada por APP_VERSION en CMake.
    if (auto* app = juce::JUCEApplicationBase::getInstance())
        return app->getApplicationVersion();

    return DISCNATIVEPRO_VERSION;
}

int UpdateService::compareVersions(const juce::String& a, const juce::String& b)
{
    // Compara solo componentes numéricos ("1.2.0" vs "1.10.0" => -1).
    // Los sufijos (-dev, -rc1) se ignoran aquí; se resuelven en isNewerVersion.
    auto numParts = [](const juce::String& v) {
        juce::StringArray parts;
        parts.addTokens(v.upToFirstOccurrenceOf("-", false, false), ".", "");
        return parts;
    };

    auto pa = numParts(a.trim());
    auto pb = numParts(b.trim());
    int n = juce::jmax(pa.size(), pb.size());
    for (int i = 0; i < n; ++i)
    {
        int va = i < pa.size() ? pa[i].getIntValue() : 0;
        int vb = i < pb.size() ? pb[i].getIntValue() : 0;
        if (va != vb)
            return va < vb ? -1 : 1;
    }
    return 0;
}

bool UpdateService::isNewerVersion(const juce::String& latest, const juce::String& current)
{
    juce::String l = latest.trim();
    juce::String c = current.trim();
    if (l.startsWithChar('v') || l.startsWithChar('V'))
        l = l.substring(1);
    if (c.startsWithChar('v') || c.startsWithChar('V'))
        c = c.substring(1);

    int cmp = compareVersions(l, c);
    // Comparación solo numérica: un empate ("1.0.0-3-gabc123" vs "1.0.0")
    // NO es update. Solo una versión numéricamente mayor avisa.
    // (Los builds locales ya no reportan un "-dev" genérico: CMake estampa
    // git describe, así que no hacen falta reglas especiales de sufijos.)
    return cmp > 0;
}

juce::File UpdateService::getSettingsFile()
{
    auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                       .getChildFile("DiscNativePro");
    if (!appData.exists())
        appData.createDirectory();
    return appData.getChildFile("settings.json");
}

juce::String UpdateService::getSkippedTag()
{
    auto file = getSettingsFile();
    if (!file.existsAsFile())
        return {};
    auto json = juce::JSON::parse(file);
    if (json.isObject() && json.hasProperty("skippedUpdateTag"))
        return json["skippedUpdateTag"].toString();
    return {};
}

void UpdateService::setSkippedTag(const juce::String& tag)
{
    auto file = getSettingsFile();
    juce::var json;
    if (file.existsAsFile())
        json = juce::JSON::parse(file);
    if (!json.isObject())
        json = juce::var(new juce::DynamicObject());

    if (auto* obj = json.getDynamicObject())
    {
        if (tag.isEmpty())
            obj->removeProperty("skippedUpdateTag");
        else
            obj->setProperty("skippedUpdateTag", tag);
    }
    file.replaceWithText(juce::JSON::toString(json));
}

InstallContext::Kind UpdateService::getInstallContext()
{
    static const InstallContext::Kind ctx = InstallContext::detect();
    return ctx;
}

bool UpdateService::isInternalUpdaterActive()
{
    switch (getInstallContext())
    {
        case InstallContext::Kind::MacApp:
        case InstallContext::Kind::WinSetup:
        case InstallContext::Kind::LinuxPortable:
            return true;
        case InstallContext::Kind::WinStore:
        case InstallContext::Kind::LinuxDeb:
        default:
            return false;
    }
}

void UpdateService::setPendingInstallOnQuit(const juce::String& installerPath)
{
    auto file = getSettingsFile();
    juce::var json;
    if (file.existsAsFile())
        json = juce::JSON::parse(file);
    if (!json.isObject())
        json = juce::var(new juce::DynamicObject());

    if (auto* obj = json.getDynamicObject())
    {
        if (installerPath.isEmpty())
            obj->removeProperty("pendingInstallOnQuit");
        else
            obj->setProperty("pendingInstallOnQuit", installerPath);
    }
    file.replaceWithText(juce::JSON::toString(json));
}

juce::String UpdateService::getPendingInstallOnQuit()
{
    auto file = getSettingsFile();
    if (!file.existsAsFile())
        return {};
    auto json = juce::JSON::parse(file);
    if (json.isObject() && json.hasProperty("pendingInstallOnQuit"))
        return json["pendingInstallOnQuit"].toString();
    return {};
}

void UpdateService::runPendingInstaller()
{
    const auto pending = getPendingInstallOnQuit();
    setPendingInstallOnQuit({});
    if (pending.isEmpty())
        return;

   #if JUCE_WINDOWS
    if (getInstallContext() != InstallContext::Kind::WinSetup)
        return;

    auto installerFile = juce::File(pending);
    if (!installerFile.existsAsFile())
        return;

    // Inno Setup silencioso: sin diálogos, cierra la app en ejecución y
    // actualiza en el mismo directorio donde ya está instalada.
    const auto installDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                                .getParentDirectory().getFullPathName();
    const juce::String args = "/VERYSILENT /SUPPRESSMSGBOXES /NORESTART "
                              "/CloseApplications=force "
                              "/DIR=\"" + installDir + "\"";
    juce::Process::openDocument(installerFile.getFullPathName(), args);
   #elif JUCE_LINUX
    if (getInstallContext() != InstallContext::Kind::LinuxPortable)
        return;

    auto stagedDir = juce::File(pending);
    if (!stagedDir.isDirectory())
        return;

    // Script de auto-reemplazo: espera la muerte de este proceso, hace backup
    // del binario, copia el staging encima del dir de la app y relanza.
    // Vive en .cache (fuera del dir de la app) para no ser sobrescrito por la
    // propia copia. El PID se hornea en el script porque el script sobrevive
    // al proceso que lo lanza.
    const auto appDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                            .getParentDirectory().getFullPathName();
    const auto script = stagedDir.getParentDirectory().getChildFile("apply_update.sh");

    const juce::String body =
        "#!/bin/bash\n"
        "# Auto-reemplazo de DiscNativePro (generado por la app al salir)\n"
        "DST=\"" + appDir + "\"\n"
        "SRC=\"" + stagedDir.getFullPathName() + "\"\n"
        "while [ -d /proc/" + juce::String((int) getpid()) + " ]; do sleep 0.3; done\n"
        "cp -f \"$DST/DiscNativePro\" \"$DST/DiscNativePro.bak\" 2>/dev/null\n"
        "cp -f \"$SRC\"/* \"$DST\"/ || exit 1\n"
        "rm -rf \"$SRC\"\n"
        "cd \"$DST\" && nohup ./run.sh >/dev/null 2>&1 &\n";

    script.deleteFile();
    script.replaceWithText(body);
    script.setExecutePermission(true);
    juce::Process::openDocument(script.getFullPathName(), {});
   #endif
}

void UpdateService::checkForUpdatesAsync(UpdateCallback onComplete)
{
    juce::Thread::launch([onComplete]() {
        UpdateInfo info;
        info.currentVersion = getCurrentVersion();

        // Store (MSIX) y .deb no usan el updater interno: responder vacío.
        if (!isInternalUpdaterActive())
        {
            juce::MessageManager::callAsync([onComplete, info]() { onComplete(info); });
            return;
        }

        // 1. Intentar listar releases recientes
        juce::URL listUrl(kReleasesApi);
        std::unique_ptr<juce::InputStream> stream(listUrl.createInputStream(
            juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(10000)
                .withExtraHeaders("User-Agent: DiscNativePro-Updater\r\n"
                                  "Accept: application/vnd.github+json\r\n")));

        if (stream != nullptr)
        {
            auto json = juce::JSON::parse(stream->readEntireStreamAsString());
            if (json.isArray())
            {
                auto* arr = json.getArray();
                for (const auto& elem : *arr)
                {
                    auto item = parseReleaseItem(elem);
                    if (item.version.isNotEmpty())
                        info.allReleases.push_back(item);
                }
            }
            else if (json.isObject())
            {
                auto item = parseReleaseItem(json);
                if (item.version.isNotEmpty())
                    info.allReleases.push_back(item);
            }
        }

        // 2. Si falló la lista o vino vacía, intentar /releases/latest directamente
        if (info.allReleases.empty())
        {
            juce::URL latestUrl(kLatestReleaseApi);
            std::unique_ptr<juce::InputStream> latestStream(latestUrl.createInputStream(
                juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                    .withConnectionTimeoutMs(10000)
                    .withExtraHeaders("User-Agent: DiscNativePro-Updater\r\n"
                                      "Accept: application/vnd.github+json\r\n")));

            if (latestStream != nullptr)
            {
                auto json = juce::JSON::parse(latestStream->readEntireStreamAsString());
                if (json.isObject())
                {
                    auto item = parseReleaseItem(json);
                    if (item.version.isNotEmpty())
                        info.allReleases.push_back(item);
                }
            }
        }

        // 3. Procesar release más reciente
        if (!info.allReleases.empty())
        {
            const auto& latest = info.allReleases.front();
            info.latestTag = latest.tagName;
            info.latestVersion = latest.version;
            info.releaseName = latest.name;
            info.releaseNotes = latest.releaseNotes;
            info.pageUrl = latest.pageUrl;
            info.downloadUrl = latest.downloadUrl;
            info.assetName = latest.assetName;

            if (info.latestVersion.isNotEmpty()
                && info.latestTag != getSkippedTag()
                && isNewerVersion(info.latestVersion, info.currentVersion))
            {
                info.available = true;
            }
        }

        juce::MessageManager::callAsync([onComplete, info]() { onComplete(info); });
    });
}

void UpdateService::downloadAndOpenReleaseAsset(
    const juce::String& downloadUrl,
    const juce::String& assetName,
    std::function<void(float progress, const juce::String& statusText)> onProgress,
    std::function<void(bool success, const juce::String& errorMessage, const juce::File& file)> onComplete)
{
    juce::Thread::launch([downloadUrl, assetName, onProgress, onComplete]() {
        if (downloadUrl.isEmpty())
        {
            if (onComplete)
                juce::MessageManager::callAsync([onComplete]() {
                    onComplete(false, "URL de descarga vacía.", {});
                });
            return;
        }

        // Si es una página web general en vez de un archivo instalable, abrir navegador
        if (!downloadUrl.endsWithIgnoreCase(".zip") &&
            !downloadUrl.endsWithIgnoreCase(".dmg") &&
            !downloadUrl.endsWithIgnoreCase(".exe") &&
            !downloadUrl.endsWithIgnoreCase(".tar.gz"))
        {
            juce::URL(downloadUrl).launchInDefaultBrowser();
            if (onComplete)
                juce::MessageManager::callAsync([onComplete]() {
                    onComplete(true, {}, {});
                });
            return;
        }

       #if JUCE_WINDOWS
        // P30: el Setup se descarga a staging en %TEMP% (no a Descargas) y no
        // se lanza acá: la UI ofrece "Instalar al reiniciar"/"Actualizar ahora"
        // y runPendingInstaller lo ejecuta /VERYSILENT al cerrar la app.
        juce::File dlFolder = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                  .getChildFile("DiscNativePro").getChildFile("update");
        dlFolder.createDirectory();
       #else
        auto dlFolder = juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile("Downloads");
        if (!dlFolder.exists())
            dlFolder = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
       #endif

        juce::String safeFileName = assetName.isNotEmpty() ? assetName : downloadUrl.fromLastOccurrenceOf("/", false, false);
        if (safeFileName.isEmpty())
            safeFileName = "DiscNativePro-Update.zip";

        auto targetFile = dlFolder.getChildFile(safeFileName);
        if (targetFile.existsAsFile())
            targetFile.deleteFile();

        juce::URL url(downloadUrl);
        std::unique_ptr<juce::InputStream> stream(url.createInputStream(
            juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(15000)
                .withNumRedirectsToFollow(10)
                .withExtraHeaders("User-Agent: DiscNativePro-Updater\r\n"
                                  "Accept: application/octet-stream\r\n")));

        if (stream == nullptr)
        {
            if (onComplete)
                juce::MessageManager::callAsync([onComplete]() {
                    onComplete(false, "No se pudo conectar al servidor de descargas.", {});
                });
            return;
        }

        juce::int64 totalLength = stream->getTotalLength();
        std::unique_ptr<juce::FileOutputStream> out(targetFile.createOutputStream());
        if (out == nullptr || !out->openedOk())
        {
            if (onComplete)
                juce::MessageManager::callAsync([onComplete]() {
                    onComplete(false, "No se pudo crear el archivo en Descargas.", {});
                });
            return;
        }

        constexpr juce::int64 bufferSize = 64 * 1024;
        juce::HeapBlock<char> buffer((size_t)bufferSize);
        juce::int64 bytesWritten = 0;

        while (!stream->isExhausted())
        {
            int read = stream->read(buffer.get(), (int)bufferSize);
            if (read <= 0)
                break;

            out->write(buffer.get(), (size_t)read);
            bytesWritten += read;

            if (onProgress && totalLength > 0)
            {
                float prog = (float)bytesWritten / (float)totalLength;
                juce::String status = juce::String::formatted("%.1f MB / %.1f MB",
                                                              (double)bytesWritten / (1024.0 * 1024.0),
                                                              (double)totalLength / (1024.0 * 1024.0));
                juce::MessageManager::callAsync([onProgress, prog, status]() {
                    onProgress(prog, status);
                });
            }
        }

        out->flush();
        out.reset();

        bool success = targetFile.existsAsFile() && targetFile.getSize() > 1000;

       #if JUCE_LINUX
        // P30 LinuxPortable: extraer el tar.gz a staging acá (thread de fondo)
        // para que el reemplazo al salir solo copie archivos ya listos.
        juce::File extractedDir;
        if (success && getInstallContext() == InstallContext::Kind::LinuxPortable
            && targetFile.getFileName().endsWithIgnoreCase(".tar.gz"))
        {
            auto staging = juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                               .getChildFile(".cache/DiscNativePro/update");
            staging.createDirectory();
            auto dest = staging.getChildFile("new");
            if (dest.exists())
                dest.deleteRecursively();
            dest.createDirectory();

            juce::ChildProcess tar;
            const bool ok = tar.start("tar -xzf \"" + targetFile.getFullPathName()
                                      + "\" -C \"" + dest.getFullPathName() + "\"")
                            && tar.waitForProcessToFinish(120000)
                            && tar.getExitCode() == 0;
            if (ok)
            {
                extractedDir = dest;
                // Marcado acá en el thread de fondo: evita capturar
                // extractedDir en el lambda de cierre y la escritura es solo
                // un JSON en disco.
                setPendingInstallOnQuit(extractedDir.getFullPathName());
            }
            else
                success = false;
        }
       #endif

        if (success)
        {
            juce::MessageManager::callAsync([onComplete, targetFile]() {
               #if JUCE_MAC
                if (targetFile.getFileExtension().equalsIgnoreCase(".dmg"))
                {
                    targetFile.startAsProcess();
                }
                else
                {
                    targetFile.revealToUser();
                    targetFile.startAsProcess();
                }
               #elif JUCE_WINDOWS
                // Setup en staging: no se ejecuta desde acá. La UI setea
                // setPendingInstallOnQuit(targetFile) y runPendingInstaller
                // lo lanza silencioso al cerrar la app.
               #elif JUCE_LINUX
                // Portable: si hubo extracción a staging ya quedó marcado para
                // instalarse al salir; si no (p. ej. asset no-tar.gz), solo se
                // revela el archivo.
                if (getPendingInstallOnQuit().isEmpty())
                    targetFile.revealToUser();
               #else
                targetFile.revealToUser();
               #endif

                if (onComplete)
                    onComplete(true, {}, targetFile);
            });
        }
        else
        {
            if (onComplete)
                juce::MessageManager::callAsync([onComplete]() {
                    onComplete(false, "El archivo descargado está incompleto o dañado.", {});
                });
        }
    });
}

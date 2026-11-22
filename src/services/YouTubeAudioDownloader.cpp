#include "YouTubeAudioDownloader.h"
#include "YouTubeService.h"
#include "LanguageManager.h"

YouTubeAudioDownloader& YouTubeAudioDownloader::getInstance()
{
    static YouTubeAudioDownloader instance;
    return instance;
}

YouTubeAudioDownloader::YouTubeAudioDownloader()
{
    getCacheFolder().createDirectory();
}

juce::File YouTubeAudioDownloader::getCacheFolder() const
{
    auto userAppDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    auto cacheDir = userAppDir.getChildFile("Caches/DiscNativePro/YouTubeAudio");
    return cacheDir;
}

namespace
{
// P26: un archivo m4a real arranca con el box "ftyp" en el offset 4. yt-dlp con
// el fallback "ba" puede devolver webm/opus con nombre .m4a que el reader no
// abre: mejor rechazarlo aquí que cachear un archivo roto para siempre.
bool hasM4aHeader(const juce::File& f)
{
    juce::FileInputStream in(f);
    if (!in.openedOk())
        return false;
    char head[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    const auto n = in.read(head, 8);
    return n >= 8 && head[4] == 'f' && head[5] == 't' && head[6] == 'y' && head[7] == 'p';
}

// --- Localizacion e instalacion del binario (issue #4) ----------------------
// El asset de GitHub es distinto por plataforma. Instalar siempre el de macOS
// dejaba en Windows y Linux un binario que no arranca, y ademas getBinaryPath()
// lo daba por bueno porque el archivo existia: el fallo era permanente.
juce::File appYtDlpDir()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("DiscNativePro/bin");
}

const char* ytDlpAssetName()
{
   #if JUCE_WINDOWS
    return "yt-dlp.exe";
   #elif JUCE_MAC
    return "yt-dlp_macos";
   #else
    return "yt-dlp_linux";
   #endif
}

const char* ytDlpLocalName()
{
   #if JUCE_WINDOWS
    return "yt-dlp.exe";
   #else
    return "yt-dlp";
   #endif
}

// Un binario inservible (arquitectura equivocada, descarga a medias, sin permiso
// de ejecucion) se detecta arrancandolo, no comprobando que el archivo exista.
bool ytDlpBinaryRuns (const juce::File& f)
{
    if (! f.existsAsFile() || f.getSize() < 100000)
        return false;

    juce::ChildProcess p;
    if (! p.start (juce::StringArray { f.getFullPathName(), "--version" }))
        return false;

    return p.waitForProcessToFinish (20000) && p.getExitCode() == 0;
}
} // namespace

juce::File YouTubeAudioDownloader::getCachedAudioFile(const juce::String& videoId) const
{
    if (videoId.isEmpty())
        return {};

    // 1. Verificar primero si reside en la carpeta permanente de Descargas
    auto dlFolder = YouTubeService::getDownloadsFolder();
    if (dlFolder.isDirectory())
    {
        juce::Array<juce::File> dlFiles;
        dlFolder.findChildFiles(dlFiles, juce::File::findFiles, false, "*" + videoId + "*.m4a");
        for (const auto& f : dlFiles)
        {
            if (f.existsAsFile() && f.getSize() > 10000 && hasM4aHeader(f))
                return f;
        }
    }

    // 2. Verificar en la caché temporal
    auto file = getCacheFolder().getChildFile(videoId + ".m4a");
    if (file.existsAsFile() && file.getSize() > 10000 && hasM4aHeader(file))
        return file;

    return {};
}

bool YouTubeAudioDownloader::isDownloading(const juce::String& videoId) const
{
    const std::lock_guard<std::mutex> lock(mutex);
    return activeDownloads.find(videoId.toStdString()) != activeDownloads.end();
}

juce::String YouTubeAudioDownloader::getBinaryPath() const
{
    const juce::String localName (ytDlpLocalName());

    // 1. Instalacion propia, y la ruta heredada de versiones anteriores (que en
    //    macOS duplicaba "Application Support" y nunca coincidia).
    juce::Array<juce::File> candidates;
    candidates.add (appYtDlpDir().getChildFile (localName));
    candidates.add (juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                        .getChildFile ("Application Support/DiscNativePro/bin")
                        .getChildFile (localName));

    for (const auto& f : candidates)
        if (f.existsAsFile() && f.hasWriteAccess())
            return f.getFullPathName();

    // 2. Empaquetado con la app (Contents/Resources en macOS, junto al .exe en
    //    Windows/Linux).
    auto exeFile = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
    juce::Array<juce::File> bundled;
    bundled.add (exeFile.getParentDirectory().getParentDirectory()
                     .getChildFile ("Resources").getChildFile (localName));
    bundled.add (exeFile.getParentDirectory().getChildFile (localName));

    for (const auto& f : bundled)
        if (f.existsAsFile())
            return f.getFullPathName();

   #if JUCE_MAC
    // 3. Homebrew Apple Silicon y 4. Homebrew Intel
    for (const auto& f : { juce::File ("/opt/homebrew/bin/yt-dlp"), juce::File ("/usr/local/bin/yt-dlp") })
        if (f.existsAsFile())
            return f.getFullPathName();
   #endif

    // 5. PATH: "where" en Windows, "which" en POSIX (siempre con argumentos
    //    sueltos, sin pasar por un shell).
   #if JUCE_WINDOWS
    juce::ChildProcess finder;
    if (finder.start (juce::StringArray { "where", "yt-dlp" }))
    {
        for (const auto& line : juce::StringArray::fromLines (finder.readAllProcessOutput()))
        {
            const auto path = line.trim();
            if (path.isNotEmpty() && juce::File (path).existsAsFile())
                return juce::File (path).getFullPathName();
        }
    }
   #else
    juce::ChildProcess finder;
    if (finder.start (juce::StringArray { "which", "yt-dlp" }))
    {
        const auto out = finder.readAllProcessOutput().trim();
        if (out.isNotEmpty() && juce::File (out).existsAsFile())
            return out;
    }
   #endif

    return {};
}

void YouTubeAudioDownloader::ensureBinaryInstalledAsync(std::function<void(bool success)> onDone)
{
    juce::Thread::launch([this, onDone]() {
        auto existing = getBinaryPath();
        if (existing.isNotEmpty())
        {
            if (onDone)
                juce::MessageManager::callAsync([onDone]() { onDone(true); });
            return;
        }

        auto targetDir = appYtDlpDir();
        targetDir.createDirectory();

        auto targetFile = targetDir.getChildFile (juce::String (ytDlpLocalName()));
        targetFile.deleteFile();

        // Sin shell y con el asset de esta plataforma.
        juce::StringArray curlArgs { "curl", "-sL",
                                     "https://github.com/yt-dlp/yt-dlp/releases/latest/download/"
                                         + juce::String (ytDlpAssetName()),
                                     "-o", targetFile.getFullPathName() };
        juce::ChildProcess proc;
        bool ok = proc.start (curlArgs)
                  && proc.waitForProcessToFinish (120000)
                  && proc.getExitCode() == 0;

        if (ok)
            ok = targetFile.setExecutePermission (true);

        ok = ok && ytDlpBinaryRuns (targetFile);
        if (! ok)
            targetFile.deleteFile();

        if (onDone)
            juce::MessageManager::callAsync([onDone, ok]() { onDone(ok); });
    });
}

void YouTubeAudioDownloader::cancelDownload(const juce::String& videoId)
{
    const std::lock_guard<std::mutex> lock(mutex);
    auto it = cancelFlags.find(videoId.toStdString());
    if (it != cancelFlags.end() && it->second)
    {
        it->second->store(true);
    }
}

void YouTubeAudioDownloader::downloadAudioAsync(const juce::String& videoId,
                                                ProgressCallback onProgress,
                                                CompletionCallback onComplete)
{
    downloadAudioAsync(videoId, {}, {}, onProgress, onComplete);
}

void YouTubeAudioDownloader::downloadAudioAsync(const juce::String& videoId,
                                                const juce::String& title,
                                                const juce::String& artist,
                                                ProgressCallback onProgress,
                                                CompletionCallback onComplete)
{
    if (videoId.isEmpty())
    {
        if (onComplete)
            juce::MessageManager::callAsync([onComplete]() {
                onComplete({}, false, LanguageManager::trUtf8("ID de video no válido", "Invalid video ID"));
            });
        return;
    }

    // 1. Instant cache hit
    auto cached = getCachedAudioFile(videoId);
    if (cached.existsAsFile())
    {
        if (onProgress)
            juce::MessageManager::callAsync([onProgress]() {
                onProgress(1.0f, LanguageManager::trUtf8("Audio cargado de caché", "Audio loaded from cache"));
            });

        if (onComplete)
            juce::MessageManager::callAsync([onComplete, cached]() {
                onComplete(cached, true, {});
            });
        return;
    }

    // 2. P26: dedup — si el mismo video ya está descargándose (otro deck,
    // doble clic), el segundo pedido espera al primero y reusa el resultado.
    if (isDownloading(videoId))
    {
        juce::Thread::launch([this, videoId, onProgress, onComplete]() {
            if (onProgress)
                juce::MessageManager::callAsync([onProgress]() {
                    onProgress(0.0f, LanguageManager::trUtf8("Esperando descarga en curso...", "Waiting for ongoing download..."));
                });
            const auto deadlineMs = juce::Time::getMillisecondCounter() + 600000; // tope 10 min
            while (juce::Time::getMillisecondCounter() < deadlineMs && isDownloading(videoId))
                juce::Thread::sleep(200);

            auto done = getCachedAudioFile(videoId);
            if (onComplete)
                juce::MessageManager::callAsync([onComplete, done]() {
                    if (done.existsAsFile())
                        onComplete(done, true, {});
                    else
                        onComplete({}, false, LanguageManager::trUtf8("La descarga anterior no terminó", "The previous download did not finish"));
                });
        });
        return;
    }

    // 3. Launch background download
    auto cancelFlag = std::make_shared<std::atomic<bool>>(false);
    {
        const std::lock_guard<std::mutex> lock(mutex);
        activeDownloads.insert(videoId.toStdString());
        cancelFlags[videoId.toStdString()] = cancelFlag;
    }

    juce::Thread::launch([this, videoId, title, artist, cancelFlag, onProgress, onComplete]() {
        // Ensure binary path is resolved
        auto binPath = getBinaryPath();

        // Un binario presente pero inservible (p. ej. el yt-dlp_macos que
        // instalaban versiones anteriores en Windows/Linux) se borra y se
        // reinstala, en vez de fallar para siempre (issue #4).
        if (binPath.isNotEmpty() && ! ytDlpBinaryRuns (juce::File (binPath)))
        {
            juce::File (binPath).deleteFile();
            binPath.clear();
        }

        if (binPath.isEmpty())
        {
            // Auto-install binary if missing
            auto targetDir = appYtDlpDir();
            targetDir.createDirectory();
            auto targetFile = targetDir.getChildFile (juce::String (ytDlpLocalName()));
            targetFile.deleteFile();

            if (onProgress)
            {
                juce::MessageManager::callAsync([onProgress]() {
                    onProgress(0.05f, LanguageManager::trUtf8("Preparando motor de descarga...", "Preparing download engine..."));
                });
            }

            // P43: el auto-instalador NO pasa por shell — ChildProcess en POSIX
            // llama execvp directo, así que un comando con comillas/&& queda
            // roto (curl recibía la URL con las comillas y chmod nunca
            // corría). Se instalan en pasos discretos con argumentos sueltos.
            juce::StringArray curlArgs { "curl", "-sL",
                                         "https://github.com/yt-dlp/yt-dlp/releases/latest/download/"
                                             + juce::String (ytDlpAssetName()),
                                         "-o", targetFile.getFullPathName() };
            juce::ChildProcess curlProc;
            bool installed = curlProc.start(curlArgs)
                             && curlProc.waitForProcessToFinish(60000)
                             && curlProc.getExitCode() == 0
                             && targetFile.existsAsFile()
                             && targetFile.getSize() > 0;
            if (installed)
                installed = targetFile.setExecutePermission(true);

            // El binario debe arrancar en esta maquina; si no, se borra. Dejarlo
            // hacia que getBinaryPath() lo devolviese para siempre (issue #4).
            if (installed)
                installed = ytDlpBinaryRuns (targetFile);
            if (! installed)
                targetFile.deleteFile();

            binPath = getBinaryPath(); // re-evalúa tras la instalación
        }

        if (binPath.isEmpty())
        {
            {
                const std::lock_guard<std::mutex> lock(mutex);
                activeDownloads.erase(videoId.toStdString());
                cancelFlags.erase(videoId.toStdString());
            }
            if (onComplete)
            {
                juce::MessageManager::callAsync([onComplete]() {
                    onComplete({}, false, LanguageManager::trUtf8("No se pudo iniciar el extractor yt-dlp", "Could not start yt-dlp extractor"));
                });
            }
            return;
        }

        auto targetFolder = getCacheFolder();
        targetFolder.createDirectory();
        // P26: salida temporal ÚNICA por proceso — dos yt-dlp nunca se pisan el
        // .part; al validar, se renombra (atómico en el mismo volumen).
        auto outFile = targetFolder.getChildFile(videoId + ".m4a");
        const auto tmpName = videoId + ".p" + juce::String((int)juce::Time::getMillisecondCounterHiRes()) + ".m4a";
        auto tmpFile = targetFolder.getChildFile(tmpName);
        tmpFile.deleteFile();

        // Download command requesting best native AAC audio stream (m4a container).
        // P26: formato estricto — sin el fallback "ba" desnudo, que puede bajar
        // webm/opus (48 kHz) guardándolo como .m4a.
        juce::StringArray args;
        args.add(binPath);
        args.add("-f");
        args.add("ba[ext=m4a]/ba[ext=mp4]");
        args.add("--no-playlist");
        args.add("--newline");
        args.add("-o");
        args.add(tmpFile.getFullPathName());
        args.add("https://www.youtube.com/watch?v=" + videoId);

        juce::ChildProcess proc;
        if (!proc.start(args))
        {
            {
                const std::lock_guard<std::mutex> lock(mutex);
                activeDownloads.erase(videoId.toStdString());
                cancelFlags.erase(videoId.toStdString());
            }
            if (onComplete)
            {
                juce::MessageManager::callAsync([onComplete]() {
                    onComplete({}, false, LanguageManager::trUtf8("Fallo al iniciar descarga", "Failed to start download process"));
                });
            }
            return;
        }

        // Stream output reader to report progress
        char buffer[1024];
        juce::String currentLine;
        float lastReportedProgress = 0.0f;

        while (proc.isRunning())
        {
            if (cancelFlag->load())
            {
                proc.kill();
                tmpFile.deleteFile();
                {
                    const std::lock_guard<std::mutex> lock(mutex);
                    activeDownloads.erase(videoId.toStdString());
                    cancelFlags.erase(videoId.toStdString());
                }
                return;
            }

            int bytesRead = proc.readProcessOutput(buffer, sizeof(buffer) - 1);
            if (bytesRead > 0)
            {
                buffer[bytesRead] = '\0';
                currentLine += buffer;

                int newlineIdx;
                while ((newlineIdx = currentLine.indexOfAnyOf("\r\n")) >= 0)
                {
                    auto line = currentLine.substring(0, newlineIdx).trim();
                    currentLine = currentLine.substring(newlineIdx + 1);

                    // Parse yt-dlp download progress: "[download]  45.2% of ..."
                    if (line.contains("[download]") && line.contains("%"))
                    {
                        int pctIdx = line.indexOf("%");
                        if (pctIdx > 0)
                        {
                            int startIdx = pctIdx - 1;
                            while (startIdx >= 0 && (juce::CharacterFunctions::isDigit(line[startIdx]) || line[startIdx] == '.'))
                                --startIdx;

                            auto numStr = line.substring(startIdx + 1, pctIdx).trim();
                            float pct = (float)numStr.getDoubleValue() / 100.0f;
                            pct = juce::jlimit(0.0f, 1.0f, pct);

                            if (pct >= lastReportedProgress + 0.02f || pct >= 0.99f)
                            {
                                lastReportedProgress = pct;
                                if (onProgress)
                                {
                                    juce::MessageManager::callAsync([onProgress, pct]() {
                                        juce::String status = LanguageManager::trUtf8("Descargando audio de YouTube... ", "Downloading YouTube audio... ")
                                                            + juce::String((int)(pct * 100.0f)) + "%";
                                        onProgress(pct, status);
                                    });
                                }
                            }
                        }
                    }
                }
            }
            else
            {
                juce::Thread::sleep(25);
            }
        }

        proc.waitForProcessToFinish(5000);
        juce::uint32 exitCode = proc.getExitCode();

        // P26: validación fuerte — exitCode 0 OBLIGATORIO, tamaño mínimo y
        // contenedor m4a real (header "ftyp"). Un archivo basura nunca llega a
        // la caché ni a los Downloads.
        bool success = exitCode == 0 && tmpFile.existsAsFile()
                       && tmpFile.getSize() > 10000 && hasM4aHeader(tmpFile);
        if (success)
        {
            if (outFile.existsAsFile())
                outFile.deleteFile();
            if (!tmpFile.moveFileTo(outFile))
            {
                tmpFile.deleteFile();
                success = false;
            }
        }
        else
        {
            tmpFile.deleteFile();
            outFile.deleteFile(); // caché previa rota/parcial: fuera
        }

        juce::File finalResultFile = outFile;
        bool savedToDownloads = false;

        if (success && YouTubeService::isSaveDownloadsEnabled())
        {
            auto dlFolder = YouTubeService::getDownloadsFolder();
            if (!dlFolder.exists())
                dlFolder.createDirectory();

            juce::String safeTitle = YouTubeService::sanitizeFilename(title.isNotEmpty() ? title : ("YouTube_" + videoId));
            juce::String safeArtist = YouTubeService::sanitizeFilename(artist);

            juce::String finalFileName;
            if (safeArtist.isNotEmpty() && !safeArtist.equalsIgnoreCase("Artista Local") && !safeArtist.equalsIgnoreCase("Local Artist"))
                finalFileName = safeArtist + " - " + safeTitle + " [" + videoId + "].m4a";
            else
                finalFileName = safeTitle + " [" + videoId + "].m4a";

            auto persistentFile = dlFolder.getChildFile(finalFileName);
            if (persistentFile.existsAsFile() && persistentFile.getSize() != outFile.getSize())
                persistentFile.deleteFile();
            if (!persistentFile.existsAsFile())
            {
                // P26: copia atómica — el destino nunca es visible a medio copiar
                // (getCachedAudioFile escanea esta carpeta).
                auto tmpPersist = dlFolder.getChildFile(finalFileName + ".part");
                tmpPersist.deleteFile();
                if (outFile.copyFileTo(tmpPersist) && tmpPersist.getSize() == outFile.getSize()
                    && tmpPersist.moveFileTo(persistentFile))
                {
                }
                else
                {
                    tmpPersist.deleteFile();
                }
            }

            if (persistentFile.existsAsFile() && persistentFile.getSize() > 10000)
            {
                finalResultFile = persistentFile;
                savedToDownloads = true;
            }
        }

        {
            const std::lock_guard<std::mutex> lock(mutex);
            activeDownloads.erase(videoId.toStdString());
            cancelFlags.erase(videoId.toStdString());
        }

        if (onComplete)
        {
            juce::MessageManager::callAsync([this, onComplete, finalResultFile, success, savedToDownloads, videoId, title, artist]() {
                if (success)
                {
                    if (savedToDownloads && onTrackSavedToDownloads)
                    {
                        onTrackSavedToDownloads(finalResultFile, videoId, title, artist);
                    }
                    onComplete(finalResultFile, true, {});
                }
                else
                {
                    onComplete({}, false, LanguageManager::trUtf8("No se pudo obtener el audio de YouTube", "Could not retrieve YouTube audio"));
                }
            });
        }
    });
}

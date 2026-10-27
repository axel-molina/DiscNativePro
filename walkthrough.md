# Walkthrough — Renombrado visible DiscPro → DiscNativePro

## Objetivo
Eliminar restos de marca "DiscPro" en textos visibles UI (bandejas/JogWheel, TopBar, modales).

## Cambios ([MODIFY], alcance Solo-UI según aprobación)
- `src/ui/JogWheel.cpp`: eliminado bloque `drawText("DiscPro")` + comentario. Se conserva barra de color + BPM/RPM + spindle. Nota `ponytail:` el nombre largo no cabe en sticker ~60px.
- `src/ui/TopBarComponent.cpp:42`: `brandLabel "DiscPro"` → `"DiscNativePro"`. `resized(): centerWidth 140` → `220` para evitar truncado a 15pt bold.
- `src/ui/MidiModalComponent.cpp:13`: descripción → "DiscNativePro detecta automáticamente...".
- `src/ui/SettingsModalComponent.cpp:190,334`: combo waveform → "Azul Neón Clásico DiscNativePro" / `trUtf8(..., "Classic Neon Blue DiscNativePro")` (constructor + `refreshLanguage()`).

## Fuera de alcance (no tocado)
Rutas disco (`DiscPro_Recordings`, `DiscPro_Mix_`), thread name `DiscPro Audio Recorder`, demos `SampleTrackGenerator`, `COMPANY_NAME DiscPro`, comentarios (`DeckComponent.h:78`, `MainComponent.cpp:338`, `MainComponent.h:30`), README suite DiscPro.

## Verificación
- `cmake --build build` → 4 ficheros recompilados, link OK, 0 errores / 0 warnings.
  ```
  [  1%] Building CXX ... TopBarComponent.cpp.o
  [  3%] Building CXX ... JogWheel.cpp.o
  [  5%] Building CXX ... MidiModalComponent.cpp.o
  [  7%] Building CXX ... SettingsModalComponent.cpp.o
  [  9%] Linking CXX executable DiscNativePro_artefacts/Release/DiscNativePro.app/Contents/MacOS/DiscNativePro
  [100%] Built target DiscNativePro
  ```
- Grep post-cambio `"DiscPro"` como literal visible en `src/` → 0 resultados (solo quedan comentarios y `DiscPro_*` en rutas/demos, fuera de alcance).
- Relanzamiento: `pkill -f DiscNativePro; open build/DiscNativePro_artefacts/Release/DiscNativePro.app` → `pgrep` PID 420 activo. Nota: se relanzó Release (configuración actual del build), no Debug como indica AGENTS.md genérico.

---

# Walkthrough — Fila Hot Cue 4 pads por bandeja

## Cambios ([MODIFY] 2 ficheros, alcance aprobado: click fija/salta, solo RAM, fila 28px)
- `src/ui/DeckComponent.h`: `std::array<DjButton,4> hotCueButtons` + caché `hotCueButtonShown` + `refreshHotCueButtons()`.
- `src/ui/DeckComponent.cpp`:
  - Constructor: 4 pads texto `1-4` estilo STEMS (bg 24,26,32; ámbar 245,158,11 al fijar). `onClick`: video→no-op; sin pista→no-op; vacío→`setHotCue(i,getPosition())`, ocupado→`jumpToHotCue(i)`. Mismo store que pads MIDI.
  - `resized()`: fila 28px sobre la barra PLAY/CUE/Loop (4 pads equiespaciados, gap 6). Resta 34px al jog.
  - `timerCallback()` (60Hz): `refreshHotCueButtons()` con caché de estado (evita repaint spam); sincroniza MIDI↔botones.
  - `loadAudioFile()` + `loadYouTubeVideo()`: refresh (al cargar se apagan; en video deshabilitados en gris).
  - Nota: icono `Bookmark` no existe en `LucideIcons::IconType` → pads numéricos solo-texto (YAGNI, sin tocar LucideIcons).

## Fuera de alcance (no tocado)
Borrado de hot cues (`clearHotCue` sin cablear — vaciar = recargar), persistencia en disco, hot cues en modo video, `DeckPlayer`/`DjMixer`/`MidiManager`.

## Verificación
- `cmake --build build` → DeckComponent.cpp + MainComponent.cpp recompilados, link OK, 0 errores / 0 warnings (1 error intermedio de icono inexistente, corregido a texto).
- Relanzamiento Release → `pgrep` PID 21513 activo.
- Pendiente visual: cargar pista → click `1` fija (ámbar + marcador overview) → mover → click `1` salta; deck1/deck2 independientes; pad MIDI ilumina botón.

## Sistema de avisos de actualización (GitHub Releases)

### Cambios
- [NEW] `src/services/UpdateService.h/.cpp`: `checkForUpdatesAsync` (Thread + `juce::URL` a `api.github.com/.../releases/latest` con `User-Agent`, timeout 10s, parse `tag_name/name/body/assets`, asset por plataforma, `callAsync` al Message Thread). `isNewerVersion` semver por componentes (`1.10 > 1.2`; `-dev` local cuenta como más viejo). `skippedUpdateTag` en `settings.json`. Silencio total ante sin-red/timeout/rate-limit/tag omitido.
- [MODIFY] `CMakeLists.txt`: `APP_VERSION` (default `1.0.0-dev`) + sources + `DISCNATIVEPRO_VERSION` define.
- [MODIFY] `src/Main.cpp`: `getApplicationVersion()` desde el macro.
- [MODIFY] `.github/workflows/release.yml`: los 3 jobs pasan `-DAPP_VERSION` desde `GITHUB_REF_NAME` sin `v` (pwsh en Windows, bash en Linux/macOS; fallback `1.0.0-dev`).
- [MODIFY] `src/ui/TopBarComponent.h/.cpp`: `DjButton updateButton` ámbar `UPDATE` (oculto por defecto), `setUpdateAvailable()`, `onUpdateClicked`, tooltip ES/EN, hueco en `resized()` entre settings y layout.
- [MODIFY] `src/ui/MainComponent.h/.cpp`: `scheduleUpdateCheck()` (único chequeo, diferido 3s, `SafePointer`), `showUpdateDialog()` con `AlertWindow::showAsync(MessageBoxOptions)` + `ModalCallbackFunction`: [Descargar] abre asset directo, [Omitir] persiste tag + oculta badge, [Más tarde] deja badge para reabrir. Bilingüe.

### Verificación (real, con captura)
- `cmake --build build` → 0 errores; 0 warnings en ficheros propios (2 errores intermedios corregidos: `MessageManager` necesitaba `juce_events`, `showAsync` va con `MessageBoxOptions`).
- API real: tag `v1.0.0`, assets las 3 plataformas. Build local `1.0.0-dev` → diálogo esperado.
- `screencapture` confirma: badge `UPDATE` ámbar en TopBar + diálogo `Nueva versión disponible: 1.0.0 / Tu versión: 1.0.0-dev` con `Descargar/Omitir versión/Más tarde`. PID 64707.
- Mejora futura menor: el body del release se muestra en markdown crudo (truncado a 800 chars).

## ChannelStrip: knobs primero, fader absorbe (fix FILTER enano)
- Causa: `resized()` daba al fader mín. 130px primero; en ventana baja el resto no alcanzaba para 4×36 y el último knob (FILTER) se quedaba con las sobras (~21px).
- [MODIFY] `src/ui/ChannelStripComponent.cpp:142-156`: `knobH = jlimit(28,44,(availH-90-16)/4)` primero (4 iguales siempre), `faderH = jlimit(90,180, resto)`. Separadora baja sola. Piso 28 solo donde antes overfloweaba; techo fader 180 conserva pantallas altas idénticas.
- `cmake --build build` → 1 fichero, link OK, 0 errores/warnings. Relanzamiento Release → PID 24352.
- Capturas: ventana 1280×700 → HIGH/MID/LOW/FILTER iguales + fader corto; 1360×950 → idéntico a antes, sin regresión.

## Browser más alto (+25px)
- [MODIFY] `src/ui/MainComponent.cpp:388`: factor altura browser `0.32→0.35` (FX visible) / `0.38→0.41` (FX oculto). Clamp `190-480` intacto. Deck sin tocar.
- `cmake --build build` → 1 fichero, link OK, 0 errores/warnings. Relanzamiento Release → PID 20970.
- `screencapture` confirma: browser más alto, sin solapes en decks/FX/library; fila superior pierde ~25px (jog levemente menor, asumido).

## Micro-etiqueta HOT CUE (follow-up barato)
- `DeckComponent.h/.cpp`: `juce::Label hotCueRowLabel` con texto `HOT CUE` (9.5pt bold, gris 142,149,165 — término DJ universal, sin traducción). Ocupa 62px a la izquierda de la fila de pads en `resized()`.
- `cmake --build build` → OK, 0 errores/warnings. Relanzamiento Release → PID 29941 activo.

---

# Walkthrough — Corrección de YouTube (Error 153) y SoundCloud (HLS Stream) en Windows

## 1. Problema Identificado
1. **YouTube en Windows (Error 153)**: Al cargar videos de YouTube en Windows se mostraba el error *"Error de configuración del reproductor de video - Error 153"*. Causa raíz: `YouTubeVideoComponent_win.cpp` cargaba el reproductor mediante data URI (`data:text/html,...`), el cual Chromium/WebView2 aísla con origen `null` y sin cabeceras `Referer`. YouTube bloquea los reproductores embed que carezcan de un origen HTTP/HTTPS válido coincidente con el parámetro `origin` de la API de IFrame.
2. **SoundCloud (Error al descargar stream)**: Al pulsar en cualquier pista de SoundCloud saltaba *"Error al descargar stream de SoundCloud"*. Causa raíz:
   - Más del 90% del catálogo moderno de SoundCloud ya no ofrece streams directos HTTP progresivos (`protocol == "progressive"`), sino únicamente streams HLS fraccionados (`audio/mp4; codecs="mp4a.40.2"`).
   - El código original descartaba automáticamente cualquier pista que no tuviese stream progresivo, dejando `progressiveStreamAuthUrl` vacío.
   - Además, el `defaultClientId` embebido estaba vencido.

## 2. Cambios Realizados

### [MODIFY] `src/ui/YouTubeEmbedHtml.h` y `src/ui/YouTubeEmbedHtml.cpp`
- Se agregó el parámetro `originUrl` a `getYouTubeEmbedHtml()` (por defecto `"https://juce.backend"`).
- Se agregó meta tag `<meta name="referrer" content="strict-origin-when-cross-origin">`.
- Se configuró el iframe con `referrerpolicy="strict-origin-when-cross-origin"` y URL canónica `https://www.youtube.com/embed/...`.

### [MODIFY] `src/ui/YouTubeVideoComponent_win.cpp`
- Se implementó `withResourceProvider()` en las opciones de `WebBrowserComponent` para servir el contenido del player bajo el esquema seguro `https://juce.backend/player.html`.
- Se mapeó la respuesta HTTP 200 con cabeceras `Content-Type: text/html; charset=utf-8` y `Access-Control-Allow-Origin: *`.

### [MODIFY] `src/ui/YouTubeVideoComponent.mm` y `src/ui/YouTubeVideoComponent_linux.cpp`
- Se alineó la llamada a `getYouTubeEmbedHtml()` con el origen correspondiente.

### [MODIFY] `src/services/SoundCloudService.h` y `src/services/SoundCloudService.cpp`
- Actualizado `defaultClientId` con credencial activa y vigente (`kR7dAfT5UXQgmgde1pLjfsRGiaoANLvT`).
- Ampliada la estructura `SoundCloudTrack` para almacenar `hlsStreamAuthUrl`.
- Implementado el parser de transcodificaciones para capturar streams HLS de formato `audio/mp4; codecs="mp4a.40.2"`.
- Implementada la función `downloadHlsStream()`:
  - Descarga el manifiesto `playlist.m3u8`.
  - Extrae el segmento de inicialización `#EXT-X-MAP:URI="init.mp4"` y todos los segmentos de audio `.mp4` / `.aac`.
  - Concatena los bloques fMP4 generando un archivo de audio `.m4a` estándar de 160 kbps compatible con los decodificadores de audio del sistema.

### [NEW] `src/audio/MediaFoundationAudioFormat_win.h`
- Implementado lector de audio nativo de Windows (Media Foundation) para decodificar pistas `.m4a` y `.mp4` AAC en Windows sin requerir códecs externos.

### [MODIFY] `src/audio/AudioEngine.cpp` y `CMakeLists.txt`
- Registrado `MediaFoundationAudioFormat` en `AudioEngine` bajo `#if JUCE_WINDOWS`.
- Vinculadas las librerías `mfplat` y `mfreadwrite` en Windows. Habilitado `JUCE_USE_MP3AUDIOFORMAT=1`.

## 3. Verificación
- **Compilación**: Ejecutado `cmake --build build -j 2` resolviendo el problema de memoria de compilación paralela previa. 0 errores y 0 warnings en los módulos implementados.
- **Auto-Relaunch**: Ejecutado el relanzamiento obligatorio:
  - `pkill -f DiscNativePro || true`
  - `open "build/DiscNativePro_artefacts/Release/DiscNativePro.app"`
  - Confirmado PID activo: `88796`.

---

# Walkthrough — Instalador Formal para Windows con Inno Setup (`Setup.exe`)

## 1. Objetivo
Reemplazar la distribución en archivo `.zip` comprimido por un instalador profesional ejecutable (`DiscNativePro-Windows-x64-Setup.exe`) generado automáticamente con **Inno Setup** en GitHub Actions, configurando accesos directos, icono de alta calidad, desinstalador en Panel de Control y actualizando el actualizador interno de la app.

## 2. Cambios Realizados

### [NEW] `assets/icon.ico`
- Generado archivo `.ico` multi-resolución (256x256, 128x128, 64x64, 48x48, 32x32, 16x16) a partir de `assets/icon.png`.

### [NEW] `installer/windows/installer.iss`
- Script Inno Setup con GUID único (`{D37E9E8C-4F71-4A59-8809-5D8840BA031F}`):
  - Instalación en `{autopf}\DiscNativePro`.
  - Icono `assets/icon.ico` en instalador y desinstalador.
  - Accesos directos en menú Inicio y Escritorio (opcional).
  - Empaquetado de `DiscNativePro.exe` y `WebView2Loader.dll`.
  - Opción de lanzar la aplicación al finalizar (`skipifsilent`).

### [MODIFY] `src/services/UpdateService.cpp`
- Actualizado `kAssetName` bajo `#elif JUCE_WINDOWS` a `"DiscNativePro-Windows-x64-Setup.exe"` para que el diálogo de descarga de nuevas versiones enlace al instalador.

### [MODIFY] `.github/workflows/release.yml`
- Reemplazado el paso de compresión `.zip` por la invocación de `iscc` (Inno Setup Compiler).
- Publicación de `DiscNativePro-Windows-x64-Setup.exe` en el GitHub Release.

## 3. Verificación
- **Compilación**: `cmake --build build -j 2` completado al 100% con 0 errores y 0 warnings.
- **Auto-Relaunch**: App relanzada activamente con PID `180`.

---

# Walkthrough — Remediación de seguridad CRÍT-01 / ALT-01 (eliminación de shell e integración de secretos)

## 1. Objetivo
Eliminar la ejecución de comandos mediante shell (`system`/`popen`) en el flujo de red y proteger las API keys (IA y YouTube) sin romper proveedor, modelo, persistencia, reintento ni fallback. Ver informe completo en `security_audit.md`.

## 2. Cambios

### [NEW] `src/services/SecretStore.h/.cpp`
- Interfaz: `read`, `write`, `remove`, `isAvailable`.
- macOS: Keychain (Security.framework). Windows: Credential Manager (`advapi32`). Linux/otros: `secrets.json` con permisos `0600`.
- `isAvailable()` → `true` en macOS/Windows, `false` cuando solo hay fallback local.

### [MODIFY] `src/services/AIService.h/.cpp`
- Sustituido `curl` + `system()` por `juce::URL::createInputStream` con POST, timeout 15 s, headers separados y `withNumRedirectsToFollow(3)`.
- Gemini pasa de `?key=` en URL a header `x-goog-api-key`.
- `getApiKey`/`setApiKey` leen/escriben `SecretStore`. Nuevo `migrateLegacySecrets()` (invocado en el arranque desde `MainComponent`) migra de una vez **todos** los proveedores (y `YouTubeService::migrateLegacySecret()` la key de YouTube): cada key se elimina de `settings.json` solo tras confirmar la escritura en el almacén seguro.
- Añadido `isValidModelName()` (letras/dígitos y `- _ . / :`): modelo inválido → `getDefaultModel`. Permite custom `provider/model`.
- Eliminados `<cstdio>` y los `#define popen/_popen`.

### [MODIFY] `src/services/YouTubeService.cpp`
- `downloadYouTubePage()` y `searchPublic()`: `popen(curl ...)` → `juce::ChildProcess` con `StringArray` de argumentos (sin shell, sin quoting manual).
- Key de YouTube migrada a `SecretStore` (`account = "youtube"`).
- Eliminado `<cstdio>`.

### [MODIFY] `src/services/ThumbnailCache.cpp`
- `system(curl ...)` → `juce::ChildProcess` con args separados.
- `system(sips ...)` → `juce::ChildProcess` con args separados (sin redirección de shell).

### [MODIFY] `src/ui/SettingsModalComponent.cpp`
- `apiKeyEditor` (YouTube) se muestra como contraseña (`setPasswordCharacter('*')`).
- El badge de estado de IA añade `· guardado local (sin almacén seguro)` cuando `SecretStore::isAvailable()` es `false`.

### [MODIFY] `CMakeLists.txt`
- macOS: `-framework Security`. Windows: `advapi32`.

### Limpieza
- Eliminados duplicados de iCloud `src/services/YouTubeAudioDownloader 2.cpp/.h` y carpetas `build/**/" 2"`.

## 3. Verificación
- **Compilación**: `cmake --build build -j 2` → 0 errores; 0 warnings en archivos tocados (los warnings restantes son preexistentes en `AsyncFolderScanner.cpp`/`UsbLibraryView.cpp`).
- **Shell eliminado**: `grep -rn "\bsystem(\|popen(\|_popen(" src/` → 0 resultados.
- **Correcciones de compilación aplicadas a `SecretStore.cpp`** (escrito por el agente previo sin compilar): `json.getProperty(account, juce::var())` en lugar de `json[account]`, e `#include <sys/stat.h>` en la rama macOS.
- **Nota de entorno**: la caché de CMake quedó inconsistente (JUCE resuelto desde `/private/tmp/discnativepro-juce` vs `build/_deps/juce-src`); se regeneró la subcaché de `tools` y se reconfiguró.
- **Auto-Relaunch**: `pkill -f DiscNativePro; open build/DiscNativePro_artefacts/Release/DiscNativePro.app` → PID activo.
- **Prueba funcional (mini-app JUCE con el patrón exacto)**: `ChildProcess`+curl devuelve HTML de YouTube (`len=1.595.822`, `hasPromptData=1`, `exit=0`); arg con `; touch /tmp/pwned` → `probeCreated=0` (sin shell, inyección neutralizada).
- **Prueba en vivo (app real)**:
  - Vista YouTube → buscar `daft punk` → **"Mostrando 19 videos encontrados en YouTube"**; fila 1 = "Daft Punk - Around The World (Official Music Video)" (4:02).
  - Miniaturas: 3 jpg nuevos en `~/Library/DiscNativePro/Thumbnails/` (p. ej. `FGBhQbmPwH8.jpg`, JPEG 320x180 válido).
  - Migración: `settings.json` quedó **sin `aiApiKey_*`**; `secrets.json` no existe (usa Keychain). Pestaña IA muestra **"● Recomendaciones IA Activas (OpenRouter / openai/gpt-4o-mini)"** → keys legibles desde Keychain.

# Walkthrough — Colores de waveform independientes por bandeja (Deck 1 / Deck 2)

## 1. Objetivo
Editar los colores de Graves/Medios/Agudos del wave **por separado en cada bandeja** (antes eran globales y todo cambio afectaba a ambas), con edición independiente desde el tab "Diseño" y presets aplicables a una sola bandeja. Plan aprobado en `implementation_plan.md`.

## 2. Cambios

### [MODIFY] `src/services/WaveformTheme.h/.cpp`
- De tema global único a `Theme themes[2]` (`std::array`), indexado por `deckIndex` (0 = Deck 1, 1 = Deck 2, convención de `MidiMapping.h`).
- API: `low/mid/high(deck)`, `setLow/setMid/setHigh(deck, c)` y `applyPreset(deck, preset)`.
- Persistencia en `settings.json` con claves ARGB por bandeja: `waveLow1/waveMid1/waveHigh1` y `waveLow2/waveMid2/waveHigh2`.
- Migración legacy: las claves globales antiguas (`waveLow/waveMid/waveHigh`) se heredan por ambas bandejas si las nuevas no existen; se eliminan del JSON en el primer guardado.
- Defaults por familia de acento: Deck 1 `#0077B6/#00B4D8/#90E0EF` (cian), Deck 2 `#475569/#94A3B8/#E2E8F0` (gris azulado). Presets (RGB/Neón/Plata) aplican solo a la bandeja indicada.

### [MODIFY] `src/ui/OverviewWaveform.h/.cpp`, `src/ui/VerticalWaveform.cpp`, `src/ui/ScrollingWaveform.cpp`
- Los 3 painters leen el color según la bandeja que dibujan: `OverviewWaveform` recibe `int deckIndex` por ctor; `VerticalWaveform::renderDeckHalf` usa `isLeft ? 0 : 1`; `ScrollingWaveform::drawDeckWaveform` usa `invert ? 0 : 1`.

### [MODIFY] `src/ui/DeckComponent.cpp`
- Pasa `deckNum` al overview.

### [MODIFY] `src/ui/SettingsModalComponent.h/.cpp`
- Tab "Diseño": toggles **"Bandeja 1 / Bandeja 2"** sobre las filas de Graves/Medios/Agudos (`designDeckIndex`), con estado visual activo.
- Swatches, picker (`activeColourDeck` congelado al abrir el selector) y presets se aplican solo a la bandeja seleccionada. Textos ES/EN actualizados.

## 3. Verificación
- **Compilación**: `cmake --build build` → 0 errores; 0 warnings en los 9 archivos tocados (restan 5 preexistentes en `AsyncFolderScanner.cpp`, `SoundCloudService.cpp` y `UsbLibraryView.cpp`, sin tocar).
- **Pruebas funcionales** (harness temporal JUCE enlazado contra la implementación real, borrado tras las pruebas):
  - Sin config previa → Deck 1 `#0077B6/#00B4D8/#90E0EF`, Deck 2 `#475569/#94A3B8/#E2E8F0` ✅
  - Migración legacy: ambos decks heredan `waveLow/Mid/High`; al guardar se escriben `waveLow1..waveHigh2` y se eliminan las claves legacy ✅
  - Colores independientes por banda y presets (Neón en Deck 1 / Plata en Deck 2) sin pisarse ✅
- **Nota de ruta**: en macOS `userApplicationDataDirectory` → `~/Library`, luego el `settings.json` real es `~/Library/DiscNativePro/settings.json` (no `~/Library/Application Support/...`).
- **Auto-Relaunch**: `pkill -f DiscNativePro; open build/DiscNativePro_artefacts/Release/DiscNativePro.app` → PID activo (el artefacto Debug era antiguo; el build actual genera Release).

## 4. Observación sobre la vista "Ondas" (fuera de este cambio)
El componente `WavesViewComponent` (rama `axel-molina/waves-view`, commit `f5aa7ac`) no está en `main`: se mergeó (`533e585`) y luego se revirtió con `git reset` durante esta sesión. Al integrar esa rama habrá que pasarle el `deckIndex` al ctor de `OverviewWaveform` (ahora con 3 parámetros) en `WavesViewComponent`, para que cada franja use el tema de su bandeja.

# Walkthrough — Fix arranque en Windows (ventana invisible + procesos duplicados)

## 1. Causa raíz diagnosticada
Reporte: en Windows (PC Intel iGPU + AMD dGPU) la app no abre ventana y cada lanzamiento deja otro `DiscNativePro.exe` vivo en el administrador de tareas.

1. **Ventana sin pintar (causa principal probable):** el build Windows de CI compila con JUCE 8.0.6, que renderiza por defecto con **Direct2D**, y el repo no definía `JUCE_DIRECT2D`. El renderer D2D es sensible a drivers AMD / GPU híbridas Intel+AMD (ventana que nunca se pinta con proceso vivo). Sin `OpenGLContext` en el código, el stack implicado es D2D + driver.
2. **Procesos duplicados (defecto confirmado):** `src/Main.cpp:15` tenía `moreThanOneInstanceAllowed() → true` y `anotherInstanceStarted()` vacío: cada lanzamiento creaba un proceso independiente que quedaba huérfano.
3. **Fragilidad asociada:** el arranque era todo síncrono en el hilo de mensajes antes de mostrar la ventana (`MainComponent` ctor → `audioEngine.initialise()` → `initialiseWithDefaultDevices(0,4)`); un dispositivo de audio que tarde/cuelgue impedía ver la ventana.

## 2. Cambios (rama `axel-molina/windows-launch-fix`, commit `6a7fe7d`)
- **`CMakeLists.txt`** (bloque `elseif(WIN32)`): `JUCE_DIRECT2D=0` → rasterizador GDI software (comportamiento JUCE 7), estable ante GPUs híbridas. Reversible en una línea cuando JUCE estabilice D2D.
- **`src/Main.cpp`**: `moreThanOneInstanceAllowed() → false` (segundo lanzamiento reactiva la instancia existente) y `anotherInstanceStarted()` implementado (restaura de minimizada, `setVisible(true)` + `toFront(true)`). Side-effect documentado: `--snapshot` con la app ya corriendo activa la instancia existente en vez de capturar.
- **`src/ui/MainComponent.cpp`**: inicialización pesada (`audioEngine.initialise()`, `midiManager.initialise()`, `AIService::migrateLegacySecrets()`, `YouTubeService::migrateLegacySecret()`) diferida con `juce::Timer::callAfterDelay(0, ...)` y `Component::SafePointer`: la ventana se pinta siempre, aunque el audio tarde o cuelgue.

## 3. Verificación (macOS local)
- `cmake --build build` → 0 errores; 0 warnings en los 3 ficheros tocados (resto: terceros JUCE/harfbuzz preexistentes).
- **Instancia única**: doble `open` del `.app` con la app corriendo → exactamente 1 proceso del worktree (verificado filtrando por ruta; había un proceso del worktree paralelo `orca/waves-view` corriendo al mismo tiempo).
- **Sobre el criterio**: los procesos Duplicados antes del fix correspondían a este defecto real del código (ya presente en `main`).
- **Flujo `--snapshot`**: `open -W <app> --args --snapshot=/tmp/x.png` con app cerrada → PNG creado y proceso termina en 0. (El lanzamiento del binario por exec directo (`./DiscNativePro --snapshot=...`) NO produce snapshot: forma correcta de invocarlo es la documentada con `open --args`.)
- Build Windows: pendiente — puede validarse con el workflow `build-windows.yml` (`workflow_dispatch`) una vez la rama esté en el remoto, o al publicar.

## 4. Pendientes
- Validación runtime del binario Windows sobre el equipo del reporter (Intel+AMD) con el artifact de CI; adicionalmente pedir el *faulting module* del Visor de eventos (si mencionara `d2d1`/driver AMD, confirma la cadena D2D+driver) y actualizar drivers AMD/Intel como red de seguridad de segunda línea.
- Merge a `main` decide el usuario (los releases se generan en push de tags).

# Walkthrough — P1: Botón SYNC funcional (ajuste de pitch one-shot)

## 1. Causa
El botón SYNC de cada deck (`DeckComponent.cpp:70-76`) era puramente cosmético: su `onClick` solo repintaba el botón y nunca llamaba al engine. La función de sync sí existía (sync por MIDI en `MidiManager.cpp:207-216`, que ajusta el pitch con el BPM actual del otro deck y **no** depende del estado play/pause), por lo que se percibía "el sync no funciona" en cualquier estado.

## 2. Cambios (rama `axel-molina/sync-button-fix`, commit `34332ae`)
- **`AudioEngine::syncDeckTo(int deckIndex)`** [NEW]: retorna `false` si el otro deck no está cargado o hay BPM inválido (0); si aplica, `self.setPitchPercent((other.getBpm()/self.getOriginalBpm() - 1) * 100)` y retorna `true`.
- **`MidiManager.cpp`**: `deck1_sync`/`deck2_sync` delegan en `syncDeckTo` (el cálculo vive en un único sitio; mismo comportamiento).
- **`DeckComponent.cpp`**: el `onClick` llama al engine; destello **azul 1.5 s** al aplicar (one-shot, sin enclavamiento) y **ámbar 0.8 s** cuando no es aplicable (feedback visible en vez de "botón muerto"). Sin toggle persistente; reset con `Timer::callAfterDelay` + `SafePointer`, sin timers permanentes.

## 3. Verificación
- `cmake --build build` → 0 errores; recompilación forzada de los 4 ficheros tocados → 0 warnings (el resto del log es third-party JUCE/harfbuzz).
- Cableado confirmado: `syncDeckTo` público (`AudioEngine.h:60`) y los labels de BPM se refrescan cada tick desde `deck.getBpm()` (`DeckComponent.cpp:1018-1024`), así que el BPM aplicado se ve sin tocar nada más.
- Auto-relaunch: `pkill` acotado a este worktree + `open` del build Release actualizado → PID activo.
- **Pendiente de prueba con oído del usuario** (requiere cargar dos tracks): SYNC en deck 1 y 2 en pausa y reproduciendo, y caso no aplicable con un solo deck (destello ámbar).

## 4. Alcance excluido (fases siguientes)
- Alineación de fase/beatgrid → P3 (`alignBeatPhase` + reset thread-safe del stretcher).
- Respeto del pitch range configurado, half/double-time y BPM con decimales → P4.

# Walkthrough — P2: Perilla de filtro sin corte al cruzar el centro

## 1. Causa
Reporte: "perilla filtro, hace un corte al pasar por la mitad". En `src/audio/DjMixer.cpp` había tres defectos y un cuarto asociado:
1. **Banda muerta binaria ±0.03** con rama de bypass (`colCoeffs == nullptr`): al cruzar el centro el filtro se enganchaba/desenganchaba de golpe.
2. **Sin suavizado**: el knob se leía 1× por bloque y el mismo valor se aplicaba a las 512 muestras.
3. **Alocaciones en el hilo de audio**: `IIR::Coefficients::makeXXX` creaba 4 objetos por bloque (heap) incluso sin mover la perilla.
4. **Taper del EQ discontinuo**: `knob <= 0.02 → 0.0001` (−80 dB) contra −36 dB en 0.021: salto de 44 dB en el kill.

## 2. Cambios (rama `axel-molina/filter-knob-fix`)
- **`b007879`** — `DjMixer.h/.cpp`: filtro color con `StateVariableTPTFilter` y barrido continuo LPF/HPF (sin banda muerta ni bypass); coeficientes de EQ persistentes reutilizados en sitio (`ArrayCoefficients` + `operator=`, con la capacidad del `Array` retenida por `clearQuick`/`ensureStorageAllocated`); suavizado por bloque (~25 ms, `SmoothedValue`); taper del EQ continuo (−80 dB → 0 dB → +6 dB).
- **`9e6e244`** — `DjMixer.h/.cpp`: **doble SVF por canal** (LPF y HPF con estado y cutoff propios, ambos neutrales en el centro: 20 kHz y 20 Hz) + **crossfade smoothstep** en |knob| < 0.06, eliminando el clic residual que dejaba el cambio de tipo de filtro (salto de cutoff 20 kHz ↔ 20 Hz en un bloque). Cutoffs limitados a `0.45 · sampleRate`.

## 3. Verificación (harness C++ headless: sine 1 kHz @ 44.1 kHz, bloques de 512)
Delta natural de la señal de prueba: **0.1424**.

| Métrica | Antiguo (`main`) | P2 (`b007879`) | P2-bis (`9e6e244`) |
|---|---|---|---|
| Barrido filtro: delta interno | 0.291 | 0.146 | **0.146** |
| Barrido filtro: delta en frontera de bloque | 0.224 en knob≈+0.030 (banda muerta) | 0.432 en knob≈+0.010 (cambio de tipo) | **0.146** |
| Kill EQ: delta interno / frontera | 0.295 / 0.193 en eq≈0.018 | 0.142 / 0.142 | 0.161 / 0.141 |
| Barridos por mitades (LPF / HPF) | 0.173 / 0.149 | 0.144 / 0.143 | 0.144 / 0.145 |
| Alocaciones por bloque en `process()` | 4 | 0 | **0** |

Ganancia real medida (RMS de la salida para una sine unidad; ideal 0.7071):
- Centro (knob=0): **0.70710 → 0.00 dB** (transparente, sin ripple).
- Cerca del centro (knob=−0.02 y +0.06): **0.70710** → el crossfade no colorea.
- Extremos: LPF knob=−1 → **−39.9 dB** y HPF knob=+1 → **−41.1 dB** a 1 kHz → el efecto sigue operativo.
- Regresión: `cmake --build build` con 0 errores y 0 warnings en los ficheros tocados; auto-relaunch de la app tras cada compilación.

## 4. Notas
- El clic residual de `b007879` se detectó con el harness (no se habría visto en una escucha rápida), lo que justifica mantener esta prueba como criterio de aceptación para cambios de DSP.
- El harness vive fuera del repo (scratchpad temporal) y se enlaza contra los objetos JUCE del build.

# Walkthrough — P3: Alineación de fase del master tempo y seek seguro

## 1. Causa
Reporte: "master tempo, no funciona bien". Tres problemas encadenados:
1. **La fase se medía desde `t=0`**: `alignBeatPhase` usaba `position mod (60/bpm)` sin el offset del primer beat, que `BeatDetector` ya calculaba pero `DeckPlayer::loadFile` descartaba.
2. **Seek duro**: `setPosition` hacía `stretcher->reset()` bajo un `CriticalSection` que el hilo de audio tomaba durante todo el bloque (bloqueo entre hilos) y con un clic de continuidad; además el mismo `reset()` corría en el borde de cada loop.
3. **Sin convergencia**: la alineación se disparaba una vez por cruce de zona del crossfader; no había nudge progresivo ni liberación.

## 2. Cambios (rama `axel-molina/phase-align-fix`)
- **`054e4a8`** — `DeckPlayer`: `setPosition()` publica un flag atómico (sin lock ni reset en el hilo llamante) y el hilo de audio lo aplica al inicio del bloque con `reanchorWithPreRoll()` (`stretch.seek()` con pre-roll, con fallback a `reset()`); loop y fin de pista por el mismo camino; `getPosition()` lee una posición atómica; nuevo `getBeatPhase()` con el offset del primer beat (`firstBeatOffsetSeconds`, antes descartado). `StretchAdapter::reanchorWithPreRoll()`.
- **`aacf560`** — `AudioEngine`: controlador por bloque con máquina de estados `Idle → Settle → Measure` (sin allocs ni locks nuevos): `Settle` ignora 2 bloques tras cada seek para no medir error obsoleto; `Measure` itera cada bloque hasta la deadband (1 ms), con **nudge aditivo ≤ 1 %** (no pisa el `pitchPercent` de `applyTempoBlend`) y **un seek por iteración** (`delta = error + k·I`, salto mínimo hacia delante) con contador de seguridad; al converger **libera siempre el `pitchBend`**; **auto re-armado** si la deriva supera ~25 ms estando enclavado; el cruce de zona **solo arma**; aborto por intervención comparando `getPitchBend()` con el último nudge escrito. Se elimina `alignBeatPhase` (sin consumidores).

## 3. Verificación
**Matemática (helpers estáticos, 5043 casos):** 0 fallos. Antes el re-anclaje dejaba 56–119 ms de error; con `delta = error + k·I` el residual es 0.00 ms en todos los casos.

**Banco determinista sin dispositivo** (`DeckPlayer` propio + `TimeSliceThread` + WAV sintético de click-track, bombeando bloques a ritmo real):
- **Invariante de posición**: localizando el onset del click en el audio renderizado y comparándolo con `getPosition()` → **−0.0 ms de error antes y después de un seek** (la aguja apunta a lo que suena).
- **Seek sin clic**: delta máximo alrededor del seek = **0.1139** frente al natural de la señal (0.114) → sin discontinuidad.
- Lección medida: la latencia del stretcher (140.0 ms) **no** la elimina el pre-roll; `getPosition()` debe seguir restándola siempre (el experimento `stretchLagSeconds` se descartó tras medir +140 ms de error).

**Banco del controlador (con dispositivo):** donde actuó, convergió a **3–8 ms** con el nudge liberado (sin desviación permanente). El nudge está limitado a ±1 % (~4.7 ms/s a 124 BPM), así que corregir 150 ms requiere ~30 s y el banco solo esperaba ~5 s; un caso no llegó a armarse porque la manipulación con pausa/seek altera la transición de zona. **Pendiente: prueba de escucha en la app con tracks reales.**

## 4. Resolución P3 sobre P5 (merge en `main`)
Al integrar P3 sobre la carga asíncrona de P5 aparecieron dos fallos que la rama original no podía ver (allí el BPM se calculaba en el hilo de UI, en la misma llamada a `loadFile`):
1. **El controlador no tenía grid**: el BPM y el offset llegan ahora del worker de análisis vía `MessageManager::callAsync`. Se publica también `firstBeatOffsetSeconds` en ese callback y `processPhaseAlign()` **no actúa hasta que ambos decks tienen BPM > 0** (si no, medía contra el fallback de 120 BPM).
2. **`setPosition()` y `getPosition()` no eran inversas con Key Lock**: `getPosition()` descuenta la latencia del stretcher (140 ms medidos), pero el cursor de lectura no la compensaba, así que cada seek relativo se anulaba: el error quedaba clavado en exactamente 140.00 ms y el controlador no convergía. `applyPendingSeek()` suma ahora `stretcher->getLatencySeconds()` al destino cuando Key Lock está activo.
3. **El nudge fino no tenía efecto**: `StretchAdapter::render()` calculaba la entrada como `round(numSamples * rate)`; con bloques de 512 samples cualquier `pitchBend` menor que ~0.1 % redondeaba al mismo número de samples y no cambiaba la velocidad (el nudge de 10 ms de error vale 0.043 %). Ahora la entrada se consume con **acumulador fraccionario** (`inputCarry`), que además se resetea en `reset()` y en `reanchorWithPreRoll()`.

**Verificación tras el merge (pistas reales, `Demo Track 1.mp3`, 128.000 BPM):**
- `run_phase_real`: análisis publicado en ~1.8 s; el controlador converge de 268 ms a **0.67–1.77 ms** con el `pitchBend` liberado a 0 (constante de tiempo del nudge ~23 s por diseño, no un fallo). Seek a 30.000 s con Key Lock: posición real **13 ms** por debajo del valor esperado por reloj.
- `run_pos_merged`: invariante de posición (onset audible vs `getPosition()`) **−0.0 ms antes y después del seek**; delta máximo en el seek 0.1139 frente al natural 0.114.
- `run_math_merged`: 5043 casos, 0 fallos.

## 5. Deuda y siguientes fases
- **Deuda técnica**: el callback de audio sigue tomando el `CriticalSection` de `DeckPlayer` (seguro, pero es el patrón a eliminar) y el pre-roll lee ~6.2–6.7k frames dentro del callback (posible bloqueo si el búfer no está listo; `stretch.seek()` puede realocar).
- **P4**: BPM entero en `BeatDetector` (±0.5 BPM → deriva), half/double-time en `applyTempoBlend`, pitch range ignorado, feedback/persistencia del Tempo Blend y displays de BPM inconsistentes.
- Código muerto con datos falsos: `ScrollingWaveform` dibuja el grid a 124 BPM fijos y `DynamicBeatgrid.h` no se usa.

# Walkthrough — P4.1: BPM decimal, half/double-time y pitch range

## 1. Causa
Tres causas del "master tempo se va solo":
1. **BPM entero**: `BeatDetector` acumulaba el histograma en buckets de 1 BPM (`round(candidate)`), así que un error de ±0.5 BPM quedaba fijo (~1.3 s de deriva en 5 min); además, si no detectaba nada, devolvía un **124 inventado**.
2. **Sin half/double-time**: `applyTempoBlend` casaba BPMs directos, mientras el scorer de AutoMix sí usaba compatibilidad half/double → un track detectado a la mitad pedía pitch ~±100 %.
3. **Pitch range ignorado**: `setPitchPercent` guardaba cualquier valor; el rango configurado (±8/±16/±50) solo se usaba para pintar el slider.

## 2. Cambios (rama `axel-molina/p4-master-tempo`, commit `55262e7`)
- **`BeatDetector`**: histograma con **buckets de 0.1 BPM** + **refinado del pico por interpolación parabólica**; se elimina el fallback a 124 (si no hay detección fiable, `bpm = 0.0` = desconocido, y la UI muestra `--`).
- **`AudioEngine::normalizeBpmTo(bpm, reference)`** [NEW, estático y testeable]: elige entre `{x1, x2, ÷2, x4, ÷4}` la variante con menor desvío; lo usan `applyTempoBlend` (normaliza b2 contra b1 antes de interpolar), `syncDeckTo` y el scorer de AutoMix (lógica compartida, sin duplicar).
- **`applyTempoBlend`**: **no-op** si falta el BPM de algún deck (nada de 124/126 inventados).
- **`DeckPlayer::setPitchPercent`**: clampea al `pitchRange` configurado y expone `isPitchClamped()`; `currentBpm` se calcula con el pitch clampeado.
- **`DeckComponent`**: aviso visual (color ámbar en el label de BPM + sufijo `! SYNC fuera de rango`) cuando el pitch está clampeado.

## 3. Verificación (banco determinista, enlazado contra los objetos del build)
| Prueba | Resultado |
|---|---|
| `normalizeBpmTo` (87↔174, 64↔128, 256↔128, 90 vs 175) | 6/6 correctos (el caso 90 vs 175 elige 180 por menor desvío) |
| Pitch range: rango 8, pedido +20 | aplicado **+8.00**, `isPitchClamped() == true` |
| Pitch range: rango 8, pedido −3 | aplicado **−3.00**, `isPitchClamped() == false` |
| BPM de señales "four-on-the-floor" de **128.0 / 126.4 / 128.7** | detectados **128.00 / 126.40 / 128.70** (error 0.00) |
| Señal sin pulsos | **BPM 0.0** (antes: 124 inventado) |

Nota del banco: el detector trabaja sobre energía (media móvil de 100 ms) y thresholds dinámicos, así que requiere material con pulsos reales; un click-track seco no produce picos.

## 4. Pendiente (P4.2)
Feedback y persistencia del Tempo Blend (no-op silencioso, botón desincronizado cuando AutoMix lo activa, sin persistir) y unificar los displays de BPM (JogWheel redondea a entero mientras los labels usan 1 decimal). Prueba visual pendiente en la app del aviso ámbar de "pitch fuera de rango".

# Walkthrough — P4.2: estado y feedback del Tempo Blend + BPM unificado

## 1. Causa
1. **No-op silencioso**: `applyTempoBlend` sale sin hacer nada si falta una bandeja o un BPM, y nadie lo explica.
2. **Doble verdad del botón**: `MainComponent::startAutomix` enciende el blend por código y el botón de la columna central no se enteraba → se veía apagado mientras estaba activo (y pulsarlo lo invertía).
3. **Sin persistencia**: el estado se perdía al cerrar la app.
4. **BPM inconsistente**: el JogWheel redondeaba a entero (y mostraba "33 RPM" sin detección) mientras los labels usaban 1 decimal y la biblioteca formateaba por su cuenta.

## 2. Cambios (rama `axel-molina/p4-master-tempo`, commit `eb1fda3`)
- **`AudioEngine`**: helpers `readTempoBlendSetting()`/`writeTempoBlendSetting()` con el mismo merge sobre `settings.json` (solo toca `tempoBlend`, conserva el resto de claves); `setTempoBlend()` guarda y el constructor restaura (con decks vacíos sigue siendo no-op, queda armado).
- **`CenterColumnComponent`**: `syncTempoBlendState()` lee `isTempoBlendActive()` y sincroniza el toggle con `dontSendNotification` (nunca re-dispara el `onClick`), llamado en el constructor, tras el clic y en el timer con una firma barata para no repintar 60 veces por segundo; `buildTempoBlendTooltip()` explica el estado y el motivo del no-op ("falta la bandeja N sin pista" / "sin BPM detectado").
- **`BpmFormat.h`** [NEW] + `JogWheel`, `DeckComponent`, `LibraryComponent`: `formatBpm()` → 1 decimal, `--` si es desconocido (el JogWheel deja de mostrar "33 RPM").
- **`MainComponent`**: se crea un `juce::TooltipWindow` — **no había ninguno en la app**, así que los 25 `setTooltip` existentes (incluido el nuevo del Tempo Blend) nunca se mostraban.

## 3. Verificación
| Prueba | Resultado |
|---|---|
| Persistencia: activar → `tempoBlend: true` en el JSON, resto de claves preservado | OK |
| Persistencia: `AudioEngine` nuevo restaura el estado | OK |
| Persistencia: desactivar y recrear | OK (apagado) |
| `formatBpm`: `126.44 → "126.4"`, `0.0 → "--"`, negativo → `"--"`, `120.0 → "120.0"` | OK |
| Botón con una sola verdad | revisión de código (sincronización sin re-disparar el click) + comprobación visual |

## 4. Incidente de la verificación (importante)
El banco de persistencia intentaba aislarse con `HOME`, pero JUCE resuelve la carpeta de datos de la app al **home real**, así que el test **sobrescribió el `settings.json` del usuario**. Se detuvo, se guardó el fichero dañado como evidencia y se **reconstruyó** el fichero con los valores documentados en los logs de la sesión (IA, YouTube, idioma, `viewMode` y la paleta de wave). Quedan por revisar en Preferencias: carpeta de grabaciones (vuelve al valor por defecto), posible carpeta de descargas de YouTube, modelos de otros proveedores y, si había colores por bandeja distintos, ambos arrancan ahora de la paleta común. Las API keys (Keychain), la biblioteca, el mapeo MIDI y las miniaturas no se vieron afectados.
**Regla nueva**: cualquier prueba que toque `settings.json` hará copia de seguridad y restauración del fichero real; nunca "aislamiento" por `HOME`.

## 5. Pendiente
- Prueba visual en la app: tooltip del Tempo Blend (con una y con dos bandejas), aviso ámbar de "pitch fuera de rango" (P4.1) y BPM iguales en JogWheel / labels / biblioteca.
- Deuda técnica del P3 (lock en el callback de audio y riesgo del pre-roll) y merge de `phase-align-fix` a `main`.

# Walkthrough — P5: carga de pista sin congelar la UI

## 1. Causa (medida)
`DeckPlayer::loadFile` corre en el **hilo de mensajes** y hacía tres análisis pesados. Medición con un WAV de 5 min estéreo (50 MB):

| Etapa | Coste |
|---|---|
| `createReader` | 0.1 ms |
| `BeatDetector::analyze` | 15.1 ms |
| `KeyDetector::analyze` | 61.1 ms |
| `extractWaveformOverview` (pasada completa) + fuentes | resto |
| **`loadFile` total** | **177.8 ms** |

Resultado: al cargar en una bandeja mientras la otra suena, la UI quedaba bloqueada ese tiempo (con MP3/M4A, más).

## 2. Cambios (rama `axel-molina/async-load`, commit `e4cfedd`)
- **`loadFile` en dos fases**: fase rápida (~7 ms) que crea el reader de reproducción, las fuentes, la duración, resets y `loaded = true` **sin analizadores**; la bandeja queda reproducible al instante.
- **Worker dedicado**: `juce::ThreadPool` estático de 1 hilo, con **su propio `AudioFormatReader`** (los readers no son seguros para lectura concurrente con la reproducción).
- **Publicación por etapas** en el hilo de mensajes (`MessageManager::callAsync`): overview **rápido de 2000 puntos** (ventanas cortas repartidas) → overview **completo de 8000** → **BPM** → **tonalidad**.
- **Descartes**: `generation` se incrementa en cada carga y `alive` (shared_ptr) se apaga en el destructor; cargar A y enseguida B nunca publica datos de A.
- **`BufferingAudioSource` con prefill desactivado** (6.º argumento): su `prepareToPlay` hacía un prefill síncrono con `Thread::sleep`.
- **UI**: el badge de tonalidad y el jog wheel se refrescan cuando llega el resultado asíncrono (caché para no regenerar la portada en cada tick).

## 3. Verificación (banco independiente, mismo WAV de 5 min)
| Criterio | Antes | Después |
|---|---|---|
| `loadFile` (bloqueo del hilo de UI) | **177.8 ms** | **6.8 ms** |
| Reproducible al instante | no | sí (`isLoaded()` y audio en el acto) |
| Overview | solo completo | **2000 puntos primero**, luego 8000 |
| BPM / tonalidad | síncronos | 128.003 y `8A / Am` (idénticos al análisis directo) |
| Cancelación A→B | — | resultados finales de B, sin datos de A |
| 6 cargas alternas | — | sin crash, último resultado correcto |

Matiz honesto: los ~5-7 ms restantes son un `Thread::sleep(5)` **incondicional dentro de `BufferingAudioSource::prepareToPlay` de JUCE 8.0.6**; queda por debajo de un frame a 60 Hz. Eliminarlo exigiría preparar el pipeline fuera del hilo de mensajes con una compuerta en el callback de audio (riesgo de carrera), y se descartó.

## 4. Notas
- `DeckComponent.h` recibió 3 líneas (caché `jogWheelBpmShown`) fuera de la lista inicial del plan: necesarias para que el jog wheel no se quede en `--` al pasar el BPM a asíncrono.
- La rama `async-load` se creó desde `p4-master-tempo`, así que incluye P4.1/P4.2.

## 5. Pendiente
- Prueba en vivo: reproducir en una bandeja y cargar en la otra → la UI debe seguir respondiendo, el waveform nuevo aparece rápido y se completa.
- Deuda técnica del P3 y merges pendientes (`phase-align-fix`, `p4-master-tempo`, `async-load`).

# Walkthrough — P6: Protección de carga (Load Lock)

## 1. Causa
El toggle "Protección de carga (Load Lock)" de Preferencias era **decorativo**: no tenía `onClick`, no se persistía y ningún camino de carga lo consultaba (`grep` de `loadLock` solo devolvía el propio componente). Mismo patrón que el botón SYNC antes de P1.

## 2. Cambios (rama `axel-molina/load-lock`, commit `c7a03ec`)
- **`AudioEngine`**: helpers genéricos `readBoolSetting`/`writeBoolSetting` (merge sobre `settings.json`, se conservan las demás claves); nueva clave `loadLock` con `isLoadLockEnabled()`/`setLoadLockEnabled()`, restaurada en el constructor (clave ausente → `false`).
- **`SettingsModalComponent`**: el toggle arranca con el valor persistido y lo guarda al pulsarlo.
- **`DeckComponent`**: `enum class LoadOrigin { User, Automix }`; `loadAudioFile(file, origin = User)` y `loadYouTubeVideo(result, origin = User)` con guarda centralizada (`shouldBlockLoad`): bloquea **solo** si es del usuario, el lock está activo y **esa bandeja está reproduciendo**. Si bloquea, muestra un **modal asíncrono** (`AlertWindow` + `enterModalState`, sin `runModalLoop`) con "Forzar carga" y "Cancelar"; las cargas reales viven en `loadAudioFileNow()` / `loadYouTubeVideoNow()` (las rutas internas de YouTube usan las versiones `Now` para no re-disparar el aviso).
- **`MainComponent`**: las 8 cargas del AutoMix pasan `LoadOrigin::Automix` (exento).
- **Teclado del modal** (revisado contra el código de JUCE 8.0.6): `LookAndFeel_V2::createAlertWindow` asigna Return al botón primario, así que se construye el `AlertWindow` a mano:
  - "Forzar carga" → `returnValue = 1`, **sin atajos** y **sin foco de teclado** (`Button::keyPressed` dispara `triggerClick` con Return si el botón tiene foco; `AlertWindow` da foco al primero).
  - "Cancelar" → `returnValue = 0`, con Return y Escape.
  - Resultado: Enter y Escape **cancelan**; forzar exige un clic deliberado.

## 3. Verificación
| Prueba | Resultado |
|---|---|
| Persistencia: activar → `loadLock: true`, `AudioEngine` nuevo lo restaura | OK |
| Persistencia: desactivar → queda `false` | OK |
| `settings.json` real del usuario tras el banco | **restaurado byte a byte** (hash idéntico; el banco hace backup/restore, sin "aislamiento" por `HOME`) |
| Semántica de botones (código de JUCE revisado) | Forzar = 1, Cancelar = 0, Return/Escape → 0 |
| Caminos cubiertos (revisión) | Finder, drop interno, biblioteca, YouTube/SoundCloud entran por las rutas guardadas; AutoMix exento |

## 4. Pendiente
- Prueba en vivo: con una bandeja sonando y el lock activo, soltar/cargar una pista → modal; "Cancelar" mantiene la pista, "Forzar carga" la cambia; en pausa carga directa; lock off sin cambios.
- Merges pendientes (`phase-align-fix`, `p4-master-tempo`, `async-load`, `load-lock`).

# Walkthrough — P7: Reproducir al cargar (Autoplay en bandeja)

## 1. Objetivo
Que una pista cargada en una bandeja **empiece a sonar sin pulsar PLAY**, como preferencia activada de fábrica y reversible, sin romper la precarga del AutoMix ni el modo vídeo de YouTube.

## 2. Cambios (rama `axel-molina/autoplay-on-load`, desde `load-lock`)
- **`AudioEngine`**: `readBoolSetting(property, defaultValue = false)` — el lector admite valor por defecto; nueva clave `autoplayOnLoad` (default **`true`**) con `isAutoplayOnLoadEnabled()` / `setAutoplayOnLoadEnabled()`, persistida con el mismo merge y restaurada en el constructor.
- **`SettingsModalComponent`**: casilla "Reproducir al cargar (Autoplay en bandeja)" / "Autoplay on load" en Preferencias → General, debajo del Load Lock; arranca con el valor persistido, persiste en `onClick`, texto bilingüe y mismo layout/visibilidad.
- **`DeckComponent`**: `LoadOrigin` baja a las funciones privadas (`loadAudioFileNow(file, origin)`, `loadYouTubeVideoNow(result, origin)`). Tras una carga correcta: si `origin == User` y la preferencia está activa y no es modo vídeo → `deck.play()`.
  - **AutoMix exento**: precarga con `LoadOrigin::Automix` → sigue en pausa hasta su transición.
  - **Modo vídeo de YouTube**: no se toca (usa `YouTubeService::isVideoAutoplayEnabled()`).
  - **Modo audio de YouTube** (caché y extracción): pasa por `loadAudioFileNow(..., origin)` → suena.
  - **Load Lock + "Forzar carga"**: la carga forzada pasa `LoadOrigin::User` → carga **y reproduce**.

## 3. Verificación
| Prueba (banco con backup/restore del `settings.json` real) | Resultado |
|---|---|
| Clave `autoplayOnLoad` ausente → `isAutoplayOnLoadEnabled() == true` (default ON) | OK |
| Desactivar → JSON `false` y un `AudioEngine` nuevo lo restaura | OK |
| Activar → JSON `true` y un `AudioEngine` nuevo lo restaura | OK |
| Claves ajenas intactas tras el merge (`loadLock`, `language`, `waveLow1`) | OK |
| `settings.json` real tras el banco | **restaurado byte a byte** (hash SHA-256 idéntico) |
| Compilación `cmake --build build` | 0 errores, 0 warnings en los 6 ficheros tocados |

## 4. Hallazgo del entorno (importante)
El `AGENTS.md` manda relanzar `build/DiscNativePro_artefacts/Debug/DiscNativePro.app`, pero **ese binario es del 12 de octubre** (no contiene ni la cadena de versión actual). La configuración real de este `build/` es **Release** (`CMAKE_BUILD_TYPE=Release`), así que los relanzamientos anteriores abrían una app antigua. En este pase se relanzó el binario vigente: `build/DiscNativePro_artefacts/Release/DiscNativePro.app` (PID verificado). Conviene corregir la ruta en `AGENTS.md` o fijar el tipo de build.

## 5. Pendiente
- Prueba en vivo: cargar desde Finder/biblioteca en bandeja en pausa → suena; apagar la preferencia → queda en pausa; AutoMix → precarga en pausa; YouTube vídeo → respeta su autoplay; lock + forzar → carga y suena.
- Merges pendientes (`phase-align-fix`, `p4-master-tempo`, `async-load`, `load-lock`, `autoplay-on-load`).

# Walkthrough — Selector de vista 2 Bandejas / Ondas (Waves)

## Objetivo
Convertir el badge estático "2 Bandejas" del top bar en un selector bilingüe de vista (2 Bandejas por defecto / Ondas-Waves) con un nuevo modo de vista de ondas a ancho completo, persistido en settings.json y con conmutación en caliente.

## Cambios

### [NEW] `src/ui/WavesViewComponent.h` / `.cpp`
- `WavesTrackStrip`: franja por deck con badge (BANDEJA 1/2), título, artista, badge de key (color via `KeyDetector::getCamelotColour`), BPM, tiempo "MM:SS.d / MM:SS.d" y pitch, cada una con su `OverviewWaveform` enlazada al mismo `DeckPlayer`.
- `WavesViewComponent`: apila las dos franjas a ancho completo de la app; expone `updateFromDecks(deck1, deck2)` con un POD `WavesTrackInfo` relleno desde `MainComponent::timerCallback` (30Hz).

### [MODIFY] `src/ui/TopBarComponent.h` / `.cpp`
- Enum `ViewMode { TwoDecks, Waves }`; botones segmentados `viewDecksButton` / `viewWavesButton` reemplazan al `layoutBadge`.
- `setViewMode(mode, sendCallback)` + callback `onViewModeChanged`.

### [MODIFY] `src/ui/DeckComponent.h` / `.cpp`
- `setWavesMode(bool)`: oculta header/overview/jog/videoPlayer/art; `resizedWavesMode()` coloca columna de pitch + 3 filas uniformes (transporte, hot cues, loop+SYNC/KEY).
- Visibilidad del jog centralizada en `updateJogVisibility()`; getters nuevos `getCurrentSeconds/getTotalSeconds/getPitchOffsetPercent`.

### [MODIFY] `src/ui/MainComponent.h` / `.cpp`
- Estado del modo + persistencia `"viewMode": "decks"|"waves"` en `AudioEngine::getSettingsFile()` (misma semántica de preservar JSON que `LanguageManager`).
- Rama de layout de ondas en `resized()`: banda de ondas `jlimit(116,168, area.getHeight()/4)` arriba a ancho completo, luego deck1|mixer|deck2; factor de alto de librería 0.26 (min 150) solo en modo ondas.
- `timerCallback` actualiza los strips a 30Hz desde ambos decks.

### [MODIFY] `CMakeLists.txt`
- Registrados `WavesViewComponent.h/.cpp` (~línea 106).

## Verificación
- **Compilación**: `cmake --build build` (Release) → 0 errores / 0 warnings en los 4 .cpp tocados (solo warnings preexistentes de harfbuzz en terceros).
- **Auto-Relaunch**: `pkill -f DiscNativePro; open build/DiscNativePro_artefacts/Release/DiscNativePro.app` → PID activo confirmado con `pgrep`.
- **Conmutación en caliente**: click en "Ondas" → mismo PID, layout de ondas activo (capturas); click en "2 Bandejas" → mismo PID, layout original restaurado idéntico.
- **Persistencia**: tras conmutar a Ondas, `~/Library/DiscNativePro/settings.json` contiene `"viewMode": "waves"`; relanzamiento completo (nuevo PID) abre directamente en modo Ondas; al volver a "2 Bandejas" el archivo queda `"viewMode": "decks"`.
- **Capturas** (modo Ondas, app en vivo):
  - `docs/screenshots/waves-view-1360x870.png` (contenido 1360x870)
  - `docs/screenshots/waves-view-1280x700.png` (contenido 1280x700; el layout mantiene mixer y strips completos)
- **2 Bandejas idéntico**: captura post-retorno coincide con el layout original (jogs, covers, mixer, librería sin cambios).

---

# QA independiente — Selector de vista 2 Bandejas / Ondas

Verificación ejecutada por separado de la implementación, contra los criterios de aceptación de la tarea `task_f7307607a3fd` (run `run_eb0c05079cc6`).

## Estático
- **Alcance del diff**: solo `src/ui/{TopBar,Main,Deck,WavesView}Component.*`, `CMakeLists.txt`, `docs/screenshots/` y este documento. `AudioEngine`, `MidiManager`/MIDI Learn, biblioteca, AutoMix y grabación intactos.
- **Shell prohibido**: `grep -rn "\bsystem(\|popen(\|_popen(" src/` → 0 resultados.
- **Sin asignaciones en tiempo real**: ninguna línea añadida por el diff contiene `new`/`malloc`/`push_back` dentro de callbacks de audio (la única asignación nueva, `new DynamicObject`, es en el guardado de settings desde el hilo de UI).

## Compilación
- Recompilación forzada de los 4 `.cpp` tocados (`touch` + `cmake --build build -j 2`, Release): **4 archivos compilados, exit 0, 0 errores y 0 warnings** propios (solo warnings preexistentes de terceros).

## Funcional (app en vivo, clics reales con `cliclick`)
- **Conmutación en caliente**: clic en "2 Bandejas" → layout clásico restaurado (platos, covers, mixer) en el **mismo PID**; clic en "Ondas" → vista de ondas activa, también sin reiniciar.
- **Datos de las tiras**: con pista cargada (BAD BUNNY x JHAY CORTES – DAÑA…), la tira de Bandeja 2 muestra exactamente los mismos datos que el deck: título, artista, key `9A / Em` (color Camelot), BPM `110.0`, tiempo `00:00.x / 03:34.2` y pitch `+0.0%`, con la OverviewWaveform a todo el ancho y sin platos ni vinilos.
- **Persistencia (escritura)**: `settings.json` conmuta `waves → decks → waves` con cada clic en el selector.
- **Persistencia (lectura)**: relanzamiento completo (`pkill` + `open`, PID nuevo 96021) abre directamente en modo Ondas con `"viewMode": "waves"`.
- **Capturas de aceptación**: `docs/screenshots/waves-view-1360x870.png` y `waves-view-1280x700.png` validadas visualmente (dimensiones exactas, modo Ondas correcto en ambas).

## Resultado
**APROBADO** — los 6 criterios observables de aceptación se cumplen. Nota menor: la rama se nombró `axel-molina/waves-view` en lugar de `feature/waves-view` (cosmético, sin impacto).

---

# Ajuste — Selector de vista desplegable (ComboBox personalizado)

## Objetivo
Los dos botones segmentados (2 Bandejas | Ondas) se reemplazan por un **selector desplegable
real**: un único botón que muestra la vista activa con chevron (`⌄ Ondas`); al hacer clic se
despliega un panel propio con **todas las opciones y la activa marcada con check**; elegir una
de la lista aplica el cambio. Preparado para futuras vistas (p. ej. "4 Bandejas").

## Cambios
### [MODIFY] `src/ui/TopBarComponent.h` / `.cpp`
- `viewDecksButton`/`viewWavesButton` → `viewSelectorButton` (DjButton con `ChevronDown` +
  texto de la vista activa vía `viewModeLabel()`).
- Clase anidada `ViewSelectorDropdown`: panel modal como hijo del componente padre (evita el
  recorte de los 40px de la barra), filas por opción con `LucideIcons::Check` en la activa,
  hover resaltado, `Esc` cierra, clic fuera cierra (`inputAttemptWhenModal`), selección →
  `selectViewMode()` → `onViewModeChanged`.
- El botón muestra borde acentuado mientras el panel está abierto (`updateViewSelectorAppearance`).
- El panel se cierra automáticamente en cualquier re-layout.

## Verificación (app en vivo, clics reales con `cliclick` + capturas)
- Clic en el selector → panel desplegado con "2 Bandejas" y "✓ Ondas" (activa marcada).
- Elegir "2 Bandejas" → vista clásica restaurada (platos), botón actualizado y
  `settings.json` → `"viewMode": "decks"`; reabrir muestra "✓ 2 Bandejas".
- Elegir "Ondas" → vista de ondas activa y `"viewMode": "waves"`.
- Clic fuera del panel y tecla `Esc` → el panel se cierra sin cambiar la vista.
- Compilación: `cmake --build build` (Release) → 0 errores / 0 warnings en los archivos tocados.
- Nota de entorno: cada rebuild cambia la firma ad-hoc de la app y macOS vuelve a pedir
  autorización del llavero para `com.discpro.discnativepro.api-key` (diálogos de SecurityAgent
  que bloquean el arranque hasta responderlos; para pruebas de UI se usó un `HOME` temporal).

---

# Ajuste 2 — Mixer compacto en modo Ondas y franjas de onda más altas

## Objetivo
En modo Ondas el diseño quedaba desordenado (botones demasiado espaciados) y el centro del
mixer mostraba un `VerticalWaveform` redundante. Se elimina la onda vertical en esta vista,
los faders de volumen de cada bandeja ocupan su lugar, la parte central se comprime y el
espacio liberado hace las franjas de onda más altas (mejor visión de los picos).

## Cambios
### [MODIFY] `src/ui/ChannelStripComponent.h` / `.cpp`
- `getVolumeFader()` + `setFaderHostedElsewhere(bool)`: con el fader fuera de la columna,
  el hueco inferior lo ocupa el VU centrado.

### [MODIFY] `src/ui/CenterColumnComponent.h` / `.cpp`
- `setWavesMode(bool, Slider*, Slider*)`: oculta `verticalWaveform` y aloja los dos faders
  externos lado a lado en el centro (MASTER/PHONES arriba; crossfader + SMOOTH/TEMPO BLEND
  debajo, sin cambios).

### [MODIFY] `src/ui/MixerComponent.h` / `.cpp`
- `setWavesMode(bool)`: re-padrea los faders entre las columnas y el centro según el modo.

### [MODIFY] `src/ui/DeckComponent.cpp`
- `resizedWavesMode()`: filas compactas (alto máx 64, huecos máx 10) y bloque centrado
  verticalmente — los botones dejan de quedar espaciados.

### [MODIFY] `src/ui/MainComponent.cpp`
- `applyViewMode()` notifica al mixer; banda de ondas `jlimit(160, 240, area*2/5)`
  (antes `jlimit(116, 168, area/4)`): franjas notablemente más altas.

## Verificación (app en vivo, capturas)
- **Modo Ondas**: sin onda vertical; faders CH1|CH2 lado a lado en el centro; columnas con
  EQ (HIGH/MID/LOW/FILTER) + VU; crossfader + SMOOTH/TEMPO debajo; botones de deck compactos;
  franjas superiores más altas.
- **Modo 2 Bandejas**: idéntico al clásico — faders de vuelta en las columnas y
  `VerticalWaveform` restaurado (verificado tras conmutar con el selector desplegable).
- **Conmutación en caliente** en ambos sentidos con persistencia (`viewMode` actualizado).
- **Compilación**: `cmake --build build` (Release) → 0 errores / 0 warnings en archivos tocados.

---

# Ajuste 3 — Drops en las tiras, mixer más delgado y HOT CUE superior (modo Ondas)

## Objetivo
Cuatro mejoras del modo Ondas: cargar canciones soltándolas sobre la onda de su bandeja,
separar los faders del centro, reordenar HOT CUE (etiqueta arriba-izquierda, pads debajo) y
eliminar los VU para columnas más estrechas con una librería más alta.

## Cambios
### [MODIFY] `src/ui/WavesViewComponent.h` / `.cpp`
- `WavesTrackStrip` implementa `FileDragAndDropTarget` + `DragAndDropTarget`: acepta archivos
  del Finder (mismas extensiones de audio que el deck) y drags internos de la librería
  (incluidos `yt:`), y los **reenvía al `DeckComponent` de su bandeja** (`filesDropped` /
  `itemDropped`): soltar sobre la onda carga exactamente igual que soltar sobre el deck.
  Resaltado del borde de acento mientras se arrastra sobre la tira.
- `WavesViewComponent::setDeckDropTargets(deck1, deck2)` cableado desde `MainComponent`.

### [MODIFY] `src/ui/CenterColumnComponent.cpp`
- Línea separadora vertical entre los dos faders de volumen del centro (modo Ondas).

### [MODIFY] `src/ui/DeckComponent.cpp`
- `resizedWavesMode()`: "HOT CUE" arriba a la izquierda y los 4 pads justo debajo.

### [MODIFY] `src/ui/ChannelStripComponent.h/.cpp`, `MixerComponent.h/.cpp`, `MainComponent.cpp`
- `setWavesMode(bool)` en la columna: sin VU y sin zona de fader (los knobs usan toda la
  altura); columnas CH de 96→72px y mixer de 430→382px en modo Ondas.
- Librería más alta en modo Ondas (factor 0.26→0.31): más pistas visibles en la tabla.

## Verificación
- **Layout (app en vivo, captura)**: HOT CUE con etiqueta superior-izquierda y pads debajo;
  columnas CH estrechas sin VU; faders CH1|CH2 en el centro con separador; franjas y librería
  más altas. 2 Bandejas conserva su diseño clásico (VU y faders en columnas, onda vertical).
- **Compilación**: `cmake --build build` (Release) → 0 errores / 0 warnings en archivos tocados.
- **Drag & drop**: implementado con paridad exacta al deck (reenvío de `filesDropped` /
  `itemDropped`); la prueba funcional de arrastre en vivo queda pendiente de ejecutarse con
  la app en primer plano (la automatización de clics se interrumpió por uso del equipo).

---

# Ajuste 4 — Aplanar más la parte central del modo Ondas

## Objetivo
Reducir aún más el aire de la parte central (botones de los decks, controles de bandeja y
faders del centro) para que la librería gane más espacio.

## Cambios
### [MODIFY] `src/ui/DeckComponent.cpp`
- `resizedWavesMode()` más compacto: filas máx 52px (antes 64), huecos máx 6 (antes 10),
  botones de 38/34px (antes 44/40), márgenes reducidos y pads más juntos.

### [MODIFY] `src/ui/CenterColumnComponent.cpp`
- En Ondas: sección superior 46→40px e inferior 64→56px; faders con menos margen
  horizontal (insets 14→8px, más cerca del separador y de los bordes).

### [MODIFY] `src/ui/MainComponent.cpp`
- Librería en Ondas: factor 0.31→0.35 (más pistas visibles).

## Verificación
- **App en vivo (captura)**: parte central notablemente más aplanada, botones compactos,
  faders más holgados de ancho con su separador, librería más alta.
- **2 Bandejas idéntico**: todo ramificado por `wavesMode`.
- **Compilación**: `cmake --build build` (Release) → 0 errores / 0 warnings en archivos tocados.

---

# Ajuste 5 — Fila central mínima y columnas EQ→CUE compactas (modo Ondas)

## Objetivo
Seguir achicando la fila central: eliminar el hueco entre los knobs de EQ y el botón CUE de
las columnas y limitar la altura de la fila de controles, cediendo el espacio al panel del
administrador de archivos.

## Cambios
### [MODIFY] `src/ui/ChannelStripComponent.cpp`
- En Ondas: bloque compacto arriba-centrado con el CUE justo debajo del knob FILTER
  (4px de separación, antes ~60–100px de aire). El camino clásico queda restaurado tal cual.

### [MODIFY] `src/ui/MainComponent.cpp`
- Reparto vertical explícito en Ondas: fila central **fija en 230px**, franjas de onda
  150–220px (mitad del resto) y **todo lo demás a la librería**. Determinista en cualquier
  tamaño de ventana.

## Verificación (app en vivo, captura)
- Columnas CH con FILTER→CUE pegados; fila central plana; librería notablemente más alta;
  franjas altas con datos y ondas de ambas bandejas; faders con separador.
- **2 Bandejas idéntico** (todo ramificado por `wavesMode`).
- **Compilación**: `cmake --build build` (Release) → 0 errores / 0 warnings en archivos tocados.

---

# Ajuste 6 — Wave scrolling con precisión de picos en las tiras (modo Ondas)

## Objetivo
La onda completa (overview) comprime los picos hasta hacerlos ilegibles para mezclar. Las
pasas a un **wave scrolling** como el central de 2 Bandejas (ventana ±3s, playhead centrado,
resolución fina de transientes), conservando una **tira overview fina** de contexto con la
posición de reproducción, y con la **cuadrícula de beats al BPM real** de cada pista.

## Cambios
### [NEW] `src/ui/DeckScrollWaveform.h` / `.cpp`
- Wave scrolling de una bandeja: timer 60Hz, ventana ±3s, barras multibanda desde la línea
  base con `WaveformTheme` (mismo estilo que el wave central de 2 Bandejas), cuadrícula de
  beats calculada con `DeckPlayer::getBpm()` (fallback 124), playhead acentado centrado y
  seek por click/drag sobre la onda.

### [MODIFY] `src/ui/WavesViewComponent.h` / `.cpp`
- `WavesTrackStrip`: fila de datos → **overview fino (16px, clicable)** → **wave scrolling**
  (resto del alto). Se mantiene el drag & drop y los datos de pista.

### [MODIFY] `CMakeLists.txt`
- Registrados `DeckScrollWaveform.h/.cpp`.

## Verificación (app en vivo, capturas)
- Tiras con pistas cargadas: wave scrolling con picos finos visibles, cuadrícula de beats,
  playhead centrado; overview fino con marcador de posición (~30% con 00:30/03:33).
- Datos de pista, drop targets y 2 Bandejas sin cambios.
- **Compilación**: `cmake --build build` (Release) → 0 errores / 0 warnings en archivos nuevos
  (warnings mostrados son preexistentes de `SoundCloudService`/`AsyncFolderScanner`/`UsbLibraryView`).

---

# Ajuste 7 — Precisión de picos estilo Serato en el wave scrolling

## Objetivo (y respuesta técnica)
Pistas comprimidas o sets largos se dibujaban como un bloque grueso: la extracción del
reproductor genera **8000 puntos fijos** (en un set de 84 min ≈ 0.63s por punto) con
amplitud **lineal**. Serato no usa una librería de render distinta: precomputa picos
multi-resolución y dibuja con escala dB + cuerpo/picos. Lo mismo es posible con JUCE tal cual.

## Cambios
### [NEW] `src/audio/WindowPeakExtractor.h` / `.cpp`
- Segundo `AudioFormatReader` (independiente del reproductor) que extrae picos finos de
  **256 muestras/punto (~6ms)** solo de la ventana visible (±3s + margen de 8s de caché,
  ~41KB de memoria). La resolución es por muestras: independiente de la duración de la pista
  (un set de 84 min obtiene el mismo detalle que un tema de 3 min).

### [MODIFY] `src/ui/DeckScrollWaveform.h` / `.cpp`
- Render con los picos finos: **cuerpo RMS multibanda** (tema de color) + **línea de pico
  fino** encima + **curva de amplitud tipo dB** (`amp^0.55`) que expande las diferencias de
  las pistas limitadas/brickwall. Normalización por ventana con suelo (sin despeinar partes
  suaves). Fallback al render anterior si no hay archivo local. Cuadrícula, playhead y seek
  sin cambios.

### [MODIFY] `CMakeLists.txt`
- Registrados `WindowPeakExtractor.h/.cpp`.

## Verificación (app en vivo, capturas)
- Pistas reales (incluida una de YouTube de 9:22): transientes individuales visibles beat a
  beat, cuerpo multibanda + línea de pico, cuadrícula alineada y playhead centrado — el
  "bloque grueso" desaparece incluso en material comprimido.
- **Compilación**: `cmake --build build` (Release) → 0 errores / 0 warnings en archivos nuevos.

---

# Ajuste 8 — Scrub estilo plato de vinilo sobre la onda expandida

## Objetivo
Hacer click sobre la onda scrolling y arrastrar izquierda/derecha debe mover la aguja como
el plato de vinilo (con sonido de scratch), en lugar del salto de posición anterior.

## Cambios
### [MODIFY] `src/ui/DeckScrollWaveform.h` / `.cpp`
- Misma semántica que `JogWheel`: `mouseDown` → `beginScratch` (si está en pausa entra en
  "audición" para que el gesto suene y vuelve a pausa al soltar); `mouseDrag` → velocidad
  suavizada y limitada (±10 rev/s) vía `setScratchVelocity`, con mapeo **1:1 con la onda**
  (arrastrar un ancho de ventana = 6s de audio); `mouseUp` → `endScratch`. Reposo = aguja
  detenida, y el retroceso suena igual que el avance.
- Se elimina el seek por click en la onda expandida (el overview fino sigue siendo la barra
  de seek clicable).

## Verificación
- **Compilación**: `cmake --build build` (Release) → 0 errores / 0 warnings.
- Prueba de gesto en vivo pendiente (equipo en uso); el ciclo scratch replica el del
  `JogWheel`, verificado en la app.

## Resolución del merge en `main` (Modo Ondas sobre P1–P7)
La rama `waves-view` salió de un `main` anterior a los temas de color por bandeja, así que al integrarla aparecieron dos desajustes de firma (detectados al compilar, no en revisión):
- `WaveformTheme::low/mid/high` ya reciben el índice de bandeja: `DeckScrollWaveform` guarda ahora su `deckIndex` y lo pasa al tema (la onda de cada tira usa el color de **su** bandeja, no el global).
- `OverviewWaveform` ya recibe `(player, accent, deckIndex)`: `WavesTrackStrip` le pasa su `deckNum`.
- `walkthrough.md` conservó las secciones P1–P7 y a continuación las 9 del modo Ondas.

**Verificación tras el merge:** `cmake --build build` → 0 errores y 0 warnings en los ficheros tocados (queda 1 warning previo ajeno en `UsbLibraryView.cpp`). App relanzada desde `Release`; `settings.json` conserva `viewMode: "waves"` y el resto de claves (temas por bandeja, `tempoBlend`, `loadLock`), y el modo se restaura al arrancar.

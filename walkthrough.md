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

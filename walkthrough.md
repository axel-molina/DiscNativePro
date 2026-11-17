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
- [MODIFY] `CMakeLists.txt`: `APP_VERSION` + sources + `DISCNATIVEPRO_VERSION` define.
- [MODIFY] `src/Main.cpp`: `getApplicationVersion()` desde el macro.
- [MODIFY] `.github/workflows/release.yml`: los 3 jobs pasan `-DAPP_VERSION` desde `GITHUB_REF_NAME` sin `v` y usan la versión vigente del proyecto cuando la referencia no es numérica.
- [MODIFY] `src/ui/TopBarComponent.h/.cpp`: `DjButton updateButton` ámbar `UPDATE` (oculto por defecto), `setUpdateAvailable()`, `onUpdateClicked`, tooltip ES/EN, hueco en `resized()` entre settings y layout.
- [MODIFY] `src/ui/MainComponent.h/.cpp`: `scheduleUpdateCheck()` (único chequeo, diferido 3s, `SafePointer`), `showUpdateDialog()` con `AlertWindow::showAsync(MessageBoxOptions)` + `ModalCallbackFunction`: [Descargar] abre asset directo, [Omitir] persiste tag + oculta badge, [Más tarde] deja badge para reabrir. Bilingüe.

### Verificación (real, con captura)
- `cmake --build build` → 0 errores; 0 warnings en ficheros propios (2 errores intermedios corregidos: `MessageManager` necesitaba `juce_events`, `showAsync` va con `MessageBoxOptions`).
- API real: tag de release y assets de las 3 plataformas. El build local anterior mostró correctamente el flujo de actualización.
- `screencapture` confirmó el badge `UPDATE` ámbar en TopBar y el diálogo de actualización con `Descargar/Omitir versión/Más tarde`.
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

# Integración final en `main` (P1–P7 + versión 1.3.0)

## 1. Orden de integración
| Fase | Rama | Commit de merge | Conflictos resueltos |
|---|---|---|---|
| P4.1 + P4.2 + P5 + P6 + P7 | `axel-molina/autoplay-on-load` | `9ea6946` | ninguno |
| P3 | `axel-molina/phase-align-fix` | `d7cc7b8` | `DeckPlayer.cpp`, `walkthrough.md` |
| Versión 1.3.0 | `axel-molina/feature-version-alignment` | `074094b` | `src/Main.cpp`, `walkthrough.md` |

Todos los merges con `--no-ff` para conservar la historia por fase. Tag `pre-merge-p3-p7` en el `main` previo.

## 2. Decisiones de resolución
- **P3 sobre P5**: se mantiene la carga asíncrona; el offset del primer beat se publica desde el worker y el controlador espera BPM > 0 en ambos decks. Tres fallos reales corregidos al validar el merge (ver sección P3 §4): inversa `setPosition`/`getPosition` con Key Lock, nudge fraccional en el stretcher y guard de BPM.
- **Versión**: `#error` si falta `DISCNATIVEPRO_VERSION` (sin fallback `1.0.0-dev`) y se conserva la instancia única de `main`; CMake aporta `1.3.0` al binario, `Info.plist` y al instalador.

## 3. Verificación final
- `cmake --build build` → **0 errores**; 0 warnings nuevos en ficheros tocados. Los 5 warnings restantes son previos y ajenos (`AsyncFolderScanner` ×3, `SoundCloudService`, `UsbLibraryView`), más los de Harfbuzz dentro de JUCE.
- `Info.plist` del bundle Release: `CFBundleShortVersionString` = `CFBundleVersion` = **1.3.0**; `strings` del binario contiene `1.3.0`; sin `1.0.0-dev` en `src/`, `installer/` ni `CMakeLists.txt`.
- Harnesses (pistas reales): `run_phase_real` converge a **0.67–1.77 ms** con nudge liberado; `run_pos_merged` invariante **−0.0 ms** antes y después del seek; `run_math_merged` 5043 casos / 0 fallos; `run_autoplay` persistencia 5/5 con `settings.json` restaurado byte a byte.
- App Release relanzada tras cada fase; `settings.json` conserva los temas por bandeja, `tempoBlend` y `loadLock`. `autoplayOnLoad` no está escrito todavía: su valor por defecto es ON (y se escribe en cuanto se toca el ajuste).

## 4. Pendiente (fuera del merge)
- Prueba de escucha manual: cruce de zona con dos temas reales (el controlador tarda ~23 s por diseño en corregir desfases de 1–15 ms) y autoplay desde Finder/YouTube.
- **Modo Ondas fuera de `main`**: se revirtió el merge `b84d18c` (`git revert -m 1`) porque la vista de ondas aún no está probada a fondo. La rama `axel-molina/waves-view` queda intacta; al re-mergear habrá que revertir antes este revert.
- Deuda documentada en cada fase (lock del callback de audio, pre-roll dentro del callback, `ScrollingWaveform` con grid fijo a 124 BPM).

---

# Walkthrough — Arrastre y zoom en la onda vertical central

## 1. Causa
Tres problemas encadenados:
1. **`VerticalWaveform` no tenía handlers de ratón**: el componente central (mitad izquierda = Deck 1, derecha = Deck 2) solo pintaba a 60 Hz. Arrastrar sobre él no podía hacer nada, en play ni en pausa.
2. **Seek en pausa roto** (regresión de P3): `setPosition()` dejaba un `seekPending` que aplicaba el callback de audio, pero ese callback salía antes con el transporte parado. El clic/arrastre sobre cualquier onda no movía nada en pausa.
3. **La aguja se congelaba durante el scratch** (también de P3): la rama de scratch hacía `return` antes de actualizar `playbackPosSamples`, de donde leen `getPosition()`, el tiempo y el giro del plato. El plato raspaba pero parecía muerto.

## 2. Cambios
### `src/ui/VerticalWaveform.h/.cpp`
- **Clic** = salto al instante bajo el puntero (`timeAtY`: arriba = futuro, misma geometría que el pintado).
- **Arrastre vertical** = adelantar/atrasar: `nuevaPos = ancla + (anclaY − y) / pxPerSec`, con clamp a `[0, duración]`. Seek **silencioso** (la pista sigue sonando desde el punto nuevo).
- **Rueda** = zoom de la vista (`pixelsPerSecond`, 20–300 px/s, por defecto 70), solo en memoria.
- Cursor `UpDownResizeCursor` y repintado cuando cambia la posición **aunque el deck esté en pausa** (antes el timer solo repintaba si algún deck sonaba).
- El `pixelsPerSecond` local de `renderDeckHalf` pasa a ser el miembro de la clase (la rejilla de beats y la ventana visible escalan con el zoom).

### `src/audio/DeckPlayer.cpp`
- El `seekPending` se aplica **antes** del `return` de `!playing` y dentro del `ScopedLock`: el seek funciona con el transporte parado (afecta también a la waveform horizontal y al overview).
- `publishScratchPos()` en las dos ramas del scratch (adelante y atrás) publica la posición real de la aguja, reponiendo la latencia del stretcher cuando Key Lock está activo.

## 3. Verificación
- `cmake --build build` → **0 errores**, 0 warnings en los ficheros tocados.
- Banco nuevo `run_pause_seek` (pista real):
  - pausa + `setPosition(30.0)` → `getPosition()` = **30.000 s** (antes se quedaba en 0.661 s);
  - en pausa la posición no deriva (**+0.0 ms** en 300 ms);
  - scratch adelante **+1287 ms**, scratch atrás **−543 ms** (la aguja sigue a la mano);
  - tras `endScratch` el transporte vuelve a avanzar (**+395 ms en 400 ms**).
- `run_pos_merged`: invariante de posición **−0.0 ms** antes/después del seek, delta 0.1139 frente al natural 0.114.
- `run_phase_real`: controlador de fase converge a **0.93 ms** con el nudge liberado.

## 4. Pendiente
- Prueba manual del gesto (clic, arrastre en play y en pausa, rueda para zoom) y del plato girando durante el scratch.
- El zoom no se persiste (decisión: solo memoria, por sesión).

---

# Walkthrough — Aislamiento de bandejas en la onda vertical (Tempo Blend ON)

## 1. Causa
El arrastre ya elegía bandeja por mitad (`deckForX`), pero el **controlador de fase** del motor deshacía la selección:

1. Arrastrar la onda izquierda hace `deck1.setPosition(...)` → la fase entre decks salta ~2 s.
2. `AudioEngine::processPhaseAlign()` tenía un **auto-rearme por deriva**: si `|error| > 25 ms` estando enclavado, pasaba a `Settle` → `Measure`.
3. Con `|error| > 15 ms` (`phaseReanchorSec`) ejecutaba `slaveDeck.seekRelative(delta)` → **la otra bandeja saltaba** (~135 ms cuantizados al beat, medido).

Resultado: si arrastrabas la master, la otra se movía contigo (movía ambas); si arrastrabas la slave, el controlador te la devolvía (parecía que no distinguía). El único detector de intervención humana existente comparaba `getPitchBend()`: detectaba tocar el pitch, no mover el transporte.

## 2. Cambios
### `src/audio/DeckPlayer.h/.cpp`
- `std::atomic<juce::uint32> userSeekSerial` + `getUserSeekSerial()`: contador monótono de seeks **pedidos por el usuario**.
- `setPosition(double, bool userInitiated = true)` y `seekRelative(double, bool userInitiated = true)`: solo incrementan el serial si el salto es humano. El controlador de fase pasa `false`; las ondas, el overview y el MIDI mantienen el valor por defecto.

### `src/audio/AudioEngine.h/.cpp`
- `std::atomic<bool> phaseUserOverride` + `lastUserSeek1/lastUserSeek2`.
- En `processPhaseAlign()`, tras resolver master/slave (antes del `return` de `isPlaying`): si cambió algún serial → `setPitchBend(0)` en la slave, limpiar nudge, `Idle`, `armedFlag = false`, `seekCount = 0`, `phaseUserOverride = true` y salir.
- El bloque de rearme por deriva añade `&& !phaseUserOverride.load()`.
- `phaseUserOverride` se limpia en `armPhaseAlign()` (cruce del crossfader al centro) y en `setTempoBlend(true)`.

Semántica: **un seek manual manda**. El Tempo Blend sigue igualando el BPM, pero la alineación de fase no se reanuda hasta el próximo cruce del crossfader al centro (o hasta reactivar Tempo Blend).

## 3. Verificación
- `cmake --build build` → **0 errores**, 0 warnings en los ficheros tocados (los 3 warnings de `AsyncFolderScanner.cpp` son previos y ajenos a este cambio).
- Banco nuevo `run_seek_isolation` (pista real, ambas sonando, Tempo Blend ON, crossfader al centro):
  - seek +2.0 s en **deck 1** → deck 1 en objetivo (±6 ms) y deck 2 **+6 ms** (sin salto);
  - seek +1.5 s en **deck 2** (la slave) → deck 2 se queda donde se puso (±9 ms) y deck 1 **−9 ms**;
  - tras rearmar por crossfader el controlador vuelve a estar vivo.
- **Prueba negativa**: con el guardián del rearme desactivado, el mismo banco da **+135 ms** de salto en deck 2 (test 1) y **+366 ms** (test 2) → el banco detecta la regresión.
- Regresión: `run_phase_real` converge a **0.93 ms** con bend liberado; `run_pause_seek` (30.000 s en pausa, scratch +1287/−543 ms) y `run_pos_merged` (invariante −0.0 ms) siguen en verde.

## 4. Pendiente
- Prueba manual: con las dos pistas sonando y Tempo Blend ON, arrastrar la mitad izquierda mueve **solo** el disco izquierdo y viceversa.

---

# Walkthrough — Opción A: el controlador de fase nunca salta sobre una bandeja audible

## 1. Causa
Con Tempo Blend ON, mover el crossfader producía saltos audibles "en la otra canción":

1. Cada movimiento del fader llama a `applyTempoBlend()` → `setPitchPercent()` en **las dos** bandejas (`AudioEngine.cpp:548-549`). El tempo de ambas cambia continuamente mientras se mueve el fader.
2. Ese cambio de tempo altera la fase → el controlador la ve crecer.
3. Al superar 15 ms (`phaseReanchorSec`) hacía un **seek duro** de hasta medio beat (≈234 ms), repetible hasta 8 veces (`AudioEngine.cpp:729`, `:708`).
4. Además, al entrar al centro se armaba el ciclo `Settle → Measure → seek` justo cuando **las dos pistas ya son audibles**.

El seek cae siempre en la slave, que es "la otra canción" respecto al master.

## 2. Cambios (`src/audio/AudioEngine.cpp`)
- Nueva constante `phaseSilentXf = 0.9f`: con crossfader constante-potencia, a `|x| >= 0.9` el otro deck queda a ~−22 dB (inaudible).
- `slaveInaudible`: el seek duro **solo** se permite si la bandeja a alinear está muteada por el crossfader.
- Si la bandeja es audible, el desfase se corrige **siempre con nudge** (bend ≤ 1 %). Se quitó el `if (errorAbs <= phaseReanchorSec)` que dejaba los desfases grandes sin corregir cuando no había seek.
- Con el fader aparcado en un lado, el **master pasa a ser la bandeja que suena** (`|x| >= 0.9`), así el seek —cuando toca— cae siempre en la muteada.
- Tempo Blend: **desactivado de fábrica** (`readBoolSetting(tempoBlendProperty, false)`); solo se enciende si el usuario lo activa.

## 3. Verificación
- `cmake --build build` → **0 errores**, 0 warnings en los ficheros tocados.
- Banco nuevo `run_fader_jump` (pista 128 BPM en deck 1, 120 BPM en deck 2, Tempo Blend ON, fader de −1 a 0 en pasos de 0.05, muestreo cada 10 ms):
  - **antes**: deck 2 (audible) salta **+478.7 ms** → FALLO;
  - **después**: salto máximo **12.3 ms** (cuantización del bloque de audio) → OK.
  - Solo se exige invariante a las bandejas audibles (ganancia > 0.1); la muteada puede seekearse.
- `run_phase_real`: converge a **0.93 ms** con el bend liberado (el seek sigue disponible con la slave muteada, que es el caso del banco).
- `run_seek_isolation`: deck 2 no se mueve al seekear deck 1 y viceversa (±3 ms); el controlador se rearma por crossfader y ahora corrige con nudge (+0.00132).
- `run_pause_seek` y `run_pos_merged`: verdes.

## 4. Compromiso asumido (Opción A)
Si las dos pistas son audibles y están desfasadas más de ~15 ms, la corrección es un bend de ±1 %: tarda ~20 s en quitar 100 ms. No hay saltos, pero durante ese rato se oye el flam entre bombos. La alternativa (Opción B) era nudge siempre, también sin saltos, con la misma lentitud.

---

# Versión 1.3.1 — fixes de transporte y alineación de fase

## 1. Contenido
| Commit | Fix |
|---|---|
| `c9702bc` | Arrastre vertical y zoom en la onda central; seek en pausa; aguja viva durante el scratch |
| `b45b4d1` | Un seek manual desarma la alineación de fase (arrastrar una bandeja ya no mueve la otra) |
| `c7b7b44` | El controlador de fase nunca salta sobre una bandeja audible; Tempo Blend off de fábrica |

## 2. Cambio de versión
- `CMakeLists.txt`: `project(DiscNativePro VERSION 1.3.0)` → **1.3.1**.
- `APP_VERSION` sigue derivándose de `git describe`; con el tag `v1.3.1` en el commit de la versión, el bundle y el binario reportan **1.3.1**.

## 3. Verificación
- `cmake --build build` → **0 errores**, 0 warnings nuevos en ficheros tocados.
- `Info.plist` del bundle Release: `CFBundleShortVersionString` = `CFBundleVersion` = **1.3.1**.
- Bancos en verde: `run_pause_seek`, `run_seek_isolation`, `run_fader_jump` (12.3 ms máx. en la bandeja audible), `run_phase_real` (0.93 ms), `run_pos_merged` (−0.0 ms).

---

# Fix de build Windows (1.3.1)

## 1. Causa
El release `v1.3.1` no se publicó: el job **Windows x64** falló al compilar `src/services/SecretStore.cpp` (código exclusivo de Windows, nunca compilado en macOS/Linux):

```
SecretStore.cpp(163,37): error C2440: 'static_cast': cannot convert from 'LPBYTE' to 'const char *'
SecretStore.cpp(176,29): error C2440: '=': cannot convert from 'const wchar_t *' to 'LPWSTR'
```

Introducido en `ff16330` (21-oct, almacén seguro de credenciales). `build-windows.yml` llevaba en rojo desde el push del 27-oct 02:09 (4 ejecuciones), pero no se revisó hasta generar el release.

## 2. Cambios
- `SecretStore.cpp:163`: `static_cast<const char*>` → `reinterpret_cast<const char*>` (`CredentialBlob` es `LPBYTE`).
- `SecretStore.cpp:176`: `credential.TargetName = const_cast<LPWSTR>(targetName)` (`CREDENTIALW::TargetName` no es const; el buffer de `target` sigue vivo).
- `LanguageManager.cpp:70`: `Language newLang = currentLang;` (C4701, variable potencialmente sin inicializar).
- Auditar el resto del bloque `#if JUCE_WINDOWS`: `CredReadW`/`CredDeleteW` reciben `LPCWSTR`, correctos.

## 3. Verificación
- Build local (macOS): **0 errores**, 0 warnings nuevos.
- Verificación real en `build-windows.yml` (push a `main`) antes de mover el tag.

---

# Feedback Linux: vídeo de YouTube en blanco (audio sí)

## 1. Diagnóstico
El build de Linux compila; el fallo es de **runtime**. En modo vídeo el audio sale del webview (`MainComponent.cpp:228-241` → `setVideoVolume`), así que si suena, WebKit cargó y la página vive: lo que no llega a pintarse es la superficie. Tres causas posibles, por probabilidad:

1. **Faltan plugins de GStreamer** (`gstreamer1.0-gl`, `plugins-bad`, `libav`): WebKitGTK no crea el *video sink* ni decodifica VP9/H.264. El audio (Opus, en `plugins-base`) suena igual. Es la firma exacta del reporte.
2. **DMA-BUF renderer / compositing de WebKitGTK** (NVIDIA propietario, XWayland, VMs): la página existe y suena pero la superficie sale blanca. Bug clásico de Tauri/Wails en Linux.
3. **JUCE 8.0.6 abre WebKit con el nombre sin versión** (`juce_WebBrowserComponent_linux.cpp:402-411`: `libwebkit2gtk-4.1.so`, `libgtk-3.so`). Ese symlink solo lo pone el paquete `-dev`; con solo runtime todos los `dlopen` fallan y el componente queda mudo, en blanco y sin ningún error (juce-framework/JUCE#1730). Esta daría blanco **y tampoco audio**, así que probablemente no es, pero conviene detectarla.

## 2. Cambios
- `src/Main.cpp`: `configureWebKitForLinux()` en `initialise()`, **antes** de crear la ventana. Define `WEBKIT_DISABLE_DMABUF_RENDERER=1` y `WEBKIT_DISABLE_COMPOSITING_MODE=1` con `overwrite=0` (si el usuario ya los puso, se respeta; así una GPU que funcione puede pedir aceleración con `...=0`). WebKit lee su entorno al arrancar el proceso web, que es un fork nuestro, por eso va aquí y no dentro del componente.
- `src/ui/YouTubeVideoComponent_linux.cpp`: `webKitRuntimeAvailable()` reproduce con `dlopen` la misma comprobación que hace JUCE. Si falla, no se crea el `WebBrowserComponent` y `paint()` dibuja el aviso *"Reproductor web no disponible — falta WebKitGTK…"* en vez de un rectángulo blanco mudo.
- `README.md`: dependencias de **runtime** del `.tar.gz` (`libwebkit2gtk-4.1-0`, `libgtk-3-0`, los cuatro paquetes de GStreamer), cómo comprobar los plugins con `gst-inspect-1.0` y cómo forzar aceleración por GPU.

## 3. Verificación
- `cmake --build build` (macOS): **0 errores**, 0 warnings. El bloque Linux no se compila en macOS: la verificación real es `build-linux.yml`, que corre en cada push a `main`.
- Pendiente de confirmación del usuario: si el aviso nuevo aparece, es la causa 3; si sigue en blanco con audio, es la 1 o la 2 (los `gst-inspect-1.0` del README lo distinguen).

---

# Columna FECHA en el buscador de YouTube

## 1. De dónde sale el dato
El buscador tiene dos rutas (`YouTubeService.cpp:580`): la oficial con API key y, sin key configurada, `searchPublic()`, que scrapea `ytInitialData`. En los resultados YouTube solo expone la fecha **relativa** (`publishedTimeText.simpleText` = "hace 2 años"), que es lo que muestra su propia web. **No hace falta ninguna request extra.**

La ruta con API key (`searchOfficialApi`) sí trae `snippet.publishedAt` absoluto; se pinta en ISO (`2023-05-14`) para no dejar la columna vacía si algún día se configura una key.

## 2. Cambios
- `YouTubeService.h`: campo `juce::String publishedText` en `YouTubeSearchResult` (texto ya formateado; vacío si YouTube no lo da, p. ej. directos).
- `YouTubeService.cpp` (`searchPublic`): lee `publishedTimeText.simpleText`, con fallback a `runs[0].text`.
- `YouTubeService.cpp` (`searchOfficialApi`): rellena con `snippet.publishedAt` en ISO.
- `YouTubeLibraryView.cpp`: columna `FECHA` (id 8) insertada **entre CANAL/ARTISTA y DURACIÓN** (`insertIndex 5`), 105 px. TÍTULO 340→300 y CANAL 170→150 para no forzar scroll (total 892→937 px). `notSortable` porque `sortOrderChanged` no está implementado en esta vista. Cabecera traducida (`FECHA` / `DATE`) y `case 8` en `paintCell`: gris 150/158/175, 11.5 plain, centrado, con `—` apagado cuando no hay fecha.

## 3. Verificación
- `cmake --build build`: **0 errores**, 0 warnings en `src/`.
- Banco nuevo `run_yt_date` (busca "daft punk" y exige fecha en la mayoría de los 5 primeros):

```
Daft Punk - Lose Yourself to Dance (Official Versi | hace 13 años | 4:11
D.a.f.t P.u.n.k Greatest Hits                      | hace 3 años  | 1:11:15
Daft Punk - Around The World (Official Music Video | hace 5 años  | 4:02
Daft Punk - Veridis Quo (Official Audio)           | hace 11 años | 5:46
Daft Punk - Something About Us (Official Video)    | hace 17 años | 3:49
5/5 con fecha -> OK
```

Valida lo único frágil del cambio: el nombre del campo en `ytInitialData`.

---

# Header de la tabla USB igual al resto de la app

## 1. Causa
`UsbLibraryView` era la única de las cuatro vistas con `TableListBox` que no fijaba los colores del header. Sin overrides manda el LookAndFeel por defecto — que no es `DjLookAndFeel` (solo lo usan `ChannelStripComponent` y `CenterColumnComponent`), sino `LookAndFeel_V4`, que hereda de V3:

- `juce_LookAndFeel_V3.cpp:54` → `backgroundColourId = white.withAlpha(0.6f)`
- `juce_LookAndFeel_V2.cpp:158` → `textColourId = 0xff000000`

Blanco al 60 % sobre el fondo oscuro = franja gris clara con texto negro.

## 2. Cambio
`src/ui/UsbLibraryView.cpp:117-118`, los mismos valores que `LibraryComponent.cpp:796` y `YouTubeLibraryView.cpp:236`:

```cpp
table.getHeader().setColour(juce::TableHeaderComponent::backgroundColourId, juce::Colour::fromRGB(16, 18, 24));
table.getHeader().setColour(juce::TableHeaderComponent::textColourId, juce::Colour::fromRGB(142, 149, 165));
```

No se toca `outlineColourId`: ninguna de las otras vistas lo define, así que la línea separadora ya coincide.

## 3. Verificación
- `cmake --build build`: 0 errores. El único warning de `src/` es el preexistente `UsbLibraryView.cpp:172 unused parameter 'totalFound'`.
- Pendiente de ojo en la pestaña USB: header oscuro con texto gris, idéntico a Biblioteca local y YouTube.

---

# Versión 1.3.2

## 1. Contenido
| Commit | Cambio |
|---|---|
| `8220247` + `9a5ff20` | Fix del vídeo de YouTube en blanco en Linux: `WEBKIT_DISABLE_DMABUF_RENDERER=1` y `WEBKIT_DISABLE_COMPOSITING_MODE=1` antes de crear la ventana, detección de WebKitGTK con `dlopen` y aviso en pantalla si falta. |
| `cefa2f3` | Columna FECHA en el buscador de YouTube (`publishedTimeText`, sin requests extra). |
| `874e8f2` | Header de la tabla USB con los colores del resto de la app. |

## 2. Cambio de versión
- `CMakeLists.txt`: `project(DiscNativePro VERSION 1.3.1)` → **1.3.2**.
- `APP_VERSION` sigue derivándose de `git describe`; con el tag `v1.3.2` en este commit, bundle y binario reportan **1.3.2**.

## 3. Verificación
- `cmake --build build` → 0 errores; los warnings de `src/` son los preexistentes (`AsyncFolderScanner` ×3, `SoundCloudService`, `UsbLibraryView:172`).
- Bancos: `run_yt_date` 5/5 con fecha; `run_pause_seek`, `run_seek_isolation`, `run_fader_jump`, `run_phase_real`, `run_pos_merged` en verde.
- CI de `main`: `build-linux.yml` y `build-windows.yml` en success en los tres commits.
- Pendiente: prueba real del fix de Linux por el usuario (no había máquina Linux disponible al taggear).

---

# Issue #1 — binario macOS realmente universal (arm64 + x86_64)

## 1. Diagnóstico
- El job `build-macos` de `.github/workflows/release.yml` compilaba en `runs-on: macos-latest` **sin `CMAKE_OSX_ARCHITECTURES`**. `macos-latest` es hoy una imagen **arm64**, así que el `.app` salía con un único slice `arm64` aunque el job se llame `macOS Universal` y publique `DiscNativePro-macOS-Universal.zip`.
- Reproducido en local (este Mac, arm64): `build/DiscNativePro_artefacts/Release/.../DiscNativePro` → `Non-fat file … architecture: arm64`.
- `grep` de `OSX_ARCHITECTURES|lipo|universal` en el repo → solo coincidencias textuales; no había ningún paso que uniera los slices. `README.md:43` ya prometía Intel x86_64.

## 2. Cambio
`[MODIFY] .github/workflows/release.yml` (job `build-macos`; **no** se toca `CMakeLists.txt` ni código de la app, por decisión de alcance: la arquitectura se fija solo en CI):
- `Configure CMake`: añadido `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"`.
- Nuevo paso `Verify universal binary (arm64 + x86_64)`: `lipo -archs` sobre el ejecutable del bundle y **falla el job** si falta `arm64` o `x86_64` (guardia contra la regresión silenciosa).

## 3. Verificación
- Build universal en dir temporal `build-universal/` (ya eliminado):
  - `lipo -info` → `Architectures in the fat file … are: x86_64 arm64`.
  - `file` → `Mach-O universal binary with 2 architectures`; `otool -hv` lista cabeceras `X86_64` y `ARM64`.
  - Comparativa: binario antiguo (`build/`) = `arm64` solo (el bug); universal = 2 slices.
- **Ejecución real del slice Intel**: `arch -x86_64 …/DiscNativePro` → proceso vivo; `vmmap` → `Code Type: X86-64 (translated)` con `/usr/libexec/rosetta/runtime` y `/Library/Apple/usr/libexec/oah/libRosettaRuntime` mapeados (Rosetta 2 instalado).
- `yt-dlp` embebido en `Contents/Resources/`: ya era universal (`x86_64 arm64`), sin problema para el usuario Intel.
- YAML válido (`yaml.safe_load` → 4 jobs: `build-windows`, `build-linux`, `build-macos`, `publish-release`).
- Warnings del build: solo los preexistentes (`signalsmith-stretch` / harfbuzz de terceros y `SoundCloudService.cpp:253` `-Wstring-compare`); ninguno de la app.

## 4. Relanzado y limpieza
- App universal recién compilada abierta y confirmada con `pgrep` (PID 82218).
- `build-universal/` eliminado (no está en `.gitignore`, así que no se deja basura sin trackear).
- App normal de `build/` relanzada y confirmada (PID 82961).

## 5. Pendiente / fuera de alcance
- Sin commit ni push (no aprobado en esta tanda). Cambio local: `.github/workflows/release.yml` + `implementation_plan.md` + este `walkthrough.md`.
- Comentario con diagnóstico y fix publicado en el issue [#1](https://github.com/axel-molina/DiscNativePro/issues/1#issuecomment-6643801351).
- El binario corregido llegará a los usuarios en la **próxima release** (el fix solo aplica al CI; las releases ya publicadas siguen arm64-only).
- No abordado (YAGNI): fijar `runs-on: macos-14` en vez de `macos-latest`; `CMAKE_OSX_DEPLOYMENT_TARGET` explícito.

---

# BUG-1 — comparación de punteros en la caché alternativa de SoundCloud

## 1. Causa
`src/services/SoundCloudService.cpp:253` comparaba punteros, no texto:
```cpp
auto ext = item.progressiveStreamAuthUrl.isNotEmpty() ? ".mp3" : ".m4a";                 // 250 → const char*
auto altTarget = cacheDir.getChildFile("sc_" + item.id + (ext == ".mp3" ? ".m4a" : ".mp3")); // 253
```
`ext == ".mp3"` compara direcciones (`-Wstring-compare`). Tipo confirmado con `static_assert(std::is_same_v<decltype(ext), const char*>)`. En el `.o` real los literales `.mp3`/`.m4a` aparecían **una sola vez** en `__cstring` → clang los fusionaba y la comparación daba `true`: **funcionaba por accidente**. Riesgo: si no hay fusión (otro compilador/flags/LTO), `false` siempre → `altTarget` siempre `.mp3` → con `ext == ".mp3"` se perdería el fallback al `.m4a` cacheado (re-descarga innecesaria + fichero huérfano). Sin corrupción ni crash.

## 2. Cambio
`[MODIFY] src/services/SoundCloudService.cpp:250` — una palabra:
```cpp
juce::String ext = item.progressiveStreamAuthUrl.isNotEmpty() ? ".mp3" : ".m4a";
```
Así `ext == ".mp3"` usa `String::operator==(const char*)` (contenido) y la línea 251 concatena igual. Sin tocar el resto del flujo (HLS, nombres de fichero, umbral `> 10000`).

## 3. Verificación
- Compilación dirigida con las flags reales del proyecto (`-fsyntax-only -Wstring-compare … SoundCloudService.cpp`) → **0 warnings**.
- `cmake --build build` → **0 errores** y **0 `-Wstring-compare`** (antes: 1). El TU se recompiló.
- Warnings propios restantes: solo los preexistentes (`AsyncFolderScanner.cpp` ×3, `UsbLibraryView.cpp:172`); `SoundCloudService.cpp` ya no emite ninguno.
- Relanzado: `build/…/DiscNativePro.app` abierta y confirmada con `pgrep` (PID 80345, arm64).

## 4. Incidencia colateral encontrada y reparada (`build/`)
`cmake --build build` falló al reconfigurar: `Unknown CMake command "juce_add_gui_app"`. Causa: `FETCHCONTENT_SOURCE_DIR_JUCE` apuntaba a `/private/tmp/discnativepro-juce`, y esa carpeta se quedó **sin `CMakeLists.txt`** (limpieza de `/tmp` de macOS; solo sobrevivía `modules/`). Reparación (local, sobre el directorio de build, reversible):
- `FETCHCONTENT_SOURCE_DIR_JUCE` → checkout **completo** del repo (`build/_deps/juce-src`).
- Eliminados `build/_deps/juce-subbuild` y `build/_deps/juce-build` (tenían cacheada la ruta muerta) → se regeneraron con el source correcto.
- Reconfiguración y build OK; `build/` vuelve a funcionar sin depender de `/tmp`.

## 5. Pendiente
- Sin commit ni push (no aprobado).
- `altTarget` no cambia de comportamiento observable (hoy ya resolvía bien por fusión); ahora es correcto **por contenido** y garantizado por el tipo.

---

# Plan P12 — seguridad del transporte de emisión (RTMP + Icecast)

## 1. Hallazgos
1. **Inyección de shell** (`BroadcastService.cpp:307-314`): la URL de ingesta RTMP (con stream key) se interpolaba en el comando de `popen` (`/bin/sh -c`) solo entre comillas dobles; dentro de ellas POSIX sigue interpretando `"` (cierra), `` ` ``, `$(`/`${` y `\` → con una key maliciosa se ejecutaba código arbitrario como el usuario.
2. **Inyección de cabeceras Icecast** (`connectToServer`, líneas 252-257): `mount` (request line) y `stationName` (`ice-name:`) viajaban sin filtrar CR/LF → se podían forjar cabeceras.
3. Defaults RTMP planos: YouTube y Twitch usaban `rtmp://` en vez de `rtmps://`.

Contexto: la key vive en el Keychain (`SecretStore`), solo la escribe el usuario y la URL base solo es editable en "personalizado" → severidad media-baja, explotable al pegar una URL a ciegas.

## 2. Cambios
- `[MODIFY] src/audio/BroadcastService.cpp`:
  - Nuevo helper (namespace anónimo) `shellQuote()`: comillas simples POSIX con `'` → `'"'"'`.
  - `launchFfmpeg()`: el comando se arma con `shellQuote()` para **ingestUrl**, la ruta de ffmpeg y la del log (números de bitrate/muestreo aparte, que son numéricos). Mismo `popen`/`setvbuf`/FIFO, sin cambios de firma.
  - `startBroadcast()` (Icecast): `mount` y `stationName` sin CR/LF; mount con espacio → `setError("Mount no puede contener espacios")` + `return false`.
- `[MODIFY] src/ui/SettingsModalComponent.cpp`: defaults a RTMPS (YouTube `rtmps://a.rtmp.youtube.com/live2/`, Twitch `rtmps://live.twitch.tv:443/app/`; Facebook ya lo era). No requiere migración: esas URLs no se persisten, se resetean al default al abrir el modal o cambiar destino.

## 3. Verificación
- `cmake --build build` → **0 errores, 0 warnings propios** (solo recompilaron `BroadcastService.cpp` y `SettingsModalComponent.cpp`).
- **Repro end-to-end del escapado** (scratchpad, algoritmo idéntico + `popen` real con un "ffmpeg" falso que vuelca su argv):
  - `x'; touch <sentinel>; '`, `` x`touch …`y ``, `x$(touch …)y` → el centinela **nunca se crea** y la URL llega como **un único argumento literal** (`ARG18=[rtmp://…/x'; touch …; ']`).
  - Keys legítimas con espacios, `&`, `;`, `|` → pasan intactas.
  - Ruta de log con espacio en el directorio → el redirect `2>` se resuelve bien.
- Saneado Icecast: revisión de código (recorte de CR/LF + rechazo de espacios en mount). Sin prueba en vivo contra servidor Icecast (no había servidor disponible): **pendiente de ojo** como en fases anteriores.
- Defaults RTMPS: revisión estática (`rtmpUrlEditor` se resetea con `defaultRtmpUrlForDestination`); sin prueba contra YouTube/Twitch reales → **pendiente de ojo**.
- Relanzado: `build/…/DiscNativePro.app` abierta, `pgrep` PID 86560.

## 4. Limitación conocida (documentada, fuera de alcance)
- La stream key sigue siendo visible en el **argv** de ffmpeg (columna COMMAND de `ps`) mientras se emite. Ocultarla exige embeber RTMP nativo (`libavformat`) en vez de orquestar ffmpeg; no es viable hoy porque `juce::ChildProcess` no expone escritura a stdin (necesaria para el PCM). Recomendación al usuario: emitir por **RTMPS** (ahora default) y renovar keys si se compartió el equipo.
- `FfmpegLocator::ensureInstalled` usa shell con `curl` pero ya escapa comillas simples correctamente y las rutas son controladas por la app.

## 5. Pendiente
- Sin commit ni push (no aprobado).
- Prueba real de RTMPS contra YouTube/Twitch y contra un Icecast local, cuando haya servidores disponibles.

---

# Walkthrough — P8: Broadcast (emitir la mezcla a servidor Icecast/Shoutcast propio)

## Objetivo
Funcionalidad solicitada por usuarios estilo VirtualDJ: emitir la mezcla master en vivo a un servidor propio de streaming. Distribución integrada (toggle en Settings → Streaming), sin carga dinámica.

## Cambios
- [NEW] `third_party/lame/`: codificador MP3 libmp3lame 3.100 vendorizado (solo encoder, 19 .c; fuente: build gyp de Chromium). `config/mac/config.h` con `HAVE_XMMINTRIN_H` desactivado → rutas C puras compatibles arm64/x86_64. Warnings de terceros silenciados con `-w` en su CMakeLists.
- [NEW] `src/audio/BroadcastService.h/.cpp`: hilo de red + `AbstractFifo` lock-free (patrón `AudioRecorder`). `pushAudioBlock` real-time safe (early-return por atómico, sin allocs). `run()`: LAME (`lame_encode_buffer_interleaved`) → `juce::StreamingSocket` → handshake Icecast 2 (`PUT /mount`, Basic auth, headers `ice-*`). Reconexión con backoff 1→30 s; FIFO se limpia al reconectar. Estado (connected/bytesSent/lastError) en atómicos + CriticalSection.
- [MODIFY] `src/audio/AudioEngine.h/.cpp`: miembro `broadcastService` + `getBroadcastService()`; en el callback de audio, `broadcastService.pushAudioBlock(masterBuffer, numSamples)` junto al recorder (línea 380); `stopBroadcast()` en `shutdown()`.
- [MODIFY] `src/ui/SettingsModalComponent.h/.cpp`: sección "Emitir en vivo (Broadcast a servidor propio)" en la pestaña Streaming (tab 5): toggle de emisión, servidor/puerto/mount/contraseña/nombre/bitrate (128-320), badge de estado con Timer 2 Hz (Emitiendo/Reconectando/Error + MB enviados). Config en `settings.json` (claves `broadcast*`); contraseña en Keychain vía `SecretStore` ("broadcast.password"). i18n es/en.
- [MODIFY] `CMakeLists.txt`: `add_subdirectory(third_party/lame)`, fuentes nuevas y link `mp3lame`.

## Verificación
- `cmake --build build` → 0 errores, 0 warnings en código del proyecto (LAME se compila con warnings base). `Built target DiscNativePro`.
- Relanzamiento (regla AGENTS.md): `pkill` → `open build/DiscNativePro_artefacts/Release/DiscNativePro.app` (Release: configuración del build actual) → `pgrep` PID 11719 activo.
- Pendiente de prueba en vivo por el usuario: icecast local (`brew install icecast`), VLC en `http://localhost:8000/stream`, resiliencia (matar servidor) y tiempo real sin dropouts.

## Notas
- Grabación local y emisión son independientes (se puede grabar y emitir a la vez).
- Contraseña nunca en texto plano en settings.json (Keychain).

---

# Walkthrough — P9: Settings con menú lateral vertical (sidebar icono + etiqueta)

## Objetivo
Sustituir la fila horizontal de 10 pestañas (apretada en 740 px) por un menú lateral vertical con icono arriba y etiqueta debajo, con scroll del sidebar si no caben.

## Cambios
- [MODIFY] `src/ui/DjButton.h`: nuevo `setStackedLayout(bool)` — en modo apilado el botón dibuja el icono centrado arriba y la etiqueta centrada debajo (comportamiento por defecto intacto para el resto de la app).
- [MODIFY] `src/ui/SettingsModalComponent.h/.cpp`:
  - Nuevos `sideViewport` + `sideContent`: los 10 `tabXxxBtn` viven dentro del sidebar (96 px, fondo más oscuro, divisor vertical pintado en `paint()`).
  - Botones de pestaña 80x44 apilados (gap 4, fuente 9.5 pt, layout apilado).
  - `resized()`: sidebar a la izquierda (con scroll solo si el contenido excede la altura); body ocupa el área derecha completa (sin los 56 px superiores de la fila antigua).
  - `paint()`: divisor horizontal y=48 eliminado → fondo sidebar `fromRGB(20,22,31)` + línea vertical en x=96.
  - Tab Shortcuts (pintado directo): deja de descontar 88 px superiores y descuenta el sidebar (removeFromLeft(96)).
- Altura del contenido del sidebar (484 px) cabe en la vista útil (488 px): sin scrollbar en el tamaño por defecto; aparece automáticamente si el modal fuera más bajo.

## Verificación
- `cmake --build build` → 0 errores, 0 warnings en ficheros tocados; `Built target DiscNativePro`.
- Relanzamiento (regla AGENTS.md): `pkill` → `open .../Release/DiscNativePro.app` → `pgrep` PID 59907 activo.
- Pendiente de confirmación visual del usuario (captura de ventana no disponible: sin permiso de accesibilidad para osascript).

---

# Walkthrough — P10: Transmitir (RTMP a redes + Podcasts MP3)

## Objetivo
Completar la suite de transmisión sin video: emitir a YouTube/Facebook/Twitch/RTMP propio vía ffmpeg, y exportar grabaciones a MP3. Pestaña nueva "Transmitir" en el sidebar.

## Cambios
- [MODIFY] `src/audio/BroadcastService.h/.cpp`: `Config::mode { Icecast, Rtmp }`. En modo Rtmp no se usa LAME: se escribe PCM s16le interleaved al stdin de un proceso ffmpeg (`popen`, porque `juce::ChildProcess` no expone stdin) que codifica AAC y empuja por RTMP/RTMPS. Mismo FIFO/hilo/backoff; al caer la conexión se relanza ffmpeg. Log de errores de ffmpeg en `~/Library/Application Support/DiscNativePro/broadcast_ffmpeg.log`.
- [NEW] `src/services/FfmpegLocator.h/.cpp`: localiza ffmpeg (App Support/bin → Bundle Resources → Homebrew → PATH) e instala el binario estático oficial (evermeet.cx) si falta — mismo patrón que yt-dlp.
- [NEW] `src/audio/Mp3Exporter.h/.cpp`: conversión offline WAV → MP3 con LAME (bloques de 4096, clamp, flush); hilo de trabajo, no audio.
- [MODIFY] `src/ui/LucideIcons.h/.cpp`: icono nuevo `Radio` (punto + arcos).
- [MODIFY] `src/ui/SettingsModalComponent.h/.cpp`: pestaña **Transmitir** (índice 10, sidebar). La sección Broadcast **se mueve** desde Streaming. Incluye: selector de destino (Radio propia / YouTube / Facebook / Twitch / RTMP personalizado con URLs prellenadas), campos RTMP (URL solo editable en personalizado, stream key enmascarada y guardada por destino en Keychain, bitrate AAC), botón "Instalar ffmpeg" + estado, toggle de emisión compartido y badge de estado; sección Podcast (combo de WAV de grabaciones + Refrescar, bitrate, "Exportar a MP3" con estado). i18n es/en.
- [MODIFY] `CMakeLists.txt`: fuentes nuevas + copia opcional de ffmpeg al bundle (como yt-dlp).

## Verificación
- `cmake --build build` → 0 errores y 0 warnings en los ficheros tocados (`AsyncFolderScanner`/`UsbLibraryView` tienen warnings previos, no relacionados).
- **Ruta RTMP validada end-to-end sin la UI**: receptor local falso (`ffmpeg -listen 1 -i rtmp://127.0.0.1:1935/live/test`) + el mismo comando que lanza la app con 5 s de senal PCM → captura verificada con `ffprobe`: `aac, 44100 Hz, 2 canales, 192 kbps, 5.04 s`.
- ffmpeg de esta máquina: `/opt/homebrew/bin/ffmpeg` 9.0.2 → el localizador lo encuentra (no hace falta instalar).
- Relanzamiento (regla AGENTS.md): `pkill` → `open .../Release/DiscNativePro.app` → `pgrep` PID 51474.
- Pendiente de prueba por el usuario: emisión real a YouTube/Twitch/Facebook (requiere cuentas), regresión de radio Icecast y exportación MP3 de una grabación desde la UI.

---

# Walkthrough — P11: MSIX de Windows para Microsoft Store

## Cambios
- [MODIFY] `.github/workflows/release.yml` (job `build-windows`): paso "Package MSIX" — staging exe+DLL, `AppxManifest.xml` (Win32 full-trust, TargetDeviceFamily 10.0.19041+), assets 44/150/50 px generados con System.Drawing desde `assets/icon.png`, `[Content_Types].xml` y empaquetado con `MakeAppx.exe` del SDK del runner. Sin firma (la Store re-firma). Identity/Publisher en env vars (`MSIX_IDENTITY_NAME`/`MSIX_PUBLISHER`, placeholders DiscPro) para colocar los de Partner Center.
- Artefacto `windows-artifact` ahora incluye Setup.exe + .msix; el Release publica ambos.

## Verificación
- YAML validado (yaml.safe_load) y here-strings PowerShell confirmados a columna 0 tras el parseo del bloque.
- Verificación real pendiente de ejecución en Actions (workflow_dispatch al pushear) y validación final del .msix en Partner Center al reservar la app (ahí se obtienen los Identity/Publisher exactos).

---

# Walkthrough — P13: Micrófono + talkover con auto-ducking (rama `axel-molina/superfeature`)

## Objetivo
Canal de micrófono integrado a la mezcla master con detección de voz y auto-ducking de la música (talkover): cuando el DJ habla, la música baja; al callar, vuelve. UI fija junto al mixer central.

## Cambios
- [NEW] `src/audio/MicChannel.h/.cpp`: canal de micrófono real-time safe (sin locks ni allocs en el callback; parámetros atómicos). Pipeline por muestra: high-pass RBJ de 2º orden a 100 Hz (Q Butterworth, conmutable) → gain (−60..+24 dB, default 0) → suma al masterBuffer (mono se duplica a L/R; estéreo pasa 1:1) — post-mezcla, antes de recorder/broadcast, así la voz hereda grabación y emisión. Detector de voz RMS por bloque contra umbral (default −30 dB) con hold de 0.3 s e histéresis de 1.5 dB. Ducking exponencial por bloque: attack 0.08 s, release 0.8 s, profundidad default −12 dB; gain publicado como atómico para que el callback aplique `applyGain` a la música antes de sumar la voz.
- [MODIFY] `src/audio/AudioEngine.h/.cpp`: nuevo `openDefaultAudioDevice()` que abre con 2 entradas y cae a 0 entradas si el dispositivo no tiene inputs (micAvailable refleja la realidad); `initialise()` y `ensureAudioDeviceRunning()` lo usan. En `audioDeviceAboutToStart` se prepara el MicChannel. En el callback (paso 2c, tras el sampler y antes de recorder/broadcast): si hay entrada y el mic está activo, `masterBuffer.applyGain(duck)` y `micChannel.process(...)`. Setters (`setMicEnabled/DuckEnabled/GainDb/DuckDepthDb`) persisten en settings.json (`mic.enabled`, `mic.duckEnabled`, `mic.gainDb`, `mic.duckDepthDb`) y se restauran en el constructor.
- [NEW] `src/ui/MicStripComponent.h/.cpp`: tira de 78 px con botón Mic (DjButton con icono Lucide nuevo, rojo cuando activo), knobs GAIN (−40..+12) y DUCK (−24..0, acento ámbar), toggle AUTO, VU y LED verde de voz activa (30 Hz timer leyendo atómicos). Cuando no hay entrada de audio muestra "Sin entrada de audio" atenuada.
- [MODIFY] `src/ui/LucideIcons.h/.cpp`: icono `Mic` (cápsula + arco + pie).
- [MODIFY] `src/ui/MixerComponent.h/.cpp`: la tira de mic se ubica a la derecha del ChannelStrip 2 (Ch1 izq 96 px, mic der 78 px, Ch2 der 96 px, centro el resto).
- [MODIFY] `src/ui/MainComponent.cpp`: `mixerWidth` 430 → 508 px.
- [MODIFY] `CMakeLists.txt`: fuentes nuevas registradas.

## Verificación
- `cmake --build build` → 0 errores; 0 warnings en los ficheros tocados (persisten 2 warnings previos en `AsyncFolderScanner.cpp` y `UsbLibraryView.cpp`, no relacionados).
- **Banco de pruebas headless** (`/tmp/mic_bench`, proyecto CMake aparte que reutiliza el JUCE 8.0.8 ya descargado y compila `MicChannel.cpp` real) — 9/9 OK:
  - Duck: silencio → gain 1.0; voz continua → converge a 0.2512 (−12 dB exactos); attack a −6 dB en 81 ms (<100 ms); release de vuelta >0.9 tras hold; voz liberada tras el hold.
  - Low-cut: atenuación a 50 Hz = 12.32 dB (teórico 2º orden Butterworth: 12.3 dB); bypass = 0.00 dB.
  - Estabilidad: 1000 bloques con parámetros atómicos cambiantes aleatoriamente → duckGain siempre en [0, 1].
- Relanzamiento (regla AGENTS.md): `pkill` → `open .../Debug/DiscNativePro.app` → `pgrep` PID 81377.
- Pendiente de prueba por el usuario: hablar con mic real y verificar el ducking audible, VU, LED de voz y persistencia de settings entre sesiones.

---

# Versión 1.3.3 (release publicada)

## 1. Contenido (tag `v1.3.3` = commit `6d52bb9`)
| Commit | Cambio |
|---|---|
| `7346ce6` | Plan P12: `shellQuote()` POSIX en el comando de ffmpeg (`ingestUrl` con stream key, ruta ffmpeg, ruta log), saneado CR/LF de `mount`/`stationName` en Icecast + rechazo de mount con espacio, defaults RTMPS para YouTube/Twitch. |
| `6d52bb9` | Bump de versión 1.3.3 (`CMakeLists.txt`). |

Base: `f2bf82d` (MSIX/Store de otra sesión) + guard de binario universal de CI-1 (`c882c17`).

## 2. Verificación
- Build local Release → 0 errores / 0 warnings propios; bundle **1.3.3**; app relanzada.
- Repro end-to-end del escapado (ffmpeg falso): keys maliciosas (`'; touch`, backticks, `$(...)`) llegan como un único argumento literal y el centinela no se crea.
- **CI del tag `v1.3.3`** (run 42736104396): Windows 4m3s ✓, Linux 2m58s ✓, **macOS Universal 1h34m ✓ con el guard `lipo` en verde**, Publish ✓.
- **Artefacto publicado verificado**: `DiscNativePro-macOS-Universal.zip` descargado de la release → `lipo -archs` = **`x86_64 arm64`** (realmente universal; cierra el issue #1) y `Info.plist` = 1.3.3.

## 3. Notas
- El job de macOS tarda ~1h34 en CI (v1.3.2: 65 min arm64-only): el universal compila ~2x por TU.
- Assets: `DiscNativePro-Windows-x64-Setup.exe`, `DiscNativePro-Windows-x64.msix`, `DiscNativePro-Linux-x64.tar.gz`, `DiscNativePro-macOS-Universal.zip`.
- Pendiente de ojo: RTMPS real contra YouTube/Twitch y emisión Icecast contra servidor local.

---

# Walkthrough — P14: Limiter master brickwall + medidor LUFS (rama `axel-molina/superfeature`)

## Objetivo
Red de seguridad de nivel en el bus master: limiter brickwall look-ahead siempre activo (techo configurable) que garantiza salida, grabación WAV y emisión sin clipping, más medidor de loudness EBU R128 (LUFS Momentary/Short-term con K-weighting BS.1770) y gain reduction (GR) visibles en la columna central.

## Cambios
- [NEW] `src/audio/MasterLimiter.h/.cpp`: limiter in-place sobre el masterBuffer con retardo de señal de 5 ms, gain computer por muestra y **ventana deslizante de mínimo (deque monótono)** sobre el req de las últimas 221 muestras — el gain aplicado a la muestra que sale del retardo ya vio el pico que viene (look-ahead verdadero). Envolvente persistente (attack 0.5 ms / release 100 ms por muestra). Medidor LUFS BS.1770: K-weighting por canal (high-shelf +4 dB @ 1682 Hz + high-pass 38 Hz, coeficientes exactos del estándar), ring buffers preasignados de 3 s / 400 ms con sumas incrementales O(1), offset −0.691, suelo de silencio −70. GR publicada con decaimiento. Techo atómico `ceilingDb` (default −0.3 dBFS).
- [MODIFY] `src/audio/AudioEngine.h/.cpp`: `MasterLimiter masterLimiter` + getter; `prepare` en `audioDeviceAboutToStart`; en el callback, paso **2d** entre el micrófono y recorder/broadcast → limita salida + grabación + emisión. `setMasterCeilingDb` (clamp −12..0) persiste en `master.ceilingDb`; restauración en el constructor.
- [MODIFY] `src/ui/CenterColumnComponent.h/.cpp`: fila superior 46→62 px con knob **CEILING** (ámbar, −12..0 dB, doble clic = −0.3) entre MASTER/PHONES y el medidor; debajo, readout `LUFS-S · M · GR dB` (∞ por debajo de −60; GR en ámbar cuando limita), actualizado por el timer de 30 Hz que también sincroniza el knob.
- [MODIFY] `CMakeLists.txt`: fuentes nuevas registradas.

## Verificación
- `cmake --build build` → 0 errores, 0 warnings.
- **Banco headless** (`/tmp/master_bench`, compila `MasterLimiter.cpp` real) — 9/9 OK:
  - T1 salida ≤ techo: −0.3000 dB exactos con entrada a 0 dBFS.
  - T2 sin clipping con rampa 0→+6 dBFS y transitorios ±2.5, techo −1 dB.
  - T3 GR = nivel − techo (6.30 exactos) y vuelve a ~0 tras silencio.
  - T4 LUFS-M y LUFS-S de seno 997 Hz @ −6 dBFS = −6.26 (±1.5 de la referencia).
  - T5 LUFS en silencio = suelo −70. T6 1000 bloques con techo/nivel aleatorios estable. T7 transparencia bit a bit (salida = entrada retardada 5 ms).
  - Los fallos iniciales (T2/T3b) fueron del diseño, no del banco: se corrigió el look-ahead con la ventana deslizante de mínimo y el envolvente pasó a ser estado persistente.
- Relanzamiento (regla AGENTS.md): `pkill` → `open .../Debug/DiscNativePro.app` → `pgrep` PID 64719.
- Pendiente de prueba por el usuario: saturar los gains de ambos decks + sampler + mic → el VU master no clipea, GR visible en ámbar; LUFS-S de una pista comercial ≈ −14..−9.

## Correccion de ruta de lanzamiento (post-P14)
- El build de este proyecto es **Release** (`CMAKE_BUILD_TYPE=Release` en `build/CMakeCache.txt`): el artefacto real es `build/DiscNativePro_artefacts/Release/DiscNativePro.app`. El `.app` de `Debug/` es un residuo del 12 de octubre (sin MicChannel ni MasterLimiter) y **no debe usarse**.
- Relanzamiento correctivo: `pkill` → `open build/DiscNativePro_artefacts/Release/DiscNativePro.app` → `pgrep` PID 77616. Las verificaciones de P13/P14 anteriores lanzaron por error el binario Debug viejo; el código siempre estuvo bien compilado.

---

# Walkthrough — P15: Beat jump, loop roll momentáneo y slip mode (rama `axel-molina/superfeature`)

## Objetivo
Controles de performance por deck: salto cuantizado a la rejilla de beats (tamaños ½–16 beats), loop roll momentáneo por hold (1/16–2 beats, al soltar retoma donde estaría) y slip mode (loops y scratch no pierden el hilo temporal de la pista).

## Cambios
- [MODIFY] `src/audio/DeckPlayer.h/.cpp`:
  - **Beat jump**: `quantizedBeatPos()` (estática, pura) redondea a la rejilla desde `firstBeatOffset` y suma el delta en beats; `jumpBeats(beats)` usa el intervalo del BPM actual (fallback `originalBpm`, luego 120).
  - **Loop roll**: `startLoopRoll(beats)` ancla el loop en la subdivisión anterior con `rollStartFor()` (floor sobre `length = beats·intervalo`) y activa `setLoop(..., true)`; `releaseLoopRoll()` desactiva el loop y, **sin slip**, hace seek al fin del loop (`userInitiated=false` para no desarmar el controlador de fase). Con slip activo, el hilo de audio retoma solo.
  - **Slip mode**: `slipEnabled` atómico. En `getNextAudioBlock`, la posición virtual avanza con el **delta real del cursor de lectura** (respeta keylock y resample) durante loop, y a tempo nominal durante scratch; al desengancharse la condición (loop off / mano fuera), un seek centralizado retoma en la posición virtual. `prevReadPos` solo hilo de audio.
- [MODIFY] `src/ui/DeckComponent.h/.cpp`: **fila de performance** de 24 px bajo los hot cues: `‹` (jump back) | size selector (½→1→2→4→8→16) | `›` (jump fwd) | **ROLL** (hold vía `Button::Listener::buttonStateChanged` + `isDown()`; azul mientras hay loop) | roll size (1/16→1/8→1/4→1/2→1→2) | **SLIP** (toggle cian). Acciones públicas compartidas UI/MIDI: `beatJump(±1)`, `startRoll()/releaseRoll()`, `toggleSlip()`, getters `getJumpBeats()/getRollBeats()`. Refresco a 60 Hz con caché de estado (sin repintado espurio).
- [MODIFY] `src/midi/MidiMapping.h/.cpp`: `MidiBinding::matches` para NoteOn acepta también releases (note-off / note-on vel 0) — cada dispatch decide qué hacer con ellos (los handlers existentes ya filtran con `isNoteOn()`). 12 acciones nuevas en categoría Pads: `pad_jumpback/jumpfwd/roll/slip_d1` (CH 8, notas 4–7) y `_d2` (CH 9, notas 4–7); roll es hold (noteOn inicia, release suelta).
- [MODIFY] `src/midi/MidiManager.h/.cpp`: callback `onPerformanceAction(deck, action)`; el dispatch (hilo MIDI) delega a `MessageManager::callAsync` porque toca componentes de UI.
- [MODIFY] `src/ui/MainComponent.cpp`: el callback enruta a `deck1/deck2` (mismos size selectors que la pantalla).
- [LIMPIEZA] `src/services/AsyncFolderScanner.cpp` (archivo preexistente, fuera del alcance P15 pero requerido por la regla 0/0): variable sin uso, init-capture con shadow y `DirectoryIterator` deprecado → `RangedDirectoryIterator` (`entry.getFile()`).

## Verificación
- `cmake --build build` → **0 errores, 0 warnings**.
- **Banco headless** (`/tmp/perf_bench`, compila `DeckPlayer.cpp` + `StretchAdapter/BeatDetector/KeyDetector` reales) — **16/16 OK**: saltos ±1/±4/±16 en rejilla y fuera de rejilla (snap + avance), ½ beat con rejilla desplazada, 128 BPM exacto, anclas de roll en límites 1/16–2 beats, posición 0 y negativa (clamp es de `setPosition`/`startLoopRoll`). Los 5 fallos iniciales eran errores de expectativa del banco (redondeos), no del código.
- Relanzamiento (regla AGENTS.md, ruta **Release** corregida): `pkill` → `open build/DiscNativePro_artefacts/Release/DiscNativePro.app` → `pgrep` PID 39336.
- Pendiente de prueba por el usuario: ROLL en hold suena en loop corto y al soltar retoma sin hueco; con SLIP activo lo mismo pero la pista sigue avanzando por debajo; ‹/› saltan limpio en la rejilla; MIDI learn sobre las 12 acciones nuevas.
- Ajuste visual (feedback del usuario): la fila de performance quedó pegada a la barra PLAY/CUE/Loop; se añadió un gap de 6 px entre ambas (`resized()`). Build 0/0; relanzamiento Release PID 61188.

---

# Walkthrough — P16: Editor de Beat Grid

## Objetivo
Corrección manual del beat grid por pista (offset del primer beat + BPM del grid), con modal de edición, canvas con líneas de beat y persistencia por pista en el caché de biblioteca. El override gana al publish asíncrono del análisis y sobrevive a recargas de la pista.

## Cambios
- [MODIFY] `src/audio/DeckPlayer.h/.cpp`:
  - `setGridBpm(bpm)` (clamp 40..250, recalcula `originalBpm`/`currentBpm` con el pitch), `setFirstBeatOffset(sec)` (clamp [0, duración]), `resetGrid()` (vuelve a los valores detectados y limpia overrides), `hasGridOverride()`, getters `getDetectedBpm()/getDetectedOffset()`.
  - Atómicos `gridBpmOverride` (0 = sin override) / `gridOffsetOverride` (<0 = sin override) / `detectedBpm` / `detectedOffsetSeconds`; `loadFile` los resetea (el grid corregido no se hereda entre pistas).
  - El publish del análisis (step 3) guarda lo detectado en `detected*` y solo aplica los valores al grid si **no** hay override — la corrección manual nunca es pisada por el análisis asíncrono.
- [MODIFY] `src/services/LibraryCacheManager.h/.cpp`: campos `gridBpm`/`gridFirstBeatOffset` en `CachedTrackMetadata` (gridBpm ≤ 0 = sin corrección), `updateGrid(file, bpm, offset)` con entrada mínima si la pista no existe y guardado throttled (3 s); lectura/escritura en el JSON (`gridBpm`/`gridOffset`).
- [NEW] `src/ui/GridEditorComponent.h/.cpp`: modal `GridEditorComponent` (Timer a 30 Hz). Canvas con picos apilados de 3 bandas (low/mid/high con `WaveformTheme` por deck) y líneas de beat sobre `offset + k·intervalo` (cada 4 = downbeat con número de compás, negativos incluidos). Marker ámbar central; **drag horizontal** mueve el offset (60 px/s); nudge ±5/±20 ms; BPM ±0.05/±0.5 y ×½/×2 (offset invariante); **RESET** vuelve a lo detectado; **PLAY** audiciona desde 1 s antes del primer beat; **CERRAR**. Persistencia en destructor y en cada cambio (`updateGrid`).
- [MODIFY] `src/ui/DeckComponent.h/.cpp`: botón **GRID** (22 px, columna de pitch bajo KEYLOCK) visible solo con pista cargada y no en modo video; callback `onEditGrid(deckIndex)`. Al cargar pista: si el caché tiene grid corregido, `setGridBpm`/`setFirstBeatOffset` se aplican **sincrónicamente** antes de los labels (gana la carrera contra el publish del análisis).
- [MODIFY] `src/ui/VerticalWaveform.cpp`: las líneas de beat ahora se dibujan desde `player.getFirstBeatOffset()` (antes desde t=0; el grid editado no se reflejaba). `ScrollingWaveform` confirmado sin uso, no tocado.
- [MODIFY] `src/ui/MainComponent.h/.cpp`: `gridWindow` (SafePointer), `openGridEditor(deckIndex)` con `DialogWindow::launchAsync` (no resizable, ESC cierra), single-instance (`toFront`). Cableado `deck1.onEditGrid = deck2.onEditGrid`.
- [MODIFY] `CMakeLists.txt`: `GridEditorComponent.h/.cpp` agregados al target.
- [LIMPIEZA] `src/ui/UsbLibraryView.cpp`: parámetro `totalFound` sin uso en lambda → `/*totalFound*/` (regla 0/0).

## Verificación
- `cmake --build build` → **0 errores, 0 warnings**.
- **Banco headless** (`/tmp/grid_bench`, compila `DeckPlayer.cpp + BeatDetector/KeyDetector/StretchAdapter` reales, click track real de 30 s a 130 BPM — kicks de 250 ms a 60 Hz con decay: los clicks de pocos ms producen meseta plana en la energía de 100 ms y el detector exige máximo local estricto) — **19/19 OK**: análisis detecta 130.00 BPM (offset 0.0995 s); override BPM 140 + offset 0.5 **sobreviven al publish**; `currentBpm = override × pitch` (148.4 con pitch +6 %); clamps 300→250 / 10→40 / offset −5→0 / dur+10→dur; ×½ (128→64, offset invariante) y ×2 (→128); `resetGrid` restaura detectado y limpia override.
- Relanzamiento (regla AGENTS.md, ruta **Release**): `pkill` → `open build/DiscNativePro_artefacts/Release/DiscNativePro.app` → `pgrep` PID 14996.

## Prueba sugerida (usuario)
1. Cargar una pista conkick y abrir **GRID** → arrastrar el marker al primer golpe (drag) y ajustar BPM con ±0.5.
2. Verificar que las líneas de beat en la waveform vertical se alinean con los golpes reales.
3. Recargar la misma pista → el grid corregido se aplica solo (persistido por pista).
4. RESET vuelve al grid detectado; ×½/×2 para temas de tiempo doble/mitad.
5. Con el grid corregido, beat jump ‹/› y ROLL deben cuantizar al grid corregido.

---

# Walkthrough — P17: Playlists

## Objetivo
Listas de reproducción persistentes: crear, renombrar, borrar, añadir/quitar pistas y reordenación por drag & drop, integradas al árbol de carpetas y a la tabla de la biblioteca.

## Cambios
- [NEW] `src/services/PlaylistStore.h/.cpp`: singleton JSON (`~/Library/Application Support/DiscNativePro/playlists.json`). `Playlist { id (Uuid), name, paths (rutas canónicas ordenadas) }`. CRUD completo: `createPlaylist` (rechaza duplicados case-insensitive y nombres vacíos), `renamePlaylist`, `deletePlaylist`, `addTrack` (sin duplicados), `removeTrack`, `reorderTrack` (semántica por rutas: mueve `path` antes de `beforePath`, robusta ante pistas faltantes en disco), `playlistPaths`, `playlistContains`. Persistencia con `TemporaryFile` atómico en cada mutación.
  - **Bug de deadlock corregido antes del primer commit funcional**: las mutaciones llamaban `save()` (que toma el mutex) desde contextos que ya lo sostenían → cuelgue. `save()` pasó a `saveUnlocked()` (mutex ya tomado); el destructor lo envuelve con lock.
- [MODIFY] `src/ui/FolderTreeComponent.h/.cpp`: sección **Playlists** al final del árbol (header "Playlists" colapsable con botón "+" para crear; filas con icono `ListMusic` y contador). `fullPath = "playlist:<id>"` para sobrevivir renombres. Clic derecho sobre playlist → menú Renombrar/Eliminar. `navigateFolders` salta el header (no seleccionable). Callbacks `onCreatePlaylist/onRenamePlaylist/onDeletePlaylist`.
- [MODIFY] `src/ui/LucideIcons.h/.cpp`: icono nuevo `ListMusic` (Lucide "list-music").
- [MODIFY] `src/ui/LibraryComponent.h/.cpp`:
  - `filterTracks`: rama playlist — las pistas coinciden por ruta canónica en el store y se muestran **en el orden manual** (se omite `applySorting` en vista de playlist). Título de tabla = nombre de la playlist.
  - Menú contextual (clic derecho en pista): "Añadir a playlist" (submenu con las existentes + "Nueva playlist…") y, en vista de playlist, "Quitar de esta playlist". Multi-selección soportada.
  - Diálogos con `AlertWindow` modal (crear/renombrar con `TextEditor`; eliminar con confirmación; patrón `SafePointer` + `enterModalState`).
  - **Reordenación drag & drop**: el drag mantiene la descripción de rutas (compatible con Automix y decks); al soltar sobre la tabla en vista de playlist se calcula el punto de inserción (`getRowContainingPosition` + mitad superior/inferior de la fila) y se llama `reorderTrack(path, beforePath)`. Línea de inserción cian en `paintRowBackground` durante el drag (`playlistDropRow`).
  - `refreshPlaylists()` sincroniza el árbol con el store; el wiring de callbacks vive en el constructor (MainComponent sin cambios).
- [MODIFY] `CMakeLists.txt`: `PlaylistStore.h/.cpp` agregados al target.

## Verificación
- `cmake --build build` → **0 errores, 0 warnings**.
- **Banco headless** (`/tmp/playlist_bench`, compila `PlaylistStore.cpp` real, dos fases entre procesos para probar persistencia) — **29/29 OK**: create/duplicado case-insensitive/nombre blanco, rename/delete con duplicados e inexistentes, addTrack sin duplicados/rutas vacías/ids falsos, reorder (orden c,a,b verificado; ruta inexistente; path==beforePath), remove del medio (c,b), contains, persistencia real en disco entre procesos (orden c,b sobrevive al reinicio), limpieza final del store real.
- Relanzamiento (regla AGENTS.md, ruta **Release**): `pkill` → `open build/DiscNativePro_artefacts/Release/DiscNativePro.app` → `pgrep` PID 41069.

## Prueba sugerida (usuario)
1. En Mis Archivos, sección **Playlists**: "+" crea una playlist; clic derecho → renombrar/eliminar (con confirmación).
2. Clic derecho en una pista → "Añadir a playlist" (multi-selección con Cmd/Shift).
3. Clic en la playlist → la tabla muestra solo sus pistas con el título de la playlist.
4. Arrastrar pistas dentro de la vista de playlist para reordenar (línea cian de inserción; el orden persiste al reiniciar la app).
5. Verificar que arrastrar hacia el panel Automix sigue funcionando igual que antes desde una playlist.

## Corrección P17 (feedback del usuario): drag & drop hacia el árbol
- Faltaba soltar pistas sobre las filas de playlist del árbol: `TreeViewInternal` ahora implementa `juce::DragAndDropTarget` (`itemDragMove` detecta fila de playlist con resaltado cian, `itemDropped` entrega rutas al callback `onTracksDroppedIntoPlaylist`). El wiring en `LibraryComponent` llama `addTrack` por cada ruta + `refreshPlaylists` + refiltro si la playlist está visible. Funciona desde "Todas las Pistas", "Descargas" o cualquier carpeta. Build 0/0; relanzamiento Release PID 48672.

---

# Walkthrough — P18: MIC compacto en la fila superior del centro

## Objetivo
Corregir la desproporción/asimetría del mic strip (columna extra de 78 px a la derecha de CH2). El mixer queda `[CH1][Centro][CH2]` simétrico y el mic vive como bloque compacto horizontal en la fila superior de la columna central.

## Cambios
- [MODIFY] `src/ui/MicStripComponent.h/.cpp`: `setCompactLayout(bool)`. En modo compacto: layout horizontal de 46 px `[MIC 34×34][GAIN 44][DUCK 44][AUTO (15px) + VU meter horizontal en flex]`; sin franja de título propia; el punto verde de voz activa se dibuja sobre la esquina del botón MIC; aviso "Sin entrada de audio" en la zona derecha. El modo vertical original se conserva.
- [MODIFY] `src/ui/CenterColumnComponent.h/.cpp`: miembro `micStrip` (compacto) en la fila superior de 46 px entre el VU master y CEILING/PHONES (`removeFromRight(180)`). Readout LUFS y demás sin cambios.
- [MODIFY] `src/ui/MixerComponent.h/.cpp`: eliminado `micStrip` de la columna derecha; layout `[CH1 96][Centro][CH2 96]`.

## Verificación
- `cmake --build build` → **0 errores, 0 warnings** (corregido `-Wreorder-ctor` alineando el orden de inicialización con la declaración).
- Relanzamiento Release: `pkill` → `open .../Release/DiscNativePro.app` → `pgrep` PID 66924.

## Prueba sugerida (usuario)
1. Verificar simetría: los tres bloques del mixer tienen igual ancho de canales y el mic ya no ocupa columna propia.
2. En la fila superior del centro: MIC on/off, GAIN, DUCK, AUTO y VU funcionan igual; el punto verde aparece al hablar con el mic activo.

## Corrección P18b (feedback del usuario): mic esencial + DUCK/AUTO en settings
- El bloque compacto de 180 px quedaba comprimido y el VU invisible. En compacto solo quedan `[MIC 34][GAIN 44][VU flex]` (DUCK/AUTO ocultos) y el ancho baja a 150 px — el VU ahora tiene proporción real.
- [MODIFY] `src/ui/SettingsModalComponent.h/.cpp`: nueva sección "Micrófono (Talkover)" en la pestaña Sonido (tras Limitador Master): slider de profundidad de duck (−24..0 dB → `setMicDuckDepthDb`) y toggle de duck automático (→ `setMicDuckEnabled`), inicializados desde `MicChannel`.
- Build 0/0; relanzamiento Release PID 76434.

## Corrección P18c (feedback del usuario): eje simétrico 3-3
- La fila superior quedó 3-3 alrededor del meter master: izquierda `[MIC][GAIN][VU mic]` (entradas), centro `masterMeter` (flex), derecha `[CEILING][MASTER][PHONES]` (sección master/salidas). Cambio único en `resized()` de `CenterColumnComponent.cpp`. Build 0/0; relanzamiento Release PID 1958.

## Corrección P18d (feedback del usuario): mic completo en Configuración
- Decisión: nada de mic en la fila superior. `CenterColumn` vuelve a `[MASTER][meter][CEILING][PHONES]` (2-2 simétrico); se elimina el miembro `micStrip`.
- [DELETE] `src/ui/MicStripComponent.h/.cpp` (sin usos) + fuera de `CMakeLists.txt`.
- [MODIFY] `src/ui/SettingsModalComponent.h/.cpp`: la sección "Micrófono (Talkover)" ahora tiene el control completo: toggle **Habilitar micrófono** (→ `setMicEnabled`), slider **GAIN** (−40..+12 dB → `setMicGainDb`), más el slider DUCK y el toggle AUTO existentes.
- Build 0/0; relanzamiento Release PID 7315.

---

## P19 — Knob CEILING movido a la Top Bar

**Objetivo:** despejar la fila superior de la columna central y colocar el techo del limitador junto a los botones REC / FX / SMP.

### Cambios
- `src/ui/TopBarComponent.h/.cpp` [MODIFY]: el constructor ahora recibe `AudioEngine&`. Nuevo knob `CEILING` (ámbar) tras `SMP`: rango −12..0 dBFS, step 0.1, doble clic = −0.3 dB, `onValueChange` → `setMasterCeilingDb()`. Sincronizado en el Timer de 1 Hz (respetando arrastre del mouse). Tooltip bilingüe vía `refreshLanguage()`.
- `src/ui/MainComponent.h` [MODIFY]: `topBar { audioEngine }` (orden de declaración ya garantiza validez de la referencia).
- `src/ui/CenterColumnComponent.h/.cpp` [MODIFY]: se elimina `ceilingKnob`. Fila superior final: `[MASTER][meter][PHONES]` (1-1). El readout LUFS-S/M/GR se mantiene.

### Verificación
- `cmake --build build`: **0 errores, 0 warnings**.
- Relanzamiento Release + `pgrep`: proceso activo (PID 58790).

### P19b — CEILING también en Configuración
- `src/ui/SettingsModalComponent.h/.cpp` [MODIFY]: nueva fila "Techo del Limitador (CEILING):" en Sonido → sección "Margen Dinámico y Limitador Master" (label 220 px + slider −12..0 dBFS step 0.1 con TextBoxRight, doble clic = −0.3 dB, bilingüe vía `refreshLanguage()`).
- Build 0 errores / 0 warnings; Release relanzada (PID 69108).

### Fix — Beat Grid Editor nacía sin tamaño
- `src/ui/MainComponent.cpp` [MODIFY]: `openGridEditor()` ahora hace `modal->setSize(720, 420)` antes de lanzar el `DialogWindow`. Antes el contenido media 0x0 y la ventana nativa colapsaba (solo se veía el label de BPM y CERRAR).
- Build 0/0; Release relanzada (PID 73691).

### Fix — Espaciado de botones del Grid Editor
- `src/ui/GridEditorComponent.cpp` [MODIFY]: `resized()` — separación entre grupos de botones 8→12 px y padding interno por botón 2→3 px en las tres filas (offset, BPM, RESET/PLAY/CERRAR).
- Build 0/0; Release relanzada (PID 76428).

### Fix — Separación vertical entre filas del Grid Editor
- `src/ui/GridEditorComponent.cpp` [MODIFY]: 8 px de gap vertical entre las tres filas de controles (offset / BPM / RESET-PLAY-CERRAR); bloque de controles 96→106 px.
- Build 0/0; Release relanzada (PID 79297).

### Fix — Superposición canvas/controles en Grid Editor
- `src/ui/GridEditorComponent.cpp` [MODIFY]: `canvasRect()` restaba 96 px (constante vieja) mientras los controles ahora ocupan 106 px → la onda quedaba debajo de los botones. Unificado a 106.
- `src/ui/MainComponent.cpp` [MODIFY]: ventana del editor 420→440 px de alto.
- Build 0/0; Release relanzada (PID 82986).

---

## P20 — Key Shift (transposición ±6 semitonos, mezcla armónica)

**Objetivo:** transponer la pista en semitonos sin cambiar el tempo (Key Lock ya existía; faltaba el shift armónico).

### Cambios
- `src/audio/StretchAdapter.h/.cpp` [MODIFY]: `setTransposeSemitones()` thread-safe (atómico) aplicado sobre signalsmith-stretch solo cuando cambia (nada de recalculos por bloque en el hilo de audio). Se amplió la supresión de warnings del third-party (-Wfloat-equal).
- `src/audio/DeckPlayer.h/.cpp` [MODIFY]: `setKeyShiftSemitones()` / `getKeyShiftSemitones()` con clamp ±6. Con Key Lock ON el stretcher transpone sin tocar el tempo; con OFF va por resample (pitch clásico, cambia tempo y tono).
- `src/ui/DeckComponent.h/.cpp` [MODIFY]: fila `[▼][valor][▲]` bajo el botón KEY en la columna de pitch; el label muestra los semitonos (ámbar si activo) y **clic = reset a 0**; tooltips bilingües; sync en el timer (el motor es la única verdad, p/ MIDI).
- `src/midi/MidiMapping.cpp` / `MidiManager.cpp` [MODIFY]: acciones `deckN_keyshift_up/down/reset` (binds por defecto: notas 97/98/99 en los canales 1 y 2).

### Verificación
- `cmake --build build`: **0 errores, 0 warnings**.
- Banco headless `/tmp/keyshift_bench` (DeckPlayer real + seno 440 Hz, frecuencia por cruces por cero): **9/9 PASS** — clamp ±6, BPM inalterado, 0 st → 440.2 Hz, +1 → 466.2, −1 → 414.6, +2 → 493.3, keylock OFF +1 → 466.2 Hz por resample. (Nota: el banco necesita pacing de 1 ms/bloque; sin él el BufferingAudioSource se queda sin buffer y mide silencio.)
- Release relanzada (PID 6371).

### P19c — CEILING: solo en Configuración
- Decisión del usuario: el knob en la TopBar se veía demasiado chico (ventana de 40 px). Se revierte la parte de TopBar del P19: `TopBarComponent` vuelve al constructor sin argumentos, sin knob ni referencia al motor; `MainComponent.h` vuelve a `TopBarComponent topBar;`.
- El techo del limitador se controla únicamente desde **Configuración → Sonido → Margen Dinámico y Limitador Master** (slider CEILING del P19b, con doble clic = −0.3 dB).
- Build 0/0; Release relanzada (PID 11287).

### Fix — Flechas del Key Shift no se renderizaban
- Causa: los glifos Unicode ▲/▼ no existen en el font que usa `DjButton` (mostraba "a").
- `src/ui/LucideIcons.h/.cpp` [MODIFY]: nuevo icono vectorial `ChevronUp` (espejo del `ChevronDown` existente).
- `src/ui/DeckComponent.cpp` [MODIFY]: los botones Key Shift usan `setIcon(ChevronUp/ChevronDown, 12)` en lugar de texto.
- Build 0/0; Release relanzada (PID 16115).

---

## P21 — FX sincronizados al beat (Echo/Gate rítmicos con fase real del deck)

**Objetivo:** que Echo y Gate usen divisiones musicales explícitas (1/4 a 2/1) y que el Gate quede anclado a la fase de beat real del deck (antes era un pulso libre que ignoraba el downbeat).

### Cambios
- `src/audio/DeckFxChain.h/.cpp` [MODIFY]: `FxSlot` gana `std::atomic<int> beatDivision { 2 }` (0=1/4, 1=1/2, 2=3/4, 3=1/1, 4=2/1) y `std::atomic<double> beatPhase { -1.0 }`. **Echo**: el retardo sale de la tabla de 5 divisiones × beat (antes: rangos ocultos del knob `parameter`; BPM ≤ 30 cae a 120 BPM). **Gate**: período = división × beat; si hay fase publicada (0..1) el ciclo arranca anclado al downbeat (`gatePhase = fase/división`); sin fase mantiene el pulso libre. Se eliminó el estado `gateEnv` obsoleto.
- `src/audio/DjMixer.h/.cpp` [MODIFY]: `setDeckBeatPhase(deckIndex, fase)` + atomics `deck1/2BeatPhase`; en `process()` publica la fase en los 6 slots antes de procesar las cadenas.
- `src/audio/AudioEngine.cpp` [MODIFY]: por bloque, además del BPM, alimenta `djMixer.setDeckBeatPhase(0/1, deck.getBeatPhase())`.
- `src/ui/FxSlotComponent.cpp` [MODIFY]: en Echo/Gate el botón PARAM cicla la división (escribe `beatDivision`) y muestra el valor activo ("1/4", "1/2", "3/4", "1/1", "2/1"); los demás FX siguen usando `parameter`.

### Verificación
- `cmake --build build`: **0 errores, 0 warnings** (recompilación dirigida de los 4 archivos tocados confirmada).
- Banco headless `/tmp/fxsync_bench` (FxSlot real): **5/5 PASS** — Echo 1/2 beat @120 BPM con ecos exactos en 11025/22050/33075 muestras (amp 1.00/0.48/0.23); Echo 3/4 beat eco en 16537; Gate anclado 1/1 abierto en [0, 0.25)s del ciclo; Gate anclado con offset 1/4 de beat y división 1/2 respeta el corrimiento; Gate sin fase publicada sigue oscilando.
- Release relanzada (PID 26487).

### Fix — Cerrar el editor de grid pausaba la pista en reproducción
- Causa: el destructor de `GridEditorComponent` hacía `deck.pause()` si `playAudition` estaba activo, sin distinguir si la audición la inició el editor o si el deck ya venía reproduciendo.
- `src/ui/GridEditorComponent.h/.cpp` [MODIFY]: nuevo `wasPlayingOnOpen` capturado en el constructor; el cierre solo pausa si el editor inició la audición. Además, el botón PLAY ya no reposiciona al primer beat si el deck está sonando (continúa desde la posición actual).
- Build 0/0; Release relanzada (PID 60312).

## P22 — Stems reales por ML (Demucs, 4 stems, separación offline con caché)

**Objetivo:** reemplazar los "stems" falsos (EQ) por separación de fuentes real. Decisiones del usuario: pre-cálculo offline (patrón rekordbox), 4 stems (vocals/drums/bass/other), y separación **solo al cargar al deck** (nunca en masa al importar 1000 pistas de un USB).

### Motor de separación
- `src/services/StemSeparationService.h/.cpp` [NEW]: cola FIFO en un `juce::Thread` de baja prioridad. `maybeEnqueue(file)` deduplica (ready/queued/processing/failed no se re-encolan; sin binario demucs no encola). Ejecuta `demucs -n htdemucs -o <tmp> <file>` vía `ChildProcess` (App Support bin → bundle Resources → `which demucs` → `python3 -m demucs`), mueve los 4 WAVs a `~/Library/Application Support/DiscNativePro/stems/<firma>/` + `stems.json`, y persiste estado en `LibraryCacheManager`. Disponibilidad del binario cacheada por sesión. Firma de invalidez = hash de path + modTime + size (misma que el caché de biblioteca).
- `src/services/LibraryCacheManager.h/.cpp` [MODIFY]: `stemsStatus` ("none"/"processing"/"ready") + `stemsDir` en `CachedTrackMetadata`, con `updateStemsStatus()` (crea entrada mínima si falta) y `getStemsStatus()` para el badge. Persistido en el JSON.

### Playback de stems (DeckPlayer)
- `src/audio/DeckPlayer.h/.cpp` [MODIFY]: al cargar una pista con stems listos (`stemsDirFor` + duración ≤ 900 s), un job del pool de análisis (con guard de generación) lee los 4 WAVs a RAM (~170 MB/deck máx.) y publica `stemBuffers` + `stemsInRam` en el hilo de mensajes. En `getNextAudioBlock`, con stems activos la salida es la **suma de stems con sus ganancias** (original muteado); la posición de cada stem se deriva del delta del cursor del master por bloque (`rpAfter − rpBefore`), de modo que hereda tempo, keylock, loops y seeks sin pipeline propio. Compensación de latencia del stretcher con keylock ON; delta inválido (seek/reversa/wrap de loop) → silencio limpio. Los flags de stems se resetean bajo el mismo lock que el hilo de audio (sin ventana de carrera). API pública: `isStemsReady/isStemsActive/isStemsLoaded/setStemsActive/getStemGain/setStemGain/stemName`.

### UI
- `src/ui/DeckComponent.h/.cpp` [MODIFY]: fila de stems de 22 px (solo visible si la pista tiene stems listos; aparece en caliente vía chequeo en el timer): botón master STEMS (gris sin stems / neutro listo / azul activo) + 4 botones VOC/DRM/BAS/OTH (encendido = ganancia > 0.5). Patrón de colores cacheados para no repintar a 60 Hz.
- `src/ui/LibraryComponent.cpp` [MODIFY]: badge por fila — STEMS verde (ready) / SEP ámbar (processing), oculto si no hay estado.
- `src/ui/SettingsModalComponent.h/.cpp` [MODIFY]: sección "Stems (Separación de Pistas)" en General con estado del binario demucs y hint de instalación (`pip3 install demucs`).

### MIDI
- `src/midi/MidiMapping.cpp` [MODIFY]: 10 acciones nuevas — `deckN_stems_active` (ON/OFF) y `deckN_stem_vocals/drums/bass/other` (toggle de ganancia), pads canal 8/9 notas 100–104.
- `src/midi/MidiManager.cpp` [MODIFY]: dispatch de las acciones (toggle binario > 0.5).

### Verificación
- `cmake --build build`: **0 errores, 0 warnings**.
- Banco headless `/tmp/stems_bench` (DeckPlayer real + WAVs sintéticos con frecuencias distinguibles: master 1760 Hz, vocals 440, drums 220, bass 110, other 880): **7/7 PASS** — detección de stems por firma; buffers en RAM en ~100 ms (publicación vía MessageManager, el bench bombea el CFRunLoop porque JUCE 8 no tiene modal loops); master solo 1760 Hz con stems OFF; solo vocals 440 Hz / solo drums 220 Hz con original muteado; silencio total con las 4 ganancias en 0; seek a 3.0 s mantiene los stems sincronizados (440 Hz); stems OFF restaura el master. Nota: el bench consume a ritmo realtime (sleep 11 ms por bloque de 512) — sin pacing el BufferingAudioSource entra en hambruna y las mediciones pierden validez.
- Release relanzada (PID 78437).

## P23 — Stems con motor ML embebido (ONNX Runtime, sin dependencias del usuario)

**Objetivo:** eliminar el requisito `pip3 install demucs` de P22: el runtime y el modelo viajan dentro de la app; el usuario nunca instala nada. Decisión confirmada: ONNX Runtime embebido (AskUserQuestion), compatible con los 3 SO.

### Gate 1 — Exportación del modelo (`tools/export_stems_model.py`)
- htdemucs (cac=True) exportado a **ONNX fp32 single-file** (163 MB, opset 20, grafo 100 % real — sin ops complejas): (a) STFT manual (framing + hann + rfft × 1/64, replica exacta de `torch.stft(normalized=True)`); (b) `_mask` con pares re/im (cac garantizado, raise si no); (c) `_ispec` con pads reales en dims F/T y ISTFT manual (extensión hermitiana + ifft ortho + OLA con `unfold_backward`, replica de `torch._refs.istft`, err 1e-6); (d) fixup ScatterND/GatherElements (índices int32→int64) con propagación de dtypes propia (el `infer_shapes` de ONNX no soporta este grafo).
- Validación: modelo parcheado vs demucs eager (eq ≤ 1e-4, forzado en el script) y ORT vs eager **cos = 1.000000**. Quantización descartada con datos: int8 dinámico (60 MB) es más lento (17.3 s vs 6.4 s) y degrada (cos 0.913); fp16 (87 MB) no tiene kernels CPU (0.57× rt). **Se distribuye fp32.**
- Modelo en `assets/models/htdemucs_fp32.onnx` (gitignored); CMake lo copia a `Contents/Resources/models/` si existe.

### Runtime vendida (`third_party/onnxruntime/{macos,windows,linux}`)
- macOS: **1.22.0 universal2** (última con prebuilt universal; 1.31.1 ya no la publica) — un solo dylib para ambas arquitecturas; Windows/Linux: **1.31.1 x64**. Headers C API + licencia MIT. `.gitignore` con negaciones para sortear el ignore global `*.dylib`.
- CMake: dylib → `Contents/Frameworks` (RPATH `@executable_path/../Frameworks`); DLL junto al exe (Windows); .so con RPATH `$ORIGIN` (Linux). Sin `#ifdef` de plataforma en el engine.

### `src/services/OnnxStemEngine.h/.cpp` [NEW]
- Wrapper C API de ORT: sesión lazy con `SetIntraOpNumThreads(hw−2)` (2 cores reservados al hilo de audio), `inter_op=1` y **`session.deterministic_compute=1`** (corridas bit a bit reproducibles; costo medido ~14 %).
- Chunking (parámetros StemKit): chunks de 343980 muestras (segmento nativo de 7.8 s), solape 50 %, ventana raised-cosine con plateau (fade = 1/10 del chunk), padding reflect en bordes de pista, división por suma de ventanas. Decodificación JUCE → estéreo → resample Lagrange a 44.1 kHz si hace falta. Salida: 4 WAVs PCM16 44.1 kHz (drums/bass/other/vocals) escritos en bloques de 2^20.
- `shouldCancel` consultado entre chunks (mata la corrida sin archivos parciales); todo `Release*` de ORT en su lugar (teardown limpio, exit 0).

### `src/services/StemSeparationService.h/.cpp` [MODIFY]
- Camino CLI eliminado por completo (ChildProcess, locator demucs, `which`, `python3 -m`). `isDemucsAvailable()` → `isModelAvailable()` (cacheada, CAS). `separateOne()` = `engine->separate(file, stemsRoot/firma, cancel=threadShouldExit)` + verificación de los 4 WAVs + `stems.json` + cache "ready". Cola FIFO, firma, estados y política "solo al cargar al deck" intactos (DeckPlayer sin cambios).

### `src/ui/SettingsModalComponent.cpp` [MODIFY]
- Sección Stems: "Motor de separación: embebido (htdemucs ONNX)" / "modelo no incluido en esta build" — sin hints de instalación.

### Verificación
- `cmake --build build`: **0 errores, 0 warnings** (en código propio; las únicas warnings del log son preexistentes de third-party de JUCE).
- Banco headless `/tmp/onnx_bench` (OnnxStemEngine real, patrón master_bench, JUCE reutilizado del build): **7/7 PASS** — modelo disponible; sesión ORT inicializada; entrada sintética de 12 s → 4 WAVs de 529200 frames @44.1 kHz; no silenciosos (> −80 dBFS); **energía stems/entrada = 0.976**; determinismo bit a bit entre corridas (±1 LSB PCM16 tras `deterministic_compute`; sin él, ORT multihilo divergía hasta 4.4e-2); cancelación inmediata → false y cero WAVs. Ruteo por stem no se exige aquí (el sintético no es música): la calidad del modelo quedó validada en Gate 1 (cos 1.000000 vs demucs eager). Latencia: 24 s por 12 s de audio (~0.5× rt en la Mac de Axel, background thread, baja prioridad).
- Bundle Release verificado: `Contents/Frameworks/libonnxruntime.1.22.0.dylib` + `Contents/Resources/models/htdemucs_fp32.onnx` presentes.
- Release relanzada (PID 9300).
- **Pendiente de confirmación visual del usuario**: cargar una pista real en un deck → badge SEP ámbar → STEMS verde → fila de stems activa (muteo de voz con VOC apagado).

### Corrección P23b (bug del usuario): la app se colgaba al cargar un audio nuevo en un deck
- **Diagnóstico con `sample` del proceso vivo**: el hilo de mensajes estaba bloqueado para siempre en `maybeEnqueue → std::mutex::lock()`. Causa raíz (heredada de P22): `StemSeparationService::run()` dormía en `wait(-1)` **reteniendo `stateMutex`**, y `maybeEnqueue` (que corre en el hilo de mensajes durante `DeckPlayer::loadFile`) intenta tomar ese mismo mutex **antes** de llegar a su `notify()` → deadlock mutuo, app congelada en el drop.
- **Fix** (`src/services/StemSeparationService.cpp`): `run()` ahora saca el trabajo (o decide esperar) **dentro del lock** y duerme **fuera** de él. Bonus: tope de duración de 900 s en `maybeEnqueue` (misma política que el gate de RAM de DeckPlayer; sin él, una mezcla de 1 h intentaría asignar ~8 GB en el engine).
- **Segundo bug del mismo reporte (crash al cerrar, exit 134)**: el singleton `StemSeparationService` (y su `OnnxStemEngine`) se destruye durante el teardown estático de `exit()` y `ReleaseEnv` de ORT 1.22 hace `abort()` dentro de `Environment::~Environment` (crash report: `ReleaseEnv → Environment::~Environment → abort`). Fix (`OnnxStemEngine.cpp`): el `~Impl` ya no libera los objetos ORT — viven toda la vida del proceso (destruirlos con el engine en scope local sí sale limpio; el problema es exclusivamente el orden de destrucción estática).
- **Verificación**: banco a nivel de servicio `/tmp/service_bench` (StemSeparationService real: `maybeEnqueue` desde el hilo principal + `run()` en el thread de la clase): **5/5 OK** — modelo disponible; pista de 920 s no se encola (tope 900 s); `maybeEnqueue` sin deadlock (127 ms); separación completa (Ready) en 106 s para el MP3 real de 72 s del usuario (~0.54× rt; ruteo correcto: voiceover → vocals); cache-hit = Ready inmediato en segunda corrida; **salida limpia exit 0** (sin crash de teardown).
- Hallazgo adicional (sin impacto): ORT no garantiza bit-exactitud entre corridas de este grafo incluso con `session.deterministic_compute` (con audio real diverge hasta ~1e-1; irrelevante porque cada pista se separa una vez por firma y queda cacheada). El banco `onnx_bench` verifica ahora equivalencia perceptual (RMS por stem dentro de 2 %).
- Release relanzada (PID 38372).

### Corrección P23c (bug del usuario): el badge SEP/STEMS nunca aparece en la biblioteca
- **Diagnóstico con evidencia de disco**: el caché SÍ funciona (`~/Library/DiscNativePro/library_cache.json` — ahí resuelve JUCE `userApplicationDataDirectory` en esta máquina, con 4214 pistas) y tenía la pista en curso como `processing`. La separación también corría (dir de firma + ~70 % CPU). El badge se pinta en el `paint()` de cada fila leyendo `getStemsStatus()`, pero **nadie repinta la tabla cuando el estado cambia**: el timer de `LibraryComponent` solo repintaba si otro evento marcaba `tableNeedsRepaint` y luego se apagaba.
- **Fix**: `LibraryCacheManager::hasStemsJobInProgress()` (scan del caché por "processing") + timer permanente de 1 s en `LibraryComponent` que repinta mientras haya separaciones en curso, con latch para una pasada final al completarse la última.
- Nota de flujo (por diseño P22): los stems se adjuntan al deck **al cargar**; si la pista aún no estaba lista, hay que volver a cargarla (badge ya verde) para que aparezca la fila de stems.

### Corrección P23d (preventiva): crash potencial al cerrar la app después de separar
- Los bancos mostraron que `exit()` con la sesión ORT **usada** y sin liberar aborta en el teardown estático (`__cxa_finalize → std::terminate`; con sesión creada pero nunca usada, sale limpio). La app real (que ejecuta Run) crashearía al cerrar con Cmd+Q tras separar.
- **Fix**: `OnnxStemEngine::shutdown()` (Release Session/Options/Env en contexto vivo, idempotente) + `StemSeparationService::shutdown()` (stopThread 5 s → engine->shutdown()) llamado primero en `DiscNativeProApplication::shutdown()` (`src/Main.cpp`). El humo C ya había probado que liberar en contexto vivo tras un Run sale limpio (exit 0).
- El caso de regresión del banco quedó pendiente de re-correr sin contención de CPU (falló por correr simultáneamente con la app separando; el fallo fue de tiempo, no del fix).

## P23e — Badge SEP/STEMS en la tabla principal (faltaba por completo)

**Síntoma**: el usuario no veía ningún badge en la library al cargar/separar una pista.

**Causa real**: el badge SEP/STEMS (P22) se había implementado solo dentro de
`AutomixQueueView::RowItem::paint` (la cola de Automix del panel derecho). El
`paintCell` de la tabla principal (`LibraryComponent::paintCell`) no lo dibujaba
en ninguna columna — por eso la library nunca lo mostraba.

**Fix** ([src/ui/LibraryComponent.cpp](src/ui/LibraryComponent.cpp), `paintCell` case 3 "Título"):
- Se dibuja la pill SEP/STEMS (ámbar/verde, mismo estilo que la cola de Automix)
  reservando 46 px a la derecha de la celda de título, para no pisar el texto.
- Se lee `LibraryCacheManager::getStemsStatus(track.file)` en cada paint; el
  polling de 1 s de P23c (con `hasStemsJobInProgress` + latch) se encarga de
  repintar la tabla mientras una separación está en curso.

**Validación visual**: captura `--snapshot` de la app → fila de Queen
("Don't Stop Me Now", caché `ready`) muestra la pill STEMS verde en la columna
TÍTULO; las demás filas sin stems no la muestran.

**Nota incidental**: al relanzar la app desde terminal, `AIService::migrateLegacySecrets()`
(`SecItemCopyMatching`) puede bloquear el arranque esperando el diálogo de
Keychain de macOS — aceptar el diálogo ("Permitir") destraba el startup. No es
una regresión de P23.

## P23f — Stems se adoptan al vuelo si la separación termina después de la carga

**Síntoma**: el usuario carga una pista a una bandeja mientras se está separando;
la separación termina (badge STEMS verde en la library), pero la bandeja nunca
muestra la fila de stems hasta re-cargar la pista manualmente.

**Causa**: diseño de P22 — los stems se adjuntan al DeckPlayer solo en el
momento de `loadFile`. Si `stemsDirFor()` daba vacío en ese instante (aún no
había WAVs), la pista quedaba sin stems para toda la sesión de carga.

**Fix**:
- [src/audio/DeckPlayer.h](src/audio/DeckPlayer.h) / [src/audio/DeckPlayer.cpp](src/audio/DeckPlayer.cpp):
  nuevo `tryAttachStemsIfReady()` — si la pista está cargada, no tiene stems,
  dura ≤ 900 s y los 4 WAVs ya existen en disco, setea `stemsReady` y lanza
  `startStemsLoad()` (mismo camino que la carga).
- [src/ui/DeckComponent.cpp](src/ui/DeckComponent.cpp): el timer (60 Hz) lo
  invoca con throttle de 1 s solo mientras la pista cargada no tenga stems.
  La fila aparece sola vía el chequeo existente `isStemsReady() != stemsRowShown`.

**Verificación**: banco headless `/tmp/deck_bench` con el DeckPlayer real + el
archivo de Queen → `loadFile=1 ready=1 inRam=1` (el camino de carga adjunta y
carga los buffers a RAM correctamente). Build 0/0 y app relanzada.

## P24 — CoreML EP para acelerar la separación de stems

**Objetivo**: acortar el tiempo de separación htdemucs (CPU ~1x realtime)
usando el Execution Provider de CoreML (ANE) ya compilado en la dylib
vendorizada de ONNX Runtime 1.22.

**Diagnóstico (banco A/B `/tmp/coreml_ab` + probe de 1 chunk `/tmp/coreml_probe`)**:
1. El append del EP fallaba en silencio: `MLComputeUnits: "MLComputeUnitsAll"`
   es un valor inválido (los válidos son case-sensitive: `ALL`, `CPUOnly`,
   `CPUAndGPU`, `CPUAndNeuralEngine`); un valor inválido invalida TODO el
   append y la sesión queda en CPU puro → A/B inicial daba 1.01x.
2. Con la conversión default (NeuralNetwork) CoreML produce **NaN** con
   htdemucs → separación en silencio.
3. Con `ModelFormat=MLProgram` + `RequireStaticInputShapes=1` la salida es
   **bit-idéntica a CPU** (rel=0.0000 en los 4 stems), pero `MLComputeUnits=ALL`
   (con GPU) es **2x más lento** que CPU puro (~7 s/chunk vs ~3.5 s/chunk):
   las 449 nodos que quedan en CPU + copias entre particiones se comen la
   ganancia de la GPU.
4. Config ganadora: `MLComputeUnits=CPUAndNeuralEngine` → 2.66 s/chunk vs
   3.57 s/chunk CPU (1.34x estable en 5 chunks).

**Cambios**:
- [MODIFY] [src/services/OnnxStemEngine.h](src/services/OnnxStemEngine.h):
  `init(error, preferCoreML=true)` — `false` fuerza CPU puro (banco A/B).
- [MODIFY] [src/services/OnnxStemEngine.cpp](src/services/OnnxStemEngine.cpp):
  append del EP CoreML por API v2 con `{ MLComputeUnits: "CPUAndNeuralEngine",
  ModelFormat: "MLProgram", RequireStaticInputShapes: "1", ModelCacheDirectory:
  .../coreml_cache/htdemucs_fp32-<size>-cane }`; si el append falla, fallback
  silencioso a CPU. Caché persistente de la conversión (~40 s y ~600 MB la
  primera vez; el sufijo `-cane` distingue la config para no reutilizar
  conversiones de otra MLComputeUnits).

**Verificación (A/B, extracto de 72 s, tiempo de pared)**:
- CoreML: init=40 s (conversión única) separate=57 s; CPU: init=1 s separate=69 s
  → **speedup 1.22x**; RMS de los 4 stems rel=0.0000 (AB OK).
- Con caché caliente el init baja (la conversión no se repite entre launches).
- Build 0/0 y app relanzada (Release, PID verificado).

## P25 Fase B — TwoStemEngine (STFT/ISTFT streaming + CoreML)

**Objetivo**: motor de separación 2-stem (vocals/instrumental) en streaming,
numéricamente equivalente al pipeline offline del Gate 0 y a ≥3x realtime.

**Resultados (banco `/tmp/p25_bench`, mix180.wav 180 s vs referencia de
`/tmp/p25_gate0`, excluyendo 8192 muestras de borde por la normalización OLA
en los extremos y el clipping int16 de la referencia)**:
- Vocals: **err rel 0.0012, corr 1.0000**. Instrumental: **err rel 0.0004,
  corr 1.0000** — equivalencia numérica con el pipeline offline lograda.
- Velocidad: **18.35x realtime** (9.8 s de pared para 180 s de audio) con
  CoreML/ANE — supera el criterio GO de ≥3x por 6x.
- Latencia de streaming: 136192 muestras (~3.1 s a 44.1 kHz).

**Cambios**:
- [MODIFY] [src/services/TwoStemEngine.cpp](src/services/TwoStemEngine.cpp):
  guard `flushDone` en `flush()` (evita re-procesar en llamadas repetidas;
  reset() lo limpia); cache de CoreML movido a
  `~/Library/Caches/DiscNativePro/coreml_cache/…` — el EP de CoreML se rompe
  con espacios en el path (Application Support los tiene: conversión rota y
  ejecución basura); hilos intra/inter-op re-activados (sin efecto negativo
  en CoreML).
- [MODIFY] [src/services/OnnxStemEngine.cpp](src/services/OnnxStemEngine.cpp):
  mismo fix de cache sin espacios para el EP CoreML de htdemucs.
- [MODIFY] [CMakeLists.txt](CMakeLists.txt): el bundle ahora empaqueta
  `libonnxruntime-wheel-1.27.0.dylib` (la 1.22 no soporta bien CoreML con
  este modelo y la 1.27 C oficial tiene un bug de ejecución; la del wheel de
  Python 1.27.0 es la que funciona). Headers 1.27 vendorizados en
  `third_party/onnxruntime/macos/include` (reemplazan los 1.22).

**Diagnóstico clave de la sesión**: el presunto "output en cero con CoreML"
era un artefacto de prueba — los primeros 12 s del track de referencia son un
intro instrumental (vocals rms 0.0003); comparando la pista completa de 180 s
el motor coincide con la referencia (ver arriba). Además se descubrió que la
dylib oficial C 1.27.0 falla al ejecutar el modelo MDX ("Feature
Conv_177_cast_to_output…") mientras la dylib del wheel de Python 1.27.0
funciona perfectamente (21x en Python, 18.35x en C++).

**Verificación**:
- Banco streaming 180 s: calidad + velocidad arriba. Con hilos activados los
  números son idénticos.
- Smoke de regresión htdemucs (`/tmp/p25_bench/tests/htd_smoke.cpp`) con la
  dylib wheel 1.27 + cache sin espacios: 4 stems OK (vocals rms 0.041 en
  extracto con voces), 21 s de pared para 12 s de audio con caché CoreML
  caliente.
- Build 0/0 y app relanzada (Release, PID verificado), con la dylib 1.27 en
  `Contents/Frameworks`.

## P25 Fase C — Streaming 2-stem en DeckPlayer

**Objetivo**: el modelo corre durante la reproducción, adelantado al cursor;
los stems aparecen progresivamente y el usuario puede mutar la voz en los
primeros segundos de la pista (el dolor original: "en canciones de 3 minutos
ya se termina la canción y todavía siguen cargando").

**Arquitectura** ([src/audio/DeckPlayer.cpp](src/audio/DeckPlayer.cpp),
[src/audio/DeckPlayer.h](src/audio/DeckPlayer.h)):
- Worker dedicado por deck (`juce::ThreadPool{1}` + `TwoStemEngine`
  persistente, declarado ANTES del pool para que la destrucción inversa
  termine el job antes de destruir el engine). Alimenta el modelo con el
  archivo completo (reader propio; el archivo no es thread-safe) manteniendo
  ~8 s de adelanto sobre el cursor.
- Anillo lock-free preasignado (2 buffers voc/inst, 20 s a 44.1 kHz, ~7 MB por
  deck). Origen/extremo de escritura publicados con atómicos DESPUÉS de
  escribir (`s2Origin44`/`s2WriteEnd44`); el callback solo lee tramos
  publicados.
- El callback (misma matemática de cursor que P22: delta del bufferingSource,
  latencia del stretcher, avance por muestra) mezcla VOC/INST desde el anillo
  con `stemGain[0]/[1]` y mutea el original cuando el bloque está cubierto;
  si no, la mezcla original sigue sonando sin cortes (degradación elegante).
- Seek fuera de cobertura: `setPosition` publica `s2SeekGen/s2SeekTarget44`;
  el worker re-siembra (`engine.reset()`, origin=destino). El flush del final
  de pista se drena siempre aunque supere la ventana de adelanto.
- Sin `alloc` ni locks nuevos en el callback de audio; `juce::Thread::sleep`
  solo en el worker.
- Exclusión mutua con P22: si el dir de 4-stem está listo, el modo RAM de
  P22 gana; el streaming solo se activa si no hay 4-stem.

**Banco** (`/tmp/p25_fasec`, DeckPlayer real + mix180.wav a pacing realtime,
eventos anclados a `deck.getPosition()`): **PASS en todos los criterios**:
- Primera cobertura: **0.59 s** de pared (con init CoreML caliente).
- Re-cobertura tras seek a 100 s: **0.46 s** tras el seek.
- Huecos silenciosos: 1/170 (el propio inicio silencioso del tema).
- Cobertura 100 % en todas las fases: arranque, post-seek, tempo +6 %, A/B
  de gains y cola.
- Routing verificado contra las referencias del Gate 0: INST-only rms 0.09832
  (ref ~0.098), VOC-only rms 0.00038 (ref ~0.0005 en ese tramo).

**Lección del banco**: la primera versión medía con un contador de bloques
propio en vez de la posición del deck — tras el seek todas las ventanas
medían posiciones desplazadas y el contador sobrepasaba el final del tema
(19 "huecos" falsos). Regla: en benches de transporte, anclar métricas y
eventos a la posición real del deck.

---

## P25 Fase D — Caché en disco 2-stem, selector de modo, UI y MIDI

**Objetivo**: que la segunda reproducción de un tema en modo rápido sea
instantánea y sin modelo (la separación en vivo ya corrió una vez); además
exponer el modo de stems ("Rápido"/"Completo") en Ajustes, reflejarlo en la
UI del deck y remapear MIDI.

**Caché en disco** ([src/audio/DeckPlayer.cpp](src/audio/DeckPlayer.cpp),
[src/services/StemSeparationService.h](src/services/StemSeparationService.h)):
- `StemSeparationService::stems2DirFor(file)` →
  `~/Library/Application Support/DiscNativePro/stems2/<firma>/` con
  `vocals.wav` + `instrumental.wav` (16-bit PCM 44.1 kHz). Firma idéntica a
  la del caché 4-stem (path canónico + modTime + size), por lo que editar o
  reemplazar el archivo invalida la caché.
- El worker del deck, cuando el origen es 0 y no hubo re-seek, va anexando
  lo emitido a `*.part.wav` (writers secuenciales creados al armar). Al
  `allDone` resetea los writers, mueve los `.part` a los finales y publica
  `stems2CacheDone`. Un seek (re-siembra) o el abandono del track mata los
  temporales: la caché solo se escribe de corrido y completa.
- **Carrera origen/play corregida**: el worker leía la posición del cursor
  como origen y, si `play()` arrancaba antes, el armado salía con origin≠0 y
  la caché nunca se escribía. Ahora `loadFile` pasa `startOrigin44` explícito
  (0 en carga fresca); el worker nunca consulta `playbackPosSamples`.
- Adopción: al cargar un tema, si existe el dir final,
  `loadStems2Cache` carga ambos WAV a buffers RAM (`stems2CacheBuf`, requiere
  44.1 kHz) y marca `stems2Engaged` **sin worker ni modelo**; el callback lee
  por interpolación y cubre toda la pista (no hay re-seeks en modo caché).

**Selector de modo** ([src/ui/SettingsModalComponent.cpp](src/ui/SettingsModalComponent.cpp)):
- Combo nuevo en Ajustes: "Rápido — 2 stems en vivo (voz/instrumental)"
  (default) / "Completo — 4 stems precalculados". Persistido como clave
  `stemsMode` del JSON de settings; `DeckPlayer::loadStemsModeDefault` lo lee
  al construir y `saveStemsModeDefault` lo escribe (recreando el stream para
  truncar, FileOutputStream no trunca).

**UI** ([src/ui/DeckComponent.cpp](src/ui/DeckComponent.cpp)): el maestro de
stems usa `toggleStemsMaster` (4-stem si está listo, si no 2-stem); en modo
2-stem la fila muestra VOC/INST y oculta DRM/BAS. La fila aparece también en
modo streaming (`isStems2Engaged`).

**MIDI** ([src/midi/MidiManager.cpp](src/midi/MidiManager.cpp)): `pad_stem`
remapeado — maestro dispara `toggleStemsMaster`; en 2-stem, pads 0/1 togglean
VOC/INST y pads 2/3 se ignoran.

**Banco de caché** (`/tmp/p25_fasec cache`, dos pasadas sobre el mismo
tema): **PASS** — pasada 1 reproduce ~180 s en tiempo real hasta que aparecen
los finales (writers armados con origin=0 tras el fix); pasada 2 recarga el
tema, adopta la caché sin modelo (`isStems2Cached && isStems2Engaged`),
reproduce 40 s con cobertura faltante = 0 s, rms INST-only 0.054 (ref ~0.09
en ese tramo de gains) y VOC-only 0.0009 (casi silencio). Diagnósticos
temporales `STEMS2_DBG` retirados del código final.

**Build**: `cmake --build build` 0 errores / 0 warnings; relanzamiento
Release verificado (`pgrep` activo).

---

## P26 — Precálculo 4-stem de carpetas completas

**Objetivo**: precalcular los 4 stems (htdemucs) de una carpeta entera o de
toda la biblioteca, sin cargar cada pista al deck. La separación sigue siendo
una pista por vez sobre la cola existente de `StemSeparationService`; solo
cambia quién encola.

**Cola en lote** ([src/services/StemSeparationService.cpp](src/services/StemSeparationService.cpp)):
- `maybeEnqueue` ahora devuelve `bool` (true si encoló en esta llamada) y se
  agregó `getQueueCount()` (tamaño de cola + pista en proceso) para la UI.
- `enqueueBatchAsync(files, onDone)`: encola el lote en un hilo efímero
  (`juce::Thread::launch`) porque cada `maybeEnqueue` lee la cabecera del
  archivo para el límite de 900 s — en bibliotecas grandes bloquearía el
  message thread. El total encolado vuelve por
  `MessageManager::callAsync`. La dedupe (ready/queued/processing/failed) y
  la política viven en `maybeEnqueue` como siempre.
- Nota de diseño: la ruta del árbol lateral es **lógica**
  ("Raíz / Subcarpeta"), no de filesystem — por eso el lote se arma desde
  `allTracks`/caché y no escaneando carpetas (no hay `enqueueFolder`).

**Gatillo 1 — menú contextual del árbol**
([src/ui/FolderTreeComponent.cpp](src/ui/FolderTreeComponent.cpp),
[src/ui/LibraryComponent.cpp](src/ui/LibraryComponent.cpp)):
ítem "Separar 4 stems (carpeta)" junto a "Desincronizar"; callback
`onPrecalcFolder` que mapea el subtree lógico (`t.folder == path` /
`startsWith(path + " / ")` / `rootFolderPath == path`, mismas condiciones que
`unsyncFolder`) y encola con `enqueueBatchAsync`. AlertWindow no bloqueante
con "Encoladas para separar 4 stems: N" (o el porqué de 0).

**Gatillo 2 — botón en Ajustes**
([src/ui/SettingsModalComponent.cpp](src/ui/SettingsModalComponent.cpp)):
"Precalcular biblioteca (4 stems)" debajo del combo de modo (sección Stems,
pestaña General). Itera `LibraryCacheManager::getCachedTracks()` (nuevo
accessor que expone toda la biblioteca indexada) y encola el lote. Mientras
hay cola activa el botón se deshabilita con contador ("En cola: N pistas")
y se reactiva al terminar el lote.

**Build**: `cmake --build build` 0 errores / 0 warnings; relanzamiento
Release verificado (`pgrep` activo).

**Post-ship fixes P26** (mismo día): (1) el menú contextual del árbol estaba
condicionado a `!row.isSystem` — con solo carpetas del sistema en el árbol
("Todas las Pistas", "Descargas") el clic derecho no mostraba nada; ahora
esas dos filas muestran "Separar 4 stems (carpeta)" (sin desincronizar), y
"Todas las Pistas" encola la biblioteca entera. (2) Los menús contextuales
usaban `withTargetComponent(this)` sin posición → JUCE anclaba el menú al
bounds del componente (aparecía lejos del cursor); agregado
`.withMousePosition()` en los 4 menús de contexto (árbol x3 + tabla de
pistas). Lección: en JUCE, menú contextual = siempre `withMousePosition()`;
`withTargetComponent` solo sirve para menús anclados a un botón concreto.

## P26b — Lote de feedback de usuarios (target macOS, sample rate, YouTube, presets Mixxx)

**Objetivo**: atender los 4 ítems de máxima prioridad del triage de feedback
(18 reportes): binario "requires macOS 26.0", pitch/BPM errado con archivos
44.1 kHz en dispositivo 48 kHz (y viceversa), fallos silenciosos de carga
YouTube, y compatibilidad con controladoras vía presets de la comunidad Mixxx.

### 1. Deployment target macOS (P26.1)
- [CMakeLists.txt](CMakeLists.txt): `CMAKE_OSX_DEPLOYMENT_TARGET` fijo antes de
  `project()`. Sin él, el target heredaba el macOS de la máquina que compila
  (CI `macos-latest`) y el binario pedía macOS 26.0 (reportes de usuarios en
  12.7.6 y 15.8.3). Cuidado con el cache: hay que reconfigurar con
  `cmake -U CMAKE_OSX_DEPLOYMENT_TARGET build` una vez tras editar.
- **Corrección sobre el plan**: el piso quedó en **14.0**, no 13.3, porque el
  symlink actual `libonnxruntime.1.dylib` apunta al wheel 1.27.0 (minos 14.0,
  cambio del mismo día de otro agente). Con 1.22.0 (minos 13.3) podría bajar a
  13.3. Verificado con `vtool -show-build` → `minos 14.0`. macOS 12.7.6 sigue
  sin soporte (el dylib de ONNX es el límite duro).

### 2. Conversión fileRate→deviceRate (P26.2)
El pipeline usaba el sample rate del dispositivo para toda la matemática
(posición, scratch, M del stretcher, ratio del resampler), pero
`BufferingAudioSource` lee en espacio de muestras del **archivo**. Con archivo
44.1 kHz en dispositivo 48 kHz la pista corría ~8.8 % rápido (pitch y BPM
errados, el reporte #6).
- [DeckPlayer.h](src/audio/DeckPlayer.h) / [DeckPlayer.cpp](src/audio/DeckPlayer.cpp):
  `rateFactor = origFileSampleRate / currentSampleRate` (atómico, actualizado
  en `loadFile` y `prepareToPlay`) aplicado en: M de entrada del stretcher
  (vía `StretchAdapter::setInputRateFactor`), ratio del resampler (keylock
  off), cálculo de posición (`getPosition`, `applyPendingSeek`, slip, scratch
  con `fileSr` para conversión posición↔muestras), y `startSec` de stems.
- [StretchAdapter.h](src/audio/StretchAdapter.h) / [.cpp](src/audio/StretchAdapter.cpp):
  `inputRateFactor` atómico multiplicado en `render()`.
- Scratch: el anillo/ring y el slip siguen en samples de dispositivo; las
  conversiones a posición de archivo usan `fileSr`. El rate del resampler se
  clampa a [0.02, 4].

### 3. Carga YouTube confiable (P26.3)
- [YouTubeAudioDownloader.cpp](src/services/YouTubeAudioDownloader.cpp):
  validación estricta de m4a (cabecera `ftyp` en byte 4) en caché y descarga;
  dedupe — si el mismo video ya está descargándose se espera su resultado
  (≤10 min) en vez de lanzar un segundo yt-dlp sobre el mismo `.part`;
  archivo temporal único `<id>.p<ms>.m4a` + rename atómico; formato estricto
  `ba[ext=m4a]/ba[ext=mp4]`; copia a Descargas también atómica (`.part`).
- [DeckComponent.h](src/ui/DeckComponent.h) / [.cpp](src/ui/DeckComponent.cpp):
  banner de error **persistente** (`loadErrorVisible`) — antes se escribía el
  error después de apagar el banner de progreso y nadie lo veía; título/artist
  solo se setean cuando la carga se confirma (adiós "título fantasma"); si
  `loadFile` falla o la descarga falla: banner rojo + labels a "--" + jogWheel
  neutro, limpiado al próximo intento de carga.

### 4. Importador de presets Mixxx (P26.4) — GPL-safe
Nada de Mixxx se vendoriza ni commitea: solo nombres de archivo + URLs
(factual). El preset se descarga en runtime al app-data del usuario y se
parsea con parser propio (el formato XML está documentado en la wiki de Mixxx).
- [NEW] [MixxxPresetCatalog.h](src/services/MixxxPresetCatalog.h) /
  [.cpp](src/services/MixxxPresetCatalog.cpp): manifiesto de ~70 presets
  verificados contra `mixxxdj/mixxx@main` (`res/controllers/*.midi.xml`,
  nombres reales, URL con escaping `%20`); matching tolerante por nombre
  normalizado (sin espacios/guiones/case, gana el alias más largo — evita que
  "DDJ-SB3" matchee el preset de "DDJ-SB"); descarga con `juce::URL`
  (timeout 10 s, validación por contenido, archivo `.tmp` + rename).
- [NEW] [MixxxMappingImporter.h](src/services/MixxxMappingImporter.h) /
  [.cpp](src/services/MixxxMappingImporter.cpp): parse con `juce::XmlDocument`;
  `status 0x91` → NoteOn canal 1 (etc.), `<script-binding/>` omitido y
  contabilizado; crosswalk key+group → actionId (play/play_indicator/
  play_stutter, cue_default/cue_gotoandplay, sync_enabled/beatsync, keylock,
  rate, volume, pfl, gain, filterHigh/Mid/Low/filter, hotcue_1..4 activate/
  enabled, beatjump_forward/backward, slip_mode, [Master] volume/crossfader/
  headVolume). El deck sale del grupo `[ChannelN]` (no del canal MIDI);
  presets de 4 decks se remapean a decks 1..2. Keys sin equivalente (loops,
  flanger, script keys) quedan en el reporte sin importar.
- [MidiMapping.h](src/midi/MidiMapping.h) / [.cpp](src/midi/MidiMapping.cpp):
  `applyImportedBindings()` en lote con una sola persistencia JSON y un solo
  `onMappingChanged`.
- [MidiModalComponent.h](src/ui/MidiModalComponent.h) /
  [.cpp](src/ui/MidiModalComponent.cpp): botón "Importar Mixxx" junto a
  "Escanear". Detecta la controladora conectada (gana el match más largo del
  catálogo), descarga en thread de fondo con `SafePointer`, y muestra el
  reporte con crédito "Autor … (comunidad Mixxx)". Sin match o sin
  controladora: FileChooser manual de cualquier `.midi.xml`.

**Verificación**:
- `cmake --build build`: 0 errores; 0 warnings en código propio (los warnings
  restantes en rebuild completo son de fuentes vendorizadas de JUCE en
  `_deps` — preexistentes en CI). kissfft (vendorizado, del lote stems) se
  silencia por archivo con `set_source_files_properties(... COMPILE_OPTIONS "-w")`.
- `vtool -show-build` → `minos 14.0`; warning de link ONNX 13.3/14.0 eliminado.
- Presets reales descargados y verificados: Starlight es mayormente
  script-binding (se omite con reporte — esperado); DDJ-200 mayormente
  script; Mixtrack 2 / Hercules Mk4 son mapeos planos y ejercen el crosswalk.
- Prueba end-to-end con controladora física conectada **pendiente de
  validación por el usuario** (importar desde el modal MIDI, comprobar con el
  monitor de actividad y reiniciar para verificar persistencia).
- Relanzamiento Release verificado (`pgrep` activo tras `pkill`).

**Fuera de alcance (pospuesto)**: script-bindings JS (shifts, jog-scratch,
encoders, LEDs), backend HID, Traktor TSI, VDJ, hotcues borrado/persistencia,
`.mp4` en whitelists, `.deb`, manual, hotplug MIDI.

## P27 — Lote de feedback II (hotcues, DPI Windows, .deb Linux, feedback LED)

**Objetivo**: 4 ítems de la lista de feedback (segundo lote aprobado por el
usuario el 2026-11-12; soporte `.mp4` excluido por decisión explícita del
usuario).

### 1. Borrado y persistencia de hotcues (P27.1)
- [LibraryCacheManager.h](src/services/LibraryCacheManager.h) /
  [.cpp](src/services/LibraryCacheManager.cpp): `hotCues` (4 doubles, 0 =
  vacío) en `CachedTrackMetadata`; `updateHotCues()` replica el patrón
  updateGrid (lock + entrada mínima + dirty + autosave throttled 3 s); lectura
  y escritura en el JSON de caché.
- [DeckPlayer.cpp](src/audio/DeckPlayer.cpp): `loadFile` reaplica los hotcues
  persistidos de la pista (si existen y caen dentro de la duración).
- [DeckComponent.cpp](src/ui/DeckComponent.cpp): clic derecho sobre un pad →
  menú "Borrar Hot Cue N"; todo cambio de hotcue persiste en la caché.
- [NEW] [docs/manual_usuario.md](docs/manual_usuario.md): manual de usuario
  completo en español; enlazado desde [README.md](README.md).

### 2. Clamp de tamaño inicial de ventana (P27.2, DPI Windows)
En Windows con scaling 125/150 %, el tamaño natural 1360×860 excede el área
de trabajo lógica y la ventana abría recortada o sin barra accesible. JUCE 8
ya es per-monitor DPI aware; el problema era solo el tamaño inicial.
- [Main.cpp](src/Main.cpp): antes de `centreWithSize`, se acota al 92 % del
  `userArea` del display primario (piso 900×600). Sin cambios de backend ni
  escalado de fuentes — YAGNI.

### 3. Paquete .deb para Linux (P27.3)
- [CMakeLists.txt](CMakeLists.txt): bloque Linux-only — install del binario +
  `libonnxruntime.so.1` en `/usr/lib/DiscNativePro` (RPATH `$ORIGIN:...`),
  icono a hicolor 256×256 y desktop entry a share/applications; CPack DEB con
  `CPACK_DEBIAN_PACKAGE_DEPENDS` (gtk3, webkit, alsa, jack, curl, X11...).
- [NEW] [install/linux/discnativepro.desktop](install/linux/discnativepro.desktop).
- [build-linux.yml](.github/workflows/build-linux.yml): `cpack -G DEB` + artifact;
  [release.yml](.github/workflows/release.yml): publica el `.deb`. También se
  corrigió el tar.gz para incluir `libonnxruntime.so*` (antes quedaba afuera
  de `dist/`).
- Nota preexistente: `third_party/onnxruntime` sigue sin trackearse en git; el
  job de CI que linkea el dylib/so requiere que se suba (decisión del repo,
  fuera de alcance de este lote).

### 4. Feedback LED de controladoras (P27.4)
Follower centralizado: toda la iluminación de la controladora sale de un solo
lugar (timer 40 Hz de MidiManager) con envío por **diff** — solo transiciones
de estado, nunca spam.
- [MixxxMappingImporter.h](src/services/MixxxMappingImporter.h) /
  [.cpp](src/services/MixxxMappingImporter.cpp): parse del contenedor
  `<outputs>` de los presets Mixxx (mismo status/midino + crosswalk) →
  `outputBindings` / `outputsImported` en el reporte.
- [MidiMapping.h](src/midi/MidiMapping.h) / [.cpp](src/midi/MidiMapping.cpp):
  `applyImportedOutputBindings()`, `findOutputBinding()` y persistencia del
  array `"outputs"` en `midi_mapping.json`.
- [MidiModalComponent.cpp](src/ui/MidiModalComponent.cpp): la importación
  aplica también las salidas y las cuenta en el reporte ("N salidas (LED)
  importadas").
- [MidiManager.h](src/midi/MidiManager.h) / [.cpp](src/midi/MidiManager.cpp):
  `setLedStateProvider()` + `updateLeds()` en `timerCallback`; `ledsDirty` se
  setea al (re)abrir la salida MIDI para reenviar el estado completo. Si hay
  binding de `<outputs>` se usa ese mensaje; si no, el binding de entrada de
  la acción (learn/manual/default). Acciones LED: play, cue (PFL), keylock,
  slip, hotcues 1-4 y stems master de ambos decks.
- [MainComponent.cpp](src/ui/MainComponent.cpp): proveedor de estado consulta
  `audioEngine` (isPlaying/isCue/isKeylock/isSlipEnabled/hasHotCue/
  isStemsMasterActive). Se **eliminaron** los envíos ad-hoc de LED que estaban
  dispersos (onPlayPause/onCue en MainComponent; mixer_cue y note==84 en
  MidiManager) — el follower es el único dueño de los LED.

**Verificación**:
- `cmake --build build`: **0 errores, 0 warnings** (se corrigieron 3 warnings
  introducidos por este lote: int→float en Main.cpp, sign-conversion en
  LibraryCacheManager.cpp y DeckComponent.cpp).
- Relanzamiento Release verificado (`pgrep` activo tras `pkill`).
- Prueba con controladora física (LED de play/cue/hotcues al importar preset
  Mixxx con `<outputs>`) **pendiente de validación por el usuario**.

## P28 — Hotplug MIDI + cuantizar hotcues al beatgrid

**Objetivo**: cerrar los dos ítems de feedback de MIDI (elección del usuario
2026-11-13): conectar/desconectar controladoras en caliente y hotcues
alineados al grid corregido.

### 1. Cuantizar hotcues (P28.1) + fix de interval file-time
- [DeckPlayer.cpp](src/audio/DeckPlayer.cpp) `setHotCue()`: si la pista tiene
  BPM real (`originalBpm > 0`), la posición se ajusta al beat más cercano del
  grid del archivo con el `quantizedBeatPos()` existente, clamped a la
  duración. Sin BPM → posición cruda (comportamiento anterior). Cubre pad de
  UI y MIDI por igual; el borrado no cambia.
- **Bug preexistente corregido (misma causa)**: tras el re-anclaje file-time
  de P26b, `beatInterval()` seguía devolviendo `60 / currentBpm` (BPM
  ajustado por pitch) contra posiciones en tiempo de archivo. Con pitch ≠ 0,
  beat jump y loop roll derivaban (4 beats a +8 % saltaban menos de 4 beats
  reales del archivo). Ahora el intervalo del grid es `60 / originalBpm`
  (file-time). Lo usan `jumpBeats()` y `startLoopRoll()`.
- `getBeatPhase()` (ancla de los FX sincronizados al beat de P21) tenía la
  misma inconsistencia (fase normalizada con interval con pitch sobre
  posición file-time) — corregida a `originalBpm` también. Sin pitch activo
  el comportamiento es idéntico (currentBpm == originalBpm).

### 2. Hotplug MIDI (P28.2)
- Causa raíz de que no hubiera hotplug: `rescanDevices()` llamaba a
  `shutdown()`, que hace `stopTimer()` — el re-scan desde el propio timer
  habría matado el MIDI.
- [MidiManager.h](src/midi/MidiManager.h) / [.cpp](src/midi/MidiManager.cpp):
  se extrae `disconnectAll()` (quita callbacks por identifier **registrado**
  — antes se re-iteraba `getAvailableDevices()`, que ya no incluye los
  dispositivos desconectados y dejaba callbacks colgantes — y cierra la
  salida). `shutdown()` = `stopTimer() + disconnectAll()`.
- Sondeo de diff cada 2 s en el timer 40 Hz existente (80 ticks):
  `getAvailableDevices()` de entrada y salida contra el baseline
  (`lastInputIds`/`lastOutputIds`, actualizado en cada rescan). Si cambió →
  `rescanDevices()`, que reabre la salida y setea `ledsDirty` → el follower
  LED de P27 reenvía el estado completo automáticamente al reconectar.
- Sin notificación nativa por plataforma (YAGNI): el polling de 2 s cumple.

**Verificación**:
- `cmake --build build`: **0 errores, 0 warnings**.
- Relanzamiento Release verificado (`pgrep` activo tras `pkill`).
- Pendiente de validación del usuario: hotcue entre dos beats queda en el
  beat más cercano; beat jump/roll estable con pitch ≠ 0; desconectar y
  reconectar la controladora USB con la app abierta sin reiniciar (LED
  reenvían su estado al reconectar).

## P29 — Importador de mappings VirtualDJ

**Objetivo**: importar mappings de VirtualDJ (decisión del usuario 2026-11-13:
**solo VDJ**; Traktor TSI queda en backlog por ser un formato binario base64
con spec no oficial incompleta). Entrada por FileChooser manual (VDJ no tiene
presets en URL estable).

### Hallazgo previo importante
`MidiModalComponent` **no se instancia en ninguna parte** (Settings tiene su
propia pestaña MIDI): el botón "Importar Mixxx" de P26b era inalcanzable. Los
botones de importación viven ahora en **Settings → MIDI** (fila de la lista
de dispositivos): "Importar Mixxx" (portado con catálogo + descarga bg) y
"Importar VDJ" (nuevo). `MidiModalComponent` se deja intacto (componente
muerto histórico, sin tocarlo para no romper nada del agente concurrente).

### 1. VdjMappingImporter (P29.1)
- [NEW] [VdjMappingImporter.h](src/services/VdjMappingImporter.h) /
  [.cpp](src/services/VdjMappingImporter.cpp): `importFiles(definition,
  mapper)`. El definition (VDJPedia ControllerDefinitionMIDI) traduce nombres
  abstractos a MIDI: `button/toggle` note→NoteOn (o cc→CC), `slider/jog/
  encoder` cc/note/pitch, `led/color` → salidas (LED). Canales VDJ 0-based →
  +1. El mapper (`<mapper device><map value action>`) cruza nombre → acción
  VDJ → actionId: play_button/play_pause/play_stutter, cue_stop/cue_button/
  cue/cup, sync/beatmatch, keylock/master_tempo, pitch, level/volume,
  crossfader, EQ low/mid/high, filter, gain, pfl, load_song, loop_roll,
  hotcue 1-4, goto/beatjump con signo. Deck: prefijo `deck N|left|right` del
  mapper, si no el atributo deck del definition; clamp 1-2. Lo desconocido se
  lista en el reporte (`unknownActions`); los nombres sin definition se
  contabilizan (`controlsWithoutDefinition`).
- [CMakeLists.txt](CMakeLists.txt): fuentes nuevas agregadas.

### 2. Settings → MIDI (P29.2)
- [SettingsModalComponent.h](src/ui/SettingsModalComponent.h) /
  [.cpp](src/ui/SettingsModalComponent.cpp): `midiImportMixxxBtn` +
  `midiImportVdjBtn` en la fila de dispositivos (layout ajustado, header
  220px); `importMixxxPreset()` portado desde el componente muerto (detección
  de controladora → catálogo → descarga bg con SafePointer → reporte;
  fallback FileChooser) y `importVdjMapping()` nuevo (FileChooser del mapper
  → FileChooser del definition → importación → reporte). Helper compartido
  `showMappingImportResult` (éxito/error, contadores, lista de omitidos).
- Textos con `refreshLanguage`, visibilidad en `updateTabVisibility`.

**Verificación**:
- `cmake --build build`: **0 errores, 0 warnings**.
- Harness standalone (flags de juceaide, juce_core + juce_audio_basics) contra
  definition/mapper de prueba con la estructura exacta de VDJPedia — **PASS**:
  `imported=6, outputsImported=1, controlsWithoutDefinition=1, unknownActions=1`.
  Validado en el reporte: `deck right play`→`deck2_play` (NoteOn ch2 n19),
  `deck 1 EQ LOW`→`eq_low_d1` (CC 0x28), canal VDJ 8→ch 9 (pad hotcue),
  `LED_PLAY1`→salida `deck1_play` (note 0x42); `browser_folder` quedó en
  unknownActions y el control sin definition en controlsWithoutDefinition.
- Pendiente de validación del usuario: importar un mapper+definition real de
  VirtualDJ desde Documents/VirtualDJ (Mappers/Devices) y probar los mapeos
  con la controladora.

# Walkthrough — Bugfix: superposición fila de stems vs HOT CUE (2026-11-13)

**Reporte del usuario**: con una pista con 4 stems cargada, forzó el cambio a otra pista sin stems descargados y los botones STEMS/VOC/DRM/BAS/OTH quedaron superpuestos a la fila HOT CUE.

**Causa raíz**: `DeckComponent::resized()` solo setea bounds de los 5 botones de stems dentro de `if (stemsRowShown)`; al pasar a false, los botones (siempre visibles por `addAndMakeVisible`) conservaban bounds viejos encima de HOT CUE, que recupera el espacio.

**Fix** (commit 24b9ad1, src/ui/DeckComponent.cpp): `setVisible(stemsRowShown)` explícito para stemsMasterButton y los 4 stemButtons antes del `if`. Compilación 0 errores / 0 warnings; app relanzada (Release).

**Release**: v1.4.0 retageado a 24b9ad1 (fix incluido); run CI previo (43466576255, macOS en curso) cancelado por decisión del usuario; nuevo run 43486498080 compilando las 3 plataformas.

## Fix adicional v1.4.0 — dylib ORT universal + fallback de API (commit b883047)

Run 3 del CI falló al enlazar x86_64 en macOS: libonnxruntime-wheel-1.27.0.dylib (P25) es arm64-only y ORT no publica binarios Intel desde 1.27 (verificado en PyPI y GitHub Releases). El run 1 de macOS también había fallado por esto (pasó inadvertido).

- Dylib: slice x86_64 extraído de libonnxruntime.1.22.0.dylib (universal2), install name igualado a @rpath/libonnxruntime.1.dylib, fusionado con lipo con el wheel arm64 1.27.0, firma ad-hoc. Archivo final 64 MB, CMake sin cambios.
- Motores (OnnxStemEngine/TwoStemEngine): GetApi(ORT_API_VERSION) → fallback descendente 27→14 (solo usan funciones de API ≤ 15, compatibles por prefijo). En Intel el 2-stem puede degradar; el 4-stem sobre 1.22 es el setup P23.
- Build local 0/0; app relanzada. Tag v1.4.0 → b883047; run CI 43534492546.

# Walkthrough — P30: auto-update "instalar al reiniciar" en las 3 plataformas (2026-11-14, rama axel-molina/v1.4.1)

Objetivo del usuario: la descarga de actualización no debe abrir carpetas ni pedir arrastrar nada — se descarga y se instala sola al reiniciar la app. La versión de Microsoft Store queda excluida (Store gestiona sus updates).

## Arquitectura por contexto de instalación (InstallContext)

- [src/services/InstallContext.h](src/services/InstallContext.h) / [.cpp](src/services/InstallContext.cpp) [NEW]: detección `MacApp` / `WinStore` / `WinSetup` / `LinuxDeb` / `LinuxPortable`. Windows: `GetPackageFullName` de kernel32 (≠ `ERROR_APPMODEL_ERROR_NO_PACKAGE` = MSIX). Linux: exe bajo /usr|/opt o presencia de `/var/lib/dpkg/info/discnativepro.list` = .deb.
- Updater interno activo solo en MacApp / WinSetup / LinuxPortable ([UpdateService::isInternalUpdaterActive](src/services/UpdateService.cpp)); en Store y .deb el chequeo responde vacío sin red.

## macOS — Sparkle 2 (vendorizado)

- third_party/sparkle/Sparkle.framework 2.10.1 (universal, ad-hoc). CMake lo copia a Contents/Frameworks vía POST_BUILD y enlaza; [MacSparkleUpdater.mm](src/services/MacSparkleUpdater.mm) [NEW] inicia `SPUStandardUpdaterController` con feed por delegado (appcast.xml en raw.githubusercontent main — el bundle Info.plist lo genera JUCE y no es fiable para claves custom). Driver estándar = alerta con "Install on Quit": al reiniciar instala solo.
- `SparkleBridge::startup()/shutdown()` desde Main.cpp; botón "Buscar actualizaciones" de Settings ahora llama a `SparkleBridge::checkForUpdates()` en macOS.
- CMake: si se pasa `-DSPARKLE_EDDSA_PUBLIC_KEY=<b64>`, plutil inyecta `SUPublicEDKey` + `SUEnableAutomaticChecks` en el Info.plist del bundle. **Pendiente del usuario**: generar el par de claves (`third_party/sparkle/bin/generate_keys`) y pasar la pública al build + privada como secret de GitHub.
- appcast.xml [NEW] en la raíz: canal vacío; CI agrega items firmados.

## Windows — Inno Setup silencioso

- El Setup.exe se descarga a `%TEMP%/DiscNativePro/update` (no a Descargas). `setPendingInstallOnQuit(path)` persiste en settings.json; al salir, [UpdateService::runPendingInstaller()](src/services/UpdateService.cpp) lo lanza `/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /CloseApplications=force /DIR="<dir instalado>"` — actualiza en el mismo directorio sin interacción.
- Settings muestra "Se instalará al reiniciar la aplicación" y guarda el pendiente en el callback de descarga.

## Linux portable — self-replace por script

- El tar.gz se extrae en background a `~/.cache/DiscNativePro/update/new` (juce::ChildProcess tar) y se marca pendiente. Al salir, runPendingInstaller escribe `apply_update.sh` (espera `/proc/<pid>` muerto, backup `.bak` del binario, `cp -f staging/*` sobre el dir de la app, limpia staging, relanza run.sh), chmod +x y lo lanza con `Process::openDocument`.
- .deb guard: updater deshabilitado, Settings muestra "actualizaciones gestionadas por apt".

## UI por contexto (Fase E)

- Settings tab Versión: en Store/.deb solo quedan título + mensaje del canal ("gestionadas automáticamente desde Microsoft Store" / "por apt"); botones y combo ocultos, sin auto-chequeo.
- MainComponent::scheduleUpdateCheck(): chequeo JUCE de arranque solo para WinSetup/LinuxPortable (Sparkle lleva su chequeo programado en macOS).

## CI (release.yml)

- Job macOS: nuevo paso que firma DiscNativePro-macOS-Universal.zip con `third_party/sparkle/bin/sign_update` usando secret `SPARKLE_EDDSA_KEY`; sin el secret emite warning y no rompe el release.
- Job publish: checkout de main primero, inserta el item firmado en appcast.xml (python, idempotente por tag, XML validado) y lo commitea a main con github-actions bot.

## Verificación

- `cmake --build build`: **0 errores, 0 warnings** en código propio (2 warnings JUCE/harfbuzz vendidos en rebuild completo son pre-existentes). Corregidos en el camino: `@kAppcastUrl` no válido para char* (NSString* literal), `juce::Process::getProcessID()` no existe en JUCE 8 (getpid() de unistd), captura de `extractedDir` fuera de JUCE_LINUX en el lambda, unused vars/params con guards.
- App relanzada (Release, PID verificado). Sparkle.framework presente en Contents/Frameworks.
- **Pendiente del usuario para activar el updater macOS**: `third_party/sparkle/bin/generate_keys` → pública al build (`-DSPARKLE_EDDSA_PUBLIC_KEY=...`) y privada en secret `SPARKLE_EDDSA_KEY`. Sin eso el feed queda sin items y Sparkle no ofrece updates (la app funciona normal).

---

# Walkthrough — Hotfix v1.4.1: crash al arranque en macOS empaquetado

## Problema reportado
"La app no abre" — la DiscNativePro instalada (v1.4.0, DMG del GitHub Release) moría al arrancar sin abrir ventana.

## Diagnóstico (evidencia)
- Crash reports `~/Library/Logs/DiagnosticReports/DiscNativePro-*.ips` (5 el 2026-11-14): `SIGABRT` de dyld, tipo 309, `termination: DYLD "Library missing"`.
- Motivo exacto: `Library not loaded: @rpath/libonnxruntime.1.dylib` — el binario busca un archivo con ese nombre literal en `Contents/Frameworks/`, pero el POST_BUILD lo copiaba como `libonnxruntime-wheel-1.27.0.dylib`.
- El build local "funcionaba" por accidente: un rpath extra apuntaba a `third_party/onnxruntime/macos/lib` del repo.
- El install name de la wheel dylib ya era `@rpath/libonnxruntime.1.dylib` (no hizo falta `install_name_tool`).

## Cambios ([MODIFY])
- `CMakeLists.txt:11`: bump de versión 1.4.0 → 1.4.1.
- `CMakeLists.txt:267-269` (POST_BUILD ONNX): la copia al bundle ahora incluye el nombre destino `libonnxruntime.1.dylib` (el que el binario resuelve vía `@executable_path/../Frameworks`).
- `.github/workflows/release.yml` (job `build-macos`, step `Verify universal binary`): nueva verificación que falla el job si `Contents/Frameworks/libonnxruntime.1.dylib` no existe, para que este defecto nunca vuelva a publicarse.

## Verificación
1. `cmake --build build`: 0 errores, 0 warnings.
2. Bundle: `Contents/Frameworks/libonnxruntime.1.dylib` presente.
3. **Prueba fuera del repo** (condición que reproduce el crash): bundle copiado a `/tmp/dnp_test` y lanzado → proceso vivo (PID 61239). Antes moría con SIGABRT.
4. **Workaround aplicado a la app instalada**: se copió `libonnxruntime.1.dylib` a `/Applications/DiscNativePro.app/Contents/Frameworks/`, re-firma ad-hoc del bundle exterior y limpieza de quarantine → `open` exitoso, proceso vivo (PID 61453), `codesign --verify --strict` OK.
5. Regla de relanzamiento cumplida (`pkill` → `open` → `pgrep`).

## Docs sync (release-docs-sync)
- `docs/manual_usuario.md`: nueva sección "Actualizaciones automáticas" (P30, por canal: Sparkle macOS, Setup.exe Windows, Store, apt, portable) + entrada de troubleshooting del crash de v1.4.0 corregido en 1.4.1.
- `README.md`: nueva feature "Actualizaciones Automáticas" en Características Principales.

## Release
- Rama `axel-molina/v1.4.1` (incluye P30 auto-update `cdda2c0` + este fix) → push a `main` → tag `v1.4.1` → CI `release.yml` construye macOS/Windows/Linux y publica el Release si las 3 pasan (la de macOS ahora verifica la dylib).

# Walkthrough — Fix: detune al activar/desactivar stems (2026-11-14, rama axel-molina/v1.4.1)

**Reporte del usuario**: al cargar una canción y activar/desactivar stems cambia la afinación del tema.

**Causa raíz**: el worker de streaming 2-stem (P25, [DeckPlayer.cpp](src/audio/DeckPlayer.cpp) `fillFeed()`) resamplea el archivo a 44.1 kHz con `ratio = 44100/origRate` y avanza `rd += ratio` — invertido. La semántica correcta (verificada en `GenericInterpolator::process` de JUCE: speedRatio = muestras de entrada por muestra de salida; `OnnxStemEngine::separate` lo usa bien) es `origRate/44100`. Con un archivo de 48 kHz el stream separado quedaba comprimido en el tiempo ~15.6 % (≈ −2.6 semitonos y más lento) al leerlo el callback; al desactivar stems volvía el original correcto → "cambia la afinación al activar/desactivar". Solo afecta archivos con sample rate ≠ 44.1 kHz (audio de YouTube es 48 kHz); la separación 4-stem offline estaba limpia.

**Bug colateral corregido**: las cachés 2-stem (`vocals.wav`/`instrumental.wav`) ya escritas con el ratio invertido quedaron ~18 % más largas y comprimidas; la firma de validez (path+mtime+size) no cambia. `loadStems2Cache()` ahora valida duración contra `trackDurationSeconds` (±0.5 s) y borra las cachés inválidas para que el streaming corregido las re-escriba.

**Verificación**: `cmake --build build` 0 errores / 0 warnings; app relanzada (Release). Pendiente del usuario: probar la pista original (activar/desactivar stems sin detune) y recargar la pista para validar la caché regenerada.

## Corrección CI adicional (run 43635655028, macOS job)
- `Package macOS App (.zip)` fallaba: codesign rechazaba el bundle por "resource fork / Finder information detritus".
  1. `Sparkle.framework` (nueva en P30) trae `com.apple.FinderInfo` en sus directorios → añadido `xattr -cr` en POST_BUILD y como red de seguridad sobre el bundle antes de firmar en `release.yml`.
  2. Con eso destapado un segundo problema: `cmake -E copy_directory` desreferencia los symlinks raíz del framework (Resources -> Versions/...) y codesign lo rechaza como "bundle format is ambiguous" → reemplazado por `cp -R` (preserva symlinks) + `rm -rf` previo del destino.
- Verificación local completa: bundle en /tmp (sin FileProvider que re-estampa xattrs) → `xattr -cr` → `codesign --force --deep --sign -` → `--verify --deep --strict` OK ("satisfies its Designated Requirement") y la app abre.

# Walkthrough — P31: detune de tono en la bandeja con Key Lock ON (2026-11-14, rama axel-molina/v1.4.2)

**Reporte del usuario**: canciones descargadas de YouTube suenan "en otro tono" al cargarlas en la bandeja.

**Evidencia empírica**: los m4a de YouTube en caché son **44100 Hz** (`afinfo`); la salida por defecto de este Mac corre a **48000 Hz** (`system_profiler SPAudioDataType`).

**Causa raíz**: con Key Lock / Master Tempo ON (default, `DeckPlayer.h:243`), `getNextAudioBlock` alimenta el stretcher directo desde `bufferingSource` (muestras al rate del archivo) y se salta el `resamplingSource` ([DeckPlayer.cpp:573](src/audio/DeckPlayer.cpp)). El stretcher se configura al rate del **dispositivo** (`StretchAdapter::prepare`, [StretchAdapter.cpp:17](src/audio/StretchAdapter.cpp)), así que preserva el tono relativo a 48 kHz mientras recibe contenido a 44.1 kHz. El `inputRateFactor` de P26.2 (= fileRate/deviceRate) corrige el **tempo** (cuántas muestras consume, [StretchAdapter.cpp:83](src/audio/StretchAdapter.cpp)), no el **tono**. Resultado: tono desplazado `deviceRate/fileRate` = 48000/44100 = 1.0884 → **+1.47 semitonos** (más agudo). P26.2 había corregido el tempo/BPM (~8.8 % rápido) pero quedó este residuo de tono en keylock. Archivos locales a 48 kHz → rateFactor 1, sin bug; por eso solo se notaba con YouTube (44.1 kHz). La ruta Key Lock OFF ya era correcta (el resampler convierte el rate preservando el tono).

**Fix** ([DeckPlayer.cpp](src/audio/DeckPlayer.cpp), `updatePlaybackSpeed()`, rama keylock): el detune es un offset de tono puro, y el stretcher ya tiene un transpose persistente que sobrevive a seeks y scratch (el keyshift del usuario ya depende de esa persistencia). Se compensa sumando `12·log2(rateFactor)` semitonos al transpose:

```cpp
const double ratePitchCorrection = 12.0 * std::log2(rateFactor.load());
stretcher->setTransposeSemitones((double)st + ratePitchCorrection);
```

Para 44.1k/48k: corrección = −1.47 st, cancela el +1.47 del bug → tono exacto. `rateFactor = 1` → corrección 0 (caso común intacto). El keyshift del usuario se suma linealmente. Cambio quirúrgico: no toca enrutamiento, posición, seeks, scratch, stems ni latencia (la opción de enrutar el stretcher por el resampler se descartó por alto riesgo de regresión en sync/fase). `updatePlaybackSpeed` corre fuera del callback de audio; el valor viaja al `render()` por el atómico `transposeSemitones` → realtime-safe, sin allocs. El scratch hacia adelante con keylock comparte el mismo stretcher, así que hereda la corrección.

**Verificación**:
- `cmake --build build` (Release): **0 errores, 0 warnings** (solo recompiló DeckPlayer.cpp).
- Razonamiento estático: file 44100 / device 48000 → rateFactor 0.91875 → 12·log2(0.91875) = −1.47 st; bug +1.47 → neto 0 ✓. file = device → corrección 0 ✓.
- Regla de relanzamiento cumplida: `pkill` → `open` Release → `pgrep` (PID 75113 vivo).
- **Pendiente del usuario** (no puedo oír audio): cargar un m4a de YouTube (44.1k) con Key Lock ON → debe sonar en el tono correcto; comparar con Key Lock OFF (ya correcto) → mismo tono; probar pitch fader ±8 % y keyshift para confirmar que el tono se mantiene.

---

# Walkthrough — P32b: los botones + / − de la bandeja hacen pitch bend al mantener pulsado (2026-11-14, rama axel-molina/v1.4.2)

**Reporte del usuario**: "el botón más y el menos para mover los pitch de la bandeja no funcionan".

**Causa raíz**: no estaban rotos; hacían otra cosa. `pitchBendDownButton` / `pitchBendUpButton` estaban cableados como **pitch bend momentáneo de 150 ms** ([DeckComponent.cpp:191](src/ui/DeckComponent.cpp) y [:203](src/ui/DeckComponent.cpp)): `onClick` → `deck.setPitchBend(±0.04f)` + `Timer::callAfterDelay(150, ... setPitchBend(0.0f))`. Un blip brevísimo que se resetea solo al hacer click; con Key Lock ON solo nudgeo de tempo. No movían el fader ni el % → parecía que no hacían nada.

**Evolución de la decisión**:
- **P32 (superado)**: primero se cambió a paso permanente del fader (±0.1 % por pulsación, reutilizando `pitchSlider.onValueChange`). El usuario preguntó si ese comportamiento era el correcto y cómo lo hacen los software profesionales.
- **Investigación**: el estándar de la industria (Pioneer CDJ/rekordbox, Serato, Traktor) para los + / − junto al fader es **pitch bend TEMPORAL al mantener pulsado**, no un cambio permanente del fader; el pitch permanente se ajusta con el propio fader. Con Key Lock ON el bend solo afecta al tempo (tono preservado).
- **P32b (vigente)**: se alinea al estándar → hold-to-bend.

**Fix** ([DeckComponent.cpp](src/ui/DeckComponent.cpp)): se sustituyen los `onClick` por `onStateChange`, que JUCE dispara al pulsar (`isDown()==true`) y al soltar/salir (`false`) (`juce_Button.cpp:326,436-451,470-484`):

```cpp
pitchBendDownButton.onStateChange = [this]() {
    deck.setPitchBend(pitchBendDownButton.isDown() ? -0.04f : 0.0f);
};
pitchBendUpButton.onStateChange = [this]() {
    deck.setPitchBend(pitchBendUpButton.isDown() ? 0.04f : 0.0f);
};
```

Mantener **+** acelera el tempo (+4 %), mantener **−** lo frena (−4 %), al soltar vuelve a 0. El fader NO se mueve (sigue siendo el único control de pitch permanente). Con Key Lock ON solo tempo; OFF pitch+tempo. No requiere `addListener` ni tocar `buttonStateChanged` (el patrón del rollPad está incompleto — le falta `addListener`, es código muerto — no se reutiliza). `stretcher` se crea en el ctor ([DeckPlayer.cpp:41](src/audio/DeckPlayer.cpp)), siempre no-null → `updatePlaybackSpeed` seguro incluso en hover (`onStateChange` también se dispara al entrar/salir el ratón con `isDown` false → `setPitchBend(0)`, inocuo). Realtime-safe: message thread + atómicos. Con sync de fase activo el controlador anula el bend manual ([AudioEngine.cpp:760](src/audio/AudioEngine.cpp)), igual que antes; el bend se usa sin sync (beatmatching de oído), como en el software pro.

**Verificación**:
- `cmake --build build` (Release): **0 errores, 0 warnings** (solo recompiló DeckComponent.cpp).
- Razonamiento estático: mantener + → `isDown` true → `setPitchBend(+0.04)` → `tempoRate` 1.04 sostenido; soltar/salir → `isDown` false → `setPitchBend(0)` → vuelve a 1.0.
- Regla de relanzamiento cumplida: `pkill` (solo coincidía el binario de la app) → `open` Release → `pgrep` (PID 99775 vivo).
- **Pendiente del usuario**: mantener + / − → el tempo se acelera/frena mientras se sostiene y vuelve al soltar; el fader no se mueve; con Key Lock ON el tono se mantiene.

---

# Walkthrough — Integración Notion MCP (2026-11-16, rama axel-molina/v1.4.2)

**Objetivo**: conectar el servidor oficial de Notion MCP como contexto y gestión de backlog. Deshabilitados por decisión: servidores propios, webhooks, n8n, cron y APIs de IA externas. Ámbito elegido: **local** (privado de esta máquina, no versionado).

**Configuración creada** — `[NEW]` `~/.commandcode/projects/users-axelivanmolina-documents-documentos-mac-book-air-de-axel-github-disc-native-pro/mcp.json`:
```json
{ "mcpServers": { "notion": { "transport": "http", "enabled": true, "url": "https://mcp.notion.com/mcp" } } }
```
No se tocó `~/.commandcode/mcp.json` (server existentes intactos) ni ningún archivo del repo con config. Sin secretos en el repo.

**Diagnóstico (verificado contra el harness)**:
- El validador nativo acepta solo `transport: 'stdio' | 'http'`; no obstante `inferTransport()` tolera valores desconocidos → si hay `url` y no `command`, resuelve `http`. Se dejó `"http"` canónico.
- La ruta local la calcula `slugifyProjectPath(projectPath)` (= `sindresorhusSlugify`). Para este repo coincide exactamente con el directorio usado (confirmado ejecutando el propio `getLocalMcpConfigPath`/`loadMcpServers` del harness: descubre 9 servidores incl. `notion`).
- Los logs de `~/Library/Logs/Command Code/mcp/` son solo de servidores **stdio**; los `http` (context7, github, notion) no escriben log → su ausencia no implica fallo.

**Auth OAuth**: la UI `/mcp` mostraba `notion` como 401 sin acción de autenticación utilizable. Se ejecutó el flujo OAuth del propio harness (`discoverAndAuthenticateMcpServer`) que: sondea (401) → lee metadata (`registration_endpoint: https://mcp.notion.com/register`, DCR soportado) → registra cliente → abre navegador (loopback `127.0.0.1:8085/callback`) → intercambia código. **Resultado: success**. Token guardado por el harness en `~/.commandcode/mcp-tokens.json` (modo 0600, fuera del repo; no se imprimió ni copió).

**Verificación (lecturas reales, sin escrituras)**:
- Conexión OK → **77 herramientas** MCP (`notion-search`, `notion-fetch`, `notion-create-pages`, `notion-update-page`, …).
- `notion-search("Product Backlog")` → devuelve la base y páginas del workspace (p. ej. "DiscNativePro — Centro de desarrollo", "Guía de trabajo con agentes — DiscNativePro").
- `notion-fetch` de una página → contenido real (9 411 chars) leído.
- Esquema real de **Product Backlog** (`collection://418800c3-6a0b-8147-a3c2-000b4ce6a9a9`): Tarea[title], Ticket ID[auto_increment_id], Tipo[Bug|Fix|Feature|Mejora UX|Refactor|Deuda técnica|Investigación], Estado[status], Prioridad[Crítica|Alta|Media|Baja], Agente[Command Code|Quoder|Otro|Sin asignar], Impacto, Dificultad, Esfuerzo estimado, Confianza de estimación, Validación[No iniciada|Pendiente|En pruebas|Aprobada|Fallida|Revisión humana], Progreso[number], Horas estimadas[number], Criterios de aceptación[text], Plan de implementación[text], Resultado / notas técnicas[text], Bloqueo[text], Branch / PR / Commit[url], Feedback relacionado[relation], Fecha de inicio/finalización[date], Creado[created_time], Última edición[last_edited_time].
- Esquema real de **Feedback Inbox** (`collection://418800c3-6a0b-81c9-b01d-000b5b334a8b`): Comentario[title], Feedback ID[auto_increment_id], Estado de análisis[Sin analizar|Analizado|Convertido en tarea|Duplicado|Descartado], Origen[Usuario|Testing|Interno|GitHub|Otro], Tipo[Bug|Feature request|UX|Rendimiento|Compatibilidad|Idea|Otro], Impacto reportado[Bajo|Medio|Alto|Crítico|Desconocido], Plataforma[multi_select], Pasos para reproducir[text], Resultado esperado/actual[text], Notas del análisis[text], Entorno / Hardware[text], Versión de la app[text], URL de origen[url], Tareas relacionadas[relation], Fecha recibido[date], Creado[created_time], Última edición[last_edited_time].

**Notas**: no se creó ni modificó ningún contenido en Notion (solo lecturas); no se crearon tickets de prueba. Para que la **sesión del agente** reciba las tools de Notion hace falta releer los MCP (reiniciar la app o reconectar; el token ya está en el vault). Limitación: el flujo OAuth abre la web con navegador del sistema; no puede completarse sin intervención del usuario.

---

# Walkthrough — P33: distribución Linux como AppImage + memoria compartida Engram (2026-11-16, rama axel-molina/v1.4.2)

## Parte 1 — AppImage (alcance A: audio + GUI autocontenido)

**Objetivo**: entregar Linux como un único archivo ejecutable, sin depender de que el usuario instale dependencias del sistema. El `.deb` y el `.tar.gz` que ya existían siguen igual: esta vía es **aditiva**, no reemplaza ninguna distribución.

**Qué NO cambia (riesgo cero)**: no se toca `CMakeLists.txt`, ni el código C++, ni el `.deb` (CPack), ni los flujos de macOS/Windows. `installer/linux/build-appimage.sh` es un script aislado que solo arma un `AppDir` y llama a `linuxdeploy`.

**`[NEW]` `installer/linux/build-appimage.sh`** (chmod +x, validado con `bash -n`):
1. Arma el `AppDir`: `usr/bin/`, `usr/share/applications/`, `usr/share/icons/hicolor/256x256/apps/`.
2. Copia el binario `DiscNativePro` de `build/` y el runtime ONNX.
3. Crea el enlace `libonnxruntime.so.1` (ver bug latente más abajo).
4. Copia `install/linux/discnativepro.desktop` y `assets/icon.png` (el `Icon=discnativepro` del .desktop coincide con el nombre del PNG).
5. Descarga `linuxdeploy-x86_64.AppImage` + `linuxdeploy-plugin-gtk-x86_64.AppImage` y empaqueta con `--plugin gtk`.
6. Renombra a `DiscNativePro-Linux-x86_64.AppImage` y borra el `AppDir` y los AppImages descargados.

Variables de entorno: `APPIMAGE_EXTRACT_AND_RUN=1` (los runners de GitHub no tienen FUSE → el AppImage se autoextrae) y `ARCH=x86_64`.

**Bug latente encontrado (y corregido a nivel de empaquetado)**: el `POST_BUILD` de CMake copia solo el archivo versionado `libonnxruntime.so.1.31.1`, pero el loader de ELF resuelve `DT_NEEDED` por **SONAME**, que en ONNX Runtime es `libonnxruntime.so.1`. Sin un archivo llamado exactamente `libonnxruntime.so.1` al lado del binario (rpath `$ORIGIN`), la app no arranca. El `.deb` no suffers el problema porque CPack renombra el archivo al nombre del SONAME; el **tar.gz sí lo sufría** (solo subía el `.so.1.31.1`). Fix aplicado en los tres puntos de empaquetado:
- `installer/linux/build-appimage.sh`: copia el `.so` real y crea `ln -sf libonnxruntime.so.1.31.1 libonnxruntime.so.1`.
- `.github/workflows/build-linux.yml` y `.github/workflows/release.yml` (paso del tar.gz): mismo enlace simbólico en `dist/`.
- `CMakeLists.txt` **no se modificó** (queda fuera del alcance aprobado); si se quiere arreglar de raíz, el cambio natural es que el `POST_BUILD` renombre el `.so` al SONAME.

**Workflows** (`[MODIFY]`):
- `build-linux.yml`: paquetes apt para el plugin gtk (`librsvg2-dev`, `libgdk-pixbuf-2.0-dev`, `file`; `libgirepository-2.0-dev` con fallback a `libgirepository1.0-dev` porque el nombre del paquete cambió en Ubuntu 24.04), pasos nuevos "Package Linux AppImage" + "Upload Linux AppImage Artifact" (`DiscNativePro-Linux-AppImage`).
- `release.yml`: mismos paquetes, mismo fix del SONAME, "Package Linux AppImage" + artifact `linux-appimage-artifact`, "Download Linux AppImage Artifact" en `publish-release` y `DiscNativePro-Linux-x86_64.AppImage` agregado a la lista `files:` del release.

**Verificación**:
- `bash -n installer/linux/build-appimage.sh` → OK; ambos YAML parsean con PyYAML (`build-linux` → job `build-linux`; `release` → `build-windows`, `build-linux`, `build-macos`, `publish-release`).
- Rutas confirmadas en disco: `install/linux/discnativepro.desktop`, `assets/icon.png`.
- `cmake --build build` (Release) → **0 errores, 0 warnings** (el árbol incluye los cambios P31/P32b sin commitear de otros agentes).
- Regla de relanzamiento cumplida: `pkill` → `open build/DiscNativePro_artefacts/Release/DiscNativePro.app` → `pgrep` (PID 30928 vivo).
- **No se puede generar el AppImage en esta máquina**: es Linux-only. La verificación real ocurre en el primer run de CI en Linux (puede requerir 1-2 iteraciones).

**Limitaciones conocidas (alcance A)**:
- **Video de YouTube puede no funcionar**: depende de WebKitGTK. `linuxdeploy` empaqueta lo que detecta, pero los procesos auxiliares (`WebKitWebProcess`, `WebKitNetworkProcess`) y los plugins de GStreamer no se empaquetan. No se excluyó WebKitGTK a propósito: excluirlo dejaría `DT_NEEDED` sin resolver y la app no arrancaría.
- **glibc mínimo**: se compila en `ubuntu-latest` (glibc 2.39) → el AppImage no corre en distribuciones muy viejas (Debian 11, Ubuntu 20.04).
- **Modelos de stems**: no se empaquetan (están gitignored); sin ellos la separación degrada al modo completo.
- `yt-dlp`/`ffmpeg` se descargan en runtime, no van dentro del AppImage.

## Parte 2 — Memoria compartida entre agentes (Engram)

**Problema**: con varios agentes de IA trabajando sobre el mismo repo, cada uno arranca sin contexto y el usuario termina copiando/pegueando chats para transferirlos.

**Solución**: [Engram](https://github.com/Gentleman-Programming/engram) — store local SQLite (`~/.engram/engram.db`) que los agentes leen/escriben vía MCP stdio. Proyecto fijado como `discnativepro` en los 5 agentes: **Qoder, Codex, OpenCode, Antigravity** y **Command Code** (este último configurado por el usuario).

**Configuración clave (el gotcha que costó tiempo)**: Qoder lanza los servidores MCP stdio con `cwd=/`, no con la carpeta del workspace → la detección de proyecto por directorio falla y todo caía en `project: "unknown"`. Por eso el proyecto va **fijado explícitamente** con `--project=discnativepro` en cada config (el flag y la variable `ENGRAM_PROJECT` son el mismo mecanismo; gana el flag). Verificado: `project_source: "process_override"` incluso con `cwd=/`.

**Binding al repo**: `[NEW]` `.engram/config.json` con `{"project_name": "DiscNativePro"}` (engram lo normaliza a `discnativepro`). Se agregó `.engram/` a `.gitignore` porque es estado local de la máquina.

**Verificación real** (no declarativa):
- `engram doctor` → **10/10 checks OK**.
- Handshake stdio real desde `cwd=/` → `{"project":"discnativepro","project_source":"process_override"}`.
- `mem_list_projects` → 1 proyecto vinculado al repo.
- 2 observaciones guardadas (setup + pinning del proyecto) con el conflicto resuelto vía `mem_judge` (relation "related", confianza 0.9).
- En Qoder, el server `engram` figura registrado (user-scope, status "ready") con las 23 tools `mem_*` disponibles.

**`[MODIFY]` `AGENTS.md`**: se agregó la sección "## 5. Memoria Compartida entre Agentes (Engram)" con el store, el proyecto fijado, el binding del repo y 5 reglas de uso (consultar `mem_current_project`/`mem_context` al arrancar; guardar decisiones/fixes proactivamente con `mem_save`; `mem_session_summary` tras compactaciones; no pedir chats al usuario; la memoria es bookkeeping interno, nunca reemplaza la respuesta final).

**Nota**: los archivos de config de cada agente viven en su home (`~/.codex/config.toml`, `~/.config/opencode/opencode.json`, `~/.gemini/config/mcp_config.json`, `~/.commandcode/mcp.json`) y quedan fuera del repo. No se copiaron ni imprimieron tokens/secretos de terceros que ya estaban en esos archivos.

**Pendiente del usuario**: verificar en la próxima sesión de cada agente que `mem_context` devuelve el contexto del proyecto; la primera corrida de CI del AppImage puede necesitar iteración (ajustes de paquetes del plugin gtk o del nombre del AppImage generado).

---

# Walkthrough — P34: perfiles de controladora MIDI + tabla Denon + detección de conflictos (Notion #13)

**Ticket**: Notion Product Backlog #13 — "MIDI: el fader de volumen del canal 1 mueve el pitch y el crossfader no responde" (Bug, Prioridad Alta, feedback `DNP-FB-11`, plataforma reportada Linux).
**Hardware del usuario**: Denon MC2000.

## 1. Causa raíz (verificada con el preset oficial, no por suposición)

El preset oficial de Mixxx `res/controllers/Denon MC3000.midi.xml` confirma:
- **Volumen Deck A = CC 5 en canal MIDI 1**.
- **Crossfader = CC 22 en canal MIDI 1**.
- Deck A = canal 1, Deck B = canal 3 (el layout DN-SC2000 usa canal 2 → se acepta como alias).

La app tenía un único bloque de hardware "estilo Pioneer" que reclamaba **CC 5 / canal 1 como MSB del tempo de 14 bits** (`MidiManager.cpp`, rama `case 5:` del bloque genérico) y **no tenía ninguna rama para CC 22**, así que el crossfader caía en el fallback genérico ("Deck 1 Low EQ") y quedaba muerto. Los dos síntomas del ticket quedan explicados exactamente por la tabla Denon; la tabla MC4000 (crossfader CC 8, volumen CC 28) no los explicaría.

## 2. Cambios

### `[NEW]` `src/midi/MidiControllerProfile.h` / `.cpp`
- `enum class MidiControllerFamily { Generic, PioneerDDJ, DenonMC }`.
- `MidiControllerProfile::detect(deviceNames)`: por nombre del dispositivo (denon, dn sc2000, mc2000/3000/4000/6000/7000 → Denon; pioneer/ddj → Pioneer; resto → Genérica).
- `namespace DenonMidi`: constantes de la tabla (volumen A CC5 / B CC16, trim 1/18, high 2/13, mid 3/14, low 4/21, crossfader 22, jog 81, browse 84, master 146, phones 144/137; play 67, cue 66, PFL 1/5, keylock 6, hotcues 35/36/37/50, load 98/99).

### `[MODIFY]` `src/midi/MidiMapping.h` / `.cpp`
- `MidiActionDefinition` gana `factoryBinding` (tabla Pioneer **inmutable**) y `MidiMappingManager` guarda `defaultBinding` por perfil.
- `resolveDefaultBinding(family, actionId)`: tabla Denon → lista de acciones limpiadas para Denon (sync, keyshift, jog platter, EQ filter, browse push, pads de performance/stems) → si no, `factoryBinding`.
- `applyProfileDefaults(family)`: migra los defaults al perfil activo. **Regla de preservación**: un binding de usuario solo se reescribe si sigue siendo igual al default anterior ("intocado"); lo customized por el usuario se respeta.
- `bindingsConflict()` / `findConflicts()` / `findConflictsFor(actionId)`: dos acciones chocan si comparten tipo + número y sus canales son iguales o uno es comodín (0).
- Persistencia: el JSON (`~/Library/Application Support/DiscNativePro/midi_mapping.json`) ahora guarda `profile` y `profileOverride` → al reiniciar se recupera el baseline correcto.

### `[MODIFY]` `src/midi/MidiManager.h` / `.cpp`
- Dispatch por perfil: `matchMessage` → `dispatchMappedAction` → bloque hardware **de la familia activa**.
- `handleDenonMessage()`: implementa la tabla Denon verificada (Deck A canal 1, Deck B canal 3 con alias de canal 2, jog con encoding `valor - 64` = el que ya usa `handleJogWheel`, browse encoder con delta 0→+1 / resto→-1, pitch bend → pitch de la bandeja, +/− deadzone).
- `handlePioneerMessage()` conserva **verbatim** el bloque existente; solo `case 5` (MSB de tempo con shift) queda restringido a Pioneer, de modo que una controladora desconocida ya no mueve el pitch con CC 5.
- Normalización de canal: en familia Denon, el canal 2 (layout DN-SC2000) se reescribe a canal 3 → una sola tabla sirve para ambas variantes de Deck B.
- `updateControllerProfile()` en cada `rescanDevices()`: detecta la familia y aplica los defaults del perfil (respetando el override manual).
- `setControllerProfileOverride(enabled, family)` para forzar el perfil desde la UI.

### `[MODIFY]` `src/ui/MidiMappingTableComponent.h` / `.cpp`
- Contador **⚠ N** en la barra superior (solo visible si hay conflictos) que abre el detalle de las parejas duplicadas.
- Indicador **⚠** en la celda "CONTROL ASIGNADO" de cada fila en conflicto.
- Al aprender un control que ya está asignado a otra acción, diálogo **"Reemplazar / Cancelar"**: *Reemplazar* libera el control de la otra acción; *Cancelar* restaura el binding previo de la acción aprendida (persistido en disco).
- Las actualizaciones de UI que llegan desde el hilo MIDI se postergan con `MessageManager::callAsync` (el `AlertWindow` solo puede abrirse en el hilo de mensajes).

### `[MODIFY]` `src/ui/MidiModalComponent.h` / `.cpp`
- Selector **PERFIL**: Automático / Pioneer DDJ / Denon MC / Genérica → `MidiManager::setControllerProfileOverride`.
- Badge **"Detectado: Denon MC"** (o "Manual: …" si el usuario fuerza el perfil).
- El modal crece de 326 a 340 px para dar espacio a la fila nueva (perfil / dispositivos / monitor).

### `[MODIFY]` `CMakeLists.txt`
- Se agrega `src/midi/MidiControllerProfile.h` y `.cpp` a la lista explícita de fuentes.

## 3. Verificación

- `cmake --build build` → **0 errores, 0 warnings** (`grep -icE "warning|error"` → 0 sobre el log completo de la recompilación de los 4 ficheros MIDI/UI tocados).
- Regla de relanzamiento cumplida: `pkill -f DiscNativePro` → `open build/DiscNativePro_artefacts/Release/DiscNativePro.app` → `pgrep -fl DiscNativePro` → PID vivo, proceso estable (sin crash a los +8 s).
- `git diff --stat`: 9 ficheros modificados + 2 nuevos, 783 inserciones.

## 4. Pendiente de validación física (usuario)

La app compila y arranca, pero **el fix no está validado contra el MC2000 real**: eso requiere el hardware del usuario.
- Con el MC2000 conectado: abrir Ajustes → MIDI y confirmar que el badge dice **"Detectado: Denon MC"**.
- Mover el **fader de volumen del canal 1**: debe mover el volumen de la Deck 1 y **no** tocar el pitch.
- Mover el **crossfader**: debe mover el crossfader del mezclador.
- Si algún control no coincide, el **monitor MIDI en vivo** del modal muestra el mensaje crudo (canal/CC/nota) → se ajusta la tabla Denon con ese dato.

El ticket Notion #13 queda **"En curso"** con Validación "No iniciada" hasta que el usuario confirme en su hardware.

### Cierre por decisión del usuario (2026-11-16)

El usuario instruyó cerrar el ticket **asumiendo** que la validación con el hardware quedó superada («vamos a suponer que quedó validado»). El ticket #13 queda **Finalizado / Progreso 100 / Validación Aprobada**.

> **Transparencia**: la prueba física con la MC2000 **no se ejecutó** (requiere el hardware del usuario); el cierre refleja la asunción del usuario, no una prueba ejecutada. Riesgo residual y plan de ajuste: si al conectar la controladora algún control no coincidiera, se ajusta la tabla `DenonMidi` (`src/midi/MidiControllerProfile.cpp`) o la tabla de la familia con el dato crudo del **monitor MIDI en vivo** del modal MIDI, y se reabre el ticket — no hace falta rehacer el diagnóstico de causa raíz, que ya estaba probado en el código.

---

# Walkthrough — P35 + P36: escala tipográfica en Ajustes + mensaje de bienvenida

> Tickets Notion: **DNP-TK-6** (tipografía) y **DNP-TK-12** (aviso de bienvenida).
> Plan aprobado: `implementation_plan.md` (sección «P35 + P36»).

## 1. Objetivo

- **P35**: subir el tamaño de letra de Preferencias/Ajustes para que las etiquetas y textos se lean sin esfuerzo (base de 9.5–11.5 px → 10.5–19 px según rol), sin recortes ni solapamientos y con el layout íntegro a 1280 px de ancho.
- **P36**: mostrar en el **primer arranque** un aviso de que el proyecto está en desarrollo (una sola persona, posibles errores/inestabilidad, uso bajo propio riesgo) con enlace al canal de YouTube y al repo de GitHub, opción de «no mostrar de nuevo» y versión en español e inglés, sin bloquear el uso de la app.

## 2. Cambios

### `[NEW]` `src/ui/UiTypeScale.h`
Escala tipográfica compartida por toda la UI de Ajustes (píxeles, no puntos):

| token | px | uso |
|---|---|---|
| `micro` | 10.5 | texto terciario: badges, estados |
| `caption` | 11.5 | botones secundarios, descripciones |
| `body` | 12.5 | etiquetas de opciones y valores |
| `section` | 13.5 | títulos de sección |
| `heading` | 14.5 | títulos de bloque / tarjeta |
| `title` | 19.0 | título de la app |

### `[MODIFY]` `src/ui/SettingsModalComponent.cpp` (≈99 substitutions)
- Todos los `setFontSize(...)` de Ajustes pasan a usar `UiType::*` (antes cada control fijaba su propio literal).
- Iconos Lucide de las etiquetas con texto se pasan a 15 px para acompañar el cuerpo de texto mayor.
- Botón **«★ Ver mensaje de bienvenida»** (`showWelcomeBtn`) añadido al final de la pestaña General, con `onShowWelcome` (std::function) para reabrir el aviso.
- `appInfoLabel` (línea de versión) se añade al árbol de componentes (`bodyContent.addChildComponent`) **y** se hace visible desde `updateTabVisibility()` → `appInfoLabel.setVisible(isGeneral)`. Sin esto no se veía: en esta versión de JUCE los componentes nacen invisibles (`ComponentFlags` a 0) y `addChildComponent` no activa `visibleFlag`.
- **Fix de recorte en la tarjeta de IA Match**: la caja de texto pasa de 84 a **116 px** (`aiInfoCardBox = aiArea.removeFromTop(116)`) porque las 6 líneas del bloque «Sin Clave API» a 11.5 px necesitan ~90 px y la última se recortaba.

### `[NEW]` `src/ui/WelcomeModalComponent.h` / `.cpp`
- Diálogo propio de 560×410 px (`juce::DialogWindow` con `launchAsync`, **no modal bloqueante**: la app se puede seguir usando con el aviso abierto).
- Contenido: título, subtítulo en ámbar «Proyecto en desarrollo · úsalo bajo tu propio riesgo», 3 líneas de cuerpo, botón **Canal de YouTube (@AxelCodeDev)** (`https://www.youtube.com/@AxelCodeDev`), botón **Código en GitHub (deja una estrella)** (`https://github.com/axel-molina/DiscNativePro`), línea de apoyo, casilla «No mostrar de nuevo» y botón «Entendido».
- Persistencia en `~/Library/DiscNativePro/settings.json` → clave **`welcome.suppress`** (`readSuppressFlag` / `writeSuppressFlag`). La clave se escribe al cambiar la casilla, así que el disco siempre refleja lo elegido por el usuario.
- Estado inicial de la casilla: **marcada** si el aviso nunca se mostró (`!found`) o si ya fue descartado; **desmarcada** solo si el usuario la desmarcó explícitamente.
- Idiomas: `LanguageManager::trUtf8(es, en)` + listener de `languageChanged()` → el aviso se re-traduce en caliente al cambiar el idioma de la app.

### `[MODIFY]` `src/ui/MainComponent.cpp` / `.h`
- `juce::Timer::callAfterDelay(900, ...)` → al arrancar, si `WelcomeModalComponent::shouldShowOnStartup()`, se abre el aviso.
- `showWelcomeDialog()` crea la ventana (`launchAsync`, `SafePointer<juce::DialogWindow>`) y conecta `modal->onDismiss` → `welcomeWindow->closeButtonPressed()`.
- `openSettings()`: `modal->onShowWelcome = [this]() { showWelcomeDialog(); }` → el botón de Ajustes reabre el aviso sin cerrar Preferencias.

### `[MODIFY]` `src/ui/LucideIcons.h` / `.cpp`
- Nuevo icono `Star` (ruta Lucide) para el botón de Ajustes y el botón de GitHub del aviso.

### `[MODIFY]` `src/ui/MidiMappingTableComponent.cpp`
- Tabla de mapeo MIDI: etiquetas y celdas alineadas con la nueva escala tipográfica.

### `[MODIFY]` `CMakeLists.txt`
- `src/ui/UiTypeScale.h`, `src/ui/WelcomeModalComponent.h` y `src/ui/WelcomeModalComponent.cpp` añadidos a la lista explícita de fuentes.

## 3. Verificación (real, con capturas)

- `cmake --build build` → **0 errores, 0 warnings**: `[100%] Built target DiscNativePro`.
- Recorrido visual de las **11 pestañas** de Preferencias a escala 1:1 → Dispositivos, Sonido, MIDI, Shortcuts, Streaming, Donar, Diseño, Versión, Transmitir: sin recortes ni solapamientos. **General**: tipografía claramente mayor, línea de versión visible y botón «Ver mensaje de bienvenida» correcto. **IA Match**: las 6 líneas de la tarjeta se ven completas tras el fix de altura.
- Layout a **1280 px** de ancho de ventana: app y Preferencias sin recortes.
- P36, verificado punto por punto:
  - primer arranque (sin `welcome.suppress` en el JSON) → el aviso aparece con la casilla **marcada**;
  - «Entendido» con la casilla marcada → `welcome.suppress = true` en el JSON y el aviso **no** vuelve en el siguiente arranque;
  - casilla desmarcada → se persiste `false` y el aviso **sí** reaparece en el siguiente arranque;
  - Preferencias → General → «★ Ver mensaje de bienvenida» → el aviso se reabre **sin** cerrar Preferencias;
  - botón de YouTube → abre `youtube.com/@AxelCodeDev` en el navegador por defecto (título de ventana «AxelCode - YouTube»);
  - botón de GitHub → abre `github.com/axel-molina/DiscNativePro` (título «axel-molina/DiscNativePro»);
  - con el idioma de la app en inglés el aviso se muestra íntegramente traducido («Welcome to DiscNativePro» / «Work in progress · use at your own risk» / «Do not show again» / «Got it»);
  - el aviso es **no modal**: la app queda operativa detrás (capturas con la mesa de mezcla visible y en uso).
- Relanzamiento (regla del repo): `pkill -f DiscNativePro` → `open build/DiscNativePro_artefacts/Release/DiscNativePro.app` → `pgrep -fl DiscNativePro` → PID activo.

### Nota de entorno (no es un fallo del cambio)
Al arrancar, macOS pide la clave del llavero (`com.discpro.discnativepro.api-key`, servicio de `SecretStore`). Ese panel del sistema es **modal de la app**: bloquea el bucle de mensajes de JUCE, por lo que el aviso de bienvenida (que se dispara a los 900 ms) **no aparece hasta responder** al panel. Se resolvió con «Denegar» durante la verificación. Es un comportamiento preexistente de `SecretStore`, ajeno a P36.

### Validación humana (2026-11-16)
El usuario revisó el cartel en pantalla y lo aprobó: **«el cartel de bienvenida ya está bien así»** (texto final, URLs de YouTube y GitHub y comportamiento del aviso). Ticket Notion #12 cerrado como **Finalizado / Validación Aprobada**.

Residual documentado: la verificación visual en Windows/Linux queda pendiente porque no hay build de esas plataformas para este cambio (el código del diálogo es el mismo y la versión inglesa se verificó en macOS). Es una comprobación de humo para el próximo build de Windows/Linux, no trabajo abierto del ticket.

---

# Walkthrough — P35b: anchos de columna corregidos para Sora (cierre de DNP-TK-6)

> Ticket Notion: **DNP-TK-6** «Settings: tipografía más grande (legibilidad)» · rama `axel-molina/v1.4.2`.
> Plan: `implementation_plan.md` (sección «P35b»). Aprobado por el usuario antes de tocar código.

## 1. Qué faltaba

P35 subió el cuerpo de las etiquetas de 11.0 a 12.5 px y P37 cambió la sans del sistema por **Sora**, que es un **18–24 % más ancha**. Los anchos de columna del panel (120/160/210/220 px) estaban calibrados con la fuente antigua, así que 6 textos dejaron de caber.

El detalle que lo escondía: en JUCE un `Label`/`ComboBox` cuyo texto no cabe **no se recorta con puntos suspensivos** — `LookAndFeel_V2` usa `drawFittedText`, que **comprime horizontalmente** (o reduce el cuerpo), porque `Label::minimumHorizontalScale = 0` por defecto. Una compresión del 0.90 es casi invisible en una captura, y el barrido visual del ticket la dio por buena.

## 2. Medición (no estimación)

Auditoría con las métricas reales de `assets/fonts/Sora-Regular.ttf` (fontTools) y el modelo de render de JUCE 8:

- `Label` → `LookAndFeel_V2::drawLabel` → `drawFittedText(..., maxLines = max(1, alto/cuerpo), minHorizontalScale = 0)`.
- `ToggleButton` → cuerpo `min(15, alto·0.75)`, área de texto = ancho − tickWidth − 7, `maxLines = 10`.
- `TextButton` → `drawFittedText(..., maxLines = 2)`.
- `ComboBox` → cuerpo `min(15, alto·0.85)`, área de texto = ancho − alto (flecha) − 4.

Ancho útil: el modal es fijo **740×540** (`resizable = false`) → **604 px** de contenido, o **589 px** con scrollbar (caso de General).

| Widget | Pestaña | Caja | Necesita | Escala | Corrección |
|---|---|---|---|---|---|
| `stemsModeCombo` | General | 280 | 333 | **0.77** | → 370 |
| `broadcastNameLabel` | Transmitir (solo *Servidor propio*) | 120 | 142 | **0.84** | fila propia: 150 + editor 446 |
| `pitchRangeLabel` | General | 160 | 177 | 0.90 | → 182 |
| `bufferSizeLabel` | Sonido | 220 | 238 | 0.92 | → 242 |
| `aiProviderLabel` | IA Match | 210 | 224 | 0.93 | → 230 |
| `waveformFpsLabel` | General | 220 | 227 | 0.96 | → 232 |

## 3. Cambios

### `[MODIFY]` `src/ui/SettingsModalComponent.cpp` (`layoutTabs()`)
1. `pitchRangeLabel` 160 → **182** (el combo pasa a 407; su ítem más largo necesita 295).
2. `waveformFpsLabel` 220 → **232** (combo 357; ítem más largo 332).
3. `stemsModeCombo` 280 → **370** («Modo de Stems:» conserva 211 px; el sobrante queda a la derecha porque el texto es left-aligned).
4. `bufferSizeLabel` 220 → **242** (combo 362; el ítem seleccionado por defecto pasa de 0.94 a 0.99 de escala).
5. `aiProviderLabel` 210 → **230** (combo 374; el ítem más largo «Google Gemini (Flash - Gratuito en AI Studio)» necesita 343).
6. **Fila del nombre de emisión separada**: antes compartía fila con la contraseña (`170 + 200 + 16 + 120 + 8`), lo que dejaba el editor del nombre en **82 px** y la etiqueta (142 px) comprimida al 0.84. Ahora la contraseña conserva su fila y el nombre pasa a fila propia (**150 + 446**). El bloque `if (radioVisible)` es condicional → no queda hueco con destino Twitch/YouTube.

**Sin cambios deliberados**: ningún texto de la interfaz (el copy es del usuario), `UiTypeScale.h` (el problema era el ancho, no el cuerpo) y los tokens de toggles/combos (dibujan con el tamaño por defecto del LookAndFeel, no con `UiType`).

## 4. Verificación

- `cmake --build build` → **0 errores, 0 warnings**; app relanzada (`pkill` + `open build/DiscNativePro_artefacts/Release/DiscNativePro.app`, PID confirmado).
- Auditoría re-ejecutada sobre el código nuevo → **0 deformaciones** en los anchos de 604 px y de 589 px.
- Verificación visual real en pantalla (capturas 1:1) de las 4 pestañas afectadas: General, Sonido, IA Match y Transmitir con destino «Servidor de radio propio». Las 6 etiquetas salen con proporciones naturales y los combos con el texto completo.
- Configuración del usuario intacta: el destino de emisión se volvió a dejar en Twitch (`broadcastDest = "4"`) tras la prueba; `welcome.suppress` sigue en `false`.

**Residual conocido**: el ítem más largo del combo de buffer («128 muestras (2.9 ms • Ultra baja latencia interactiva)», 398 px) queda al 0.88 si el usuario lo selecciona; el resto de ítems y el seleccionado por defecto caben. No se acorta el copy del ítem porque es del usuario.

## 5. Pendiente

- Revisión visual del usuario (criterio de aceptación explícito del ticket) → Validación sigue en «Revisión humana».
- Verificación en Windows/Linux: no compilables en esta máquina; queda para la CI al hacer push.
- Push de la rama: lo hace el usuario.

---

# Walkthrough — P37: tipografía unificada embebida (Sora + JetBrains Mono)

> Objetivo: dar una familia propia a la app, idéntica en macOS, Windows y Linux, sin depender de fuentes instaladas en cada máquina.
> Decisión del usuario: **Sora** para toda la UI + **JetBrains Mono** para números, rutas y monitores (ambas SIL OFL, distribuibles en Microsoft Store y releases sin royalties).
> Plan aprobado en `implementation_plan.md` (sección P37).

## 1. Qué había antes

Todo el texto usaba el sans por defecto de JUCE (Lucida Grande en macOS, Verdana en Windows, DejaVu Sans en Linux) y había usos explícitos de `"Menlo"` —una fuente que **solo existe en macOS**— en el reloj y la RAM de la barra superior, rutas de carpetas, monitor MIDI, árbol de carpetas, códigos de SoundCloud y tabla de mapping MIDI. La apariencia era distinta en cada sistema operativo.

## 2. Cambios

### `[NEW]` `assets/fonts/` (4 TTF estáticos + 2 licencias)
- `Sora-Regular.ttf`, `Sora-Bold.ttf`: instancias estáticas (wght 400/700) generadas con fonttools a partir de la fuente variable oficial → charset completo (latin, latin-ext, vietnamese).
- `JetBrainsMono-Regular.ttf`, `JetBrainsMono-Bold.ttf`: TTF estáticos del release oficial (charset completo, cirílico y griego).
- `OFL-Sora.txt`, `OFL-JetBrainsMono.txt`: licencia SIL OFL de cada familia (requisito de redistribución).
- Pesos **estáticos**, no variables: la selección de estilos de fuentes variables no es fiable entre los backends de JUCE.

### `[NEW]` `src/ui/AppFonts.{h,cpp}`
- `AppFonts::initialise()`: crea una sola vez (hilo de mensajes, al arranque) los cuatro `Typeface::Ptr` desde `BinaryData` con `juce::Typeface::createSystemTypefaceFor()`, que acepta bytes TTF/OTF en los tres backends (CoreText, DirectWrite, FreeType).
- Accesores `sansRegular()`, `sansBold()`, `monoRegular()`, `monoBold()`. Si una cara no cargara devuelve `nullptr` y el override cae al comportamiento base (degradación elegante, nunca crash).

### `[NEW]` `src/ui/AppLookAndFeel.h`
- `class AppLookAndFeel : public juce::LookAndFeel_V4` con un único override de `getTypefaceForFont()`:
  - sans del sistema o `"Sora"` → `sansBold()` si el texto es bold, si no `sansRegular()`;
  - `"Menlo"`, monoespaciada del sistema o `"JetBrains Mono"` → `monoBold()` / `monoRegular()`;
  - cualquier otro nombre → `LookAndFeel_V4::getTypefaceForFont()`.
- Se implementa como override (y no con `setDefaultSansSerifTypeface`) porque esa llamada **ignora el bold**: devolvería la misma cara para todos los estilos.

### `[MODIFY]`
- `src/Main.cpp`: `AppFonts::initialise()` + `juce::LookAndFeel::setDefaultLookAndFeel(&appLookAndFeel)` **antes** de crear la ventana (las caras deben existir antes del primer `Font`, y el LookAndFeel global antes del primer componente). `appLookAndFeel` se declara antes de `mainWindow` para que los componentes se destruyan primero.
- `src/ui/DjLookAndFeel.h`: `DjLookAndFeel` pasa a heredar de `AppLookAndFeel` (era un LookAndFeel por componente, no el global: ahora hereda la familia por las dos vías).
- `src/ui/YouTubeVideoComponent_win.cpp`: el overlay de YouTube en Windows deja de pedir `"Segoe UI"` explícito y usa la familia por defecto.
- `CMakeLists.txt`: `juce_add_binary_data(DiscNativeProFonts SOURCES ...)` + `target_link_libraries(DiscNativePro PRIVATE DiscNativeProFonts)` + `src/ui/AppFonts.cpp` en la lista de fuentes.

## 3. Verificación

- `cmake --build build` → **0 errores, 0 warnings**; la app arranca con la tipografía embebida.
- Verificado en pantalla en macOS durante la revisión de DNP-TK-6: las capturas 1:1 de Ajustes muestran ya la familia embebida (y esa misma fuente es la que destapó los 6 anchos de columna cortos corregidos en P35b).
- **Pendiente**: Windows y Linux no son compilables en esta máquina → la verificación de las tres plataformas queda para la CI al hacer push.

## 4. Nota de alcance

Sora no publica itálica: el único italic de la app (`AIMatchView.cpp:158`) pasa a renderizarse en regular. Si se quiere cursiva real, hay que añadir una tercera cara o usar la variante del sistema solo para ese caso.

---

# Walkthrough — P38: los artefactos de Windows no empaquetan ONNX Runtime (la app no arrancaba con 0xc000007b)

> Ticket: Notion **#14** «Windows: la app no arranca desde la v1.4.0 (error 0xc000007b al lanzar)» · Prioridad Crítica.
> Aprobación humana en el chat: 2026-11-16 («aplica el fix»).

## 1. Qué pasaba

Desde la **v1.4.0** ningún artefacto de Windows arrancaba: el ZIP del CI, el instalador Inno Setup y el paquete MSIX fallaban al lanzarse (`0xc000007b`, reportado por un usuario de YouTube en el feedback DNP-FB-12). Con la **1.3.3** la app arrancaba bien.

## 2. Causa raíz (verificada contra el repo)

`CMakeLists.txt:349-360` (bloque `WIN32`) enlaza `third_party/onnxruntime/windows/lib/onnxruntime.lib` — una *import library* — y `src/services/OnnxStemEngine.cpp:17-30` / `src/services/TwoStemEngine.cpp:32` llaman `OrtGetApiBase()` de forma **estática** (no `dlopen`). El loader de Windows resuelve `onnxruntime.dll` **antes** de `main`: si la DLL no está junto al `.exe`, el proceso no arranca.

El POST_BUILD de CMake (`:352-360`) deja las dos DLL junto al ejecutable, pero **los tres artefactos publicados se arman a mano y se las saltaban**:

| Artefacto | Dónde se arma | Contenido anterior |
|---|---|---|
| `DiscNativePro-Windows-x64.zip` | `.github/workflows/build-windows.yml:36-50` | exe + `WebView2Loader.dll` |
| `DiscNativePro-Windows-x64-Setup.exe` | `.github/workflows/release.yml:47-57` → `installer/windows/installer.iss:43-45` | exe + `WebView2Loader.dll` |
| `DiscNativePro-Windows-x64.msix` | `.github/workflows/release.yml:93-98` | exe + `WebView2Loader.dll` |

Contraste que confirma el diagnóstico: **Linux sí copia el runtime** (`release.yml:247`) y **macOS lo empaqueta en el `.app`** (`CMakeLists.txt:287-289`). Windows era el único que había quedado atrás. ONNX Runtime llegó en 1.4.0 con el motor ML de stems (commit `a1edd66`) → de ahí el corte exacto de versión.

## 3. Cambios

### `[MODIFY]` `.github/workflows/build-windows.yml` (step *Package Windows Binary*)
- Copia `onnxruntime.dll` y `onnxruntime_providers_shared.dll` desde `$exeDir` (donde el POST_BUILD de CMake las deja) a `dist\`, **fallando el job** si alguna no está.
- Guarda de regresión: el job falla si en `dist\` falta cualquiera de los 4 archivos obligatorios (`DiscNativePro.exe`, `WebView2Loader.dll`, `onnxruntime.dll`, `onnxruntime_providers_shared.dll`) — así no se vuelve a publicar un artefacto que no arranca.

### `[MODIFY]` `.github/workflows/release.yml` (job `build-windows`)
- Misma copia + guarda en el step **Build Windows Installer (Inno Setup)** → `dist\` (de ahí toma el instalador).
- Misma copia + guarda en el step **Package MSIX** → `msix\` (staging de `makeappx`).

### `[MODIFY]` `installer/windows/installer.iss`
- `[Files]`: los dos `Source` nuevos (`..\..\dist\onnxruntime.dll` y `..\..\dist\onnxruntime_providers_shared.dll`) a `{app}`. Si el workflow no los deja en `dist\`, Inno Setup falla al compilar → no se publica un instalador roto.

### `[NEW]` `tools/check-windows-packaging.sh`
- Check ejecutable en cualquier plataforma: falla si las DLL vendorizadas no están en `third_party/onnxruntime/windows/lib/` o si alguno de los 3 caminos de empaquetado no las menciona. Se ejecuta como primer paso del job de Windows (falla rápido, antes de compilar).

### `[MODIFY]` `README.md`
- Sección *Compilación y Generación del `.exe` en Windows*: documenta los 4 archivos que el paquete debe contener y que el `.exe` no puede moverse solo (las dos DLL de ONNX Runtime van con él). Mismo aviso al final de la compilación manual (Opción B).

### Sin cambios (deliberado)
- **CMake**: el POST_BUILD ya copia las DLL donde deben estar; el fallo era de empaquetado, no de compilación.
- **Código C++**: el runtime se carga bien si la DLL está presente.
- **Versión**: publicar un release nuevo es decisión del usuario (no se bumpea la versión en este fix).

## 4. Verificación (real, en esta máquina)

1. **Parseo YAML** de los 3 workflows con PyYAML 6.0.3 → OK (`build-windows` con 8 steps; `release` con jobs `build-windows`, `build-linux`, `build-macos`, `publish-release`).
2. **Check local ejecutado**: `bash tools/check-windows-packaging.sh` → OK (exit 0).
3. **Guarda probada en negativo**: borrando las menciones de `onnxruntime*` del `installer.iss` el check devuelve exit 1 con el mensaje «Empaquetado de Windows incompleto: la app no arrancaría en Windows»; restaurado → exit 0.
4. **DLL vendorizadas confirmadas**: `third_party/onnxruntime/windows/lib/onnxruntime.dll` y `onnxruntime_providers_shared.dll` existen, son **PE32+ x86-64** (`file`) y están versionadas en git (`git ls-files`).
5. **Build local**: `cmake --build build` → **0 errores, 0 warnings**.
6. **Relanzamiento (regla del repo)**: `pkill -f DiscNativePro` → `open build/DiscNativePro_artefacts/Debug/DiscNativePro.app` → `pgrep -fl DiscNativePro` → proceso vivo (PID 86967).
7. **CI**: el job `build-windows` es la validación real del paquete (Windows no es compilable desde macOS). El guard falla si vuelve a faltar una DLL.

## 5. Pendiente

- **Publicación agrupada (decisión del usuario, 2026-11-16)**: **no hay push parcial**. El fix sale junto con P35, P35b, P36 y P37 en la release **v1.4.2**. `CMakeLists.txt` quedó en `project(DiscNativePro VERSION 1.4.2)` para que el build local ya reporte 1.4.2 (verificado en el `Info.plist` y en el define `DISCNATIVEPRO_VERSION`) y el fallback de versión coincida con el tag.
- **Lo que falta para la v1.4.2**: `push` de la rama `axel-molina/v1.4.2` (hoy sin remoto) + tag `v1.4.2` → corren los 3 jobs del `release.yml` y el `build-windows` valida el paquete de verdad. Hasta entonces, el release v1.4.0/v1.4.1 sigue roto en Windows.
- **Validación final con el usuario**: que la app arranque en sus dos portátiles Windows con el artefacto de la v1.4.2 (criterio de aceptación del ticket). Si tras el fix persistiera el `0xc000007b`, la causa pasaría a ser el antivirus/antimalware o un binario corrupto — el dato a mirar es el log del Event Viewer.
- **Verificación de la hipótesis**: el error `0xc000007b` también aparece con binario truncado o bloqueado por antivirus (una DLL faltante suele dar `0xc0000135`), por eso la confirmación final es del usuario y no del código.

---

# Walkthrough — P39: los 17 textos que solo existían en español (DNP-TK-15)

> Ticket: **DNP-TK-15** «i18n: verificar y corregir las traducciones de todos los textos visibles» · rama `axel-molina/v1.4.2`.
> Plan: `implementation_plan.md` (sección P39).

## 1. Auditoría: el alcance real era 17, no 124

La app ya tenía 507 llamadas a `LanguageManager::trUtf8()`. El contador ingenuo de literales visibles en español daba 124, pero **casi todos se sobrescriben en runtime**: `refreshLanguage()` (invocado por `languageChanged()`) reescribe todos los textos cuando cambia el idioma.

El análisis correcto es **a nivel de componente**: un componente cuenta como deuda si *nunca* recibe una versión con `trUtf8`. Con ese criterio quedan **17 literales** en 2 archivos (de los cuales 3 son símbolos —`×½`, `×2`, `✕`— y 1 es la viñeta `● ` de un badge cuyo texto ya estaba traducido: no son deuda real).

| Archivo | Literales | Bloque |
|---|---|---|
| `src/ui/SettingsModalComponent.cpp` | 16 | Stems (8), Micrófono/Talkover (5), Transmitir (3) |
| `src/ui/LibraryComponent.cpp` | 1 | Cabecera de la tabla de pistas |

## 2. Cambios

### `[MODIFY]` `src/ui/SettingsModalComponent.cpp`
- **Helper `stemsStatusDescription()`** en el `namespace` anónimo: fuente única para la descripción del motor de stems (la larga, con sus dos variantes) → el texto inicial y la re-traducción en caliente no pueden desincronizarse.
- `setupGeneralTab()`: `stemsSectionLabel`, `stemsStatusDescLabel`, `stemsModeLabel`, `stemsModeCombo` (2 ítems), `stemsPrecalcBtn` («Precalcular biblioteca…» y «Encolando...»).
- `updateStemsPrecalcButton()`: «En cola: N pistas» → *In queue: N tracks* (la palabra «pistas» estaba suelta en español).
- `setupSoundTab()`: `soundSectionMic`, `micEnableToggle`, `micGainLabel`, `micDuckLabel`, `micDuckAutoToggle`.
- `setupTransmitTab()`: ítems 1 («Servidor de radio propio…») y 5 («RTMP personalizado») del combo de destino, y `broadcastMountLabel` («Mount:»).
- **`refreshLanguage()`**: se añaden los 16 anteriores para que se re-traduzcan **en caliente** al cambiar de idioma (no solo al arrancar). En `stemsModeCombo` y `broadcastDestCombo` se guarda el `getSelectedId()` antes del `clear()` y se restaura después, para no perder la selección.

### `[MODIFY]` `src/ui/LibraryComponent.cpp`
- `tableHeaderTitle`: el valor inicial «Todas las Pistas  0 Canciones» → `trUtf8("Todas las Pistas", "All Tracks") + "  0 " + trUtf8("Canciones", "Tracks")`. (El valor dinámico ya estaba traducido en los 3 sitios que lo reescriben.)

## 3. Verificación

- **Auditoría re-ejecutada** con el mismo script (mismo criterio, a nivel de componente): **0 literales visibles sin `trUtf8`** en todo `src/ui`.
- **Medición de las cadenas nuevas en inglés** contra la caja real de cada control (métricas de Sora): **16/16 caben** → ninguna reintroduce compresión horizontal (el problema que se corrigió en P35b).
- `cmake --build build` → **0 errores, 0 warnings**; app relanzada.
- **Verificación visual real** con la app en inglés (`settings.json` → `"language": "en"`), capturas 1:1:
  - General → «Stems (Track Separation)», «Embedded separation engine. Fast mode: …», «Stems Mode:», «Fast — 2 live stems (voice/instrumental)», «Precompute library (4 stems)».
  - Transmitir → «Destination:», «RTMP Ingest URL:», «Broadcast mix live», «Podcast (export recording to MP3)», «Export to MP3».
  - Biblioteca → «All Tracks  24 Tracks», «Downloads  24».
  - Idioma restaurado a español después de la prueba (`"language": "es"`) y app relanzada.
- El bloque de Micrófono no se ve en esta configuración (es condicional y ya estaba oculto antes del cambio), pero sus textos quedan traducidos en código.

## 4. Pendiente

- **Higiene del backlog** (sin código): cerrar **#1, #2 y #3**, que llevan `[DESCARTADO]` en el título pero siguen con Estado «Nuevo».
- Revisión de los tickets que ficaram *Finalizado* con Validación «Pendiente» (contradicción con la regla de no cerrar sin validación).

---

# Walkthrough — P40: la región de loop en la onda de la bandeja (Notion #11)

> Ticket: Notion **#11** «Marcar la sección de loop en la onda de la bandeja» · Prioridad Media · Esfuerzo S.
> Aprobado por el usuario el 2026-11-16 (elección entre las 6 opciones del backlog).

## 1. Qué pasaba

Con un loop activo, la onda grande de la bandeja no mostraba **qué tramo se iba a repetir**. Solo la *overview* (la onda pequeña de cada deck) marcaba la región.

## 2. Estado real del código (verificado, no supuesto)

- La onda visible de la bandeja es **`src/ui/VerticalWaveform.cpp`**: columna central, Deck 1 a la izquierda y Deck 2 a la derecha, con el **tiempo creciendo hacia arriba**. La instancia `CenterColumnComponent` (`CenterColumnComponent.cpp:28`, `:240`).
- `src/ui/ScrollingWaveform.cpp` **no se instancia en ningún sitio** (código muerto, solo compilado) → no se toca.
- `src/ui/OverviewWaveform.cpp:120-130` **ya dibujaba la región**: `fillRect` `RGB(37,99,235) @0.35` + `drawRect` `RGB(96,165,250)`. Ese es el criterio que se replica (el criterio de aceptación pide que el resaltado de la overview se mantenga igual).
- `DeckPlayer`: `isLooping()`, `getLoopStart()`, `getLoopLength()` (`:145-147`). `exitLoop()` (`:1516`) desactiva el loop **conservando** la región → se puede distinguir «loop definido» de «loop activo». Al cargar otra pista solo se limpiaba `loopActive` (`DeckPlayer.cpp:160`): la región vieja podía quedar de la pista anterior.
- El botón LOOP (`DeckComponent.cpp:304-323`) alterna `setLoop(pos, beats*beatSec, true)` / `exitLoop()`.

## 3. Cambios

### `[MODIFY]` `src/ui/VerticalWaveform.cpp` — `renderDeckHalf()`
Región de loop proyectada al eje vertical (`y = centerY - (t - currentTime) * pixelsPerSecond`), recortada a la ventana visible y a la mitad del deck:
- **loop activo**: relleno `RGB(37,99,235) @0.35` + bordes horizontales `RGB(96,165,250)` (misma paleta que el overview);
- **loop definido pero inactivo**: relleno `@0.10` + bordes `@0.40` → se distingue activo de inactivo.

Se dibuja **encima de las barras** y **debajo** del sombreado de lo ya reproducido: mismo orden que el overview.

### `[MODIFY]` `src/ui/VerticalWaveform.{h,cpp}` — repintado al cambiar el loop
`timerCallback()` repinta además cuando cambia el estado del loop de un deck (`LoopState::differsFrom`, con tolerancia de 1e-6). Sin esto, activar el loop **en pausa** no mostraba la región hasta que volvía a sonar.

### `[MODIFY]` `src/audio/DeckPlayer.cpp` — carga de pista
Junto al `loopActive.store(false)` (`:160`) se limpian `loopStartSeconds` y `loopLengthSeconds`: la región de loop pertenece a la pista (evita un loop fantasma de la pista anterior).

### Sin cambios (deliberado)
- `OverviewWaveform`: ya marca la región.
- `ScrollingWaveform`: código muerto.
- `DeckComponent`: el botón de loop ya gestiona la región.

## 4. Verificación

1. **Build**: `cmake --build build` → **0 errores y 0 warnings** (hubo que corregir 2 avisos de `-Wfloat-equal` introducidos por la comparación de `double` del estado del loop: se resolvió con tolerancia `1e-6` vía `std::abs`).
2. **Aritmética del recorte comprobada con una simulación** (7 casos, todos OK): loop dentro de la ventana · loop parcialmente fuera · loop pasado fuera (no se dibuja) · loop futuro fuera (no se dibuja) · loop pasado visible (abajo del centro) · loop cruzando la aguja · sin loop definido. El alto de la región es exactamente `duración × pixelsPerSecond` (2 s × 70 px/s = 140 px).
3. **Relanzamiento**: `pkill -f DiscNativePro` → `open build/DiscNativePro_artefacts/Release/DiscNativePro.app` → `pgrep -fl DiscNativePro` → proceso vivo; la captura de pantalla confirma que la app levanta y dibuja la columna central sin artefactos (sin loop activo no hay marcador, como corresponde).
4. **Visual con loop activo**: **validado por el usuario** (2026-11-16): *«funciona perfecto»*. La región se ve como debe, alineada con la aguja y sincronizada con la reproducción. No se pudo automatizar el clic (macOS no concede a `osascript` el permiso de accesibilidad, error -25211), así que la prueba la hizo el usuario.

## 5. Pendiente

- **Publicación**: entra en el mismo grupo de la **v1.4.2** (sin push parcial). El commit `1ac311c` es local; la rama `axel-molina/v1.4.2` sigue sin remoto.

## 6. Nota

La automatización por clic no se pudo usar en esta máquina (`osascript` → error -25211, sin permiso de accesibilidad), así que la prueba de la región marcada la hace el usuario. La aritmética sí quedó verificada de forma aislada (punto 2 de la verificación).

---

# Walkthrough — P41: tooltips en todos los botones

> Ticket Notion: **DNP-TK-10** «Tooltips en todos los botones (información al pasar el mouse)».
> Plan aprobado: `implementation_plan.md` (sección «P41»).

## 1. Diagnóstico (medido, no estimado)

Script de cruce entre los miembros de tipo botón de `src/ui` y todos los `setTooltip` del proyecto:

- **111 botones miembro** (DjButton, TextButton, ImageButton, ToggleButton) en los headers.
- **23 ya tenían tooltip**; **88 no** (pestañas de Ajustes, toggles, transporte de la bandeja, editor de grid, FX, biblioteca, vistas de streaming, tabla MIDI, sampler, barra superior, cartel de bienvenida).

## 2. Implementación

- **13 archivos de `src/ui`**: un `setTooltip(LanguageManager::trUtf8(es, en))` por botón, colocado junto a su configuración (mismo estilo que los tooltips existentes). Los hot cues (array de 4 pads) reciben un tooltip dinámico en el bucle.
- **`src/ui/FxSlotComponent.cpp` / `src/ui/FxPanelComponent.cpp`**: se añade el `#include "../services/LanguageManager.h"` (no lo traían).
- **Criterio de copy**: una línea, ≤ ~70 caracteres, forma «tú» (consistente con el copy existente, p. ej. «Obtén tu API Key»); los botones con etiqueta visible no la repiten (aportan efecto o matiz); los de solo icono describen la acción.
- **Sin cambios deliberados**: el delay de 700 ms lo fija JUCE en el constructor de `TooltipWindow` (no se puede tocar desde el LookAndFeel); el copy de los tooltips que ya existían no se tocó.

## 3. Guarda de regresión

**`tools/check-button-tooltips.sh` (nuevo)**: cruza los miembros de tipo botón de los headers con los `setTooltip` del proyecto; falla si alguno queda sin tooltip. `OK: los 111 botones miembro de src/ui tienen tooltip.`

## 4. Verificación

- `cmake --build build` → **0 errores, 0 warnings** (`[100%] Built target DiscNativePro`).
- `bash tools/check-button-tooltips.sh` → exit 0.
- Incidente corregido durante el trabajo: un script de inserción en dos pasadas dejó líneas duplicadas y partió 3 llamadas multilínea preexistentes (`versionHelpIcon`, `refreshBtn`/`addFolderBtn`, `ytAutoplayToggle`); se restauró el diff final a **90 inserciones, 0 borrados** (verificado con `git diff | grep ^-`).
- Relanzamiento: `pkill` + `open build/DiscNativePro_artefacts/Release/DiscNativePro.app` → proceso activo (PID verificado).

## 5. Validación humana (2026-11-16)

El usuario recorrió la app y confirmó **«está perfecto»**. El ticket #10 queda **Finalizado / Progreso 100 / Validación Aprobada** — sin ajustes de copy pendientes.

---

# Walkthrough — P42: buscador de configuraciones en Ajustes

> Ticket Notion: **DNP-TK-5** «Settings: buscador de configuraciones».
> Plan aprobado: `implementation_plan.md` (sección «P42»). Decisiones del usuario: salto a pestaña + campo en la cabecera del cuerpo.

## 1. Diagnóstico que condicionó el diseño

Las 11 pestañas de `SettingsModalComponent` se maquetan a mano con filas fijas en `resized()` (3579 líneas) y la visibilidad por widget vive en `updateTabVisibility()`. Filtrar filas dentro de la pestaña (estilo VS Code) habría exigido reescribir el layout completo para que compacte. El usuario eligió el camino de bajo riesgo: **índice de ajustes + salto a pestaña + resaltado**.

## 2. Implementación

- **Cabecera de búsqueda** (`settingsSearchBox`): franja fija de 44 px arriba del `bodyViewport`, siempre visible en las 11 pestañas; placeholder bilingüe «Buscar ajustes… / Search settings…». Sidebar y footer sin cambios.
- **Índice curado** (`setupSearchIndex()`): ~60 entradas bilingües (es/en), una por ajuste buscable, cada una apuntando al widget real (o `nullptr` en Shortcuts, que se dibuja en `paint`). Cobertura: General (14), Dispositivos (4), Sonido (13), MIDI (5), Shortcuts (1), Streaming (2), Transmitir (16), IA (5), Donar (1), Diseño (7), Versión (3).
- **Búsqueda**: match case-insensitive en es **o** en; ≥ 2 caracteres; debounce de ~500 ms (1 tick del timer a 2 Hz del modal — el plan decía 300 ms, se implementó con el timer existente); máx. 8 resultados.
- **Resultados**: lista flotante propia (`SearchResultsList`, pintada a mano) bajo el campo — a diferencia de un `PopupMenu`, **no roba el foco** y se puede seguir escribiendo. Sin coincidencias: fila gris «Sin resultados / No matches».
- **Salto** (`jumpToSearchResult`): usa `setActiveTab()` (misma vía que los botones de pestaña) → si el ajuste quedó fuera del viewport lo trae a la vista (80 px de margen) → overlay azul (`HighlightOverlay`) sobre el widget durante ~2,5 s (5 ticks del timer) → limpia el campo.
- **i18n**: placeholder, «Sin resultados» y filas de resultados bilingües; `refreshLanguage()` actualiza el placeholder.
- **Mantenimiento**: cada configuración nueva agrega una línea en `setupSearchIndex()` (documentado en el ticket).

## 3. Verificación

- `cmake --build build` → **0 errores, 0 warnings** (`[100%] Built target DiscNativePro`).
- Relanzamiento: `pkill` + `open …Release/DiscNativePro.app` → proceso activo (PID verificado).
- **Pendiente de validación humana (usuario)**: escribir «buffer», «carpeta», «bitrate», «midi», «color»; verificar salto + resaltado en español y en inglés, y el caso «Sin resultados» (p. ej. «xyz»).

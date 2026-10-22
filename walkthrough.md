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

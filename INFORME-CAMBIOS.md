# Informe de cambios para revisión — rama `fix/audio-p0`

Resumen de una auditoría externa del proyecto con los arreglos aplicados, verificados
en lo posible sin depender de una máquina con Visual Studio. Todo está en 12 commits
sobre `main` (`4f5886f`), en 22 archivos, **+841 / −114** líneas.

Para reproduciros el binario: `cmake -B build -DAPP_VERSION=<v>` + `cmake --build build`.
La verificación de audio se hizo con un banco propio (`bench/`, fuera del repo) que
compila los `.cpp` reales contra JUCE 8.0.6 y los ejecuta.

---

## 1. Audio — defectos medidos y corregidos

| # | Archivo | Defecto | Medido antes → después |
|---|---|---|---|
| 1 | `DjMixer.h/.cpp` | El mixer procesaba **antes** de `prepare()`: `juce::dsp::IIR` llamaba a `getFilterOrder()` sobre un puntero nulo. Crash intermitente al arrancar. | muere en 3 de cada 5 arranques → **5/5 estable** |
| 2 | `DeckPlayer.cpp/.h` | El loop se pasaba del final hasta un bloque y el cursor quedaba en `start+exceso`: el inicio del loop rotaba en cada pasada. | 2,33 ms por pasada (140 ms en 60) → borde a **±1 muestra** |
| 3 | `SamplerBank.cpp` | `setResamplingRatio()` es *samplesInPerOutputSample*; se le pasaba el inverso. | 844,6 Hz (−2,92 st), 84,6 % del sample → **1000,4 Hz, 100 %** |
| 4 | `DeckFxChain.h/.cpp` | El Gate era un cuadrado duro (`sin>0?1:0`) pese al comentario "soft cosine transitions"; la fase era local. | salto de **0,475** por flanco → **0,0066** |
| 5 | `DjMixer.cpp` | Ninguna ganancia estaba suavizada: escalón por bloque. | master 1,0→0,5 era un salto de **0,5 en 1 muestra** → rampa de 20 ms; crossfader interpolado por muestra |
| 6 | `BeatDetector.cpp` | (a) la energía era media móvil de arrastre, (b) picos con máximo estricto, (c) offset desde el primer pico. | **0 BPM** con transitorios <100 ms; offset **+99 ms** → detecta 87/128/174; offset **+3…10 ms** |
| 7 | `AudioEngine.cpp`, `DjMixer.cpp`, `MasterLimiter.cpp` | Sin `ScopedNoDenormals`. | CPU en silencio (denormales en IIR) |
| 8 | `DeckPlayer.cpp` | `ScopedLock` bloqueante en el callback: `loadFile()` lo retiene durante E/S y ~1 MB de reserva. | `ScopedTryLock`: silencio durante la carga, nunca bloquea |
| 9 | `DeckPlayer.cpp` | `setLoop()` aceptaba longitud ≤ 0 → re-seek continuo. | se rechaza |

## 2. Servicios

| # | Archivo | Defecto | Arreglo |
|---|---|---|---|
| 10 | `LibraryCacheManager.cpp` | `clear()` tomaba `cacheMutex` y llamaba `saveCache()`, que lo vuelve a tomar: **deadlock** (la UI se congelaba). | `saveCache()` fuera del lock |
| 11 | `PlaylistStore.cpp` | Un `playlists.json` corrupto se leía como lista vacía y la primera edición lo **sobrescribía**: pérdida total sin aviso. | copia fechada `.corrupto-AAAAMMDD-HHMMSS.json` antes de continuar |
| 12 | `YouTubeAudioDownloader.cpp` (issue #4) | Descargaba **siempre** `yt-dlp_macos` y resolvía con rutas de macOS/`which`; el fallo era permanente porque el archivo existía. | asset por plataforma, `where` en Windows, ruta sin "Application Support" duplicado, validación con `--version` y borrado si no arranca |

## 3. Interfaz y empaquetado

| # | Archivo | Cambio |
|---|---|---|
| 13 | `MainComponent.cpp` | **F11 / ESC / botón** de pantalla completa pasan por el mismo `toggleFullScreenMode()` (modo kiosco, tapa la barra de Windows) con debounce de 500 ms. Antes el botón usaba `DocumentWindow::setFullScreen()` y dejaba la barra. |
| 14 | `Main.cpp` | `--fullscreen` (arranque directo en kiosco); la ventana arranca ocupando el área útil y aplica **escala global** cuando el layout (1800×860 lógicos) no cabe. |
| 15 | `MainComponent.cpp` | `MainComponent` **pedía foco de teclado y nunca lo tomaba**: `keyPressed()` no recibía nada. Ahora lo toma al mostrarse. |
| 16 | `GridEditorComponent.cpp/.h` | **BPM del grid editable a mano** (doble clic, 40–250, se persiste) y la **audición arranca siempre desde el principio** (antes, si el deck ya sonaba, seguía desde donde estuviera). |
| 17 | `CMakeLists.txt` | Versión **1.4.4**. El bump por `project(VERSION)` no se aplicaba porque `APP_VERSION` sale de `git describe` y el regex lo recorta. |

---

## 4. Cómo verificar

1. **Audio**: banco `bench/` (fuera del repo) — 7 pruebas que ejecutan los `.cpp` reales
   y miden con números (`bench/bench-output.txt`). Reproduce los antes/después de la tabla.
2. **Compilación**: `nmake` (MSVC 19.44) sin errores; `g++ -c` también en los TU de audio.
3. **Escucha pendiente**: el loop y el detector de BPM cambian comportamiento y **necesitan
   prueba en un deck real** (loops de 1/4 a 16 beats con keylock, y 20-30 pistas reales).

## 5. Riesgos / precedencia

- `DeckPlayer::getNextAudioBlock` ahora **re-entra** en sí mismo para trocear el loop
  (`inLoopCorrection` evita la recursión de la lógica). Es el cambio más delicado.
- El try-lock deja **un bloque de silencio** si se carga una pista en un deck sonando.
- La escala global de UI reduce todo si la pantalla es pequeña: revisar en 1080p y 4K.

## 6. Pendiente (no incluido a propósito)

1. **`UpdateService`**: ejecuta el instalador descargado con `getSize() > 1000` como única
   validación, y `stagedDir` sale de `settings.json` y se interpola en un script shell.
   Propuesta: allowlist de `github.com/axel-molina/DiscNativePro/releases/download/`,
   verificación de firma (Authenticode / `codesign --verify`) o `SHA256SUMS` firmado, y
   `isAChildOf()` sobre el directorio de datos.
2. **`settings.json` sin lock**: lo reescriben cuatro clases con read-modify-write; se
   pierden ajustes. Propuesta: un `SettingsStore` único con lock, `"version"` y `.bak`.
3. **Stems**: ~1,8 GB de RAM por pista de 15 min; el `shutdown()` libera la sesión ORT con
   el hilo de inferencia potencialmente dentro de `Run()`.
4. **Licencias**: `LICENSE` es GPL-3.0 y el README dice "Todos los derechos reservados";
   ningún instalador incluye la licencia y LAME va enlazado estático (LGPL).
5. **Firma/notarización**: macOS ad-hoc sin notarizar y Windows sin Authenticode.
6. **Layout**: la bandeja 2 se recorta en pantallas de 1550 px lógicos (el reparto de
   anchos de `MainComponent::resized()` no coincide con el tamaño real del componente).
7. **Modo día**: no viable por `ImageEffectFilter` (en JUCE 8 solo lo aplica
   `ShapeButton`); requiere centralizar la paleta en un tema claro.

## 7. Notas de la auditoría

- Los números de "antes" salen de ejecutar el código original en el banco, no de estimaciones.
- Dos hallazgos iniciales del análisis estático resultaron **falsos** y están corregidos:
  el Gate no estaba "muerto" (dependía de la fase publicada) y el sampler no sonaba agudo
  sino grave.

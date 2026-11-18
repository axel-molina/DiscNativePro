# Manual de Usuario — DiscNativePro

Guía de uso de la aplicación. Para compilar el proyecto, ver [README](../README.md).

## Primeros pasos

Al abrir la app verás la consola completa: dos decks con jog wheels, la
biblioteca a la izquierda, la mesa de mezclas a la derecha y la columna
central con transport, hotcues, loops y stems.

La primera vez se muestra un **mensaje de bienvenida** con un aviso sobre el
uso de la app. Podés marcar la casilla **"No mostrar de nuevo"** para que no
aparezca en los próximos inicios; si después querés volver a verlo, usá
**Ajustes → General → "Ver mensaje de bienvenida"**.

### Audio

- En **Ajustes → Dispositivos** elegís la salida de audio (CoreAudio en macOS,
  WASAPI/Media Foundation en Windows, ALSA/JACK en Linux) y el buffer.
- Auriculares: cada canal tiene su botón de preescucha (PFL) y su volumen de
  auriculares en la mesa.

## Ajustes

- **Buscador**: el campo **"Buscar ajustes…"** de la barra superior del modal
  filtra las opciones con 2 o más caracteres (funciona en español e inglés);
  al elegir un resultado salta a la pestaña correspondiente y resalta la
  opción unos segundos.
- **Idioma**: español / inglés en **Ajustes → General**. El cambio se aplica
  en caliente a toda la interfaz, incluidos los tooltips y el botón de
  actualizar (ACTUALIZAR / UPDATE), sin reiniciar la app.

## Biblioteca

- **Importar**: botón de carpeta o arrastrar carpetas/archivos al árbol.
  Formatos soportados: WAV, MP3, FLAC, AIFF, M4A/AAC, OGG.
- El árbol lateral organiza por carpetas; la tabla muestra BPM, tonalidad y
  estado de análisis.
- **Editar BPM / beat grid**: clic derecho sobre una pista → **"Editar BPM /
  beat grid"**. No hace falta cargarla a una bandeja: el botón PLAY del
  editor audiciona la pista por el canal de preview. Si la pista ya está
  cargada en un deck, el editor trabaja sobre esa bandeja. Los cambios se
  guardan por archivo.
- **YouTube**: en la barra superior buscás videos; arrastrás un resultado a un
  deck y la app descarga el audio (m4a) mostrando el progreso. Los tracks
  guardados viven en la carpeta de Descargas configurada.
- **SoundCloud**: pestaña dedicada con buscador de tracks, remixes y sets;
  los resultados se cargan a la bandeja con sus botones de carga (MP3
  directo para mezclar).
- **Pendrive / USB**: pestaña dedicada para bibliotecas portátiles.
- **Playlists**: sección en el árbol; arrastrar pistas para armarlas.

## Decks

- **Play/Cue/Sync**: transporte estándar. Sync ajusta el tempo contra el otro
  deck (usando el BPM detectado).
- **Pitch (fader)**: cambia el tempo. Con **KeyLock** activado el tono se
  mantiene; sin keylock el tono acompaña el tempo.
- **Key Shift**: ±6 semitonos independientes del tempo; el label vuelve a 0 al
  clicarlo, y los botones ▲/▼ suben/bajan de a 1.
- **Hotcues**: 4 pads por deck.
  - Pad vacío + clic → marca el hotcue en la posición actual.
  - Pad activo + clic → salta a ese punto.
  - **Clic derecho sobre un pad activo → "Borrar Hot Cue"**.
  - Los hotcues se **guardan por pista** (sobreviven al reiniciar la app); el
    grid corregido (BPM manual) se persiste igual.
- **Jog wheel**: el anillo externo hace nudge (pitch bend); arrastrando el
  platter con slip/scratch activo hacés scratch.
- **Slip Mode**: el audio sigue por debajo mientras hacés el gesto.
- **Beat Jump**: salta N beats adelante/atrás (tamaño elegido con el selector
  de beats).
- **Loop Roll**: mantener apretado = loop del tamaño elegido; soltar = vuelve.
- **Pitch bend**: los botones **+ / −** junto al pitch suben/bajan el tempo
  un 4% mientras se mantienen apretados (estándar de industria); al soltar
  vuelven al tempo del fader.
- **Región de loop en la onda**: el loop activo se marca sobre la forma de
  onda central y el overview (color intenso cuando está activo, tenue cuando
  solo está definido).
- **Títulos largos**: el nombre y artista de la pista se desplazan en
  marquesina cuando no entran en el display del deck.

## Stems (separación de voces e instrumentos)

- **Modo Rápido (2 stems)**: al cargar una pista al deck se separa voz e
  instrumental al vuelo (~segundos); la primera vez tarda un poco más, luego
  queda en caché.
- **Modo Completo (4 stems)**: VOC/DRM/BAS/OTH precalculados por pista
  (procesamiento en background). Podés encolar una carpeta entera con clic
  derecho en el árbol → "Separar 4 stems (carpeta)" o desde Ajustes → General
  → "Precalcular biblioteca".
- Cada stem tiene botón ON/OFF y el botón STEMS activa/desactiva todo.
- El estado de separación se ve en la biblioteca (badge) y en Ajustes.

## FX

- 3 slots por deck (Echo, Gate, etc.). En Echo/Gate, el botón PARAM elige la
  **división de beat** (1/4 … 2/1) — los efectos quedan sincronizados al BPM
  del deck.
- EQ de 3 bandas + filtro por canal en la mesa.

## Sampler

- Panel de 8 pads one-shot para disparar samples; clic en un pad vacío o
  arrastrar un archivo lo carga.

## MIDI

- La app detecta controladoras USB automáticamente (botón "Escanear" en el
  modal MIDI).
- **Perfiles de controladora**: el selector **PERFIL** del modal MIDI aplica
  la tabla de hardware y los mapeos por defecto de tu controladora (Pioneer
  DDJ, Denon MC, genérica). El badge indica **"Detectado: …"** cuando el
  perfil se eligió solo por el nombre del dispositivo, o **"Manual: …"** si
  lo elegiste vos.
- **Conflictos de mapeo**: la tabla muestra un contador ⚠ con los controles
  que comparten el mismo mensaje MIDI. Al aprender un control que ya está
  asignado, un diálogo ofrece **Reemplazar / Cancelar**.
- **Aprender (MIDI Learn)**: en la tabla de mapeo, seleccionás una acción,
  apretás "Learn" y movés el control físico de tu controladora.
- **Importar preset de la comunidad Mixxx**: botón "Importar Mixxx" del modal
  MIDI. Si tu controladora conectada tiene preset oficial de Mixxx (Hercules,
  Pioneer DDJ, Numark, Reloop, Denon y más), la app lo descarga, lo traduce a
  las acciones de DiscNativePro y muestra un reporte con el crédito al autor.
  Los controles que dependen de scripts JavaScript de Mixxx se omiten y se
  listan. También podés importar cualquier archivo `.midi.xml` manualmente.
- Todos los mapeos se guardan en `~/…/DiscNativePro/midi_mapping.json` y
  podés exportarlos/importarlos como JSON.

## Transmitir / Grabar

- **Transmitir**: Ajustes → Transmitir, con servidor Icecast/Shoutcast o RTMP
  propio (URL + credenciales).
- **Grabación**: botón REC de la barra superior; exporta a WAV sin pérdidas
  (24 bits).

## YouTube / Video

- Cada deck puede reproducir el video del clip de YouTube embebido (modo
  video) o solo el audio (modo audio) — configurable en Ajustes.
- En modo video el deck muestra el reproductor; el BPM/key no se analizan.

## Actualizaciones automáticas

La app detecta cómo fue instalada y gestiona las actualizaciones según el canal:

- **macOS (DMG / .app)**: en **Ajustes → Versión** apretás "Buscar actualizaciones"
  (motor Sparkle). La versión nueva se descarga y se instala al cerrar la app
  ("Install on Quit"); al volver a abrirla ya estás actualizado.
- **Windows (Setup.exe)**: la descarga se completa en segundo plano y se instala
  automáticamente al reiniciar la app ("Se instalará cuando la aplicación se
  reinicie").
- **Windows (Microsoft Store)**: no hay updater interno; Ajustes → Versión indica
  que las actualizaciones las gestiona la Store.
- **Linux (.deb)**: se actualiza desde el repositorio apt (`sudo apt upgrade`).
- **Linux (portable .tar.gz)**: al cerrar la app se reemplaza a sí misma por la
  versión nueva (con backup y relanzamiento automático).
- **Linux (AppImage)**: descargá el AppImage nuevo del release de GitHub y
  reemplazá el archivo (`chmod +x` si hace falta). El updater interno está
  pensado para el portable .tar.gz; en un AppImage la instalación se detecta
  como portable, así que conviene actualizar a mano.

## Atajos y sugerencias

- Doble clic en un knob lo vuelve a su valor por defecto.
- **Tooltips**: al pasar el mouse por los botones aparece su descripción, en
  español o inglés según el idioma elegido en Ajustes (se actualizan en
  caliente, sin reiniciar).
- Arrastrar un track desde la biblioteca a un deck lo carga; el deck izquierdo
  es Deck 1, el derecho Deck 2.
- El monitor de señales del modal MIDI muestra en vivo qué mensajes llega tu
  controladora — útil para diagnosticar.

## Solución de problemas

- **"requires macOS 13.3 o superior"**: la app usa un motor de IA embebido y
  binarios vendorizados que exigen macOS 13.3 (Ventura) o superior.
- **La app no abre en macOS (versión 1.4.0)**: defecto de empaquetado corregido
  en la versión 1.4.1. Descargá el DMG nuevo desde el release 1.4.1 y
  reemplazá la app; no hace falta desinstalar.
- **La app no abre en Windows (`0xc000007b` / `0xc0000135`, versiones
  1.4.0/1.4.1)**: faltaban las DLL de ONNX Runtime junto al ejecutable.
  Corregido en la 1.4.2: los paquetes ya las incluyen. No muevas el `.exe`
  solo a otra carpeta; llevá también `onnxruntime.dll` y
  `onnxruntime_providers_shared.dll`.
- **Los binarios de Linux (v1.4.2) no abren en Ubuntu 24.04 / Debian 13**:
  la imagen de CI cambió a Ubuntu 26.04 y los precompilados (tarball, `.deb`
  y AppImage) exigen glibc 2.43. Workaround: compilar desde fuente o usar
  Ubuntu 26.04. La corrección llega en la 1.4.3, con el build pineado a
  Ubuntu 24.04 (glibc ≥ 2.39: Ubuntu 24.04+, Debian 13+, Fedora 40+).
- **La pista suena más rápido/lento de lo esperado**: corregido desde la
  versión 1.3.3 (archivos 44.1 kHz vs 48 kHz). En la 1.4.2 se corrigió además
  un detune del modo 2 stems con archivos que no son 44.1 kHz. Si persiste,
  avisá con el formato del archivo y la placa de audio.
- **Descarga de YouTube falla**: revisá conexión; el error ahora se muestra en
  el deck. Un video ya en descarga no se vuelve a pedir (se reutiliza).
- **"No se pudo iniciar el extractor yt-dlp" (macOS Apple Silicon)**: defecto
  de lanzamiento del proceso corregido en la versión 1.4.2; actualizá y
  reintentá la descarga.
- **La controladora no responde**: "Escanear" en el modal MIDI, y probá
  "Importar Mixxx" si el fabricante está en el catálogo.

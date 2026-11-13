# Manual de Usuario — DiscNativePro

Guía de uso de la aplicación. Para compilar el proyecto, ver [README](../README.md).

## Primeros pasos

Al abrir la app verás la consola completa: dos decks con jog wheels, la
biblioteca a la izquierda, la mesa de mezclas a la derecha y la columna
central con transport, hotcues, loops y stems.

### Audio

- En **Ajustes → Dispositivos** elegís la salida de audio (CoreAudio en macOS,
  WASAPI/Media Foundation en Windows, ALSA/JACK en Linux) y el buffer.
- Auriculares: cada canal tiene su botón de preescucha (PFL) y su volumen de
  auriculares en la mesa.

## Biblioteca

- **Importar**: botón de carpeta o arrastrar carpetas/archivos al árbol.
  Formatos soportados: WAV, MP3, FLAC, AIFF, M4A/AAC, OGG.
- El árbol lateral organiza por carpetas; la tabla muestra BPM, tonalidad y
  estado de análisis.
- **YouTube**: en la barra superior buscás videos; arrastrás un resultado a un
  deck y la app descarga el audio (m4a) mostrando el progreso. Los tracks
  guardados viven en la carpeta de Descargas configurada.
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

- 2 slots por deck (Echo, Gate, etc.). En Echo/Gate, el botón PARAM elige la
  **división de beat** (1/4 … 2/1) — los efectos quedan sincronizados al BPM
  del deck.
- EQ de 3 bandas + filtro por canal en la mesa.

## Sampler

- Panel de pads para disparar samples; arrastrás archivos para cargarlos.

## MIDI

- La app detecta controladoras USB automáticamente (botón "Escanear" en el
  modal MIDI).
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
- **Grabación**: botón REC de la barra superior; exporta a MP3.

## YouTube / Video

- Cada deck puede reproducir el video del clip de YouTube embebido (modo
  video) o solo el audio (modo audio) — configurable en Ajustes.
- En modo video el deck muestra el reproductor; el BPM/key no se analizan.

## Atajos y sugerencias

- Doble clic en un knob lo vuelve a su valor por defecto.
- Arrastrar un track desde la biblioteca a un deck lo carga; el deck izquierdo
  es Deck 1, el derecho Deck 2.
- El monitor de señales del modal MIDI muestra en vivo qué mensajes llega tu
  controladora — útil para diagnosticar.

## Solución de problemas

- **"requires macOS 14.0 o superior"**: la app usa un motor de IA embebido que
  exige macOS 14+.
- **La pista suena más rápido/lento de lo esperado**: corregido desde la
  versión 1.3.3 (archivos 44.1 kHz vs 48 kHz); si persiste, avisá con el
  formato del archivo y la placa de audio.
- **Descarga de YouTube falla**: revisá conexión; el error ahora se muestra en
  el deck. Un video ya en descarga no se vuelve a pedir (se reutiliza).
- **La controladora no responde**: "Escanear" en el modal MIDI, y probá
  "Importar Mixxx" si el fabricante está en el catálogo.

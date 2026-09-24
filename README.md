# DiscNativePro

**DiscNativePro** es una aplicación de DJ nativa para macOS de alto rendimiento, construida en **C++20** y **JUCE 8**. Diseñada con una arquitectura de audio de latencia ultrabaja, interfaz oscura profesional inspirada en el estándar de la industria (estilo Pioneer Rekordbox / djay Pro) e integración nativa de streaming de video de YouTube.

---

## Características Principales

- **Arquitectura de Doble Bandeja (Dual Deck)**:
  - Formas de onda duales de alto refresco: visualización en tiempo real desplazable (*scrolling waveform*) y vista general de pista (*overview waveform*).
  - Jog wheels interactivos con simulación de vinilo, scratch y pitch bend.
  - Detección precisa de BPM y beat grid en tiempo real.
  - Hot Cues, loops automáticos cuantizados y sincronización inteligente (Sync).

- **Motor de Audio de Grado Profesional**:
  - Ecualizador de 3 bandas (High, Mid, Low) con respuesta suave y curvas Kill.
  - Filtros dedicados bipolar (Low-Pass / High-Pass) por canal.
  - Faders de volumen de canal y crossfader central con curvas configurables (Smooth / Scratch).
  - Monitoreo independiente de auriculares (*Cue/Phones*) con balance Master/Cue.
  - Grabación en vivo sin pérdidas (*Lossless Audio Recorder*).

- **Integración de Video DJ con YouTube**:
  - Buscador público de alta velocidad (0.2 ms) y soporte opcional para YouTube API v3.
  - Carga directa de canciones y sets a las bandejas mediante drag & drop o botones dedicados.
  - Reproductores WebKit nativos embebidos con sincronización de audio, fader de volumen y mezcla de video con el crossfader central.

- **Biblioteca y Gestión de Archivos**:
  - Árbol jerárquico de carpetas con navegación de subdirectorios.
  - Soporte de metadatos, duración, BPM y tonalidad musical en notación Camelot.
  - Generador procedural de carátulas para pistas sin arte de tapa.

- **Soporte MIDI y Configuración**:
  - Detección y mapeo MIDI plug-and-play.
  - Selector de dispositivos de audio y configuración de frecuencias de muestreo / tamaño de búfer.

---

## Requisitos del Sistema

- **macOS**: 11.0 (Big Sur) o superior (optimizado para Apple Silicon M1/M2/M3/M4 e Intel).
- **Compilador**: Clang compatible con C++20 (Xcode Command Line Tools).
- **CMake**: Versión 3.22 o superior.
- **JUCE**: Versión 8 (gestionada automáticamente mediante `FetchContent`).

---

## Compilación y Ejecución

```bash
# 1. Clonar el repositorio
git clone https://github.com/axel-molina/DiscNativePro.git
cd DiscNativePro

# 2. Configurar el proyecto con CMake
cmake -B build

# 3. Compilar la aplicación
cmake --build build

# 4. Ejecutar DiscNativePro
open build/DiscNativePro_artefacts/Debug/DiscNativePro.app
```

---

## Licencia

Desarrollado para la suite **DiscPro**. Todos los derechos reservados.

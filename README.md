# DiscNativePro

**DiscNativePro** es una aplicación de DJ nativa de alto rendimiento para **macOS**, **Windows** y **Linux**, construida en **C++20** y **JUCE 8**. Diseñada con una arquitectura de audio de latencia ultrabaja, interfaz oscura profesional inspirada en el estándar de la industria (estilo Pioneer Rekordbox / djay Pro) e integración nativa de streaming y mezcla de video de YouTube.

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
  - Drivers de baja latencia nativos: **CoreAudio** en macOS y **WASAPI / ASIO** en Windows.

- **Integración de Video DJ con YouTube**:
  - Buscador público de alta velocidad (0.2 ms) con miniaturas panorámicas 16:9 y soporte para YouTube API v3.
  - Carga directa de canciones y sets a las bandejas mediante drag & drop o botones dedicados.
  - Reproductores nativos embebidos acelerados por hardware (**WebKit** en macOS y **Microsoft Edge WebView2** en Windows) con sincronización de audio, fader de volumen y mezcla de video con el crossfader central.

- **Biblioteca y Gestión de Archivos**:
  - Árbol jerárquico de carpetas con navegación de subdirectorios del sistema de archivos.
  - Soporte de metadatos, duración, BPM y tonalidad musical en notación Camelot.
  - Generador procedural de carátulas para pistas sin arte de tapa.

- **Soporte MIDI y Rendimiento**:
  - Detección y mapeo MIDI plug-and-play.
  - Medidor de uso de memoria RAM en tiempo real en la barra superior.
  - Selector de dispositivos de audio y configuración de frecuencias de muestreo / tamaño de búfer.

---

## Requisitos del Sistema

### macOS
- **Sistema Operativo**: macOS 11.0 (Big Sur) o superior (optimizado nativamente para Apple Silicon M1/M2/M3/M4 e Intel x86_64).
- **Compilador**: Clang con soporte C++20 (Xcode Command Line Tools).
- **CMake**: Versión 3.22 o superior.

### Windows
- **Sistema Operativo**: Windows 10 u 11 (64-bit).
- **Compilador**: Visual Studio 2022 (MSVC v143+ con carga de trabajo *"Desarrollo para el escritorio con C++"*).
- **CMake**: Versión 3.22 o superior.
- **WebView2**: Microsoft Edge WebView2 Runtime (incluido por defecto en Windows 10 y 11).

### Linux
- **Sistema Operativo**: Ubuntu 22.04 / 24.04, Debian 12, Fedora o similar (x64).
- **Compilador**: GCC 11+ o Clang 14+ con soporte C++20.
- **CMake**: Versión 3.22 o superior.
- **Librerías de Audio & Gráficos**: ALSA (`libasound2-dev`), X11, WebKitGTK (`libwebkit2gtk-4.1-dev`), OpenGL.
- **Reproducción de vídeo de YouTube**: WebKitGTK **y** los plugins de GStreamer (`gstreamer1.0-gl`, `gstreamer1.0-plugins-good`, `gstreamer1.0-plugins-bad`, `gstreamer1.0-libav`). Sin ellos el audio de YouTube puede sonar pero la imagen queda en blanco.

---

## Compilación y Ejecución en macOS

```bash
# 1. Clonar el repositorio
git clone https://github.com/axel-molina/DiscNativePro.git
cd DiscNativePro

# 2. Configurar con CMake en modo Release (optimizado para Apple Silicon)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 3. Compilar la aplicación
cmake --build build --config Release -j

# 4. Instalar en /Applications (opcional) o ejecutar directamente:
open build/DiscNativePro_artefacts/Release/DiscNativePro.app
```

> **Primer inicio en macOS (Binario descargado de la web o GitHub Releases)**:  
> Al ser un proyecto de código abierto sin licencia corporativa anual de Apple, macOS Gatekeeper puede solicitar confirmación al abrir por primera vez:
> 1. Haz clic derecho sobre `DiscNativePro.app` > **Abrir** > Confirmar **Abrir**.
> 2. O en **Ajustes del Sistema** > **Privacidad y seguridad** > pulsa **"Abrir de todos modos"**.
> 3. O desde Terminal: `xattr -cr /Applications/DiscNativePro.app`

---

## Compilación y Generación del `.exe` en Windows

Tienes dos alternativas para obtener el binario de Windows:

### Opción A: Descarga Automática desde GitHub Actions (Recomendada)
No necesitas tener una computadora con Windows ni compilar manualmente:
1. Dirígete a la pestaña **[Actions](https://github.com/axel-molina/DiscNativePro/actions)** de este repositorio.
2. Selecciona la ejecución más reciente del flujo **Build Windows Release (x64)** (o haz clic en *"Run workflow"* para disparar una compilación a demanda).
3. En la sección inferior **Artifacts**, descarga el archivo comprimido **`DiscNativePro-Windows-x64.zip`**.
4. Descomprímelo en cualquier PC con Windows y ejecuta directamente **`DiscNativePro.exe`**.

---

### Opción B: Compilación Manual en una PC con Windows

Si deseas compilar el código fuente directamente desde una máquina con Windows:

#### 1. Preparar las herramientas
Asegúrate de tener instalados:
- **Visual Studio 2022** (con la herramienta *MSVC C++ x64/x86 build tools* y *C++ CMake tools for Windows*).
- **NuGet CLI** (disponible instalándolo vía `winget install Microsoft.NuGet` o descargando `nuget.exe` al PATH).

#### 2. Clonar el repositorio
Abre una terminal de **PowerShell** o **Developer Command Prompt for VS 2022**:
```powershell
git clone https://github.com/axel-molina/DiscNativePro.git
cd DiscNativePro
```

#### 3. Descargar el SDK de WebView2
Instala el paquete oficial de Microsoft WebView2 para el reproductor de YouTube:
```powershell
nuget install Microsoft.Web.WebView2 -Version 1.0.2903.40 -OutputDirectory packages
```

#### 4. Configurar el proyecto con CMake
Genera los archivos de solución en modo Release vinculando el SDK de WebView2:
```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Release -DJUCE_WEBVIEW2_PACKAGE_LOCATION="$PWD/packages"
```

#### 5. Compilar el ejecutable
Compila con MSVC en modo Release utilizando todos los núcleos del procesador:
```powershell
cmake --build build --config Release --parallel
```

#### 6. Ejecutar
El binario ejecutable optimizado estará disponible en:
```powershell
.\build\DiscNativePro_artefacts\Release\DiscNativePro.exe
```

---

## Compilación y Ejecución en Linux

Tienes dos alternativas para obtener el binario ejecutable en Linux:

### Opción A: Descarga Automática desde GitHub Actions (Recomendada)
1. Dirígete a la pestaña **[Actions](https://github.com/axel-molina/DiscNativePro/actions)** de este repositorio.
2. Selecciona la ejecución más reciente del flujo **Build Linux Release (x64)** (o ejecútalo bajo demanda).
3. En la sección **Artifacts**, descarga el archivo comprimido **`DiscNativePro-Linux-x64.tar.gz`**.
4. Descomprímelo y ejecuta:
   ```bash
   tar -xzvf DiscNativePro-Linux-x64.tar.gz
   ./DiscNativePro
   ```

> **Dependencias de runtime del binario ya compilado.** El `.tar.gz` no las incluye:
> ```bash
> sudo apt-get install -y libwebkit2gtk-4.1-0 libgtk-3-0 \
>   gstreamer1.0-gl gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-libav
> ```
> `libwebkit2gtk-4.1-0` y `libgtk-3-0` son **obligatorias**: sin ellas la app arranca pero el reproductor de YouTube queda en blanco (y ahora muestra un aviso en su lugar). Los cuatro paquetes de GStreamer son los que decodifican y pintan el vídeo de YouTube; sin ellos suele pasar que **el audio suena y la imagen queda en blanco**.
>
> **Si el vídeo sale en blanco pero el audio suena**, comprueba primero los plugins:
> ```bash
> gst-inspect-1.0 vp9dec avdec_h264 glimagesink
> ```
> La app ya lanza WebKit en modo software (`WEBKIT_DISABLE_DMABUF_RENDERER=1` y `WEBKIT_DISABLE_COMPOSITING_MODE=1`) para evitar el fallo de compositing en NVIDIA/XWayland. Si tu GPU funciona bien y quieres aceleración, expórtalos a `0` antes de abrir la app:
> ```bash
> WEBKIT_DISABLE_DMABUF_RENDERER=0 WEBKIT_DISABLE_COMPOSITING_MODE=0 ./DiscNativePro
> ```

---

### Opción B: Compilación Manual en Linux (Ubuntu / Debian / Pop!_OS)

#### 1. Instalar dependencias del sistema
```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential cmake ninja-build pkg-config \
  libasound2-dev libjack-jackd2-dev libfreetype6-dev \
  libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \
  libxinerama-dev libxrandr-dev libxrender-dev \
  libglu1-mesa-dev mesa-common-dev \
  libwebkit2gtk-4.1-dev libgtk-3-dev libcurl4-openssl-dev \
  gstreamer1.0-gl gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-libav
```

#### 2. Clonar y configurar con CMake
```bash
git clone https://github.com/axel-molina/DiscNativePro.git
cd DiscNativePro
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release
```

#### 3. Compilar y ejecutar
```bash
cmake --build build --config Release --parallel
./build/DiscNativePro_artefacts/Release/DiscNativePro
```

---

## Licencia

Desarrollado para la suite **DiscPro**. Todos los derechos reservados.

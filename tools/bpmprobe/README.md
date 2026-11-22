# bpmprobe — medir el detector de BPM sin abrir la app

Sonda de línea de comandos que enlaza el `BeatDetector` **real** de la app con
JUCE y mide cualquier archivo de audio. Sirve para iterar la detección en
segundos en vez de recompilar la app, cargar una pista y escuchar.

## Uso

```
bpmprobe.exe "C:\ruta\pista.mp3"
```

Salida:

```
fichero: capaz (merengueton) (visualizer).mp3
  sr=44100  ch=2  dur=179.85 s
DETECTOR: 126.98 BPM   offset 0.3447 s
```

Con `-DDNP_BPM_DEBUG=1` el detector imprime además el número de picos y el
contraste on-beat/off-beat del tempo ganador y de su mitad/doble, que es lo que
sirve para ver por qué acierta o falla:

```
  [dbg] picos=92  rate=4009 Hz
  [dbg] mejor contraste: 126.98 BPM (0.033)
  [dbg]   63.49 BPM -> contraste -0.019
  [dbg]   126.98 BPM -> contraste 0.033
```

## Medición de referencia

| Pista | BPM real | Antes del arreglo | Después |
|---|---|---|---|
| capaz | 127 | 119,30 | 126,98 |
| Ya no me muero (Dj Konik Remix) | 155 | 119,52 | 154,23 |

## Compilar

### Opción A — CMake (recomendado)

Añadir al `CMakeLists.txt` del proyecto:

```cmake
juce_add_console_app(bpmprobe PRODUCT_NAME "bpmprobe")
target_sources(bpmprobe PRIVATE
    tools/bpmprobe/probe.cpp
    tools/bpmprobe/compilationtime.cpp
    src/audio/BeatDetector.cpp
)
target_include_directories(bpmprobe PRIVATE src)
target_link_libraries(bpmprobe PRIVATE
    juce::juce_audio_basics juce::juce_audio_formats juce::juce_core)
```

`compilationtime.cpp` existe porque `juce_core` referencia
`juce::juce_compilationDate` / `juce_compilationTime`, que normalmente genera
`juceaide`; aquí se definen a mano.

### Opción B — línea de comandos con MSVC

```bat
cl /nologo /std:c++20 /EHsc /O2 /MT /DNDEBUG ^
   /DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 /DJUCE_STANDALONE_APPLICATION=1 ^
   /DJUCE_MODULE_AVAILABLE_juce_core=1 /DJUCE_MODULE_AVAILABLE_juce_audio_basics=1 ^
   /DJUCE_MODULE_AVAILABLE_juce_audio_formats=1 /DJUCE_USE_CURL=0 ^
   /I juce\modules /I src ^
   tools\bpmprobe\probe.cpp tools\bpmprobe\compilationtime.cpp ^
   juce\modules\juce_core\juce_core.cpp ^
   juce\modules\juce_audio_basics\juce_audio_basics.cpp ^
   juce\modules\juce_audio_formats\juce_audio_formats.cpp ^
   src\audio\BeatDetector.cpp ^
   /Fe:bpmprobe.exe /link winmm.lib ole32.lib shell32.lib user32.lib
```

Con GCC/MinGW el `juceaide` de JUCE 8.0.6 no compila
(`CaretPosition_BeginningOfLine was not declared`), así que esta sonda se
compila con MSVC.

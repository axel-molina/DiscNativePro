# Cambios en la rama `fix/audio-p0`

Empecé usando la app, no leyendo el código. Instalé una pista y se cerró sola tres veces de cada cinco. A partir de ahí fui tirando del hilo y salieron más cosas. Esto es lo que hay y por qué.

Son 21 commits sobre `main` (`4f5886f`), 27 archivos.

## El crash de arranque

Lo primero era que la app se cerraba sola al abrir. Tres de cada cinco intentos. Con el visor de eventos saqué la dirección y con el mapa de símbolos di con la función: `juce::dsp::IIR::Coefficients<float>::getFilterOrder()`. El mixer podía procesar un bloque antes de que `prepare()` hubiera configurado los filtros, y ahí había un puntero nulo.

Puse una bandera `prepared` en `DjMixer`: hasta que no se llama a `prepare()`, `process()` devuelve silencio. Cinco arranques seguidos sin fallo. Ese es el arreglo que más me importaba dejar.

## El loop se iba de mezcla

Lo probé y efectivamente se iba. La causa era que el render se pasaba del final hasta un bloque entero y el cursor quedaba en `start + exceso`, así que el inicio del loop rotaba un poco en cada vuelta: unos 2,33 ms por pasada, que en 60 vueltas son 140 ms. Se nota.

Lo cambié para trocear el render justo en el borde del loop. Pero me equivoqué dos veces más antes de dar con ello:

1. La primera versión seguía derivando porque medía cuánto faltaba para el final del loop con la frecuencia del **dispositivo** en vez de la del **archivo**. Un MP3 a 44,1 kHz en una tarjeta a 48 kHz daba un 8,8 % de error en cada vuelta.
2. Y antes de eso, el bloque donde se reajusta el loop incluye un seek, así que la medida del avance de ese bloque salía casi cero y el tramo se calculaba fatal.

Al final lo medí en tu máquina con una sonda que apuntaba el error de cada vuelta: 103 vueltas, 0,4 muestras de error constante, o sea 0,009 ms por vuelta. Inaudible.

También hice que el inicio del loop se cuantice a la rejilla cuando el desvío es menor de un cuarto de beat, para no tener que adelantarlo a mano cada vez.

## El BPM

Este me llevó más de lo que esperaba. El detector devolvía unos 119 BPM para todo. Lo saqué a un ejecutable aparte para medirlo sin abrir la app, y con dos pistas de BPM conocido:

| Pista | BPM real | Antes | Ahora |
|---|---|---|---|
| capaz | 127 | 119,30 | 126,98 |
| Ya no me muero (Dj Konik Remix) | 155 | 119,52 | 154,23 |

Dos tempos muy distintos dando casi el mismo número. Metí trazas y vi el problema: el detector de picos se deja la mayoría de los golpes. En la de 155 encontraba 109 picos en 237 segundos, uno cada cinco beats. El histograma de intervalos se llenaba de huecos de dos a veinticuatro segundos y siempre ganaba el mismo valor.

Lo cambié por medir, para cada tempo candidato, cuánta energía cae en el pulso menos cuánta cae entre pulsos. El tempo bueno es el que da más contraste, y de paso se descarta el doble y la mitad (a 63,49 el contraste era −0,019 y a 126,98 era 0,033).

Y había un segundo problema, distinto: el BPM que veías no se guardaba. Había dos análisis, el de la biblioteca, que sí guardaba, y el del deck al cargar la pista, que no. Por eso en `library_cache.json` la pista capaz estaba con `"bpm": 0.0` mientras en pantalla salía otro número, y al reabrirla volvía el viejo. Ahora el deck guarda lo que detecta una vez por carga.

Lo que no he podido comprobar es cómo suena. Las cifras están medidas, pero si alguna pista sigue cayendo mal, dímelo con su BPM real y lo ajusto.

## El resto del audio

- **Sampler**: `setResamplingRatio()` recibe muestras de entrada por muestra de salida, y se le pasaba el inverso. Un sample a 48 kHz en un dispositivo a 44,1 sonaba 2,92 semitonos abajo y perdía el 15 %. Ahora da 1000,4 Hz exactos.
- **Gate**: era un cuadrado duro, `sin > 0 ? 1 : 0`, aunque el comentario decía «soft cosine transitions». Clicaba en cada flanco. Ahora el salto es de 0,0066 en vez de 0,475.
- **Ganancias**: no había ninguna suavizada. Bajar el master de 1,0 a 0,5 era un escalón de medio valor en una muestra. Ahora hay rampa de 20 ms en trim, fader, master y auriculares, y el crossfader se interpola por muestra.
- **Denormales**: no había `ScopedNoDenormals` en ningún sitio. Añadido en el callback, en el canal y en el limiter.
- **Lock del hilo de audio**: `loadFile()` cogía un `ScopedLock` bloqueante mientras el callback también lo pedía, y ahí dentro hay lectura de disco y cerca de un mega de reserva. Cambiado a `ScopedTryLock`: si está ocupado, un bloque de silencio y a seguir.
- **`setLoop`**: aceptaba longitud cero o negativa, y entonces la comprobación de fin siempre era cierta y el deck re-seekaba en cada bloque. Ahora lo rechaza.

## Servicios

- **`LibraryCacheManager::clear()`**: cogía el mutex y llamaba a `saveCache()`, que lo vuelve a coger. Deadlock, y la interfaz se congelaba. Saqué la llamada fuera del lock.
- **Playlists**: si `playlists.json` estaba corrupto se leía como lista vacía y la primera edición lo sobrescribía. Ahora se guarda una copia con fecha antes de seguir.
- **yt-dlp (issue #4)**: descargaba siempre el binario de macOS y resolvía con rutas de macOS y `which`. Como el archivo ya existía, el fallo era permanente aunque lo reinstalaras. Ahora elige el asset según la plataforma, usa `where` en Windows, arregla la ruta que duplicaba «Application Support» y comprueba con `--version` que el binario arranca.

## Interfaz

- **Pantalla completa**: el botón y F11 hacían cosas distintas. El botón usaba `DocumentWindow::setFullScreen()`, que deja la ventana dentro del área de trabajo y por eso seguía viéndose la barra de Windows. Ahora F11, ESC, el botón y un `--fullscreen` al arrancar pasan todos por el modo kiosco, con un pequeño debounce para que machacar la tecla no encadene entradas y salidas.
- **Ventana**: arrancaba limitada a 1360×860 y dejaba franjas negras. Ahora ocupa el área útil y, si el diseño de 1800 px lógicos no cabe, se aplica una escala global (0,853 en mi pantalla). Con eso la bandeja 2 entra completa.
- **Foco de teclado**: `MainComponent` pedía foco pero nunca lo tomaba, así que `keyPressed()` no recibía nada. Ni F11 ni los atajos q/w/p/o/x/c/v. Ahora lo toma al mostrarse.
- **Grid editable**: BPM a doble clic (40–250), offset de ±5 y ±20 ms, ×½, ×2 y ±0,05 / ±0,5, todo guardado. Y la audición arranca siempre desde el primer golpe; antes, si el deck ya venía sonando, seguía desde donde estuviera.
- **Versión**: 1.4.4. El bump por `project(VERSION)` no hacía nada porque `APP_VERSION` sale de `git describe` y la expresión regular lo recortaba.

## Cómo lo he comprobado

Monté un banco fuera del repositorio que compila los `.cpp` reales contra JUCE 8.0.6 y los ejecuta, para tener cifras en vez de impresiones. Siete pruebas con su antes y después.

Para el BPM y el loop hice dos sondas de línea de comandos; la del BPM la he dejado en `tools/bpmprobe/` con su README, por si te sirve para reproducir las mediciones. La del loop la quité al terminar.

El banco de audio de JUCE confirmó que en mi máquina el dispositivo abre a 48 kHz, bloque de 480 y dos canales.

## Lo que no he hecho, a propósito

- **`UpdateService`**: hoy ejecuta el instalador que descarga con `getSize() > 1000` como única comprobación, y `stagedDir` sale de `settings.json` y se interpola en un script. Eso es ejecución de código si alguien toca la URL o el JSON. Necesita lista blanca de la URL, verificación de firma y comprobar que la carpeta está dentro de los datos de la app. Mejor en otra rama.
- **`settings.json` con lock**: lo reescriben cuatro clases con leer-modificar-escribir. Pide un almacén único y es refactor con riesgo de perder ajustes.
- **Stems**: el pico es de unos 1,8 GB por pista de 15 minutos y `shutdown()` libera la sesión de ONNX con el hilo de inferencia posiblemente dentro de `Run()`. Necesita trocear el proceso, y además los modelos no están en el repo.
- **Licencias**: `LICENSE` es GPL-3.0 y el README dice «Todos los derechos reservados». Ningún instalador lleva la licencia y LAME va enlazado estático, que es LGPL. Eso lo tienes que decidir tú.
- **Firma y notarización**: macOS ad-hoc sin notarizar, Windows sin Authenticode.

## Dos cosas que probé y quité

- **Modo día invirtiendo colores**: en JUCE 8 `ImageEffectFilter` solo lo aplica `ShapeButton`; el camino de pintado normal ya no lo ejecuta. El filtro se instalaba y no hacía nada. Para un modo claro de verdad hay que centralizar la paleta.
- **Un botón ALINEAR en el grid**: cerraba la app por corrupción de heap al pulsarlo. Lo quité. El grid se sigue pudiendo ajustar a mano.

Una nota sobre esto último: las dos veces que rompí algo fue por no poder probarlo yo. No puedo pulsar botones ni oír el resultado desde aquí, así que lo que no puedo verificar no debería haber entrado. El resto de lo que está en la rama sí está medido.

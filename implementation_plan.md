# Plan de implementación — P0 verificables (2026-11-22)

## Objetivo y contexto

Arreglar los defectos de audio del informe de auditoría (`discnativepro-auditoria.md`, sección 2)
que **se pueden verificar con el banco offline** sin poder compilar la app completa en esta
máquina (JUCE 8 no soporta MinGW; MSVC exige administrador).

Criterio de alcance: entran los cambios de bajo riesgo con evidencia medida de antes/después.
**Queda fuera** el loop sample-accurate y el detector de BPM: son reescrituras del camino de
render y del análisis que exigen escucha real en una bandeja, y aquí no hay audio ni deck.
Meterlos a ciegas sería peor que dejarlos documentados.

## Cambios

### [MODIFY] `src/audio/SamplerBank.cpp` — ratio de resample invertido
- `ratio = reader->sampleRate / deviceSampleRate` (entrada por salida, como `DeckPlayer::updateRateFactor`).
- `outLen = ceil(srcLen / ratio) + 64` para no truncar el sample.
- Motivo: JUCE `setResamplingRatio` es *samplesInPerOutputSample*. Medido antes: −2,92 st, −15,5 % de velocidad, 84,6 % del material.

### [MODIFY] `src/audio/DjMixer.h` / `DjMixer.cpp` — suavizado de ganancias
- `ChannelStrip`: `juce::SmoothedValue` para trim y fader de canal, aplicados con `applyGainRamp`.
- `DjMixer`: interpolación por muestra de las ganancias del crossfader entre el bloque anterior y el actual (`lastGain1/lastGain2`), 1 bloque de rampa.
- `masterVolume` y `phonesVolume` con `SmoothedValue` (20 ms).
- Motivo: medido, master 1,0 → 0,5 produce un salto de 0,5 en **una** muestra.

### [MODIFY] `src/audio/DeckFxChain.h` / `DeckFxChain.cpp` — flancos del Gate
- `FxSlot`: nuevos miembros `gatePhasePos` y `gateEnv`, reseteados en `prepare`/`reset`.
- El pulso duro se sustituye por una exponencial de ~1,5 ms hacia el objetivo (0,05 / 1,00).
- Motivo: medido, 8 discontinuidades de 0,475 en 2 s.

### [MODIFY] `src/audio/AudioEngine.cpp`, `DjMixer.cpp`, `MasterLimiter.cpp` — denormales
- `juce::ScopedNoDenormals` en el callback de audio y en `ChannelStrip::process` / `MasterLimiter::process`.
- Motivo: IIR del EQ sin `snapToZero` en pasajes silenciosos.

### [MODIFY] `src/audio/DeckPlayer.cpp` — no bloquear el hilo de audio
- `getNextAudioBlock`: `ScopedTryLock`; si no se obtiene, silencio y salida.
- Efecto aceptado y documentado: durante una carga hay un bloque (o dos) de silencio en vez de
  un *dropout* por inversión de prioridad.
- `setLoop`: ignorar longitudes no positivas (evita el re-seek continuo cuando `length == 0`).

## Verificación

1. Recompilar el banco (`bench/`) con los fuentes modificados y ejecutar:
   - sampler → la frecuencia medida debe pasar de 844,6 Hz a 1000 Hz (±1) y el material al 100 %;
   - master 1,0 → 0,5 → el salto de una muestra debe desaparecer (rampa);
   - gate → 0 discontinuidades y rango 0,025..0,5 (sigue modulando).
2. Compilar sin errores (`-c`) `DeckPlayer.cpp`, `AudioEngine.cpp`, `DjMixer.cpp`,
   `DeckFxChain.cpp`, `MasterLimiter.cpp` y `SamplerBank.cpp` con los mismos flags del banco.
3. Revisar que el diff no toque nada fuera de lo listado.

## Lo que NO se puede verificar aquí (y por qué)

- Compilación de la app completa y `cmake --build`: sin MSVC/administrador. La regla de
  relanzar la app de `AGENTS.md` es de macOS y no aplica en esta máquina.
- Escucha real y test de deck: no hay dispositivo de audio ni GUI.
- Loop y BPM: se quedan como propuesta en el informe; su arreglo exige escucha.

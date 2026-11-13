#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

class StretchAdapter; // PIMPL: signalsmith-stretch (pesado) no contamina los TUs que incluyen esto
class TwoStemEngine;  // P25: motor 2-stem (declaración adelantada, PIMPL igual)

class DeckPlayer : public juce::AudioSource
{
public:
    DeckPlayer(juce::AudioFormatManager& formatManager, juce::TimeSliceThread& thread);
    ~DeckPlayer() override;

    // Load track from file path using disk streaming
    bool loadFile(const juce::File& audioFile);

    // AudioSource interface
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;

    // Transport controls
    void play();
    void pause();
    void stop();
    void triggerCue();
    void setCuePosition();

    bool isPlaying() const { return playing.load(); }
    bool isLoaded() const  { return loaded.load(); }

    // Navigation & position
    // userInitiated = true cuando el salto lo pide una persona (onda, MIDI,
    // jog). El controlador de fase usa false: sus seeks no deben desarmarlo.
    void setPosition(double seconds, bool userInitiated = true);
    juce::uint32 getUserSeekSerial() const { return userSeekSerial.load(); }
    double getPosition() const;
    double getLengthInSeconds() const { return trackDurationSeconds.load(); }

    // Beat phase (P3)
    double getFirstBeatOffset() const { return firstBeatOffsetSeconds.load(); }
    double getBeatPhase() const;

    // Tempo / Pitch
    void setPitchPercent(float percent); // -8.0 to +8.0, etc.
    float getPitchPercent() const { return pitchPercent.load(); }
    bool isPitchClamped() const { return pitchClamped.load(); }
    void setPitchBend(float bend); // Temporary nudge (e.g. +/- 0.05)
    float getPitchBend() const { return pitchBend.load(); }
    void setPitchRange(float range) { pitchRange.store(juce::jmax(1.0f, range)); }
    float getPitchRange() const     { return pitchRange.load(); }

    // Jog wheel behavior: 0 = Vinyl (center scratch + edge bend), 1 = CDJ (bend all over)
    void setJogMode(int mode) { jogMode.store(mode == 1 ? 1 : 0); }
    int getJogMode() const    { return jogMode.load(); }

    // Key Lock / Master Tempo: pitch cambia tempo sin desafinar (default ON)
    void setKeylock(bool on) { keylock.store(on); updatePlaybackSpeed(); }
    bool isKeylock() const   { return keylock.load(); }

    // P20: Key Shift — transpone ±semitonos sin cambiar el tempo (mezcla armónica).
    // Con Key Lock ON lo aplica el stretcher; con OFF va por resample (pitch clásico).
    void setKeyShiftSemitones(int st) { keyShiftSemitones.store(juce::jlimit(-6, 6, st)); updatePlaybackSpeed(); }
    int getKeyShiftSemitones() const  { return keyShiftSemitones.load(); }

    // P22: stems (vocals/drums/bass/other precálculados offline con Demucs).
    // Los buffers en RAM siguen el cursor del master: heredan tempo, keylock,
    // loops y seeks sin duplicar el pipeline.
    bool isStemsReady() const  { return stemsReady.load(); }
    bool isStemsActive() const { return stemsActive.load(); }
    bool isStemsLoaded() const { return stemsInRam.load(); } // buffers en RAM listos
    void setStemsActive(bool on) { if (stemsReady.load()) stemsActive.store(on); }
    // P23f: si la pista cargada aún no tiene stems pero ya están separados en
    // disco (la separación terminó DESPUÉS de la carga), los adjunta al vuelo.
    // Devuelve true si los adjuntó. Liviano: 4 chequeos de existencia de archivo.
    bool tryAttachStemsIfReady();
    float getStemGain(int index) const
    {
        return (index >= 0 && index < 4) ? stemGain[(size_t)index].load() : 0.0f;
    }
    void setStemGain(int index, float gain)
    {
        if (index >= 0 && index < 4)
            stemGain[(size_t)index].store(juce::jlimit(0.0f, 1.0f, gain));
    }
    static const char* stemName(int index)
    {
        static const char* names[] = { "vocals", "drums", "bass", "other" };
        return (index >= 0 && index < 4) ? names[index] : "";
    }

    // P25: modo 2-stem en streaming (TwoStemEngine corre adelantado al cursor
    // en un hilo propio del deck). Gains: stemGain[0] = VOC, stemGain[1] = INST.
    bool isStems2Engaged() const { return stems2Engaged.load(); }
    bool isStems2Active() const  { return stems2Active.load(); }
    void setStems2Active(bool on) { if (stems2Engaged.load()) stems2Active.store(on); }
    bool isStems2Covered() const { return stems2Covered.load(); }
    bool isStems2Cached() const  { return stems2Cached.load(); }
    // Master de stems con conocimiento de modo: 4-stem (P22) si está listo,
    // 2-stem en caso contrario. UI y MIDI lo comparten.
    bool isStemsMasterAvailable() const { return isStemsReady() || isStems2Engaged(); }
    bool isStemsMasterActive() const
    {
        return isStemsReady() ? isStemsActive() : isStems2Active();
    }
    void toggleStemsMaster()
    {
        if (isStemsReady()) setStemsActive(!isStemsActive());
        else setStems2Active(!isStems2Active());
    }

    // P25 Fase D: modo de stems para pistas nuevas.
    // false = Rápido (2-stem en streaming, default) / true = Completo (4-stem precompute).
    // Persistido en settings.json ("stemsMode") como "fast"/"full".
    void setStemsPrecompute4(bool on) { stemsPrecompute4.store(on); }
    bool isStemsPrecompute4() const   { return stemsPrecompute4.load(); }
    static bool loadStemsModeDefault();
    static void saveStemsModeDefault(bool precompute4);

    // Scratch v2: la mano es dueña de la aguja. UI publica velocidad (rev/s
    // con signo); el hilo de audio integra la posición por bloque.
    void beginScratch();
    void endScratch();
    void setScratchVelocity(float revsPerSecond) { scratchVelRevPerSec.store(revsPerSecond); }
    bool isScratching() const { return scratchTouch.load(); }

    // Hot Cues (0 to 3 for 4 hot cues)
    void setHotCue(int index, double positionSeconds);
    void jumpToHotCue(int index);
    void clearHotCue(int index);
    bool hasHotCue(int index) const;
    double getHotCue(int index) const;

    // Loop
    void setLoop(double startSeconds, double lengthSeconds, bool enabled);
    void exitLoop();
    bool isLooping() const { return loopActive.load(); }
    double getLoopStart() const { return loopStartSeconds.load(); }
    double getLoopLength() const { return loopLengthSeconds.load(); }

    // Performance: beat jump, loop roll momentaneo y slip mode.
    // Matematica pura (testeable sin audio): saltos/anclas cuantizadas al grid.
    static double quantizedBeatPos(double pos, double firstBeat, double interval, double beats);
    static double rollStartFor(double pos, double firstBeat, double interval, double beats);

    void jumpBeats(double beats);        // salto cuantizado al beatgrid
    void startLoopRoll(double beats);    // loop momentaneo anclado a subdivision
    void releaseLoopRoll();              // al soltar: fin del loop (o posicion virtual con slip)

    // Slip: con loop/scratch activos el track sigue avanzando en una posicion
    // virtual; al soltar, retoma donde habria estado.
    void setSlipEnabled(bool on) { slipEnabled.store(on); }
    bool isSlipEnabled() const   { return slipEnabled.load(); }

    // BPM & Beatgrid
    double getBpm() const         { return currentBpm.load(); }
    double getOriginalBpm() const { return originalBpm.load(); }
    void seekRelative(double deltaSeconds, bool userInitiated = true);

    // P16: editor de beat grid. El override gana al publish (async) del
    // análisis; los valores detectados se conservan para resetGrid().
    void setGridBpm(double bpm);          // clamp 40..250, recalcula currentBpm con el pitch
    void setFirstBeatOffset(double sec);  // clamp [0, duración]
    void resetGrid();                     // vuelve a los valores detectados
    bool hasGridOverride() const { return gridBpmOverride.load() > 0.0 || gridOffsetOverride.load() >= 0.0; }
    double getDetectedBpm() const    { return detectedBpm.load(); }
    double getDetectedOffset() const { return detectedOffsetSeconds.load(); }

    // Waveform peak data (3 frequency bands)
    const std::vector<float>& getPeaksLow() const  { return peaksLow; }
    const std::vector<float>& getPeaksMid() const  { return peaksMid; }
    const std::vector<float>& getPeaksHigh() const { return peaksHigh; }
    bool hasWaveformData() const { return !peaksLow.empty(); }

    juce::String getTrackTitle() const;
    juce::String getTrackArtist() const;
    juce::String getTrackKey() const;
    juce::String getTrackCamelot() const;
    juce::Colour getTrackKeyColour() const;
    const juce::File& getCurrentFile() const { return currentFile; }

private:
    void updatePlaybackSpeed();
    void updateRateFactor();
    double beatInterval() const;

    juce::AudioFormatManager& formatManager;
    juce::TimeSliceThread& diskThread;

    juce::CriticalSection lock;

    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    std::unique_ptr<juce::BufferingAudioSource> bufferingSource;
    std::unique_ptr<juce::ResamplingAudioSource> resamplingSource;
    std::unique_ptr<StretchAdapter> stretcher; // Key Lock (solo se usa con keylock ON)
    int numSrcChannels { 2 };
    int expectedSamplesPerBlock { 512 };

    juce::File currentFile;
    juce::String trackTitle;
    juce::String trackArtist;
    juce::String trackKey { "--" };
    juce::String trackCamelot { "--" };

    std::atomic<bool> loaded { false };
    std::atomic<bool> playing { false };
    std::atomic<double> trackDurationSeconds { 0.0 };
    std::atomic<double> cuePositionSeconds { 0.0 };
    std::atomic<double> originalBpm { 0.0 };
    std::atomic<double> currentBpm { 0.0 };
    std::atomic<double> firstBeatOffsetSeconds { 0.0 };

    // P16: grid corregido a mano. detected* guarda lo que publicó el análisis
    // (para RESET); los overrides son lo que manda mientras estén activos.
    std::atomic<double> gridBpmOverride { 0.0 };      // 0 = sin override
    std::atomic<double> gridOffsetOverride { -1.0 };  // <0 = sin override
    std::atomic<double> detectedBpm { 0.0 };
    std::atomic<double> detectedOffsetSeconds { 0.0 };

    // Seek seguro (sin lock, sin reset en el llamante)
    std::atomic<bool> seekPending { false };
    std::atomic<double> seekTargetSec { 0.0 };
    std::atomic<juce::int64> playbackPosSamples { 0 };
    // Contador monótono de seeks pedidos por el usuario (nunca por el
    // controlador de fase). AudioEngine lo compara para desarmar la alineación.
    std::atomic<juce::uint32> userSeekSerial { 0 };

    void applyPendingSeek(double targetSec);

    std::atomic<float> pitchPercent { 0.0f };
    std::atomic<float> pitchBend { 0.0f };
    std::atomic<float> pitchRange { 8.0f };
    std::atomic<bool> pitchClamped { false };
    std::atomic<int> jogMode { 0 };
    std::atomic<bool> keylock { true };
    std::atomic<int> keyShiftSemitones { 0 }; // P20: ±6 semitonos

    // Scratch state (atómicos UI<>audio; anclas solo hilo de audio)
    std::atomic<bool> scratchTouch { false };
    std::atomic<float> scratchVelRevPerSec { 0.0f };
    double scratchAnchorSec { 0.0 };
    int scratchDir { 0 }; // +1 adelante, -1 atrás, 0 quieto (solo hilo de audio)
    bool scratchAnchorInit { false };

    // Scratch v3b: anillo de historial (lo último escuchado, device-rate).
    // La reversa lee de aquí: determinista, sin disco ni hilo de fondo.
    static constexpr int scratchRingSize = 768000; // ~8 s estéreo a 48 kHz (~3 MB)
    static constexpr int scratchTailSize = 128;
    static constexpr int scratchXfade = 64;
    juce::AudioBuffer<float> scratchRing;
    juce::AudioBuffer<float> scratchTail;
    int ringWritePos { 0 }; // solo hilo de audio
    int ringValid { 0 };    // samples válidos (solo hilo de audio)
    double revHead { 0.0 }; // cabeza fraccional en samples de anillo (solo hilo de audio)

    void writeScratchRing(const juce::AudioSourceChannelInfo& info);
    float readScratchRing(int ch, double pos) const;

    std::atomic<bool> loopActive { false };
    std::atomic<double> loopStartSeconds { 0.0 };
    std::atomic<double> loopLengthSeconds { 0.0 };

    // Slip mode (P15): atómicos UI<>audio; el resto es solo hilo de audio.
    std::atomic<bool> slipEnabled { false };
    std::atomic<double> slipVirtualSec { 0.0 };
    bool slipActive { false };           // slip enganchado (hilo de audio)
    juce::int64 prevReadPos { -1 };      // para medir el avance real por bloque

    std::array<std::atomic<double>, 4> hotCues;
    std::array<std::atomic<bool>, 4> hotCuesActive;

    // Los peaks solo se escriben en el hilo de mensajes (los pintores son de
    // mensajes); el worker de análisis nunca toca estos vectores.
    std::vector<float> peaksLow;
    std::vector<float> peaksMid;
    std::vector<float> peaksHigh;

    // P5: análisis en segundo plano. El worker usa su propio reader (un
    // AudioFormatReader no es seguro para lectura concurrente) y publica los
    // resultados en el hilo de mensajes.
    struct AnalysisShared
    {
        std::atomic<bool> alive { true };  // el destructor lo pone a false
        std::atomic<int> generation { 0 }; // ++ en cada loadFile: descarta trabajos viejos
    };

    std::shared_ptr<AnalysisShared> analysis;

    // P22: stems en RAM. Los buffers solo se publican en el hilo de mensajes
    // (antes de stemsInRam) y el hilo de audio no los lee hasta la bandera.
    std::atomic<bool> stemsReady { false };   // hay 4 WAVs válidos para esta pista
    std::atomic<bool> stemsActive { false };  // modo stems: original muteado
    std::atomic<bool> stemsInRam { false };
    std::array<std::atomic<float>, 4> stemGain;
    juce::AudioBuffer<float> stemBuffers[4];
    double stemSampleRate[4] { 0.0, 0.0, 0.0, 0.0 };
    double origFileSampleRate { 44100.0 };

    // P26: factor de conversión file→device (origFileSampleRate/currentSampleRate).
    // Muestras de archivo que hay que consumir por muestra de dispositivo para
    // reproducir a velocidad real. Atómico: lo escriben loadFile/prepareToPlay
    // (bajo lock) y lo leen updatePlaybackSpeed y el callback.
    std::atomic<double> rateFactor { 1.0 };

    void startStemsLoad(const juce::String& dir, int generation);
    static float readStemInterp(const float* data, int length, double pos);

    // P25: streaming 2-stem. El anillo (voc/ins a 44.1 kHz) lo escribe el
    // worker de separación y lo lee el callback de audio; la zona cubierta es
    // [max(origin, writeEnd - anillo), writeEnd) en muestras absolutas 44.1k.
    // Coordinación lock-free: el worker publica writeEnd DESPUÉS de escribir
    // los samples (release), el callback lee solo lo cubierto.
    static constexpr int stems2RingSamples = 44100 * 20; // ~20 s (7 MB por deck)
    std::atomic<bool> stems2Engaged { false };
    std::atomic<bool> stems2Active { false };
    std::atomic<bool> stems2Covered { false };
    std::atomic<juce::int64> s2Origin44 { 0 };   // muestra 44.1k del slot 0
    std::atomic<juce::int64> s2WriteEnd44 { 0 }; // primera muestra NO cubierta
    std::atomic<juce::int64> s2SeekGen { 0 };
    std::atomic<juce::int64> s2SeekTarget44 { 0 };
    juce::AudioBuffer<float> stems2Ring[2]; // [voc, ins], 2 canales
    // P25 Fase D: caché 2-stem completa en RAM (segunda carga instantánea).
    // Escritos en el hilo de mensajes y publicados con stems2Cached; el
    // callback no los lee hasta ver la bandera.
    std::atomic<bool> stems2Cached { false };
    juce::AudioBuffer<float> stems2CacheBuf[2]; // [voc, ins] a 44.1 kHz
    juce::int64 stems2CacheLen44 { 0 };         // muestras 44.1k disponibles
    // Escritura de la caché (solo el worker del pool; declarados antes del
    // pool para que en la destrucción inversa el job termine primero).
    std::unique_ptr<juce::AudioFormatWriter> s2CacheWV, s2CacheWI;
    std::atomic<bool> stems2CacheDone { false };
    // El motor se declara ANTES del pool: en la destrucción inversa el pool
    // (y su job) terminan antes de destruir el engine.
    std::unique_ptr<TwoStemEngine> stems2Engine;
    juce::ThreadPool stems2Pool { 1 };
    std::atomic<bool> stems2Quit { false }; // ~DeckPlayer lo enciende: los jobs-lambda no ven shouldExit()

    void startStems2Stream(std::shared_ptr<juce::AudioFormatReader> reader,
                           const juce::File& audioFile, juce::int64 startOrigin44,
                           int generation);
    bool loadStems2Cache(const juce::File& cacheDir);
    static float readStems2Ring(const juce::AudioBuffer<float>& ring, int ch, double pos44,
                                juce::int64 origin44);

    // P25 Fase D: modo de stems para pistas nuevas (Rápido=2-stem streaming).
    std::atomic<bool> stemsPrecompute4 { false };

    void startAnalysis(std::shared_ptr<juce::AudioFormatReader> analysisReader, int generation);
    static void computeQuickOverview(juce::AudioFormatReader* reader, size_t numPoints,
                                     std::vector<float>& low, std::vector<float>& mid,
                                     std::vector<float>& high);
    static void computeFullOverview(juce::AudioFormatReader* reader, size_t numPoints,
                                    std::vector<float>& low, std::vector<float>& mid,
                                    std::vector<float>& high);
    static void publishIfCurrent(const std::shared_ptr<AnalysisShared>& shared, int generation,
                                 std::function<void()> publishFn);

    double currentSampleRate { 44100.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeckPlayer)
};

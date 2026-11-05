#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <atomic>
#include <cstdio>
#include <vector>

// BroadcastService: emite la mezcla master en vivo. Dos modos:
//  - Icecast: codifica a MP3 (LAME) y envia por TCP al servidor propio.
//  - Rtmp: escribe PCM al stdin de ffmpeg, que codifica AAC y empuja por
//    RTMP/RTMPS (YouTube, Facebook, Twitch, servidor personalizado).
// Misma arquitectura que AudioRecorder: AbstractFifo lock-free + hilo aparte
// + atómicos. El callback de audio solo copia al FIFO (real-time safe).
class BroadcastService : public juce::Thread
{
public:
    enum class Mode { Icecast, Rtmp };

    struct Config
    {
        Mode mode { Mode::Icecast };
        int bitrateKbps { 192 };

        // Icecast
        juce::String host;
        int port { 8000 };
        juce::String mount { "/stream" };
        juce::String password;
        juce::String stationName;
        bool makePublic { false };

        // Rtmp
        juce::String ingestUrl; // URL completa, con el stream key incluido
    };

    BroadcastService();
    ~BroadcastService() override;

    bool startBroadcast(const Config& newConfig, double sampleRate, int numChannels);
    void stopBroadcast();

    bool isBroadcasting() const { return broadcasting.load(std::memory_order_relaxed); }
    bool isConnected() const { return connected.load(std::memory_order_relaxed); }
    int64_t getBytesSent() const { return bytesSent.load(std::memory_order_relaxed); }
    juce::String getLastError() const;

    // Real-time safe: pushes audio into lock-free ring buffer (no memory allocation or I/O)
    void pushAudioBlock(const juce::AudioBuffer<float>& buffer, int numSamples);

    void run() override;

private:
    // Icecast
    bool connectToServer();
    void sendRemainingEncoderOutput();
    bool sendBytes(const char* data, int numBytes);

    // Rtmp (ffmpeg por pipe)
    bool launchFfmpeg();
    void closeFfmpeg();
    bool writePcmToFfmpeg(const short* interleaved, int numFrames);

    void closeConnection();
    void setError(const juce::String& message);

    static constexpr int fifoSize = 65536; // ~1.5 sec audio buffer at 44.1kHz
    juce::AbstractFifo fifo { fifoSize };
    juce::AudioBuffer<float> fifoBuffer;

    Config config;
    int streamChannels { 2 };
    double streamSampleRate { 44100.0 };

    // Codificador LAME (solo modo Icecast): se crea/destruye fuera del hilo de red.
    void* lameFlags { nullptr };

    // Salidas
    std::unique_ptr<juce::StreamingSocket> socket; // Icecast
    std::FILE* ffmpegPipe { nullptr };              // Rtmp
    juce::File ffmpegLogFile;

    std::vector<short> pcmInterleaved;
    std::vector<char> mp3Buffer;

    std::atomic<bool> broadcasting { false };
    std::atomic<bool> connected { false };
    std::atomic<int64_t> bytesSent { 0 };
    int reconnectAttempt { 0 };

    mutable juce::CriticalSection errorLock;
    juce::String lastError;

    juce::WaitableEvent wakeEvent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BroadcastService)
};

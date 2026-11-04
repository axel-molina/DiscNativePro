#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <atomic>
#include <vector>

// BroadcastService: emite la mezcla master en vivo a un servidor Icecast/Shoutcast
// propio. Misma arquitectura que AudioRecorder: AbstractFifo lock-free + hilo de red
// aparte + atómicos. El callback de audio solo copia al FIFO (real-time safe).
class BroadcastService : public juce::Thread
{
public:
    struct Config
    {
        juce::String host;
        int port { 8000 };
        juce::String mount { "/stream" };
        juce::String password;
        juce::String stationName;
        int bitrateKbps { 192 };
        bool makePublic { false };
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
    bool connectToServer();
    void closeConnection();
    void sendRemainingEncoderOutput();
    bool sendBytes(const char* data, int numBytes);
    void setError(const juce::String& message);

    static constexpr int fifoSize = 65536; // ~1.5 sec audio buffer at 44.1kHz
    juce::AbstractFifo fifo { fifoSize };
    juce::AudioBuffer<float> fifoBuffer;

    Config config;
    int streamChannels { 2 };

    // Codificador LAME: se crea/destruye fuera del hilo de red; solo run() lo usa.
    void* lameFlags { nullptr };
    std::vector<short> pcmInterleaved;
    std::vector<char> mp3Buffer;

    std::unique_ptr<juce::StreamingSocket> socket;
    std::atomic<bool> broadcasting { false };
    std::atomic<bool> connected { false };
    std::atomic<int64_t> bytesSent { 0 };
    int reconnectAttempt { 0 };

    mutable juce::CriticalSection errorLock;
    juce::String lastError;

    juce::WaitableEvent wakeEvent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BroadcastService)
};

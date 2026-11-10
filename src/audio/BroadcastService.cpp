#include "BroadcastService.h"

#include "../services/FfmpegLocator.h"

#include <lame.h>

#ifdef _WIN32
    #define BROADCAST_POPEN  _popen
    #define BROADCAST_PCLOSE _pclose
#else
    #define BROADCAST_POPEN  popen
    #define BROADCAST_PCLOSE pclose
#endif

namespace
{
    // Escapado para /bin/sh: envuelve en comillas simples y sustituye cada '
    // interna por '"'"' (convencion POSIX). Sin esto, una stream key con
    // comillas/backticks/$(...) ejecutaria comandos via popen.
    juce::String shellQuote(const juce::String& value)
    {
        return "'" + value.replace("'", "'\\''") + "'";
    }
}

BroadcastService::BroadcastService()
    : juce::Thread("DiscPro Broadcast Thread")
{
    fifoBuffer.setSize(2, fifoSize);
}

BroadcastService::~BroadcastService()
{
    stopBroadcast();
}

bool BroadcastService::startBroadcast(const Config& newConfig, double sampleRate, int numChannels)
{
    stopBroadcast();

    config = newConfig;
    config.host = config.host.trim();
    // Sin CR/LF: mount y stationName viajan en la request line y en la cabecera
    // ice-name del handshake Icecast; un salto de linea inyectaria cabeceras.
    config.mount = config.mount.trim().replace("\r", "").replace("\n", "");
    config.stationName = config.stationName.replace("\r", "").replace("\n", "");
    config.ingestUrl = config.ingestUrl.trim();

    if (config.mode == Mode::Icecast)
    {
        if (config.host.isEmpty())
        {
            setError("Servidor vacio");
            return false;
        }
        if (!config.mount.startsWith("/"))
            config.mount = "/" + config.mount;

        // Un espacio rompe la request line ("PUT <mount> HTTP/1.0")
        if (config.mount.contains(" "))
        {
            setError("Mount no puede contener espacios");
            return false;
        }
    }
    else
    {
        if (config.ingestUrl.isEmpty())
        {
            setError("URL de ingesta vacia");
            return false;
        }
        if (!FfmpegLocator::locateFfmpeg().existsAsFile())
        {
            setError("ffmpeg no esta instalado");
            return false;
        }
    }

    streamChannels = juce::jlimit(1, 2, numChannels);
    streamSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

    if (config.mode == Mode::Icecast)
    {
        auto* gfp = lame_init();
        if (gfp == nullptr)
        {
            setError("No se pudo inicializar el codificador MP3");
            return false;
        }

        lame_set_in_samplerate(gfp, (int) streamSampleRate);
        lame_set_num_channels(gfp, streamChannels);
        lame_set_brate(gfp, config.bitrateKbps);
        lame_set_quality(gfp, 2);

        if (lame_init_params(gfp) < 0)
        {
            lame_close(gfp);
            setError("Parametros MP3 invalidos");
            return false;
        }

        lameFlags = gfp;
    }

    pcmInterleaved.assign((size_t) 2048 * (size_t) streamChannels, 0);
    if (config.mode == Mode::Icecast)
    {
        // Tamaño requerido por LAME por llamada: 1.25 * numFrames + 7200 (con margen)
        mp3Buffer.assign((size_t) (2048 * 5 / 4) + 7200 + 1024, 0);
    }

    ffmpegLogFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                        .getChildFile("DiscNativePro/broadcast_ffmpeg.log");

    fifo.reset();
    fifoBuffer.setSize(streamChannels, fifoSize, false, false, false);
    fifoBuffer.clear();
    bytesSent.store(0);
    reconnectAttempt = 0;
    setError("");

    broadcasting.store(true);
    startThread(juce::Thread::Priority::normal);
    return true;
}

void BroadcastService::stopBroadcast()
{
    if (!broadcasting.exchange(false))
        return;

    wakeEvent.signal();
    stopThread(3000);

    if (config.mode == Mode::Rtmp)
    {
        closeFfmpeg();
    }
    else
    {
        // Flush final del codificador (el hilo de red ya no esta corriendo)
        sendRemainingEncoderOutput();

        if (auto* gfp = static_cast<lame_global_flags*>(lameFlags))
            lame_close(gfp);
        lameFlags = nullptr;

        closeConnection();
    }
}

void BroadcastService::pushAudioBlock(const juce::AudioBuffer<float>& buffer, int numSamples)
{
    if (!broadcasting.load(std::memory_order_relaxed) || numSamples <= 0)
        return;

    int start1, size1, start2, size2;
    fifo.prepareToWrite(numSamples, start1, size1, start2, size2);

    int numChannelsToCopy = juce::jmin(streamChannels, buffer.getNumChannels());

    if (size1 > 0)
    {
        for (int ch = 0; ch < numChannelsToCopy; ++ch)
            fifoBuffer.copyFrom(ch, start1, buffer, ch, 0, size1);
    }

    if (size2 > 0)
    {
        for (int ch = 0; ch < numChannelsToCopy; ++ch)
            fifoBuffer.copyFrom(ch, start2, buffer, ch, size1, size2);
    }

    fifo.finishedWrite(size1 + size2);
    wakeEvent.signal();
}

void BroadcastService::run()
{
    const int framesPerChunk = 2048;

    while (!threadShouldExit() && broadcasting.load(std::memory_order_relaxed))
    {
        // 1. Asegurar el transporte (servidor Icecast o proceso ffmpeg)
        if (!connected.load(std::memory_order_relaxed))
        {
            const bool up = (config.mode == Mode::Icecast) ? connectToServer() : launchFfmpeg();
            if (!up)
            {
                // Backoff: 1s, 2s, 4s ... max 30s. Despierta antes si se detiene.
                const int waitMs = juce::jmin(30000, 1000 * (1 << juce::jmin(reconnectAttempt, 5)));
                ++reconnectAttempt;
                wakeEvent.wait(waitMs);
                continue;
            }

            // No reenviar audio viejo tras (re)conectar: el oyente arranca en vivo.
            fifo.reset();
            reconnectAttempt = 0;
        }

        int numReady = fifo.getNumReady();
        if (numReady <= 0)
        {
            wakeEvent.wait(50);
            continue;
        }

        int start1, size1, start2, size2;
        fifo.prepareToRead(juce::jmin(numReady, framesPerChunk), start1, size1, start2, size2);

        const int total = size1 + size2;
        if (total <= 0)
            continue;

        // Float -> short interleaved (rango completo, con clamp de seguridad)
        auto* out = pcmInterleaved.data();
        for (int i = 0; i < total; ++i)
        {
            const int pos = (i < size1) ? start1 + i : start2 + (i - size1);
            for (int ch = 0; ch < streamChannels; ++ch)
            {
                const float s = juce::jlimit(-1.0f, 1.0f, fifoBuffer.getSample(ch, pos));
                const int v = (int) std::lround(s * 32767.0f);
                out[(size_t) ((size_t) i * (size_t) streamChannels + (size_t) ch)]
                    = (short) juce::jlimit(-32768, 32767, v);
            }
        }

        fifo.finishedRead(total);

        // 3. Enviar
        if (config.mode == Mode::Rtmp)
        {
            if (!writePcmToFfmpeg(out, total))
            {
                closeFfmpeg();
                setError("Conexion perdida con la salida RTMP, reconectando...");
            }
            continue;
        }

        auto* gfp = static_cast<lame_global_flags*>(lameFlags);
        if (gfp == nullptr)
            break;

        const int mp3Bytes = lame_encode_buffer_interleaved(gfp, out, total,
                                                            (unsigned char*) mp3Buffer.data(),
                                                            (int) mp3Buffer.size());
        if (mp3Bytes > 0 && !sendBytes(mp3Buffer.data(), mp3Bytes))
        {
            closeConnection();
            setError("Conexion perdida, reconectando...");
        }
    }
}

bool BroadcastService::connectToServer()
{
    auto sock = std::make_unique<juce::StreamingSocket>();

    if (!sock->connect(config.host, config.port, 5000))
    {
        setError("No se pudo conectar a " + config.host + ":" + juce::String(config.port));
        return false;
    }

    // Handshake Icecast 2 (compatible con Shoutcast v2)
    const juce::String auth = juce::Base64::toBase64("source:" + config.password);
    juce::String request;
    request << "PUT " << config.mount << " HTTP/1.0\r\n"
            << "Accept: */*\r\n"
            << "Content-Type: audio/mpeg\r\n"
            << "Authorization: Basic " << auth << "\r\n"
            << "ice-public: " << (config.makePublic ? "1" : "0") << "\r\n"
            << "ice-name: " << config.stationName << "\r\n"
            << "ice-bitrate: " << config.bitrateKbps << "\r\n"
            << "\r\n";

    const auto utf8 = request.toUTF8();
    if (sock->write(utf8.getAddress(), (int) utf8.sizeInBytes()) != (int) utf8.sizeInBytes())
    {
        setError("Fallo enviando el handshake al servidor");
        return false;
    }

    if (sock->waitUntilReady(true, 5000) != 1)
    {
        setError("Sin respuesta del servidor de streaming");
        return false;
    }

    char response[256] = { 0 };
    const int got = sock->read(response, sizeof(response) - 1, true);
    if (got <= 0)
    {
        setError("Sin respuesta del servidor de streaming");
        return false;
    }

    const juce::String responseText(response, (size_t) got);
    if (!responseText.contains(" 200 "))
    {
        setError("El servidor rechazo la emision: "
                 + responseText.upToFirstOccurrenceOf("\r", false, false).trim());
        return false;
    }

    socket = std::move(sock);
    connected.store(true);
    return true;
}

bool BroadcastService::launchFfmpeg()
{
    auto ffmpeg = FfmpegLocator::locateFfmpeg();
    if (!ffmpeg.existsAsFile())
    {
        setError("ffmpeg no esta instalado");
        return false;
    }

    ffmpegLogFile.getParentDirectory().createDirectory();

    // ffmpeg: PCM por stdin -> AAC -> FLV -> RTMP/RTMPS (TLS incluido en el build estatico)
    juce::String cmd = shellQuote(ffmpeg.getFullPathName())
                       + " -hide_banner -loglevel error -nostdin"
                       + " -f s16le -ar " + juce::String((int) streamSampleRate)
                       + " -ac " + juce::String(streamChannels)
                       + " -i pipe:0"
                       + " -c:a aac -b:a " + juce::String(config.bitrateKbps) + "k"
                       + " -f flv " + shellQuote(config.ingestUrl)
                       + " 2> " + shellQuote(ffmpegLogFile.getFullPathName());

    ffmpegPipe = BROADCAST_POPEN(cmd.toUTF8(), "w");
    if (ffmpegPipe == nullptr)
    {
        setError("No se pudo iniciar ffmpeg");
        return false;
    }

    // Sin buffering: el audio ya llega en tiempo real desde el FIFO
    std::setvbuf(ffmpegPipe, nullptr, _IONBF, 0);

    connected.store(true);
    return true;
}

void BroadcastService::closeFfmpeg()
{
    connected.store(false);

    if (ffmpegPipe != nullptr)
    {
        auto* pipe = ffmpegPipe;
        ffmpegPipe = nullptr;
        BROADCAST_PCLOSE(pipe);
    }
}

bool BroadcastService::writePcmToFfmpeg(const short* interleaved, int numFrames)
{
    if (ffmpegPipe == nullptr)
        return false;

    const size_t numBytes = (size_t) numFrames * (size_t) streamChannels * sizeof(short);
    const size_t written = std::fwrite(interleaved, 1, numBytes, ffmpegPipe);
    if (written != numBytes)
        return false;

    bytesSent.fetch_add((int64_t) numBytes, std::memory_order_relaxed);
    return true;
}

void BroadcastService::closeConnection()
{
    connected.store(false);
    if (socket != nullptr)
    {
        socket->close();
        socket.reset();
    }
}

void BroadcastService::sendRemainingEncoderOutput()
{
    auto* gfp = static_cast<lame_global_flags*>(lameFlags);
    if (gfp == nullptr)
        return;

    const int mp3Bytes = lame_encode_flush(gfp, (unsigned char*) mp3Buffer.data(),
                                           (int) mp3Buffer.size());
    if (mp3Bytes > 0 && connected.load(std::memory_order_relaxed) && socket != nullptr)
        sendBytes(mp3Buffer.data(), mp3Bytes);
}

bool BroadcastService::sendBytes(const char* data, int numBytes)
{
    if (socket == nullptr)
        return false;

    const int written = socket->write(data, numBytes);
    if (written != numBytes)
        return false;

    bytesSent.fetch_add(numBytes, std::memory_order_relaxed);
    return true;
}

juce::String BroadcastService::getLastError() const
{
    juce::ScopedLock lock(errorLock);
    return lastError;
}

void BroadcastService::setError(const juce::String& message)
{
    juce::ScopedLock lock(errorLock);
    lastError = message;
}

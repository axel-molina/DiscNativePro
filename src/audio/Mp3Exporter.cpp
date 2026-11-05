#include "Mp3Exporter.h"

#include <lame.h>
#include <vector>

namespace Mp3Exporter
{

juce::String encodeWavToMp3(const juce::File& wavFile, const juce::File& outMp3, int bitrateKbps)
{
    if (!wavFile.existsAsFile())
        return "El archivo de entrada no existe";

    if (outMp3.existsAsFile() && !outMp3.deleteFile())
        return "No se pudo sobrescribir el MP3 de salida";

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(wavFile));
    if (reader == nullptr)
        return "No se pudo leer el archivo de audio";

    const int numChannels = juce::jlimit(1, 2, (int) reader->numChannels);
    const double sampleRate = reader->sampleRate > 0.0 ? reader->sampleRate : 44100.0;

    auto* gfp = lame_init();
    if (gfp == nullptr)
        return "No se pudo inicializar el codificador MP3";

    lame_set_in_samplerate(gfp, (int) sampleRate);
    lame_set_num_channels(gfp, numChannels);
    lame_set_brate(gfp, bitrateKbps);
    lame_set_quality(gfp, 2);

    if (lame_init_params(gfp) < 0)
    {
        lame_close(gfp);
        return "Parametros MP3 invalidos";
    }

    auto outStream = outMp3.createOutputStream();
    if (outStream == nullptr)
    {
        lame_close(gfp);
        return "No se pudo crear el MP3 de salida";
    }

    constexpr int framesPerBlock = 4096;
    juce::AudioBuffer<float> block(numChannels, framesPerBlock);
    std::vector<short> pcm((size_t) framesPerBlock * (size_t) numChannels, 0);
    std::vector<char> mp3Buffer((size_t) (framesPerBlock * 5 / 4) + 7200 + 1024, 0);

    juce::String error;
    juce::int64 remaining = reader->lengthInSamples;
    juce::int64 readPos = 0;

    while (remaining > 0)
    {
        const int frames = (int) juce::jmin<juce::int64>(framesPerBlock, remaining);
        if (!reader->read(&block, 0, frames, readPos, true, numChannels > 1))
        {
            error = "Error leyendo el audio de entrada";
            break;
        }
        readPos += frames;

        for (int i = 0; i < frames; ++i)
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float s = juce::jlimit(-1.0f, 1.0f, block.getSample(ch, i));
                const int v = (int) std::lround(s * 32767.0f);
                pcm[(size_t) ((size_t) i * (size_t) numChannels + (size_t) ch)]
                    = (short) juce::jlimit(-32768, 32767, v);
            }
        }

        const int mp3Bytes = lame_encode_buffer_interleaved(gfp, pcm.data(), frames,
                                                            (unsigned char*) mp3Buffer.data(),
                                                            (int) mp3Buffer.size());
        if (mp3Bytes < 0)
        {
            error = "Error codificando MP3";
            break;
        }

        if (mp3Bytes > 0 && outStream->write(mp3Buffer.data(), (size_t) mp3Bytes) != (size_t) mp3Bytes)
        {
            error = "Error escribiendo el MP3";
            break;
        }

        remaining -= frames;
    }

    const int flushed = lame_encode_flush(gfp, (unsigned char*) mp3Buffer.data(), (int) mp3Buffer.size());
    if (error.isEmpty() && flushed > 0
        && outStream->write(mp3Buffer.data(), (size_t) flushed) != (size_t) flushed)
    {
        error = "Error escribiendo el MP3";
    }

    lame_close(gfp);
    outStream->flush();
    outStream.reset();

    if (error.isNotEmpty())
        outMp3.deleteFile();

    return error;
}

} // namespace Mp3Exporter

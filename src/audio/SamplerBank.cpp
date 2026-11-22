#include "SamplerBank.h"
#include <cmath>
#include <cstring>

bool SamplerBank::loadSlot(int index, const juce::File& file,
                           juce::AudioFormatManager& formatManager, double deviceSampleRate)
{
    if (index < 0 || index >= numSlots || deviceSampleRate <= 0.0)
        return false;

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader == nullptr || reader->lengthInSamples <= 0)
        return false;

    juce::int64 srcLen = reader->lengthInSamples;
    juce::int64 maxLen = (juce::int64)(deviceSampleRate * maxSampleSeconds);
    if (srcLen > maxLen)
        srcLen = maxLen;

    const int srcCh = (int)reader->numChannels;
    // JUCE: setResamplingRatio() son muestras de ENTRADA por muestra de SALIDA.
    // Para reproducir a la velocidad real hay que consumir fileRate/deviceRate
    // por muestra del dispositivo (misma semantica que
    // DeckPlayer::updateRateFactor). Con el inverso, un sample de 48 kHz en un
    // dispositivo de 44.1 kHz salia 2,92 semitonos abajo y solo sonaba el 84,6 %
    // del material (medido en el banco de pruebas).
    const double ratio = reader->sampleRate / deviceSampleRate;
    const int outLen = juce::jmax(1, (int)std::ceil((double)srcLen / ratio)) + 64;

    juce::AudioBuffer<float> buf(2, outLen);
    buf.clear();

    if (std::abs(ratio - 1.0) < 1e-6)
    {
        const int n = (int)juce::jmin<juce::int64>(srcLen, outLen);
        reader->read(&buf, 0, n, 0, true, true);
        if (srcCh < 2) // mono: duplica determinista
            std::memcpy(buf.getWritePointer(1), buf.getReadPointer(0), sizeof(float) * (size_t)n);
    }
    else
    {
        juce::AudioFormatReaderSource src(reader.release(), true);
        juce::ResamplingAudioSource rs(&src, false, 2);
        rs.prepareToPlay(512, deviceSampleRate);
        rs.setResamplingRatio(ratio);
        juce::AudioSourceChannelInfo info(&buf, 0, outLen);
        rs.getNextAudioBlock(info);
        if (srcCh < 2)
            std::memcpy(buf.getWritePointer(1), buf.getReadPointer(0), sizeof(float) * (size_t)outLen);
    }

    // Fade-out de 8 ms horneado (sin clics al terminar, costo cero en RT)
    const int fade = juce::jmin(outLen, (int)(deviceSampleRate * 0.008));
    for (int ch = 0; ch < 2; ++ch)
    {
        float* d = buf.getWritePointer(ch);
        for (int i = 0; i < fade; ++i)
            d[outLen - 1 - i] *= (float)i / (float)fade;
    }

    auto& slot = slots[(size_t)index];
    slot.playing.store(false);
    juce::Thread::sleep(120); // deja terminar el bloque de audio en vuelo
    slot.sample = std::move(buf);
    slot.name = file.getFileNameWithoutExtension();
    slot.readPos.store(0);
    lastDeviceSr = deviceSampleRate;
    return true;
}

void SamplerBank::freeSlot(int index)
{
    auto& slot = slots[(size_t)index];
    slot.playing.store(false);
    juce::Thread::sleep(120);
    slot.sample.setSize(0, 0);
    slot.name.clear();
    slot.readPos.store(0);
}

void SamplerBank::unloadSlot(int index)
{
    if (index < 0 || index >= numSlots)
        return;
    freeSlot(index);
}

void SamplerBank::trigger(int index)
{
    if (index < 0 || index >= numSlots)
        return;
    auto& slot = slots[(size_t)index];
    if (slot.sample.getNumSamples() <= 0)
        return;
    slot.readPos.store(0);
    slot.playing.store(true);
}

void SamplerBank::stopSlot(int index)
{
    if (index < 0 || index >= numSlots)
        return;
    slots[(size_t)index].playing.store(false);
}

void SamplerBank::stopAll()
{
    for (auto& slot : slots)
        slot.playing.store(false);
}

bool SamplerBank::isLoaded(int index) const
{
    if (index < 0 || index >= numSlots)
        return false;
    return slots[(size_t)index].sample.getNumSamples() > 0;
}

juce::String SamplerBank::getName(int index) const
{
    if (index < 0 || index >= numSlots)
        return {};
    return slots[(size_t)index].name;
}

double SamplerBank::getDurationSeconds(int index) const
{
    if (index < 0 || index >= numSlots || lastDeviceSr <= 0.0)
        return 0.0;
    return (double)slots[(size_t)index].sample.getNumSamples() / lastDeviceSr;
}

void SamplerBank::process(juce::AudioBuffer<float>& master, int numSamples)
{
    if (numSamples <= 0)
        return;
    const float g = masterVolume.load();
    if (g <= 0.0f)
    {
        // Volumen 0: igual avanza (los one-shots siguen su curso en silencio)
        for (auto& slot : slots)
        {
            if (!slot.playing.load())
                continue;
            const int len = slot.sample.getNumSamples();
            const int next = slot.readPos.load() + numSamples;
            slot.readPos.store(next);
            if (next >= len)
                slot.playing.store(false);
        }
        return;
    }

    const int mCh = master.getNumChannels();
    for (auto& slot : slots)
    {
        if (!slot.playing.load())
            continue;
        const int len = slot.sample.getNumSamples();
        const int pos = slot.readPos.load();
        const int avail = len - pos;
        if (avail <= 0)
        {
            slot.playing.store(false);
            continue;
        }
        const int n = juce::jmin(numSamples, avail);
        const int sCh = slot.sample.getNumChannels();
        for (int ch = 0; ch < mCh; ++ch)
            master.addFrom(ch, 0, slot.sample, ch % juce::jmax(1, sCh), pos, n, g);
        const int next = pos + n;
        slot.readPos.store(next);
        if (next >= len)
            slot.playing.store(false);
    }
}

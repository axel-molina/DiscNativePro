#include "BeatDetector.h"
#include <cmath>
#include <vector>
#include <map>
#include <numeric>
#include <algorithm>

BeatAnalysisResult BeatDetector::analyze(juce::AudioFormatReader* reader)
{
    BeatAnalysisResult defaultRes;
    defaultRes.bpm = 0.0;
    defaultRes.offset = 0.0;
    defaultRes.beatInterval = 0.0;
    if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate <= 0.0)
        return defaultRes;

    // Analyze up to 60 seconds
    const double maxDuration = 60.0;
    int64_t maxSamples = std::min<int64_t>(static_cast<int64_t>(maxDuration * reader->sampleRate),
                                           static_cast<int64_t>(reader->lengthInSamples));

    int downsampleFactor = std::max(1, (int)(reader->sampleRate / 4000.0));
    double effectiveRate = reader->sampleRate / (double)downsampleFactor;
    int downsampledLength = (int)(maxSamples / downsampleFactor);
    if (downsampledLength <= 100)
        return defaultRes;

    std::vector<float> downsampled(static_cast<size_t>(downsampledLength), 0.0f);

    // Read audio in blocks of 4096 samples
    const int readChunkSize = 4096;
    juce::AudioBuffer<float> readBuffer(1, readChunkSize);
    int64_t samplesReadTotal = 0;

    while (samplesReadTotal < maxSamples)
    {
        int toRead = static_cast<int>(std::min<int64_t>(readChunkSize, maxSamples - samplesReadTotal));
        readBuffer.clear();
        reader->read(&readBuffer, 0, toRead, samplesReadTotal, true, false);

        const float* raw = readBuffer.getReadPointer(0);
        for (int i = 0; i < toRead; ++i)
        {
            int64_t globalSample = samplesReadTotal + i;
            size_t downIndex = static_cast<size_t>(globalSample / downsampleFactor);
            if (downIndex < downsampled.size())
            {
                downsampled[downIndex] += std::abs(raw[i]);
            }
        }
        samplesReadTotal += toRead;
    }

    for (auto& val : downsampled)
    {
        val /= (float)downsampleFactor;
    }

    // Moving average filter (100ms window) for kick transient energy
    int windowSize = std::max(1, (int)(effectiveRate * 0.1));
    std::vector<float> energy(downsampled.size(), 0.0f);
    float movingSum = 0.0f;

    for (size_t i = 0; i < downsampled.size(); ++i)
    {
        movingSum += downsampled[i];
        if (i >= static_cast<size_t>(windowSize))
        {
            movingSum -= downsampled[i - static_cast<size_t>(windowSize)];
        }
        energy[i] = movingSum / (float)windowSize;
    }

    // La media movil es de ARRASTRE: su maximo cae medio ventana (~50 ms)
    // despues del transitorio. Se desplaza media ventana hacia atras para que
    // energyC[i] corresponda al tiempo i (antes el pico salia ~99 ms tarde).
    const size_t halfWindow = (size_t) (windowSize / 2);
    std::vector<float> energyC(energy.size(), 0.0f);
    for (size_t i = 0; i + halfWindow < energy.size(); ++i)
        energyC[i] = energy[i + halfWindow];

    // Dynamic threshold calculation
    float sumEnergy = std::accumulate(energyC.begin(), energyC.end(), 0.0f);
    float meanEnergy = sumEnergy / (float)energyC.size();
    float threshold = meanEnergy * 1.35f;

    // Detect peaks with minimum distance (200 BPM -> min interval ~0.25s)
    std::vector<int> peaks;
    int minPeakDistance = std::max(1, (int)(effectiveRate * 0.25));

    // Un transitorio mas corto que la ventana deja una meseta con valores
    // identicos; con un maximo ESTRICTO no se detectaba ningun pico (0 BPM en
    // material con kicks de 90 ms). Con ">=" a la izquierda y ">" a la derecha
    // se detecta el final de la meseta.
    for (size_t i = 1; i + 1 < energyC.size(); ++i)
    {
        if (energyC[i] > threshold && energyC[i] >= energyC[i - 1] && energyC[i] > energyC[i + 1])
        {
            if (peaks.empty() || (static_cast<int>(i) - peaks.back() > minPeakDistance))
            {
                peaks.push_back(static_cast<int>(i));
            }
        }
    }

    if (peaks.size() < 2)
        return defaultRes;

    // --- Tempo por contraste on-beat / off-beat sobre la envolvente ---
    // El histograma de intervalos entre picos fallaba en musica real: el
    // detector de picos se deja la mayoria de los golpes, y los huecos de varios
    // segundos contaminaban los votos (daba ~119 BPM tanto para 127 como para
    // 155). Aqui se mide, por cada tempo candidato, la energia que cae EN el
    // pulso menos la que cae ENTRE pulsos: el tempo correcto maximiza eso.
    const double dbgRate = effectiveRate;

    auto meanEnergyAt = [&energyC, dbgRate] (double periodSec) -> double
    {
        const double period = periodSec * dbgRate; // en muestras
        if (period < 2.0) return 0.0;

        double sum = 0.0;
        int n = 0;
        for (double p = 0.0; p + 1.0 < (double) energyC.size(); p += period)
        {
            sum += (double) energyC[(size_t) p];
            ++n;
        }
        return n > 0 ? sum / (double) n : 0.0;
    };

    auto contrastAt = [&meanEnergyAt] (double bpm) -> double
    {
        const double period = 60.0 / bpm;
        const double on = meanEnergyAt (period);
        const double half = meanEnergyAt (period * 0.5);   // mezcla on y off
        return on - (2.0 * half - on);                     // on - off real
    };

    double bestBpm = 0.0, bestContrast = -1.0e30;
    for (double bpm = 60.0; bpm <= 200.0; bpm += 0.25)
    {
        const double c = contrastAt (bpm);
        if (c > bestContrast) { bestContrast = c; bestBpm = bpm; }
    }

    if (bestBpm <= 0.0)
        return defaultRes;

    // Refinamiento fino alrededor del maximo.
    double refinedBpm = bestBpm;
    double fineBest = -1.0e30;
    for (double bpm = juce::jmax (60.0, bestBpm - 0.6); bpm <= juce::jmin (200.0, bestBpm + 0.6); bpm += 0.01)
    {
        const double c = contrastAt (bpm);
        if (c > fineBest) { fineBest = c; refinedBpm = bpm; }
    }

#if defined (DNP_BPM_DEBUG)
    std::fprintf (stderr, "  [dbg] picos=%d  rate=%.0f Hz\n", (int) peaks.size(), effectiveRate);
    std::fprintf (stderr, "  [dbg] mejor contraste: %.2f BPM (%.3f)\n", refinedBpm, fineBest);
    for (double b = refinedBpm * 0.5; b <= refinedBpm * 2.0; b *= 2.0)
        if (b >= 60.0 && b <= 200.0)
            std::fprintf (stderr, "  [dbg]   %.2f BPM -> contraste %.3f\n", b, contrastAt (b));
#endif
    BeatAnalysisResult result;
    result.bpm = refinedBpm;
    if (result.bpm <= 0.0)
        return defaultRes;
    result.beatInterval = 60.0 / result.bpm;

    // Fase del grid por busqueda en rejilla: se elige el desfase que maximiza
    // la energia en los tiempos de beat. Antes se usaba el primer pico (con su
    // retraso y pudiendo caer en un snare), que dejaba el offset ~99 ms tarde.
    // Se maximiza la ENVOLVENTE DE ATAQUE (primera diferencia positiva) y no la
    // energia: el maximo de energia cae en el centro del golpe (~45 ms tarde en
    // material con cuerpo), mientras que la subida maxima esta en el ataque.
    auto riseAt = [&energyC] (int idx) -> float
    {
        if (idx <= 0) return 0.0f;
        return juce::jmax (0.0f, energyC[(size_t) idx] - energyC[(size_t) idx - 1]);
    };

    const int intervalBuckets = juce::jmax (1, (int) std::lround (result.beatInterval * effectiveRate));
    double bestScore = -1.0;
    int bestPhase = 0;
    for (int ph = 0; ph < intervalBuckets; ++ph)
    {
        double score = 0.0;
        for (int idx = ph; idx < (int) energyC.size(); idx += intervalBuckets)
            score += (double) riseAt (idx);

        if (score > bestScore)
        {
            bestScore = score;
            bestPhase = ph;
        }
    }

    // Correccion de sesgo: para cada pulso del grid se busca el maximo de la
    // energia CRUDA (buckets de ~2.7 ms, que en un golpe con caida esta en el
    // ataque) en +-60 ms, y se aplica la MEDIANA del desplazamiento. Sin esto
    // el offset heredaba el retraso de la ventana de 100 ms.
    const int reach = juce::jmax (1, (int) std::lround (0.060 * effectiveRate));
    std::vector<double> corrections;

    for (int k = 0; k < 256; ++k)
    {
        const int base = bestPhase + k * intervalBuckets;
        if (base >= (int) downsampled.size())
            break;

        int bestIdx = base;
        double bestValue = -1.0;
        for (int j = -reach; j <= reach; ++j)
        {
            const int idx = base + j;
            if (idx < 0 || idx >= (int) downsampled.size())
                continue;
            const double v = (double) downsampled[(size_t) idx];
            if (v > bestValue)
            {
                bestValue = v;
                bestIdx = idx;
            }
        }

        corrections.push_back ((double) (bestIdx - base));
    }

    double bias = 0.0;
    if (! corrections.empty())
    {
        std::sort (corrections.begin(), corrections.end());
        bias = corrections[corrections.size() / 2];
    }

    double offsetSec = ((double) bestPhase + bias) / effectiveRate;
    offsetSec = std::fmod (offsetSec, result.beatInterval);
    if (offsetSec < 0.0)
        offsetSec += result.beatInterval;
    result.offset = offsetSec;
    return result;
}

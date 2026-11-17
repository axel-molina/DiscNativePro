#include "KeyDetector.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>
#include <array>
#include <algorithm>
#include "../ui/UiPalette.h"

namespace
{
// Perfiles Sha'ath / EDMA (Electronic Dance Music Analysis)
// Optimizados específicamente para música moderna, electrónica, pop y DJ.
// Mayor: Tónica (0), Tercera Mayor (4), Quinta (7) dominantes con semitonos diatónicos balanceados.
constexpr float kShaathMajor[12] = {
    6.35f, 1.25f, 3.48f, 1.15f, 4.38f, 3.80f,
    1.00f, 5.19f, 1.20f, 3.66f, 1.15f, 2.88f
};

// Menor: Refleja el modo Eólico / Menor Natural predominante en música electrónica.
// Énfasis pronunciado en Tónica (0), b3 (3), 4ta (5), 5ta (7), b6 (8) y b7 (10).
constexpr float kShaathMinor[12] = {
    6.33f, 1.20f, 3.52f, 5.38f, 1.15f, 3.53f,
    1.05f, 4.75f, 3.98f, 1.20f, 3.95f, 1.15f
};

// Camelot por pitch class (0 = C, 1 = C#/Db, 2 = D, ..., 11 = B)
const char* const kCamelotMinor[12] = {
    "5A", "12A", "7A", "2A", "9A", "4A",
    "11A", "6A", "1A", "8A", "3A", "10A"
};

const char* const kCamelotMajor[12] = {
    "8B", "3B", "10B", "5B", "12B", "7B",
    "2B", "9B", "4B", "11B", "6B", "1B"
};

// Nombres musicales estándar
const char* const kMusicalMinor[12] = {
    "Cm", "C#m", "Dm", "Ebm", "Em", "Fm",
    "F#m", "Gm", "Abm", "Am", "Bbm", "Bm"
};

const char* const kMusicalMajor[12] = {
    "C", "Db", "D", "Eb", "E", "F",
    "F#", "G", "Ab", "A", "Bb", "B"
};

constexpr int kFftOrder = 12;
constexpr int kFftSize = 1 << kFftOrder; // 4096 samples (~93 ms a 44.1 kHz)
constexpr int kHop = 2048;               // 50% solapamiento para mayor resolución temporal

// Colores oficiales de la Rueda Camelot
const juce::Colour kCamelotColours[12] = {
    juce::Colour::fromRGB(0, 210, 190),   // 1A / 1B: Aqua / Cyan
    UiPalette::accentBright,   // 2A / 2B: Sky Blue
    juce::Colour::fromRGB(67, 97, 238),   // 3A / 3B: Royal Blue
    juce::Colour::fromRGB(114, 9, 183),   // 4A / 4B: Deep Indigo / Violet
    juce::Colour::fromRGB(181, 23, 158),  // 5A / 5B: Magenta
    juce::Colour::fromRGB(230, 57, 70),   // 6A / 6B: Ruby Red
    juce::Colour::fromRGB(247, 127, 0),   // 7A / 7B: Amber / Orange-Red
    juce::Colour::fromRGB(243, 156, 18),  // 8A / 8B: Warm Orange
    juce::Colour::fromRGB(241, 196, 15),  // 9A / 9B: Gold / Yellow
    juce::Colour::fromRGB(132, 204, 22),  // 10A / 10B: Lime Green
    juce::Colour::fromRGB(16, 185, 129),  // 11A / 11B: Emerald Green
    juce::Colour::fromRGB(20, 184, 166)   // 12A / 12B: Teal
};

// Extrae el número Camelot (1..12) y letra ('A' o 'B') de una cadena arbitraria
bool parseCamelotCode(const juce::String& s, int& outNum, char& outLetter)
{
    juce::String t = s.trim().toUpperCase();
    if (t.isEmpty() || t == "--")
        return false;

    // Buscar formato directo "8A", "11B", o inicio de "8A / Am"
    int num = 0;
    int idx = 0;
    while (idx < t.length() && t[idx] >= '0' && t[idx] <= '9')
    {
        num = num * 10 + (t[idx] - '0');
        idx++;
    }

    if (num >= 1 && num <= 12 && idx < t.length())
    {
        char let = static_cast<char>(t[idx]);
        if (let == 'A' || let == 'B')
        {
            outNum = num;
            outLetter = let;
            return true;
        }
    }

    // Intentar interpretar nombre musical si viene en formato "Am", "F#m", "C"
    juce::String clean = t.upToFirstOccurrenceOf(" ", false, false);
    for (int pc = 0; pc < 12; ++pc)
    {
        if (clean.equalsIgnoreCase(kMusicalMinor[pc]))
        {
            juce::String cam = kCamelotMinor[pc];
            outNum = cam.dropLastCharacters(1).getIntValue();
            outLetter = 'A';
            return true;
        }
        if (clean.equalsIgnoreCase(kMusicalMajor[pc]))
        {
            juce::String cam = kCamelotMajor[pc];
            outNum = cam.dropLastCharacters(1).getIntValue();
            outLetter = 'B';
            return true;
        }
    }

    return false;
}
} // namespace

juce::String KeyDetector::camelotFor(int rootPc, bool minor)
{
    if (rootPc < 0 || rootPc > 11)
        return "--";
    return minor ? kCamelotMinor[rootPc] : kCamelotMajor[rootPc];
}

juce::String KeyDetector::musicalKeyFor(int rootPc, bool minor)
{
    if (rootPc < 0 || rootPc > 11)
        return "--";
    return minor ? kMusicalMinor[rootPc] : kMusicalMajor[rootPc];
}

juce::String KeyDetector::formatFullKey(int rootPc, bool minor)
{
    if (rootPc < 0 || rootPc > 11)
        return "--";
    return camelotFor(rootPc, minor) + " / " + musicalKeyFor(rootPc, minor);
}

juce::String KeyDetector::formatKeyString(const juce::String& rawKey)
{
    int num = 0;
    char let = 'A';
    if (parseCamelotCode(rawKey, num, let))
    {
        juce::String cam = juce::String(num) + juce::String::charToString(let);
        // Buscar el musicalKey correspondiente
        bool isMinor = (let == 'A');
        for (int pc = 0; pc < 12; ++pc)
        {
            if (cam == (isMinor ? kCamelotMinor[pc] : kCamelotMajor[pc]))
                return cam + " / " + (isMinor ? kMusicalMinor[pc] : kMusicalMajor[pc]);
        }
        return cam;
    }
    return rawKey.isNotEmpty() ? rawKey : "--";
}

juce::Colour KeyDetector::getCamelotColour(const juce::String& keyStr)
{
    int num = 0;
    char let = 'A';
    if (parseCamelotCode(keyStr, num, let))
    {
        if (num >= 1 && num <= 12)
            return kCamelotColours[num - 1];
    }
    return juce::Colour::fromRGB(142, 149, 165); // Gris neutro por defecto
}

bool KeyDetector::areKeysCompatible(const juce::String& keyA, const juce::String& keyB)
{
    int numA = 0, numB = 0;
    char letA = 'A', letB = 'A';
    if (!parseCamelotCode(keyA, numA, letA) || !parseCamelotCode(keyB, numB, letB))
        return false;

    // 1. Mismo tono exacto
    if (numA == numB && letA == letB)
        return true;

    // 2. Cambio de modalidad entre relativa Mayor/Menor (mismo número, diferente letra)
    if (numA == numB)
        return true;

    // 3. Mismo anillo, paso adyacente (+1 o -1 en la rueda de 12 horas)
    if (letA == letB)
    {
        int diff = std::abs(numA - numB);
        if (diff == 1 || diff == 11) // 12 a 1 es adyacente
            return true;
    }

    return false;
}

KeyAnalysisResult KeyDetector::analyze(juce::AudioFormatReader* reader, double maxSeconds)
{
    KeyAnalysisResult result;
    if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate <= 0.0)
        return result;

    const double sr = reader->sampleRate;
    const double fileSec = (double)reader->lengthInSamples / sr;
    double span = (maxSeconds > 0.0) ? juce::jmin(fileSec, maxSeconds) : fileSec;

    // Ventana centrada para capturar el cuerpo musical y evitar intros/outros con solo percusión
    double startSec = (fileSec > span) ? (fileSec - span) * 0.5 : 0.0;
    juce::int64 startSample = (juce::int64)(startSec * sr);
    juce::int64 totalSamples = juce::jmin(reader->lengthInSamples - startSample, (juce::int64)(span * sr));

    if (totalSamples < kFftSize)
        return result;

    const int numCh = (int)reader->numChannels;
    juce::AudioBuffer<float> window(numCh, kFftSize);

    juce::dsp::FFT fft(kFftOrder);
    std::vector<float> fftData((size_t)kFftSize * 2, 0.0f);

    // Ventana Hann precalculada
    std::vector<float> hann((size_t)kFftSize);
    for (int i = 0; i < kFftSize; ++i)
        hann[(size_t)i] = 0.5f - 0.5f * std::cos(2.0f * juce::MathConstants<float>::pi * (float)i / (float)(kFftSize - 1));

    // -------------------------------------------------------------------------
    // PASO 1: Estimación de Afinación Global (Detuning cents respecto a A=440Hz)
    // -------------------------------------------------------------------------
    std::array<double, 100> tuningHistogram {}; // 100 bins de -50 a +49 cents
    juce::int64 pos = startSample;
    const juce::int64 end = startSample + totalSamples;
    int tuningFrames = 0;

    // Escaneo preliminar rápido de picos espectrales para determinar afinación
    const juce::int64 tuneStep = kHop * 4;
    while (pos + kFftSize <= end && tuningFrames < 32)
    {
        if (!reader->read(&window, 0, kFftSize, pos, true, true))
            break;

        for (int i = 0; i < kFftSize; ++i)
        {
            float s = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
                s += window.getReadPointer(ch)[i];
            fftData[(size_t)i] = (s / (float)juce::jmax(1, numCh)) * hann[(size_t)i];
        }

        fft.performFrequencyOnlyForwardTransform(fftData.data(), true);

        // Detectar picos espectrales entre 110 Hz (A2) y 1760 Hz (A6)
        int minK = juce::jmax(2, (int)std::floor(110.0 * (double)kFftSize / sr));
        int maxK = juce::jmin(kFftSize / 2 - 2, (int)std::ceil(1760.0 * (double)kFftSize / sr));

        for (int k = minK; k <= maxK; ++k)
        {
            float m = fftData[(size_t)k];
            // Pico local más prominente que sus vecinos inmediatos
            if (m > fftData[(size_t)(k - 1)] && m > fftData[(size_t)(k + 1)] && m > 0.001f)
            {
                double f = (double)k * sr / (double)kFftSize;
                double midiPitch = 69.0 + 12.0 * std::log2(f / 440.0);
                double nearestSemi = std::round(midiPitch);
                double centOffset = (midiPitch - nearestSemi) * 100.0; // [-50.0, +50.0]

                int bin = (int)std::floor(centOffset + 50.0);
                bin = juce::jlimit(0, 99, bin);
                tuningHistogram[(size_t)bin] += std::sqrt((double)m);
            }
        }

        tuningFrames++;
        pos += tuneStep;
    }

    // Calcular centroide del pico de afinación
    int maxTuningBin = 50; // default 0 cents (bin 50 = 0)
    double maxTuningVal = 0.0;
    for (int b = 0; b < 100; ++b)
    {
        if (tuningHistogram[(size_t)b] > maxTuningVal)
        {
            maxTuningVal = tuningHistogram[(size_t)b];
            maxTuningBin = b;
        }
    }

    float detectedTuningCents = 0.0f;
    if (maxTuningVal > 0.01)
    {
        // Interpolación parabólica de 3 puntos alrededor del máximo
        int b0 = juce::jmax(0, maxTuningBin - 1);
        int b1 = maxTuningBin;
        int b2 = juce::jmin(99, maxTuningBin + 1);
        double y0 = tuningHistogram[(size_t)b0];
        double y1 = tuningHistogram[(size_t)b1];
        double y2 = tuningHistogram[(size_t)b2];
        double denom = (y0 - 2.0 * y1 + y2);
        double delta = (std::abs(denom) > 1e-9) ? 0.5 * (y0 - y2) / denom : 0.0;
        detectedTuningCents = (float)((double)b1 + delta - 50.0);
    }
    result.tuningCents = detectedTuningCents;

    // Frecuencia de afinación de referencia corregida
    const double adjustedA4 = 440.0 * std::pow(2.0, (double)detectedTuningCents / 1200.0);

    // -------------------------------------------------------------------------
    // PASO 2: Dual-Band Chroma (Bajo vs Armonía) con Spectral Whitening
    // -------------------------------------------------------------------------
    double bassChroma[12] = { 0.0 };  // 35 Hz - 220 Hz (Tónica del bajo fundamental)
    double harmChroma[12] = { 0.0 };  // 220 Hz - 2800 Hz (Acordes, melodía y pads)

    pos = startSample;
    int windows = 0;

    // Búfer para envolvente espectral móvil (spectral whitening para suprimir percusión)
    std::vector<float> smoothedMag(kFftSize / 2 + 1, 0.0f);
    constexpr int smoothHalfWin = 16;

    while (pos + kFftSize <= end)
    {
        if (!reader->read(&window, 0, kFftSize, pos, true, true))
            break;

        for (int i = 0; i < kFftSize; ++i)
        {
            float s = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
                s += window.getReadPointer(ch)[i];
            fftData[(size_t)i] = (s / (float)juce::jmax(1, numCh)) * hann[(size_t)i];
        }

        fft.performFrequencyOnlyForwardTransform(fftData.data(), true);

        // Suavizado del piso espectral (para calcular prominencia de picos melódicos)
        float runningSum = 0.0f;
        for (int k = 1; k <= juce::jmin(smoothHalfWin * 2, kFftSize / 2); ++k)
            runningSum += fftData[(size_t)k];

        for (int k = 1; k <= kFftSize / 2; ++k)
        {
            int addK = k + smoothHalfWin;
            int remK = k - smoothHalfWin - 1;
            if (addK <= kFftSize / 2) runningSum += fftData[(size_t)addK];
            if (remK >= 1)           runningSum -= fftData[(size_t)remK];
            int winLen = juce::jmin(kFftSize / 2, k + smoothHalfWin) - juce::jmax(1, k - smoothHalfWin) + 1;
            smoothedMag[(size_t)k] = runningSum / (float)juce::jmax(1, winLen);
        }

        for (int k = 1; k <= kFftSize / 2; ++k)
        {
            double freq = (double)k * sr / (double)kFftSize;
            if (freq < 35.0 || freq > 2800.0)
                continue;

            float rawMag = fftData[(size_t)k];
            float noiseFloor = smoothedMag[(size_t)k];

            // Prominencia espectral (filtrado de transientes y percusión)
            float prominence = (rawMag > noiseFloor * 1.25f) ? (rawMag - noiseFloor) : (rawMag * 0.15f);
            if (prominence <= 0.0f)
                continue;

            double midiPitch = 69.0 + 12.0 * std::log2(freq / adjustedA4);
            int pc = (int)std::lround(midiPitch) % 12;
            if (pc < 0) pc += 12;

            // Peso gaussiano según proximidad al centro del semitono
            double dist = std::abs(midiPitch - std::round(midiPitch));
            double noteWeight = std::exp(-0.5 * (dist / 0.22) * (dist / 0.22));

            double energy = (double)prominence * noteWeight;

            // Clasificación por bandas
            if (freq <= 220.0)
            {
                // Banda de Bajos: Fundamental directa
                bassChroma[pc] += energy * 1.5;
            }
            else
            {
                // Banda Armónica: Acordes, voz y melodía
                harmChroma[pc] += energy;

                // Contribución armónica compensada (HPCP: plegado de armónicos sobretonales)
                // El 3er armónico (19 semitonos = octava + quinta) refuerza la tónica real
                int subFifthPc = (pc - 7 + 12) % 12;
                harmChroma[subFifthPc] += energy * 0.22;
            }
        }

        ++windows;
        pos += kHop;
    }

    if (windows < 4)
        return result;

    // Normalizar cromagramas
    double maxH = 0.0, maxB = 0.0;
    for (int i = 0; i < 12; ++i)
    {
        maxH = juce::jmax(maxH, harmChroma[i]);
        maxB = juce::jmax(maxB, bassChroma[i]);
    }

    if (maxH <= 0.0)
        return result;

    for (int i = 0; i < 12; ++i)
    {
        harmChroma[i] /= maxH;
        if (maxB > 0.0)
            bassChroma[i] /= maxB;
    }

    // -------------------------------------------------------------------------
    // PASO 3: Correlación Armónica Sha'ath + Fusión de Tónica del Bajo
    // -------------------------------------------------------------------------
    double bestScore = -999.0, secondScore = -999.0;
    int bestRoot = -1;
    bool bestMinor = true;

    for (int root = 0; root < 12; ++root)
    {
        for (int m = 0; m < 2; ++m) // m=0: Mayor, m=1: Menor
        {
            const float* tmpl = (m == 0) ? kShaathMajor : kShaathMinor;
            double corr = 0.0, sumC = 0.0, sumT = 0.0, sumCC = 0.0, sumTT = 0.0;

            for (int i = 0; i < 12; ++i)
            {
                double c = harmChroma[i];
                double t = (double)tmpl[(i - root + 12) % 12];
                corr += c * t;
                sumC += c;
                sumT += t;
                sumCC += c * c;
                sumTT += t * t;
            }

            double denom = std::sqrt((sumCC - sumC * sumC / 12.0) * (sumTT - sumT * sumT / 12.0));
            double r = (denom > 1e-9) ? (corr - sumC * sumT / 12.0) / denom : -1.0;

            // Ponderación de la Tónica en el Bajo (Elimina la confusión Mayor vs Menor relativa)
            // Si la canción es Am (root 9), el bajo tiene alta energía en pc 9.
            // Si es C (root 0), el bajo tiene alta energía en pc 0.
            double bassBoost = 1.0 + (0.55 * bassChroma[root]);
            double finalScore = r * bassBoost;

            if (finalScore > bestScore)
            {
                secondScore = bestScore;
                bestScore = finalScore;
                bestRoot = root;
                bestMinor = (m == 1);
            }
            else if (finalScore > secondScore)
            {
                secondScore = finalScore;
            }
        }
    }

    if (bestRoot < 0)
        return result;

    result.rootPc = bestRoot;
    result.isMinor = bestMinor;
    result.camelot = camelotFor(bestRoot, bestMinor);
    result.musicalKey = musicalKeyFor(bestRoot, bestMinor);
    result.formattedKey = formatFullKey(bestRoot, bestMinor);
    result.confidence = (float)juce::jlimit(0.0, 1.0, (bestScore - secondScore) / juce::jmax(0.12, std::abs(bestScore)));

    return result;
}

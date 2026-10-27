#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <vector>

// Picos finos (~6ms/punto) de una ventana local de un archivo de audio, con
// caché de margen. Es la base para el wave scrolling con precisión de Serato:
// la extracción de picos del reproductor es de resolución fija (8000 puntos),
// que en sets largos aplana todo en un bloque; aquí la resolución es por
// muestras (256/punto), independiente de la duración de la pista.
class WindowPeakExtractor
{
public:
    WindowPeakExtractor();

    // Abre un reader propio sobre el archivo (independiente del reproductor).
    bool open(const juce::File& audioFile);
    void close();
    bool isOpen() const { return reader != nullptr; }

    // Garantiza datos para [centerSec - halfWindow, centerSec + halfWindow];
    // solo re-extrae cuando la ventana visible sale de la zona cacheada.
    void ensureWindow(double centerSec, double halfWindowSec);

    bool hasData() const { return !peaks.empty(); }
    double dataStartSec() const { return dataStart; }
    double bucketSeconds() const { return bucketSec; }
    const std::vector<float>& getPeaks() const { return peaks; }
    const std::vector<float>& getAverages() const { return averages; }

    static constexpr int samplesPerBucket = 256;
    static constexpr double cacheMarginSec = 8.0;

private:
    void extract(double startSec, double numSecs);

    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReader> reader;
    std::vector<float> peaks;
    std::vector<float> averages;
    double dataStart { 0.0 };
    double dataEnd { 0.0 };
    double bucketSec { 0.0 };
};

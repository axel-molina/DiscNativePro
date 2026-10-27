#include "DeckScrollWaveform.h"
#include "../services/WaveformTheme.h"
#include <cmath>

namespace
{
// Curva tipo dB: expande las diferencias de pistas comprimidas/limitadas
// (amplitudes pegadas a 1.0) para que los transientes se distingan.
inline float toDisplayAmplitude (float v)
{
    return std::pow (juce::jlimit (0.0f, 1.0f, v), 0.55f);
}
} // namespace

DeckScrollWaveform::DeckScrollWaveform(DeckPlayer& player, int deckNum, juce::Colour deckAccent)
    : deck(player), deckIndex(deckNum), accent(deckAccent)
{
    startTimerHz(60);
}

DeckScrollWaveform::~DeckScrollWaveform()
{
    stopTimer();
}

void DeckScrollWaveform::timerCallback()
{
    if (deck.isPlaying())
    {
        ensureFinePeaks();
        repaint();
    }
}

void DeckScrollWaveform::ensureFinePeaks()
{
    const auto file = deck.getCurrentFile();
    if (file != extractorFile)
    {
        extractorFile = file;
        if (file.existsAsFile())
            extractor.open(file);
        else
            extractor.close();
    }

    if (extractor.isOpen())
        extractor.ensureWindow(deck.getPosition(), visibleWindowSecs * 0.5);
}

void DeckScrollWaveform::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour(juce::Colour::fromRGB(14, 15, 19));
    g.fillRect(bounds);

    const double totalDur = deck.getLengthInSeconds();
    if (!deck.isLoaded() || totalDur <= 0.0)
        return;

    const double currentPos = deck.getPosition();
    const double pixelsPerSec = (double)bounds.getWidth() / visibleWindowSecs;
    const double halfWindow = visibleWindowSecs * 0.5;
    const float centerX = bounds.getCentreX();

    // Cuadrícula de beats al BPM real de la pista (fallback 124)
    const double bpm = deck.getBpm() > 1.0 ? deck.getBpm() : 124.0;
    const double beatInterval = 60.0 / bpm;
    const double firstBeatTime = std::floor((currentPos - halfWindow) / beatInterval) * beatInterval;

    g.setColour(juce::Colour::fromRGB(45, 52, 68));
    for (double bt = firstBeatTime; bt <= currentPos + halfWindow; bt += beatInterval)
    {
        if (bt >= 0.0 && bt <= totalDur)
        {
            float gridX = centerX + (float)((bt - currentPos) * pixelsPerSec);
            g.drawVerticalLine((int)gridX, bounds.getY(), bounds.getBottom());
        }
    }

    ensureFinePeaks();
    if (extractor.hasData())
        drawFineWave(g, bounds, currentPos);
    else
        drawOverviewFallback(g, bounds, currentPos);

    // Playhead centrado
    g.setColour(accent.withAlpha(0.95f));
    g.drawVerticalLine((int)centerX, bounds.getY(), bounds.getBottom());
}

void DeckScrollWaveform::drawFineWave(juce::Graphics& g, juce::Rectangle<float> area, double currentPos)
{
    const auto& pk = extractor.getPeaks();
    const auto& av = extractor.getAverages();
    const double dataStart = extractor.dataStartSec();
    const double bucketSec = extractor.bucketSeconds();
    const float height = area.getHeight();
    const float baselineY = area.getBottom();
    const float centerX = area.getCentreX();
    const double pixelsPerSec = (double)area.getWidth() / visibleWindowSecs;
    const int stepPixels = 2;
    const size_t numBuckets = pk.size();

    for (float x = area.getX(); x < area.getRight(); x += (float)stepPixels)
    {
        const double timeA = currentPos + (double)(x - centerX) / pixelsPerSec;
        const double timeB = currentPos + (double)(x + stepPixels - centerX) / pixelsPerSec;

        size_t b0 = (size_t) juce::jmax (0.0, (timeA - dataStart) / bucketSec);
        size_t b1 = (size_t) juce::jmax (0.0, (timeB - dataStart) / bucketSec);
        if (b0 >= numBuckets)
            continue;
        b1 = juce::jmin (b1 + 1, numBuckets);

        float peak = 0.0f, avg = 0.0f;
        int avgCount = 0;
        for (size_t b = b0; b < b1; ++b)
        {
            peak = juce::jmax (peak, pk[b]);
            avg += av[b];
            ++avgCount;
        }
        if (avgCount > 0)
            avg /= (float) avgCount;

        // Bandas (misma semántica que la extracción del reproductor) + curva dB
        const float low = toDisplayAmplitude (peak * 0.95f);
        const float mid = toDisplayAmplitude (juce::jmin (1.0f, avg * 1.5f));
        const float high = toDisplayAmplitude (juce::jmin (1.0f, (peak - avg) * 1.25f));

        g.setColour(WaveformTheme::low(deckIndex));
        g.fillRect(x, baselineY, (float)stepPixels, -low * height * 0.85f);
        g.setColour(WaveformTheme::mid(deckIndex));
        g.fillRect(x, baselineY, (float)stepPixels, -mid * height * 0.75f);
        g.setColour(WaveformTheme::high(deckIndex));
        g.fillRect(x, baselineY, (float)stepPixels, -high * height * 0.65f);

        // Línea de pico fino sobre el cuerpo (estilo Serato)
        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.fillRect(x, baselineY - low * height * 0.85f - 1.5f, (float)stepPixels, 1.5f);
    }
}

void DeckScrollWaveform::drawOverviewFallback(juce::Graphics& g, juce::Rectangle<float> area, double currentPos)
{
    const auto& low = deck.getPeaksLow();
    const auto& mid = deck.getPeaksMid();
    const auto& high = deck.getPeaksHigh();
    if (low.empty())
        return;

    const size_t numPeaks = low.size();
    const double totalDur = deck.getLengthInSeconds();
    const float height = area.getHeight();
    const float baselineY = area.getBottom();
    const float centerX = area.getCentreX();
    const double pixelsPerSec = (double)area.getWidth() / visibleWindowSecs;
    const int stepPixels = 2;

    for (float x = area.getX(); x < area.getRight(); x += (float)stepPixels)
    {
        const double trackTime = currentPos + (double)(x - centerX) / pixelsPerSec;
        if (trackTime < 0.0 || trackTime > totalDur)
            continue;

        size_t idx = static_cast<size_t>((trackTime / totalDur) * (double)numPeaks);
        if (idx >= numPeaks)
            idx = numPeaks - 1;

        const float lAmp = toDisplayAmplitude (low[idx]) * height * 0.9f;
        const float mAmp = toDisplayAmplitude (mid[idx]) * height * 0.75f;
        const float hAmp = toDisplayAmplitude (high[idx]) * height * 0.65f;

        g.setColour(WaveformTheme::low(deckIndex));
        g.fillRect(x, baselineY, (float)stepPixels, -lAmp * 0.85f);
        g.setColour(WaveformTheme::mid(deckIndex));
        g.fillRect(x, baselineY, (float)stepPixels, -mAmp * 0.75f);
        g.setColour(WaveformTheme::high(deckIndex));
        g.fillRect(x, baselineY, (float)stepPixels, -hAmp * 0.65f);
    }
}

// Scrub estilo plato de vinilo (misma semántica que JogWheel): tocar la onda
// "agarra" la aguja — el movimiento horizontal suena como scratch, incluido el
// reposo (quieto = aguja detenida) y el retroceso. En pausa entra en "audición"
// para que el gesto suene; al soltar vuelve a pausa.

void DeckScrollWaveform::mouseDown(const juce::MouseEvent& e)
{
    if (!deck.isLoaded())
        return;

    auditioning = false;
    if (!deck.isPlaying())
    {
        deck.play();
        auditioning = true;
    }

    scrubbing = true;
    smoothRevPerSec = 0.0f;
    deck.beginScratch();
    lastMouseX = (float)e.x;
    lastMoveMs = juce::Time::getMillisecondCounter();
}

void DeckScrollWaveform::mouseDrag(const juce::MouseEvent& e)
{
    if (!scrubbing)
        return;

    const juce::uint32 now = juce::Time::getMillisecondCounter();
    const double dt = (double)(now - lastMoveMs) / 1000.0;
    const float dx = (float)e.x - lastMouseX;
    lastMouseX = (float)e.x;
    lastMoveMs = now;

    if (dt <= 0.0)
        return;

    // Mapeo 1:1 con la onda: arrastrar un ancho de ventana = 6s de audio.
    // La API del plato usa rev/s donde 1 rev/s = 1.8 audio-segundos por segundo.
    const double pixelsPerSec = (double)juce::jmax(1, getWidth()) / visibleWindowSecs;
    const float instRevPerSec = (float)(((double)dx / pixelsPerSec) / dt / 1.8);
    smoothRevPerSec += (instRevPerSec - smoothRevPerSec) * 0.35f;
    deck.setScratchVelocity(juce::jlimit(-10.0f, 10.0f, smoothRevPerSec));
}

void DeckScrollWaveform::mouseUp(const juce::MouseEvent&)
{
    if (scrubbing)
    {
        deck.setScratchVelocity(0.0f);
        deck.endScratch();
        scrubbing = false;
    }
    if (auditioning)
    {
        deck.pause();
        auditioning = false;
    }
}

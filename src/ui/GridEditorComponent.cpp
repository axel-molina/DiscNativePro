#include "GridEditorComponent.h"
#include "../audio/AudioEngine.h"
#include "../services/LibraryCacheManager.h"
#include "../services/LanguageManager.h"
#include "../services/WaveformTheme.h"

GridEditorComponent::GridEditorComponent(DeckPlayer& player, AudioEngine& engineRef, int deckNumber)
    : deck(player), engine(engineRef), deckIdx(deckNumber),
      wasPlayingOnOpen(player.isPlaying())
{
    auto neutral = [](DjButton& b, float fs) {
        b.setFontSize(fs);
        b.setCornerRadius(4.0f);
        b.setCustomColours(juce::Colour::fromRGB(24, 27, 35),
                           juce::Colours::white,
                           juce::Colour::fromRGB(142, 149, 165),
                           juce::Colour::fromRGB(38, 43, 56));
    };

    auto wire = [this](DjButton& b, std::function<void()> fn) {
        b.onClick = [this, fn]() { fn(); persistGrid(); };
        addAndMakeVisible(b);
    };

    // Fila de offset (ms)
    neutral(nudgeBackFarButton, 10.0f);
    nudgeBackFarButton.setText("-20");
    wire(nudgeBackFarButton, [this]() {
        deck.setFirstBeatOffset(deck.getFirstBeatOffset() - 0.020);
        repaint();
    });

    neutral(nudgeBackButton, 10.0f);
    nudgeBackButton.setText("-5");
    wire(nudgeBackButton, [this]() {
        deck.setFirstBeatOffset(deck.getFirstBeatOffset() - 0.005);
        repaint();
    });

    neutral(nudgeFwdButton, 10.0f);
    nudgeFwdButton.setText("+5");
    wire(nudgeFwdButton, [this]() {
        deck.setFirstBeatOffset(deck.getFirstBeatOffset() + 0.005);
        repaint();
    });

    neutral(nudgeFwdFarButton, 10.0f);
    nudgeFwdFarButton.setText("+20");
    wire(nudgeFwdFarButton, [this]() {
        deck.setFirstBeatOffset(deck.getFirstBeatOffset() + 0.020);
        repaint();
    });

    // Fila de BPM
    neutral(bpmDownFarButton, 10.0f);
    bpmDownFarButton.setText("-0.5");
    wire(bpmDownFarButton, [this]() {
        if (deck.getOriginalBpm() > 0.0) deck.setGridBpm(deck.getOriginalBpm() - 0.5);
        repaint();
    });

    neutral(bpmDownButton, 10.0f);
    bpmDownButton.setText("-0.05");
    wire(bpmDownButton, [this]() {
        if (deck.getOriginalBpm() > 0.0) deck.setGridBpm(deck.getOriginalBpm() - 0.05);
        repaint();
    });

    neutral(bpmUpButton, 10.0f);
    bpmUpButton.setText("+0.05");
    wire(bpmUpButton, [this]() {
        if (deck.getOriginalBpm() > 0.0) deck.setGridBpm(deck.getOriginalBpm() + 0.05);
        repaint();
    });

    neutral(bpmUpFarButton, 10.0f);
    bpmUpFarButton.setText("+0.5");
    wire(bpmUpFarButton, [this]() {
        if (deck.getOriginalBpm() > 0.0) deck.setGridBpm(deck.getOriginalBpm() + 0.5);
        repaint();
    });

    neutral(bpmHalfButton, 10.0f);
    bpmHalfButton.setText(juce::String::fromUTF8("×½"));
    wire(bpmHalfButton, [this]() {
        if (deck.getOriginalBpm() > 0.0) deck.setGridBpm(deck.getOriginalBpm() / 2.0);
        repaint();
    });

    neutral(bpmDoubleButton, 10.0f);
    bpmDoubleButton.setText(juce::String::fromUTF8("×2"));
    wire(bpmDoubleButton, [this]() {
        if (deck.getOriginalBpm() > 0.0) deck.setGridBpm(deck.getOriginalBpm() * 2.0);
        repaint();
    });

    // Fila inferior
    neutral(resetButton, 10.0f);
    resetButton.setText("RESET");
    resetButton.setCustomColours(juce::Colour::fromRGB(70, 34, 20),
                                 juce::Colour::fromRGB(245, 158, 11),
                                 juce::Colour::fromRGB(245, 158, 11),
                                 juce::Colour::fromRGB(245, 158, 11).withAlpha(0.4f));
    wire(resetButton, [this]() {
        deck.resetGrid();
        repaint();
    });
    resetButton.setTooltip(LanguageManager::trUtf8("Volver a los valores detectados por el análisis", "Back to auto-detected values"));

    neutral(playButton, 10.5f);
    playButton.setText("PLAY");
    playButton.onClick = [this]() {
        playAudition = !playAudition;
        if (playAudition)
        {
            engine.ensureAudioDeviceRunning();
            // Si el deck ya venía reproduciendo, la audición continúa desde la
            // posición actual; solo reposiciona si estaba detenido.
            if (!deck.isPlaying())
                deck.setPosition(juce::jmax(0.0, deck.getFirstBeatOffset() - 1.0), true);
            deck.play();
        }
        else
        {
            deck.pause();
        }
        refreshLabels();
    };
    addAndMakeVisible(playButton);

    neutral(closeButton, 10.5f);
    closeButton.setText("CERRAR");
    closeButton.setCustomColours(juce::Colour::fromRGB(37, 99, 235),
                                 juce::Colours::white,
                                 juce::Colours::white,
                                 juce::Colour::fromRGB(59, 130, 246));
    // CERRAR no re-persiste via wire: el destructor/flujo de cierre lo hace.
    closeButton.onClick = [this]() {
        if (onClosed)
            onClosed();
    };
    addAndMakeVisible(closeButton);

    offsetValueLabel.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    offsetValueLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    offsetValueLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(offsetValueLabel);

    bpmValueLabel.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    bpmValueLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    bpmValueLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(bpmValueLabel);

    helpLabel.setFont(juce::FontOptions(9.5f));
    helpLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(142, 149, 165));
    helpLabel.setJustificationType(juce::Justification::centred);
    helpLabel.setText(LanguageManager::trUtf8(
                          "Arrastra la onda hasta que el marcador central caiga sobre el primer golpe; las líneas deben caer en cada beat.",
                          "Drag the waveform until the center marker lands on the first hit; lines should fall on every beat."),
                      juce::dontSendNotification);
    addAndMakeVisible(helpLabel);

    refreshLabels();
    startTimerHz(10);
}

GridEditorComponent::~GridEditorComponent()
{
    stopTimer();
    // Solo detiene la audición si el editor la inició; si el deck ya venía
    // reproduciendo, el cierre no debe pausarlo.
    if (playAudition && deck.isPlaying() && !wasPlayingOnOpen)
        deck.pause();
    persistGrid();
}

juce::Rectangle<int> GridEditorComponent::canvasRect() const
{
    auto area = getLocalBounds().reduced(14, 10);
    area.removeFromTop(8);
    auto helpRow = area.removeFromBottom(16);
    juce::ignoreUnused(helpRow);
    auto controls = area.removeFromBottom(106);
    juce::ignoreUnused(controls);
    return area; // el resto superior es el canvas
}

void GridEditorComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colour::fromRGB(20, 22, 30));
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(juce::Colour::fromRGB(40, 45, 58));
    g.drawRoundedRectangle(bounds, 8.0f, 1.2f);

    const auto canvas = canvasRect().toFloat();
    g.setColour(juce::Colour::fromRGB(12, 14, 19));
    g.fillRoundedRectangle(canvas, 6.0f);
    g.setColour(juce::Colour::fromRGB(38, 43, 56));
    g.drawRoundedRectangle(canvas, 6.0f, 1.0f);

    if (!deck.isLoaded() || deck.getLengthInSeconds() <= 0.0)
        return;

    const auto& low = deck.getPeaksLow();
    const auto& mid = deck.getPeaksMid();
    const auto& high = deck.getPeaksHigh();
    const size_t numPeaks = low.size();
    if (numPeaks == 0)
        return;

    const double totalDur = deck.getLengthInSeconds();
    const double offset = deck.getFirstBeatOffset();
    const double interval = deck.getBpm() > 0.0 ? 60.0 / deck.getBpm() : 0.0;

    // --- Peaks: 3 bandas apiladas simétricas (como el overview vertical). ---
    const float cx = canvas.getCentreX();
    const float midY = canvas.getCentreY();
    const float maxHalfAmp = canvas.getHeight() * 0.5f - 14.0f;

    for (int x = 0; x < (int)canvas.getWidth(); ++x)
    {
        double t = offset + ((double)x - (canvas.getWidth() * 0.5)) / pixelsPerSecond;
        if (t < 0.0 || t > totalDur)
            continue;

        double fIdx = (t / totalDur) * (double)numPeaks;
        size_t i0 = (size_t)fIdx;
        if (i0 >= numPeaks) i0 = numPeaks - 1;
        size_t i1 = (i0 + 1 < numPeaks) ? i0 + 1 : i0;
        float fr = (float)(fIdx - (double)i0);

        float lowV  = low[i0]  + fr * (low[i1]  - low[i0]);
        float midV  = mid[i0]  + fr * (mid[i1]  - mid[i0]);
        float highV = high[i0] + fr * (high[i1] - high[i0]);

        const float px = canvas.getX() + (float)x;

        g.setColour(WaveformTheme::low(deckIdx));
        g.fillRect(px, midY - lowV * maxHalfAmp, 1.0f, lowV * maxHalfAmp * 2.0f);

        g.setColour(WaveformTheme::mid(deckIdx));
        g.fillRect(px, midY - midV * maxHalfAmp * 0.7f, 1.0f, midV * maxHalfAmp * 1.4f);

        g.setColour(WaveformTheme::high(deckIdx));
        g.fillRect(px, midY - highV * maxHalfAmp * 0.45f, 1.0f, highV * maxHalfAmp * 0.9f);
    }

    // --- Líneas de beat (grid actual), downbeat cada 4 con número de compás. ---
    if (interval > 0.0)
    {
        const double tLeft = offset - (canvas.getWidth() * 0.5) / pixelsPerSecond;
        const double tRight = offset + (canvas.getWidth() * 0.5) / pixelsPerSecond;
        g.setFont(juce::FontOptions(9.5f, juce::Font::bold));

        int k0 = (int)std::floor((tLeft - offset) / interval) - 1;
        int k1 = (int)std::ceil((tRight - offset) / interval) + 1;
        for (int k = k0; k <= k1; ++k)
        {
            double beatTime = offset + (double)k * interval;
            if (beatTime < 0.0 || beatTime > totalDur)
                continue;
            float x = cx + (float)((beatTime - offset) * pixelsPerSecond);
            if (x < canvas.getX() || x > canvas.getRight())
                continue;

            const bool isBar = (((k % 4) + 4) % 4) == 0;
            g.setColour(isBar ? juce::Colours::white.withAlpha(0.55f)
                              : juce::Colours::white.withAlpha(0.18f));
            g.drawVerticalLine((int)x, canvas.getY(), canvas.getBottom());

            if (isBar)
            {
                g.setColour(juce::Colours::white.withAlpha(0.85f));
                const int measure = (k >= 0 ? k : k - 3) / 4 + 1;
                g.drawText(juce::String(measure),
                           (int)x - 18, (int)canvas.getY() + 2, 36, 12,
                           juce::Justification::centred, false);
            }
        }
    }

    // --- Marcador del primer beat (centro fijo). ---
    g.setColour(juce::Colour::fromRGB(245, 158, 11));
    g.drawVerticalLine((int)cx, canvas.getY(), canvas.getBottom());
    juce::Path tri;
    tri.addTriangle(cx - 6.0f, canvas.getY(), cx + 6.0f, canvas.getY(), cx, canvas.getY() + 9.0f);
    g.fillPath(tri);
}

void GridEditorComponent::mouseDown(const juce::MouseEvent& e)
{
    if (canvasRect().contains(e.getPosition()))
        lastDragX = e.getPosition().x;
}

void GridEditorComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (!canvasRect().contains(e.getPosition()) || lastDragX < 0)
        return;
    const int dx = e.getPosition().x - lastDragX;
    lastDragX = e.getPosition().x;
    if (dx == 0)
        return;
    // Arrastrar a la derecha mueve la onda a la derecha => el tiempo del
    // centro (offset) disminuye.
    deck.setFirstBeatOffset(deck.getFirstBeatOffset() - (double)dx / pixelsPerSecond);
    repaint();
    refreshLabels();
}

void GridEditorComponent::mouseUp(const juce::MouseEvent&)
{
    lastDragX = -1;
    persistGrid();
}

void GridEditorComponent::timerCallback()
{
    refreshLabels();
}

void GridEditorComponent::refreshLabels()
{
    const double offset = deck.getFirstBeatOffset();
    offsetValueLabel.setText(juce::String::formatted("Offset: %d ms", (int)std::round(offset * 1000.0)),
                             juce::dontSendNotification);
    const double bpm = deck.getOriginalBpm();
    bpmValueLabel.setText(bpm > 0.0 ? juce::String::formatted("Grid BPM: %.2f", bpm)
                                    : "Grid BPM: --",
                          juce::dontSendNotification);

    // Estado de la audición
    if (playAudition && !deck.isPlaying())
        playAudition = false;
    if (playAudition)
    {
        playButton.setText("STOP");
        playButton.setCustomColours(juce::Colour::fromRGB(30, 215, 96),
                                    juce::Colours::black,
                                    juce::Colours::black,
                                    juce::Colour::fromRGB(38, 230, 108));
    }
    else
    {
        playButton.setText("PLAY");
        playButton.setCustomColours(juce::Colour::fromRGB(24, 27, 35),
                                    juce::Colours::white,
                                    juce::Colour::fromRGB(30, 215, 96),
                                    juce::Colour::fromRGB(38, 43, 56));
    }
}

void GridEditorComponent::persistGrid()
{
    if (!deck.isLoaded())
        return;
    const auto file = deck.getCurrentFile();
    if (!file.existsAsFile())
        return;
    LibraryCacheManager::getInstance().updateGrid(file, deck.getOriginalBpm(), deck.getFirstBeatOffset());
}

void GridEditorComponent::resized()
{
    auto area = getLocalBounds().reduced(14, 10);
    area.removeFromTop(8);

    helpLabel.setBounds(area.removeFromBottom(16));
    auto controls = area.removeFromBottom(106);

    // Fila 3: RESET | PLAY | CERRAR
    auto row3 = controls.removeFromBottom(30);
    closeButton.setBounds(row3.removeFromRight(90).reduced(3, 0));
    row3.removeFromRight(12);
    playButton.setBounds(row3.removeFromRight(80).reduced(3, 0));
    row3.removeFromRight(12);
    resetButton.setBounds(row3.removeFromRight(80).reduced(3, 0));
    controls.removeFromBottom(8);

    // Fila 2: BPM
    auto row2 = controls.removeFromBottom(30);
    bpmValueLabel.setBounds(row2.removeFromLeft(120));
    bpmUpFarButton.setBounds(row2.removeFromRight(52).reduced(3, 0));
    bpmUpButton.setBounds(row2.removeFromRight(56).reduced(3, 0));
    bpmDownButton.setBounds(row2.removeFromRight(56).reduced(3, 0));
    bpmDownFarButton.setBounds(row2.removeFromRight(52).reduced(3, 0));
    row2.removeFromRight(12);
    bpmDoubleButton.setBounds(row2.removeFromRight(42).reduced(3, 0));
    bpmHalfButton.setBounds(row2.removeFromRight(42).reduced(3, 0));
    controls.removeFromBottom(8);

    // Fila 1: offset
    auto row1 = controls.removeFromBottom(30);
    offsetValueLabel.setBounds(row1.removeFromLeft(120));
    nudgeFwdFarButton.setBounds(row1.removeFromRight(52).reduced(3, 0));
    nudgeFwdButton.setBounds(row1.removeFromRight(56).reduced(3, 0));
    nudgeBackButton.setBounds(row1.removeFromRight(56).reduced(3, 0));
    nudgeBackFarButton.setBounds(row1.removeFromRight(52).reduced(3, 0));

    area.removeFromBottom(6);
    // El resto es el canvas (pintado en paint(), sin hijo).
}

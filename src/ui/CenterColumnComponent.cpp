#include "CenterColumnComponent.h"

CenterColumnComponent::CenterColumnComponent(AudioEngine& engine)
    : audioEngine(engine),
      mixer(engine.getMixer()),
      verticalWaveform(engine.getDeck(0), engine.getDeck(1))
{

    // Top Controls: Master, Meter, Phones
    masterKnob.setRange(0.0, 1.5, 0.01);
    masterKnob.setValue(1.0);
    masterKnob.onValueChange = [this]() { mixer.setMasterVolume((float)masterKnob.getValue()); };
    addAndMakeVisible(masterKnob);

    addAndMakeVisible(masterMeter);

    phonesKnob.setRange(0.0, 1.5, 0.01);
    phonesKnob.setValue(1.0);
    phonesKnob.onValueChange = [this]() { mixer.setPhonesVolume((float)phonesKnob.getValue()); };
    addAndMakeVisible(phonesKnob);

    loudnessLabel.setFont(juce::FontOptions(9.5f, juce::Font::bold));
    loudnessLabel.setJustificationType(juce::Justification::centred);
    loudnessLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(142, 149, 165));
    addAndMakeVisible(loudnessLabel);

    // Middle: Dual Vertical Waveform
    addAndMakeVisible(verticalWaveform);

    // Bottom: Crossfader header controls
    deck1Label.setText("1", juce::dontSendNotification);
    deck1Label.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    deck1Label.setColour(juce::Label::textColourId, juce::Colour::fromRGB(59, 130, 246));
    deck1Label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(deck1Label);

    curveButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(24, 27, 35));
    curveButton.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(142, 149, 165));
    curveButton.onClick = [this]() {
        if (curveButton.getButtonText() == "SMOOTH") curveButton.setButtonText("SCRATCH");
        else if (curveButton.getButtonText() == "SCRATCH") curveButton.setButtonText("LINEAR");
        else curveButton.setButtonText("SMOOTH");
    };
    addAndMakeVisible(curveButton);

    tempoBlendButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(24, 27, 35));
    tempoBlendButton.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(142, 149, 165));
    tempoBlendButton.setClickingTogglesState(true);
    tempoBlendButton.onClick = [this]() {
        // El boton solo ordena al motor; la parte visual la resuelve syncTempoBlendState()
        // leyendo isTempoBlendActive(), para que AutoMix no pueda dejarlo mintiendo.
        audioEngine.setTempoBlend(tempoBlendButton.getToggleState());
        syncTempoBlendState();
    };
    addAndMakeVisible(tempoBlendButton);

    deck2Label.setText("2", juce::dontSendNotification);
    deck2Label.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    deck2Label.setColour(juce::Label::textColourId, juce::Colour::fromRGB(203, 213, 225));
    deck2Label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(deck2Label);

    // Crossfader
    crossfader.setLookAndFeel(&djLookAndFeel);
    crossfader.setSliderStyle(juce::Slider::LinearHorizontal);
    crossfader.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    crossfader.setRange(-1.0, 1.0, 0.01);
    crossfader.setValue(0.0);
    crossfader.setDoubleClickReturnValue(true, 0.0);
    crossfader.onValueChange = [this]() {
        audioEngine.setCrossfader((float)crossfader.getValue());
    };
    addAndMakeVisible(crossfader);

    syncTempoBlendState();
    startTimerHz(60);
}

CenterColumnComponent::~CenterColumnComponent()
{
    stopTimer();
    crossfader.setLookAndFeel(nullptr);
}

void CenterColumnComponent::timerCallback()
{
    masterMeter.setLevels(mixer.getMasterPeakLeft(), mixer.getMasterPeakRight());

    // Sincronizar perillas MASTER y PHONES con el estado del mezclador (MIDI / Shortcuts)
    if (!masterKnob.isMouseButtonDown())
    {
        masterKnob.setValue(mixer.getMasterVolume(), juce::dontSendNotification);
    }

    if (!phonesKnob.isMouseButtonDown())
    {
        phonesKnob.setValue(mixer.getPhonesVolume(), juce::dontSendNotification);
    }

    // Synchronize crossfader if nudged via shortcuts / MIDI
    if (!crossfader.isMouseButtonDown())
    {
        crossfader.setValue(mixer.getCrossfader(), juce::dontSendNotification);
    }

    // El motor es la unica verdad del Tempo Blend (AutoMix tambien lo cambia por codigo)
    syncTempoBlendState();

    // Loudness (LUFS) y gain reduction del limiter master
    const auto& limiter = audioEngine.getMasterLimiter();
    const float lufsS = limiter.getLufsShortTerm();
    const float lufsM = limiter.getLufsMomentary();
    const float grDb = limiter.getGainReductionDb();
    const auto infinity = juce::String(juce::juce_wchar(0x221E));
    auto fmtLufs = [infinity](float v) {
        return (v <= -60.0f) ? juce::String("-") + infinity : juce::String(v, 1);
    };
    loudnessLabel.setText("LUFS-S " + fmtLufs(lufsS) + "   M " + fmtLufs(lufsM)
                              + "   GR " + juce::String(grDb, 1) + " dB",
                          juce::dontSendNotification);
    loudnessLabel.setColour(juce::Label::textColourId,
                            (grDb > 0.05f) ? juce::Colour::fromRGB(245, 158, 11)
                                           : juce::Colour::fromRGB(142, 149, 165));
}

void CenterColumnComponent::updateTempoBlendAppearance(bool active)
{
    tempoBlendButton.setColour(juce::TextButton::buttonColourId,
                               active ? juce::Colour::fromRGB(37, 99, 235).withAlpha(0.25f)
                                      : juce::Colour::fromRGB(24, 27, 35));
    tempoBlendButton.setColour(juce::TextButton::textColourOffId,
                               active ? juce::Colour::fromRGB(147, 197, 253)
                                      : juce::Colour::fromRGB(142, 149, 165));
}

juce::String CenterColumnComponent::buildTempoBlendTooltip() const
{
    juce::String tip = audioEngine.isTempoBlendActive()
        ? "Tempo Blend ON: las dos bandejas comparten el BPM segun el crossfader."
        : "Tempo Blend OFF: activalo para igualar el BPM de ambas bandejas con el crossfader.";

    // Motivos por los que applyTempoBlend queda en no-op (mismos requisitos que el motor)
    juce::StringArray missing;
    for (int i = 0; i < 2; ++i)
    {
        const auto& deck = audioEngine.getDeck(i);
        if (!deck.isLoaded())
            missing.add("la bandeja " + juce::String(i + 1) + " sin pista");
        else if (deck.getOriginalBpm() <= 0.0)
            missing.add("la bandeja " + juce::String(i + 1) + " sin BPM detectado");
    }

    if (!missing.isEmpty())
        tip += " No aplicable ahora: falta " + missing.joinIntoString(" y ") + ".";

    return tip;
}

void CenterColumnComponent::syncTempoBlendState()
{
    const bool active = audioEngine.isTempoBlendActive();

    // El motor es la unica verdad: AutoMix puede encender/apagar el blend sin pasar por el boton.
    // dontSendNotification evita re-disparar onClick (que invertiria el estado que acabamos de leer).
    if (tempoBlendButton.getToggleState() != active)
        tempoBlendButton.setToggleState(active, juce::dontSendNotification);

    // Firma barata del contexto (activo + pista/BPM por bandeja): solo se rehace el tooltip
    // y el repintado de colores cuando algo cambia de verdad, no 60 veces por segundo.
    int signature = active ? 1 : 0;
    for (int i = 0; i < 2; ++i)
    {
        const auto& deck = audioEngine.getDeck(i);
        const int readiness = deck.isLoaded() ? (deck.getOriginalBpm() > 0.0 ? 3 : 2) : 0;
        signature |= readiness << (1 + i * 2);
    }

    if (signature == tempoBlendSignature)
        return;

    tempoBlendSignature = signature;
    updateTempoBlendAppearance(active);
    tempoBlendButton.setTooltip(buildTempoBlendTooltip());
}

void CenterColumnComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Dark base background #0c0d12 with borders #1c202a
    g.fillAll(juce::Colour::fromRGB(12, 13, 18));

    g.setColour(juce::Colour::fromRGB(28, 32, 42));
    g.drawVerticalLine(0, 0.0f, bounds.getHeight());
    g.drawVerticalLine(getWidth() - 1, 0.0f, bounds.getHeight());

    // Top section background #12141a
    g.setColour(juce::Colour::fromRGB(18, 20, 26));
    g.fillRect(0, 0, getWidth(), 46);
    g.setColour(juce::Colour::fromRGB(31, 35, 46));
    g.drawHorizontalLine(46, 0.0f, (float)getWidth());

    // Bottom section background #12141a
    int bottomY = getHeight() - 64;
    g.setColour(juce::Colour::fromRGB(18, 20, 26));
    g.fillRect(0, bottomY, getWidth(), 64);
    g.setColour(juce::Colour::fromRGB(31, 35, 46));
    g.drawHorizontalLine(bottomY, 0.0f, (float)getWidth());
}

void CenterColumnComponent::resized()
{
    auto area = getLocalBounds();

    // 1. Top Section (62px): fila de knobs + readout de loudness
    auto topArea = area.removeFromTop(62).reduced(6, 2);
    auto knobRow = topArea.removeFromTop(46);
    // P19: fila 1-1 alrededor del meter (CEILING vive ahora en la Top Bar)
    masterKnob.setBounds(knobRow.removeFromLeft(48).reduced(0, 1));
    phonesKnob.setBounds(knobRow.removeFromRight(48).reduced(0, 1));
    masterMeter.setBounds(knobRow.reduced(8, 4));
    loudnessLabel.setBounds(topArea.reduced(4, 0));

    // 2. Bottom Section (64px)
    auto bottomArea = area.removeFromBottom(64).reduced(6, 4);

    // Crossfader header controls (1, SMOOTH, TEMPO BLEND, 2)
    auto ctrlRow = bottomArea.removeFromTop(20);
    deck1Label.setBounds(ctrlRow.removeFromLeft(16));
    deck2Label.setBounds(ctrlRow.removeFromRight(16));
    ctrlRow.reduce(4, 0);
    curveButton.setBounds(ctrlRow.removeFromLeft(54).reduced(0, 1));
    ctrlRow.removeFromLeft(4);
    tempoBlendButton.setBounds(ctrlRow.reduced(0, 1));

    bottomArea.removeFromTop(4);
    crossfader.setBounds(bottomArea.reduced(4, 0));

    // 3. Middle Section: Dual Vertical Waveform fills all remaining space!
    verticalWaveform.setBounds(area);
}

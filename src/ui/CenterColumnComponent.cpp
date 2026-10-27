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

void CenterColumnComponent::setWavesMode(bool active, juce::Slider* fader1, juce::Slider* fader2)
{
    if (externalFader1 != nullptr)
        removeChildComponent(externalFader1);
    if (externalFader2 != nullptr)
        removeChildComponent(externalFader2);

    wavesMode = active;
    externalFader1 = active ? fader1 : nullptr;
    externalFader2 = active ? fader2 : nullptr;

    verticalWaveform.setVisible(!active);
    if (externalFader1 != nullptr)
        addAndMakeVisible(externalFader1);
    if (externalFader2 != nullptr)
        addAndMakeVisible(externalFader2);

    resized();
    repaint();
}

void CenterColumnComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const int topH = wavesMode ? 40 : 46;
    const int bottomH = wavesMode ? 56 : 64;

    // Dark base background #0c0d12 with borders #1c202a
    g.fillAll(juce::Colour::fromRGB(12, 13, 18));

    g.setColour(juce::Colour::fromRGB(28, 32, 42));
    g.drawVerticalLine(0, 0.0f, bounds.getHeight());
    g.drawVerticalLine(getWidth() - 1, 0.0f, bounds.getHeight());

    // Top section background #12141a
    g.setColour(juce::Colour::fromRGB(18, 20, 26));
    g.fillRect(0, 0, getWidth(), topH);
    g.setColour(juce::Colour::fromRGB(31, 35, 46));
    g.drawHorizontalLine(topH, 0.0f, (float)getWidth());

    // Bottom section background #12141a
    int bottomY = getHeight() - bottomH;
    g.setColour(juce::Colour::fromRGB(18, 20, 26));
    g.fillRect(0, bottomY, getWidth(), bottomH);
    g.setColour(juce::Colour::fromRGB(31, 35, 46));
    g.drawHorizontalLine(bottomY, 0.0f, (float)getWidth());

    // Modo Ondas: separador vertical entre los dos faders de volumen
    if (wavesMode)
    {
        g.setColour(juce::Colour::fromRGB(31, 35, 46));
        g.drawVerticalLine(getWidth() / 2, (float)topH + 6.0f, (float)bottomY - 6.0f);
    }
}

void CenterColumnComponent::resized()
{
    auto area = getLocalBounds();
    const int topH = wavesMode ? 40 : 46;
    const int bottomH = wavesMode ? 56 : 64;

    // 1. Top Section
    auto topArea = area.removeFromTop(topH).reduced(6, 2);
    masterKnob.setBounds(topArea.removeFromLeft(48).reduced(0, 1));
    phonesKnob.setBounds(topArea.removeFromRight(48).reduced(0, 1));
    masterMeter.setBounds(topArea.reduced(8, 4));

    // 2. Bottom Section
    auto bottomArea = area.removeFromBottom(bottomH).reduced(6, 4);

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

    // 3. Middle Section: Dual Vertical Waveform, u ondas: faders de volumen
    // de cada bandeja lado a lado (la onda horizontal superior ya muestra la onda)
    if (wavesMode && externalFader1 != nullptr && externalFader2 != nullptr)
    {
        auto mid = area.reduced(6, 6);
        auto half = mid.getWidth() / 2;
        externalFader1->setBounds(mid.removeFromLeft(half).reduced(8, 2));
        mid.removeFromLeft(2);
        externalFader2->setBounds(mid.reduced(8, 2));
    }
    else
    {
        verticalWaveform.setBounds(area);
    }
}

#include "FxPanelComponent.h"
#include "../services/LanguageManager.h"

FxPanelComponent::FxPanelComponent(DjMixer& djMixer)
    : slot1_1(djMixer.getFxChain(0).getSlot(0)),
      slot1_2(djMixer.getFxChain(0).getSlot(1)),
      slot1_3(djMixer.getFxChain(0).getSlot(2)),
      slot2_1(djMixer.getFxChain(1).getSlot(0)),
      slot2_2(djMixer.getFxChain(1).getSlot(1)),
      slot2_3(djMixer.getFxChain(1).getSlot(2))
{
    // 1. Deck 1 FX slots
    addAndMakeVisible(slot1_1);
    addAndMakeVisible(slot1_2);
    addAndMakeVisible(slot1_3);

    // 2. Center FX Mode Controls
    fxTitleLabel.setText("FX", juce::dontSendNotification);
    fxTitleLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    fxTitleLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(148, 163, 184));
    fxTitleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(fxTitleLabel);

    manualBtn.setText("MANUAL");
    manualBtn.setFontSize(8.5f);
    manualBtn.setTooltip(LanguageManager::trUtf8("Modo manual: control de los efectos con precisión", "Manual mode: precise FX control"));
    manualBtn.setCornerRadius(3.0f);
    manualBtn.onClick = [this]() {
        isInstantMode = false;
        updateModeButtons();
    };
    addAndMakeVisible(manualBtn);

    instantBtn.setText("INSTANT");
    instantBtn.setFontSize(8.5f);
    instantBtn.setTooltip(LanguageManager::trUtf8("Modo instantáneo: control de los efectos con un clic", "Instant mode: one-click FX control"));
    instantBtn.setCornerRadius(3.0f);
    instantBtn.onClick = [this]() {
        isInstantMode = true;
        updateModeButtons();
    };
    addAndMakeVisible(instantBtn);

    updateModeButtons();

    // 3. Deck 2 FX slots
    addAndMakeVisible(slot2_1);
    addAndMakeVisible(slot2_2);
    addAndMakeVisible(slot2_3);
}

void FxPanelComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Outer container background
    g.setColour(juce::Colour::fromRGB(12, 14, 19));
    g.fillRoundedRectangle(bounds, 3.0f);

    // Outline
    g.setColour(juce::Colour::fromRGB(26, 30, 42));
    g.drawRoundedRectangle(bounds, 3.0f, 1.0f);
}

void FxPanelComponent::resized()
{
    auto area = getLocalBounds().reduced(5, 3);

    int centerWidth = 84;
    int deckAreaWidth = (area.getWidth() - centerWidth - 12) / 2;

    // Deck 1 Area (Left)
    auto leftArea = area.removeFromLeft(deckAreaWidth);
    int slotWidth1 = (leftArea.getWidth() - 8) / 3;

    slot1_1.setBounds(leftArea.removeFromLeft(slotWidth1));
    leftArea.removeFromLeft(4);
    slot1_2.setBounds(leftArea.removeFromLeft(slotWidth1));
    leftArea.removeFromLeft(4);
    slot1_3.setBounds(leftArea);

    // Deck 2 Area (Right)
    auto rightArea = area.removeFromRight(deckAreaWidth);
    int slotWidth2 = (rightArea.getWidth() - 8) / 3;

    slot2_1.setBounds(rightArea.removeFromLeft(slotWidth2));
    rightArea.removeFromLeft(4);
    slot2_2.setBounds(rightArea.removeFromLeft(slotWidth2));
    rightArea.removeFromLeft(4);
    slot2_3.setBounds(rightArea);

    // Center Group
    area.reduce(2, 0);
    auto titleArea = area.removeFromTop(20);
    fxTitleLabel.setBounds(titleArea);

    auto modeArea = area.removeFromTop(18).reduced(2, 0);
    manualBtn.setBounds(modeArea.removeFromLeft(modeArea.getWidth() / 2 - 1));
    instantBtn.setBounds(modeArea.removeFromRight(modeArea.getWidth()));
}

void FxPanelComponent::updateModeButtons()
{
    if (!isInstantMode)
    {
        manualBtn.setCustomColours(juce::Colour::fromRGB(30, 41, 59),
                                  juce::Colour::fromRGB(56, 189, 248),
                                  juce::Colours::transparentBlack,
                                  juce::Colour::fromRGB(56, 189, 248).withAlpha(0.5f));
        instantBtn.setCustomColours(juce::Colour::fromRGB(18, 20, 28),
                                   juce::Colour::fromRGB(100, 116, 139),
                                   juce::Colours::transparentBlack,
                                   juce::Colours::transparentBlack);
    }
    else
    {
        manualBtn.setCustomColours(juce::Colour::fromRGB(18, 20, 28),
                                   juce::Colour::fromRGB(100, 116, 139),
                                   juce::Colours::transparentBlack,
                                   juce::Colours::transparentBlack);
        instantBtn.setCustomColours(juce::Colour::fromRGB(30, 41, 59),
                                   juce::Colour::fromRGB(56, 189, 248),
                                   juce::Colours::transparentBlack,
                                   juce::Colour::fromRGB(56, 189, 248).withAlpha(0.5f));
    }
}

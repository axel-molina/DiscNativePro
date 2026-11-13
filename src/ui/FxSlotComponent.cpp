#include "FxSlotComponent.h"

FxSlotComponent::FxSlotComponent(FxSlot& fxSlot)
    : slot(fxSlot)
{
    // 1. Navigation & Name Buttons
    prevBtn.setIcon(LucideIcons::IconType::ChevronLeft, 10.0f);
    prevBtn.setCustomColours(juce::Colour::fromRGB(22, 25, 34),
                             juce::Colour::fromRGB(148, 163, 184),
                             juce::Colour::fromRGB(148, 163, 184),
                             juce::Colour::fromRGB(36, 41, 54));
    prevBtn.setCornerRadius(4.0f);
    prevBtn.onClick = [this]() { cycleEffect(-1); };
    addAndMakeVisible(prevBtn);

    fxNameBtn.setText(juce::String(getFxName(static_cast<FxType>(slot.type.load()))) + juce::String::fromUTF8("  ▾"));
    fxNameBtn.setFontSize(10.5f);
    fxNameBtn.setCustomColours(juce::Colour::fromRGB(22, 25, 34),
                               juce::Colour::fromRGB(241, 245, 249),
                               juce::Colours::transparentBlack,
                               juce::Colour::fromRGB(36, 41, 54));
    fxNameBtn.setCornerRadius(4.0f);
    fxNameBtn.onClick = [this]() { showFxMenu(); };
    addAndMakeVisible(fxNameBtn);

    nextBtn.setIcon(LucideIcons::IconType::ChevronRight, 10.0f);
    nextBtn.setCustomColours(juce::Colour::fromRGB(22, 25, 34),
                             juce::Colour::fromRGB(148, 163, 184),
                             juce::Colour::fromRGB(148, 163, 184),
                             juce::Colour::fromRGB(36, 41, 54));
    nextBtn.setCornerRadius(4.0f);
    nextBtn.onClick = [this]() { cycleEffect(1); };
    addAndMakeVisible(nextBtn);

    // 2. Control Row: ON Button
    onBtn.setText("ON");
    onBtn.setFontSize(9.5f);
    onBtn.setCornerRadius(4.0f);
    onBtn.onClick = [this]() {
        bool current = slot.enabled.load();
        slot.enabled.store(!current);
        updateUiFromSlot();
    };
    addAndMakeVisible(onBtn);

    // Parameter division button
    paramBtn.setText(getParamText());
    paramBtn.setFontSize(9.5f);
    paramBtn.setCustomColours(juce::Colour::fromRGB(20, 23, 31),
                              juce::Colour::fromRGB(147, 197, 253),
                              juce::Colours::transparentBlack,
                              juce::Colour::fromRGB(36, 41, 54));
    paramBtn.setCornerRadius(4.0f);
    paramBtn.onClick = [this]() { cycleParam(); };
    addAndMakeVisible(paramBtn);

    // Dry/Wet Slider & Labels
    dryLabel.setText("D", juce::dontSendNotification);
    dryLabel.setFont(juce::FontOptions(8.5f, juce::Font::bold));
    dryLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(100, 116, 139));
    dryLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(dryLabel);

    wetDrySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    wetDrySlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    wetDrySlider.setRange(0.0, 1.0, 0.01);
    wetDrySlider.setValue(slot.wetDry.load());
    wetDrySlider.setColour(juce::Slider::thumbColourId, juce::Colour::fromRGB(56, 189, 248));
    wetDrySlider.setColour(juce::Slider::trackColourId, juce::Colour::fromRGB(37, 99, 235));
    wetDrySlider.setColour(juce::Slider::backgroundColourId, juce::Colour::fromRGB(30, 34, 46));
    wetDrySlider.onValueChange = [this]() {
        slot.wetDry.store(static_cast<float>(wetDrySlider.getValue()));
    };
    addAndMakeVisible(wetDrySlider);

    wetLabel.setText("W", juce::dontSendNotification);
    wetLabel.setFont(juce::FontOptions(8.5f, juce::Font::bold));
    wetLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(100, 116, 139));
    wetLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(wetLabel);

    updateUiFromSlot();
}

void FxSlotComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(1.0f);

    // Card background
    g.setColour(juce::Colour::fromRGB(17, 20, 27));
    g.fillRoundedRectangle(bounds, 6.0f);

    // Subtle outline
    bool isActive = slot.enabled.load();
    g.setColour(isActive ? juce::Colour::fromRGB(59, 130, 246).withAlpha(0.35f)
                         : juce::Colour::fromRGB(32, 36, 48));
    g.drawRoundedRectangle(bounds, 6.0f, 1.0f);
}

void FxSlotComponent::resized()
{
    auto area = getLocalBounds().reduced(4, 3);

    // Top row: Effect selection (21px)
    auto topRow = area.removeFromTop(21);
    prevBtn.setBounds(topRow.removeFromLeft(16));
    topRow.removeFromLeft(2);
    nextBtn.setBounds(topRow.removeFromRight(16));
    topRow.removeFromRight(2);
    fxNameBtn.setBounds(topRow);

    area.removeFromTop(3);

    // Bottom row: Controls (21px)
    auto botRow = area.removeFromTop(21);
    onBtn.setBounds(botRow.removeFromLeft(26));
    botRow.removeFromLeft(3);
    paramBtn.setBounds(botRow.removeFromLeft(30));
    botRow.removeFromLeft(3);

    dryLabel.setBounds(botRow.removeFromLeft(10));
    wetLabel.setBounds(botRow.removeFromRight(10));
    wetDrySlider.setBounds(botRow.reduced(1, 0));
}

void FxSlotComponent::updateUiFromSlot()
{
    fxNameBtn.setText(juce::String(getFxName(static_cast<FxType>(slot.type.load()))) + juce::String::fromUTF8("  ▾"));
    paramBtn.setText(getParamText());

    bool on = slot.enabled.load();
    if (on)
    {
        onBtn.setCustomColours(juce::Colour::fromRGB(37, 99, 235),
                               juce::Colours::white,
                               juce::Colours::transparentBlack,
                               juce::Colour::fromRGB(96, 165, 250));
    }
    else
    {
        onBtn.setCustomColours(juce::Colour::fromRGB(22, 25, 34),
                               juce::Colour::fromRGB(100, 116, 139),
                               juce::Colours::transparentBlack,
                               juce::Colour::fromRGB(36, 41, 54));
    }

    repaint();
}

void FxSlotComponent::showFxMenu()
{
    juce::PopupMenu menu;
    int currentType = slot.type.load();

    menu.addItem(1, "Echo", true, currentType == 0);
    menu.addItem(2, "Reverb", true, currentType == 1);
    menu.addItem(3, "Flanger", true, currentType == 2);
    menu.addItem(4, "Phaser", true, currentType == 3);
    menu.addItem(5, "Bit Crusher", true, currentType == 4);
    menu.addItem(6, "Gate", true, currentType == 5);

    menu.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(&fxNameBtn),
                       [this](int result) {
                           if (result >= 1 && result <= 6)
                           {
                               slot.type.store(result - 1);
                               updateUiFromSlot();
                           }
                       });
}

void FxSlotComponent::cycleEffect(int delta)
{
    int cur = slot.type.load();
    cur = (cur + delta + 6) % 6;
    slot.type.store(cur);
    updateUiFromSlot();
}

void FxSlotComponent::cycleParam()
{
    float p = slot.parameter.load();
    auto fx = static_cast<FxType>(slot.type.load());

    if (fx == FxType::Echo || fx == FxType::Gate)
    {
        // P21: cicla la división de beat (0=1/4 ... 4=2/1)
        int d = slot.beatDivision.load();
        d = (d + 1) % 5;
        slot.beatDivision.store(d);
    }
    else
    {
        if (p < 0.35f) p = 0.50f;
        else if (p < 0.65f) p = 0.85f;
        else p = 0.25f;
    }

    slot.parameter.store(p);
    updateUiFromSlot();
}

juce::String FxSlotComponent::getParamText() const
{
    float p = slot.parameter.load();
    auto fx = static_cast<FxType>(slot.type.load());

    if (fx == FxType::Echo || fx == FxType::Gate)
    {
        static const char* divNames[] = { "1/4", "1/2", "3/4", "1/1", "2/1" };
        return divNames[juce::jlimit(0, 4, slot.beatDivision.load())];
    }
    else if (fx == FxType::Reverb)
    {
        if (p < 0.35f) return "Room";
        if (p < 0.65f) return "Hall";
        return "Arena";
    }
    else if (fx == FxType::BitCrusher)
    {
        if (p < 0.35f) return "10b";
        if (p < 0.65f) return "6b";
        return "3b";
    }
    else
    {
        // Flanger & Phaser
        if (p < 0.35f) return "Slow";
        if (p < 0.65f) return "Mid";
        return "Fast";
    }
}

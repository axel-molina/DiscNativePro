#include "SamplerPanelComponent.h"

SamplerPanelComponent::SamplerPanelComponent(SamplerBank& bank,
                                             juce::AudioFormatManager& formatManager,
                                             std::function<double()> sampleRateProvider)
    : sampler(bank), formats(formatManager), getSampleRate(std::move(sampleRateProvider))
{
    titleLabel.setText("SAMPLER", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(148, 163, 184));
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);

    stopAllBtn.setText("STOP");
    stopAllBtn.setFontSize(8.5f);
    stopAllBtn.setCornerRadius(3.0f);
    stopAllBtn.setCustomColours(juce::Colour::fromRGB(18, 20, 28),
                                juce::Colour::fromRGB(100, 116, 139),
                                juce::Colours::transparentBlack,
                                juce::Colours::transparentBlack);
    stopAllBtn.onClick = [this]() { sampler.stopAll(); };
    stopAllBtn.setTooltip(LanguageManager::trUtf8("Detiene todos los samples a la vez", "Stop all samples at once"));
    addAndMakeVisible(stopAllBtn);

    volKnob.setRange(0.0, 1.5, 0.01);
    volKnob.setValue(sampler.getMasterVolume(), juce::dontSendNotification);
    volKnob.onValueChange = [this]() {
        sampler.setMasterVolume((float)volKnob.getValue());
    };
    addAndMakeVisible(volKnob);

    for (int i = 0; i < SamplerBank::numSlots; ++i)
    {
        pads[i].setFontSize(9.0f);
        pads[i].setCornerRadius(4.0f);
        pads[i].onClick = [this, i]() {
            if (sampler.isLoaded(i))
                sampler.trigger(i);
            else
                chooseSampleFor(i);
        };
        pads[i].addMouseListener(this, false);
        addAndMakeVisible(pads[i]);
    }

    LanguageManager::getInstance().addListener(this);
    refreshLanguage();
    refreshAllPads();
}

SamplerPanelComponent::~SamplerPanelComponent()
{
    LanguageManager::getInstance().removeListener(this);
}

void SamplerPanelComponent::languageChanged()
{
    refreshLanguage();
}

void SamplerPanelComponent::refreshLanguage()
{
    stopAllBtn.setText(LanguageManager::trUtf8("STOP", "STOP"));
    repaint();
}

void SamplerPanelComponent::refreshPad(int index)
{
    if (index < 0 || index >= SamplerBank::numSlots)
        return;
    auto& btn = pads[index];
    if (sampler.isLoaded(index))
    {
        juce::String nm = sampler.getName(index);
        if (nm.length() > 12)
            nm = nm.substring(0, 12);
        btn.setText(juce::String(index + 1) + " " + nm);
        btn.setTooltip(nm + " (" + juce::String(sampler.getDurationSeconds(index), 1) + "s)");
        btn.setCustomColours(juce::Colour::fromRGB(30, 41, 59),
                             juce::Colours::white,
                             juce::Colour::fromRGB(56, 189, 248),
                             juce::Colour::fromRGB(56, 189, 248).withAlpha(0.5f));
    }
    else
    {
        btn.setText(juce::String(index + 1) + " +");
        btn.setTooltip(LanguageManager::trUtf8("Vacío: click para cargar sample",
                                               "Empty: click to load sample"));
        btn.setCustomColours(juce::Colour::fromRGB(18, 20, 28),
                             juce::Colour::fromRGB(100, 116, 139),
                             juce::Colours::transparentBlack,
                             juce::Colours::transparentBlack);
    }
}

void SamplerPanelComponent::refreshAllPads()
{
    for (int i = 0; i < SamplerBank::numSlots; ++i)
        refreshPad(i);
}

void SamplerPanelComponent::chooseSampleFor(int index)
{
    pendingSlot = index;
    fileChooser = std::make_unique<juce::FileChooser>(
        LanguageManager::trUtf8("Cargar sample...", "Load sample..."),
        juce::File::getSpecialLocation(juce::File::userMusicDirectory),
        "*.wav;*.mp3;*.flac;*.aiff;*.aif;*.m4a;*.ogg");
    auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    fileChooser->launchAsync(flags, [this](const juce::FileChooser& fc) {
        auto file = fc.getResult();
        int slot = pendingSlot;
        pendingSlot = -1;
        if (file.existsAsFile() && slot >= 0)
        {
            double sr = getSampleRate ? getSampleRate() : 48000.0;
            if (sampler.loadSlot(slot, file, formats, sr))
                refreshPad(slot);
        }
    });
}

void SamplerPanelComponent::showPadMenu(int index)
{
    juce::PopupMenu menu;
    if (sampler.isLoaded(index))
    {
        menu.addItem(1, LanguageManager::trUtf8("Disparar", "Trigger"));
        menu.addItem(2, LanguageManager::trUtf8("Descargar sample", "Unload sample"));
        menu.addSeparator();
        menu.addItem(3, LanguageManager::trUtf8("Cargar otro...", "Load another..."));
    }
    else
    {
        menu.addItem(3, LanguageManager::trUtf8("Cargar sample...", "Load sample..."));
    }
    menu.showMenuAsync(juce::PopupMenu::Options(), [this, index](int result) {
        if (result == 1)
            sampler.trigger(index);
        else if (result == 2)
        {
            sampler.unloadSlot(index);
            refreshPad(index);
        }
        else if (result == 3)
        {
            chooseSampleFor(index);
        }
    });
}

void SamplerPanelComponent::mouseDown(const juce::MouseEvent& e)
{
    if (!e.mods.isRightButtonDown() && !e.mods.isCtrlDown())
        return;
    for (int i = 0; i < SamplerBank::numSlots; ++i)
    {
        if (e.eventComponent == &pads[i])
        {
            showPadMenu(i);
            return;
        }
    }
}

void SamplerPanelComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colour::fromRGB(12, 14, 19));
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(juce::Colour::fromRGB(26, 30, 42));
    g.drawRoundedRectangle(bounds, 6.0f, 1.0f);
}

void SamplerPanelComponent::resized()
{
    auto area = getLocalBounds().reduced(5, 3);

    // Bloque izquierdo: título + stop + volumen
    auto leftArea = area.removeFromLeft(120);
    titleLabel.setBounds(leftArea.removeFromTop(18));
    auto ctrlRow = leftArea;
    stopAllBtn.setBounds(ctrlRow.removeFromLeft(56).reduced(0, 6));
    ctrlRow.removeFromLeft(4);
    volKnob.setBounds(ctrlRow);

    area.removeFromLeft(6);

    // 8 pads repartidos
    int gap = 4;
    int padW = (area.getWidth() - gap * (SamplerBank::numSlots - 1)) / SamplerBank::numSlots;
    for (int i = 0; i < SamplerBank::numSlots; ++i)
    {
        pads[i].setBounds(area.removeFromLeft(padW));
        if (i < SamplerBank::numSlots - 1)
            area.removeFromLeft(gap);
    }
}

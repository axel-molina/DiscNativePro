#include "TopBarComponent.h"
#if JUCE_MAC
#include <mach/mach.h>
#endif
#include <cmath>

TopBarComponent::TopBarComponent()
{
    // 1. Left Action Buttons
    recButton.setIcon(LucideIcons::IconType::CircleDot, 12.0f);
    recButton.setText("REC");
    recButton.setCustomColours(juce::Colour::fromRGB(24, 26, 32),
                               juce::Colour::fromRGB(142, 149, 165),
                               juce::Colour::fromRGB(239, 68, 68),
                               juce::Colour::fromRGB(45, 50, 65));
    recButton.onClick = [this]() { if (onRecClicked) onRecClicked(); };
    addAndMakeVisible(recButton);

    fxButton.setIcon(LucideIcons::IconType::Sparkles, 13.0f);
    fxButton.setText("FX");
    fxButton.setCustomColours(juce::Colour::fromRGB(24, 26, 32),
                              juce::Colour::fromRGB(96, 165, 250),
                              juce::Colour::fromRGB(96, 165, 250),
                              juce::Colour::fromRGB(45, 50, 65));
    fxButton.onClick = [this]() { if (onFxClicked) onFxClicked(); };
    addAndMakeVisible(fxButton);

    stemsButton.setIcon(LucideIcons::IconType::Sliders, 13.0f);
    stemsButton.setText("STEMS");
    stemsButton.setCustomColours(juce::Colour::fromRGB(24, 26, 32),
                                 juce::Colour::fromRGB(148, 163, 184),
                                 juce::Colour::fromRGB(148, 163, 184),
                                 juce::Colour::fromRGB(45, 50, 65));
    addAndMakeVisible(stemsButton);

    // 2. Center Brand
    brandLabel.setText("DiscPro", juce::dontSendNotification);
    brandLabel.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    brandLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    brandLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(brandLabel);

    // 3. Right Status & Actions
    updateClock();
    clockLabel.setFont(juce::FontOptions("Menlo", 11.0f, juce::Font::plain));
    clockLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(142, 149, 165));
    clockLabel.setColour(juce::Label::backgroundColourId, juce::Colour::fromRGB(22, 24, 32));
    clockLabel.setColour(juce::Label::outlineColourId, juce::Colour::fromRGB(37, 40, 51));
    clockLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(clockLabel);

    updateMemoryUsage();
    ramLabel.setFont(juce::FontOptions("Menlo", 11.0f, juce::Font::plain));
    ramLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(142, 149, 165));
    ramLabel.setColour(juce::Label::backgroundColourId, juce::Colour::fromRGB(22, 24, 32));
    ramLabel.setColour(juce::Label::outlineColourId, juce::Colour::fromRGB(37, 40, 51));
    ramLabel.setJustificationType(juce::Justification::centred);
    ramLabel.setTooltip("Consumo de memoria RAM de DiscNativePro en tiempo real");
    addAndMakeVisible(ramLabel);

    layoutBadge.setIcon(LucideIcons::IconType::CircleDot, 12.0f);
    layoutBadge.setText("2 Decks");
    layoutBadge.setFontSize(10.5f);
    layoutBadge.setCornerRadius(6.0f);
    layoutBadge.setCustomColours(juce::Colour::fromRGB(22, 24, 32),
                                juce::Colour::fromRGB(96, 165, 250),
                                juce::Colour::fromRGB(96, 165, 250),
                                juce::Colour::fromRGB(37, 40, 51));
    layoutBadge.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(layoutBadge);

    settingsButton.setIcon(LucideIcons::IconType::Settings, 15.0f);
    settingsButton.setCustomColours(juce::Colour::fromRGB(24, 26, 32),
                                   juce::Colours::white,
                                   juce::Colour::fromRGB(142, 149, 165),
                                   juce::Colour::fromRGB(45, 50, 65));
    settingsButton.onClick = [this]() { if (onSettingsClicked) onSettingsClicked(); };
    addAndMakeVisible(settingsButton);

    fullscreenButton.setIcon(LucideIcons::IconType::Maximize, 14.0f);
    fullscreenButton.setCustomColours(juce::Colour::fromRGB(24, 26, 32),
                                     juce::Colours::white,
                                     juce::Colour::fromRGB(142, 149, 165),
                                     juce::Colour::fromRGB(45, 50, 65));
    fullscreenButton.onClick = [this]() { if (onFullscreenClicked) onFullscreenClicked(); };
    addAndMakeVisible(fullscreenButton);

    startTimer(1000); // 1-second clock timer
}

TopBarComponent::~TopBarComponent()
{
    stopTimer();
}

void TopBarComponent::setRecordingState(bool recording, double elapsedSeconds)
{
    isRecording = recording;
    currentRecSeconds = elapsedSeconds;

    if (isRecording)
    {
        int mins = (int)elapsedSeconds / 60;
        int secs = (int)elapsedSeconds % 60;
        recButton.setIcon(LucideIcons::IconType::CircleDot, 12.0f);
        recButton.setText(juce::String::formatted("%02d:%02d", mins, secs));
        recButton.setCustomColours(juce::Colour::fromRGB(220, 38, 38),
                                   juce::Colours::white,
                                   juce::Colours::white,
                                   juce::Colour::fromRGB(239, 68, 68));
    }
    else
    {
        recButton.setIcon(LucideIcons::IconType::CircleDot, 12.0f);
        recButton.setText("REC");
        recButton.setCustomColours(juce::Colour::fromRGB(24, 26, 32),
                                   juce::Colour::fromRGB(142, 149, 165),
                                   juce::Colour::fromRGB(239, 68, 68),
                                   juce::Colour::fromRGB(45, 50, 65));
    }
}

void TopBarComponent::updateClock()
{
    auto now = juce::Time::getCurrentTime();
    int h = now.getHours();
    int m = now.getMinutes();
    bool pm = h >= 12;
    if (h == 0) h = 12;
    else if (h > 12) h -= 12;

    timeString = juce::String::formatted("%d:%02d %s", h, m, pm ? "PM" : "AM");
    clockLabel.setText(timeString, juce::dontSendNotification);
}

double TopBarComponent::getProcessMemoryMB()
{
#if JUCE_MAC
    mach_task_basic_info info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &count) == KERN_SUCCESS)
    {
        return static_cast<double>(info.resident_size) / (1024.0 * 1024.0);
    }
#endif
    return 0.0;
}

void TopBarComponent::updateMemoryUsage()
{
    double memMb = getProcessMemoryMB();
    if (memMb >= 1024.0)
    {
        memoryString = juce::String::formatted("RAM: %.1f GB", memMb / 1024.0);
    }
    else if (memMb > 0.0)
    {
        memoryString = juce::String::formatted("RAM: %d MB", static_cast<int>(std::round(memMb)));
    }
    else
    {
        memoryString = "RAM: --";
    }

    ramLabel.setText(memoryString, juce::dontSendNotification);
}

void TopBarComponent::timerCallback()
{
    updateClock();
    updateMemoryUsage();
}

void TopBarComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Dark TopBar background #0f1115
    g.fillAll(juce::Colour::fromRGB(15, 17, 21));

    // Bottom border #20232c
    g.setColour(juce::Colour::fromRGB(32, 35, 44));
    g.drawHorizontalLine(getHeight() - 1, 0.0f, bounds.getWidth());

    // Center Disc Logo icon (pure vector LucideIcons::Disc)
    float centerX = bounds.getCentreX() - 48.0f;
    float centerY = bounds.getCentreY();
    LucideIcons::draw(g, LucideIcons::IconType::Disc,
                      juce::Rectangle<float>(centerX - 8.0f, centerY - 8.0f, 16.0f, 16.0f),
                      juce::Colour::fromRGB(200, 205, 215), 1.8f);
}

void TopBarComponent::resized()
{
    auto area = getLocalBounds().reduced(8, 4);

    // Left Buttons
    recButton.setBounds(area.removeFromLeft(78).reduced(0, 2));
    area.removeFromLeft(6);
    fxButton.setBounds(area.removeFromLeft(58).reduced(0, 2));
    area.removeFromLeft(4);
    stemsButton.setBounds(area.removeFromLeft(82).reduced(0, 2));

    // Right Buttons & Indicators
    fullscreenButton.setBounds(area.removeFromRight(32).reduced(0, 2));
    area.removeFromRight(4);
    settingsButton.setBounds(area.removeFromRight(32).reduced(0, 2));
    area.removeFromRight(6);
    layoutBadge.setBounds(area.removeFromRight(95).reduced(0, 3));
    area.removeFromRight(6);
    clockLabel.setBounds(area.removeFromRight(80).reduced(0, 3));
    area.removeFromRight(6);
    ramLabel.setBounds(area.removeFromRight(95).reduced(0, 3));

    // Center Brand Title
    int centerWidth = 140;
    brandLabel.setBounds((getWidth() - centerWidth) / 2 + 10, area.getY(), centerWidth, area.getHeight());
}

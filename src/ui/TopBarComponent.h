#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "DjButton.h"
#include <functional>

class TopBarComponent : public juce::Component,
                        public juce::Timer
{
public:
    TopBarComponent();
    ~TopBarComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    void setRecordingState(bool recording, double elapsedSeconds = 0.0);
    bool isRecordingActive() const { return isRecording; }

    std::function<void()> onRecClicked;
    std::function<void()> onFxClicked;
    std::function<void()> onSettingsClicked;
    std::function<void()> onFullscreenClicked;

private:
    void updateClock();

    // Left buttons
    DjButton recButton;
    DjButton fxButton;
    DjButton stemsButton;

    // Center logo
    juce::Label brandLabel;

    // Right elements
    juce::Label clockLabel;
    DjButton layoutBadge;
    DjButton settingsButton;
    DjButton fullscreenButton;

    bool isRecording { false };
    double currentRecSeconds { 0.0 };
    juce::String timeString;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TopBarComponent)
};

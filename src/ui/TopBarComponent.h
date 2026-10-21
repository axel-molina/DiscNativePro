#include <juce_gui_basics/juce_gui_basics.h>
#include "DjButton.h"
#include "../services/LanguageManager.h"
#include <functional>

class TopBarComponent : public juce::Component,
                        public juce::Timer,
                        public LanguageManager::Listener
{
public:
    TopBarComponent();
    ~TopBarComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;
    void languageChanged() override;
    void refreshLanguage();

    void setRecordingState(bool recording, double elapsedSeconds = 0.0);
    bool isRecordingActive() const { return isRecording; }
    void setFxActive(bool active);
    void setSmpActive(bool active);
    void setUpdateAvailable(bool available);

    // Selector de vista: 2 Bandejas (default) u Ondas/Waves, bilingüe y en caliente
    enum class ViewMode { TwoDecks, Waves };
    ViewMode getViewMode() const { return currentViewMode; }
    void setViewMode(ViewMode mode, bool sendCallback = true);

    std::function<void()> onRecClicked;
    std::function<void()> onFxClicked;
    std::function<void()> onSmpClicked;
    std::function<void()> onSettingsClicked;
    std::function<void()> onFullscreenClicked;
    std::function<void()> onUpdateClicked;
    std::function<void(ViewMode)> onViewModeChanged;

private:
    void updateClock();
    void updateMemoryUsage();
    double getProcessMemoryMB();
    void updateViewSelectorColours();

    // Left buttons
    DjButton recButton;
    DjButton fxButton;
    DjButton smpButton;

    // Center logo
    juce::Label brandLabel;

    // Right elements
    juce::Label ramLabel;
    juce::Label clockLabel;
    DjButton viewDecksButton;
    DjButton viewWavesButton;
    DjButton updateButton;
    DjButton settingsButton;
    DjButton fullscreenButton;
    ViewMode currentViewMode { ViewMode::TwoDecks };

    bool isRecording { false };
    double currentRecSeconds { 0.0 };
    juce::String timeString;
    juce::String memoryString;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TopBarComponent)
};

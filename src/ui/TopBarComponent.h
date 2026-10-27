#include <juce_gui_basics/juce_gui_basics.h>
#include "DjButton.h"
#include "../services/LanguageManager.h"
#include <functional>
#include <memory>

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

    // Selector de vista desplegable: 2 Bandejas (default) u Ondas/Waves, bilingüe y en caliente.
    // El botón muestra la vista activa; al hacer clic despliega la lista de opciones con la
    // activa marcada, y elegir una de la lista aplica el cambio.
    enum class ViewMode { TwoDecks, Waves };
    ViewMode getViewMode() const { return currentViewMode; }
    void setViewMode(ViewMode mode, bool sendCallback = true);
    void selectViewMode(ViewMode mode);

    std::function<void()> onRecClicked;
    std::function<void()> onFxClicked;
    std::function<void()> onSmpClicked;
    std::function<void()> onSettingsClicked;
    std::function<void()> onFullscreenClicked;
    std::function<void()> onUpdateClicked;
    std::function<void(ViewMode)> onViewModeChanged;

private:
    class ViewSelectorDropdown;

    void updateClock();
    void updateMemoryUsage();
    double getProcessMemoryMB();
    void toggleViewSelector();
    void hideViewSelector();
    void updateViewSelectorAppearance(bool open);
    juce::String viewModeLabel(ViewMode mode) const;

    // Left buttons
    DjButton recButton;
    DjButton fxButton;
    DjButton smpButton;

    // Center logo
    juce::Label brandLabel;

    // Right elements
    juce::Label ramLabel;
    juce::Label clockLabel;
    DjButton viewSelectorButton;
    std::unique_ptr<ViewSelectorDropdown> viewDropdown;
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

#pragma once

#include "../audio/AudioEngine.h"
#include "../midi/MidiManager.h"
#include "TopBarComponent.h"
#include "DeckComponent.h"
#include "WavesViewComponent.h"
#include "MixerComponent.h"
#include "LibraryComponent.h"
#include "FxPanelComponent.h"
#include "SamplerPanelComponent.h"
#include "../services/UpdateService.h"

class MainComponent : public juce::Component,
                      public juce::DragAndDropContainer,
                      public juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    // Keyboard DJ shortcuts
    bool keyPressed(const juce::KeyPress& key) override;

private:
    AudioEngine audioEngine;
    MidiManager midiManager;

    // UI Structure matching DiscPro
    TopBarComponent topBar;
    FxPanelComponent fxPanel;
    SamplerPanelComponent samplerPanel;
    DeckComponent deck1;
    DeckComponent deck2;
    MixerComponent mixer;
    LibraryComponent library;
    WavesViewComponent wavesView;

    // Selector de vista del TopBar: 2 Bandejas (default) u Ondas, persistente
    // en settings.json ("viewMode": "decks" | "waves") y conmutable en caliente.
    TopBarComponent::ViewMode currentViewMode { TopBarComponent::ViewMode::TwoDecks };
    void setViewMode(TopBarComponent::ViewMode mode);
    void applyViewMode();
    static TopBarComponent::ViewMode loadViewModeFromSettings();
    static void saveViewModeToSettings(TopBarComponent::ViewMode mode);

    bool isFxPanelVisible { true };
    bool isSamplerPanelVisible { false };
    juce::Component::SafePointer<juce::DialogWindow> settingsWindow;

    // Automix orchestration state
    std::vector<TrackItem> automixPlaylist;
    int automixCurrentIndex { -1 };
    bool automixActive { false };
    int automixActiveDeck { 0 };
    bool automixTransitioning { false };
    bool smartAutomixBusy { false };
    float automixCrossfaderTarget { 0.0f };
    float automixCrossfaderStep { 0.0f };

    void startAutomix(const std::vector<TrackItem>& tracks);
    void stopAutomix();
    void automixAdvance();
    void startSmartAutomix();
    void openSettings(int targetTab = 0);

    // Update notification (GitHub Releases, once per launch)
    UpdateInfo pendingUpdate;
    void scheduleUpdateCheck();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

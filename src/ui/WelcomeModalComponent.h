#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../services/LanguageManager.h"
#include "DjButton.h"

// P36 (Notion DNP-TK-12): aviso de bienvenida.
// El proyecto está en desarrollo y se usa bajo propio riesgo; el diálogo ofrece
// el canal de YouTube y el repo de GitHub para apoyar el desarrollo.
class WelcomeModalComponent : public juce::Component,
                              public LanguageManager::Listener
{
public:
    WelcomeModalComponent();
    ~WelcomeModalComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void languageChanged() override;

    // Se dispara al pulsar "Entendido": el owner cierra la ventana.
    std::function<void()> onDismiss;

    // Flag persistido en settings.json ("welcome.suppress").
    static bool isSuppressed();
    static void setSuppressed(bool suppressed);
    static bool shouldShowOnStartup();

private:
    void refreshLanguage();

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label bodyLabel;
    DjButton youtubeBtn;
    DjButton githubBtn;
    juce::Label supportHintLabel;
    juce::ToggleButton dontShowAgainToggle;
    DjButton okBtn;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WelcomeModalComponent)
};

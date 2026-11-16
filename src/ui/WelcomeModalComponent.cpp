#include "WelcomeModalComponent.h"
#include "UiTypeScale.h"
#include "LucideIcons.h"
#include "../audio/AudioEngine.h"

namespace
{
    constexpr const char* welcomeSuppressProperty = "welcome.suppress";
    constexpr const char* youtubeUrl = "https://www.youtube.com/@AxelCodeDev";
    constexpr const char* githubUrl = "https://github.com/axel-molina/DiscNativePro";

    // Lee el flag del aviso. 'found' distingue "nunca se mostró" (primer arranque)
    // de "el usuario lo desmarcó explícitamente".
    bool readSuppressFlag(bool& found)
    {
        found = false;
        auto file = AudioEngine::getSettingsFile();
        if (!file.existsAsFile())
            return false;

        auto json = juce::JSON::parse(file);
        if (!json.isObject() || !json.hasProperty(welcomeSuppressProperty))
            return false;

        found = true;
        return static_cast<bool>(json[welcomeSuppressProperty]);
    }

    void writeSuppressFlag(bool value)
    {
        auto file = AudioEngine::getSettingsFile();
        juce::var json;
        if (file.existsAsFile())
            json = juce::JSON::parse(file);
        if (!json.isObject())
            json = juce::var(new juce::DynamicObject());

        if (auto* obj = json.getDynamicObject())
            obj->setProperty(welcomeSuppressProperty, value);

        file.replaceWithText(juce::JSON::toString(json));
    }
}

WelcomeModalComponent::WelcomeModalComponent()
{
    setSize(560, 410);

    titleLabel.setFont(juce::FontOptions(UiType::title, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel);

    subtitleLabel.setFont(juce::FontOptions(UiType::caption, juce::Font::bold));
    subtitleLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(245, 158, 11));
    subtitleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(subtitleLabel);

    bodyLabel.setFont(juce::FontOptions(UiType::body, juce::Font::plain));
    bodyLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(180, 185, 200));
    bodyLabel.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(bodyLabel);

    youtubeBtn.setIcon(LucideIcons::IconType::YouTube, 16.0f);
    youtubeBtn.setFontSize(UiType::caption);
    youtubeBtn.setCornerRadius(6.0f);
    youtubeBtn.setCustomColours(juce::Colour::fromRGB(24, 27, 36),
                               juce::Colour::fromRGB(255, 120, 120),
                               juce::Colour::fromRGB(255, 120, 120),
                               juce::Colour::fromRGB(58, 44, 52));
    youtubeBtn.onClick = []() { juce::URL(youtubeUrl).launchInDefaultBrowser(); };
    addAndMakeVisible(youtubeBtn);

    githubBtn.setIcon(LucideIcons::IconType::Star, 16.0f);
    githubBtn.setFontSize(UiType::caption);
    githubBtn.setCornerRadius(6.0f);
    githubBtn.setCustomColours(juce::Colour::fromRGB(24, 27, 36),
                                juce::Colour::fromRGB(200, 205, 215),
                                juce::Colour::fromRGB(200, 205, 215),
                                juce::Colour::fromRGB(36, 40, 54));
    githubBtn.onClick = []() { juce::URL(githubUrl).launchInDefaultBrowser(); };
    addAndMakeVisible(githubBtn);

    supportHintLabel.setFont(juce::FontOptions(UiType::caption, juce::Font::plain));
    supportHintLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(148, 163, 184));
    supportHintLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(supportHintLabel);

    dontShowAgainToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::white.withAlpha(0.85f));
    dontShowAgainToggle.setColour(juce::ToggleButton::tickColourId, juce::Colour::fromRGB(0, 180, 216));
    dontShowAgainToggle.onClick = [this]() {
        // Se persiste al cambiar la casilla para que el estado en disco siempre
        // refleje lo que el usuario ha elegido.
        setSuppressed(dontShowAgainToggle.getToggleState());
    };
    addAndMakeVisible(dontShowAgainToggle);

    okBtn.setFontSize(UiType::caption);
    okBtn.setCornerRadius(6.0f);
    okBtn.setCustomColours(juce::Colour::fromRGB(37, 99, 235),
                           juce::Colours::white,
                           juce::Colours::white,
                           juce::Colour::fromRGB(37, 99, 235));
    okBtn.onClick = [this]() {
        setSuppressed(dontShowAgainToggle.getToggleState());
        if (onDismiss)
            onDismiss();
    };
    addAndMakeVisible(okBtn);

    // Estado inicial de la casilla: marcada si nunca se mostró (primer arranque)
    // o si el aviso ya fue descartado; desmarcada solo si el usuario lo pidió.
    bool found = false;
    const bool suppressed = readSuppressFlag(found);
    dontShowAgainToggle.setToggleState(!found || suppressed, juce::dontSendNotification);

    LanguageManager::getInstance().addListener(this);
    refreshLanguage();
}

WelcomeModalComponent::~WelcomeModalComponent()
{
    LanguageManager::getInstance().removeListener(this);
}

void WelcomeModalComponent::languageChanged()
{
    refreshLanguage();
}

void WelcomeModalComponent::refreshLanguage()
{
    titleLabel.setText(LanguageManager::trUtf8("Bienvenido a DiscNativePro", "Welcome to DiscNativePro"),
                       juce::dontSendNotification);
    subtitleLabel.setText(LanguageManager::trUtf8("Proyecto en desarrollo · úsalo bajo tu propio riesgo",
                                                   "Work in progress · use at your own risk"),
                           juce::dontSendNotification);
    bodyLabel.setText(LanguageManager::trUtf8(
        "DiscNativePro es un proyecto en desarrollo hecho por una sola persona.\n\n"
        "Puede contener errores, funciones a medio terminar y problemas de estabilidad.\n\n"
        "Úsalo bajo tu propio riesgo: guarda tus mezclas antes de una sesión importante.",
        "DiscNativePro is a work-in-progress project built by one person.\n\n"
        "It may contain errors, unfinished features and stability issues.\n\n"
        "Use it at your own risk: save your mixes before an important session."),
        juce::dontSendNotification);

    youtubeBtn.setText(LanguageManager::trUtf8("Canal de YouTube (@AxelCodeDev)", "YouTube channel (@AxelCodeDev)"));
    githubBtn.setText(LanguageManager::trUtf8("Código en GitHub (deja una estrella)", "Source code on GitHub (leave a star)"));
    supportHintLabel.setText(LanguageManager::trUtf8("Ver tutoriales, novedades y el estado del proyecto.",
                                                      "Tutorials, news and project status."),
                              juce::dontSendNotification);
    dontShowAgainToggle.setButtonText(LanguageManager::trUtf8("No mostrar de nuevo", "Do not show again"));
    okBtn.setText(LanguageManager::trUtf8("Entendido", "Got it"));
}

bool WelcomeModalComponent::isSuppressed()
{
    bool found = false;
    const bool value = readSuppressFlag(found);
    return found && value;
}

void WelcomeModalComponent::setSuppressed(bool suppressed)
{
    writeSuppressFlag(suppressed);
}

bool WelcomeModalComponent::shouldShowOnStartup()
{
    return !isSuppressed();
}

void WelcomeModalComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.fillAll(juce::Colour::fromRGB(26, 29, 38));
    g.setColour(juce::Colour::fromRGB(45, 50, 66));
    g.drawRect(bounds, 1.0f);

    g.setColour(juce::Colour::fromRGB(38, 43, 58));
    g.drawHorizontalLine(206.0f, 24.0f, bounds.getWidth() - 24.0f);
}

void WelcomeModalComponent::resized()
{
    titleLabel.setBounds(24, 16, getWidth() - 48, 30);
    subtitleLabel.setBounds(24, 48, getWidth() - 48, 22);
    bodyLabel.setBounds(24, 78, getWidth() - 48, 116);

    youtubeBtn.setBounds(24, 218, getWidth() - 48, 36);
    githubBtn.setBounds(24, 262, getWidth() - 48, 36);
    supportHintLabel.setBounds(24, 306, getWidth() - 48, 20);

    dontShowAgainToggle.setBounds(24, 338, 300, 24);
    okBtn.setBounds(getWidth() - 144, 334, 120, 34);
}

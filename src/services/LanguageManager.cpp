#include "LanguageManager.h"
#include "../audio/AudioEngine.h"

juce::StringPairArray LanguageManager::translations;

void LanguageManager::rememberTranslation(const juce::String& es, const juce::String& en)
{
    if (es.isNotEmpty() && en.isNotEmpty() && es != en)
    {
        translations.set(es, en);
        translations.set(en, es);
    }
}

void LanguageManager::refreshTooltips()
{
    // P50: los tooltips se asignan una sola vez en los constructores; al cambiar
    // de idioma no se retraducen solos. Recorremos el árbol de componentes y
    // cambiamos cada texto por su par del idioma contrario.
    struct Visitor
    {
        static void walk(juce::Component* c)
        {
            if (c == nullptr)
                return;
            // En JUCE 8 los tooltips viven en SettableTooltipClient, no en Component.
            if (auto* tc = dynamic_cast<juce::SettableTooltipClient*>(c))
            {
                auto tip = tc->getTooltip();
                if (tip.isNotEmpty() && translations.containsKey(tip))
                    tc->setTooltip(translations[tip]);
            }
            for (int i = 0; i < c->getNumChildComponents(); ++i)
                walk(c->getChildComponent(i));
        }
    };

    auto& desktop = juce::Desktop::getInstance();
    for (int i = 0; i < desktop.getNumComponents(); ++i)
        Visitor::walk(desktop.getComponent(i));
}

LanguageManager::LanguageManager()
{
    loadFromSettings();
}

Language LanguageManager::resolveSystemLanguage() const
{
    auto sysLang = juce::SystemStats::getUserLanguage().toLowerCase();
    if (sysLang.startsWith("es"))
        return Language::Spanish;
    return Language::English;
}

void LanguageManager::loadFromSettings()
{
    auto file = AudioEngine::getSettingsFile();
    if (file.existsAsFile())
    {
        auto json = juce::JSON::parse(file);
        if (json.isObject() && json.hasProperty("language"))
        {
            auto val = json["language"].toString().trim().toLowerCase();
            if (val == "es")
            {
                langSetting = LanguageSetting::Spanish;
                currentLang = Language::Spanish;
                return;
            }
            else if (val == "en")
            {
                langSetting = LanguageSetting::English;
                currentLang = Language::English;
                return;
            }
        }
    }
    langSetting = LanguageSetting::System;
    currentLang = resolveSystemLanguage();
}

void LanguageManager::saveToSettings()
{
    auto file = AudioEngine::getSettingsFile();
    juce::var json;
    if (file.existsAsFile())
        json = juce::JSON::parse(file);
    if (!json.isObject())
        json = juce::var(new juce::DynamicObject());

    if (auto* obj = json.getDynamicObject())
    {
        juce::String val;
        switch (langSetting)
        {
            case LanguageSetting::System:  val = "system"; break;
            case LanguageSetting::Spanish: val = "es"; break;
            case LanguageSetting::English: val = "en"; break;
        }
        obj->setProperty("language", val);
    }
    file.replaceWithText(juce::JSON::toString(json));
}

void LanguageManager::setLanguageSetting(LanguageSetting setting)
{
    langSetting = setting;
    Language newLang = currentLang; // sin valor por defecto MSVC avisa C4701
    switch (setting)
    {
        case LanguageSetting::System:  newLang = resolveSystemLanguage(); break;
        case LanguageSetting::Spanish: newLang = Language::Spanish; break;
        case LanguageSetting::English: newLang = Language::English; break;
    }

    saveToSettings();

    if (newLang != currentLang)
    {
        currentLang = newLang;
        notifyListeners();
    }
}

void LanguageManager::addListener(Listener* listener)
{
    if (listener != nullptr)
        listeners.push_back(listener);
}

void LanguageManager::removeListener(Listener* listener)
{
    listeners.erase(std::remove(listeners.begin(), listeners.end(), listener), listeners.end());
}

void LanguageManager::notifyListeners()
{
    for (auto* l : listeners)
    {
        if (l != nullptr)
            l->languageChanged();
    }
    // Después de las refreshes propias, cubrimos los tooltips que nadie reasigna.
    refreshTooltips();
}

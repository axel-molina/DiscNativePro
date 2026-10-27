#include "LanguageManager.h"
#include "../audio/AudioEngine.h"

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
}

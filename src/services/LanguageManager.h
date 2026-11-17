#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <juce_gui_basics/juce_gui_basics.h>

enum class Language { Spanish, English };
enum class LanguageSetting { System, Spanish, English };

class LanguageManager
{
public:
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void languageChanged() = 0;
    };

    static LanguageManager& getInstance()
    {
        static LanguageManager instance;
        return instance;
    }

    Language getCurrentLanguage() const { return currentLang; }
    LanguageSetting getLanguageSetting() const { return langSetting; }
    bool isSpanish() const { return currentLang == Language::Spanish; }
    bool isEnglish() const { return currentLang == Language::English; }

    void setLanguageSetting(LanguageSetting setting);

    // Core translation helper: returns es or en depending on current language.
    // También registra el par para poder retraducir tooltips ya asignados.
    static juce::String tr(const juce::String& es, const juce::String& en)
    {
        rememberTranslation(es, en);
        return getInstance().isSpanish() ? es : en;
    }

    static juce::String trUtf8(const char* es, const char* en)
    {
        juce::String s = juce::String::fromUTF8(es);
        juce::String e = juce::String::fromUTF8(en);
        rememberTranslation(s, e);
        return getInstance().isSpanish() ? s : e;
    }

    // P50: reasigna todos los tooltips visibles con el idioma actual.
    static void refreshTooltips();

    void addListener(Listener* listener);
    void removeListener(Listener* listener);

private:
    LanguageManager();
    ~LanguageManager() = default;

    Language resolveSystemLanguage() const;
    void loadFromSettings();
    void saveToSettings();
    void notifyListeners();
    static void rememberTranslation(const juce::String& es, const juce::String& en);

    // pares ES↔EN vistos por tr/trUtf8; la última entrada gana si el mismo
    // texto se tradujo de dos formas distintas (ponytail: suficiente para tooltips).
    static juce::StringPairArray translations;

    Language currentLang { Language::Spanish };
    LanguageSetting langSetting { LanguageSetting::System };
    std::vector<Listener*> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LanguageManager)
};

// Convenience macro for inline translation
#define TR(es, en) LanguageManager::tr(es, en)

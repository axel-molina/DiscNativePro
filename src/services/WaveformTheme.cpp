#include "WaveformTheme.h"
#include "../ui/UiPalette.h"

namespace
{
    // Sufijos de banda usados tanto para las claves por bandeja
    // (waveLow1 / waveLow2) como para las claves legacy (waveLow).
    const juce::String lowSuffix  { "Low" };
    const juce::String midSuffix  { "Mid" };
    const juce::String highSuffix { "High" };

    juce::String keyFor(int deck, const juce::String& suffix)
    {
        return juce::String("wave") + suffix + juce::String(deck + 1);
    }

    juce::String legacyKeyFor(const juce::String& suffix)
    {
        return juce::String("wave") + suffix;
    }
}

juce::File WaveformTheme::getSettingsFile()
{
    auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                       .getChildFile("DiscNativePro");
    if (!appData.exists())
        appData.createDirectory();
    return appData.getChildFile("settings.json");
}

WaveformTheme::Theme& WaveformTheme::cached(int deck)
{
    static ThemeArray themes = loadFromDisk();
    return themes[static_cast<std::size_t>(juce::jlimit(0, numDecks - 1, deck))];
}

WaveformTheme::Theme WaveformTheme::defaultsForDeck(int deck)
{
    // Deck 1: familia azul acero sobria (#2E4B66 → #8FAECB).
    // Deck 2: familia terracota apagada del theme pálido (FB-19).
    // El preset Azul Acero y los colores guardados en settings no se tocan.
    if (deck == 0)
    {
        return { UiPalette::waveLowDeckOne,
                 UiPalette::waveMidDeckOne,
                 UiPalette::waveHighDeckOne };
    }

    return { UiPalette::waveLowDeckTwo,
             UiPalette::waveMidDeckTwo,
             UiPalette::waveHighDeckTwo };
}

WaveformTheme::ThemeArray WaveformTheme::loadFromDisk()
{
    ThemeArray themes { defaultsForDeck(0), defaultsForDeck(1) };

    auto file = getSettingsFile();
    if (!file.existsAsFile())
        return themes;

    auto json = juce::JSON::parse(file);
    if (!json.isObject())
        return themes;

    // ARGB int round-trip (sin parseo de strings, sin riesgo de formato)
    for (int deck = 0; deck < numDecks; ++deck)
    {
        auto& theme = themes[static_cast<std::size_t>(deck)];

        auto readBand = [&json, deck](const juce::String& suffix, juce::Colour& target) {
            const juce::Identifier key (keyFor(deck, suffix));
            if (json.hasProperty(key))
            {
                target = juce::Colour((juce::uint32)(int)json[key]);
                return;
            }

            // Migración legacy: si la clave por bandeja no existe pero sí la
            // clave global antigua, esta se hereda (personalización previa).
            const juce::Identifier legacyKey (legacyKeyFor(suffix));
            if (json.hasProperty(legacyKey))
                target = juce::Colour((juce::uint32)(int)json[legacyKey]);
        };

        readBand(lowSuffix,  theme.low);
        readBand(midSuffix,  theme.mid);
        readBand(highSuffix, theme.high);
    }

    return themes;
}

void WaveformTheme::saveToDisk()
{
    auto file = getSettingsFile();
    juce::var json;
    if (file.existsAsFile())
        json = juce::JSON::parse(file);
    if (!json.isObject())
        json = juce::var(new juce::DynamicObject());

    if (auto* obj = json.getDynamicObject())
    {
        for (int deck = 0; deck < numDecks; ++deck)
        {
            const auto& theme = cached(deck);
            obj->setProperty(keyFor(deck, lowSuffix),  (int)theme.low.getARGB());
            obj->setProperty(keyFor(deck, midSuffix),  (int)theme.mid.getARGB());
            obj->setProperty(keyFor(deck, highSuffix), (int)theme.high.getARGB());
        }

        // Las claves legacy se eliminan tras el primer guardado con el
        // nuevo formato por bandeja.
        obj->removeProperty(legacyKeyFor(lowSuffix));
        obj->removeProperty(legacyKeyFor(midSuffix));
        obj->removeProperty(legacyKeyFor(highSuffix));
    }

    file.replaceWithText(juce::JSON::toString(json));
}

void WaveformTheme::applyPreset(int deck, int preset)
{
    Theme theme = defaultsForDeck(deck);

    if (preset == BlueSteel)
    {
        theme.low  = juce::Colour::fromRGB(30, 58, 95);
        theme.mid  = juce::Colour::fromRGB(62, 95, 138);
        theme.high = juce::Colour::fromRGB(127, 163, 204);
    }
    else if (preset == SilverMono)
    {
        theme.low  = juce::Colour::fromRGB(85, 85, 85);
        theme.mid  = juce::Colour::fromRGB(153, 153, 153);
        theme.high = juce::Colour::fromRGB(238, 238, 238);
    }
    else // RgbMulticolor
    {
        theme.low  = juce::Colour::fromRGB(230, 57, 70);
        theme.mid  = juce::Colour::fromRGB(255, 159, 28);
        theme.high = UiPalette::accentBright;
    }

    cached(deck) = theme;
    saveToDisk();
}

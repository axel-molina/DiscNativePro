#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "AppFonts.h"

// P37: sustituye el sans/mono de JUCE por las caras embebidas, eligiendo el
// peso real (Bold de la familia, no sintetizado). setDefaultSansSerifTypeface
// a secas ignoraría el bold, por eso la sustitución va por este override.
class AppLookAndFeel : public juce::LookAndFeel_V4
{
public:
    juce::Typeface::Ptr getTypefaceForFont (const juce::Font& font) override
    {
        const auto name = font.getTypefaceName();
        const bool bold = font.isBold();
        juce::Typeface::Ptr face;

        if (name == juce::Font::getDefaultSansSerifFontName() || name == "Sora")
            face = bold ? AppFonts::sansBold() : AppFonts::sansRegular();
        else if (name == "Menlo" || name == juce::Font::getDefaultMonospacedFontName() || name == "JetBrains Mono")
            face = bold ? AppFonts::monoBold() : AppFonts::monoRegular();

        // Sora no publica itálica: el único italic de la app cae a regular.
        // Si una cara no cargó, se degrada al comportamiento base de JUCE.
        if (face != nullptr)
            return face;

        return juce::LookAndFeel_V4::getTypefaceForFont (font);
    }
};
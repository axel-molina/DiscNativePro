#pragma once

#include <juce_graphics/juce_graphics.h>

// P37: tipografía unificada embebida (Sora + JetBrains Mono), idéntica en
// macOS, Windows y Linux. Las caras viven como estáticos tras initialise().
namespace AppFonts
{
    void initialise();

    juce::Typeface::Ptr sansRegular();
    juce::Typeface::Ptr sansBold();
    juce::Typeface::Ptr monoRegular();
    juce::Typeface::Ptr monoBold();
}
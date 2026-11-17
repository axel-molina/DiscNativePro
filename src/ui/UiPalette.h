#pragma once

// Paleta de acento de la UI: un solo azul profesional (desaturado) en lugar
// de los cian/cian-neón dispersos que había antes por cada archivo.
//
// Los tokens se usan con .withAlpha() cuando hace falta un relleno tenue:
//   UiPalette::accent       → #5B8DB8  acento principal (bordes, rellenos, degradados)
//   UiPalette::accentBright → #8FB3D9  texto/iconografía de acento sobre fondo oscuro
//   UiPalette::accentDim    → #33506B  trazos, badges y detalles de bajo énfasis
//
// Fuera de esta paleta quedan, a propósito, los colores semánticos
// (ámbar = emphasis/adiestramiento, verde = éxito, rojo = error): son señales,
// no decoración, y deben seguir leyéndose de un vistazo.

#include <juce_gui_basics/juce_gui_basics.h>

namespace UiPalette
{
    inline const juce::Colour accent       { juce::Colour::fromRGB(91, 141, 184) };  // #5B8DB8
    inline const juce::Colour accentBright { juce::Colour::fromRGB(143, 179, 217) }; // #8FB3D9
    inline const juce::Colour accentDim    { juce::Colour::fromRGB(51, 80, 107) };   // #33506B

    // Tonos de la onda por banda (graves / medios / agudos).
    inline const juce::Colour waveLowDeckOne  { juce::Colour::fromRGB(46, 75, 102) };  // #2E4B66
    inline const juce::Colour waveMidDeckOne  { juce::Colour::fromRGB(79, 115, 150) }; // #4F7396
    inline const juce::Colour waveHighDeckOne { juce::Colour::fromRGB(143, 174, 203) };// #8FAECB

    // Deck 2: terracota / ladrillo apagado (FB-19) — contraste con el azul del
    // Deck 1 sin salirse del theme de colores pálidos. El ACENTO general del
    // Deck 2 (jog, key badge, cover art) sigue siendo gris azulado #94A3B8.
    inline const juce::Colour waveLowDeckTwo  { juce::Colour::fromRGB(92, 50, 48) };   // #5C3230
    inline const juce::Colour waveMidDeckTwo  { juce::Colour::fromRGB(160, 90, 78) };  // #A05A4E
    inline const juce::Colour waveHighDeckTwo { juce::Colour::fromRGB(217, 160, 147) };// #D9A093

    // Botones de carga a deck: «1» usa el azul acero (accentBright); «2» usa
    // esta familia terracota con relleno/borde tenues sobre fondo oscuro.
    inline const juce::Colour deckTwoLoadFill   { juce::Colour::fromRGB(160, 90, 78).withAlpha(0.20f) };
    inline const juce::Colour deckTwoLoadText   { juce::Colour::fromRGB(217, 160, 147) };
    inline const juce::Colour deckTwoLoadBorder { juce::Colour::fromRGB(160, 90, 78).withAlpha(0.45f) };
}

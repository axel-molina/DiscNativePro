#pragma once

// Escala tipográfica compartida por los paneles de Ajustes (y por el panel MIDI
// que vive dentro de Ajustes). Los tamaños de fuente en JUCE se expresan en
// píxeles, no en puntos: por eso los tokens van de 10.5 (micro) a 19 (title).
namespace UiType
{
    inline constexpr float micro   = 10.5f; // texto terciario: badges, estados
    inline constexpr float caption = 11.5f; // botones secundarios, descripciones
    inline constexpr float body    = 12.5f; // etiquetas de opciones y valores
    inline constexpr float section = 13.5f; // títulos de sección
    inline constexpr float heading = 14.5f; // títulos de bloque / tarjeta
    inline constexpr float title   = 19.0f; // título de la app
}

#pragma once

#include <juce_core/juce_core.h>

// Familias de controladora soportadas. El perfil decide qué tabla de
// controles hardware se aplica y qué bindings por defecto usa la capa de
// mapeo del usuario.
enum class MidiControllerFamily
{
    Generic,
    PioneerDDJ,
    DenonMC
};

// Valores brutos (CC / notas) de la familia Denon MC (MC3000 / MC2000).
// Fuente: preset oficial de Mixxx "Denon MC3000.midi.xml".
// La Deck A usa el canal MIDI 1 y la Deck B el 3; el canal 2 se acepta como
// alias de la Deck B por si el firmware de la MC2000 difiere.
namespace DenonMidi
{
    inline constexpr int kDeckAChannel = 1;
    inline constexpr int kDeckBChannel = 3;
    inline constexpr int kDeckBAliasChannel = 2;
    inline constexpr int kMasterChannel = 3;

    // Controladores continuos
    inline constexpr int kVolumeA = 5;
    inline constexpr int kVolumeB = 16;
    inline constexpr int kTrimA = 1;
    inline constexpr int kTrimB = 18;
    inline constexpr int kHighA = 2;
    inline constexpr int kHighB = 13;
    inline constexpr int kMidA = 3;
    inline constexpr int kMidB = 14;
    inline constexpr int kLowA = 4;
    inline constexpr int kLowB = 21;
    inline constexpr int kCrossfader = 22;
    inline constexpr int kJog = 81;
    inline constexpr int kBrowse = 84;
    inline constexpr int kMasterVolume = 146;
    inline constexpr int kHeadphonesVolume = 144;
    inline constexpr int kHeadphonesMix = 137;

    // Botones (notas)
    inline constexpr int kPlay = 67;
    inline constexpr int kCue = 66;
    inline constexpr int kPflA = 1;
    inline constexpr int kPflB = 5;
    inline constexpr int kKeylock = 6;
    inline constexpr int kHotCue1 = 35;
    inline constexpr int kHotCue2 = 36;
    inline constexpr int kHotCue3 = 37;
    inline constexpr int kHotCue4 = 50;
    inline constexpr int kLoadDeck1 = 98;
    inline constexpr int kLoadDeck2 = 99;
}

class MidiControllerProfile
{
public:
    // Detecta la familia a partir de los nombres de los dispositivos MIDI
    // conectados (el nombre lo reporta el sistema operativo).
    static MidiControllerFamily detect(const juce::StringArray& deviceNames);

    static juce::String toString(MidiControllerFamily family);
    static MidiControllerFamily fromString(const juce::String& text);

    static juce::String getNameEs(MidiControllerFamily family);
    static juce::String getNameEn(MidiControllerFamily family);
};

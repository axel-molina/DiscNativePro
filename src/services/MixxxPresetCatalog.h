#pragma once

// P26: catálogo de presets de controladoras de la comunidad Mixxx.
// Solo contiene nombres de archivo y URLs (datos factuales, sin contenido GPL
// embebido): el preset se descarga en runtime al app-data del usuario y se
// parsea con nuestro propio importador.

#include <juce_core/juce_core.h>
#include <vector>

struct MixxxPresetEntry
{
    juce::String deviceMatch;   // fragmento distintivo del nombre del dispositivo (normalizado)
    juce::String presetFile;    // nombre del .midi.xml en res/controllers de Mixxx
};

class MixxxPresetCatalog
{
public:
    static const std::vector<MixxxPresetEntry>& entries();

    // Normaliza para matching: minúsculas, sin espacios/guiones/guiones bajos.
    static juce::String normalizeName(const juce::String& raw);

    // Best match contra los nombres que reporta JUCE (gana el alias más largo).
    // Devuelve true y llena out si encontró.
    static bool findForDevice(const juce::String& deviceName, MixxxPresetEntry& out);

    static juce::String presetUrl(const juce::String& presetFile);

    static juce::File presetsFolder();

    // Copia local ya descargada (si existe y es válida) o archivo inválido.
    static juce::File localPresetFile(const juce::String& presetFile);

    // Descarga (bloqueante, NO llamar desde el hilo de mensaje). Escribe en
    // presetsFolder(). Devuelve false y llena error si falla.
    static bool downloadPreset(const juce::String& presetFile, juce::String& error);
};

#pragma once

// P26: importador de presets .midi.xml de Mixxx. Parse propio (el formato XML
// está documentado en la wiki de Mixxx); los <script-binding/> (JS) se omiten
// con reporte. El crosswalk key+group → actionId cubre las acciones que
// DiscNativePro soporta; las desconocidas se listan sin importar.

#include "../midi/MidiMapping.h"
#include <juce_core/juce_core.h>
#include <vector>
#include <utility>

struct MixxxImportReport
{
    bool ok { false };
    juce::String error;
    juce::String presetName;
    juce::String author;

    int imported { 0 };
    int skippedScriptBinding { 0 };
    juce::StringArray unknownKeys; // "group / key" omitidos

    std::vector<std::pair<juce::String, MidiBinding>> bindings;

    // P27: salidas (LEDs) del preset. Se guardan aparte: solo se usan para
    // feedback visual, no para dispatch de acciones.
    std::vector<std::pair<juce::String, MidiBinding>> outputBindings;
    int outputsImported { 0 };
};

class MixxxMappingImporter
{
public:
    // Parsea el XML y arma el reporte con los bindings ya traducidos.
    static MixxxImportReport importFile(const juce::File& xmlFile);
};

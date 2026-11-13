#pragma once

// P29: importador de mappings VirtualDJ. Son dos XML: el definition
// (Documents/VirtualDJ/Devices) traduce nombres abstractos a note/CC hex +
// canal (VDJ es 0-based), y el mapper (Documents/VirtualDJ/Mappers) mapea
// esos nombres a acciones ("deck 1 play_button", "level", ...). Sin el
// definition no hay traducción a MIDI: el flujo de UI pide ambos archivos.
// Formato documentado en VDJPedia (ControllerDefinitionMIDI).

#include "../midi/MidiMapping.h"
#include <juce_core/juce_core.h>
#include <vector>
#include <utility>

struct VdjImportReport
{
    bool ok { false };
    juce::String error;
    juce::String mapperName;
    juce::String definitionName;

    int imported { 0 };
    int outputsImported { 0 };
    int controlsWithoutDefinition { 0 };
    juce::StringArray unknownActions; // "nombre / acción" sin equivalente

    std::vector<std::pair<juce::String, MidiBinding>> bindings;
    std::vector<std::pair<juce::String, MidiBinding>> outputBindings;
};

class VdjMappingImporter
{
public:
    static VdjImportReport importFiles(const juce::File& definitionXml, const juce::File& mapperXml);
};

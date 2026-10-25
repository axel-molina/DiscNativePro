#pragma once

#include <juce_core/juce_core.h>

// Formato unico de BPM para toda la UI (JogWheel, labels de bandeja y biblioteca):
// 1 decimal cuando hay deteccion fiable y "--" cuando el BPM es desconocido (0).
inline juce::String formatBpm(double bpm)
{
    return bpm > 0.0 ? juce::String(bpm, 1) : juce::String("--");
}

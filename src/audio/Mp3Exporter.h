#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

// Exportacion offline a MP3 con el LAME vendorizado (mismo codificador del broadcast).
// Bloqueante: llamar desde un hilo de trabajo, nunca desde el hilo de audio/mensajes.
namespace Mp3Exporter
{
// Devuelve cadena vacia si la exportacion fue correcta; si no, el mensaje de error.
juce::String encodeWavToMp3(const juce::File& wavFile, const juce::File& outMp3, int bitrateKbps);
}

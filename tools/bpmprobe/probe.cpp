#include <juce_audio_formats/juce_audio_formats.h>
#include "audio/BeatDetector.h"
#include <cstdio>

int wmain (int argc, wchar_t** argv)
{
    if (argc < 2) { std::printf ("uso: bpmprobe.exe <archivo>\n"); return 1; }

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();

    juce::File f { juce::String (argv[1]) };
    std::printf ("fichero: %s\n", f.getFileName().toRawUTF8());

    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f.createInputStream()));
    if (r == nullptr) { std::printf ("NO SE PUDO ABRIR\n"); return 1; }

    std::printf ("  sr=%.0f  ch=%d  dur=%.2f s\n",
                 r->sampleRate, (int) r->numChannels,
                 (double) r->lengthInSamples / r->sampleRate);

    const auto res = BeatDetector::analyze (r.get());
    std::printf ("DETECTOR: %.2f BPM   offset %.4f s\n", res.bpm, res.offset);
    return 0;
}

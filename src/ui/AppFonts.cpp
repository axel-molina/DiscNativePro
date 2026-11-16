#include "AppFonts.h"
#include "BinaryData.h"

namespace
{
    juce::Typeface::Ptr sSansRegular;
    juce::Typeface::Ptr sSansBold;
    juce::Typeface::Ptr sMonoRegular;
    juce::Typeface::Ptr sMonoBold;

    juce::Typeface::Ptr load (const char* data, int size)
    {
        if (data == nullptr || size <= 0)
            return nullptr;

        return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
    }
}

void AppFonts::initialise()
{
    sSansRegular = load (BinaryData::SoraRegular_ttf, BinaryData::SoraRegular_ttfSize);
    sSansBold    = load (BinaryData::SoraBold_ttf,    BinaryData::SoraBold_ttfSize);
    sMonoRegular = load (BinaryData::JetBrainsMonoRegular_ttf, BinaryData::JetBrainsMonoRegular_ttfSize);
    sMonoBold    = load (BinaryData::JetBrainsMonoBold_ttf,    BinaryData::JetBrainsMonoBold_ttfSize);
}

juce::Typeface::Ptr AppFonts::sansRegular() { return sSansRegular; }
juce::Typeface::Ptr AppFonts::sansBold()    { return sSansBold; }
juce::Typeface::Ptr AppFonts::monoRegular() { return sMonoRegular; }
juce::Typeface::Ptr AppFonts::monoBold()    { return sMonoBold; }
#include "MidiControllerProfile.h"

MidiControllerFamily MidiControllerProfile::detect(const juce::StringArray& deviceNames)
{
    for (const auto& name : deviceNames)
    {
        const auto lower = name.toLowerCase();

        // Denon se evalúa primero: los nombres de la familia MC incluyen
        // "MC2000"/"MC3000"/"MC4000"/"MC6000"/"DN SC2000".
        if (lower.contains("denon")
            || lower.contains("dn sc2000") || lower.contains("dnsc2000")
            || lower.contains("mc2000") || lower.contains("mc3000")
            || lower.contains("mc4000") || lower.contains("mc6000")
            || lower.contains("mc7000"))
            return MidiControllerFamily::DenonMC;

        if (lower.contains("pioneer") || lower.contains("ddj"))
            return MidiControllerFamily::PioneerDDJ;
    }

    return MidiControllerFamily::Generic;
}

juce::String MidiControllerProfile::toString(MidiControllerFamily family)
{
    switch (family)
    {
        case MidiControllerFamily::PioneerDDJ: return "pioneer";
        case MidiControllerFamily::DenonMC:    return "denon";
        case MidiControllerFamily::Generic:
        default:                              return "generic";
    }
}

MidiControllerFamily MidiControllerProfile::fromString(const juce::String& text)
{
    const auto lower = text.toLowerCase();
    if (lower == "pioneer") return MidiControllerFamily::PioneerDDJ;
    if (lower == "denon")   return MidiControllerFamily::DenonMC;
    return MidiControllerFamily::Generic;
}

juce::String MidiControllerProfile::getNameEs(MidiControllerFamily family)
{
    switch (family)
    {
        case MidiControllerFamily::PioneerDDJ: return "Pioneer DDJ";
        case MidiControllerFamily::DenonMC:    return "Denon MC";
        case MidiControllerFamily::Generic:
        default:                              return "Genérica";
    }
}

juce::String MidiControllerProfile::getNameEn(MidiControllerFamily family)
{
    switch (family)
    {
        case MidiControllerFamily::PioneerDDJ: return "Pioneer DDJ";
        case MidiControllerFamily::DenonMC:    return "Denon MC";
        case MidiControllerFamily::Generic:
        default:                              return "Generic";
    }
}

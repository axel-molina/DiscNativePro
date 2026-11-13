#include "VdjMappingImporter.h"

#include <algorithm>
#include <iterator>

namespace
{
int parseHex(const juce::String& text)
{
    juce::String s = text.trim();
    if (s.startsWithIgnoreCase("0x"))
        return s.substring(2).getHexValue32();
    return s.getIntValue();
}

MidiBinding makeBinding(MidiMessageType type, const juce::String& channelAttr, int number)
{
    MidiBinding b;
    b.type = type;
    b.channel = juce::jlimit(1, 16, parseHex(channelAttr) + 1); // VDJ es 0-based
    b.number = (type == MidiMessageType::PitchBend) ? 0 : number;
    return b;
}

juce::String stripDeckPrefix(const juce::String& actionLower)
{
    if (!actionLower.startsWith("deck"))
        return actionLower;
    return actionLower.fromFirstOccurrenceOf("deck", false, true).trim().fromFirstOccurrenceOf(" ", false, true).trim();
}

// Crosswalk verbo VDJ (sin el prefijo de deck) → actionId de DiscNativePro.
// deck ya viene resuelto 1..2. Devuelve vacío si el verbo es desconocido.
juce::String crosswalkVdjAction(const juce::String& actionLower, int deck)
{
    const juce::String d = juce::String(deck);

    const auto verb = actionLower.upToFirstOccurrenceOf(" ", false, true).trim();

    if (verb == "play_button" || verb == "play_pause" || verb == "play_stutter" || verb == "play")
        return "deck" + d + "_play";
    if (verb == "cue_stop" || verb == "cue_button" || verb == "cue" || verb == "cup")
        return "deck" + d + "_cue";
    if (verb == "sync" || verb == "beatmatch")
        return "deck" + d + "_sync";
    if (verb == "keylock" || verb == "master_tempo" || verb == "key_lock")
        return "deck" + d + "_range";
    if (verb == "pitch" || verb == "tempo")
        return "deck" + d + "_tempo";
    if (verb == "level" || verb == "volume")
        return "mixer_fader_d" + d;
    if (verb == "crossfader" || verb == "video_crossfader")
        return "mixer_crossfader";
    if (verb == "filter")
        return "eq_filter_d" + d;
    if (verb == "gain" || verb == "trim")
        return "eq_gain_d" + d;
    if (verb == "pfl" || verb == "headphone" || verb == "headphones")
        return "mixer_cue_d" + d;
    if (verb == "load_song" || verb == "load" || verb == "load_track")
        return "browser_load_d" + d;
    if (verb == "loop_roll" || verb == "roll")
        return "pad_roll_d" + d;

    // EQ: "eq low" / "eq_low" / "eq high"...
    if (actionLower.startsWith("eq"))
    {
        const auto rest = actionLower.fromFirstOccurrenceOf("eq", false, true).trim();
        if (rest.startsWith("low"))  return "eq_low_d" + d;
        if (rest.startsWith("mid"))  return "eq_mid_d" + d;
        if (rest.startsWith("high")) return "eq_high_d" + d;
        return {};
    }

    // Hotcue: "hotcue N" / "hot_cue N" / "cue_hotcue N"
    if (verb == "hotcue" || verb == "hot_cue" || verb == "cue_hotcue")
    {
        const int n = actionLower.fromFirstOccurrenceOf(verb, false, true).trim().getIntValue();
        if (n >= 1 && n <= 4)
            return "pad_hotcue" + juce::String(n) + "_d" + d;
        return {};
    }

    // Beat jump: "goto +4" / "goto -2" / "beatjump +4"
    if (verb == "goto" || verb == "beatjump")
    {
        const int arg = actionLower.fromFirstOccurrenceOf(verb, false, true).trim().getIntValue();
        if (arg > 0) return "pad_jumpfwd_d" + d;
        if (arg < 0) return "pad_jumpback_d" + d;
        return {};
    }

    return {};
}
}

VdjImportReport VdjMappingImporter::importFiles(const juce::File& definitionXml, const juce::File& mapperXml)
{
    VdjImportReport report;

    if (!definitionXml.existsAsFile() || !mapperXml.existsAsFile())
    {
        report.error = "Faltan el definition y/o el mapper de VirtualDJ";
        return report;
    }

    auto definition = juce::XmlDocument::parse(definitionXml);
    if (definition == nullptr)
    {
        report.error = "XML inválido (definition): " + definitionXml.getFileName();
        return report;
    }

    // 1. Definition: nombre abstracto → binding. Inputs: button/toggle
    //    (note o cc), slider/jog/encoder (cc, note-velocity o pitch). Salidas:
    //    led/color (feedback).
    struct NamedControl { juce::String name; MidiBinding binding; };
    std::vector<NamedControl> inputs;
    std::vector<NamedControl> outputs;

    if (definition->getTagName().equalsIgnoreCase("device"))
        report.definitionName = definition->getStringAttribute("name");

    static const char* kInputTags[] = { "button", "toggle", "slider", "jog", "encoder", "fulljog", "fullencoder" };
    static const char* kOutputTags[] = { "led", "color" };

    const auto isKnownTag = [](const juce::String& tag, const char** tags, size_t n)
    {
        return std::any_of(tags, tags + n, [&](const char* t) { return tag.equalsIgnoreCase(t); });
    };

    for (auto* el : definition->getChildIterator())
    {
        const auto tag = el->getTagName();
        const bool isInput = isKnownTag(tag, kInputTags, std::size(kInputTags));
        const bool isOutput = isKnownTag(tag, kOutputTags, std::size(kOutputTags));
        if (!isInput && !isOutput)
            continue;

        const auto name = el->getStringAttribute("name").trim();
        if (name.isEmpty())
            continue;

        MidiMessageType type = MidiMessageType::CC;
        int number = -1;
        if (el->hasAttribute("note"))
        {
            type = MidiMessageType::NoteOn;
            number = parseHex(el->getStringAttribute("note"));
        }
        else if (el->hasAttribute("cc"))
        {
            type = MidiMessageType::CC;
            number = parseHex(el->getStringAttribute("cc"));
        }
        else if (el->getStringAttribute("pitch").equalsIgnoreCase("true"))
        {
            type = MidiMessageType::PitchBend;
            number = 0;
        }
        else
        {
            continue;
        }

        auto binding = makeBinding(type, el->getStringAttribute("channel", "0"), number);
        if (isInput)
            inputs.push_back({ name, binding });
        else
            outputs.push_back({ name, binding });
    }

    if (inputs.empty() && outputs.empty())
    {
        report.error = "El definition no contiene controles MIDI (button/slider/led...)";
        return report;
    }

    // 2. Mapper: nombre → acción VDJ → actionId
    auto mapper = juce::XmlDocument::parse(mapperXml);
    if (mapper == nullptr)
    {
        report.error = "XML inválido (mapper): " + mapperXml.getFileName();
        return report;
    }

    report.mapperName = mapper->getStringAttribute("device");

    for (auto* map : mapper->getChildWithTagNameIterator("map"))
    {
        const auto name = map->getStringAttribute("value").trim();
        const auto action = map->getStringAttribute("action").trim();
        if (name.isEmpty() || action.isEmpty())
            continue;

        const auto actionLower = action.toLowerCase();

        // Deck: prefijo "deck N" / "deck left|right" de la acción; si no, el
        // atributo deck del definition; último recurso deck 1.
        int deck = 1;
        bool deckFromAction = actionLower.startsWith("deck");
        if (deckFromAction)
        {
            const auto deckTok = actionLower.fromFirstOccurrenceOf("deck", false, true).trim()
                                     .upToFirstOccurrenceOf(" ", false, true);
            if (deckTok == "left")        deck = 1;
            else if (deckTok == "right")  deck = 2;
            else                          deck = juce::jlimit(1, 2, deckTok.getIntValue());
        }
        else
        {
            for (auto* el : definition->getChildIterator())
                if (el->getStringAttribute("name").trim().equalsIgnoreCase(name)
                    && el->hasAttribute("deck"))
                {
                    deck = juce::jlimit(1, 2, el->getIntAttribute("deck", 1));
                    break;
                }
        }

        // Buscar el control en definition (inputs primero, luego LEDs).
        const NamedControl* ctrl = nullptr;
        bool isOutput = false;
        for (const auto& c : inputs)
            if (c.name.equalsIgnoreCase(name)) { ctrl = &c; break; }
        if (ctrl == nullptr)
        {
            for (const auto& c : outputs)
                if (c.name.equalsIgnoreCase(name)) { ctrl = &c; isOutput = true; break; }
        }

        if (ctrl == nullptr)
        {
            ++report.controlsWithoutDefinition;
            continue;
        }

        const auto actionId = crosswalkVdjAction(stripDeckPrefix(actionLower), deck);

        if (actionId.isEmpty())
        {
            report.unknownActions.add(name + " / " + action);
            continue;
        }

        if (isOutput)
        {
            report.outputBindings.emplace_back(actionId, ctrl->binding);
            ++report.outputsImported;
        }
        else
        {
            report.bindings.emplace_back(actionId, ctrl->binding);
            ++report.imported;
        }
    }

    report.ok = report.imported > 0 || report.outputsImported > 0;
    if (!report.ok && report.error.isEmpty())
        report.error = "El mapper no contiene acciones compatibles con DiscNativePro";
    return report;
}

#include "MixxxMappingImporter.h"

namespace
{
// status de Mixxx ("0x91") → tipo + canal (1..16) de DiscNativePro
bool parseStatus(const juce::String& statusText, MidiMessageType& type, int& channel)
{
    juce::String s = statusText.trim();
    if (s.isEmpty())
        return false;

    int value;
    if (s.startsWithIgnoreCase("0x"))
        value = s.substring(2).getHexValue32();
    else
        value = s.getIntValue();

    const int high = value & 0xF0;
    channel = (value & 0x0F) + 1;

    if (high == 0x90)
        type = MidiMessageType::NoteOn;
    else if (high == 0xB0)
        type = MidiMessageType::CC;
    else if (high == 0xE0)
        type = MidiMessageType::PitchBend;
    else
        return false;

    return true;
}

int parseMidino(const juce::String& text)
{
    juce::String s = text.trim();
    if (s.startsWithIgnoreCase("0x"))
        return s.substring(2).getHexValue32();
    return s.getIntValue();
}

// Crosswalk: por cada [ChannelN] estas keys de Mixxx tienen acción equivalente.
juce::String crosswalkChannelKey(const juce::String& key, int channel)
{
    const int d = juce::jlimit(1, 2, channel); // DiscNativePro tiene 2 decks
    if (key == "play" || key == "play_indicator" || key == "play_stutter")
                                    return "deck" + juce::String(d) + "_play";
    if (key == "cue_default" || key == "cue_gotoandplay")
                                    return "deck" + juce::String(d) + "_cue";
    if (key == "sync_enabled" || key == "beatsync") return "deck" + juce::String(d) + "_sync";
    if (key == "keylock")           return "deck" + juce::String(d) + "_range";
    if (key == "rate")              return "deck" + juce::String(d) + "_tempo";
    if (key == "volume")            return "mixer_fader_d" + juce::String(d);
    if (key == "pfl")               return "mixer_cue_d" + juce::String(d);
    if (key == "gain")              return "eq_gain_d" + juce::String(d);
    if (key == "filterHigh")        return "eq_high_d" + juce::String(d);
    if (key == "filterMid")         return "eq_mid_d" + juce::String(d);
    if (key == "filterLow")         return "eq_low_d" + juce::String(d);
    if (key == "filter")            return "eq_filter_d" + juce::String(d);
    if (key == "slip_mode")         return "pad_slip_d" + juce::String(d);

    for (int i = 1; i <= 4; ++i)
        if (key == "hotcue_" + juce::String(i) + "_activate"
            || key == "hotcue_" + juce::String(i) + "_enabled")
            return "pad_hotcue" + juce::String(i) + "_d" + juce::String(d);

    if (key == "beatjump_forward")  return "pad_jumpfwd_d" + juce::String(d);
    if (key == "beatjump_backward") return "pad_jumpback_d" + juce::String(d);

    return {};
}

juce::String crosswalkMasterKey(const juce::String& group, const juce::String& key)
{
    if (group.equalsIgnoreCase("[Master]"))
    {
        if (key == "volume")     return "mixer_master_vol";
        if (key == "crossfader") return "mixer_crossfader";
        if (key == "headVolume") return "mixer_phones_vol";
    }
    return {};
}
}

MixxxImportReport MixxxMappingImporter::importFile(const juce::File& xmlFile)
{
    MixxxImportReport report;

    if (!xmlFile.existsAsFile())
    {
        report.error = "El archivo del preset no existe";
        return report;
    }

    auto xml = juce::XmlDocument::parse(xmlFile);
    if (xml == nullptr)
    {
        report.error = "XML inválido: " + xmlFile.getFileName();
        return report;
    }

    // Crédito de la comunidad (se muestra en la UI al importar)
    if (auto* info = xml->getChildByName("info"))
    {
        if (auto* n = info->getChildByName("name"))
            report.presetName = n->getAllSubText().trim();
        if (auto* a = info->getChildByName("author"))
            report.author = a->getAllSubText().trim();
    }

    if (auto* controls = xml->getChildByName("controls"))
    {
        for (auto* ctrl : controls->getChildWithTagNameIterator("control"))
        {
            auto* statusEl = ctrl->getChildByName("status");
            auto* midinoEl = ctrl->getChildByName("midino");
            auto* groupEl = ctrl->getChildByName("group");
            auto* keyEl = ctrl->getChildByName("key");
            if (statusEl == nullptr || midinoEl == nullptr || groupEl == nullptr || keyEl == nullptr)
                continue;

            const auto group = groupEl->getAllSubText().trim();
            const auto key = keyEl->getAllSubText().trim();

            auto* options = ctrl->getChildByName("options");
            if (options != nullptr && options->getChildByName("script-binding") != nullptr)
            {
                ++report.skippedScriptBinding;
                continue;
            }

            MidiMessageType type = MidiMessageType::None;
            int channel = 0;
            if (!parseStatus(statusEl->getAllSubText(), type, channel))
            {
                report.unknownKeys.add(group + " / " + key + " (status no soportado)");
                continue;
            }

            const int num = parseMidino(midinoEl->getAllSubText());

            MidiBinding binding;
            binding.type = type;
            binding.channel = channel;
            binding.number = (type == MidiMessageType::PitchBend) ? 0 : num;

            juce::String actionId;
            if (group.startsWithIgnoreCase("[Channel"))
            {
                // El deck sale del grupo ([Channel1]/[Channel2]...), no del
                // canal MIDI; preset de 4 decks → se remapea al deck 1..2.
                const int deckFromGroup = juce::jlimit(1, 2,
                    group.fromFirstOccurrenceOf("Channel", false, true)
                         .upToFirstOccurrenceOf("]", false, true).getIntValue());
                actionId = crosswalkChannelKey(key, deckFromGroup);
            }
            else
            {
                actionId = crosswalkMasterKey(group, key);
            }

            if (actionId.isEmpty())
            {
                report.unknownKeys.add(group + " / " + key);
                continue;
            }

            report.bindings.emplace_back(actionId, binding);
            ++report.imported;
        }
    }

    // P27: salidas (LEDs). Misma estructura que <control> pero dentro de
    // <outputs>: group/key/status/midino determinan la luz a encender.
    if (auto* outputs = xml->getChildByName("outputs"))
    {
        for (auto* out : outputs->getChildWithTagNameIterator("output"))
        {
            auto* statusEl = out->getChildByName("status");
            auto* midinoEl = out->getChildByName("midino");
            auto* groupEl = out->getChildByName("group");
            auto* keyEl = out->getChildByName("key");
            if (statusEl == nullptr || midinoEl == nullptr || groupEl == nullptr || keyEl == nullptr)
                continue;

            const auto group = groupEl->getAllSubText().trim();
            const auto key = keyEl->getAllSubText().trim();

            auto* options = out->getChildByName("options");
            if (options != nullptr && options->getChildByName("script-binding") != nullptr)
                continue;

            MidiMessageType type = MidiMessageType::None;
            int channel = 0;
            if (!parseStatus(statusEl->getAllSubText(), type, channel))
                continue;

            MidiBinding binding;
            binding.type = type;
            binding.channel = channel;
            binding.number = (type == MidiMessageType::PitchBend) ? 0 : parseMidino(midinoEl->getAllSubText());

            juce::String actionId;
            if (group.startsWithIgnoreCase("[Channel"))
            {
                const int deckFromGroup = juce::jlimit(1, 2,
                    group.fromFirstOccurrenceOf("Channel", false, true)
                         .upToFirstOccurrenceOf("]", false, true).getIntValue());
                actionId = crosswalkChannelKey(key, deckFromGroup);
            }
            else
            {
                actionId = crosswalkMasterKey(group, key);
            }

            if (actionId.isEmpty())
                continue;

            report.outputBindings.emplace_back(actionId, binding);
            ++report.outputsImported;
        }
    }

    report.ok = report.imported > 0;
    if (!report.ok && report.error.isEmpty())
        report.error = "El preset no contiene controles compatibles con DiscNativePro";
    return report;
}

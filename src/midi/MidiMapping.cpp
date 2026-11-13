#include "MidiMapping.h"

MidiMappingManager::MidiMappingManager()
{
    initialiseDefaultDefinitions();
    loadFromUserFile();
}

void MidiMappingManager::initialiseDefaultDefinitions()
{
    actions.clear();

    auto add = [this](const juce::String& id,
                      const juce::String& es,
                      const juce::String& en,
                      MidiActionCategory cat,
                      int deck,
                      MidiMessageType defType,
                      int defCh,
                      int defNum)
    {
        MidiActionDefinition def;
        def.id = id;
        def.nameEs = es;
        def.nameEn = en;
        def.category = cat;
        def.deckIndex = deck;
        def.defaultBinding = { defType, defCh, defNum };
        def.binding = def.defaultBinding;
        actions.push_back(def);
    };

    // -------------------------------------------------------------------------
    // 1. TRANSPORTE - DECK 1
    // -------------------------------------------------------------------------
    add("deck1_play", "Play / Pause", "Play / Pause", MidiActionCategory::Transport, 0, MidiMessageType::NoteOn, 1, 11);
    add("deck1_cue", "Cue", "Cue", MidiActionCategory::Transport, 0, MidiMessageType::NoteOn, 1, 12);
    add("deck1_sync", "Sync Tempo", "Sync Tempo", MidiActionCategory::Transport, 0, MidiMessageType::NoteOn, 1, 88);
    add("deck1_range", "KeyLock / Rango Pitch", "KeyLock / Pitch Range", MidiActionCategory::Transport, 0, MidiMessageType::NoteOn, 1, 96);
    add("deck1_keyshift_up", "Key Shift +1 (Deck 1)", "Key Shift +1 (Deck 1)", MidiActionCategory::Transport, 0, MidiMessageType::NoteOn, 1, 97);
    add("deck1_keyshift_down", "Key Shift -1 (Deck 1)", "Key Shift -1 (Deck 1)", MidiActionCategory::Transport, 0, MidiMessageType::NoteOn, 1, 98);
    add("deck1_keyshift_reset", "Key Shift Reset (Deck 1)", "Key Shift Reset (Deck 1)", MidiActionCategory::Transport, 0, MidiMessageType::NoteOn, 1, 99);
    add("deck1_tempo", "Fader de Pitch (Tempo)", "Pitch (Tempo) Fader", MidiActionCategory::Transport, 0, MidiMessageType::PitchBend, 1, 0);
    add("deck1_jog_ring", "Jog Wheel Anillo (Pitch Bend)", "Jog Wheel Outer Ring (Nudge)", MidiActionCategory::Transport, 0, MidiMessageType::CC, 1, 33);
    add("deck1_jog_platter", "Jog Wheel Platter (Scratch)", "Jog Wheel Platter (Scratch)", MidiActionCategory::Transport, 0, MidiMessageType::CC, 1, 34);

    // -------------------------------------------------------------------------
    // 2. TRANSPORTE - DECK 2
    // -------------------------------------------------------------------------
    add("deck2_play", "Play / Pause", "Play / Pause", MidiActionCategory::Transport, 1, MidiMessageType::NoteOn, 2, 11);
    add("deck2_cue", "Cue", "Cue", MidiActionCategory::Transport, 1, MidiMessageType::NoteOn, 2, 12);
    add("deck2_sync", "Sync Tempo", "Sync Tempo", MidiActionCategory::Transport, 1, MidiMessageType::NoteOn, 2, 88);
    add("deck2_range", "KeyLock / Rango Pitch", "KeyLock / Pitch Range", MidiActionCategory::Transport, 1, MidiMessageType::NoteOn, 2, 96);
    add("deck2_keyshift_up", "Key Shift +1 (Deck 2)", "Key Shift +1 (Deck 2)", MidiActionCategory::Transport, 1, MidiMessageType::NoteOn, 2, 97);
    add("deck2_keyshift_down", "Key Shift -1 (Deck 2)", "Key Shift -1 (Deck 2)", MidiActionCategory::Transport, 1, MidiMessageType::NoteOn, 2, 98);
    add("deck2_keyshift_reset", "Key Shift Reset (Deck 2)", "Key Shift Reset (Deck 2)", MidiActionCategory::Transport, 1, MidiMessageType::NoteOn, 2, 99);
    add("deck2_tempo", "Fader de Pitch (Tempo)", "Pitch (Tempo) Fader", MidiActionCategory::Transport, 1, MidiMessageType::PitchBend, 2, 0);
    add("deck2_jog_ring", "Jog Wheel Anillo (Pitch Bend)", "Jog Wheel Outer Ring (Nudge)", MidiActionCategory::Transport, 1, MidiMessageType::CC, 2, 33);
    add("deck2_jog_platter", "Jog Wheel Platter (Scratch)", "Jog Wheel Platter (Scratch)", MidiActionCategory::Transport, 1, MidiMessageType::CC, 2, 34);

    // -------------------------------------------------------------------------
    // 3. MEZCLADOR
    // -------------------------------------------------------------------------
    add("mixer_fader_d1", "Fader Volumen Canal 1", "Channel 1 Volume Fader", MidiActionCategory::Mixer, 0, MidiMessageType::CC, 1, 19);
    add("mixer_fader_d2", "Fader Volumen Canal 2", "Channel 2 Volume Fader", MidiActionCategory::Mixer, 1, MidiMessageType::CC, 2, 19);
    add("mixer_crossfader", "Crossfader", "Crossfader", MidiActionCategory::Mixer, -1, MidiMessageType::CC, 7, 31);
    add("mixer_master_vol", "Volumen Master", "Master Volume", MidiActionCategory::Mixer, -1, MidiMessageType::CC, 0, 14);
    add("mixer_phones_vol", "Volumen Auriculares", "Headphones Volume", MidiActionCategory::Mixer, -1, MidiMessageType::CC, 7, 5);
    add("mixer_cue_d1", "Preescucha Auriculares (PFL 1)", "Headphone Cue (PFL 1)", MidiActionCategory::Mixer, 0, MidiMessageType::NoteOn, 1, 84);
    add("mixer_cue_d2", "Preescucha Auriculares (PFL 2)", "Headphone Cue (PFL 2)", MidiActionCategory::Mixer, 1, MidiMessageType::NoteOn, 2, 84);

    // -------------------------------------------------------------------------
    // 4. ECUALIZADOR Y FILTROS
    // -------------------------------------------------------------------------
    add("eq_gain_d1", "Gain / Trim Canal 1", "Channel 1 Gain / Trim", MidiActionCategory::EQ, 0, MidiMessageType::CC, 1, 4);
    add("eq_high_d1", "EQ Agudos (High) Canal 1", "Channel 1 High EQ", MidiActionCategory::EQ, 0, MidiMessageType::CC, 1, 7);
    add("eq_mid_d1", "EQ Medios (Mid) Canal 1", "Channel 1 Mid EQ", MidiActionCategory::EQ, 0, MidiMessageType::CC, 1, 11);
    add("eq_low_d1", "EQ Graves (Low) Canal 1", "Channel 1 Low EQ", MidiActionCategory::EQ, 0, MidiMessageType::CC, 1, 15);
    add("eq_filter_d1", "Filtro LPF/HPF Canal 1", "Channel 1 Filter", MidiActionCategory::EQ, 0, MidiMessageType::CC, 7, 23);

    add("eq_gain_d2", "Gain / Trim Canal 2", "Channel 2 Gain / Trim", MidiActionCategory::EQ, 1, MidiMessageType::CC, 2, 4);
    add("eq_high_d2", "EQ Agudos (High) Canal 2", "Channel 2 High EQ", MidiActionCategory::EQ, 1, MidiMessageType::CC, 2, 7);
    add("eq_mid_d2", "EQ Medios (Mid) Canal 2", "Channel 2 Mid EQ", MidiActionCategory::EQ, 1, MidiMessageType::CC, 2, 11);
    add("eq_low_d2", "EQ Graves (Low) Canal 2", "Channel 2 Low EQ", MidiActionCategory::EQ, 1, MidiMessageType::CC, 2, 15);
    add("eq_filter_d2", "Filtro LPF/HPF Canal 2", "Channel 2 Filter", MidiActionCategory::EQ, 1, MidiMessageType::CC, 7, 24);

    // -------------------------------------------------------------------------
    // 5. PADS Y HOT CUES
    // -------------------------------------------------------------------------
    add("pad_hotcue1_d1", "Hot Cue 1 (Deck 1)", "Hot Cue 1 (Deck 1)", MidiActionCategory::Pads, 0, MidiMessageType::NoteOn, 8, 0);
    add("pad_hotcue2_d1", "Hot Cue 2 (Deck 1)", "Hot Cue 2 (Deck 1)", MidiActionCategory::Pads, 0, MidiMessageType::NoteOn, 8, 1);
    add("pad_hotcue3_d1", "Hot Cue 3 (Deck 1)", "Hot Cue 3 (Deck 1)", MidiActionCategory::Pads, 0, MidiMessageType::NoteOn, 8, 2);
    add("pad_hotcue4_d1", "Hot Cue 4 (Deck 1)", "Hot Cue 4 (Deck 1)", MidiActionCategory::Pads, 0, MidiMessageType::NoteOn, 8, 3);

    add("pad_hotcue1_d2", "Hot Cue 1 (Deck 2)", "Hot Cue 1 (Deck 2)", MidiActionCategory::Pads, 1, MidiMessageType::NoteOn, 9, 0);
    add("pad_hotcue2_d2", "Hot Cue 2 (Deck 2)", "Hot Cue 2 (Deck 2)", MidiActionCategory::Pads, 1, MidiMessageType::NoteOn, 9, 1);
    add("pad_hotcue3_d2", "Hot Cue 3 (Deck 2)", "Hot Cue 3 (Deck 2)", MidiActionCategory::Pads, 1, MidiMessageType::NoteOn, 9, 2);
    add("pad_hotcue4_d2", "Hot Cue 4 (Deck 2)", "Hot Cue 4 (Deck 2)", MidiActionCategory::Pads, 1, MidiMessageType::NoteOn, 9, 3);

    // P15: performance (beat jump usa el size selector de la UI; roll es hold)
    add("pad_jumpback_d1", "Beat Jump Atrás (Deck 1)", "Beat Jump Back (Deck 1)", MidiActionCategory::Pads, 0, MidiMessageType::NoteOn, 8, 4);
    add("pad_jumpfwd_d1", "Beat Jump Adelante (Deck 1)", "Beat Jump Forward (Deck 1)", MidiActionCategory::Pads, 0, MidiMessageType::NoteOn, 8, 5);
    add("pad_roll_d1", "Loop Roll (mantener, Deck 1)", "Loop Roll (hold, Deck 1)", MidiActionCategory::Pads, 0, MidiMessageType::NoteOn, 8, 6);
    add("pad_slip_d1", "Slip Mode (Deck 1)", "Slip Mode (Deck 1)", MidiActionCategory::Pads, 0, MidiMessageType::NoteOn, 8, 7);

    add("pad_jumpback_d2", "Beat Jump Atrás (Deck 2)", "Beat Jump Back (Deck 2)", MidiActionCategory::Pads, 1, MidiMessageType::NoteOn, 9, 4);
    add("pad_jumpfwd_d2", "Beat Jump Adelante (Deck 2)", "Beat Jump Forward (Deck 2)", MidiActionCategory::Pads, 1, MidiMessageType::NoteOn, 9, 5);
    add("pad_roll_d2", "Loop Roll (mantener, Deck 2)", "Loop Roll (hold, Deck 2)", MidiActionCategory::Pads, 1, MidiMessageType::NoteOn, 9, 6);
    add("pad_slip_d2", "Slip Mode (Deck 2)", "Slip Mode (Deck 2)", MidiActionCategory::Pads, 1, MidiMessageType::NoteOn, 9, 7);

    // P22: stems (toggle binario de ganancia + master on/off)
    add("pad_stems_active_d1", "Stems ON/OFF (Deck 1)", "Stems ON/OFF (Deck 1)", MidiActionCategory::Pads, 0, MidiMessageType::NoteOn, 8, 100);
    add("pad_stem_vocals_d1", "Stem Vocals (Deck 1)", "Stem Vocals (Deck 1)", MidiActionCategory::Pads, 0, MidiMessageType::NoteOn, 8, 101);
    add("pad_stem_drums_d1", "Stem Drums (Deck 1)", "Stem Drums (Deck 1)", MidiActionCategory::Pads, 0, MidiMessageType::NoteOn, 8, 102);
    add("pad_stem_bass_d1", "Stem Bass (Deck 1)", "Stem Bass (Deck 1)", MidiActionCategory::Pads, 0, MidiMessageType::NoteOn, 8, 103);
    add("pad_stem_other_d1", "Stem Other (Deck 1)", "Stem Other (Deck 1)", MidiActionCategory::Pads, 0, MidiMessageType::NoteOn, 8, 104);

    add("pad_stems_active_d2", "Stems ON/OFF (Deck 2)", "Stems ON/OFF (Deck 2)", MidiActionCategory::Pads, 1, MidiMessageType::NoteOn, 9, 100);
    add("pad_stem_vocals_d2", "Stem Vocals (Deck 2)", "Stem Vocals (Deck 2)", MidiActionCategory::Pads, 1, MidiMessageType::NoteOn, 9, 101);
    add("pad_stem_drums_d2", "Stem Drums (Deck 2)", "Stem Drums (Deck 2)", MidiActionCategory::Pads, 1, MidiMessageType::NoteOn, 9, 102);
    add("pad_stem_bass_d2", "Stem Bass (Deck 2)", "Stem Bass (Deck 2)", MidiActionCategory::Pads, 1, MidiMessageType::NoteOn, 9, 103);
    add("pad_stem_other_d2", "Stem Other (Deck 2)", "Stem Other (Deck 2)", MidiActionCategory::Pads, 1, MidiMessageType::NoteOn, 9, 104);

    // -------------------------------------------------------------------------
    // 6. BIBLIOTECA Y NAVEGACIÓN
    // -------------------------------------------------------------------------
    add("browser_rotate", "Girar Perilla Navegación (Scroll)", "Browse Knob (Scroll Tracks)", MidiActionCategory::Browser, -1, MidiMessageType::CC, 7, 64);
    add("browser_push", "Pulsar Perilla Navegación", "Browse Knob Push (Enter/Toggle)", MidiActionCategory::Browser, -1, MidiMessageType::NoteOn, 7, 65);
    add("browser_load_d1", "Cargar en Deck 1", "Load to Deck 1", MidiActionCategory::Browser, 0, MidiMessageType::NoteOn, 7, 70);
    add("browser_load_d2", "Cargar en Deck 2", "Load to Deck 2", MidiActionCategory::Browser, 1, MidiMessageType::NoteOn, 7, 71);
}

const MidiActionDefinition* MidiMappingManager::findAction(const juce::String& id) const
{
    for (const auto& a : actions)
    {
        if (a.id == id)
            return &a;
    }
    return nullptr;
}

MidiActionDefinition* MidiMappingManager::findAction(const juce::String& id)
{
    for (auto& a : actions)
    {
        if (a.id == id)
            return &a;
    }
    return nullptr;
}

const MidiActionDefinition* MidiMappingManager::matchMessage(const juce::MidiMessage& msg) const
{
    for (const auto& a : actions)
    {
        if (a.binding.matches(msg))
            return &a;
    }
    return nullptr;
}

void MidiMappingManager::startLearning(const juce::String& actionId)
{
    if (findAction(actionId) != nullptr)
    {
        learningActive = true;
        learningActionId = actionId;

        if (onLearningStarted)
            onLearningStarted(actionId);
    }
}

void MidiMappingManager::cancelLearning()
{
    if (learningActive)
    {
        learningActive = false;
        juce::String prevId = learningActionId;
        learningActionId = {};

        if (onLearningFinished)
            onLearningFinished(prevId, {});
    }
}

bool MidiMappingManager::processLearningMessage(const juce::MidiMessage& msg)
{
    if (!learningActive || learningActionId.isEmpty())
        return false;

    MidiBinding newBinding;
    if (msg.isNoteOn())
    {
        newBinding.type = MidiMessageType::NoteOn;
        newBinding.channel = msg.getChannel();
        newBinding.number = msg.getNoteNumber();
    }
    else if (msg.isController())
    {
        newBinding.type = MidiMessageType::CC;
        newBinding.channel = msg.getChannel();
        newBinding.number = msg.getControllerNumber();
    }
    else if (msg.isPitchWheel())
    {
        newBinding.type = MidiMessageType::PitchBend;
        newBinding.channel = msg.getChannel();
        newBinding.number = 0;
    }
    else
    {
        return false;
    }

    juce::String targetId = learningActionId;
    learningActive = false;
    learningActionId = {};

    setBinding(targetId, newBinding);

    if (onLearningFinished)
        onLearningFinished(targetId, newBinding);

    saveToUserFile();
    return true;
}

void MidiMappingManager::setBinding(const juce::String& actionId, const MidiBinding& binding)
{
    if (auto* a = findAction(actionId))
    {
        a->binding = binding;
        if (onMappingChanged)
            onMappingChanged();
    }
}

void MidiMappingManager::clearBinding(const juce::String& actionId)
{
    if (auto* a = findAction(actionId))
    {
        a->binding.clear();
        saveToUserFile();
        if (onMappingChanged)
            onMappingChanged();
    }
}

void MidiMappingManager::resetToDefaults()
{
    for (auto& a : actions)
    {
        a.binding = a.defaultBinding;
    }
    saveToUserFile();
    if (onMappingChanged)
        onMappingChanged();
}

int MidiMappingManager::applyImportedBindings(const std::vector<std::pair<juce::String, MidiBinding>>& bindings)
{
    int applied = 0;
    for (const auto& [actionId, binding] : bindings)
    {
        if (!binding.isValid())
            continue;
        if (auto* a = findAction(actionId))
        {
            a->binding = binding;
            ++applied;
        }
    }

    if (applied > 0)
    {
        saveToUserFile();
        if (onMappingChanged)
            onMappingChanged();
    }
    return applied;
}

int MidiMappingManager::applyImportedOutputBindings(const std::vector<std::pair<juce::String, MidiBinding>>& bindings)
{
    int applied = 0;
    for (const auto& [actionId, binding] : bindings)
    {
        if (!binding.isValid())
            continue;
        outputBindings[actionId] = binding;
        ++applied;
    }

    if (applied > 0)
        saveToUserFile();
    return applied;
}

const MidiBinding* MidiMappingManager::findOutputBinding(const juce::String& actionId) const
{
    auto it = outputBindings.find(actionId);
    return (it != outputBindings.end()) ? &it->second : nullptr;
}

juce::File MidiMappingManager::getUserMappingFile() const
{
    auto appDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                     .getChildFile("DiscNativePro");
    if (!appDir.exists())
        appDir.createDirectory();

    return appDir.getChildFile("midi_mapping.json");
}

juce::var MidiMappingManager::toVar() const
{
    auto* rootObj = new juce::DynamicObject();
    rootObj->setProperty("version", 1);
    rootObj->setProperty("name", "DiscNativePro Custom Mapping");

    juce::Array<juce::var> arr;
    for (const auto& a : actions)
    {
        auto* actObj = new juce::DynamicObject();
        actObj->setProperty("id", a.id);

        juce::String typeStr = "none";
        if (a.binding.type == MidiMessageType::NoteOn) typeStr = "note";
        else if (a.binding.type == MidiMessageType::CC) typeStr = "cc";
        else if (a.binding.type == MidiMessageType::PitchBend) typeStr = "pitchbend";

        actObj->setProperty("type", typeStr);
        actObj->setProperty("channel", a.binding.channel);
        actObj->setProperty("number", a.binding.number);

        arr.add(juce::var(actObj));
    }

    rootObj->setProperty("mappings", arr);

    // P27: salidas (LEDs) — misma codificación que los mappings.
    juce::Array<juce::var> outArr;
    for (const auto& [actionId, b] : outputBindings)
    {
        auto* outObj = new juce::DynamicObject();
        outObj->setProperty("id", actionId);

        juce::String typeStr = "none";
        if (b.type == MidiMessageType::NoteOn) typeStr = "note";
        else if (b.type == MidiMessageType::CC) typeStr = "cc";
        else if (b.type == MidiMessageType::PitchBend) typeStr = "pitchbend";

        outObj->setProperty("type", typeStr);
        outObj->setProperty("channel", b.channel);
        outObj->setProperty("number", b.number);
        outArr.add(juce::var(outObj));
    }
    rootObj->setProperty("outputs", outArr);

    return juce::var(rootObj);
}

bool MidiMappingManager::fromVar(const juce::var& v)
{
    if (!v.isObject())
        return false;

    auto* obj = v.getDynamicObject();
    if (obj == nullptr)
        return false;

    auto mappingsVar = obj->getProperty("mappings");
    if (!mappingsVar.isArray())
        return false;

    auto* arr = mappingsVar.getArray();
    for (const auto& item : *arr)
    {
        if (item.isObject())
        {
            auto* itemObj = item.getDynamicObject();
            juce::String id = itemObj->getProperty("id").toString();
            juce::String typeStr = itemObj->getProperty("type").toString();
            int ch = (int)itemObj->getProperty("channel");
            int num = (int)itemObj->getProperty("number");

            MidiBinding b;
            if (typeStr == "note") b.type = MidiMessageType::NoteOn;
            else if (typeStr == "cc") b.type = MidiMessageType::CC;
            else if (typeStr == "pitchbend") b.type = MidiMessageType::PitchBend;
            else b.type = MidiMessageType::None;

            b.channel = ch;
            b.number = num;

            if (auto* a = findAction(id))
                a->binding = b;
        }
    }

    if (auto outputsVar = obj->getProperty("outputs"); outputsVar.isArray())
    {
        outputBindings.clear();
        for (const auto& item : *outputsVar.getArray())
        {
            if (!item.isObject())
                continue;
            auto* itemObj = item.getDynamicObject();
            juce::String id = itemObj->getProperty("id").toString();
            juce::String typeStr = itemObj->getProperty("type").toString();

            MidiBinding b;
            if (typeStr == "note") b.type = MidiMessageType::NoteOn;
            else if (typeStr == "cc") b.type = MidiMessageType::CC;
            else if (typeStr == "pitchbend") b.type = MidiMessageType::PitchBend;

            if (b.isValid() && id.isNotEmpty())
            {
                b.channel = (int)itemObj->getProperty("channel");
                b.number = (int)itemObj->getProperty("number");
                outputBindings[id] = b;
            }
        }
    }

    if (onMappingChanged)
        onMappingChanged();

    return true;
}

bool MidiMappingManager::saveToUserFile()
{
    return exportToFile(getUserMappingFile());
}

bool MidiMappingManager::loadFromUserFile()
{
    auto file = getUserMappingFile();
    if (!file.existsAsFile())
        return false;

    return importFromFile(file);
}

bool MidiMappingManager::exportToFile(const juce::File& file)
{
    auto v = toVar();
    juce::String json = juce::JSON::toString(v, true);
    return file.replaceWithText(json);
}

bool MidiMappingManager::importFromFile(const juce::File& file)
{
    if (!file.existsAsFile())
        return false;

    juce::var parsed = juce::JSON::parse(file);
    return fromVar(parsed);
}

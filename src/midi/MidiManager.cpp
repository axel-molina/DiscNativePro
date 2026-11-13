#include "MidiManager.h"

MidiManager::MidiManager(AudioEngine& audioEngine)
    : engine(audioEngine)
{
}

MidiManager::~MidiManager()
{
    shutdown();
}

void MidiManager::initialise()
{
    rescanDevices();
    startTimerHz(40);
}

void MidiManager::shutdown()
{
    stopTimer();
    disconnectAll();
}

// P28: desconexión sin stopTimer — rescanDevices() la usa desde el timer
// (hotplug); antes shutdown() mataba el timer y el MIDI quedaba muerto.
void MidiManager::disconnectAll()
{
    for (const auto& id : registeredInputIds)
        engine.getDeviceManager().removeMidiInputDeviceCallback(id, this);
    registeredInputIds.clear();
    midiOutput.reset();
}

void MidiManager::rescanDevices()
{
    disconnectAll();

    auto devices = juce::MidiInput::getAvailableDevices();
    connectedDevices.clear();

    for (const auto& dev : devices)
    {
        engine.getDeviceManager().setMidiInputDeviceEnabled(dev.identifier, true);
        engine.getDeviceManager().addMidiInputDeviceCallback(dev.identifier, this);
        registeredInputIds.add(dev.identifier);
        connectedDevices.add(dev.name);
    }

    // Connect to corresponding MIDI output port for LED feedback (Pioneer DDJ-SB2, etc.)
    auto outDevices = juce::MidiOutput::getAvailableDevices();
    for (const auto& outDev : outDevices)
    {
        if (outDev.name.containsIgnoreCase("DDJ") || outDev.name.containsIgnoreCase("Pioneer"))
        {
            midiOutput = juce::MidiOutput::openDevice(outDev.identifier);
            break;
        }
    }
    if (midiOutput == nullptr && !outDevices.isEmpty())
    {
        midiOutput = juce::MidiOutput::openDevice(outDevices[0].identifier);
    }

    // P27: al (re)abrir una salida, reenviar el estado completo de los LEDs.
    ledsDirty = true;

    deviceConnected = !connectedDevices.isEmpty();

    // P28: baseline del diff de hotplug.
    lastInputIds.clear();
    for (const auto& dev : devices)
        lastInputIds.add(dev.identifier);
    lastOutputIds.clear();
    for (const auto& outDev : outDevices)
        lastOutputIds.add(outDev.identifier);

    if (onDevicesChanged)
        onDevicesChanged();
}

void MidiManager::sendMidiMessage(const juce::MidiMessage& message)
{
    if (midiOutput != nullptr)
        midiOutput->sendMessageNow(message);
}

juce::StringArray MidiManager::getConnectedDeviceNames() const
{
    return connectedDevices;
}

void MidiManager::apply14BitTempo(int deckIndex)
{
    if (deckIndex < 0 || deckIndex >= 2)
        return;

    int val14 = (tempoMsb[deckIndex] << 7) | (tempoLsb[deckIndex] & 0x7F); // 0 to 16383

    // Pioneer DDJ-SB2 hardware potentiometer orientation:
    // Center detent: 8192 (MSB=64, LSB=0) -> 0.0% pitch delta
    // Fader DOWN (towards + / faster): val14 decreases towards 0 -> delta > 0 -> positive pitch %
    // Fader UP   (towards - / slower): val14 increases towards 16383 -> delta < 0 -> negative pitch %
    float norm = 0.0f;
    int delta = 8192 - val14;
    if (std::abs(delta) > 48) // deadzone of +/- 48 ticks around center (~0.3% margin)
    {
        norm = static_cast<float>(delta) / 8192.0f;
        norm = juce::jlimit(-1.0f, 1.0f, norm);
    }

    auto& deck = engine.getDeck(deckIndex);
    float targetPercent = norm * deck.getPitchRange();
    deck.setPitchPercent(targetPercent);
}

void MidiManager::timerCallback()
{
    for (int d = 0; d < 2; ++d)
    {
        if (jogBendActive[d])
        {
            jogBendTicks[d]--;
            if (jogBendTicks[d] <= 0)
            {
                engine.getDeck(d).setPitchBend(0.0f);
                jogBendActive[d] = false;
            }
        }
    }

    updateLeds(); // P27: feedback LED por diff (25 Hz efectivos con los envíos reales)

    // P28: hotplug — sondeo del diff de dispositivos cada ~2 s (40 Hz × 80).
    if (++hotplugPollCount >= 80)
    {
        hotplugPollCount = 0;

        bool changed = false;
        const auto inDevices = juce::MidiInput::getAvailableDevices();
        if (inDevices.size() != lastInputIds.size())
        {
            changed = true;
        }
        else
        {
            for (int i = 0; i < inDevices.size(); ++i)
                if (inDevices[i].identifier != lastInputIds[i]) { changed = true; break; }
        }

        const auto outDevices = juce::MidiOutput::getAvailableDevices();
        if (!changed)
        {
            if (outDevices.size() != lastOutputIds.size())
                changed = true;
            else
                for (int i = 0; i < outDevices.size(); ++i)
                    if (outDevices[i].identifier != lastOutputIds[i]) { changed = true; break; }
        }

        if (changed)
            rescanDevices(); // no detiene el timer (usa disconnectAll)
    }
}

// P27: follower de LED. Para cada acción con estado binario consulta el
// proveedor (MainComponent → decks/mezclador) y envía NoteOn vel 127 / 0
// únicamente cuando el estado cambia. El mensaje de salida sale del binding
// de <outputs> importado (Mixxx) si existe; si no, del binding de entrada
// de la acción (learn/manual/default).
void MidiManager::updateLeds()
{
    if (midiOutput == nullptr || !ledStateProvider)
        return;

    static constexpr const char* kLedActions[] = {
        "deck1_play", "deck2_play",
        "mixer_cue_d1", "mixer_cue_d2",
        "deck1_range", "deck2_range",
        "pad_slip_d1", "pad_slip_d2",
        "pad_hotcue1_d1", "pad_hotcue2_d1", "pad_hotcue3_d1", "pad_hotcue4_d1",
        "pad_hotcue1_d2", "pad_hotcue2_d2", "pad_hotcue3_d2", "pad_hotcue4_d2",
        "pad_stems_active_d1", "pad_stems_active_d2"
    };

    for (const auto* id : kLedActions)
    {
        const juce::String actionId(id);
        const bool state = ledStateProvider(actionId);

        auto it = ledLastState.find(actionId);
        if (!ledsDirty && it != ledLastState.end() && it->second == state)
            continue;

        const MidiBinding* outBinding = mappingManager.findOutputBinding(actionId);
        if (outBinding == nullptr)
        {
            if (const auto* def = mappingManager.findAction(actionId))
                outBinding = &def->binding;
        }

        if (outBinding != nullptr && outBinding->type == MidiMessageType::NoteOn && outBinding->channel > 0)
        {
            const int ch = outBinding->channel;
            const int note = outBinding->number;
            if (state)
                sendMidiMessage(juce::MidiMessage::noteOn(ch, note, (juce::uint8)127));
            else
                sendMidiMessage(juce::MidiMessage::noteOff(ch, note));
        }

        ledLastState[actionId] = state;
    }

    ledsDirty = false;
}

void MidiManager::handleJogWheel(int deckIndex, int cc, int val)
{
    if (deckIndex < 0 || deckIndex >= 2)
        return;

    auto& deck = engine.getDeck(deckIndex);
    if (!deck.isLoaded())
        return;

    // Relative signed delta centered at 64 (0x40):
    // val == 65 -> +1 (forward / clockwise)
    // val == 63 -> -1 (backward / counter-clockwise)
    int delta = val - 64;
    if (delta == 0)
        return;

    // DJ Standard sensitivity: ~5.0 seconds per full turn (~600 ticks/revolution)
    constexpr double kSecPerTick = 0.00833;

    // Shifted mode (CC 38 or CC 31): Fast search / seek across track (5x: ~25 sec/turn)
    if (cc == 38 || cc == 31)
    {
        deck.seekRelative(static_cast<double>(delta) * kSecPerTick * 5.0);
        return;
    }

    if (!deck.isPlaying())
    {
        // When PAUSED: Jog wheel performs high-precision audio scrubbing / cue placement
        deck.seekRelative(static_cast<double>(delta) * kSecPerTick);
    }
    else
    {
        // When PLAYING:
        if (cc == 33)
        {
            // Outer ring (JOGDIALSIDE): Temporary Pitch Bend (nudge to align beats)
            float bend = juce::jlimit(-0.15f, 0.15f, static_cast<float>(delta) * 0.025f);
            deck.setPitchBend(bend);
            jogBendActive[deckIndex] = true;
            jogBendTicks[deckIndex] = 4; // ~100 ms hold at 40Hz before smoothly resetting
        }
        else // cc == 34 (Top platter)
        {
            // Scratch / seek
            deck.seekRelative(static_cast<double>(delta) * kSecPerTick);
        }
    }
}

bool MidiManager::dispatchMappedAction(const MidiActionDefinition& def, const juce::MidiMessage& message)
{
    float norm = 0.0f;
    int val = 0;
    if (message.isController())
    {
        val = message.getControllerValue();
        norm = static_cast<float>(val) / 127.0f;
    }
    else if (message.isPitchWheel())
    {
        int pitchVal = message.getPitchWheelValue();
        int delta = pitchVal - 8192;
        if (std::abs(delta) > 32)
        {
            norm = (float)delta / 8192.0f;
            norm = juce::jlimit(-1.0f, 1.0f, norm);
        }
    }

    const auto& id = def.id;

    // Transporte Deck 1
    if (id == "deck1_play")
    {
        if (message.isNoteOn())
        {
            if (onPlayPause) juce::MessageManager::callAsync([this]() { if (onPlayPause) onPlayPause(0); });
            else { auto& d = engine.getDeck(0); if (d.isPlaying()) d.pause(); else d.play(); }
            return true;
        }
    }
    else if (id == "deck1_cue")
    {
        if (message.isNoteOn())
        {
            if (onCue) juce::MessageManager::callAsync([this]() { if (onCue) onCue(0); });
            else engine.getDeck(0).triggerCue();
            return true;
        }
    }
    else if (id == "deck1_sync")
    {
        if (message.isNoteOn())
        {
            engine.syncDeckTo(0);
            return true;
        }
    }
    else if (id == "deck1_range")
    {
        if (message.isNoteOn())
        {
            auto& d1 = engine.getDeck(0);
            float cur = d1.getPitchRange();
            d1.setPitchRange((cur < 12.0f) ? 16.0f : ((cur < 30.0f) ? 50.0f : 8.0f));
            return true;
        }
    }
    else if (id == "deck1_keyshift_up" || id == "deck1_keyshift_down" || id == "deck1_keyshift_reset")
    {
        if (message.isNoteOn())
        {
            auto& d1 = engine.getDeck(0);
            if (id.endsWith("up"))          d1.setKeyShiftSemitones(d1.getKeyShiftSemitones() + 1);
            else if (id.endsWith("down"))   d1.setKeyShiftSemitones(d1.getKeyShiftSemitones() - 1);
            else                            d1.setKeyShiftSemitones(0);
            return true;
        }
    }
    else if (id == "deck1_tempo")
    {
        auto& d1 = engine.getDeck(0);
        if (message.isPitchWheel())
        {
            d1.setPitchPercent(norm * d1.getPitchRange());
            return true;
        }
        else if (message.isController())
        {
            d1.setPitchPercent((norm * 2.0f - 1.0f) * d1.getPitchRange());
            return true;
        }
    }
    else if (id == "deck1_jog_ring")
    {
        if (message.isController())
        {
            handleJogWheel(0, 33, val);
            return true;
        }
    }
    else if (id == "deck1_jog_platter")
    {
        if (message.isController())
        {
            handleJogWheel(0, 34, val);
            return true;
        }
    }

    // Transporte Deck 2
    else if (id == "deck2_play")
    {
        if (message.isNoteOn())
        {
            if (onPlayPause) juce::MessageManager::callAsync([this]() { if (onPlayPause) onPlayPause(1); });
            else { auto& d = engine.getDeck(1); if (d.isPlaying()) d.pause(); else d.play(); }
            return true;
        }
    }
    else if (id == "deck2_cue")
    {
        if (message.isNoteOn())
        {
            if (onCue) juce::MessageManager::callAsync([this]() { if (onCue) onCue(1); });
            else engine.getDeck(1).triggerCue();
            return true;
        }
    }
    else if (id == "deck2_sync")
    {
        if (message.isNoteOn())
        {
            engine.syncDeckTo(1);
            return true;
        }
    }
    else if (id == "deck2_range")
    {
        if (message.isNoteOn())
        {
            auto& d2 = engine.getDeck(1);
            float cur = d2.getPitchRange();
            d2.setPitchRange((cur < 12.0f) ? 16.0f : ((cur < 30.0f) ? 50.0f : 8.0f));
            return true;
        }
    }
    else if (id == "deck2_keyshift_up" || id == "deck2_keyshift_down" || id == "deck2_keyshift_reset")
    {
        if (message.isNoteOn())
        {
            auto& d2 = engine.getDeck(1);
            if (id.endsWith("up"))          d2.setKeyShiftSemitones(d2.getKeyShiftSemitones() + 1);
            else if (id.endsWith("down"))   d2.setKeyShiftSemitones(d2.getKeyShiftSemitones() - 1);
            else                            d2.setKeyShiftSemitones(0);
            return true;
        }
    }
    else if (id == "deck2_tempo")
    {
        auto& d2 = engine.getDeck(1);
        if (message.isPitchWheel())
        {
            d2.setPitchPercent(norm * d2.getPitchRange());
            return true;
        }
        else if (message.isController())
        {
            d2.setPitchPercent((norm * 2.0f - 1.0f) * d2.getPitchRange());
            return true;
        }
    }
    else if (id == "deck2_jog_ring")
    {
        if (message.isController())
        {
            handleJogWheel(1, 33, val);
            return true;
        }
    }
    else if (id == "deck2_jog_platter")
    {
        if (message.isController())
        {
            handleJogWheel(1, 34, val);
            return true;
        }
    }

    // Mezclador
    else if (id == "mixer_fader_d1")
    {
        if (message.isController()) { engine.getMixer().getChannel(0).volumeFader.store(norm); return true; }
    }
    else if (id == "mixer_fader_d2")
    {
        if (message.isController()) { engine.getMixer().getChannel(1).volumeFader.store(norm); return true; }
    }
    else if (id == "mixer_crossfader")
    {
        if (message.isController()) { engine.setCrossfader((norm * 2.0f) - 1.0f); return true; }
    }
    else if (id == "mixer_master_vol")
    {
        if (message.isController()) { engine.getMixer().setMasterVolume(norm * 1.5f); return true; }
    }
    else if (id == "mixer_phones_vol")
    {
        if (message.isController()) { engine.getMixer().setPhonesVolume(norm * 1.5f); return true; }
    }
    else if (id == "mixer_cue_d1")
    {
        if (message.isNoteOn())
        {
            bool s = !engine.getMixer().getChannel(0).isCue();
            engine.getMixer().getChannel(0).setCue(s);
            return true; // P27: el LED lo envía el follower
        }
    }
    else if (id == "mixer_cue_d2")
    {
        if (message.isNoteOn())
        {
            bool s = !engine.getMixer().getChannel(1).isCue();
            engine.getMixer().getChannel(1).setCue(s);
            return true; // P27: el LED lo envía el follower
        }
    }

    // EQ y Filtros
    else if (id == "eq_gain_d1")
    {
        if (message.isController()) { engine.getMixer().getChannel(0).gain.store(norm * 2.0f); return true; }
    }
    else if (id == "eq_high_d1")
    {
        if (message.isController()) { engine.getMixer().getChannel(0).eqHigh.store(norm); return true; }
    }
    else if (id == "eq_mid_d1")
    {
        if (message.isController()) { engine.getMixer().getChannel(0).eqMid.store(norm); return true; }
    }
    else if (id == "eq_low_d1")
    {
        if (message.isController()) { engine.getMixer().getChannel(0).eqLow.store(norm); return true; }
    }
    else if (id == "eq_filter_d1")
    {
        if (message.isController()) { engine.getMixer().getChannel(0).filterKnob.store((norm * 2.0f) - 1.0f); return true; }
    }
    else if (id == "eq_gain_d2")
    {
        if (message.isController()) { engine.getMixer().getChannel(1).gain.store(norm * 2.0f); return true; }
    }
    else if (id == "eq_high_d2")
    {
        if (message.isController()) { engine.getMixer().getChannel(1).eqHigh.store(norm); return true; }
    }
    else if (id == "eq_mid_d2")
    {
        if (message.isController()) { engine.getMixer().getChannel(1).eqMid.store(norm); return true; }
    }
    else if (id == "eq_low_d2")
    {
        if (message.isController()) { engine.getMixer().getChannel(1).eqLow.store(norm); return true; }
    }
    else if (id == "eq_filter_d2")
    {
        if (message.isController()) { engine.getMixer().getChannel(1).filterKnob.store((norm * 2.0f) - 1.0f); return true; }
    }

    // Pads / Hot Cues
    else if (id.startsWith("pad_hotcue"))
    {
        if (message.isNoteOn())
        {
            int padIdx = 0;
            if (id.contains("hotcue1")) padIdx = 0;
            else if (id.contains("hotcue2")) padIdx = 1;
            else if (id.contains("hotcue3")) padIdx = 2;
            else if (id.contains("hotcue4")) padIdx = 3;

            int deck = id.endsWith("_d1") ? 0 : 1;
            auto& d = engine.getDeck(deck);
            if (d.hasHotCue(padIdx)) d.jumpToHotCue(padIdx);
            else d.setHotCue(padIdx, d.getPosition());
            return true;
        }
    }

    // P15: Performance (beat jump / loop roll / slip). Tocan componentes de UI
    // (size selector, SLIP), así que la acción se ejecuta en el hilo de mensajes.
    else if (id.startsWith("pad_jumpback") || id.startsWith("pad_jumpfwd")
          || id.startsWith("pad_roll") || id.startsWith("pad_slip"))
    {
        const int deckIdx = id.endsWith("_d1") ? 0 : 1;
        juce::String action;
        if (id.contains("jumpback"))      action = "jump_back";
        else if (id.contains("jumpfwd"))  action = "jump_fwd";
        else if (id.contains("roll"))     action = "roll";
        else                              action = "slip";

        // Release: note-off explícito o note-on con velocidad 0 (pads comunes).
        const bool pressed  = message.isNoteOn();
        const bool released = message.isNoteOff(true) && !pressed;

        if (action == "roll")
        {
            if (pressed && onPerformanceAction)
                juce::MessageManager::callAsync([this, deckIdx]() { if (onPerformanceAction) onPerformanceAction(deckIdx, "roll_start"); });
            else if (released && onPerformanceAction)
                juce::MessageManager::callAsync([this, deckIdx]() { if (onPerformanceAction) onPerformanceAction(deckIdx, "roll_release"); });
            return true;
        }

        if (pressed && onPerformanceAction)
            juce::MessageManager::callAsync([this, deckIdx, action]() { if (onPerformanceAction) onPerformanceAction(deckIdx, action); });
        return true;
    }

    // P22: Stems (toggle binario de ganancia por stem + master on/off)
    else if (id.startsWith("pad_stem"))
    {
        if (message.isNoteOn())
        {
            const int deckIdx = id.endsWith("_d1") ? 0 : 1;
            auto& d = engine.getDeck(deckIdx);
            if (id.contains("stems_active"))
            {
                d.toggleStemsMaster();
            }
            else
            {
                const int stem = id.contains("vocals") ? 0
                               : id.contains("drums") ? 1
                               : id.contains("bass") ? 2 : 3;
                // P25: en modo 2-stem (VOC/INST) drums mapea a INST y
                // bass/other no tienen target (el 4-stem no está cargado).
                const bool twoStem = d.isStems2Engaged() && !d.isStemsReady();
                if (twoStem && stem >= 2)
                    return true;
                const float g = d.getStemGain(stem);
                d.setStemGain(stem, g > 0.5f ? 0.0f : 1.0f);
            }
            return true;
        }
    }

    // Navegación
    else if (id == "browser_rotate")
    {
        if (message.isController())
        {
            int delta = (val < 64) ? val : (val - 128);
            if (onBrowseRotate) juce::MessageManager::callAsync([this, delta]() { if (onBrowseRotate) onBrowseRotate(delta); });
            return true;
        }
    }
    else if (id == "browser_push")
    {
        if (message.isNoteOn())
        {
            if (onBrowseClick) juce::MessageManager::callAsync([this]() { if (onBrowseClick) onBrowseClick(); });
            return true;
        }
    }
    else if (id == "browser_load_d1")
    {
        if (message.isNoteOn())
        {
            if (onLoadTrack) juce::MessageManager::callAsync([this]() { if (onLoadTrack) onLoadTrack(0); });
            return true;
        }
    }
    else if (id == "browser_load_d2")
    {
        if (message.isNoteOn())
        {
            if (onLoadTrack) juce::MessageManager::callAsync([this]() { if (onLoadTrack) onLoadTrack(1); });
            return true;
        }
    }

    return false;
}

void MidiManager::handleIncomingMidiMessage(juce::MidiInput* /*source*/, const juce::MidiMessage& message)
{
    // Format message description for live UI monitor
    if (onMidiActivity)
    {
        juce::String actText;
        if (message.isController())
        {
            actText = juce::String::formatted("Status: 0x%02X (CC) | Control: %d | Valor: %d",
                                              message.getRawData()[0],
                                              message.getControllerNumber(),
                                              message.getControllerValue());
        }
        else if (message.isNoteOn())
        {
            actText = juce::String::formatted("Status: 0x%02X (NoteOn) | Nota: %d | Velocidad: %d",
                                              message.getRawData()[0],
                                              message.getNoteNumber(),
                                              message.getVelocity());
        }
        else if (message.isNoteOff())
        {
            actText = juce::String::formatted("Status: 0x%02X (NoteOff) | Nota: %d | Velocidad: %d",
                                              message.getRawData()[0],
                                              message.getNoteNumber(),
                                              message.getVelocity());
        }
        else if (message.isPitchWheel())
        {
            actText = juce::String::formatted("Status: PitchBend | Valor: %d", message.getPitchWheelValue());
        }
        else
        {
            actText = message.getDescription();
        }

        juce::MessageManager::callAsync([this, actText]() {
            if (onMidiActivity)
                onMidiActivity(actText);
        });
    }

    // 1. Process MIDI Learn if active
    if (mappingManager.isLearning())
    {
        if (mappingManager.processLearningMessage(message))
            return;
    }

    // 2. Process custom MIDI Mapping dispatch
    if (auto* matched = mappingManager.matchMessage(message))
    {
        if (dispatchMappedAction(*matched, message))
            return;
    }

    // 1. Pioneer DDJ-SB2 Tempo Faders (14-bit Pitch Bend Messages: 0xE0 on Ch 1, 0xE1 on Ch 2)
    if (message.isPitchWheel())
    {
        int ch = message.getChannel();
        int pitchVal = message.getPitchWheelValue(); // 0 to 16383, center is 8192

        // Normalized delta: center is 8192 -> 0.0f
        // Standard Pioneer DDJ / Serato convention:
        // Moving fader DOWN (+) -> value > 8192 -> positive pitch delta (speed up)
        // Moving fader UP (-)   -> value < 8192 -> negative pitch delta (slow down)
        // Deadzone of +/- 32 ticks around center for firm 0.0% lock
        float norm = 0.0f;
        int delta = pitchVal - 8192;
        if (std::abs(delta) > 32)
        {
            norm = (float)delta / 8192.0f;
            norm = juce::jlimit(-1.0f, 1.0f, norm);
        }

        if (ch == 1) // Pioneer DDJ-SB2 Deck 1 (Channel 1)
        {
            auto& d1 = engine.getDeck(0);
            float targetPercent = norm * d1.getPitchRange();
            d1.setPitchPercent(targetPercent);
            return;
        }
        else if (ch == 2) // Pioneer DDJ-SB2 Deck 2 (Channel 2)
        {
            auto& d2 = engine.getDeck(1);
            float targetPercent = norm * d2.getPitchRange();
            d2.setPitchPercent(targetPercent);
            return;
        }
        else // Fallback for controllers using channel 0 / other
        {
            auto& d1 = engine.getDeck(0);
            float targetPercent = norm * d1.getPitchRange();
            d1.setPitchPercent(targetPercent);
            return;
        }
    }

    if (message.isController())
    {
        int ch = message.getChannel(); // 1 to 16
        int cc = message.getControllerNumber();
        int val = message.getControllerValue();
        float norm = static_cast<float>(val) / 127.0f;

        // 1. Crossfader: DDJ-SB2 sends CC 31 on Ch 7; generic controllers send CC 8 or CC 31
        if ((ch == 7 && cc == 31) || cc == 8 || cc == 31)
        {
            engine.setCrossfader((norm * 2.0f) - 1.0f);
            return;
        }

        // 2. Deck 1 Controls (Pioneer DDJ-SB2 sends on MIDI Channel 1 / 3)
        if (ch == 1 || ch == 3)
        {
            switch (cc)
            {
                case 33: // Pioneer DDJ-SB2 Jog Ring (0x21)
                case 34: // Pioneer DDJ-SB2 Jog Platter (0x22)
                case 38: // Shifted Jog Ring (0x26)
                case 31: // Shifted Jog Platter (0x1F)
                    handleJogWheel(0, cc, val);
                    return;

                case 0:  // Pioneer DDJ-SB2 Tempo Fader MSB (0x00)
                case 5:  // Shifted Tempo Fader MSB (0x05)
                    tempoMsb[0] = val;
                    apply14BitTempo(0);
                    return;

                case 32: // Pioneer DDJ-SB2 Tempo Fader LSB (0x20)
                case 37: // Shifted Tempo Fader LSB (0x25)
                    tempoLsb[0] = val;
                    apply14BitTempo(0);
                    return;

                case 19: // Channel 1 Volume Fader MSB (0x13)
                    engine.getMixer().getChannel(0).volumeFader.store(norm);
                    return;
                case 51: // Channel 1 Volume Fader LSB (0x33)
                    return;

                case 7:  // Channel 1 High EQ MSB (0x07)
                    engine.getMixer().getChannel(0).eqHigh.store(norm);
                    return;
                case 39: // Channel 1 High EQ LSB (0x27) - consumed to prevent jog wheel conflict
                    return;

                case 11: // Channel 1 Mid EQ MSB (0x0B)
                    engine.getMixer().getChannel(0).eqMid.store(norm);
                    return;
                case 43: // Channel 1 Mid EQ LSB (0x2B)
                    return;

                case 15: // Channel 1 Low EQ MSB (0x0F)
                    engine.getMixer().getChannel(0).eqLow.store(norm);
                    return;
                case 47: // Channel 1 Low EQ LSB (0x2F)
                    return;

                case 4:  // Channel 1 Trim / Gain MSB (0x04)
                    engine.getMixer().getChannel(0).gain.store(norm * 2.0f);
                    return;
                case 36: // Channel 1 Trim / Gain LSB (0x24)
                    return;

                default:
                    break;
            }
        }

        // 3. Deck 2 Controls (Pioneer DDJ-SB2 sends on MIDI Channel 2 / 4)
        if (ch == 2 || ch == 4)
        {
            switch (cc)
            {
                case 33: // Pioneer DDJ-SB2 Jog Ring (0x21)
                case 34: // Pioneer DDJ-SB2 Jog Platter (0x22)
                case 38: // Shifted Jog Ring (0x26)
                case 31: // Shifted Jog Platter (0x1F)
                    handleJogWheel(1, cc, val);
                    return;

                case 0:  // Pioneer DDJ-SB2 Tempo Fader MSB (0x00)
                case 5:  // Shifted Tempo Fader MSB (0x05)
                    tempoMsb[1] = val;
                    apply14BitTempo(1);
                    return;

                case 32: // Pioneer DDJ-SB2 Tempo Fader LSB (0x20)
                case 37: // Shifted Tempo Fader LSB (0x25)
                    tempoLsb[1] = val;
                    apply14BitTempo(1);
                    return;

                case 19: // Channel 2 Volume Fader MSB (0x13)
                    engine.getMixer().getChannel(1).volumeFader.store(norm);
                    return;
                case 51: // Channel 2 Volume Fader LSB (0x33)
                    return;

                case 7:  // Channel 2 High EQ MSB (0x07)
                    engine.getMixer().getChannel(1).eqHigh.store(norm);
                    return;
                case 39: // Channel 2 High EQ LSB (0x27) - consumed to prevent jog wheel conflict
                    return;

                case 11: // Channel 2 Mid EQ MSB (0x0B)
                    engine.getMixer().getChannel(1).eqMid.store(norm);
                    return;
                case 43: // Channel 2 Mid EQ LSB (0x2B)
                    return;

                case 15: // Channel 2 Low EQ MSB (0x0F)
                    engine.getMixer().getChannel(1).eqLow.store(norm);
                    return;
                case 47: // Channel 2 Low EQ LSB (0x2F)
                    return;

                case 4:  // Channel 2 Trim / Gain MSB (0x04)
                    engine.getMixer().getChannel(1).gain.store(norm * 2.0f);
                    return;
                case 36: // Channel 2 Trim / Gain LSB (0x24)
                    return;

                default:
                    break;
            }
        }

        // 4. Mixer / Master Channel (Pioneer DDJ-SB2 sends Filter and Headphones Mix on Channel 7)
        if (ch == 7)
        {
            switch (cc)
            {
                case 5:  // DDJ-SB2 Headphones Mix (0.0 to 1.5)
                    engine.getMixer().setPhonesVolume(norm * 1.5f);
                    return;
                case 23: // Deck 1 Filter Knob (DDJ-SB2 MSB, -1.0 to +1.0)
                    engine.getMixer().getChannel(0).filterKnob.store((norm * 2.0f) - 1.0f);
                    return;
                case 24: // Deck 2 Filter Knob (DDJ-SB2 MSB, -1.0 to +1.0)
                    engine.getMixer().getChannel(1).filterKnob.store((norm * 2.0f) - 1.0f);
                    return;
                case 64: // DDJ-SB2 BROWSE Rotary encoder (1 = CW/down, 127 = CCW/up)
                {
                    int delta = (val < 64) ? val : (val - 128);
                    if (onBrowseRotate)
                    {
                        juce::MessageManager::callAsync([this, delta]() {
                            if (onBrowseRotate) onBrowseRotate(delta);
                        });
                    }
                    return;
                }
                default:
                    break;
            }
        }

        // 5. Generic Controller Fallback (when controller transmits all controls on same channel or generic CCs)
        switch (cc)
        {
            case 64: // Generic BROWSE encoder
            {
                int delta = (val < 64) ? val : (val - 128);
                if (onBrowseRotate)
                {
                    juce::MessageManager::callAsync([this, delta]() {
                        if (onBrowseRotate) onBrowseRotate(delta);
                    });
                }
                break;
            }
            case 14: // Standard Master Volume on DJ controllers
                engine.getMixer().setMasterVolume(norm * 1.5f);
                break;
            case 5:  // Generic Headphone Mix / Volume
            case 12: // Generic Headphone Volume
                engine.getMixer().setPhonesVolume(norm * 1.5f);
                break;

            case 19: // Generic Deck 1 Volume Fader
                engine.getMixer().getChannel(0).volumeFader.store(norm);
                break;
            case 20: // Generic Deck 1 High EQ
                engine.getMixer().getChannel(0).eqHigh.store(norm);
                break;
            case 21: // Generic Deck 1 Mid EQ
                engine.getMixer().getChannel(0).eqMid.store(norm);
                break;
            case 22: // Generic Deck 1 Low EQ
                engine.getMixer().getChannel(0).eqLow.store(norm);
                break;
            case 23: // Generic Deck 1 Filter
                engine.getMixer().getChannel(0).filterKnob.store((norm * 2.0f) - 1.0f);
                break;

            case 24: // Generic Deck 2 Volume Fader (only if not Ch 7 Filter)
                engine.getMixer().getChannel(1).volumeFader.store(norm);
                break;
            case 25: // Generic Deck 2 High EQ
                engine.getMixer().getChannel(1).eqHigh.store(norm);
                break;
            case 26: // Generic Deck 2 Mid EQ
                engine.getMixer().getChannel(1).eqMid.store(norm);
                break;
            case 27: // Generic Deck 2 Low EQ
                engine.getMixer().getChannel(1).eqLow.store(norm);
                break;
            case 28: // Generic Deck 2 Filter
                engine.getMixer().getChannel(1).filterKnob.store((norm * 2.0f) - 1.0f);
                break;

            case 9:  // Generic Deck 1 Pitch Fader (CC)
                engine.getDeck(0).setPitchPercent((norm * 2.0f - 1.0f) * engine.getDeck(0).getPitchRange());
                break;

            case 29: // Generic Deck 2 Pitch Fader (CC)
                engine.getDeck(1).setPitchPercent((norm * 2.0f - 1.0f) * engine.getDeck(1).getPitchRange());
                break;

            case 33: // Generic Jog Wheel / Platter Deck 1
            case 34:
                handleJogWheel(0, cc, val);
                break;
            case 35: // Generic Jog Wheel / Platter Deck 2
                handleJogWheel(1, cc, val);
                break;

            default:
                break;
        }
    }
    else if (message.isNoteOn())
    {
        int ch = message.getChannel();
        int note = message.getNoteNumber();

        // 1. Pioneer DDJ-SB2 Deck 1 (Channel 1)
        if (ch == 1)
        {
            if (note == 11) // Play/Pause Deck 1
            {
                if (onPlayPause)
                {
                    juce::MessageManager::callAsync([this]() {
                        if (onPlayPause) onPlayPause(0);
                    });
                }
                else
                {
                    auto& d1 = engine.getDeck(0);
                    if (d1.isPlaying()) d1.pause(); else d1.play();
                }
                return;
            }
            if (note == 12) // Cue Deck 1
            {
                if (onCue)
                {
                    juce::MessageManager::callAsync([this]() {
                        if (onCue) onCue(0);
                    });
                }
                else
                {
                    engine.getDeck(0).triggerCue();
                }
                return;
            }
            if (note == 84) // Headphone CUE Deck 1 (PFL)
            {
                bool newState = !engine.getMixer().getChannel(0).isCue();
                engine.getMixer().getChannel(0).setCue(newState);
                return; // P27: el LED lo envía el follower
            }
            if (note == 88) // Sync Deck 1
            {
                auto& d1 = engine.getDeck(0);
                auto& d2 = engine.getDeck(1);
                if (d2.isLoaded() && d2.getBpm() > 0.0)
                {
                    double target = d2.getBpm();
                    double orig = d1.getOriginalBpm();
                    if (orig > 0.0)
                        d1.setPitchPercent((float)(((target / orig) - 1.0) * 100.0));
                }
                return;
            }
            if (note == 96) // Shift + KeyLock -> Cycle Tempo Range (8% -> 16% -> 50% -> 8%)
            {
                auto& d1 = engine.getDeck(0);
                float cur = d1.getPitchRange();
                float next = (cur < 12.0f) ? 16.0f : ((cur < 30.0f) ? 50.0f : 8.0f);
                d1.setPitchRange(next);
                return;
            }
        }

        // 2. Pioneer DDJ-SB2 Deck 2 (Channel 2)
        if (ch == 2)
        {
            if (note == 11) // Play/Pause Deck 2
            {
                if (onPlayPause)
                {
                    juce::MessageManager::callAsync([this]() {
                        if (onPlayPause) onPlayPause(1);
                    });
                }
                else
                {
                    auto& d2 = engine.getDeck(1);
                    if (d2.isPlaying()) d2.pause(); else d2.play();
                }
                return;
            }
            if (note == 12) // Cue Deck 2
            {
                if (onCue)
                {
                    juce::MessageManager::callAsync([this]() {
                        if (onCue) onCue(1);
                    });
                }
                else
                {
                    engine.getDeck(1).triggerCue();
                }
                return;
            }
            if (note == 84) // Headphone CUE Deck 2 (PFL)
            {
                bool newState = !engine.getMixer().getChannel(1).isCue();
                engine.getMixer().getChannel(1).setCue(newState);
                return; // P27: el LED lo envía el follower
            }
            if (note == 88) // Sync Deck 2
            {
                auto& d1 = engine.getDeck(0);
                auto& d2 = engine.getDeck(1);
                if (d1.isLoaded() && d1.getBpm() > 0.0)
                {
                    double target = d1.getBpm();
                    double orig = d2.getOriginalBpm();
                    if (orig > 0.0)
                        d2.setPitchPercent((float)(((target / orig) - 1.0) * 100.0));
                }
                return;
            }
            if (note == 96) // Shift + KeyLock -> Cycle Tempo Range (8% -> 16% -> 50% -> 8%)
            {
                auto& d2 = engine.getDeck(1);
                float cur = d2.getPitchRange();
                float next = (cur < 12.0f) ? 16.0f : ((cur < 30.0f) ? 50.0f : 8.0f);
                d2.setPitchRange(next);
                return;
            }
        }

        // 3. Pioneer DDJ-SB2 Channel 7: Browse Click & Load Buttons
        if (ch == 7)
        {
            if (note == 65) // BROWSE push (click)
            {
                if (onBrowseClick)
                {
                    juce::MessageManager::callAsync([this]() {
                        if (onBrowseClick) onBrowseClick();
                    });
                }
                return;
            }
            if (note == 70) // LOAD Deck 1
            {
                if (onLoadTrack)
                {
                    juce::MessageManager::callAsync([this]() {
                        if (onLoadTrack) onLoadTrack(0);
                    });
                }
                return;
            }
            if (note == 71) // LOAD Deck 2
            {
                if (onLoadTrack)
                {
                    juce::MessageManager::callAsync([this]() {
                        if (onLoadTrack) onLoadTrack(1);
                    });
                }
                return;
            }
        }

        // 4. Generic Note fallbacks (Deck 1 / Deck 2 / Browse / Load)
        if (note == 65) // Generic Browse Click
        {
            if (onBrowseClick)
            {
                juce::MessageManager::callAsync([this]() {
                    if (onBrowseClick) onBrowseClick();
                });
            }
        }
        else if (note == 70) // Generic Load Deck 1
        {
            if (onLoadTrack)
            {
                juce::MessageManager::callAsync([this]() {
                    if (onLoadTrack) onLoadTrack(0);
                });
            }
        }
        else if (note == 71) // Generic Load Deck 2
        {
            if (onLoadTrack)
            {
                juce::MessageManager::callAsync([this]() {
                    if (onLoadTrack) onLoadTrack(1);
                });
            }
        }
        else if (note == 11) // Play/Pause Deck 1
        {
            if (onPlayPause)
            {
                juce::MessageManager::callAsync([this]() {
                    if (onPlayPause) onPlayPause(0);
                });
            }
            else
            {
                auto& d1 = engine.getDeck(0);
                if (d1.isPlaying()) d1.pause(); else d1.play();
            }
        }
        else if (note == 12) // Cue Deck 1
        {
            if (onCue)
            {
                juce::MessageManager::callAsync([this]() {
                    if (onCue) onCue(0);
                });
            }
            else
            {
                engine.getDeck(0).triggerCue();
            }
        }
        else if (note == 84) // Generic Headphone Cue Deck 1
        {
            bool newState = !engine.getMixer().getChannel(0).isCue();
            engine.getMixer().getChannel(0).setCue(newState);
        }
        else if (note == 15) // Play/Pause Deck 2
        {
            if (onPlayPause)
            {
                juce::MessageManager::callAsync([this]() {
                    if (onPlayPause) onPlayPause(1);
                });
            }
            else
            {
                auto& d2 = engine.getDeck(1);
                if (d2.isPlaying()) d2.pause(); else d2.play();
            }
        }
        else if (note == 16) // Cue Deck 2
        {
            if (onCue)
            {
                juce::MessageManager::callAsync([this]() {
                    if (onCue) onCue(1);
                });
            }
            else
            {
                engine.getDeck(1).triggerCue();
            }
        }
        // Hot Cues Deck 1: Notes 0..3 on Ch 8 (DDJ-SB2) or Notes 1..4 (Generic)
        else if ((ch == 8 && note >= 0 && note <= 3) || (note >= 1 && note <= 4))
        {
            int idx = (ch == 8) ? note : (note - 1);
            auto& d1 = engine.getDeck(0);
            if (d1.hasHotCue(idx)) d1.jumpToHotCue(idx);
            else d1.setHotCue(idx, d1.getPosition());
        }
        // Hot Cues Deck 2: Notes 0..3 on Ch 9 (DDJ-SB2) or Notes 5..8 (Generic)
        else if ((ch == 9 && note >= 0 && note <= 3) || (note >= 5 && note <= 8))
        {
            int idx = (ch == 9) ? note : (note - 5);
            auto& d2 = engine.getDeck(1);
            if (d2.hasHotCue(idx)) d2.jumpToHotCue(idx);
            else d2.setHotCue(idx, d2.getPosition());
        }
    }
}

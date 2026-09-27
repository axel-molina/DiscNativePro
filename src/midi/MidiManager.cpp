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
}

void MidiManager::shutdown()
{
    auto devices = juce::MidiInput::getAvailableDevices();
    for (const auto& dev : devices)
    {
        engine.getDeviceManager().removeMidiInputDeviceCallback(dev.identifier, this);
    }
    midiOutput.reset();
}

void MidiManager::rescanDevices()
{
    shutdown();

    auto devices = juce::MidiInput::getAvailableDevices();
    connectedDevices.clear();

    for (const auto& dev : devices)
    {
        engine.getDeviceManager().setMidiInputDeviceEnabled(dev.identifier, true);
        engine.getDeviceManager().addMidiInputDeviceCallback(dev.identifier, this);
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

    deviceConnected = !connectedDevices.isEmpty();

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

        // 2. Deck 1 Controls (Pioneer DDJ-SB2 sends on MIDI Channel 1)
        if (ch == 1)
        {
            switch (cc)
            {
                case 19: // Channel 1 Volume Fader (DDJ-SB2 MSB)
                    engine.getMixer().getChannel(0).volumeFader.store(norm);
                    return;
                case 7:  // Channel 1 High EQ (DDJ-SB2 MSB)
                    engine.getMixer().getChannel(0).eqHigh.store(norm);
                    return;
                case 11: // Channel 1 Mid EQ (DDJ-SB2 MSB)
                    engine.getMixer().getChannel(0).eqMid.store(norm);
                    return;
                case 15: // Channel 1 Low EQ (DDJ-SB2 MSB)
                    engine.getMixer().getChannel(0).eqLow.store(norm);
                    return;
                case 4:  // Channel 1 Trim / Gain (DDJ-SB2 MSB)
                    engine.getMixer().getChannel(0).gain.store(norm * 2.0f);
                    return;
                default:
                    break;
            }
        }

        // 3. Deck 2 Controls (Pioneer DDJ-SB2 sends on MIDI Channel 2)
        if (ch == 2)
        {
            switch (cc)
            {
                case 19: // Channel 2 Volume Fader (DDJ-SB2 MSB)
                    engine.getMixer().getChannel(1).volumeFader.store(norm);
                    return;
                case 7:  // Channel 2 High EQ (DDJ-SB2 MSB)
                    engine.getMixer().getChannel(1).eqHigh.store(norm);
                    return;
                case 11: // Channel 2 Mid EQ (DDJ-SB2 MSB)
                    engine.getMixer().getChannel(1).eqMid.store(norm);
                    return;
                case 15: // Channel 2 Low EQ (DDJ-SB2 MSB)
                    engine.getMixer().getChannel(1).eqLow.store(norm);
                    return;
                case 4:  // Channel 2 Trim / Gain (DDJ-SB2 MSB)
                    engine.getMixer().getChannel(1).gain.store(norm * 2.0f);
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
                default:
                    break;
            }
        }

        // 5. Generic Controller Fallback (when controller transmits all controls on same channel or generic CCs)
        switch (cc)
        {
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
                auto& d1 = engine.getDeck(0);
                if (d1.isPlaying()) d1.pause(); else d1.play();
                return;
            }
            if (note == 12) // Cue Deck 1
            {
                engine.getDeck(0).triggerCue();
                return;
            }
            if (note == 84) // Headphone CUE Deck 1 (PFL)
            {
                bool newState = !engine.getMixer().getChannel(0).isCue();
                engine.getMixer().getChannel(0).setCue(newState);
                sendMidiMessage(juce::MidiMessage::noteOn(1, 84, (juce::uint8)(newState ? 127 : 0)));
                return;
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
        }

        // 2. Pioneer DDJ-SB2 Deck 2 (Channel 2)
        if (ch == 2)
        {
            if (note == 11) // Play/Pause Deck 2
            {
                auto& d2 = engine.getDeck(1);
                if (d2.isPlaying()) d2.pause(); else d2.play();
                return;
            }
            if (note == 12) // Cue Deck 2
            {
                engine.getDeck(1).triggerCue();
                return;
            }
            if (note == 84) // Headphone CUE Deck 2 (PFL)
            {
                bool newState = !engine.getMixer().getChannel(1).isCue();
                engine.getMixer().getChannel(1).setCue(newState);
                sendMidiMessage(juce::MidiMessage::noteOn(2, 84, (juce::uint8)(newState ? 127 : 0)));
                return;
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
        }

        // 3. Generic Note fallbacks (Deck 1 / Deck 2)
        if (note == 11) // Play/Pause Deck 1
        {
            auto& d1 = engine.getDeck(0);
            if (d1.isPlaying()) d1.pause(); else d1.play();
        }
        else if (note == 12) // Cue Deck 1
        {
            engine.getDeck(0).triggerCue();
        }
        else if (note == 84) // Generic Headphone Cue Deck 1
        {
            bool newState = !engine.getMixer().getChannel(0).isCue();
            engine.getMixer().getChannel(0).setCue(newState);
        }
        else if (note == 15) // Play/Pause Deck 2
        {
            auto& d2 = engine.getDeck(1);
            if (d2.isPlaying()) d2.pause(); else d2.play();
        }
        else if (note == 16) // Cue Deck 2
        {
            engine.getDeck(1).triggerCue();
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

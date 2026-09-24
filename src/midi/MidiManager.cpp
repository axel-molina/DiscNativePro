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

    deviceConnected = !connectedDevices.isEmpty();

    if (onDevicesChanged)
        onDevicesChanged();
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
        int cc = message.getControllerNumber();
        int val = message.getControllerValue();
        float norm = static_cast<float>(val) / 127.0f;

        switch (cc)
        {
            // Crossfader (CC 8 or CC 31)
            case 8:
            case 31:
                engine.getMixer().setCrossfader((norm * 2.0f) - 1.0f);
                break;

            // Channel 1 Volume & EQs
            case 19:
                engine.getMixer().getChannel(0).volumeFader.store(norm);
                break;
            case 20:
                engine.getMixer().getChannel(0).eqHigh.store(norm);
                break;
            case 21:
                engine.getMixer().getChannel(0).eqMid.store(norm);
                break;
            case 22:
                engine.getMixer().getChannel(0).eqLow.store(norm);
                break;
            case 23:
                engine.getMixer().getChannel(0).filterKnob.store((norm * 2.0f) - 1.0f);
                break;

            // Channel 2 Volume & EQs
            case 24:
                engine.getMixer().getChannel(1).volumeFader.store(norm);
                break;
            case 25:
                engine.getMixer().getChannel(1).eqHigh.store(norm);
                break;
            case 26:
                engine.getMixer().getChannel(1).eqMid.store(norm);
                break;
            case 27:
                engine.getMixer().getChannel(1).eqLow.store(norm);
                break;
            case 28:
                engine.getMixer().getChannel(1).filterKnob.store((norm * 2.0f) - 1.0f);
                break;

            default:
                break;
        }
    }
    else if (message.isNoteOn())
    {
        int note = message.getNoteNumber();

        // Deck 1 standard DJ mapping
        if (note == 11) // Play/Pause Deck 1
        {
            auto& d1 = engine.getDeck(0);
            if (d1.isPlaying()) d1.pause(); else d1.play();
        }
        else if (note == 12) // Cue Deck 1
        {
            engine.getDeck(0).triggerCue();
        }
        else if (note >= 1 && note <= 4) // Hot cues 1-4 Deck 1
        {
            int idx = note - 1;
            auto& d1 = engine.getDeck(0);
            if (d1.hasHotCue(idx)) d1.jumpToHotCue(idx);
            else d1.setHotCue(idx, d1.getPosition());
        }

        // Deck 2 standard DJ mapping
        else if (note == 15) // Play/Pause Deck 2
        {
            auto& d2 = engine.getDeck(1);
            if (d2.isPlaying()) d2.pause(); else d2.play();
        }
        else if (note == 16) // Cue Deck 2
        {
            engine.getDeck(1).triggerCue();
        }
        else if (note >= 5 && note <= 8) // Hot cues 1-4 Deck 2
        {
            int idx = note - 5;
            auto& d2 = engine.getDeck(1);
            if (d2.hasHotCue(idx)) d2.jumpToHotCue(idx);
            else d2.setHotCue(idx, d2.getPosition());
        }
    }
}

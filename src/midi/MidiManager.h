#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include "../audio/AudioEngine.h"
#include <functional>

class MidiManager : public juce::MidiInputCallback
{
public:
    MidiManager(AudioEngine& audioEngine);
    ~MidiManager() override;

    void initialise();
    void shutdown();
    void rescanDevices();

    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override;

    bool isAnyDeviceConnected() const { return deviceConnected; }
    juce::StringArray getConnectedDeviceNames() const;
    void sendMidiMessage(const juce::MidiMessage& message);

    std::function<void(const juce::String& description)> onMidiActivity;
    std::function<void()> onDevicesChanged;

private:
    AudioEngine& engine;
    bool deviceConnected { false };
    juce::StringArray connectedDevices;
    std::unique_ptr<juce::MidiOutput> midiOutput;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiManager)
};

#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_events/juce_events.h>
#include "../audio/AudioEngine.h"
#include <functional>

class MidiManager : public juce::MidiInputCallback,
                    private juce::Timer
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
    std::function<void(int delta)> onBrowseRotate;
    std::function<void()> onBrowseClick;
    std::function<void(int deckIndex)> onLoadTrack;
    std::function<void(int deckIndex)> onPlayPause;
    std::function<void(int deckIndex)> onCue;

private:
    AudioEngine& engine;
    bool deviceConnected { false };
    juce::StringArray connectedDevices;
    std::unique_ptr<juce::MidiOutput> midiOutput;

    // 14-bit CC Tempo Fader tracking (MSB defaults to 64, LSB defaults to 0 -> center: 8192)
    int tempoMsb[4] { 64, 64, 64, 64 };
    int tempoLsb[4] { 0, 0, 0, 0 };
    void apply14BitTempo(int deckIndex);

    // Jog Wheel handling & pitch bend decay
    void handleJogWheel(int deckIndex, int cc, int val);
    void timerCallback() override;
    bool jogBendActive[2] { false, false };
    int jogBendTicks[2] { 0, 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiManager)
};

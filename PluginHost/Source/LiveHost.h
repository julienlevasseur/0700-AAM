// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "HostConfig.h"
#include "HostProcessor.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <atomic>

// The running host: the plugin on the audio device, fed by the MIDI ports that
// match the configuration. The audio device and the ports are retried every
// two seconds, so the host can start before the KICK application and survive
// its restarts.
class LiveHost : private juce::Timer
{
public:
    explicit LiveHost( const HostConfig& config );
    ~LiveHost() override;

    // Empty when the plugin loaded.
    juce::String getError() const { return error; }

    // Set from a signal handler; the host then saves its state and stops the
    // message loop.
    static std::atomic<bool> quitRequested;

private:
    void timerCallback() override;
    void openAudio();
    void updateMidiInputs();
    void report();
    void saveIfChanged();

    HostConfig                                 config;
    juce::String                               error;
    juce::AudioPluginFormatManager             formats;
    std::unique_ptr<juce::AudioPluginInstance> plugin;
    std::unique_ptr<HostProcessor>             host;
    juce::AudioDeviceManager                   devices;
    juce::AudioProcessorPlayer                 player;
    juce::StringArray                          openInputs; // identifiers
    juce::MemoryBlock                          savedState;
    int                                        ticks = 0;
};

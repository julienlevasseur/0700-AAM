// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <juce_core/juce_core.h>

// Settings of the plugin host, from /etc/kick/plugin-host.conf ("key = value"
// lines, # comments) and from --key=value options, which win.
struct HostConfig
{
    juce::String plugin;                     // .vst3 or .lv2 bundle, or an LV2 URI
    juce::StringArray midiInputs{ "Kick:Out:1" }; // port name fragments, "*" for all
    int midiChannel = 0;                     // 1..16, 0 for all channels
    juce::String audioType   = "ALSA";
    juce::String audioDevice = "default";    // device name, or a fragment of it
    double sampleRate = 48000.0;
    int blockSize     = 256;
    float gainDb      = 0.0f;
    juce::File stateFile{ "/media/kickstorage/plugin-host/state.xml" };
    int stateSaveSeconds = 30;               // 0 saves only on exit

    // Loads a file, then applies options. Returns an error message, empty when fine.
    juce::String load( const juce::File& file );
    juce::String set( const juce::String& key, const juce::String& value );

    juce::String describe() const;
};

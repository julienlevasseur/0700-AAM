// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>

// Finds the plugin in a .vst3/.lv2 bundle path (or an LV2 URI) and creates it,
// with no audio input and a stereo output when the plugin allows it.
std::unique_ptr<juce::AudioPluginInstance> loadPlugin( juce::AudioPluginFormatManager& formats,
                                                       const juce::String& fileOrIdentifier,
                                                       double sampleRate, int blockSize,
                                                       juce::String& error );

// Plugin state, kept with the plugin's identity so a state is never given to
// another plugin. Writes go through a temporary file.
bool saveState( juce::AudioPluginInstance& plugin, const juce::File& file );
bool restoreState( juce::AudioPluginInstance& plugin, const juce::File& file, juce::String& message );

// Runs the plugin for the device: drops MIDI on other channels, maps the
// plugin's outputs to stereo, applies the output gain, and measures the output
// peak and the time each block takes.
class HostProcessor : public juce::AudioProcessor
{
public:
    HostProcessor( juce::AudioPluginInstance& plugin, int midiChannel, float gainDb );

    void prepareToPlay( double sampleRate, int blockSize ) override;
    void releaseResources() override;
    void processBlock( juce::AudioBuffer<float>&, juce::MidiBuffer& ) override;

    // Since the last call: output peak (linear), and the average and worst block
    // time as a share of the block's duration.
    struct Meter
    {
        float peak = 0, averageLoad = 0, worstLoad = 0;
        int   blocks = 0, notes = 0;
    };
    Meter takeMeter();

    const juce::String getName() const override { return "kick-plugin-host"; }
    double getTailLengthSeconds() const override { return 0; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram( int ) override {}
    const juce::String getProgramName( int ) override { return {}; }
    void changeProgramName( int, const juce::String& ) override {}
    void getStateInformation( juce::MemoryBlock& ) override {}
    void setStateInformation( const void*, int ) override {}

private:
    juce::AudioPluginInstance& plugin;
    const int                  midiChannel;
    const float                gain;

    juce::AudioBuffer<float> scratch;
    juce::MidiBuffer         filtered;
    double                   ticksPerSample = 0;

    std::atomic<float> peak{ 0 }, worst{ 0 };
    std::atomic<double> loadSum{ 0 };
    std::atomic<int>    blocks{ 0 }, notes{ 0 };
};

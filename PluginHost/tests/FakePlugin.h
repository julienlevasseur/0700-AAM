// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Stands in for a VST3/LV2 instrument in the unit tests: writes a constant
// level on each output channel (0.1 on the first, 0.2 on the second, ...),
// keeps the MIDI it was given, and has a state that is just a string.
class FakePlugin : public juce::AudioPluginInstance
{
public:
    explicit FakePlugin( int outputs = 2, juce::String pluginName = "Fake Instrument" )
        : AudioPluginInstance( BusesProperties().withOutput( "Out", juce::AudioChannelSet::canonicalChannelSet( outputs ), true ) )
        , name( std::move( pluginName ) )
    {
    }

    juce::Array<juce::MidiMessage> received;
    juce::String                   state = "initial";

    void fillInPluginDescription( juce::PluginDescription& d ) const override
    {
        d.name             = name;
        d.pluginFormatName = "Fake";
        d.fileOrIdentifier = "fake://" + name;
        d.uniqueId         = 1234;
        d.isInstrument     = true;
    }

    using AudioPluginInstance::processBlock;
    void processBlock( juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi ) override
    {
        for( const auto m : midi )
            received.add( m.getMessage() );
        for( int ch = 0; ch < getTotalNumOutputChannels(); ++ch )
            for( int i = 0; i < buffer.getNumSamples(); ++i )
                buffer.setSample( ch, i, 0.1f * (float)( ch + 1 ) );
    }

    void getStateInformation( juce::MemoryBlock& blob ) override
    {
        blob.reset();
        blob.append( state.toRawUTF8(), state.getNumBytesAsUTF8() );
    }
    void setStateInformation( const void* data, int size ) override
    {
        state = juce::String::fromUTF8( static_cast<const char*>( data ), size );
    }

    const juce::String getName() const override { return name; }
    void prepareToPlay( double, int ) override {}
    void releaseResources() override {}
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

private:
    const juce::String name;
};

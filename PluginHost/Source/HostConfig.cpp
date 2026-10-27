// SPDX-License-Identifier: AGPL-3.0-or-later
#include "HostConfig.h"

juce::String HostConfig::load( const juce::File& file )
{
    if( !file.existsAsFile() )
        return "config file not found: " + file.getFullPathName();

    juce::StringArray lines;
    file.readLines( lines );
    for( int i = 0; i < lines.size(); ++i )
    {
        const auto line = lines[i].upToFirstOccurrenceOf( "#", false, false ).trim();
        if( line.isEmpty() )
            continue;
        if( !line.contains( "=" ) )
            return file.getFileName() + ":" + juce::String( i + 1 ) + ": expected key = value";
        const auto error = set( line.upToFirstOccurrenceOf( "=", false, false ).trim(),
                                line.fromFirstOccurrenceOf( "=", false, false ).trim() );
        if( error.isNotEmpty() )
            return file.getFileName() + ":" + juce::String( i + 1 ) + ": " + error;
    }
    return {};
}

juce::String HostConfig::set( const juce::String& key, const juce::String& value )
{
    auto number = [&]( double lo, double hi, double& out ) -> juce::String {
        if( !value.containsOnly( "-+.0123456789" ) || value.isEmpty() )
            return key + ": not a number: " + value;
        out = value.getDoubleValue();
        if( out < lo || out > hi )
            return key + ": out of range " + juce::String( lo ) + ".." + juce::String( hi );
        return {};
    };
    double v = 0;
    juce::String error;

    if( key == "plugin" )
        plugin = value;
    else if( key == "midi_inputs" )
    {
        midiInputs.clear();
        midiInputs.addTokens( value, ",", "\"" );
        midiInputs.trim();
        midiInputs.removeEmptyStrings();
    }
    else if( key == "midi_channel" )
    {
        if( ( error = number( 0, 16, v ) ).isEmpty() ) midiChannel = (int)v;
    }
    else if( key == "audio_type" )
        audioType = value;
    else if( key == "audio_device" )
        audioDevice = value;
    else if( key == "sample_rate" )
    {
        if( ( error = number( 8000, 192000, v ) ).isEmpty() ) sampleRate = v;
    }
    else if( key == "block_size" )
    {
        if( ( error = number( 16, 4096, v ) ).isEmpty() ) blockSize = (int)v;
    }
    else if( key == "gain_db" )
    {
        if( ( error = number( -60, 24, v ) ).isEmpty() ) gainDb = (float)v;
    }
    else if( key == "state_file" )
        stateFile = juce::File( value );
    else if( key == "state_save_seconds" )
    {
        if( ( error = number( 0, 86400, v ) ).isEmpty() ) stateSaveSeconds = (int)v;
    }
    else
        error = "unknown setting: " + key;
    return error;
}

juce::String HostConfig::describe() const
{
    return "plugin=" + plugin + "\nmidi_inputs=" + midiInputs.joinIntoString( "," )
         + "\nmidi_channel=" + juce::String( midiChannel ) + "\naudio=" + audioType + ":" + audioDevice
         + " " + juce::String( sampleRate ) + " Hz, " + juce::String( blockSize ) + " frames"
         + "\ngain_db=" + juce::String( gainDb ) + "\nstate_file=" + stateFile.getFullPathName();
}

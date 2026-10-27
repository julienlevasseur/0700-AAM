// SPDX-License-Identifier: AGPL-3.0-or-later
//
// kick-plugin-host: runs one VST3 or LV2 instrument on the KICK, next to the
// KICK application, which plays it through its MIDI output ports.

#include "HostConfig.h"
#include "LiveHost.h"
#include "OfflineRun.h"

#include <csignal>
#include <iostream>

namespace
{
    const char* usage =
        "usage: kick-plugin-host [--config=FILE] [--key=value ...] [--render=OUT.wav] [--bench]\n"
        "                        [--seconds=N] [--notes=36,38,...] [--save-state] [--list-audio]\n"
        "\n"
        "Runs the plugin on the audio device, played from the MIDI ports named in\n"
        "midi_inputs. --render and --bench play a pattern without a device instead.\n"
        "Settings (in the config file or as --key=value): plugin, midi_inputs,\n"
        "midi_channel, audio_type, audio_device, sample_rate, block_size, gain_db,\n"
        "state_file, state_save_seconds.\n";

    void onSignal( int ) { LiveHost::quitRequested.store( true ); }

    void listAudio()
    {
        juce::AudioDeviceManager dm;
        for( auto* type : dm.getAvailableDeviceTypes() )
        {
            type->scanForDevices();
            std::cout << type->getTypeName() << ":\n";
            for( const auto& n : type->getDeviceNames( false ) )
                std::cout << "  " << n << "\n";
        }
        std::cout << "MIDI inputs:\n";
        for( const auto& d : juce::MidiInput::getAvailableDevices() )
            std::cout << "  " << d.name << " (" << d.identifier << ")\n";
    }
} // namespace

int main( int argc, char* argv[] )
{
    juce::ScopedJuceInitialiser_GUI juce;

    HostConfig     config;
    OfflineOptions offline;
    bool           runOfflineMode = false, list = false;
    juce::File     configFile( "/etc/kick/plugin-host.conf" );
    juce::StringArray overrides;

    for( int i = 1; i < argc; ++i )
    {
        const juce::String arg( argv[i] );
        const auto key   = arg.fromFirstOccurrenceOf( "--", false, false ).upToFirstOccurrenceOf( "=", false, false );
        const auto value = arg.fromFirstOccurrenceOf( "=", false, false );
        if( !arg.startsWith( "--" ) || arg == "--help" )
        {
            std::cout << usage;
            return arg == "--help" ? 0 : 2;
        }
        if( key == "config" )
            configFile = juce::File::getCurrentWorkingDirectory().getChildFile( value );
        else if( key == "render" )
        {
            offline.wavFile = juce::File::getCurrentWorkingDirectory().getChildFile( value );
            runOfflineMode  = true;
        }
        else if( key == "bench" )
            runOfflineMode = true;
        else if( key == "seconds" )
            offline.seconds = juce::jlimit( 0.1, 3600.0, value.getDoubleValue() );
        else if( key == "notes" )
        {
            offline.notes.clear();
            for( const auto& n : juce::StringArray::fromTokens( value, ",", "" ) )
                offline.notes.add( juce::jlimit( 0, 127, n.getIntValue() ) );
            if( offline.notes.isEmpty() )
                offline.notes.add( 36 );
        }
        else if( key == "save-state" )
            offline.saveStateAtEnd = true;
        else if( key == "list-audio" )
            list = true;
        else
            overrides.add( arg.substring( 2 ) );
    }

    if( list )
    {
        listAudio();
        return 0;
    }

    // The config file is optional when the plugin is given on the command line.
    if( configFile.existsAsFile() || !overrides.joinIntoString( " " ).contains( "plugin=" ) )
    {
        const auto error = config.load( configFile );
        if( error.isNotEmpty() )
        {
            std::cerr << "error: " << error << "\n";
            return 2;
        }
    }
    for( const auto& o : overrides )
    {
        const auto error = config.set( o.upToFirstOccurrenceOf( "=", false, false ), o.fromFirstOccurrenceOf( "=", false, false ) );
        if( error.isNotEmpty() )
        {
            std::cerr << "error: " << error << "\n" << usage;
            return 2;
        }
    }
    if( config.plugin.isEmpty() )
    {
        std::cerr << "error: no plugin set\n";
        return 2;
    }

    if( runOfflineMode )
        return runOffline( config, offline );

    std::cout << config.describe() << std::endl;
    std::signal( SIGTERM, onSignal );
    std::signal( SIGINT, onSignal );
    {
        LiveHost host( config );
        if( host.getError().isNotEmpty() )
        {
            std::cerr << "error: " << host.getError() << "\n";
            return 1;
        }
        juce::MessageManager::getInstance()->runDispatchLoop();
    }
    std::cout << "stopped" << std::endl;
    return 0;
}

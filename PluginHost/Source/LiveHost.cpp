// SPDX-License-Identifier: AGPL-3.0-or-later
#include "LiveHost.h"

#include <iostream>

std::atomic<bool> LiveHost::quitRequested{ false };

namespace
{
    void log( const juce::String& text ) { std::cout << text << std::endl; }

    bool matches( const juce::StringArray& fragments, const juce::String& name )
    {
        for( const auto& f : fragments )
            if( f == "*" || name.containsIgnoreCase( f ) )
                return true;
        return false;
    }
} // namespace

LiveHost::LiveHost( const HostConfig& c )
    : config( c )
{
    formats.addDefaultFormats();
    plugin = loadPlugin( formats, config.plugin, config.sampleRate, config.blockSize, error );
    if( !plugin )
        return;
    log( "plugin: " + plugin->getName() + " (" + plugin->getPluginDescription().pluginFormatName + ")" );

    juce::String message;
    restoreState( *plugin, config.stateFile, message );
    log( message );
    plugin->getStateInformation( savedState );

    host = std::make_unique<HostProcessor>( *plugin, config.midiChannel, config.gainDb );
    player.setProcessor( host.get() );

    openAudio();
    updateMidiInputs();
    startTimer( 100 );
}

LiveHost::~LiveHost()
{
    stopTimer();
    if( plugin )
    {
        saveIfChanged();
        for( const auto& id : openInputs )
        {
            devices.removeMidiInputDeviceCallback( id, &player );
            devices.setMidiInputDeviceEnabled( id, false );
        }
        devices.removeAudioCallback( &player );
        devices.closeAudioDevice();
        player.setProcessor( nullptr );
    }
}

void LiveHost::timerCallback()
{
    if( quitRequested.load() )
    {
        stopTimer();
        juce::MessageManager::getInstance()->stopDispatchLoop();
        return;
    }
    ++ticks;
    if( ticks % 20 == 0 ) // every 2 s
    {
        if( devices.getCurrentAudioDevice() == nullptr )
            openAudio();
        updateMidiInputs();
    }
    if( ticks % 100 == 0 )
        report();
    if( config.stateSaveSeconds > 0 && ticks % ( config.stateSaveSeconds * 10 ) == 0 )
        saveIfChanged();
}

void LiveHost::openAudio()
{
    static juce::String lastError;

    devices.getAvailableDeviceTypes(); // creates them, so the type can be chosen
    devices.setCurrentAudioDeviceType( config.audioType, true );
    auto* type = devices.getCurrentDeviceTypeObject();
    if( type == nullptr || type->getTypeName() != config.audioType )
    {
        juce::StringArray types;
        for( auto* t : devices.getAvailableDeviceTypes() )
            types.add( t->getTypeName() );
        if( lastError != "type" )
            log( "audio: no " + config.audioType + " support, available: " + types.joinIntoString( ", " ) );
        lastError = "type";
        return;
    }

    type->scanForDevices();
    const auto names = type->getDeviceNames( false );
    juce::String name;
    for( const auto& n : names )
        if( n == config.audioDevice )
            name = n;
    for( const auto& n : names )
        if( name.isEmpty() && n.containsIgnoreCase( config.audioDevice ) )
            name = n;
    if( name.isEmpty() )
    {
        if( lastError != "name" )
            log( "audio: no output device matches \"" + config.audioDevice + "\", available: " + names.joinIntoString( " | " ) );
        lastError = "name";
        return;
    }

    juce::AudioDeviceManager::AudioDeviceSetup setup;
    setup.outputDeviceName         = name;
    setup.inputDeviceName          = {};
    setup.sampleRate               = config.sampleRate;
    setup.bufferSize               = config.blockSize;
    setup.useDefaultInputChannels  = false;
    setup.useDefaultOutputChannels = true;

    const auto err = devices.initialise( 0, 2, nullptr, false, {}, &setup );
    if( err.isNotEmpty() || devices.getCurrentAudioDevice() == nullptr )
    {
        if( lastError != err )
            log( "audio: cannot open " + name + ": " + err + " (retrying)" );
        lastError = err;
        devices.closeAudioDevice();
        return;
    }
    lastError = {};
    devices.addAudioCallback( &player );

    auto* dev = devices.getCurrentAudioDevice();
    log( "audio: " + type->getTypeName() + " " + dev->getName() + ", " + juce::String( dev->getCurrentSampleRate() ) + " Hz, "
         + juce::String( dev->getCurrentBufferSizeSamples() ) + " frames, "
         + juce::String( dev->getOutputLatencyInSamples() ) + " frames output latency" );
}

void LiveHost::updateMidiInputs()
{
    const auto available = juce::MidiInput::getAvailableDevices();

    for( int i = openInputs.size(); --i >= 0; )
    {
        bool present = false;
        for( const auto& d : available )
            present = present || d.identifier == openInputs[i];
        if( !present )
        {
            log( "midi: lost " + openInputs[i] );
            devices.removeMidiInputDeviceCallback( openInputs[i], &player );
            devices.setMidiInputDeviceEnabled( openInputs[i], false );
            openInputs.remove( i );
        }
    }

    for( const auto& d : available )
    {
        if( openInputs.contains( d.identifier ) || !matches( config.midiInputs, d.name ) )
            continue;
        devices.setMidiInputDeviceEnabled( d.identifier, true );
        if( !devices.isMidiInputDeviceEnabled( d.identifier ) )
            continue;
        devices.addMidiInputDeviceCallback( d.identifier, &player );
        openInputs.add( d.identifier );
        log( "midi: listening to " + d.name + " (" + d.identifier + ")" );
    }
}

void LiveHost::report()
{
    const auto m = host->takeMeter();
    if( m.blocks == 0 )
        return;
    log( juce::String::formatted( "meter: %d notes, peak %.1f dBFS, cpu %.1f %% average, %.1f %% worst block",
                                  m.notes, juce::Decibels::gainToDecibels( m.peak, -200.f ),
                                  100.f * m.averageLoad, 100.f * m.worstLoad ) );
}

void LiveHost::saveIfChanged()
{
    juce::MemoryBlock now;
    plugin->getStateInformation( now );
    if( now == savedState && config.stateFile.existsAsFile() )
        return;
    if( saveState( *plugin, config.stateFile ) )
    {
        savedState = now;
        log( "state saved to " + config.stateFile.getFullPathName() );
    }
    else
        log( "error: cannot save state to " + config.stateFile.getFullPathName() );
}

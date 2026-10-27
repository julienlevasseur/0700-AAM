// SPDX-License-Identifier: AGPL-3.0-or-later
#include "HostProcessor.h"

std::unique_ptr<juce::AudioPluginInstance> loadPlugin( juce::AudioPluginFormatManager& formats,
                                                       const juce::String& fileOrIdentifier,
                                                       double sampleRate, int blockSize,
                                                       juce::String& error )
{
    juce::OwnedArray<juce::PluginDescription> found;
    for( auto* format : formats.getFormats() )
        if( format->fileMightContainThisPluginType( fileOrIdentifier ) )
            format->findAllTypesForFile( found, fileOrIdentifier );

    if( found.isEmpty() )
    {
        error = "no VST3 or LV2 plugin found in " + fileOrIdentifier;
        return nullptr;
    }

    // An instrument when the bundle has several plugins.
    const juce::PluginDescription* chosen = found.getFirst();
    for( auto* d : found )
        if( d->isInstrument )
        {
            chosen = d;
            break;
        }

    auto plugin = formats.createPluginInstance( *chosen, sampleRate, blockSize, error );
    if( !plugin )
        return nullptr;

    auto layout = plugin->getBusesLayout();
    for( auto& in : layout.inputBuses )
        in = juce::AudioChannelSet::disabled();
    if( !layout.outputBuses.isEmpty() )
        layout.outputBuses.getReference( 0 ) = juce::AudioChannelSet::stereo();
    if( !plugin->setBusesLayout( layout ) )
        plugin->disableNonMainBuses();
    return plugin;
}

bool saveState( juce::AudioPluginInstance& plugin, const juce::File& file )
{
    juce::MemoryBlock blob;
    plugin.getStateInformation( blob );

    juce::XmlElement xml( "KickPluginHostState" );
    xml.setAttribute( "plugin", plugin.getPluginDescription().createIdentifierString() );
    xml.setAttribute( "name", plugin.getName() );
    xml.addTextElement( blob.toBase64Encoding() );

    if( !file.getParentDirectory().createDirectory() )
        return false;
    juce::TemporaryFile temp( file );
    return xml.writeTo( temp.getFile() ) && temp.overwriteTargetFileWithTemporary();
}

bool restoreState( juce::AudioPluginInstance& plugin, const juce::File& file, juce::String& message )
{
    if( !file.existsAsFile() )
    {
        message = "no saved state";
        return false;
    }
    auto xml = juce::parseXML( file );
    if( !xml || !xml->hasTagName( "KickPluginHostState" ) )
    {
        message = "unreadable state file " + file.getFullPathName();
        return false;
    }
    if( xml->getStringAttribute( "plugin" ) != plugin.getPluginDescription().createIdentifierString() )
    {
        message = "saved state belongs to " + xml->getStringAttribute( "name" ) + ", not restored";
        return false;
    }
    juce::MemoryBlock blob;
    if( !blob.fromBase64Encoding( xml->getAllSubText().trim() ) )
    {
        message = "corrupt state in " + file.getFullPathName();
        return false;
    }
    plugin.setStateInformation( blob.getData(), (int)blob.getSize() );
    message = "state restored from " + file.getFullPathName();
    return true;
}

HostProcessor::HostProcessor( juce::AudioPluginInstance& p, int channel, float gainDb )
    : AudioProcessor( BusesProperties().withOutput( "Output", juce::AudioChannelSet::stereo(), true ) )
    , plugin( p )
    , midiChannel( channel )
    , gain( juce::Decibels::decibelsToGain( gainDb ) )
{
}

void HostProcessor::prepareToPlay( double sampleRate, int blockSize )
{
    plugin.setRateAndBufferSizeDetails( sampleRate, blockSize );
    plugin.prepareToPlay( sampleRate, blockSize );
    const int channels = juce::jmax( 2, plugin.getTotalNumInputChannels(), plugin.getTotalNumOutputChannels() );
    scratch.setSize( channels, blockSize, false, true, false );
    filtered.ensureSize( 4096 );
    ticksPerSample = (double)juce::Time::getHighResolutionTicksPerSecond() / sampleRate;
}

void HostProcessor::releaseResources() { plugin.releaseResources(); }

void HostProcessor::processBlock( juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi )
{
    const auto start = juce::Time::getHighResolutionTicks();
    const int  n     = buffer.getNumSamples();

    if( n > scratch.getNumSamples() ) // the device asked for more than it said
        scratch.setSize( scratch.getNumChannels(), n, false, false, true );

    juce::MidiBuffer* events = &midi;
    if( midiChannel > 0 )
    {
        filtered.clear();
        for( const auto m : midi )
        {
            const auto msg = m.getMessage();
            if( msg.getChannel() == 0 || msg.getChannel() == midiChannel )
                filtered.addEvent( msg, m.samplePosition );
        }
        events = &filtered;
    }
    int noteOns = 0;
    for( const auto m : *events )
        noteOns += m.getMessage().isNoteOn() ? 1 : 0;
    if( noteOns > 0 )
        notes.fetch_add( noteOns, std::memory_order_relaxed );

    juce::AudioBuffer<float> work( scratch.getArrayOfWritePointers(), scratch.getNumChannels(), n );
    work.clear();
    plugin.processBlock( work, *events );

    const int outs = plugin.getTotalNumOutputChannels();
    for( int ch = 0; ch < buffer.getNumChannels(); ++ch )
    {
        if( outs > 0 )
            buffer.copyFrom( ch, 0, work.getReadPointer( juce::jmin( ch, outs - 1 ) ), n, gain );
        else
            buffer.clear( ch, 0, n );
    }
    midi.clear();

    float p = 0;
    for( int ch = 0; ch < buffer.getNumChannels(); ++ch )
        p = juce::jmax( p, buffer.getMagnitude( ch, 0, n ) );
    if( p > peak.load( std::memory_order_relaxed ) )
        peak.store( p, std::memory_order_relaxed );

    const float load = n > 0 ? (float)( ( juce::Time::getHighResolutionTicks() - start ) / ( ticksPerSample * n ) ) : 0.f;
    if( load > worst.load( std::memory_order_relaxed ) )
        worst.store( load, std::memory_order_relaxed );
    loadSum.store( loadSum.load( std::memory_order_relaxed ) + load, std::memory_order_relaxed );
    blocks.fetch_add( 1, std::memory_order_release );
}

HostProcessor::Meter HostProcessor::takeMeter()
{
    Meter m;
    m.blocks      = blocks.exchange( 0, std::memory_order_acquire );
    m.notes       = notes.exchange( 0 );
    m.peak        = peak.exchange( 0 );
    m.worstLoad   = worst.exchange( 0 );
    m.averageLoad = m.blocks > 0 ? (float)( loadSum.exchange( 0 ) / m.blocks ) : 0.f;
    return m;
}

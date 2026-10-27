// SPDX-License-Identifier: AGPL-3.0-or-later
#include "OfflineRun.h"
#include "HostProcessor.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <iostream>

int runOffline( const HostConfig& config, const OfflineOptions& options )
{
    juce::AudioPluginFormatManager formats;
    formats.addDefaultFormats();

    juce::String error;
    auto plugin = loadPlugin( formats, config.plugin, config.sampleRate, config.blockSize, error );
    if( !plugin )
    {
        std::cerr << "error: " << error << "\n";
        return 1;
    }
    std::cout << "plugin: " << plugin->getName() << " (" << plugin->getPluginDescription().pluginFormatName
              << "), " << plugin->getTotalNumOutputChannels() << " outputs, "
              << plugin->getParameters().size() << " parameters\n";

    juce::String message;
    if( restoreState( *plugin, config.stateFile, message ) || config.stateFile.existsAsFile() )
        std::cout << message << "\n";

    HostProcessor host( *plugin, config.midiChannel, config.gainDb );
    const int     bs = config.blockSize;
    host.prepareToPlay( config.sampleRate, bs );

    std::unique_ptr<juce::AudioFormatWriter> writer;
    if( options.wavFile != juce::File() )
    {
        options.wavFile.deleteFile();
        if( auto out = options.wavFile.createOutputStream() )
            writer.reset( juce::WavAudioFormat().createWriterFor( out.release(), config.sampleRate, 2, 24, {}, 0 ) );
        if( !writer )
        {
            std::cerr << "error: cannot write " << options.wavFile.getFullPathName() << "\n";
            return 1;
        }
    }

    // Sixteenth notes at 120 BPM, each held for a thirty-second.
    const auto step  = (juce::int64)( config.sampleRate * 0.125 );
    const auto total = (juce::int64)( options.seconds * config.sampleRate );
    const int  ch    = config.midiChannel > 0 ? config.midiChannel : 10;

    juce::AudioBuffer<float> buffer( 2, bs );
    juce::MidiBuffer         midi;
    std::vector<double>      loads;
    double                   sumSquares = 0;
    float                    peak       = 0;
    const double             ticksPerSample = (double)juce::Time::getHighResolutionTicksPerSecond() / config.sampleRate;

    for( juce::int64 pos = 0; pos < total; pos += bs )
    {
        midi.clear();
        for( juce::int64 s = pos; s < pos + bs; ++s )
        {
            if( s % step == 0 )
                midi.addEvent( juce::MidiMessage::noteOn( ch, options.notes[(int)( ( s / step ) % options.notes.size() )], (juce::uint8)100 ), (int)( s - pos ) );
            if( s % step == step / 2 )
                midi.addEvent( juce::MidiMessage::noteOff( ch, options.notes[(int)( ( s / step ) % options.notes.size() )] ), (int)( s - pos ) );
        }

        const auto start = juce::Time::getHighResolutionTicks();
        host.processBlock( buffer, midi );
        loads.push_back( ( juce::Time::getHighResolutionTicks() - start ) / ( ticksPerSample * bs ) );

        for( int c = 0; c < 2; ++c )
        {
            peak = juce::jmax( peak, buffer.getMagnitude( c, 0, bs ) );
            for( int i = 0; i < bs; ++i )
                sumSquares += (double)buffer.getSample( c, i ) * buffer.getSample( c, i );
        }
        if( writer )
            writer->writeFromAudioSampleBuffer( buffer, 0, bs );
    }
    writer.reset();

    if( options.saveStateAtEnd )
        std::cout << ( saveState( *plugin, config.stateFile ) ? "state saved to " : "error: cannot save state to " )
                  << config.stateFile.getFullPathName() << "\n";
    host.releaseResources();

    std::sort( loads.begin(), loads.end() );
    auto pct = [&]( double q ) { return 100.0 * loads[(size_t)juce::jlimit( 0.0, (double)loads.size() - 1, q * ( loads.size() - 1 ) )]; };
    double mean = 0;
    for( auto l : loads )
        mean += l;
    mean /= (double)loads.size();

    const auto rms = std::sqrt( sumSquares / ( 2.0 * (double)loads.size() * bs ) );
    std::cout << juce::String::formatted( "output: peak %.1f dBFS, rms %.1f dBFS\n",
                                          juce::Decibels::gainToDecibels( peak, -200.f ),
                                          juce::Decibels::gainToDecibels( (float)rms, -200.f ) );
    std::cout << juce::String::formatted( "cpu per %d-frame block: mean %.2f %%, median %.2f %%, 99.9th %.2f %%, worst %.2f %%\n",
                                          bs, 100.0 * mean, pct( 0.5 ), pct( 0.999 ), pct( 1.0 ) );
    if( peak < juce::Decibels::decibelsToGain( -80.f ) )
    {
        std::cerr << "error: the plugin stayed silent\n";
        return 1;
    }
    return 0;
}

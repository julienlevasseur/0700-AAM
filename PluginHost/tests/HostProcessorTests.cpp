// SPDX-License-Identifier: AGPL-3.0-or-later
#include "../Source/HostProcessor.h"
#include "FakePlugin.h"

class HostProcessorTests : public juce::UnitTest
{
public:
    HostProcessorTests() : UnitTest( "HostProcessor", "kick-plugin-host" ) {}

    void runTest() override
    {
        constexpr int blockSize = 64;
        juce::AudioBuffer<float> buffer( 2, blockSize );

        auto play = []( HostProcessor& host, juce::AudioBuffer<float>& out, juce::MidiBuffer midi ) {
            out.clear();
            host.processBlock( out, midi );
            return midi;
        };

        beginTest( "stereo plugin goes to both outputs" );
        {
            FakePlugin    plugin( 2 );
            HostProcessor host( plugin, 0, 0.0f );
            host.prepareToPlay( 48000, blockSize );
            const auto left = play( host, buffer, {} );
            expectWithinAbsoluteError( buffer.getSample( 0, 10 ), 0.1f, 1e-6f );
            expectWithinAbsoluteError( buffer.getSample( 1, 10 ), 0.2f, 1e-6f );
            expect( left.isEmpty(), "the MIDI is consumed" );
        }

        beginTest( "mono plugin is copied to both outputs" );
        {
            FakePlugin    plugin( 1 );
            HostProcessor host( plugin, 0, 0.0f );
            host.prepareToPlay( 48000, blockSize );
            play( host, buffer, {} );
            expectWithinAbsoluteError( buffer.getSample( 0, 0 ), 0.1f, 1e-6f );
            expectWithinAbsoluteError( buffer.getSample( 1, 0 ), 0.1f, 1e-6f );
        }

        beginTest( "output gain" );
        {
            FakePlugin    plugin( 2 );
            HostProcessor host( plugin, 0, -20.0f );
            host.prepareToPlay( 48000, blockSize );
            play( host, buffer, {} );
            expectWithinAbsoluteError( buffer.getSample( 0, 0 ), 0.01f, 1e-6f );
            expectWithinAbsoluteError( buffer.getSample( 1, blockSize - 1 ), 0.02f, 1e-6f );
        }

        beginTest( "a block larger than prepared still plays" );
        {
            FakePlugin    plugin( 2 );
            HostProcessor host( plugin, 0, 0.0f );
            host.prepareToPlay( 48000, blockSize );
            juce::AudioBuffer<float> big( 2, blockSize * 4 );
            play( host, big, {} );
            expectWithinAbsoluteError( big.getSample( 1, blockSize * 4 - 1 ), 0.2f, 1e-6f );
        }

        beginTest( "all channels pass when midi_channel is 0" );
        {
            FakePlugin    plugin;
            HostProcessor host( plugin, 0, 0.0f );
            host.prepareToPlay( 48000, blockSize );
            play( host, buffer, notesOnChannels( { 1, 10, 16 } ) );
            expectEquals( plugin.received.size(), 3 );
        }

        beginTest( "other channels are dropped, but not messages without a channel" );
        {
            FakePlugin    plugin;
            HostProcessor host( plugin, 10, 0.0f );
            host.prepareToPlay( 48000, blockSize );
            auto midi = notesOnChannels( { 1, 10, 16 } );
            midi.addEvent( juce::MidiMessage::midiClock(), 5 );
            midi.addEvent( juce::MidiMessage::controllerEvent( 10, 74, 64 ), 6 );
            midi.addEvent( juce::MidiMessage::controllerEvent( 2, 74, 64 ), 7 );
            play( host, buffer, midi );
            expectEquals( plugin.received.size(), 3 );
            for( const auto& m : plugin.received )
                expect( m.getChannel() == 10 || m.getChannel() == 0, m.getDescription() );
        }

        beginTest( "meter counts note-ons and blocks, and resets" );
        {
            FakePlugin    plugin;
            HostProcessor host( plugin, 10, 0.0f );
            host.prepareToPlay( 48000, blockSize );
            auto midi = notesOnChannels( { 10, 10, 3 } );
            midi.addEvent( juce::MidiMessage::noteOff( 10, 36 ), 20 );
            play( host, buffer, midi );
            play( host, buffer, {} );

            const auto m = host.takeMeter();
            expectEquals( m.blocks, 2 );
            expectEquals( m.notes, 2, "only note-ons on the chosen channel" );
            expectWithinAbsoluteError( m.peak, 0.2f, 1e-6f );
            expect( m.averageLoad >= 0 && m.worstLoad >= m.averageLoad );

            const auto again = host.takeMeter();
            expectEquals( again.blocks, 0 );
            expectEquals( again.notes, 0 );
            expectEquals( again.peak, 0.0f );
        }

        stateTests();
    }

private:
    static juce::MidiBuffer notesOnChannels( std::initializer_list<int> channels )
    {
        juce::MidiBuffer midi;
        int              pos = 0;
        for( auto ch : channels )
            midi.addEvent( juce::MidiMessage::noteOn( ch, 36, (juce::uint8)100 ), pos++ );
        return midi;
    }

    void stateTests()
    {
        const auto dir  = juce::File::getSpecialLocation( juce::File::tempDirectory ).getNonexistentChildFile( "hoststate", "" );
        const auto file = dir.getChildFile( "sub/state.xml" );
        juce::String message;

        beginTest( "state is saved and restored, creating the folder" );
        {
            FakePlugin a;
            a.state = "cutoff=0.3";
            expect( saveState( a, file ) );
            expect( file.existsAsFile() );

            FakePlugin b;
            expect( restoreState( b, file, message ), message );
            expectEquals( b.state, juce::String( "cutoff=0.3" ) );
            expect( message.startsWith( "state restored from" ) );
        }

        beginTest( "saving again replaces the file" );
        {
            FakePlugin a;
            a.state = "cutoff=0.9";
            expect( saveState( a, file ) );
            FakePlugin b;
            expect( restoreState( b, file, message ) );
            expectEquals( b.state, juce::String( "cutoff=0.9" ) );
            expectEquals( dir.getChildFile( "sub" ).getNumberOfChildFiles( juce::File::findFiles ), 1, "no temporary file left" );
        }

        beginTest( "a state saved by another plugin is not given to this one" );
        {
            FakePlugin other( 2, "Other Synth" );
            other.state = "other";
            expect( saveState( other, file ) );
            FakePlugin b;
            expect( !restoreState( b, file, message ) );
            expectEquals( b.state, juce::String( "initial" ) );
            expectEquals( message, juce::String( "saved state belongs to Other Synth, not restored" ) );
        }

        beginTest( "missing, unreadable and corrupt state files" );
        {
            FakePlugin b;
            expect( !restoreState( b, dir.getChildFile( "none.xml" ), message ) );
            expectEquals( message, juce::String( "no saved state" ) );

            file.replaceWithText( "not xml" );
            expect( !restoreState( b, file, message ) );
            expect( message.startsWith( "unreadable state file" ) );

            file.replaceWithText( "<KickPluginHostState plugin=\"" + b.getPluginDescription().createIdentifierString() + "\">!!!</KickPluginHostState>" );
            expect( !restoreState( b, file, message ) );
            expect( message.startsWith( "corrupt state" ) );
            expectEquals( b.state, juce::String( "initial" ) );
        }

        dir.deleteRecursively();
    }
};

static HostProcessorTests hostProcessorTests;

// SPDX-License-Identifier: AGPL-3.0-or-later
#include "../Source/HostConfig.h"

class HostConfigTests : public juce::UnitTest
{
public:
    HostConfigTests() : UnitTest( "HostConfig", "kick-plugin-host" ) {}

    void runTest() override
    {
        const auto dir = juce::File::getSpecialLocation( juce::File::tempDirectory ).getNonexistentChildFile( "hostconfig", "" );
        dir.createDirectory();
        auto write = [&]( const juce::String& text ) {
            auto f = dir.getNonexistentChildFile( "plugin-host", ".conf" );
            f.replaceWithText( text );
            return f;
        };

        beginTest( "defaults" );
        {
            HostConfig c;
            expectEquals( c.midiInputs.joinIntoString( "," ), juce::String( "Kick:Out:1" ) );
            expectEquals( c.midiChannel, 0 );
            expectEquals( c.audioType, juce::String( "ALSA" ) );
            expectEquals( c.sampleRate, 48000.0 );
            expectEquals( c.blockSize, 256 );
            expectEquals( c.stateSaveSeconds, 30 );
        }

        beginTest( "a file with comments and blank lines" );
        {
            HostConfig c;
            const auto f = write( "# settings\n\nplugin = /opt/p/forge.lv2   # the instrument\n"
                                  "midi_inputs = Kick:Out:2, USB Keys\nmidi_channel=10\n"
                                  "audio_device = KICK shared output\nsample_rate = 44100\nblock_size = 128\n"
                                  "gain_db = -6.5\nstate_file = /tmp/s.xml\nstate_save_seconds = 0\n" );
            expect( c.load( f ).isEmpty() );
            expectEquals( c.plugin, juce::String( "/opt/p/forge.lv2" ) );
            expectEquals( c.midiInputs.size(), 2 );
            expectEquals( c.midiInputs[1], juce::String( "USB Keys" ) );
            expectEquals( c.midiChannel, 10 );
            expectEquals( c.audioDevice, juce::String( "KICK shared output" ) );
            expectEquals( c.sampleRate, 44100.0 );
            expectEquals( c.blockSize, 128 );
            expectEquals( c.gainDb, -6.5f );
            expectEquals( c.stateFile.getFullPathName(), juce::String( "/tmp/s.xml" ) );
            expectEquals( c.stateSaveSeconds, 0 );
        }

        beginTest( "the image's settings file loads" );
        {
            HostConfig c;
            const auto f = juce::File( KICK_SOURCE_DIR ).getChildFile( "Linux/buildroot/board/freescale/imx8mm_var_som/rootfs_overlay/etc/kick/plugin-host.conf" );
            expect( c.load( f ).isEmpty() );
            expectEquals( c.audioDevice, juce::String( "KICK shared output" ) );
        }

        beginTest( "errors name the file and line" );
        {
            HostConfig c;
            expectEquals( c.load( write( "plugin = a\nmidi_channel\n" ) ).fromFirstOccurrenceOf( ".conf", false, false ),
                          juce::String( ":2: expected key = value" ) );
            expect( c.load( write( "\n\nblock_size = 8\n" ) ).contains( ":3: block_size: out of range" ) );
            expect( c.load( dir.getChildFile( "missing.conf" ) ).startsWith( "config file not found" ) );
        }

        beginTest( "numbers are checked" );
        {
            HostConfig c;
            expect( c.set( "midi_channel", "abc" ).contains( "not a number" ) );
            expect( c.set( "midi_channel", "" ).contains( "not a number" ) );
            expect( c.set( "midi_channel", "17" ).contains( "out of range" ) );
            expect( c.set( "gain_db", "30" ).contains( "out of range" ) );
            expect( c.set( "sample_rate", "1000" ).contains( "out of range" ) );
            expectEquals( c.midiChannel, 0, "a refused value leaves the setting alone" );
            expect( c.set( "midi_channel", "16" ).isEmpty() );
            expectEquals( c.midiChannel, 16 );
            expect( c.set( "gain_db", "-60" ).isEmpty() );
        }

        beginTest( "unknown settings are refused" );
        {
            HostConfig c;
            expectEquals( c.set( "midi_input", "x" ), juce::String( "unknown setting: midi_input" ) );
        }

        beginTest( "midi_inputs drops spaces and empty entries" );
        {
            HostConfig c;
            expect( c.set( "midi_inputs", " a , b ,, " ).isEmpty() );
            expectEquals( c.midiInputs.joinIntoString( "|" ), juce::String( "a|b" ) );
            expect( c.set( "midi_inputs", "*" ).isEmpty() );
            expectEquals( c.midiInputs.joinIntoString( "|" ), juce::String( "*" ) );
        }

        dir.deleteRecursively();
    }
};

static HostConfigTests hostConfigTests;

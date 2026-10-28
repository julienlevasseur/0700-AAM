// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Unit tests of kick-plugin-host: settings, audio processing and plugin state,
// with a fake plugin, so no audio device, MIDI port or plugin bundle is needed.

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;

    juce::UnitTestRunner runner;
    runner.setAssertOnFailure( false );
    runner.runTestsInCategory( "kick-plugin-host" );

    int failures = 0;
    for( int i = 0; i < runner.getNumResults(); ++i )
        failures += runner.getResult( i )->failures;
    return failures > 0 ? 1 : 0;
}

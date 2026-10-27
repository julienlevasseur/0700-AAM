// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "HostConfig.h"

// Plays a drum pattern through the plugin without an audio device, and reports
// the output level and the time per block. Optionally writes a WAV file and
// saves the plugin state at the end. Returns the process exit code: 1 when the
// plugin does not load or stays silent.
struct OfflineOptions
{
    double             seconds = 4.0;
    juce::Array<int>   notes{ 36, 38, 42, 46 };
    juce::File         wavFile;
    bool               saveStateAtEnd = false;
};

int runOffline( const HostConfig& config, const OfflineOptions& options );

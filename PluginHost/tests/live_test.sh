#!/bin/sh
# Live test of kick-plugin-host: JACK's dummy driver for audio, a fake KICK on
# the ALSA sequencer for MIDI. The host starts before the fake KICK, as it can
# at boot, and has to find its port, play, notice the port going away and save
# its state when stopped.
#   live_test.sh <kick-plugin-host> <plugin bundle> <work dir>
# Needs /dev/snd/seq (docker run --device /dev/snd/seq --group-add audio).
set -u
HOST=$1 PLUGIN=$2 WORK=$3
HERE=$(dirname "$0")
mkdir -p "$WORK"
cc -O2 -o "$WORK/fake_kick" "$HERE/fake_kick.c" -lasound || exit 1

export JACK_NO_AUDIO_RESERVATION=1 JACK_DEFAULT_SERVER=kickhosttest
jackd -n kickhosttest -d dummy -r 48000 -p 256 >"$WORK/jackd.log" 2>&1 &
JACKD=$!
sleep 2

"$HOST" --config=/dev/null --plugin="$PLUGIN" --audio_type=JACK --audio_device=system \
        --midi_inputs=Kick:Out:1 --state_file="$WORK/state.xml" --state_save_seconds=0 \
        >"$WORK/host.log" 2>&1 &
PID=$!
sleep 3
"$WORK/fake_kick" 12 >"$WORK/fake_kick.log" 2>&1
sleep 4
kill -TERM $PID
wait $PID
STATUS=$?
kill $JACKD 2>/dev/null
wait $JACKD 2>/dev/null

cat "$WORK/host.log"
fail=0
check() { if grep -qE "$2" "$WORK/host.log"; then echo "ok   $1"; else echo "FAIL $1"; fail=1; fi; }
check "audio device opened" '^audio: JACK'
check "found the KICK port after starting" '^midi: listening to Kick:Out:1'
check "notes arrived and made sound" '^meter: [1-9][0-9]* notes, peak -([0-9]|[1-7][0-9])\.'
check "noticed the port going away" '^midi: lost'
check "saved its state on exit" '^state saved to '
check "stopped cleanly" '^stopped'
if grep -q '^audio: no ' "$WORK/host.log"; then echo "FAIL audio type or device not found"; fail=1; else echo "ok   audio type and device found at once"; fi

[ $STATUS -eq 0 ] && echo "ok   exit status 0" || { echo "FAIL exit status $STATUS"; fail=1; }
exit $fail

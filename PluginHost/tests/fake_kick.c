// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Stands in for the KICK application in tests: an ALSA sequencer client with
// an output port named like the KICK's ("Kick:Out:1"), playing sixteenth
// notes at 120 BPM for the given number of seconds.
//   fake_kick [seconds] [port name]

#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char** argv)
{
	const double seconds = argc > 1 ? atof(argv[1]) : 10.0;
	const char*  name    = argc > 2 ? argv[2] : "Kick:Out:1";

	snd_seq_t* seq;
	if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_OUTPUT, 0) < 0) {
		fprintf(stderr, "fake_kick: cannot open the ALSA sequencer\n");
		return 1;
	}
	snd_seq_set_client_name(seq, "KT_Kick");
	const int port = snd_seq_create_simple_port(seq, name, SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ,
	                                            SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
	if (port < 0) {
		fprintf(stderr, "fake_kick: cannot create port\n");
		return 1;
	}
	printf("fake_kick: %d:%d %s\n", snd_seq_client_id(seq), port, name);
	fflush(stdout);

	const int notes[] = {36, 38, 42, 46};
	const int steps   = (int)(seconds * 8);
	for (int i = 0; i < steps; ++i) {
		snd_seq_event_t ev;
		snd_seq_ev_clear(&ev);
		snd_seq_ev_set_source(&ev, port);
		snd_seq_ev_set_subs(&ev);
		snd_seq_ev_set_direct(&ev);
		snd_seq_ev_set_noteon(&ev, 9, notes[i % 4], 100);
		snd_seq_event_output_direct(seq, &ev);

		struct timespec half = {0, 62500000};
		nanosleep(&half, NULL);
		snd_seq_ev_set_noteoff(&ev, 9, notes[i % 4], 0);
		snd_seq_event_output_direct(seq, &ev);
		nanosleep(&half, NULL);
	}
	snd_seq_close(seq);
	return 0;
}

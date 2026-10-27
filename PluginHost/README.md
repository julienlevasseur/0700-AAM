# kick-plugin-host

Plays a VST3 or LV2 instrument on the KICK, next to the KICK application. The
KICK application plays it: a **MIDI program** sends its pads and sequencer
notes to one of the virtual MIDI ports it publishes (`Kick:Out:1` …
`Kick:Out:N`), and the host listens to that port. External USB MIDI devices
work too.

The host runs as its own process (`kick-plugin-host.service`), so the stock
`KT_Kick` binary is used unchanged. Both play through the shared ALSA output
(`dmix`) defined in `/etc/asound.conf`.

What it does not do (the plugin is outside the KICK application):

- the plugin's sound skips the KICK's mixer, inserts, sends and looper/sampler
  recording, and goes straight to the main output;
- no plugin editor is shown; its parameters are played with MIDI CC from a MIDI
  program or a controller;
- one plugin at a time.

## Install on a KICK

On a KICK running the development image (root over SSH):

```sh
PluginHost/deploy.sh check   <kick-address>        # what the device has; changes nothing
PluginHost/deploy.sh install <kick-address> path/to/ebm_forge.lv2
PluginHost/deploy.sh uninstall <kick-address>      # restores the previous asound.conf
```

`install` copies the host to `/opt/kick-plugin-host/bin`, the bundle to
`/opt/kick-plugin-host/plugins`, the settings to `/etc/kick/plugin-host.conf`
(an existing file is kept and only pointed at the new plugin), installs the
shared-output `/etc/asound.conf` (backing up the previous one once), runs a
5-second benchmark of the plugin on the device, enables the service and
restarts the KICK application so that it opens the shared output.

**Check the audio first.** The shared output assumes the KICK application
plays through ALSA's `default` device at 48 kHz, 32-bit, stereo. `deploy.sh
check` shows which process holds the sound card and the parameters it uses
(`hw_params`). If `KT_Kick` opens `hw:0` directly, the shared output cannot
work, and the host will log `audio: cannot open … (retrying)`. If the rate,
channels or period differ, set them in the `kickshare` block of
`asound.conf` before installing.

A new image built from `Linux/buildroot` carries the service, the settings and
`asound.conf` (the overlay files); the service starts only once
`/opt/kick-plugin-host/bin/kick-plugin-host` exists.

Logs: `journalctl -u kick-plugin-host -f`. Every 10 s the host logs the notes it
received, the output peak and its CPU load, for example
`meter: 48 notes, peak -8.5 dBFS, cpu 1.1 % average, 4.2 % worst block`.

## Settings

`/etc/kick/plugin-host.conf` (`key = value`), or `--key=value` on the command
line:

| Key | Default | |
|---|---|---|
| `plugin` | | `.vst3` or `.lv2` bundle path, or an LV2 URI |
| `midi_inputs` | `Kick:Out:1` | port name fragments, comma separated; `*` for all |
| `midi_channel` | `0` | 1–16, 0 for all |
| `audio_type` | `ALSA` | `ALSA` or `JACK` |
| `audio_device` | `default` | device name or a fragment; the image's is `KICK shared output` |
| `sample_rate`, `block_size` | `48000`, `256` | |
| `gain_db` | `0` | output gain |
| `state_file` | `/media/kickstorage/plugin-host/state.xml` | the plugin's settings, restored at start |
| `state_save_seconds` | `30` | saves when changed; 0 only on exit |

`kick-plugin-host --list-audio` lists the audio devices and MIDI inputs it sees.

## Offline test and benchmark

Without an audio device, the host plays a sixteenth-note pattern through the
plugin and reports the level and the CPU time per block; it exits with 1 if the
plugin does not load or stays silent:

```sh
kick-plugin-host --plugin=ebm_forge.lv2 --render=out.wav --seconds=4
kick-plugin-host --plugin=ebm_forge.vst3 --bench --seconds=30 --block_size=128
```

## Building

Native (x86_64), in the Kick build image:

```sh
docker build -t kick-build Kick/docker
docker run --rm -u $(id -u):$(id -g) -e HOME=/tmp -v "$PWD":/src -w /src/PluginHost kick-build \
    sh -c 'cmake -S . -B build -GNinja -DCMAKE_BUILD_TYPE=Release && ninja -C build'
```

For the KICK (aarch64), cross compiled on Debian 11 (glibc 2.31, GCC 10, no
newer than the KICK's own toolchain, so the binary runs there):

```sh
docker build -t kick-host-arm64 -f PluginHost/docker/Dockerfile.arm64 PluginHost/docker
docker run --rm -u $(id -u):$(id -g) -e HOME=/tmp -v "$PWD":/src -w /src/PluginHost kick-host-arm64 \
    sh -c 'cmake -S . -B build-arm64 -GNinja -DCMAKE_BUILD_TYPE=Release \
               -DCMAKE_TOOLCHAIN_FILE=/opt/aarch64-linux-gnu.cmake && ninja -C build-arm64'
```

With the Buildroot SDK instead (`make KTKick_defconfig && make sdk` in
`Linux/buildroot`), pass its `share/buildroot/toolchainfile.cmake` as
`CMAKE_TOOLCHAIN_FILE`; JUCE 7.0.12 needs CMake 3.22 or newer, so use a recent
CMake rather than the SDK's.

The plugins themselves must be built for aarch64 Linux and against a C library
no newer than the KICK's (glibc 2.32): build them on Debian 11 or with the SDK.

## Tests

`tests/live_test.sh` runs the host against JACK's dummy driver and a fake KICK
(`tests/fake_kick.c`, an ALSA sequencer client with a `Kick:Out:1` port), with
the host started first, as at boot:

```sh
docker run --rm -u $(id -u):$(id -g) --group-add $(getent group audio | cut -d: -f3) \
    --device /dev/snd/seq -e HOME=/tmp -v "$PWD":/src kick-build \
    sh /src/PluginHost/tests/live_test.sh \
       /src/PluginHost/build/kick-plugin-host_artefacts/Release/kick-plugin-host \
       /path/to/ebm_forge.lv2 /tmp/live
```

It checks that the host opens the audio device, finds the port once it
appears, turns its notes into sound, notices the port going away, saves the
plugin state and stops cleanly on SIGTERM.

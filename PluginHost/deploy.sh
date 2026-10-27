#!/bin/sh
# Installs kick-plugin-host on a running KICK over SSH (development image: root,
# no password, Dropbear), or checks what the device has.
#
#   deploy.sh check     <kick-address>
#   deploy.sh install   <kick-address> <plugin.lv2|plugin.vst3> [kick-plugin-host binary]
#   deploy.sh uninstall <kick-address>
#
# install keeps an existing /etc/kick/plugin-host.conf (it only points it at the
# new plugin), backs up /etc/asound.conf once, and restarts the KICK
# application so that it plays through the shared output.
set -eu

HERE=$(cd "$(dirname "$0")" && pwd)
OVERLAY=$HERE/../Linux/buildroot/board/freescale/imx8mm_var_som/rootfs_overlay
SSH_OPTS="-o StrictHostKeyChecking=accept-new -o ConnectTimeout=10"

usage() { sed -n '2,12p' "$0" | sed 's/^# \{0,1\}//'; exit 2; }
[ $# -ge 2 ] || usage
CMD=$1 KICK=$2
on_kick() { ssh $SSH_OPTS "root@$KICK" "$@"; }
send_dir() { tar -C "$1" -cf - . | on_kick "mkdir -p '$2' && tar -C '$2' -xf -"; }
send_file() { on_kick "cat > '$2'" < "$1"; }

case $CMD in
check)
	on_kick 'set +e
echo "== system"; uname -m; nproc; free -m | sed -n 2p
echo "== C library"; ls /lib/libc.so.6 /lib/ld-linux-aarch64.so.1 2>&1; /lib/libc.so.6 2>/dev/null | head -1
echo "== libraries the host and plugin need"
for l in libstdc++.so.6 libasound.so.2 libfreetype.so.6 libcairo.so.2 libX11.so.6 libjack.so.0; do
  f=$(ls /usr/lib/$l /lib/$l 2>/dev/null | head -1); echo "  $l: ${f:-MISSING}"; done
strings $(ls /usr/lib/libstdc++.so.6 /lib/libstdc++.so.6 2>/dev/null | head -1) | grep -E "^GLIBCXX_3\.4\.(2[2-9]|3)" | tail -1
echo "== sound cards"; cat /proc/asound/cards
echo "== who has the sound card open"
for p in /proc/[0-9]*; do ls -l $p/fd 2>/dev/null | grep -q /dev/snd/pcm && echo "  $(cat $p/comm) (pid ${p#/proc/})"; done
echo "== playback parameters in use (match them in asound.conf)"
for f in /proc/asound/card0/pcm*p/sub*/hw_params; do echo "  $f:"; sed "s/^/    /" $f; done
echo "== MIDI ports (the KICK publishes Kick:Out:N)"; aconnect -l 2>/dev/null || cat /proc/asound/seq/clients
echo "== ALSA configuration"; head -20 /etc/asound.conf
echo "== plugin host"; systemctl is-enabled kick-plugin-host 2>&1; systemctl is-active kick-plugin-host 2>&1
journalctl -u kick-plugin-host -n 20 --no-pager 2>/dev/null'
	;;
install)
	[ $# -ge 3 ] || usage
	PLUGIN=${3%/}
	BIN=${4:-$HERE/build-arm64/kick-plugin-host_artefacts/Release/kick-plugin-host}
	[ -d "$PLUGIN" ] || { echo "not a plugin bundle: $PLUGIN"; exit 1; }
	[ -x "$BIN" ] || { echo "no host binary: $BIN (build it, see PluginHost/README.md)"; exit 1; }
	NAME=$(basename "$PLUGIN")

	echo "installing $BIN and $NAME on $KICK"
	on_kick 'mkdir -p /opt/kick-plugin-host/bin /opt/kick-plugin-host/plugins /etc/kick /media/kickstorage/plugin-host'
	send_file "$BIN" /opt/kick-plugin-host/bin/kick-plugin-host
	on_kick "chmod 755 /opt/kick-plugin-host/bin/kick-plugin-host && rm -rf '/opt/kick-plugin-host/plugins/$NAME'"
	send_dir "$PLUGIN" "/opt/kick-plugin-host/plugins/$NAME"

	send_file "$OVERLAY/etc/kick/plugin-host.conf" /etc/kick/plugin-host.conf.default
	send_file "$OVERLAY/etc/systemd/system/kick-plugin-host.service" /etc/systemd/system/kick-plugin-host.service
	send_file "$OVERLAY/etc/asound.conf" /etc/asound.conf.plugin-host
	on_kick "set -e
[ -f /etc/kick/plugin-host.conf ] || cp /etc/kick/plugin-host.conf.default /etc/kick/plugin-host.conf
sed -i 's|^plugin *=.*|plugin = /opt/kick-plugin-host/plugins/$NAME|' /etc/kick/plugin-host.conf
[ -f /etc/asound.conf.before-plugin-host ] || cp /etc/asound.conf /etc/asound.conf.before-plugin-host
mv /etc/asound.conf.plugin-host /etc/asound.conf
/opt/kick-plugin-host/bin/kick-plugin-host --config=/etc/kick/plugin-host.conf --bench --seconds=5
systemctl daemon-reload
systemctl enable kick-plugin-host
systemctl restart ktkick
sleep 20
systemctl restart kick-plugin-host
sleep 5
journalctl -u kick-plugin-host -n 15 --no-pager"
	echo "installed. In the KICK, add a MIDI program and send it to Kick:Out:1."
	;;
uninstall)
	on_kick 'set -e
systemctl disable --now kick-plugin-host 2>/dev/null || true
rm -f /etc/systemd/system/kick-plugin-host.service
systemctl daemon-reload
[ ! -f /etc/asound.conf.before-plugin-host ] || mv /etc/asound.conf.before-plugin-host /etc/asound.conf
rm -rf /opt/kick-plugin-host
systemctl restart ktkick
echo "removed; the plugin state is kept in /media/kickstorage/plugin-host"'
	;;
*) usage ;;
esac

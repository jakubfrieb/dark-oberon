#!/usr/bin/env bash
# Network latency measurement: a headless server and two GUI clients on this machine, optionally
# behind netem delay, while both clients keep moving their units. Prints the `netstat` latency
# histograms of every host (and the server `perf` step times) into OUT/summary.txt.
#
# Usage: tests/net/run.sh BIN_DIR OUT_DIR
#        BIN_DIR holds dark-oberon and dark-oberon-server (tests/net/build.sh)
# Env:   NET_DELAY_MS=0     netem one-way delay on loopback (every packet, so both directions)
#        NET_JITTER_MS=0    netem jitter (+-)
#        NET_SECS=60        walking after the warm-up   NET_WARMUP=15  seconds before `perf reset`
#        NET_CPUS=""        CPU players on the server, e.g. "hard hard"
#        NET_SPAWN=""       server `spawn` arguments after the start, e.g. "600 fight"
#        NET_PORT=17100     NET_MAP=crossroads      NET_MIN_SAMPLES=300 (per client, else INVALID)
#
# Everything runs in an unprivileged user + network namespace (unshare -Urn), so netem needs no
# root and touches nothing outside. The clients use the X display $DISPLAY and private HOMEs; the
# harness clicks into their windows (found by PID) with xdotool, so leave the mouse alone.
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/../.." && pwd)

if [ "${1:-}" != --inner ]; then
  BIN=$(cd "${1:?usage: run.sh BIN_DIR OUT_DIR}" && pwd)
  mkdir -p "${2:?usage: run.sh BIN_DIR OUT_DIR}"
  OUT=$(cd "$2" && pwd)
  exec unshare -Urn "$0" --inner "$BIN" "$OUT"
fi

BIN=$2 OUT=$3
DELAY=${NET_DELAY_MS:-0} JITTER=${NET_JITTER_MS:-0}
SECS=${NET_SECS:-60} WARMUP=${NET_WARMUP:-15}
PORT=${NET_PORT:-17100} MAP=${NET_MAP:-crossroads}
MIN_SAMPLES=${NET_MIN_SAMPLES:-300}
export DISPLAY=${DISPLAY:-:0}
rm -rf "$OUT"/{srv,c1,c2}* "$OUT"/*.png "$OUT/summary.txt"

PIDS=()
cleanup() {   # the server ignores SIGTERM (SDL turns it into a quit event nobody reads): ask it to quit
  [ -p "$OUT/srv.fifo" ] && timeout 2 sh -c "echo quit >'$OUT/srv.fifo'" 2>/dev/null || true
  for p in "${PIDS[@]}"; do kill "$p" 2>/dev/null || true; done
  sleep 2
  for p in "${PIDS[@]}"; do kill -KILL "$p" 2>/dev/null || true; done
}
trap cleanup EXIT

ip link set lo up
if [ "$DELAY" != 0 ] || [ "$JITTER" != 0 ]; then
  tc qdisc add dev lo root netem delay "${DELAY}ms" "${JITTER}ms"
fi
tc qdisc show dev lo >"$OUT/netem.txt"

# --- server (commands through a fifo) ---
mkfifo "$OUT/srv.fifo"
(exec 3<>"$OUT/srv.fifo"; mkdir -p "$OUT/srv.home"; HOME="$OUT/srv.home" exec "$BIN/dark-oberon-server" \
   --map "$MAP" --data "$REPO" --port "$PORT" <&3 >"$OUT/srv.log" 2>&1) &
PIDS+=($!)
srv() { echo "$*" >"$OUT/srv.fifo"; }
sleep 1.5

# --- clients ---
WINS=()
for c in 1 2; do
  mkdir -p "$OUT/c$c.home/.dark-oberon"
  cat >"$OUT/c$c.home/.dark-oberon/config.cfg" <<EOF
fullscreen false
resolution "1024x768"
vert_sync false
show_disconnect_warning false
map_move_speed 1
snd_master_volume 0
snd_menu_music false
snd_game_music false
snd_unit_speech false
player_name "Bot$c"
address "127.0.0.1:$PORT"
EOF
  (cd "$REPO" && HOME="$OUT/c$c.home" exec "$BIN/dark-oberon" >"$OUT/c$c.log" 2>&1) &
  pid=$!
  PIDS+=($pid)
  w=
  for _ in $(seq 60); do sleep 0.5; w=$(xdotool search --pid "$pid" 2>/dev/null | head -1) && [ -n "$w" ] && break; done
  [ -n "$w" ] || { echo "client $c: no window"; exit 2; }
  WINS+=("$w")
done
sleep 2

click() { xdotool windowactivate --sync "$1" mousemove --window "$1" "$2" "$3" sleep 0.15 click "${4:-1}"; }
drag() {
  xdotool windowactivate --sync "$1" mousemove --window "$1" "$2" "$3" sleep 0.1 mousedown 1 sleep 0.1 \
    mousemove --window "$1" $((($2 + $4) / 2)) $((($3 + $5) / 2)) sleep 0.1 mousemove --window "$1" "$4" "$5" \
    sleep 0.2 mouseup 1
}
xdotool windowmove "${WINS[0]}" 0 0 windowmove "${WINS[1]}" 896 0
for w in "${WINS[@]}"; do   # main menu: Play -> Connect -> Connect
  click "$w" 512 250; sleep 1; click "$w" 512 336; sleep 1; click "$w" 634 478; sleep 2.5
done
for lv in ${NET_CPUS:-}; do srv addcpu "$lv"; sleep 0.5; done
srv start
for c in 1 2; do
  for _ in $(seq 120); do grep -q 'Update: Running' "$OUT/c$c.log" && break; sleep 0.5; done
  grep -q 'Update: Running' "$OUT/c$c.log" || { echo "client $c did not start the game"; exit 2; }
done
sleep 2
[ -n "${NET_SPAWN:-}" ] && srv spawn "$NET_SPAWN"

# Keep the starting units walking between two points on open ground near the base (the view starts
# centred on it): every step is a state event broadcast to the other hosts. The units are selected
# again each time, a click can get lost.
walk() {   # walk SECONDS
  local end=$((SECONDS + $1)) t=0
  while [ $SECONDS -lt $end ]; do
    for w in "${WINS[@]}"; do
      if [ $t = 0 ]; then drag "$w" 340 200 680 330; click "$w" 600 420 3
      else drag "$w" 450 300 750 480; click "$w" 400 300 3; fi
    done
    t=$((1 - t))
    sleep 1.5
  done
}
walk "$WARMUP"
srv perf reset
import -window "${WINS[0]}" "$OUT/c1.png" || true
import -window "${WINS[1]}" "$OUT/c2.png" || true
walk "$SECS"

# Every host counts from 5 s after its game started (NetStatReset in ProcessFunction). The clients
# log their numbers when they leave the game (SIGTERM closes the window like the user would).
srv netstat; srv perf; sleep 1
kill -TERM "${PIDS[1]}" "${PIDS[2]}"
for _ in $(seq 20); do kill -0 "${PIDS[1]}" 2>/dev/null || kill -0 "${PIDS[2]}" 2>/dev/null || break; sleep 0.5; done
srv quit; sleep 2

# --- summary: the last netstat block of every host ---
log() { if [ "$1" = srv ]; then echo "$OUT/srv.log"; else echo "$OUT/$1.home/.dark-oberon/logs/full.log"; fi; }
last_block() {   # the server's `netstat` command output, the clients' game-end log line
  if [ "$1" = "$OUT/srv.log" ]; then grep -E '^netstat: [tdpwc]' "$1" | head -5; else grep -o 'netstat: [tdpwc].*' "$1" | tail -5; fi || true
}
{
  echo "# $(date -Iseconds) bin=$BIN delay=${DELAY}ms jitter=${JITTER}ms secs=$SECS warmup=$WARMUP cpus='${NET_CPUS:-}' spawn='${NET_SPAWN:-}'"
  head -3 "$BIN/BUILD_INFO.txt" 2>/dev/null | sed 's/^/# /'
  for h in srv c1 c2; do echo "[$h]"; last_block "$(log $h)"; done
  echo "[srv perf]"
  grep -E '^perf: (step|events|[0-9]+ steps)' "$OUT/srv.log" | tail -3
  # Clock sync error between two hosts on one machine = difference of their clock offsets.
  off() { last_block "$(log $1)" | sed -n 's/.*clock offset=\([-0-9.]*\).*/\1/p'; }
  echo "[clock] c1-srv=$(echo "$(off c1) - $(off srv)" | bc) ms  c2-srv=$(echo "$(off c2) - $(off srv)" | bc) ms  c2-c1=$(echo "$(off c2) - $(off c1)" | bc) ms"
  for c in c1 c2; do
    n=$(last_block "$(log $c)" | sed -n 's/.*perceived ms n=\([0-9]*\).*/\1/p')
    [ "${n:-0}" -ge "$MIN_SAMPLES" ] || echo "INVALID: $c has only ${n:-0} perceived samples (< $MIN_SAMPLES)"
  done
} >"$OUT/summary.txt"
cat "$OUT/summary.txt"

#!/usr/bin/env bash
# The three baseline scenarios of tests/net/BASELINE.md for one build (tests/net/run.sh each):
#   a  localhost, no delay
#   b  netem 20 ms one-way (+-2 ms jitter) in both directions
#   c  localhost, 2 CPU players and `spawn 600 fight` (server step time and dispatch under load)
#
# Usage: tests/net/baseline.sh BIN_DIR OUT_ROOT [scenarios]   e.g. baseline.sh ../baselines/B1 /tmp/b1 "a b"
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
BIN=${1:?usage: baseline.sh BIN_DIR OUT_ROOT [scenarios]}
ROOT=${2:?usage: baseline.sh BIN_DIR OUT_ROOT [scenarios]}
for s in ${3:-a b c}; do
  case $s in
    a) NET_DELAY_MS=0 NET_JITTER_MS=0 "$HERE/run.sh" "$BIN" "$ROOT/a" ;;
    b) NET_DELAY_MS=20 NET_JITTER_MS=2 "$HERE/run.sh" "$BIN" "$ROOT/b" ;;
    c) NET_CPUS="hard hard" NET_SPAWN="600 fight" NET_WARMUP=30 "$HERE/run.sh" "$BIN" "$ROOT/c" ;;
    *) echo "unknown scenario $s"; exit 2 ;;
  esac
done

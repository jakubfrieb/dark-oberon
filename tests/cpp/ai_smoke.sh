#!/usr/bin/env bash
# Headless CPU-vs-CPU smoke test of the AI.
# Builds dark-oberon-server from the working tree in a temp copy (client .o files stay intact),
# adds CPU players, starts the match, dumps `logs <slot>` at start and after SECS seconds.
#
# Usage: tests/cpp/ai_smoke.sh [map] [seconds] [level...]     e.g. ai_smoke.sh trial 300 hard easy
set -euo pipefail
REPO=$(cd "$(dirname "$0")/../.." && pwd)
MAP=${1:-trial}
SECS=${2:-5}
shift $(( $# > 2 ? 2 : $# ))
LEVELS=("$@")
[ ${#LEVELS[@]} -eq 0 ] && LEVELS=(hard easy)

BUILD=${AI_SMOKE_BUILD:-$(mktemp -d)}
mkdir -p "$BUILD/src"
rsync -a --exclude '*.o' --exclude '.compile_flags' "$REPO/src/" "$BUILD/src/"
if ! make -C "$BUILD/src" server -j8 >"$BUILD/build.log" 2>&1; then
  tail -30 "$BUILD/build.log"
  exit 2
fi

# The server does not always exit on `quit` once a match runs (pre-existing); timeout -k ends it.
set +e
{
  sleep 1
  for lv in "${LEVELS[@]}"; do echo "addcpu $lv"; sleep 1; done
  echo "logs on"
  echo start
  sleep 3
  for i in $(seq 1 ${#LEVELS[@]}); do echo "logs $i"; done
  sleep "$SECS"
  for i in $(seq 1 ${#LEVELS[@]}); do echo "logs $i"; done
  echo quit
} | timeout -k 5 $((SECS + 60)) "$BUILD/dark-oberon-server" --map "$MAP" --data "$REPO" --port $((17200 + RANDOM % 500)) 2>&1
rc=$?
case $rc in 0|124|137) exit 0 ;; *) exit $rc ;; esac

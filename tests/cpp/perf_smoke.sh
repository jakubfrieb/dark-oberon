#!/usr/bin/env bash
# Performance test of the game simulation on the headless dedicated server.
#
# Builds dark-oberon-server from the working tree in a temp copy (client .o files stay intact),
# adds CPU players, starts the match, places UNITS military units around the bases and sends them
# all to the middle of the map (`spawn UNITS fight`: a burst of path searches, then one big battle).
# After WARMUP seconds it measures SECS seconds and prints `perf`: the simulation step time
# (events + AI, budget 20 ms per step at 50 steps/s), the path search jobs (thread pool) and how
# many searches wait in the queue.
#
# Usage: tests/cpp/perf_smoke.sh [units] [seconds] [warmup] [map]
#        e.g. perf_smoke.sh 600 90 60 crossroads
# Env:   PERF_MAX_STEP_P99_MS=5   fail (exit 1) when the step p99 is above this
#        PERF_MAX_PATH_QUEUE=100  fail when more path searches than this ever wait in the queue
#        PERF_CPUS="hard medium easy hard"  CPU players (the map must have room for them)
set -euo pipefail
REPO=$(cd "$(dirname "$0")/../.." && pwd)
UNITS=${1:-600}
SECS=${2:-90}
WARMUP=${3:-60}
MAP=${4:-crossroads}
read -r -a CPUS <<< "${PERF_CPUS:-hard medium easy hard}"

BUILD=${PERF_BUILD:-$(mktemp -d)}
mkdir -p "$BUILD/src"
rsync -a --exclude '*.o' --exclude '.compile_flags' "$REPO/src/" "$BUILD/src/"
cp "$REPO/VERSION" "$BUILD/VERSION"   # src/Makefile builds build_info.h from ../VERSION
if ! make -C "$BUILD/src" server -j"$(nproc)" >"$BUILD/build.log" 2>&1; then
  tail -30 "$BUILD/build.log"
  exit 2
fi

OUT="$BUILD/perf.log"
set +e
{
  sleep 1
  for lv in "${CPUS[@]}"; do echo "addcpu $lv"; sleep 0.5; done
  echo start
  sleep 4
  echo "spawn $UNITS fight"
  sleep "$WARMUP"
  echo "perf reset"
  sleep "$SECS"
  echo perf
  sleep 1
  echo quit
} | timeout -k 5 $((WARMUP + SECS + 60)) "$BUILD/dark-oberon-server" --map "$MAP" --data "$REPO" \
    --port $((17200 + RANDOM % 500)) >"$OUT" 2>&1
set -e

grep -E '^(spawn|perf):|Path finding:' "$OUT" || { echo "no perf output; last lines:"; tail -20 "$OUT"; exit 2; }
if grep -qE 'terminate called|Segmentation|Crit:' "$OUT"; then
  echo "FAIL: the server crashed or logged a critical error:"
  grep -E 'terminate called|Segmentation|Crit:' "$OUT" | head -5
  exit 1
fi

fail=0
step_p99=$(sed -n 's/^perf: step .* p99=\([0-9.]*\).*/\1/p' "$OUT")
queue_max=$(sed -n 's/^perf: path queue .* max=\([0-9]*\).*/\1/p' "$OUT")
if [ -n "${PERF_MAX_STEP_P99_MS:-}" ] && awk -v a="$step_p99" -v b="$PERF_MAX_STEP_P99_MS" 'BEGIN{exit !(a > b)}'; then
  echo "FAIL: step p99 ${step_p99} ms > ${PERF_MAX_STEP_P99_MS} ms"
  fail=1
fi
if [ -n "${PERF_MAX_PATH_QUEUE:-}" ] && [ "${queue_max:-0}" -gt "$PERF_MAX_PATH_QUEUE" ]; then
  echo "FAIL: path queue max ${queue_max} > ${PERF_MAX_PATH_QUEUE}"
  fail=1
fi
exit $fail

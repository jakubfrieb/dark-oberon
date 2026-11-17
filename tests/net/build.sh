#!/usr/bin/env bash
# Builds the GUI client and the headless server of the working tree into OUT (for tests/net/run.sh).
#
# Usage: tests/net/build.sh OUT
# Env:   NET_NO_NODELAY=1  drop the TCP_NODELAY calls (baseline B0: differs from B1 only in Nagle)
#
# The source is copied to a temp dir, so the .o files of the working tree stay intact (the client
# and the server share .o names). OUT/BUILD_INFO.txt records the commit and the binary hashes.
set -euo pipefail
REPO=$(cd "$(dirname "$0")/../.." && pwd)
OUT=${1:?usage: build.sh OUT}
mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

mkdir -p "$TMP/src"
rsync -a --exclude '*.o' --exclude '.compile_flags' "$REPO/src/" "$TMP/src/"
cp "$REPO/VERSION" "$TMP/VERSION"   # src/Makefile builds build_info.h from ../VERSION
if [ "${NET_NO_NODELAY:-0}" = 1 ]; then
  sed -i '/^ *EnableNoDelay (.*fd);$/d' "$TMP/src/donet.cpp"
  [ "$(grep -c 'EnableNoDelay (' "$TMP/src/donet.cpp")" = 1 ] || { echo "NET_NO_NODELAY: calls not found"; exit 2; }
fi

make -C "$TMP/src" server -j"$(nproc)" >"$TMP/build-server.log" 2>&1 || { tail -30 "$TMP/build-server.log"; exit 2; }
rm -f "$OUT/dark-oberon-server" "$OUT/dark-oberon"   # may be running from an earlier measurement
cp "$TMP/dark-oberon-server" "$OUT/"
make -C "$TMP/src" clean >/dev/null
make -C "$TMP/src" -j"$(nproc)" >"$TMP/build-gui.log" 2>&1 || { tail -30 "$TMP/build-gui.log"; exit 2; }
cp "$TMP/dark-oberon" "$OUT/"

{
  echo "commit: $(git -C "$REPO" rev-parse HEAD)$(git -C "$REPO" diff --quiet HEAD -- src || echo ' (+ uncommitted src changes)')"
  echo "version: $(cat "$REPO/VERSION")"
  echo "tcp_nodelay: $([ "${NET_NO_NODELAY:-0}" = 1 ] && echo off || echo on)"
  echo "built: $(date -Iseconds)"
  (cd "$OUT" && sha256sum dark-oberon dark-oberon-server)
} >"$OUT/BUILD_INFO.txt"
cat "$OUT/BUILD_INFO.txt"

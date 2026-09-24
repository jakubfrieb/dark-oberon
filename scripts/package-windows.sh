#!/usr/bin/env bash
# Packs dark-oberon.exe + runtime DLLs + game data (git-tracked files only)
# into dist/dark-oberon-<version>-win64.zip. Called by `make windows`.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VER="$(cat "$ROOT/VERSION")"
NAME="dark-oberon-$VER-win64"
STAGE="$ROOT/dist/$NAME"
DEPS="$ROOT/third_party/win64"

cd "$ROOT"
rm -rf "$STAGE" "$ROOT/dist/$NAME.zip"
mkdir -p "$STAGE/logs"

cp dark-oberon.exe "$STAGE/"
cp "$DEPS/bin/SDL2.dll" "$DEPS/bin/SDL2_mixer.dll" "$STAGE/"
# Only tracked data - skips *.dat-unpacked working copies and local junk.
git ls-files -z dat maps races schemes | xargs -0 cp --parents -t "$STAGE"
cp README.md CHANGELOG.md "$STAGE/"
[ -f docs/gpl.txt ] && cp docs/gpl.txt "$STAGE/"

(cd "$ROOT/dist" && zip -qr "$NAME.zip" "$NAME")
echo "Created dist/$NAME.zip ($(du -h "$ROOT/dist/$NAME.zip" | cut -f1))"

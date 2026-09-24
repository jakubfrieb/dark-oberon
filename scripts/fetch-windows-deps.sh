#!/usr/bin/env bash
# Downloads prebuilt SDL2 + SDL2_mixer MinGW development packages into
# third_party/win64 (no root needed). Used by `make windows`.
set -euo pipefail

SDL2_VER=2.32.10
SDL2_MIXER_VER=2.8.1

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$ROOT/third_party/win64"
DL="$ROOT/third_party/download"
mkdir -p "$DEST" "$DL"

fetch() {  # url file
  [ -f "$DL/$2" ] || curl -fL --retry 3 -o "$DL/$2" "$1"
  tar -xzf "$DL/$2" -C "$DL"
}

fetch "https://github.com/libsdl-org/SDL/releases/download/release-$SDL2_VER/SDL2-devel-$SDL2_VER-mingw.tar.gz" \
      "SDL2-devel-$SDL2_VER-mingw.tar.gz"
fetch "https://github.com/libsdl-org/SDL_mixer/releases/download/release-$SDL2_MIXER_VER/SDL2_mixer-devel-$SDL2_MIXER_VER-mingw.tar.gz" \
      "SDL2_mixer-devel-$SDL2_MIXER_VER-mingw.tar.gz"

for d in "$DL/SDL2-$SDL2_VER/x86_64-w64-mingw32" "$DL/SDL2_mixer-$SDL2_MIXER_VER/x86_64-w64-mingw32"; do
  cp -r "$d"/. "$DEST"/
done

echo "Installed into $DEST:"
ls "$DEST/bin"

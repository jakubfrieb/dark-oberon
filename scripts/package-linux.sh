#!/usr/bin/env bash
# Builds the Linux client in an Ubuntu 22.04 container (glibc 2.34+, so the binary
# runs on most current distros, unlike one built on a rolling release) and packs
# it with the game data into dist/dark-oberon-<version>-linux-x86_64.tar.gz.
# Called by `make linux-dist`. Needs Docker.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VER="$(cat "$ROOT/VERSION")"
NAME="dark-oberon-$VER-linux-x86_64"
IMAGE="${LINUX_BUILD_IMAGE:-ubuntu:22.04}"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

cd "$ROOT"
make -C src build_info.h >/dev/null   # git hash is not available inside the container
mkdir -p dist
rm -f "dist/$NAME.tar.gz"

# Sources (incl. uncommitted changes) + the generated build stamp, without local objects.
{ git ls-files -co --exclude-standard -z src Makefile VERSION
  printf 'src/build_info.h\0'; } | tar --null -T - -cf - | tar -xf - -C "$WORK"
touch "$WORK/src/build_info.h"

docker run --rm -v "$WORK:/build" -w /build "$IMAGE" bash -c '
  set -e
  export DEBIAN_FRONTEND=noninteractive
  apt-get update -qq
  apt-get install -y -qq --no-install-recommends build-essential pkg-config \
    libsdl2-dev libsdl2-mixer-dev libgl1-mesa-dev libglu1-mesa-dev >/dev/null
  make -C src SOUND=1 -j"$(nproc)" >build.log 2>&1 || { grep -E "error|Error" build.log | head -20; exit 1; }
  chown '"$(id -u):$(id -g)"' dark-oberon'

STAGE="$WORK/stage/$NAME"
mkdir -p "$STAGE"
cp "$WORK/dark-oberon" README.md CHANGELOG.md "$STAGE/"
git ls-files -z dat maps races schemes | xargs -0 cp --parents -t "$STAGE"
cat > "$STAGE/README-LINUX.txt" <<'EOF'
Dark Oberon for Linux (x86_64)

1. Install the runtime libraries once:
     Debian/Ubuntu: sudo apt install libsdl2-2.0-0 libsdl2-mixer-2.0-0 libglu1-mesa
     Arch/Manjaro:  sudo pacman -S sdl2 sdl2_mixer glu
     Fedora:        sudo dnf install SDL2 SDL2_mixer mesa-libGLU
2. Unpack anywhere, then run the game from its folder:
     cd dark-oberon-*/ && ./dark-oberon

Built on Ubuntu 22.04: needs glibc 2.34 or newer (Debian 12, Ubuntu 22.04, Fedora 35 and later).
Settings and logs are kept in ~/.dark-oberon/.
EOF

tar -czf "dist/$NAME.tar.gz" -C "$WORK/stage" "$NAME"
echo "Created dist/$NAME.tar.gz ($(du -h "dist/$NAME.tar.gz" | cut -f1))"

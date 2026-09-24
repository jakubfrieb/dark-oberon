#!/usr/bin/env bash
# Builds Dark Oberon natively on a Haiku machine over SSH and fetches
# dist/dark-oberon-<version>-haiku-x86_64.zip. Called by `make haiku`.
#
#   HAIKU_HOST=haiku scripts/package-haiku.sh   # ssh alias/host of the Haiku box
#
# The Haiku box needs (once): pkgman install libsdl2_devel sdl2_mixer_devel glu_devel
set -euo pipefail

HOST="${HAIKU_HOST:-haiku}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VER="$(cat "$ROOT/VERSION")"
NAME="dark-oberon-$VER-haiku-x86_64"
REMOTE="/boot/home/dark-oberon-build"

cd "$ROOT"
make -C src build_info.h >/dev/null   # git hash is not available on the remote side
mkdir -p dist
rm -f "dist/$NAME.zip"

# Sources + tracked data (incl. uncommitted changes) + the generated build stamp.
{ git ls-files -co --exclude-standard -z src dat maps races schemes Makefile VERSION README.md CHANGELOG.md
  printf 'src/build_info.h\0'; } \
  | tar --null -T - -czf - \
  | ssh "$HOST" "rm -rf $REMOTE && mkdir -p $REMOTE && cd $REMOTE && tar xzf - 2>/dev/null
      set -e
      touch src/build_info.h
      make -C src SOUND=1 -j4 >build.log 2>&1 || { tail -30 build.log; exit 1; }
      mkdir -p stage/$NAME/logs
      cp dark-oberon README.md CHANGELOG.md stage/$NAME/
      cp -r dat maps races schemes stage/$NAME/
      cat > stage/$NAME/README-HAIKU.txt <<'EOF'
Dark Oberon for Haiku (R1/beta6, x86_64)

1. Install the runtime libraries once (Terminal or HaikuDepot):
     pkgman install libsdl2 sdl2_mixer glu
2. Unzip anywhere and double-click dark-oberon (or run ./dark-oberon
   from this folder).

Settings and logs are kept in ~/.dark-oberon/.
EOF
      cd stage && zip -qr ../$NAME.zip $NAME"

scp -q "$HOST:$REMOTE/$NAME.zip" "dist/$NAME.zip"
ssh "$HOST" "rm -rf $REMOTE"
echo "Created dist/$NAME.zip ($(du -h "dist/$NAME.zip" | cut -f1))"

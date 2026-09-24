#!/usr/bin/env bash
# Installs the system packages needed to cross-compile the Windows build
# (dark-oberon.exe) on Arch/Manjaro. Run with sudo:
#
#   sudo scripts/setup-windows-toolchain.sh
#
# SDL2/SDL2_mixer for Windows are NOT installed here - they are downloaded
# without root into third_party/win64 by scripts/fetch-windows-deps.sh.
set -euo pipefail

if [ "$(id -u)" -ne 0 ]; then
  echo "Run as root: sudo $0" >&2
  exit 1
fi

# mingw-w64-gcc pulls in binutils, headers and crt for x86_64-w64-mingw32.
# wine is used for smoke-testing the .exe locally.
pacman -S --needed --noconfirm mingw-w64-gcc wine zip

x86_64-w64-mingw32-g++ --version | head -n1
wine --version

#!/usr/bin/env bash
# Usage: pack_dat.sh unpacked_dir output.dat
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
DIR="${1:?usage: $0 unpacked_dir output.dat}"
OUT="${2:?usage: $0 unpacked_dir output.dat}"
exec python3 "$HERE/do_dat_tool.py" pack "$DIR" -o "$OUT"

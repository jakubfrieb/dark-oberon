#!/usr/bin/env bash
# Usage: unpack_dat.sh path/to/file.dat [output_dir]
# Default output: same directory as file.dat → <file.dat>-unpacked
#   e.g. schemes/plastic.dat → schemes/plastic.dat-unpacked
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
DAT="${1:?usage: $0 file.dat [out_dir]}"
DAT_DIR="$(cd "$(dirname "$DAT")" && pwd)"
DAT_BASE="$(basename "$DAT")"
OUT="${2:-$DAT_DIR/${DAT_BASE}-unpacked}"
exec python3 "$HERE/do_dat_tool.py" unpack "$DAT" -o "$OUT"

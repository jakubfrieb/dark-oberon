#!/usr/bin/env bash
# Usage: unpack_dat.sh path/to/file.dat [output_dir]
# Default output: ./<basename>.unpacked
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
DAT="${1:?usage: $0 file.dat [out_dir]}"
OUT="${2:-$(basename "$DAT" .dat).unpacked}"
exec python3 "$HERE/do_dat_tool.py" unpack "$DAT" -o "$OUT"

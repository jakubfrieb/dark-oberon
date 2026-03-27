#!/usr/bin/env bash
# Unpack every *.dat in a directory (default: repo/dat/).
# Each foo.dat → foo.dat-unpacked in the same directory.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../../.." && pwd)"
DAT_DIR="${1:-$ROOT/dat}"

if [[ ! -d "$DAT_DIR" ]]; then
  echo "Not a directory: $DAT_DIR" >&2
  exit 1
fi

shopt -s nullglob
for f in "$DAT_DIR"/*.dat; do
  dir="$(cd "$(dirname "$f")" && pwd)"
  base="$(basename "$f")"
  out="$dir/${base}-unpacked"
  echo "Unpacking $f -> $out"
  python3 "$HERE/do_dat_tool.py" unpack "$f" -o "$out"
done
echo "Done."

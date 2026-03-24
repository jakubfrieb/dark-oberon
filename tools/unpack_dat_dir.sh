#!/usr/bin/env bash
# Unpack every *.dat in dat/ into dat_unpacked/<basename>/
# Same binary format as races/schemes — uses do_dat_tool.py.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
DAT_DIR="${1:-$ROOT/dat}"
OUT_ROOT="${2:-$ROOT/dat_unpacked}"

if [[ ! -d "$DAT_DIR" ]]; then
  echo "Not a directory: $DAT_DIR" >&2
  exit 1
fi

shopt -s nullglob
for f in "$DAT_DIR"/*.dat; do
  base="$(basename "$f" .dat)"
  out="$OUT_ROOT/$base"
  echo "Unpacking $f -> $out"
  python3 "$HERE/do_dat_tool.py" unpack "$f" -o "$out"
done
echo "Done. Output under $OUT_ROOT"

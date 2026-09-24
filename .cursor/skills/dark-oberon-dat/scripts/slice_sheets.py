#!/usr/bin/env python3
"""
Slice edited sprite sheets back into individual TGA textures.

Usage:
    python3 slice_sheets.py <sheets_dir> <unpacked_dir>

Reads *_layout.json + corresponding PNG files from <sheets_dir>.
Overwrites TGA files in <unpacked_dir>/textures/ based on the exact pixel
coordinates stored in each layout JSON.

Only cells with status "present" are sliced.  Alias / shared / missing rows
are skipped (the original TGA files in unpacked_dir remain untouched).
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    sys.exit("Pillow required: pip install Pillow")


def slice_sheets(sheets_dir: Path, unpacked_dir: Path) -> None:
    layouts = sorted(sheets_dir.glob("*_layout.json"))
    layouts = [p for p in layouts if not p.name.startswith("_")]
    if not layouts:
        sys.exit(f"No layout JSON files found in {sheets_dir}")

    total = 0
    for lp in layouts:
        layout = json.loads(lp.read_text("utf-8"))
        eid = layout["entity_id"]
        png_path = sheets_dir / f"{eid}.png"
        if not png_path.exists():
            print(f"  SKIP {eid}: no PNG found", file=sys.stderr)
            continue

        sheet = Image.open(png_path).convert("RGBA")
        count = 0

        for row in layout["rows"]:
            if row["status"] != "present":
                continue
            for cell in row.get("cells", []):
                if cell.get("status") != "present":
                    continue

                x = cell["x"] + cell["offset_x"]
                y = cell["y"] + cell["offset_y"]
                w, h = cell["w"], cell["h"]
                cropped = sheet.crop((x, y, x + w, y + h))

                out_path = unpacked_dir / cell["file"]
                out_path.parent.mkdir(parents=True, exist_ok=True)
                cropped.save(str(out_path), "TGA")
                count += 1

        total += count
        print(f"  {eid}: {count} textures sliced")

    print(f"Done. {total} TGAs written to {unpacked_dir}")


def main():
    ap = argparse.ArgumentParser(description="Slice sprite sheets back to TGAs")
    ap.add_argument("sheets_dir", type=Path,
                    help="Directory with edited PNGs + layout JSONs")
    ap.add_argument("unpacked_dir", type=Path,
                    help="Unpacked .dat directory to write TGAs into")
    args = ap.parse_args()
    slice_sheets(args.sheets_dir, args.unpacked_dir)


if __name__ == "__main__":
    main()

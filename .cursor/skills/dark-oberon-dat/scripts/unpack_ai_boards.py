#!/usr/bin/env python3
"""
Unpack edited AI boards back into entity sprite sheets.

Reads the board manifest and edited board PNGs, extracts each slot,
downscales to original cell dimensions, and pastes into entity sheets.

Usage:
    python3 unpack_ai_boards.py <boards_dir> [--edited-dir <dir>] [--sheets-dir <dir>]

If --edited-dir is omitted, looks for edited boards in <boards_dir>/edited/.
If --sheets-dir is omitted, reads it from the manifest.
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


def unpack_boards(boards_dir: Path, edited_dir: Path, sheets_dir: Path) -> None:
    manifest_path = boards_dir / "_boards_manifest.json"
    if not manifest_path.exists():
        sys.exit(f"Manifest not found: {manifest_path}")

    manifest = json.loads(manifest_path.read_text("utf-8"))

    if sheets_dir is None:
        sheets_dir = Path(manifest["sheets_dir"])
    if not sheets_dir.exists():
        sys.exit(f"Sheets directory not found: {sheets_dir}")

    loaded_sheets: dict[str, Image.Image] = {}
    modified: set[str] = set()
    total_slots = 0

    for board in manifest["boards"]:
        board_id = board["board_id"]
        board_file = board["board_file"]
        entity_id = board["entity_id"]

        edited_path = edited_dir / board_file
        if not edited_path.exists():
            print(f"  SKIP {board_id}: no edited board at {edited_path}",
                  file=sys.stderr)
            continue

        edited_img = Image.open(edited_path).convert("RGBA")
        expected = board["board_size"]
        if edited_img.size != (expected, expected):
            print(f"  WARN: {board_id} size {edited_img.size} != "
                  f"expected {expected}x{expected}", file=sys.stderr)

        if entity_id not in loaded_sheets:
            sheet_path = sheets_dir / f"{entity_id}.png"
            if not sheet_path.exists():
                print(f"  SKIP {entity_id}: sheet not found", file=sys.stderr)
                continue
            loaded_sheets[entity_id] = Image.open(sheet_path).convert("RGBA")

        sheet = loaded_sheets[entity_id]

        for slot in board["slots"]:
            bx, by, bw, bh = slot["dst"]
            sx, sy, sw, sh = slot["src"]

            tile = edited_img.crop((bx, by, bx + bw, by + bh))
            restored = tile.resize((sw, sh), Image.LANCZOS)
            sheet.paste(restored, (sx, sy))
            total_slots += 1

        modified.add(entity_id)

    for eid in modified:
        out_path = sheets_dir / f"{eid}.png"
        loaded_sheets[eid].save(out_path, "PNG")
        print(f"  {eid}: updated")

    print(f"\nDone. {total_slots} slots restored across {len(modified)} entities.")


def main():
    ap = argparse.ArgumentParser(
        description="Unpack edited AI boards back into entity sheets",
    )
    ap.add_argument("boards_dir", type=Path,
                    help="Directory with _boards_manifest.json + original boards")
    ap.add_argument("--edited-dir", type=Path, default=None,
                    help="Directory with edited board PNGs "
                         "(default: <boards_dir>/edited)")
    ap.add_argument("--sheets-dir", type=Path, default=None,
                    help="Entity sheets directory (default: from manifest)")
    args = ap.parse_args()

    edited = args.edited_dir or (args.boards_dir / "edited")
    unpack_boards(args.boards_dir.resolve(), edited.resolve(),
                  args.sheets_dir.resolve() if args.sheets_dir else None)


if __name__ == "__main__":
    main()

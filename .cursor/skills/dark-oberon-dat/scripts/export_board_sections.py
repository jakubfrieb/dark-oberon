#!/usr/bin/env python3
"""
Export per-frame PNGs from packed AI boards at board resolution (dst rects).

Reads boards_dir/_boards_manifest.json and each board PNG; crops each slot's
dst rectangle, subdividing by hcount x vcount the same way as sheet slicing.

Writes to <sheets_dir>/<entity_id>/<out_subdir>/ with names
  {animation}_{NN}.png
matching the ordering of export from the raw sheet (section/), but pixels
match what appears on the 1024x1024 board (LANCZOS upscale from pack).

Usage:
    python3 export_board_sections.py <boards_dir> <entity_id>
    python3 export_board_sections.py <boards_dir> <entity_id> --out-subdir section_board
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


def grid_rects(bx: int, by: int, bw: int, bh: int, hc: int, vc: int) -> list[tuple[int, int, int, int]]:
    """Integer-split dst into hc*vc tiles, row-major (fi = row * hc + col)."""
    hc = max(1, hc)
    vc = max(1, vc)
    xs = [bx + (i * bw) // hc for i in range(hc + 1)]
    ys = [by + (i * bh) // vc for i in range(vc + 1)]
    out: list[tuple[int, int, int, int]] = []
    for fi in range(hc * vc):
        col = fi % hc
        row_i = fi // hc
        x0, x1 = xs[col], xs[col + 1]
        y0, y1 = ys[row_i], ys[row_i + 1]
        out.append((x0, y0, x1 - x0, y1 - y0))
    return out


def export_entity(boards_dir: Path, entity_id: str, out_subdir: str) -> None:
    manifest_path = boards_dir / "_boards_manifest.json"
    if not manifest_path.exists():
        sys.exit(f"Missing {manifest_path}")

    manifest = json.loads(manifest_path.read_text("utf-8"))
    sheets_dir = Path(manifest["sheets_dir"])
    out_dir = sheets_dir / entity_id / out_subdir
    out_dir.mkdir(parents=True, exist_ok=True)

    per_anim: dict[str, int] = {}
    total = 0

    for board in manifest["boards"]:
        if board.get("entity_id") != entity_id:
            continue
        anim = board.get("animation") or "unknown"
        bf = boards_dir / board["board_file"]
        if not bf.exists():
            print(f"  SKIP {board.get('board_id')}: missing {bf.name}", file=sys.stderr)
            continue

        img = Image.open(bf).convert("RGBA")
        expected = int(board.get("board_size", manifest.get("board_size", 1024)))
        if img.size != (expected, expected):
            print(
                f"  WARN {bf.name}: size {img.size} != {expected}x{expected}",
                file=sys.stderr,
            )

        for slot in board.get("slots", []):
            bx, by, bw, bh = slot["dst"]
            hc = int(slot.get("hcount", 1))
            vc = int(slot.get("vcount", 1))
            for rx, ry, rw, rh in grid_rects(bx, by, bw, bh, hc, vc):
                tile = img.crop((rx, ry, rx + rw, ry + rh))
                per_anim[anim] = per_anim.get(anim, 0) + 1
                n = per_anim[anim]
                outp = out_dir / f"{anim}_{n:02d}.png"
                tile.save(outp, "PNG", compress_level=3)
                total += 1

        print(f"  {board.get('board_id')}: slots -> {anim}")

    print(f"Done. {total} PNGs -> {out_dir}")


def main() -> None:
    ap = argparse.ArgumentParser(description="Export board-scale frame PNGs from AI boards")
    ap.add_argument("boards_dir", type=Path, help="Directory with _boards_manifest.json + PNGs")
    ap.add_argument("entity_id", help="Entity id (e.g. footman)")
    ap.add_argument(
        "--out-subdir",
        default="section_board",
        help="Subfolder under sheets_dir/<entity_id>/ (default: section_board)",
    )
    args = ap.parse_args()
    export_entity(args.boards_dir.resolve(), args.entity_id, args.out_subdir)


if __name__ == "__main__":
    main()

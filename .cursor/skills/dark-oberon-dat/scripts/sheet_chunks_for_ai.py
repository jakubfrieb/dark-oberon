#!/usr/bin/env python3
"""
Split large sprite sheets into smaller crops for AI image tools, then merge back.

Models often cap input size (e.g. 1024–2048 px). Sheets are wide (many animation
frames per row). Cuts are placed only on layout-safe coordinates (guides / cell
edges) so pasted chunks align with the original footman.png dimensions.

Usage:
  python3 sheet_chunks_for_ai.py export <sheets_dir> [--max-w W] [--max-h H] [--out DIR]
  python3 sheet_chunks_for_ai.py merge <sheets_dir> [--chunks-dir DIR]

export  — reads *_layout.json + *.png, writes ai_chunks/<entity>/* + *_chunks.json
merge   — pastes edited crops onto copies of sheets (overwrites *.png in sheets_dir)
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


def _row_y_bounds(row: dict, guide_w: int, sh: int) -> tuple[int, int]:
    y = row["y"]
    h = row["height"]
    y_top = max(0, y - guide_w)
    y_bot = min(sh, y + h + guide_w)
    return y_top, y_bot


def _x_split_points_present(row: dict, sw: int, guide_w: int) -> list[int]:
    pts: set[int] = {0, sw}
    lm = row.get("label_margin")
    if lm is None:
        lm = 120
    pts.add(min(sw, max(0, lm - guide_w)))
    pts.add(min(sw, lm))
    for c in row.get("cells") or []:
        x = c["x"]
        cw = c["cell_w"]
        pts.add(min(sw, x))
        pts.add(min(sw, x + cw))
        pts.add(min(sw, x + cw + guide_w))
    return sorted(pts)


def _x_split_points_fallback(sw: int, max_w: int) -> list[int]:
    """Arbitrary vertical cuts every max_w (for rows without cell grid)."""
    pts = [0]
    x = 0
    while x < sw:
        x = min(sw, x + max_w)
        pts.append(x)
    return pts


def _next_chunk_end(xa: int, max_w: int, points: list[int]) -> int:
    """Largest p in points with xa < p <= xa + max_w; else smallest p > xa."""
    best = None
    for p in points:
        if xa < p <= xa + max_w:
            if best is None or p > best:
                best = p
    if best is not None:
        return best
    for p in points:
        if p > xa:
            return p
    return xa + max_w


def horizontal_chunks_for_row(
    sw: int, y_top: int, y_bot: int, max_w: int, row: dict, guide_w: int,
    label_margin: int,
) -> list[tuple[int, int, int, int]]:
    """List of (x0, y_top, x1, y_bot) inclusive-exclusive."""
    chunks: list[tuple[int, int, int, int]] = []

    if row.get("status") == "present" and row.get("cells"):
        # Right edge of the last cell + its trailing vertical guide (TGAs only
        # exist left of this). The sheet may stay wider for other rows — to the
        # right there is empty padding; we may step by max_w there without
        # cutting sprites.
        cell_right = max(
            min(sw, c["x"] + c["cell_w"] + guide_w) for c in row["cells"]
        )
        cell_pts = _x_split_points_present(
            {**row, "label_margin": label_margin}, sw, guide_w
        )
        xa = 0
        while xa < cell_right:
            xb = _next_chunk_end(xa, max_w, cell_pts)
            xb = min(xb, cell_right)
            if xb <= xa:
                xb = min(sw, xa + max_w, cell_right)
            if xb <= xa:
                break
            chunks.append((xa, y_top, xb, y_bot))
            xa = xb
        while xa < sw:
            xb = min(xa + max_w, sw)
            chunks.append((xa, y_top, xb, y_bot))
            xa = xb
        return chunks

    xs = _x_split_points_fallback(sw, max_w)
    xa = 0
    while xa < sw:
        xb = _next_chunk_end(xa, max_w, xs)
        if xb <= xa:
            xb = min(sw, xa + max_w)
        if xb <= xa:
            break
        chunks.append((xa, y_top, xb, y_bot))
        xa = xb
    return chunks


def _warn_oversized_cells(row: dict, max_w: int) -> None:
    if row.get("status") != "present":
        return
    cw = row.get("cell_w") or 0
    if cw > max_w:
        gn = row.get("group_name", "?")
        print(
            f"  WARN: row '{gn}' has cell_w={cw} > max_w={max_w}; "
            "one crop may exceed model limits.",
            file=sys.stderr,
        )


def cmd_export(sheets_dir: Path, out_root: Path, max_w: int, max_h: int) -> None:
    layouts = sorted(
        p for p in sheets_dir.glob("*_layout.json")
        if not p.name.startswith("_")
    )
    if not layouts:
        sys.exit(f"No *_layout.json in {sheets_dir}")

    out_root.mkdir(parents=True, exist_ok=True)
    for lp in layouts:
        layout = json.loads(lp.read_text(encoding="utf-8"))
        eid = layout["entity_id"]
        sw, sh = layout["sheet_size"]
        guide_w = layout.get("guide_width", 1)
        label_margin = layout.get("label_margin", 120)

        png_path = sheets_dir / f"{eid}.png"
        if not png_path.exists():
            print(f"  SKIP {eid}: missing {png_path.name}", file=sys.stderr)
            continue

        sheet = Image.open(png_path).convert("RGBA")
        if sheet.size != (sw, sh):
            print(
                f"  WARN: {eid} PNG size {sheet.size} != layout {sw}x{sh}",
                file=sys.stderr,
            )

        ent_dir = out_root / eid
        ent_dir.mkdir(parents=True, exist_ok=True)

        chunks_meta: list[dict] = []
        chunk_idx = 0

        for ri, row in enumerate(layout["rows"]):
            y_top, y_bot = _row_y_bounds(row, guide_w, sh)
            if y_bot <= y_top:
                continue
            row_h = y_bot - y_top
            if row_h > max_h:
                print(
                    f"  WARN: {eid} row {ri} ({row.get('tg_key')}) strip height "
                    f"{row_h} > max_h={max_h}; exporting anyway.",
                    file=sys.stderr,
                )

            row_for_pts = {**row, "label_margin": label_margin}
            if row.get("status") == "present":
                _warn_oversized_cells(row, max_w)

            hrects = horizontal_chunks_for_row(
                sw, y_top, y_bot, max_w, row_for_pts, guide_w, label_margin
            )
            for x0, yt, x1, yb in hrects:
                short = row.get("tg_key", "row").replace("tg_", "").replace("_id", "")
                fname = f"{eid}_r{ri:02d}_{short}_{chunk_idx:03d}.png"

                crop = sheet.crop((x0, yt, x1, yb))
                fp = ent_dir / fname
                crop.save(fp, "PNG")
                chunks_meta.append(
                    {
                        "file": fname,
                        "rect": [x0, yt, x1, yb],
                        "row_index": ri,
                        "tg_key": row.get("tg_key", ""),
                    }
                )
                chunk_idx += 1

        manifest = {
            "entity_id": eid,
            "sheet_size": [sw, sh],
            "source_sheet": str(png_path.resolve()),
            "layout_file": str(lp.resolve()),
            "max_w": max_w,
            "max_h": max_h,
            "chunks": chunks_meta,
        }
        (ent_dir / f"{eid}_chunks.json").write_text(
            json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
        )
        print(f"  {eid}: {len(chunks_meta)} chunks -> {ent_dir}/")

    print(f"Done. Chunks under {out_root}/")


def cmd_merge(sheets_dir: Path, chunks_root: Path) -> None:
    manifests = sorted(chunks_root.glob("*/*_chunks.json"))
    if not manifests:
        sys.exit(f"No *_chunks.json under {chunks_root}/<entity>/")

    for mp in manifests:
        data = json.loads(mp.read_text(encoding="utf-8"))
        eid = data["entity_id"]
        sw, sh = data["sheet_size"]
        ent_dir = mp.parent
        png_path = sheets_dir / f"{eid}.png"
        if not png_path.exists():
            print(f"  SKIP {eid}: {png_path} missing", file=sys.stderr)
            continue

        base = Image.open(png_path).convert("RGBA")
        if base.size != (sw, sh):
            print(
                f"  WARN: {eid} base {base.size} != manifest {sw}x{sh}",
                file=sys.stderr,
            )

        for ch in data["chunks"]:
            cf = ent_dir / ch["file"]
            if not cf.exists():
                print(f"  WARN: missing chunk {cf}, leaving base pixels", file=sys.stderr)
                continue
            x0, y0, x1, y1 = ch["rect"]
            w, h = x1 - x0, y1 - y0
            tile = Image.open(cf).convert("RGBA")
            if tile.size != (w, h):
                sys.exit(
                    f"{cf}: size {tile.size} != expected {(w, h)} "
                    "(do not resize chunks; re-export if layout changed)"
                )
            base.paste(tile, (x0, y0))

        base.save(png_path, "PNG")
        print(f"  merged {eid} -> {png_path}")

    print("Done.")


def main() -> None:
    ap = argparse.ArgumentParser(description="AI-friendly sprite sheet chunks")
    sub = ap.add_subparsers(dest="cmd", required=True)

    e = sub.add_parser("export", help="Crop sheets into smaller PNGs")
    e.add_argument("sheets_dir", type=Path, help="Directory with entity PNGs + *_layout.json")
    e.add_argument(
        "--out",
        type=Path,
        default=None,
        help="Output root (default: <sheets_dir>/ai_chunks)",
    )
    e.add_argument("--max-w", type=int, default=1536, help="Max crop width (default 1536)")
    e.add_argument("--max-h", type=int, default=1536, help="Max crop height (default 1536)")

    m = sub.add_parser("merge", help="Paste chunks back into sheets")
    m.add_argument("sheets_dir", type=Path)
    m.add_argument(
        "--chunks-dir",
        type=Path,
        default=None,
        help="Chunk root (default: <sheets_dir>/ai_chunks)",
    )

    args = ap.parse_args()
    if args.cmd == "export":
        out = args.out or (args.sheets_dir / "ai_chunks")
        cmd_export(args.sheets_dir.resolve(), out.resolve(), args.max_w, args.max_h)
    else:
        cdir = args.chunks_dir or (args.sheets_dir / "ai_chunks")
        cmd_merge(args.sheets_dir.resolve(), cdir.resolve())


if __name__ == "__main__":
    main()

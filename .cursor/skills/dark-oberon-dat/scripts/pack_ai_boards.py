#!/usr/bin/env python3
"""
Pack entity sprite sheets into 1024x1024 pose-filled AI boards.

Each "present" animation group becomes one or more boards where original
sprites are arranged in a grid and scaled up to maximize canvas usage.
A board manifest records the exact mapping so unpack_ai_boards.py can
restore edited boards back into the entity sheets.

Usage:
    python3 pack_ai_boards.py <sheets_dir> [--out <boards_dir>] \
        [--board-size 1024] [--padding 4] [--max-scale 8]
"""

from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    sys.exit("Pillow required: pip install Pillow")

BOARD_SIZE = 1024
PADDING = 4
MAX_SCALE = 8.0


def find_optimal_grid(
    n: int, cell_w: int, cell_h: int,
    board_size: int = BOARD_SIZE, padding: int = PADDING,
) -> tuple[int, int, float]:
    """Find cols x rows arrangement that maximizes scale factor for n cells."""
    best: tuple[int, int, float] | None = None
    for cols in range(1, n + 1):
        rows = math.ceil(n / cols)
        w_avail = (board_size - (cols + 1) * padding) / cols
        h_avail = (board_size - (rows + 1) * padding) / rows
        if w_avail <= 0 or h_avail <= 0:
            continue
        scale = min(w_avail / cell_w, h_avail / cell_h)
        if best is None or scale > best[2]:
            best = (cols, rows, scale)
    if best is None:
        return (1, n, min(
            (board_size - 2 * padding) / cell_w,
            (board_size - 2 * padding) / (n * cell_h),
        ))
    return best


def _split_into_boards(
    n: int, cell_w: int, cell_h: int,
    board_size: int, padding: int, max_scale: float,
) -> tuple[int, int, int, float]:
    """Determine cells_per_board, cols, rows, scale — splitting if needed."""
    cols, rows, scale = find_optimal_grid(n, cell_w, cell_h, board_size, padding)
    scale = min(scale, max_scale)
    if scale >= 1.0:
        return n, cols, rows, scale

    for batch in range(n - 1, 0, -1):
        c, r, s = find_optimal_grid(batch, cell_w, cell_h, board_size, padding)
        s = min(s, max_scale)
        if s >= 1.0:
            return batch, c, r, s

    return 1, 1, 1, min(
        (board_size - 2 * padding) / cell_w,
        (board_size - 2 * padding) / cell_h,
        max_scale,
    )


def pack_group(
    entity_id: str, entity_type: str, row: dict,
    sheet_img: Image.Image, direction_order: list[str],
    out_dir: Path, board_size: int, padding: int, max_scale: float,
) -> list[dict]:
    """Pack one animation group into one or more boards. Returns board dicts."""
    cells = [c for c in row.get("cells", []) if c.get("status") == "present"]
    if not cells:
        return []

    tg_key = row.get("tg_key", "")
    group_name = row.get("group_name", "")
    animation = tg_key.replace("tg_", "").replace("_id", "")
    cell_w, cell_h = row["cell_w"], row["cell_h"]

    n = len(cells)
    cpb, _, _, _ = _split_into_boards(n, cell_w, cell_h, board_size, padding, max_scale)

    boards = []
    for batch_start in range(0, n, cpb):
        batch_cells = cells[batch_start:batch_start + cpb]
        bn = len(batch_cells)
        bc, br, bs = find_optimal_grid(bn, cell_w, cell_h, board_size, padding)
        bs = min(bs, max_scale)
        scaled_w = int(cell_w * bs)
        scaled_h = int(cell_h * bs)

        board_idx = batch_start // cpb
        board_id = f"{entity_id}__{animation}__b{board_idx}"
        board_file = f"{board_id}.png"

        board_img = Image.new("RGBA", (board_size, board_size), (0, 0, 0, 0))

        slots = []
        for i, cell in enumerate(batch_cells):
            col = i % bc
            row_idx = i // bc
            bx = padding + col * (scaled_w + padding)
            by = padding + row_idx * (scaled_h + padding)

            # Crop full logical cell area from source sheet
            sx, sy = cell["x"], cell["y"]
            sprite = sheet_img.crop((sx, sy, sx + cell_w, sy + cell_h))

            scaled_sprite = sprite.resize((scaled_w, scaled_h), Image.LANCZOS)
            board_img.paste(scaled_sprite, (bx, by))

            tex_idx = cell.get("tex_index", i)
            if entity_type == "unit" and tex_idx < len(direction_order):
                label = direction_order[tex_idx]
            else:
                label = f"stage_{tex_idx}"

            slots.append({
                "idx": i,
                "tex_index": tex_idx,
                "label": label,
                "src": [sx, sy, cell_w, cell_h],
                "dst": [bx, by, scaled_w, scaled_h],
                "actual_size": [cell["w"], cell["h"]],
                "offset": [cell.get("offset_x", 0), cell.get("offset_y", 0)],
                "hcount": cell.get("hcount", 1),
                "vcount": cell.get("vcount", 1),
            })

        board_img.save(out_dir / board_file, "PNG")

        hcount = batch_cells[0].get("hcount", 1)
        vcount = batch_cells[0].get("vcount", 1)
        if hcount > 1 or vcount > 1:
            frame_info = f"{hcount}x{vcount} animation frame grid per sprite"
        else:
            frame_info = "single frame per sprite"

        boards.append({
            "board_id": board_id,
            "board_file": board_file,
            "entity_id": entity_id,
            "entity_type": entity_type,
            "group_name": group_name,
            "tg_key": tg_key,
            "animation": animation,
            "scale": round(bs, 4),
            "grid": [bc, br],
            "board_size": board_size,
            "padding": padding,
            "slots": slots,
            "prompt_hints": {
                "animation": animation,
                "labels": [s["label"] for s in slots],
                "frame_info": frame_info,
                "entity_type": entity_type,
            },
        })

    return boards


def pack_all(sheets_dir: Path, out_dir: Path,
             board_size: int, padding: int, max_scale: float) -> None:
    layouts = sorted(
        p for p in sheets_dir.glob("*_layout.json")
        if not p.name.startswith("_")
    )
    if not layouts:
        sys.exit(f"No *_layout.json in {sheets_dir}")

    out_dir.mkdir(parents=True, exist_ok=True)
    all_boards: list[dict] = []

    for lp in layouts:
        layout = json.loads(lp.read_text("utf-8"))
        eid = layout["entity_id"]
        etype = layout.get("entity_type", "unit")
        direction_order = layout.get("direction_order", [])

        png_path = sheets_dir / f"{eid}.png"
        if not png_path.exists():
            print(f"  SKIP {eid}: missing {png_path.name}", file=sys.stderr)
            continue

        sheet = Image.open(png_path).convert("RGBA")
        entity_boards = 0

        for row in layout["rows"]:
            if row.get("status") != "present":
                continue
            boards = pack_group(
                eid, etype, row, sheet, direction_order,
                out_dir, board_size, padding, max_scale,
            )
            all_boards.extend(boards)
            entity_boards += len(boards)

        if entity_boards:
            print(f"  {eid}: {entity_boards} board(s)")

    manifest = {
        "version": 1,
        "sheets_dir": str(sheets_dir.resolve()),
        "board_size": board_size,
        "padding": padding,
        "max_scale": max_scale,
        "total_boards": len(all_boards),
        "boards": all_boards,
    }
    manifest_path = out_dir / "_boards_manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", "utf-8")
    print(f"\nDone. {len(all_boards)} boards -> {out_dir}/")
    print(f"Manifest: {manifest_path}")


def main():
    ap = argparse.ArgumentParser(
        description="Pack sprite sheets into 1024x1024 AI boards",
    )
    ap.add_argument("sheets_dir", type=Path,
                    help="Directory with entity PNGs + *_layout.json")
    ap.add_argument("--out", type=Path, default=None,
                    help="Output directory (default: <sheets_dir>/../boards)")
    ap.add_argument("--board-size", type=int, default=BOARD_SIZE,
                    help=f"Board canvas size (default {BOARD_SIZE})")
    ap.add_argument("--padding", type=int, default=PADDING,
                    help=f"Padding between slots (default {PADDING})")
    ap.add_argument("--max-scale", type=float, default=MAX_SCALE,
                    help=f"Maximum upscale factor (default {MAX_SCALE})")
    args = ap.parse_args()

    out = args.out or (args.sheets_dir.parent / "boards")
    pack_all(args.sheets_dir.resolve(), out.resolve(),
             args.board_size, args.padding, args.max_scale)


if __name__ == "__main__":
    main()

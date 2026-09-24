#!/usr/bin/env python3
"""
Compose per-entity sprite sheets from unpacked Dark Oberon .dat textures.

Usage:
    python3 compose_sheets.py <unpacked_dir> <rac_file> <output_dir>

Reads manifest.json + TGA files from <unpacked_dir> and entity definitions
from <rac_file>.  Produces one PNG sprite sheet + layout JSON per entity
(unit / building) in <output_dir>.

The layout JSON records exact pixel coordinates for every cell so that
slice_sheets.py can deterministically cut the (possibly edited) PNG back
into individual TGA files.
"""

from __future__ import annotations

import argparse
import json
import re
import struct
import sys
from pathlib import Path
from typing import Any

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    sys.exit("Pillow required: pip install Pillow")

GUIDE_COLOR = (255, 0, 255, 255)
LABEL_BG = (30, 30, 30, 220)
LABEL_FG = (240, 240, 240, 255)
ALIAS_FG = (160, 160, 160, 200)
LABEL_MARGIN = 120
GUIDE_W = 1
PLACEHOLDER_H = 18

UNIT_TG_KEYS = [
    "tg_picture_id", "tg_stay_id", "tg_move_id", "tg_attack_id",
    "tg_rotate_id", "tg_anchor_id", "tg_land_id",
    "tg_dying_id", "tg_zombie_id", "tg_projectile_id", "tg_burning_id",
]
WORKER_EXTRA = ["tg_mine_id", "tg_repair_id"]
BUILDING_TG_KEYS = [
    "tg_picture_id", "tg_stay_id", "tg_build_id",
    "tg_dying_id", "tg_zombie_id", "tg_projectile_id", "tg_burning_id",
]

DIR_LABELS = ["S", "SW", "W", "NW", "N", "NE", "E", "SE"]


# ---------------------------------------------------------------------------
# .rac parser (extracts entities + their tg_* references)
# ---------------------------------------------------------------------------

def parse_rac(path: Path) -> dict[str, Any]:
    text = path.read_text(encoding="latin-1")
    result: dict[str, Any] = {"units": [], "buildings": []}
    for m in re.finditer(r"<Unit\s+(\d+)>(.*?)</Unit\s+\1>", text, re.DOTALL):
        result["units"].append(_parse_block(m.group(2)))
    for m in re.finditer(r"<Building\s+(\d+)>(.*?)</Building\s+\1>", text, re.DOTALL):
        result["buildings"].append(_parse_block(m.group(2)))
    return result


def _parse_block(block: str) -> dict[str, Any]:
    ent: dict[str, Any] = {"tg": {}}
    for line in block.split("\n"):
        s = line.strip()
        if not s or s.startswith("#") or s.startswith("<"):
            continue
        parts = s.split(None, 1)
        if len(parts) < 2:
            continue
        k, v = parts[0], parts[1].strip().strip('"').strip("'")
        if k == "id":
            ent["id"] = v
        elif k == "name":
            ent["name"] = v
        elif k == "item_type":
            ent["item_type"] = v
        elif k.startswith("tg_") and k.endswith("_id"):
            ent["tg"][k] = v
    return ent


# ---------------------------------------------------------------------------
# helpers
# ---------------------------------------------------------------------------

def _tga_size(p: Path) -> tuple[int, int]:
    with open(p, "rb") as f:
        f.seek(12)
        return struct.unpack("<HH", f.read(4))


def _font():
    for p in [
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/dejavu-sans-mono-fonts/DejaVuSansMono.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
    ]:
        try:
            return ImageFont.truetype(p, 10)
        except (OSError, IOError):
            continue
    return ImageFont.load_default()


# ---------------------------------------------------------------------------
# compose
# ---------------------------------------------------------------------------

def compose(unpacked: Path, rac_path: Path, out: Path) -> None:
    manifest = json.loads((unpacked / "manifest.json").read_text("utf-8"))
    rac = parse_rac(rac_path)
    groups = {g["name"]: g for g in manifest["texture_groups"]}
    out.mkdir(parents=True, exist_ok=True)
    fnt = _font()

    claimed: dict[str, str] = {}

    entities: list[tuple[dict, str, list[str]]] = []
    for u in rac["units"]:
        it = u.get("item_type", "f")
        entities.append((u, "unit", UNIT_TG_KEYS + (WORKER_EXTRA if it == "w" else [])))
    for b in rac["buildings"]:
        entities.append((b, "building", BUILDING_TG_KEYS))

    for ent, etype, keys in entities:
        _compose_entity(ent, etype, keys, groups, unpacked, out, manifest, fnt, claimed)

    state = {
        "source_rac": str(rac_path),
        "unpacked_dir": str(unpacked),
        "sheets_dir": str(out),
    }
    (out / "_pipeline_state.json").write_text(
        json.dumps(state, indent=2) + "\n", "utf-8"
    )
    print(f"Done. Sheets in {out}")


def _compose_entity(
    ent: dict, etype: str, keys: list[str], groups: dict,
    unpacked: Path, out: Path, manifest: dict, font, claimed: dict,
) -> None:
    eid = ent["id"]
    tg = ent["tg"]

    # --- phase 1: build row descriptors ---
    rows: list[dict[str, Any]] = []
    seen: dict[str, str] = {}

    for k in keys:
        gn = tg.get(k)
        if not gn or gn == "none":
            rows.append(dict(tg_key=k, group_name="", status="missing"))
            continue

        if gn in claimed and claimed[gn] != eid:
            rows.append(dict(tg_key=k, group_name=gn,
                             status="shared", shared_with=claimed[gn]))
            continue

        if gn in seen:
            rows.append(dict(tg_key=k, group_name=gn,
                             status="alias", alias_of=seen[gn]))
            continue

        if gn not in groups:
            print(f"  WARN: {eid}.{k} → unknown group '{gn}'", file=sys.stderr)
            rows.append(dict(tg_key=k, group_name=gn, status="missing"))
            continue

        seen[gn] = k
        claimed[gn] = eid
        gd = groups[gn]
        cells: list[dict[str, Any]] = []
        mw = mh = 0
        for t in gd["textures"]:
            tp = unpacked / t["file"]
            w, h = _tga_size(tp) if tp.exists() else (64, 64)
            cells.append(dict(
                tex_id=t["id"], file=t["file"],
                orig_w=w, orig_h=h,
                hcount=t["hcount"], vcount=t["vcount"], atime=t["atime"],
                pointx=t["pointx"], pointy=t["pointy"], ttype=t["ttype"],
            ))
            mw, mh = max(mw, w), max(mh, h)

        rows.append(dict(
            tg_key=k, group_name=gn, status="present",
            cell_w=mw, cell_h=mh, cells=cells,
        ))

    if not any(r["status"] == "present" for r in rows):
        return

    # --- phase 2: calculate positions ---
    y = GUIDE_W
    for r in rows:
        r["y"] = y
        r["height"] = r["cell_h"] if r["status"] == "present" else PLACEHOLDER_H
        y += r["height"] + GUIDE_W
    sh = y

    mcw = 0
    for r in rows:
        if r["status"] == "present":
            n = len(r["cells"])
            mcw = max(mcw, n * (r["cell_w"] + GUIDE_W))
    sw = LABEL_MARGIN + max(mcw, 200)

    # --- phase 3: render ---
    img = Image.new("RGBA", (sw, sh), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    # top guide
    draw.rectangle([0, 0, sw - 1, GUIDE_W - 1], fill=GUIDE_COLOR)

    for r in rows:
        ry, rh = r["y"], r["height"]

        # guide below row
        draw.rectangle([0, ry + rh, sw - 1, ry + rh + GUIDE_W - 1], fill=GUIDE_COLOR)

        # label area
        draw.rectangle([0, ry, LABEL_MARGIN - GUIDE_W - 1, ry + rh - 1], fill=LABEL_BG)
        short = r["tg_key"].replace("tg_", "").replace("_id", "")
        if r["status"] == "alias":
            short += f"\n= {r.get('alias_of', '?').replace('tg_', '').replace('_id', '')}"
        elif r["status"] == "shared":
            short += f"\n@ {r.get('shared_with', '?')}"
        elif r["status"] == "missing":
            short += "\n[none]"
        draw.text((2, ry + 1), short, fill=LABEL_FG, font=font)

        # vertical guide after label
        draw.rectangle(
            [LABEL_MARGIN - GUIDE_W, ry, LABEL_MARGIN - 1, ry + rh - 1],
            fill=GUIDE_COLOR,
        )

        if r["status"] == "present":
            cw, ch = r["cell_w"], r["cell_h"]
            for ci, c in enumerate(r["cells"]):
                cx = LABEL_MARGIN + ci * (cw + GUIDE_W)
                ox = (cw - c["orig_w"]) // 2
                oy = (ch - c["orig_h"]) // 2
                tp = unpacked / c["file"]
                if tp.exists():
                    try:
                        ti = Image.open(tp).convert("RGBA")
                        img.paste(ti, (cx + ox, ry + oy))
                    except Exception as e:
                        print(f"  WARN: {tp}: {e}", file=sys.stderr)
                c["offset_x"], c["offset_y"] = ox, oy
                c["sheet_x"], c["sheet_y"] = cx, ry

                # vertical guide after cell
                gx = cx + cw
                if gx < sw:
                    draw.rectangle([gx, ry, gx + GUIDE_W - 1, ry + rh - 1],
                                   fill=GUIDE_COLOR)
        elif r["status"] == "alias":
            draw.text((LABEL_MARGIN + 4, ry + 2),
                       f"ALIAS -> {r['group_name']}", fill=ALIAS_FG, font=font)
        elif r["status"] == "shared":
            draw.text((LABEL_MARGIN + 4, ry + 2),
                       f"SHARED -> see {r['shared_with']}", fill=ALIAS_FG, font=font)

    # --- phase 4: save ---
    sp = out / f"{eid}.png"
    img.save(str(sp), "PNG")

    layout: dict[str, Any] = dict(
        entity_id=eid, entity_type=etype,
        source_race=manifest.get("source_dat", "").replace(".dat", ""),
        sheet_size=[sw, sh],
        label_margin=LABEL_MARGIN,
        guide_width=GUIDE_W,
        guide_color=list(GUIDE_COLOR[:3]),
        direction_order=DIR_LABELS,
        rows=[],
    )

    for r in rows:
        entry: dict[str, Any] = dict(
            group_name=r.get("group_name", ""),
            tg_key=r["tg_key"],
            y=r["y"], height=r["height"],
            status=r["status"],
        )
        if r["status"] == "alias":
            entry["alias_of"] = r.get("alias_of", "")
        elif r["status"] == "shared":
            entry["shared_with"] = r.get("shared_with", "")
        elif r["status"] == "present":
            entry["cell_w"] = r["cell_w"]
            entry["cell_h"] = r["cell_h"]
            entry["cells"] = [
                dict(
                    x=c["sheet_x"], y=c["sheet_y"],
                    w=c["orig_w"], h=c["orig_h"],
                    cell_w=r["cell_w"], cell_h=r["cell_h"],
                    offset_x=c["offset_x"], offset_y=c["offset_y"],
                    tex_index=i, tex_id=c["tex_id"], file=c["file"],
                    hcount=c["hcount"], vcount=c["vcount"], atime=c["atime"],
                    pointx=c["pointx"], pointy=c["pointy"], ttype=c["ttype"],
                    status="present",
                )
                for i, c in enumerate(r["cells"])
            ]
        layout["rows"].append(entry)

    lp = out / f"{eid}_layout.json"
    lp.write_text(json.dumps(layout, indent=2, ensure_ascii=False) + "\n", "utf-8")
    print(f"  {eid}: {sw}x{sh} -> {sp.name}")


def main():
    ap = argparse.ArgumentParser(description="Compose per-entity sprite sheets")
    ap.add_argument("unpacked_dir", type=Path,
                    help="Unpacked .dat directory (contains manifest.json)")
    ap.add_argument("rac_file", type=Path,
                    help="Source .rac file for entity/alias info")
    ap.add_argument("output_dir", type=Path,
                    help="Output directory for PNG sheets + layout JSONs")
    args = ap.parse_args()
    compose(args.unpacked_dir, args.rac_file, args.output_dir)


if __name__ == "__main__":
    main()

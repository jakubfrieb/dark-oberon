#!/usr/bin/env python3
"""
Export unpacked .dat TGA textures into per-entity / per-animation PNG folders.

No sprite sheets, no layout JSONs, no scaling — pure pixel-perfect crops.
Multi-frame textures (hcount x vcount) are split into individual PNGs.

Usage:
    python3 export_frames.py <unpacked_dir> <rac_file> <output_dir>
    python3 export_frames.py <unpacked_dir> <rac_file> <output_dir> --entities footman,peasant

Output structure:
    <output_dir>/<entity_id>/<animation>/<animation>_NN.png

Example:
    footman/stay/stay_01.png   (64x64)
    footman/move/move_01.png   (64x64, one frame from 192x128 3x2 grid)
    footman/picture/picture_01.png
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
    from PIL import Image
except ImportError:
    sys.exit("Pillow required: pip install Pillow")

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


def _tga_size(p: Path) -> tuple[int, int]:
    with open(p, "rb") as f:
        f.seek(12)
        return struct.unpack("<HH", f.read(4))


def export_all(
    unpacked: Path, rac_path: Path, out: Path,
    entity_filter: set[str] | None,
) -> None:
    manifest = json.loads((unpacked / "manifest.json").read_text("utf-8"))
    rac = parse_rac(rac_path)
    groups = {g["name"]: g for g in manifest["texture_groups"]}

    claimed: dict[str, str] = {}

    entities: list[tuple[dict, str, list[str]]] = []
    for u in rac["units"]:
        it = u.get("item_type", "f")
        entities.append((u, "unit", UNIT_TG_KEYS + (WORKER_EXTRA if it == "w" else [])))
    for b in rac["buildings"]:
        entities.append((b, "building", BUILDING_TG_KEYS))

    for ent, etype, keys in entities:
        eid = ent.get("id", "")
        if entity_filter and eid not in entity_filter:
            continue
        _export_entity(ent, etype, keys, groups, unpacked, out, claimed)


def _export_entity(
    ent: dict, etype: str, keys: list[str], groups: dict,
    unpacked: Path, out: Path, claimed: dict,
) -> None:
    eid = ent["id"]
    tg = ent["tg"]
    seen: dict[str, str] = {}
    entity_total = 0

    for k in keys:
        gn = tg.get(k)
        if not gn or gn == "none":
            continue
        if gn in claimed and claimed[gn] != eid:
            continue
        if gn in seen:
            continue
        if gn not in groups:
            continue

        seen[gn] = k
        claimed[gn] = eid
        anim = k.replace("tg_", "").replace("_id", "")
        gd = groups[gn]

        anim_dir = out / eid / anim
        anim_dir.mkdir(parents=True, exist_ok=True)

        frame_n = 0
        for tex in gd["textures"]:
            tp = unpacked / tex["file"]
            if not tp.exists():
                print(f"  WARN: missing {tp}", file=sys.stderr)
                continue

            img = Image.open(tp).convert("RGBA")
            w, h = img.size
            hc = max(1, int(tex.get("hcount", 1)))
            vc = max(1, int(tex.get("vcount", 1)))
            fw, fh = w // hc, h // vc

            for fi in range(hc * vc):
                col = fi % hc
                row = fi // hc
                x0 = col * fw
                y0 = row * fh
                frame = img.crop((x0, y0, x0 + fw, y0 + fh))
                frame_n += 1
                fp = anim_dir / f"{anim}_{frame_n:02d}.png"
                frame.save(fp, "PNG", compress_level=3)

        entity_total += frame_n
        if frame_n:
            print(f"  {eid}/{anim}: {frame_n} frames")

    if entity_total:
        print(f"  {eid}: {entity_total} total")


def main() -> None:
    ap = argparse.ArgumentParser(
        description="Export .dat TGAs to per-entity/animation PNG folders (pixel-perfect)",
    )
    ap.add_argument("unpacked_dir", type=Path,
                    help="Unpacked .dat directory (contains manifest.json + textures/)")
    ap.add_argument("rac_file", type=Path,
                    help="Source .rac file for entity definitions")
    ap.add_argument("output_dir", type=Path,
                    help="Output root (creates <entity>/<animation>/ subfolders)")
    ap.add_argument("--entities", default=None,
                    help="Comma-separated entity IDs to export (default: all)")
    args = ap.parse_args()

    filt = set(args.entities.split(",")) if args.entities else None
    export_all(args.unpacked_dir, args.rac_file, args.output_dir, filt)


if __name__ == "__main__":
    main()

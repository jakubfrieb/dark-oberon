#!/usr/bin/env python3
"""
Import per-frame PNGs back into unpacked .dat TGA textures.

Reverse of export_frames.py — reads PNG frames from
  <frames_dir>/<entity_id>/<animation>/<animation>_NN.png
and reassembles them into TGA files in <unpacked_dir>/textures/.

Multi-frame textures (hcount x vcount) are reconstructed by tiling
individual PNGs back into a single TGA. No resampling — pixel-perfect.

Usage:
    python3 import_frames.py <frames_dir> <unpacked_dir> <rac_file>
    python3 import_frames.py <frames_dir> <unpacked_dir> <rac_file> --entities footman
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


def import_all(
    frames_dir: Path, unpacked: Path, rac_path: Path,
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
        _import_entity(ent, keys, groups, frames_dir, unpacked, claimed)


def _import_entity(
    ent: dict, keys: list[str], groups: dict,
    frames_dir: Path, unpacked: Path, claimed: dict,
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

        anim_dir = frames_dir / eid / anim
        if not anim_dir.is_dir():
            print(f"  SKIP {eid}/{anim}: folder not found", file=sys.stderr)
            continue

        frame_idx = 0
        count = 0
        for tex in gd["textures"]:
            hc = max(1, int(tex.get("hcount", 1)))
            vc = max(1, int(tex.get("vcount", 1)))
            nframes = hc * vc

            frame_pngs: list[Image.Image] = []
            missing = False
            for fi in range(nframes):
                frame_idx += 1
                fp = anim_dir / f"{anim}_{frame_idx:02d}.png"
                if not fp.exists():
                    print(f"  WARN: missing {fp}", file=sys.stderr)
                    missing = True
                    break
                frame_pngs.append(Image.open(fp).convert("RGBA"))

            if missing:
                continue

            if nframes == 1:
                tga_img = frame_pngs[0]
            else:
                fw, fh = frame_pngs[0].size
                tga_img = Image.new("RGBA", (fw * hc, fh * vc), (0, 0, 0, 0))
                for fi, fimg in enumerate(frame_pngs):
                    col = fi % hc
                    row = fi // hc
                    tga_img.paste(fimg, (col * fw, row * fh))

            out_path = unpacked / tex["file"]
            out_path.parent.mkdir(parents=True, exist_ok=True)
            tga_img.save(str(out_path), "TGA")
            count += 1

        entity_total += count
        if count:
            print(f"  {eid}/{anim}: {count} TGAs written")

    if entity_total:
        print(f"  {eid}: {entity_total} TGAs total")


def main() -> None:
    ap = argparse.ArgumentParser(
        description="Import per-frame PNGs back into unpacked .dat TGA textures",
    )
    ap.add_argument("frames_dir", type=Path,
                    help="Root with <entity>/<animation>/ PNG folders")
    ap.add_argument("unpacked_dir", type=Path,
                    help="Unpacked .dat directory to write TGAs into")
    ap.add_argument("rac_file", type=Path,
                    help="Source .rac file for entity definitions")
    ap.add_argument("--entities", default=None,
                    help="Comma-separated entity IDs to import (default: all)")
    args = ap.parse_args()

    filt = set(args.entities.split(",")) if args.entities else None
    import_all(args.frames_dir, args.unpacked_dir, args.rac_file, filt)


if __name__ == "__main__":
    main()

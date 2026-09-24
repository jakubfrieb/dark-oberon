#!/usr/bin/env python3
"""
Isometric preview of a plastic map from the real fragment textures (schemes/plastic.dat):
earth segment fragments, plus markers for sources (gold = yellow, coal = black,
forest = dark green) and start points (red rings).

Usage:
    python3 render_map.py maps/four_lakes.map -o preview.png [--scale 0.25]
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image, ImageDraw

from map_check import SIZES, read_map, repo_root

TILE_W, TILE_H = 32, 16          # one field in the isometric projection
_TEX_CACHE: dict = {}
MARK = {"goldmine": (255, 210, 0, 255), "coal": (20, 20, 20, 255), "forest": (10, 70, 20, 255)}


def _textures() -> dict:
    if _TEX_CACHE:
        return _TEX_CACHE
    root = repo_root()
    tmp = Path(tempfile.mkdtemp(prefix="plastic_tex_"))
    subprocess.run([sys.executable, str(root / ".cursor/skills/dark-oberon-dat/scripts/do_dat_tool.py"), "unpack",
                    str(root / "schemes/plastic.dat"), "-o", str(tmp)], check=True, stdout=subprocess.DEVNULL)
    manifest = json.loads((tmp / "manifest.json").read_text())
    for g in manifest["texture_groups"]:
        _TEX_CACHE[g["name"]] = Image.open(tmp / g["textures"][0]["file"]).convert("RGBA")
    return _TEX_CACHE


def _iso(x: float, y: float, height_fields: int) -> tuple[float, float]:
    return (x - y) * TILE_W / 2 + height_fields * TILE_W / 2, (x + y) * TILE_H / 2


def render(map_path: Path, out: Path, scale: float = 0.25) -> Path:
    root = repo_root()
    m = read_map(map_path, root / "schemes" / "plastic.sch")
    tex = _textures()
    W, H = m.width, m.height
    img = Image.new("RGBA", (int((W + H) * TILE_W / 2) + 64, int((W + H) * TILE_H / 2) + 64), (0, 0, 0, 255))
    h, w = m.grid.shape
    for cy in range(h):
        for cx in range(w):
            t = tex.get(m.grid[cy, cx])
            if t is not None:
                sx, sy = _iso(cx * 5, cy * 5, H)
                img.alpha_composite(t, (int(sx - t.width / 2), int(sy)))
    d = ImageDraw.Draw(img)
    for sid, x, y, _life, _amt in m.sources:
        s = SIZES.get(sid, 3)
        pts = [_iso(x, y, H), _iso(x + s, y, H), _iso(x + s, y + s, H), _iso(x, y + s, H)]
        d.polygon(pts, fill=MARK.get(sid, (200, 0, 200, 255)))
    for x, y in m.starts:
        cx, cy = _iso(x + 3.5, y + 3.5, H)
        d.ellipse([cx - 60, cy - 30, cx + 60, cy + 30], outline=(255, 0, 0, 255), width=8)
    if scale != 1:
        img = img.resize((int(img.width * scale), int(img.height * scale)), Image.LANCZOS)
    img.save(out)
    return Path(out)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("map", type=Path)
    ap.add_argument("-o", "--out", type=Path, required=True)
    ap.add_argument("--scale", type=float, default=0.25)
    a = ap.parse_args()
    render(a.map, a.out, a.scale)
    print(f"wrote {a.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

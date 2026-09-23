#!/usr/bin/env python3
"""
Team-colour recolouring for Dark Oberon races.

  calibrate  — learn how human-red maps to another human variant (hue + S/V scale)
  apply      — copy an unpacked .dat and recolour the red team colour in all TGAs

Usage:
    python3 recolor_race.py calibrate --red <unpacked_red> --other <unpacked_blue> -o blue.json
    python3 recolor_race.py apply <src_unpacked> <dst_unpacked> --mapping blue.json
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import sys
from pathlib import Path

import numpy as np
from PIL import Image

HUE_TOL = 18.0
SAT_MIN = 0.45
VAL_MIN = 0.15


def _hsv(rgb: np.ndarray) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    x = rgb.astype(np.float64) / 255.0
    r, g, b = x[..., 0], x[..., 1], x[..., 2]
    mx, mn = x.max(-1), x.min(-1)
    d = mx - mn
    h = np.zeros_like(mx)
    nz = d > 1e-9
    rm = nz & (mx == r)
    gm = nz & (mx == g) & ~rm
    bm = nz & ~rm & ~gm
    h[rm] = ((g - b)[rm] / d[rm]) % 6
    h[gm] = (b - r)[gm] / d[gm] + 2
    h[bm] = (r - g)[bm] / d[bm] + 4
    s = np.where(mx > 0, d / np.where(mx > 0, mx, 1), 0)
    return h * 60.0, s, mx


def _rgb(h: np.ndarray, s: np.ndarray, v: np.ndarray) -> np.ndarray:
    h6 = (h % 360) / 60.0
    i = np.floor(h6).astype(int) % 6
    f = h6 - np.floor(h6)
    p, q, t = v * (1 - s), v * (1 - s * f), v * (1 - s * (1 - f))
    table = [(v, t, p), (q, v, p), (p, v, t), (p, q, v), (t, p, v), (v, p, q)]
    out = np.zeros(h.shape + (3,))
    for k, (a, b, c) in enumerate(table):
        m = i == k
        out[m] = np.stack([a[m], b[m], c[m]], -1)
    return np.clip(np.round(out * 255), 0, 255).astype(np.uint8)


def team_mask(rgb: np.ndarray) -> np.ndarray:
    h, s, v = _hsv(rgb)
    return ((h <= HUE_TOL) | (h >= 360 - HUE_TOL)) & (s >= SAT_MIN) & (v >= VAL_MIN)


def recolor_image(img: Image.Image, mapping: dict) -> Image.Image:
    rgba = np.asarray(img.convert("RGBA")).copy()
    rgb = rgba[..., :3]
    m = team_mask(rgb)
    if m.any():
        h, s, v = _hsv(rgb)
        h2 = np.full(h.shape, float(mapping["hue"]))
        s2 = np.clip(s * mapping.get("sat_scale", 1.0), 0, 1)
        v2 = np.clip(v * mapping.get("val_scale", 1.0), 0, 1)
        rgb[m] = _rgb(h2, s2, v2)[m]
    return Image.fromarray(rgba, "RGBA")


def _texture_key(path: Path) -> str:
    """'g020_t000_townhall_stay__townhall_stay1.tga' -> 'townhall_stay__townhall_stay1'
    (group/texture indices differ between race variants)."""
    return re.sub(r"^g\d+_t\d+_", "", path.stem)


def calibrate(red_dir: Path, other_dir: Path) -> dict:
    hues, ss, vs = [], [], []
    others = {_texture_key(p): p for p in (Path(other_dir) / "textures").glob("*.tga")}
    for red in sorted((Path(red_dir) / "textures").glob("*.tga")):
        oth = others.get(_texture_key(red))
        if oth is None:
            continue
        a = np.asarray(Image.open(red).convert("RGBA"))
        b = np.asarray(Image.open(oth).convert("RGBA"))
        if a.shape != b.shape:
            continue
        diff = (np.abs(a[..., :3].astype(int) - b[..., :3].astype(int)).sum(-1) > 30) \
            & (a[..., 3] > 0) & team_mask(a[..., :3])
        if not diff.any():
            continue
        _, sa, va = _hsv(a[..., :3][diff])
        hb, sb, vb = _hsv(b[..., :3][diff])
        hues.append(hb)
        ss.append(sb / np.maximum(sa, 1e-6))
        vs.append(vb / np.maximum(va, 1e-6))
    if not hues:
        raise ValueError("no differing team-colour pixels found")
    ang = np.deg2rad(np.concatenate(hues))
    hue = float(np.rad2deg(np.arctan2(np.sin(ang).mean(), np.cos(ang).mean())) % 360)
    return {"hue": round(hue, 1),
            "sat_scale": round(float(np.median(np.concatenate(ss))), 3),
            "val_scale": round(float(np.median(np.concatenate(vs))), 3)}


def recolor_unpacked(src: Path, dst: Path, mapping: dict) -> int:
    src, dst = Path(src), Path(dst)
    if dst.exists():
        shutil.rmtree(dst)
    shutil.copytree(src, dst)
    n = 0
    for tga in sorted((dst / "textures").glob("*.tga")):
        img = Image.open(tga)
        out = recolor_image(img, mapping)
        if not np.array_equal(np.asarray(img.convert("RGBA")), np.asarray(out)):
            out.save(tga, "TGA")
            n += 1
    return n


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    c = sub.add_parser("calibrate")
    c.add_argument("--red", type=Path, required=True)
    c.add_argument("--other", type=Path, required=True)
    c.add_argument("-o", "--out", type=Path, required=True)
    a = sub.add_parser("apply")
    a.add_argument("src", type=Path)
    a.add_argument("dst", type=Path)
    a.add_argument("--mapping", type=Path, required=True)
    args = ap.parse_args()
    if args.cmd == "calibrate":
        m = calibrate(args.red, args.other)
        args.out.write_text(json.dumps(m, indent=2))
        print(json.dumps(m))
    else:
        n = recolor_unpacked(args.src, args.dst, json.loads(args.mapping.read_text()))
        print(f"recoloured {n} textures -> {args.dst}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

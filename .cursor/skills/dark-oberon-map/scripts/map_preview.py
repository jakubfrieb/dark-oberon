#!/usr/bin/env python3
"""Terrain preview PNG from a Dark Oberon .map + scheme (fragments + layers)."""

from __future__ import annotations

import argparse
import colorsys
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from dark_oberon_maplib import (  # noqa: E402
    build_terrain_grid,
    find_repo_root,
    parse_map,
    parse_scheme,
    resolve_scheme_path,
)

# Common plastic-style layer values (terrain_id grid stores layer numbers)
KNOWN_LAYER_COLORS: dict[int, tuple[int, int, int]] = {
    0: (40, 40, 40),
    5: (30, 80, 180),
    7: (200, 190, 140),
    8: (220, 210, 160),
    10: (60, 140, 60),
    20: (50, 100, 50),
    25: (140, 130, 70),
    30: (120, 120, 120),
    255: (200, 50, 200),
}


def layer_rgb(v: int) -> tuple[int, int, int]:
    if v in KNOWN_LAYER_COLORS:
        return KNOWN_LAYER_COLORS[v]
    if v < 0:
        return (0, 0, 0)
    if v >= 255:
        return (32, 32, 32)
    h = (v * 0.618033988749895) % 1.0
    r, g, b = colorsys.hsv_to_rgb(h, 0.55, 0.92)
    return (int(r * 255), int(g * 255), int(b * 255))


def main() -> int:
    ap = argparse.ArgumentParser(description="Terrain preview PNG for Dark Oberon maps")
    ap.add_argument("map_file", type=Path)
    ap.add_argument("-o", "--output", type=Path, default=None)
    ap.add_argument("--segment", type=int, default=1, help="Segment id (default: 1 Earth)")
    ap.add_argument(
        "--scheme",
        type=Path,
        default=None,
        help="Path to .sch (default: schemes/<scheme>.sch)",
    )
    ap.add_argument("--repo-root", type=Path, default=None)
    ap.add_argument(
        "--scale",
        type=int,
        default=1,
        help="Integer upscale per mapel (default 1)",
    )
    ap.add_argument(
        "--flip-y",
        action="store_true",
        help="Flip vertically (image row = map y ascending)",
    )
    args = ap.parse_args()

    repo = args.repo_root or find_repo_root(Path(__file__).parent)

    pmap = parse_map(args.map_file)
    scheme_path = args.scheme or resolve_scheme_path(pmap.header.scheme, repo)
    if not scheme_path.is_file():
        print(f"ERROR: scheme not found: {scheme_path}", file=sys.stderr)
        return 2

    sch = parse_scheme(scheme_path)
    sid = args.segment
    seg_map = pmap.segments.get(sid)
    sch_seg = sch.segments.get(sid)
    if not seg_map or not sch_seg:
        print(f"ERROR: segment {sid} missing in map or scheme", file=sys.stderr)
        return 2

    w, h = pmap.header.width, pmap.header.height
    grid = build_terrain_grid(w, h, seg_map, sch_seg, fill_default=True)

    try:
        from PIL import Image
    except ImportError:
        print("ERROR: pip install Pillow (see scripts/requirements.txt)", file=sys.stderr)
        return 2

    scale = max(1, args.scale)
    img_w, img_h = w * scale, h * scale
    img = Image.new("RGB", (img_w, img_h))
    pix = img.load()
    assert pix is not None

    for mx in range(w):
        for my in range(h):
            v = grid[mx][my]
            if v is None:
                r, g, b = (0, 0, 0)
            else:
                r, g, b = layer_rgb(int(v))
            sy = my if args.flip_y else h - 1 - my
            for dx in range(scale):
                for dy in range(scale):
                    px = mx * scale + dx
                    py = sy * scale + dy
                    if 0 <= px < img_w and 0 <= py < img_h:
                        pix[px, py] = (r, g, b)

    out = args.output or args.map_file.with_suffix(".preview.png")
    img.save(out)
    print(f"Wrote {out} ({img_w}x{img_h}) segment={sid}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

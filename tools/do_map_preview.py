#!/usr/bin/env python3
"""
Render a terrain preview PNG from a Dark Oberon .map + scheme .sch.

Reconstructs fragment placement only (same logic as TTERR_FRAG + UpdateTerrainId in
domap.cpp). Layers and objects are not painted (optional future work).

Requires: Pillow (pip install pillow)

Example:
  python3 tools/do_map_preview.py maps/trial.map -o trial_seg1.png --segment 1
  python3 tools/do_map_preview.py maps/trial.map -o trial_seg0.png --segment 0 \\
      --scheme schemes/plastic.sch
"""

from __future__ import annotations

import argparse
import colorsys
import re
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("Install Pillow: pip install pillow", file=sys.stderr)
    sys.exit(1)


def strip_comments(text: str) -> str:
    out = []
    for line in text.splitlines():
        if "#" in line:
            line = line[: line.index("#")]
        out.append(line)
    return "\n".join(out)


def extract_tag_block(text: str, tag: str) -> str | None:
    """Block from '<tag>' until '</tag>' (first match, non-greedy inner)."""
    open_re = re.compile(rf"^\s*<{re.escape(tag)}>\s*$", re.MULTILINE)
    close_re = re.compile(rf"^\s*</{re.escape(tag)}>\s*$", re.MULTILINE)
    m = open_re.search(text)
    if not m:
        return None
    rest = text[m.end() :]
    m2 = close_re.search(rest)
    if not m2:
        return None
    return rest[: m2.start()]


def parse_map_header(text: str) -> dict[str, str | int]:
    hdr: dict[str, str | int] = {}
    for line in text.splitlines():
        line = line.strip()
        if not line:
            continue
        if line.startswith("<"):
            break
        parts = line.split(None, 1)
        if len(parts) < 2:
            continue
        key, rest = parts[0], parts[1].strip()
        if key in ("name", "author", "scheme"):
            if rest.startswith('"') and rest.endswith('"'):
                rest = rest[1:-1]
            hdr[key] = rest
        elif key in ("width", "height"):
            hdr[key] = int(rest.split()[0])
    return hdr


def parse_map_fragments(segment_body: str) -> list[tuple[int, int, int]]:
    """Ordered list of (fragment_scheme_id, x, y) — same as map file fragment_N fid x y."""
    frag_sec = extract_tag_block(segment_body, "Fragments")
    if not frag_sec:
        return []
    out: list[tuple[int, int, int]] = []
    for line in frag_sec.splitlines():
        line = line.strip()
        m = re.match(r"fragment_\d+\s+(-?\d+)\s+(-?\d+)\s+(-?\d+)\s*$", line)
        if m:
            out.append((int(m.group(1)), int(m.group(2)), int(m.group(3))))
    return out


def parse_scheme_segment_fragments(sch_text: str, seg_id: int) -> tuple[dict[int, tuple[int, list[int]]], int]:
    """
    Returns (fragments[fid] = (size, flat_terrain_ids), default_terrain_id).
    """
    seg_tag = f"Segment {seg_id}"
    body = extract_tag_block(sch_text, seg_tag)
    if not body:
        raise ValueError(f"Scheme has no <{seg_tag}>")

    frag_block = extract_tag_block(body, "Fragments")
    if not frag_block:
        raise ValueError(f"No <Fragments> in {seg_tag}")

    default_tid = 0
    m = re.search(r"default_terrain_id\s+(\d+)", frag_block)
    if m:
        default_tid = int(m.group(1))

    fragments: dict[int, tuple[int, list[int]]] = {}
    pos = 0
    while True:
        m = re.search(rf"<Fragment\s+(\d+)>", frag_block[pos:])
        if not m:
            break
        fid = int(m.group(1))
        start = pos + m.end()
        mclose = re.search(rf"</Fragment\s+{fid}>", frag_block[start:])
        if not mclose:
            break
        chunk = frag_block[start : start + mclose.start()]
        pos = start + mclose.end()

        size = 1
        ms = re.search(r"^\s*size\s+(\d+)\s*$", chunk, re.MULTILINE)
        if ms:
            size = int(ms.group(1))

        tids: list[int] = []
        for line in chunk.splitlines():
            line = line.strip()
            if line.startswith("terrain_id"):
                parts = line.split()
                tids.extend(int(x) for x in parts[1:])
        if len(tids) != size * size:
            raise ValueError(
                f"Fragment {fid} in segment {seg_id}: expected {size*size} terrain_id, got {len(tids)}"
            )
        fragments[fid] = (size, tids)

    return fragments, default_tid


def flat_to_field(size: int, flat: list[int]) -> list[list[int]]:
    """field[i][j] like C++ terrain_field[i][j], i = 0..size-1 width, j = 0..size-1 height."""
    field: list[list[int]] = []
    for i in range(size):
        row = [flat[i * size + j] for j in range(size)]
        field.append(row)
    return field


def tid_color(tid: int) -> tuple[int, int, int]:
    if tid >= 255:
        return (32, 32, 32)
    if tid < 0:
        return (255, 0, 255)
    h = (tid * 0.618033988749895) % 1.0
    r, g, b = colorsys.hsv_to_rgb(h, 0.55, 0.92)
    return (int(r * 255), int(g * 255), int(b * 255))


def build_grid(
    width: int,
    height: int,
    default_tid: int,
    scheme_frags: dict[int, tuple[int, list[int]]],
    placements: list[tuple[int, int, int]],
) -> list[list[int]]:
    grid: list[list[int]] = [[default_tid for _ in range(height)] for _ in range(width)]
    for fid, px, py in placements:
        if fid not in scheme_frags:
            continue
        size, flat = scheme_frags[fid]
        field = flat_to_field(size, flat)
        for i in range(size):
            for j in range(size):
                x, y = px + i, py + j
                if 0 <= x < width and 0 <= y < height:
                    grid[x][y] = field[i][j]
    return grid


def render_png(
    grid: list[list[int]],
    scale: int,
    path: Path,
) -> None:
    w, h = len(grid), len(grid[0]) if grid else 0
    img = Image.new("RGB", (w * scale, h * scale))
    px = img.load()
    for x in range(w):
        for y in range(h):
            rgb = tid_color(grid[x][y])
            for dx in range(scale):
                for dy in range(scale):
                    px[x * scale + dx, y * scale + dy] = rgb
    path.parent.mkdir(parents=True, exist_ok=True)
    img.save(path, "PNG")


def main() -> None:
    ap = argparse.ArgumentParser(description="Dark Oberon map terrain preview (fragments only)")
    ap.add_argument("map_file", type=Path)
    ap.add_argument("-o", "--output", type=Path, required=True)
    ap.add_argument("--segment", type=int, default=1, help="segment index 0..2 (default 1 = ground)")
    ap.add_argument("--scheme", type=Path, default=None, help="path to scheme .sch (default: schemes/<name>.sch)")
    ap.add_argument("--scale", type=int, default=4, help="pixels per map cell")
    args = ap.parse_args()

    raw = args.map_file.read_text(encoding="utf-8", errors="replace")
    text = strip_comments(raw)
    hdr = parse_map_header(text)
    if "width" not in hdr or "height" not in hdr:
        print("Map missing width/height in header", file=sys.stderr)
        sys.exit(1)
    width = int(hdr["width"])
    height = int(hdr["height"])
    scheme_name = str(hdr.get("scheme", "plastic"))

    scheme_path = args.scheme
    if scheme_path is None:
        root = args.map_file.parent.parent
        scheme_path = root / "schemes" / f"{scheme_name}.sch"
    if not scheme_path.is_file():
        print(f"Scheme not found: {scheme_path}", file=sys.stderr)
        sys.exit(1)

    sch_raw = strip_comments(scheme_path.read_text(encoding="utf-8", errors="replace"))
    scheme_frags, default_tid = parse_scheme_segment_fragments(sch_raw, args.segment)

    seg_tag = f"Segment {args.segment}"
    seg_body = extract_tag_block(text, seg_tag)
    if not seg_body:
        print(f"Map has no <{seg_tag}>", file=sys.stderr)
        sys.exit(1)
    placements = parse_map_fragments(seg_body)

    grid = build_grid(width, height, default_tid, scheme_frags, placements)
    render_png(grid, max(1, args.scale), args.output)
    print(f"Wrote {args.output} ({width}x{height} mapels, segment {args.segment}, scale {args.scale})")


if __name__ == "__main__":
    main()

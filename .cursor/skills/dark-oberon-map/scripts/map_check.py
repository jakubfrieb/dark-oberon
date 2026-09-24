#!/usr/bin/env python3
"""
Playability checks for plastic maps (hand-made or generated).

  - fragment adjacency: every E/S neighbour pair of earth fragments must occur in the
    hand-made maps (sunnybay, virgin_editor, trial) -> water and rock edges are closed
  - start points on walkable terrain and mutually reachable for land units
  - resources: no overlapping footprints, gold/wood balanced between start points (+-10 %)

Usage:
    python3 map_check.py maps/four_lakes.map
"""

from __future__ import annotations

import re
import sys
from collections import deque
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
FRAG = 5
WALK_MIN, WALK_MAX = 10, 25          # grass .. rocky grass (plastic, segment 1)
SIZES = {"goldmine": 4, "coal": 4, "forest": 3}
BALANCE_RADIUS = 25
BALANCE_TOL = 0.10


def repo_root() -> Path:
    p = HERE
    while not (p / "schemes").is_dir() and p != p.parent:
        p = p.parent
    return p


HANDMADE = ["sunnybay.map", "virgin_editor.map", "trial.map"]


@dataclass
class MapData:
    width: int
    height: int
    grid: np.ndarray                          # [cy, cx] fragment names, segment 1
    sources: list = field(default_factory=list)   # (id, x, y, life, amount)
    starts: list = field(default_factory=list)    # (x, y)


def _segment(text: str, seg: int) -> str:
    s = text.index(f"<Segment {seg}>")
    return text[s:text.index(f"</Segment {seg}>", s)]


def scheme_fragments(sch_path: Path, segment: int = 1) -> dict:
    """index -> (name, terrain layers list [x*5 + y])  (terrain_id is column-major)."""
    seg = _segment(Path(sch_path).read_text(encoding="latin-1"), segment)
    out = {}
    for idx, name, ids in re.findall(
            r"<Fragment (\d+)>\s*size \d+\s*tg_id \"?([A-Za-z0-9_]+)\"?\s*(?:terrain_id ([\d ]+))?", seg):
        out[int(idx)] = (name, [int(v) for v in ids.split()] if ids else [])
    return out


def read_map(path: Path, sch_path: Path) -> MapData:
    text = Path(path).read_text(encoding="latin-1")
    width = int(re.search(r"^width (\d+)", text, re.M).group(1))
    height = int(re.search(r"^height (\d+)", text, re.M).group(1))
    frags = scheme_fragments(sch_path, 1)
    seg = _segment(text, 1)
    seg = seg[:seg.index("<Layers>")] if "<Layers>" in seg else seg
    grid = np.full((height // FRAG, width // FRAG), "grass", dtype=object)
    for fid, x, y in re.findall(r"fragment_\d+ (\d+) (\d+) (\d+)", seg):
        grid[int(y) // FRAG, int(x) // FRAG] = frags[int(fid)][0]
    sources = [(sid, int(x), int(y), int(life), int(amt)) for sid, x, y, life, amt in
               re.findall(r"source_\d+ \"([a-z_]+)\" (\d+) (\d+) (\d+) (\d+)", text)]
    starts = [(int(x), int(y)) for x, y in re.findall(r"start_point_\d+ (\d+) (\d+)", text)]
    return MapData(width, height, grid, sources, starts)


def learn_adjacency(map_paths, sch_path: Path) -> set:
    allowed = set()
    for p in map_paths:
        g = read_map(p, sch_path).grid
        h, w = g.shape
        for cy in range(h):
            for cx in range(w):
                if cx + 1 < w:
                    allowed.add((g[cy, cx], "E", g[cy, cx + 1]))
                if cy + 1 < h:
                    allowed.add((g[cy, cx], "S", g[cy + 1, cx]))
    return allowed


def _edges_match(a: str, d: str, b: str, masks: dict) -> bool:
    """Shared edge of the two fragments has the same terrain (masks are column-major).
    Full-rock masks (rocks_s, rocks_w, ...) say nothing about the drawn edge -> never match."""
    ma, mb = masks.get(a), masks.get(b)
    if not ma or not mb or set(ma) == {30} or set(mb) == {30}:
        return False
    if d == "E":
        return [ma[20 + y] for y in range(5)] == [mb[y] for y in range(5)]
    return [ma[x * 5 + 4] for x in range(5)] == [mb[x * 5] for x in range(5)]


def check_adjacency(grid: np.ndarray, allowed: set, sch_path: Path | None = None) -> list[str]:
    """Pairs must occur in the hand-made maps, or (with @p sch_path) line up exactly on the
    shared edge (e.g. a ramp right after a straight edge, which the original author never used)."""
    masks = {n: ids for n, ids in scheme_fragments(sch_path, 1).values()} if sch_path else {}
    errs = []
    h, w = grid.shape
    for cy in range(h):
        for cx in range(w):
            for d, (dx, dy) in (("E", (1, 0)), ("S", (0, 1))):
                if cx + dx >= w or cy + dy >= h:
                    continue
                a, b = grid[cy, cx], grid[cy + dy, cx + dx]
                if (a, d, b) not in allowed and not (a == b and a in ("grass", "sea")) \
                        and not _edges_match(a, d, b, masks):
                    errs.append(f"cell ({cx},{cy}): {a} -{d}- {b} never seen in hand-made maps")
    return errs


def walkable_grid(grid: np.ndarray, sch_path: Path, obstacles) -> np.ndarray:
    """[y, x] field walkability for land units; obstacles = (x, y, size) footprints."""
    by_name = {}
    for name, ids in scheme_fragments(sch_path, 1).values():
        by_name.setdefault(name, ids)
    h, w = grid.shape
    walk = np.zeros((h * FRAG, w * FRAG), bool)
    for cy in range(h):
        for cx in range(w):
            ids = by_name.get(grid[cy, cx]) or [10] * 25
            for i, layer in enumerate(ids):
                x, y = i // FRAG, i % FRAG
                walk[cy * FRAG + y, cx * FRAG + x] = WALK_MIN <= layer <= WALK_MAX
    for x, y, s in obstacles:
        walk[y:y + s, x:x + s] = False
    return walk


def check_connectivity(walk: np.ndarray, starts) -> list[str]:
    errs = [f"start {s} is not on walkable terrain" for s in starts if not walk[s[1], s[0]]]
    if errs or not starts:
        return errs
    seen = np.zeros_like(walk)
    q = deque([starts[0]])
    seen[starts[0][1], starts[0][0]] = True
    h, w = walk.shape
    while q:
        x, y = q.popleft()
        for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
            if 0 <= nx < w and 0 <= ny < h and walk[ny, nx] and not seen[ny, nx]:
                seen[ny, nx] = True
                q.append((nx, ny))
    return [f"start {s} not reachable from {starts[0]}" for s in starts[1:] if not seen[s[1], s[0]]]


def check_resources(sources, starts) -> list[str]:
    errs = []
    boxes = [(sid, x, y, SIZES.get(sid, 3)) for sid, x, y, _life, _amt in sources]
    for i in range(len(boxes)):
        a = boxes[i]
        for b in boxes[i + 1:]:
            if a[1] < b[1] + b[3] and b[1] < a[1] + a[3] and a[2] < b[2] + b[3] and b[2] < a[2] + a[3]:
                errs.append(f"overlap {a[0]}@({a[1]},{a[2]}) and {b[0]}@({b[1]},{b[2]})")
    for kind in ("goldmine", "forest"):
        totals = []
        for sx, sy in starts:
            t = 0
            for sid, x, y, _life, amt in sources:
                if sid == kind and (x - sx) ** 2 + (y - sy) ** 2 <= BALANCE_RADIUS ** 2:
                    t += amt if kind == "goldmine" else 1
            totals.append(t)
        if totals and max(totals) > 0 and min(totals) < max(totals) * (1 - BALANCE_TOL):
            errs.append(f"{kind} balance within {BALANCE_RADIUS} fields of starts: {totals}")
    return errs


def check_map(path: Path) -> list[str]:
    root = repo_root()
    sch = root / "schemes" / "plastic.sch"
    m = read_map(path, sch)
    allowed = learn_adjacency([root / "maps" / n for n in HANDMADE], sch)
    errs = check_adjacency(m.grid, allowed, sch)
    obstacles = [(x, y, SIZES.get(sid, 3)) for sid, x, y, _l, _a in m.sources]
    errs += check_connectivity(walkable_grid(m.grid, sch, obstacles), m.starts)
    errs += check_resources(m.sources, m.starts)
    return errs


def main() -> int:
    errs = check_map(Path(sys.argv[1]))
    for e in errs:
        print("ERROR", e)
    print("OK" if not errs else f"{len(errs)} error(s)")
    return 1 if errs else 0


if __name__ == "__main__":
    sys.exit(main())

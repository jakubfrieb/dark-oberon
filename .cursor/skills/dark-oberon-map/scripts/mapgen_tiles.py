"""
Autotiling of region outlines onto plastic scheme fragments (5x5 fields each).

A region (water or plateau) is a boolean mask over fragment cells, indexed [cy, cx]
(cx = x // 5, cy = y // 5; E = cx + 1, S = cy + 1). Every region cell that touches the
outside gets an edge fragment chosen by where the outside is (tables verified against the
hand-made maps sunnybay / virgin_editor / trial). Cells outside the map count as inside, so
regions touching the map border get no edge there.
"""

from __future__ import annotations

import re
from pathlib import Path

import numpy as np

# outside sides -> fragment (orthogonal sides N/E/S/W, or one diagonal dNW/dNE/dSW/dSE)
WATER_TABLE = {
    frozenset("N"): "coast_n", frozenset("W"): "coast_e", frozenset("S"): "coast_s", frozenset("E"): "coast_w",
    frozenset("NE"): "coast_wn", frozenset("NW"): "coast_en", frozenset("SW"): "coast_es",
    frozenset("SE"): "coast_ws",
    frozenset({"dNW"}): "coast_ne", frozenset({"dNE"}): "coast_nw", frozenset({"dSW"}): "coast_se",
    frozenset({"dSE"}): "coast_sw",
}
PLATEAU_TABLE = {
    frozenset("N"): "rocks_s", frozenset("S"): "rocks_n", frozenset("W"): "rocks_w", frozenset("E"): "rocks_e",
    frozenset("NW"): "rocks_sw", frozenset("NE"): "rocks_se", frozenset("SW"): "rocks_nw",
    frozenset("SE"): "rocks_ne",
    frozenset({"dNW"}): "rocks_ws", frozenset({"dNE"}): "rocks_es", frozenset({"dSW"}): "rocks_wn",
    frozenset({"dSE"}): "rocks_en",
}
# ramp pairs on straight edges: (first, second) in +x (horizontal) or +y (vertical) order
RAMPS = {
    "rocks_s": ("rocks_s_end_e", "rocks_s_end_w", (1, 0)),
    "rocks_n": ("rocks_n_end_e", "rocks_n_end_w", (1, 0)),
    "rocks_w": ("rocks_w_end_n", "rocks_w_end_s", (0, 1)),
    "rocks_e": ("rocks_e_end_n", "rocks_e_end_s", (0, 1)),
}

_ORTH = {"N": (0, -1), "E": (1, 0), "S": (0, 1), "W": (-1, 0)}
_DIAG = {"dNW": (-1, -1), "dNE": (1, -1), "dSW": (-1, 1), "dSE": (1, 1)}


def _inside(mask: np.ndarray, cx: int, cy: int) -> bool:
    h, w = mask.shape
    if cx < 0 or cy < 0 or cx >= w or cy >= h:
        return True
    return bool(mask[cy, cx])


def outside_sides(mask: np.ndarray, cx: int, cy: int) -> frozenset:
    """Orthogonal outside sides; if none, the outside diagonals (prefixed 'd')."""
    orth = frozenset(d for d, (dx, dy) in _ORTH.items() if not _inside(mask, cx + dx, cy + dy))
    if orth:
        return orth
    return frozenset(d for d, (dx, dy) in _DIAG.items() if not _inside(mask, cx + dx, cy + dy))


def outline_fragment(mask: np.ndarray, cx: int, cy: int, table: dict) -> str | None:
    """Edge fragment for a region cell; None for interior cells and cells outside the region.
    Raises ValueError when no fragment fits (unsmoothed region)."""
    if not mask[cy, cx]:
        return None
    sides = outside_sides(mask, cx, cy)
    if not sides:
        return None
    try:
        return table[sides]
    except KeyError:
        raise ValueError(f"no fragment for outside {sorted(sides)} at cell ({cx},{cy})") from None


def _opening(mask: np.ndarray) -> np.ndarray:
    """Keep only cells covered by some fully-inside 3x3 block (out-of-map counts as inside)."""
    h, w = mask.shape
    p = np.pad(mask, 1, constant_values=True)
    core = np.ones((h, w), bool)
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            core &= p[1 + dy:1 + dy + h, 1 + dx:1 + dx + w]
    out = np.zeros((h, w), bool)
    pc = np.pad(core, 1, constant_values=False)
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            out |= pc[1 + dy:1 + dy + h, 1 + dx:1 + dx + w]
    return out & mask


def _remove_saddles(mask: np.ndarray) -> bool:
    changed = False
    h, w = mask.shape
    for cy in range(h - 1):
        for cx in range(w - 1):
            a, b = mask[cy, cx], mask[cy, cx + 1]
            c, d = mask[cy + 1, cx], mask[cy + 1, cx + 1]
            if a and d and not b and not c:
                mask[cy, cx] = mask[cy + 1, cx + 1] = False
                changed = True
            elif b and c and not a and not d:
                mask[cy, cx + 1] = mask[cy + 1, cx] = False
                changed = True
    return changed


def smooth_region(mask: np.ndarray, table: dict = WATER_TABLE) -> np.ndarray:
    """Opening 3x3, remove diagonal saddles and any cell no fragment fits; repeat until stable."""
    m = mask.copy()
    for _ in range(64):
        m = _opening(m)
        changed = _remove_saddles(m)
        bad = [(cx, cy) for cy in range(m.shape[0]) for cx in range(m.shape[1])
               if m[cy, cx] and outside_sides(m, cx, cy) and outside_sides(m, cx, cy) not in table]
        for cx, cy in bad:
            m[cy, cx] = False
        if not changed and not bad:
            return m
    return m


def place_ramps(mask: np.ndarray, rng, n: int) -> dict:
    """Up to @p n ramps (pairs of end fragments) on straight plateau edges of length >= 4,
    preferring different sides. Returns {(cx, cy): fragment_name}."""
    h, w = mask.shape
    cands: dict[str, list] = {k: [] for k in RAMPS}
    for cy in range(h):
        for cx in range(w):
            name = outline_fragment(mask, cx, cy, PLATEAU_TABLE) if mask[cy, cx] else None
            if name not in RAMPS:
                continue
            dx, dy = RAMPS[name][2]
            run = [(cx + k * dx, cy + k * dy) for k in (-1, 1, 2)]
            if all(0 <= x < w and 0 <= y < h and mask[y, x]
                   and outline_fragment(mask, x, y, PLATEAU_TABLE) == name for x, y in run):
                cands[name].append((cx, cy))
    ramps: dict = {}
    sides = [k for k in RAMPS if cands[k]]
    rng.shuffle(sides)
    for side in sides[:n]:
        opts = cands[side]
        cx, cy = opts[int(rng.integers(len(opts)))]
        first, second, (dx, dy) = RAMPS[side]
        ramps[(cx, cy)] = first
        ramps[(cx + dx, cy + dy)] = second
    return ramps


def fragment_ids(sch_path: Path, segment: int) -> dict[str, int]:
    """name -> fragment index for one scheme segment (first index wins for duplicates)."""
    text = Path(sch_path).read_text(encoding="latin-1")
    start = text.index(f"<Segment {segment}>")
    seg = text[start:text.index(f"</Segment {segment}>", start)]
    ids: dict[str, int] = {}
    for idx, name in re.findall(r"<Fragment (\d+)>\s*size \d+\s*tg_id \"?([A-Za-z0-9_]+)\"?", seg):
        ids.setdefault(name, int(idx))
    return ids


def ug_name(name: str) -> str:
    """Underground (segment 0) mirror of an earth fragment: water and coast are mirrored."""
    if name.startswith("coast_") or name == "sea":
        return "ug_" + name
    return "ug_grass"

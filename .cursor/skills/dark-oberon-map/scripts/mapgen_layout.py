"""
Map layout on the fragment-cell grid: 4 start points, lakes and rock-outlined plateaus.

Everything is a boolean mask indexed [cy, cx] (one cell = one 5x5 fragment). Regions are
smoothed so every edge cell has a fragment (mapgen_tiles); plateaus keep at least one grass
cell (8-neighbourhood) from water, so no cliff pieces are needed.
"""

from __future__ import annotations

from collections import deque
from dataclasses import dataclass, field

import numpy as np

from mapgen_tiles import PLATEAU_TABLE, WATER_TABLE, outline_fragment, place_ramps, smooth_region

FRAG = 5
START_FREE_WATER = 2      # no water within this Chebyshev cell radius of a start
START_FREE_PLATEAU = 3    # no plateau within this radius


@dataclass
class Layout:
    water: np.ndarray
    plateau: np.ndarray
    ramps: dict = field(default_factory=dict)     # (cx, cy) -> ramp fragment name
    starts: list = field(default_factory=list)    # start points in fields (x, y)


def _dilate(mask: np.ndarray, r: int = 1) -> np.ndarray:
    h, w = mask.shape
    p = np.pad(mask, r, constant_values=False)
    out = np.zeros_like(mask)
    for dy in range(-r, r + 1):
        for dx in range(-r, r + 1):
            out |= p[r + dy:r + dy + h, r + dx:r + dx + w]
    return out


def _disc(shape, cx: float, cy: float, r: float) -> np.ndarray:
    ys, xs = np.mgrid[0:shape[0], 0:shape[1]]
    return (xs - cx) ** 2 + (ys - cy) ** 2 <= r * r


def _start_zone(shape, starts_cells, r: int) -> np.ndarray:
    z = np.zeros(shape, bool)
    for cx, cy in starts_cells:
        z[max(0, cy - r):cy + r + 1, max(0, cx - r):cx + r + 1] = True
    return z


def _blocked(lay: Layout) -> np.ndarray:
    """Cells land units cannot cross at cell level: water and plateau walls (ramps are open)."""
    wall = np.zeros_like(lay.plateau)
    h, w = wall.shape
    for cy in range(h):
        for cx in range(w):
            if lay.plateau[cy, cx] and outline_fragment(lay.plateau, cx, cy, PLATEAU_TABLE):
                wall[cy, cx] = (cx, cy) not in lay.ramps
    return lay.water | wall


def cells_connected(lay: Layout) -> bool:
    blocked = _blocked(lay)
    cells = [(x // FRAG, y // FRAG) for x, y in lay.starts]
    h, w = blocked.shape
    seen = np.zeros_like(blocked)
    q = deque([cells[0]])
    seen[cells[0][1], cells[0][0]] = True
    while q:
        cx, cy = q.popleft()
        for nx, ny in ((cx + 1, cy), (cx - 1, cy), (cx, cy + 1), (cx, cy - 1)):
            if 0 <= nx < w and 0 <= ny < h and not blocked[ny, nx] and not seen[ny, nx]:
                seen[ny, nx] = True
                q.append((nx, ny))
    return all(seen[cy, cx] for cx, cy in cells)


def _place_starts(rng, cells: int) -> list:
    lo, hi = 5, cells - 6
    corners = [(lo, lo), (hi, lo), (lo, hi), (hi, hi)]
    out = []
    for cx, cy in corners:
        jx, jy = rng.integers(-2, 3, size=2)
        out.append((int(cx + jx), int(cy + jy)))
    return out


def _lakes(rng, shape, starts_cells) -> np.ndarray:
    h, w = shape
    water = np.zeros(shape, bool)
    # central lake: a few overlapping discs around the centre
    cx0, cy0 = w / 2 + rng.uniform(-2, 2), h / 2 + rng.uniform(-2, 2)
    for _ in range(int(rng.integers(3, 6))):
        water |= _disc(shape, cx0 + rng.uniform(-3, 3), cy0 + rng.uniform(-3, 3), rng.uniform(2.5, 4.5))
    # smaller lakes between the bases
    for _ in range(int(rng.integers(2, 4))):
        for _try in range(30):
            x, y = rng.uniform(3, w - 4), rng.uniform(3, h - 4)
            if min(max(abs(x - sx), abs(y - sy)) for sx, sy in starts_cells) >= START_FREE_WATER + 4:
                water |= _disc(shape, x, y, rng.uniform(1.8, 3.0))
                break
    water &= ~_start_zone(shape, starts_cells, START_FREE_WATER)
    return smooth_region(water, WATER_TABLE)


def _plateaus(rng, shape, water, starts_cells) -> np.ndarray:
    h, w = shape
    plateau = np.zeros(shape, bool)
    forbidden = _dilate(water, 2) | _start_zone(shape, starts_cells, START_FREE_PLATEAU)
    placed = 0
    for _try in range(200):
        if placed >= int(rng.integers(3, 6)):
            break
        pw, ph = int(rng.integers(4, 8)), int(rng.integers(4, 8))
        x, y = int(rng.integers(1, w - pw - 1)), int(rng.integers(1, h - ph - 1))
        cand = np.zeros(shape, bool)
        cand[y:y + ph, x:x + pw] = True
        if rng.random() < 0.5:           # L-shape / irregular: union with a second rectangle
            qw, qh = int(rng.integers(3, 6)), int(rng.integers(3, 6))
            qx = int(np.clip(x + rng.integers(-2, pw), 1, w - qw - 1))
            qy = int(np.clip(y + rng.integers(-2, ph), 1, h - qh - 1))
            cand[qy:qy + qh, qx:qx + qw] = True
        if (cand & forbidden).any() or (cand & _dilate(plateau, 2)).any():
            continue
        plateau |= cand
        placed += 1
    plateau = smooth_region(plateau, PLATEAU_TABLE)
    plateau &= ~_dilate(water, 1)
    return smooth_region(plateau, PLATEAU_TABLE)


def _components(mask: np.ndarray) -> list:
    seen = np.zeros_like(mask)
    comps = []
    h, w = mask.shape
    for cy in range(h):
        for cx in range(w):
            if mask[cy, cx] and not seen[cy, cx]:
                comp = np.zeros_like(mask)
                q = deque([(cx, cy)])
                seen[cy, cx] = True
                while q:
                    x, y = q.popleft()
                    comp[y, x] = True
                    for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                        if 0 <= nx < w and 0 <= ny < h and mask[ny, nx] and not seen[ny, nx]:
                            seen[ny, nx] = True
                            q.append((nx, ny))
                comps.append(comp)
    return comps


def make_layout(seed: int, cells: int = 32) -> Layout:
    shape = (cells, cells)
    for attempt in range(20):
        rng = np.random.default_rng(seed * 1000 + attempt)
        starts_cells = _place_starts(rng, cells)
        water = _lakes(rng, shape, starts_cells)
        plateau = _plateaus(rng, shape, water, starts_cells)
        ramps: dict = {}
        for comp in _components(plateau):
            ramps.update(place_ramps(comp, rng, int(rng.integers(1, 3))))
        lay = Layout(water, plateau, ramps, [(cx * FRAG + 2, cy * FRAG + 2) for cx, cy in starts_cells])
        # drop plateaus until the bases are connected
        comps = _components(plateau)
        while not cells_connected(lay) and comps:
            comp = comps.pop(int(rng.integers(len(comps))))
            lay.plateau = lay.plateau & ~comp
            lay.ramps = {k: v for k, v in lay.ramps.items() if not comp[k[1], k[0]]}
        if cells_connected(lay) and lay.water.sum() >= 40 and lay.plateau.sum() >= 30 and len(lay.ramps) >= 4:
            return lay
    raise RuntimeError(f"no valid layout for seed {seed}")


def layout_grid(lay: Layout) -> np.ndarray:
    """Fragment names [cy, cx] for the earth segment."""
    h, w = lay.water.shape
    grid = np.full((h, w), "grass", dtype=object)
    for cy in range(h):
        for cx in range(w):
            if lay.water[cy, cx]:
                grid[cy, cx] = outline_fragment(lay.water, cx, cy, WATER_TABLE) or "sea"
            elif lay.plateau[cy, cx]:
                grid[cy, cx] = lay.ramps.get((cx, cy)) or outline_fragment(lay.plateau, cx, cy, PLATEAU_TABLE) \
                    or "grass"
    return grid

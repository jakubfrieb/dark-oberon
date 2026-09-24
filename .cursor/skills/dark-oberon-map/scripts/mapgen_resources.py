"""
Resource placement (fields): per base one goldmine, one coal pit and the same number of
forests nearby; contested goldmines and scattered forest clusters away from all bases.
Footprints (goldmine/coal 4x4, forest 3x3) lie fully on walkable, reachable terrain and never
touch each other (1 field gap).
"""

from __future__ import annotations

from collections import deque

import numpy as np

from map_check import BALANCE_RADIUS, SIZES

BASE_FREE = 9             # nothing within this radius (Chebyshev, footprint centre) of a start
BASE_GOLD = (11, 16)
BASE_COAL = (12, 19)
BASE_FORESTS = 24         # forests within BASE_FOREST_R of every start (exactly -> balanced)
BASE_FOREST_R = (10, 21)
FAR_FROM_BASES = BALANCE_RADIUS + 4
CONTESTED_GOLD = (2, 3)
TOTAL_FORESTS = 330

GOLD_BASE_AMOUNT = 35000
GOLD_CONTESTED_AMOUNT = 40000
COAL_AMOUNT = 8000
FOREST_AMOUNT = 500


def _reachable(walk: np.ndarray, start) -> np.ndarray:
    seen = np.zeros_like(walk)
    h, w = walk.shape
    q = deque([start])
    seen[start[1], start[0]] = True
    while q:
        x, y = q.popleft()
        for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
            if 0 <= nx < w and 0 <= ny < h and walk[ny, nx] and not seen[ny, nx]:
                seen[ny, nx] = True
                q.append((nx, ny))
    return seen


class _Placer:
    def __init__(self, walk, starts):
        self.ok = _reachable(walk, starts[0]) & walk
        self.occ = np.zeros_like(walk)
        self.starts = starts
        self.out = []

    def fits(self, sid, x, y) -> bool:
        s = SIZES[sid]
        h, w = self.ok.shape
        if x < 1 or y < 1 or x + s >= w or y + s >= h:
            return False
        if not self.ok[y:y + s, x:x + s].all() or self.occ[y - 1:y + s + 1, x - 1:x + s + 1].any():
            return False
        cx, cy = x + s / 2, y + s / 2
        return all(max(abs(cx - sx), abs(cy - sy)) > BASE_FREE for sx, sy in self.starts)

    def put(self, sid, x, y, amount, life=100):
        s = SIZES[sid]
        self.occ[y:y + s, x:x + s] = True
        self.out.append((sid, int(x), int(y), life, amount))

    def ring(self, rng, sid, centre, rmin, rmax, amount, tries=400, extra=None) -> bool:
        s = SIZES[sid]
        for _ in range(tries):
            a, r = rng.uniform(0, 2 * np.pi), rng.uniform(rmin, rmax)
            x = int(round(centre[0] + r * np.cos(a) - s / 2))
            y = int(round(centre[1] + r * np.sin(a) - s / 2))
            if self.fits(sid, x, y) and (extra is None or extra(x, y)):
                self.put(sid, x, y, amount)
                return True
        return False


def _dist(a, b) -> float:
    return float(np.hypot(a[0] - b[0], a[1] - b[1]))


def _cluster(placer, rng, centre, n, keep) -> int:
    """Up to n forests on a 4-field lattice around centre; keep(x, y) filters positions."""
    offs = [(dx, dy) for dx in range(-12, 13, 4) for dy in range(-12, 13, 4)]
    offs.sort(key=lambda o: o[0] ** 2 + o[1] ** 2 + rng.uniform(0, 30))
    placed = 0
    for dx, dy in offs:
        if placed >= n:
            break
        x, y = int(centre[0] + dx), int(centre[1] + dy)
        if placer.fits("forest", x, y) and keep(x, y):
            placer.put("forest", x, y, FOREST_AMOUNT)
            placed += 1
    return placed


def place_resources(layout, walk: np.ndarray, rng) -> list:
    starts = layout.starts
    p = _Placer(walk, starts)
    for st in starts:
        if not p.ring(rng, "goldmine", st, *BASE_GOLD, GOLD_BASE_AMOUNT):
            raise RuntimeError(f"no room for base goldmine at {st}")
        if not p.ring(rng, "coal", st, *BASE_COAL, COAL_AMOUNT):
            raise RuntimeError(f"no room for coal at {st}")
    for st in starts:
        def near(x, y, st=st):
            d = _dist((x + 1.5, y + 1.5), st)
            return BASE_FOREST_R[0] <= d <= BASE_FOREST_R[1]
        have = 0
        for _ in range(60):
            if have >= BASE_FORESTS:
                break
            a = rng.uniform(0, 2 * np.pi)
            r = rng.uniform(BASE_FOREST_R[0] + 2, BASE_FOREST_R[1] - 2)
            centre = (st[0] + r * np.cos(a), st[1] + r * np.sin(a))
            have += _cluster(p, rng, centre, min(10, BASE_FORESTS - have), near)
        if have < BASE_FORESTS:
            raise RuntimeError(f"only {have} forests near base {st}")

    def far(x, y, s=4):
        c = (x + s / 2, y + s / 2)
        return all(_dist(c, st) >= FAR_FROM_BASES for st in starts)

    want = int(rng.integers(CONTESTED_GOLD[0], CONTESTED_GOLD[1] + 1))
    h, w = walk.shape
    got = 0
    for _ in range(2000):
        if got >= want:
            break
        x, y = int(rng.integers(2, w - 6)), int(rng.integers(2, h - 6))
        if p.fits("goldmine", x, y) and far(x, y) and all(
                _dist((x, y), (q[1], q[2])) > 30 for q in p.out if q[0] == "goldmine"):
            p.put("goldmine", x, y, GOLD_CONTESTED_AMOUNT)
            got += 1
    forests = sum(1 for q in p.out if q[0] == "forest")
    for _ in range(400):
        if forests >= TOTAL_FORESTS:
            break
        centre = (rng.uniform(4, w - 4), rng.uniform(4, h - 4))
        forests += _cluster(p, rng, centre, int(rng.integers(5, 13)), lambda x, y: far(x, y, 3))
    return p.out

import numpy as np
import pytest

from mapgen_layout import cells_connected, make_layout
from mapgen_tiles import PLATEAU_TABLE, WATER_TABLE, outline_fragment

SEEDS = [1, 2, 3, 7, 42]


def test_deterministic_for_seed():
    a, b = make_layout(5), make_layout(5)
    assert (a.water == b.water).all() and (a.plateau == b.plateau).all() and a.starts == b.starts
    c = make_layout(6)
    assert not ((a.water == c.water).all() and (a.plateau == c.plateau).all())


@pytest.mark.parametrize("seed", SEEDS)
def test_has_lakes_and_plateaus(seed):
    lay = make_layout(seed)
    assert lay.water.sum() >= 40 and lay.plateau.sum() >= 30
    assert len(lay.ramps) >= 4


@pytest.mark.parametrize("seed", SEEDS)
def test_plateau_keeps_distance_from_water(seed):
    lay = make_layout(seed)
    h, w = lay.water.shape
    for cy, cx in zip(*np.nonzero(lay.plateau)):
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                y, x = cy + dy, cx + dx
                if 0 <= y < h and 0 <= x < w:
                    assert not lay.water[y, x], (cx, cy)


@pytest.mark.parametrize("seed", SEEDS)
def test_regions_are_tileable(seed):
    lay = make_layout(seed)
    for cy in range(32):
        for cx in range(32):
            outline_fragment(lay.water, cx, cy, WATER_TABLE)
            outline_fragment(lay.plateau, cx, cy, PLATEAU_TABLE)


@pytest.mark.parametrize("seed", SEEDS)
def test_starts_free_spread_and_connected(seed):
    lay = make_layout(seed)
    assert len(lay.starts) == 4
    for x, y in lay.starts:
        cx, cy = x // 5, y // 5
        assert not lay.water[cy - 2:cy + 3, cx - 2:cx + 3].any()
        assert not lay.plateau[cy - 3:cy + 4, cx - 3:cx + 4].any()
    for i, a in enumerate(lay.starts):
        for b in lay.starts[i + 1:]:
            assert max(abs(a[0] - b[0]), abs(a[1] - b[1])) >= 70
    assert cells_connected(lay)

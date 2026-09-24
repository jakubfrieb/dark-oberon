import numpy as np
import pytest

from mapgen_tiles import (PLATEAU_TABLE, WATER_TABLE, fragment_ids, outline_fragment, place_ramps,
                          smooth_region, ug_name)
from tests.mapgen.conftest import REPO

SCH = REPO / "schemes" / "plastic.sch"


def square(n=12, x0=3, y0=3, w=6, h=6):
    m = np.zeros((n, n), bool)          # indexed [cy, cx]
    m[y0:y0 + h, x0:x0 + w] = True
    return m


@pytest.mark.parametrize("table,expect", [
    (WATER_TABLE, {(3, 3): "coast_en", (5, 3): "coast_n", (8, 3): "coast_wn", (3, 5): "coast_e",
                   (8, 5): "coast_w", (3, 8): "coast_es", (5, 8): "coast_s", (8, 8): "coast_ws", (5, 5): None}),
    (PLATEAU_TABLE, {(3, 3): "rocks_sw", (5, 3): "rocks_s", (8, 3): "rocks_se", (3, 5): "rocks_w",
                     (8, 5): "rocks_e", (3, 8): "rocks_nw", (5, 8): "rocks_n", (8, 8): "rocks_ne", (5, 5): None}),
])
def test_outline_of_square(table, expect):
    m = square()
    for (cx, cy), name in expect.items():
        assert outline_fragment(m, cx, cy, table) == name, (cx, cy)


@pytest.mark.parametrize("table,expect", [
    (WATER_TABLE, {"NW": "coast_ne", "NE": "coast_nw", "SW": "coast_se", "SE": "coast_sw"}),
    (PLATEAU_TABLE, {"NW": "rocks_ws", "NE": "rocks_es", "SW": "rocks_wn", "SE": "rocks_en"}),
])
def test_inner_corners(table, expect):
    # 10x10 region with a 2x2 notch cut from each corner -> concave corners next to the notches
    m = square(16, 3, 3, 10, 10)
    m[3:5, 3:5] = m[3:5, 11:13] = m[11:13, 3:5] = m[11:13, 11:13] = False
    assert outline_fragment(m, 5, 5, table) == expect["NW"]
    assert outline_fragment(m, 10, 5, table) == expect["NE"]
    assert outline_fragment(m, 5, 10, table) == expect["SW"]
    assert outline_fragment(m, 10, 10, table) == expect["SE"]


def test_outside_cell_is_none():
    assert outline_fragment(square(), 0, 0, WATER_TABLE) is None


def test_smooth_removes_thin_necks():
    m = square(16, 1, 1, 5, 5) | square(16, 9, 1, 5, 5)
    m[3, 6:9] = True                     # 1-cell neck between two blobs
    s = smooth_region(m)
    for cy in range(16):
        for cx in range(16):
            if s[cy, cx]:
                n, e, so, w = s[cy - 1, cx] if cy else False, s[cy, cx + 1] if cx < 15 else False, \
                    s[cy + 1, cx] if cy < 15 else False, s[cy, cx - 1] if cx else False
                assert not ((not n and not so) or (not e and not w)), (cx, cy)


def test_smooth_removes_saddles():
    m = square(16, 1, 1, 5, 5) | square(16, 6, 6, 5, 5)   # touching only diagonally at (5,5)-(6,6)
    s = smooth_region(m)
    for cy in range(15):
        for cx in range(15):
            q = s[cy:cy + 2, cx:cx + 2]
            assert not (q[0, 0] and q[1, 1] and not q[0, 1] and not q[1, 0])
            assert not (q[0, 1] and q[1, 0] and not q[0, 0] and not q[1, 1])


def test_smooth_keeps_every_cell_tileable():
    rng = np.random.default_rng(3)
    m = rng.random((32, 32)) > 0.55
    s = smooth_region(m)
    for cy in range(32):
        for cx in range(32):
            if s[cy, cx]:
                outline_fragment(s, cx, cy, WATER_TABLE)   # raises when no fragment fits


def test_place_ramps_pairs_on_straight_edges():
    m = square(16, 2, 2, 10, 8)
    ramps = place_ramps(m, np.random.default_rng(1), 2)
    names = set(ramps.values())
    assert len(ramps) == 4
    pairs = [{"rocks_s_end_e", "rocks_s_end_w"}, {"rocks_n_end_e", "rocks_n_end_w"},
             {"rocks_w_end_n", "rocks_w_end_s"}, {"rocks_e_end_n", "rocks_e_end_s"}]
    assert sum(p <= names for p in pairs) == 2
    for (cx, cy) in ramps:
        assert outline_fragment(m, cx, cy, PLATEAU_TABLE) in {"rocks_s", "rocks_n", "rocks_w", "rocks_e"}


def test_fragment_ids_and_underground():
    ids1 = fragment_ids(SCH, 1)
    assert len(ids1) >= 79 and ids1["sea"] == 29 and ids1["coast_n"] == 30
    ids0 = fragment_ids(SCH, 0)
    for n in ("coast_n", "coast_wn", "sea", "grass", "rocks_s"):
        assert ug_name(n) in ids0

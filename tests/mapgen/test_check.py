import numpy as np

from map_check import (check_adjacency, check_connectivity, check_resources, learn_adjacency, read_map,
                       walkable_grid)
from tests.mapgen.conftest import REPO

SCH = REPO / "schemes" / "plastic.sch"
HANDMADE = [REPO / "maps" / m for m in ("sunnybay.map", "virgin_editor.map", "trial.map")]


def test_read_map_fragment_grid_and_sources():
    m = read_map(REPO / "maps" / "sunnybay.map", SCH)
    assert m.width == 200 and m.grid.shape == (40, 40)
    assert m.grid[0, 0] and isinstance(m.grid[0, 0], str)
    assert sum(1 for s in m.sources if s[0] == "goldmine") == 8
    assert len(m.starts) == 5


def test_adjacency_check_accepts_handmade_map():
    allowed = learn_adjacency(HANDMADE, SCH)
    m = read_map(REPO / "maps" / "sunnybay.map", SCH)
    assert check_adjacency(m.grid, allowed) == []


def test_adjacency_check_flags_bad_pair():
    allowed = learn_adjacency(HANDMADE, SCH)
    g = np.full((3, 3), "grass", dtype=object)
    g[1, 1] = "coast_n"
    g[1, 2] = "rocks_e"
    errs = check_adjacency(g, allowed)
    assert any("coast_n" in e and "rocks_e" in e for e in errs)


def test_walkable_and_connectivity():
    g = np.full((4, 4), "grass", dtype=object)
    g[:, 2] = "sea"                                    # water column splits the map
    walk = walkable_grid(g, SCH, [])
    assert walk.shape == (20, 20) and walk[2, 2] and not walk[2, 12]
    assert check_connectivity(walk, [(2, 2), (17, 2)])
    g[3, 2] = "grass"                                  # a land bridge at the bottom
    walk = walkable_grid(g, SCH, [])
    assert check_connectivity(walk, [(2, 2), (17, 2)]) == []


def test_resources_overlap_and_balance():
    starts = [(20, 20), (140, 20)]
    good = [("goldmine", 32, 20, 100, 35000), ("goldmine", 128, 20, 100, 35000)]
    assert check_resources(good, starts) == []
    overlap = good + [("forest", 33, 21, 100, 500)]
    assert any("overlap" in e for e in check_resources(overlap, starts))
    unbalanced = [("goldmine", 32, 20, 100, 35000)]
    assert any("balance" in e for e in check_resources(unbalanced, starts))


def test_adjacency_accepts_unseen_pair_with_matching_edges():
    allowed = learn_adjacency(HANDMADE, SCH)
    assert ("rocks_e", "S", "rocks_e_end_n") not in allowed
    g = np.full((2, 1), "grass", dtype=object)
    g[0, 0], g[1, 0] = "rocks_e", "rocks_e_end_n"
    assert check_adjacency(g, allowed, SCH) == []


def test_adjacency_rejects_unseen_full_rock_pair():
    allowed = learn_adjacency(HANDMADE, SCH)
    assert ("rocks_s", "E", "rocks_w") not in allowed
    g = np.full((1, 2), "grass", dtype=object)
    g[0, 0], g[0, 1] = "rocks_s", "rocks_w"
    assert check_adjacency(g, allowed, SCH)

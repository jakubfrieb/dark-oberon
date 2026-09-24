import numpy as np
import pytest

from map_check import SIZES, check_resources, walkable_grid
from mapgen_layout import layout_grid, make_layout
from mapgen_resources import place_resources
from tests.mapgen.conftest import REPO

SCH = REPO / "schemes" / "plastic.sch"


@pytest.fixture(scope="module", params=[1, 2, 3])
def placed(request):
    lay = make_layout(request.param)
    grid = layout_grid(lay)
    walk = walkable_grid(grid, SCH, [])
    ground = walkable_grid(grid, SCH, [], max_layer=20)
    return lay, walk, place_resources(lay, walk, np.random.default_rng(request.param), ground)


def test_counts(placed):
    lay, walk, res = placed
    kinds = [r[0] for r in res]
    assert kinds.count("goldmine") >= 4 + 2
    assert kinds.count("coal") >= 4
    assert 250 <= kinds.count("forest") <= 450


def test_on_walkable_terrain(placed):
    lay, walk, res = placed
    for sid, x, y, _l, _a in res:
        s = SIZES[sid]
        assert walk[y:y + s, x:x + s].all(), (sid, x, y)


def test_no_overlap_and_balanced(placed):
    lay, walk, res = placed
    assert check_resources(res, lay.starts) == []


def test_bases_kept_free(placed):
    lay, walk, res = placed
    for sid, x, y, _l, _a in res:
        s = SIZES[sid]
        for sx, sy in lay.starts:
            assert max(abs(x + s / 2 - sx), abs(y + s / 2 - sy)) > 6


def test_every_start_has_gold_and_wood_nearby(placed):
    lay, walk, res = placed
    for sx, sy in lay.starts:
        near = [r for r in res if (r[1] - sx) ** 2 + (r[2] - sy) ** 2 <= 22 ** 2]
        assert any(r[0] == "goldmine" for r in near)
        assert sum(r[0] == "forest" for r in near) >= 16


def test_sources_only_where_the_engine_allows_them_and_ramps_stay_open(placed):
    lay, walk, res = placed
    grid = layout_grid(lay)
    ground = walkable_grid(grid, SCH, [], max_layer=20)      # engine: sources on layers 10..20
    for sid, x, y, _l, _a in res:
        s = SIZES[sid]
        assert ground[y:y + s, x:x + s].all(), (sid, x, y)
        for (cx, cy) in lay.ramps:
            assert not (cx - 1 <= (x + s / 2) // 5 <= cx + 1 and cy - 1 <= (y + s / 2) // 5 <= cy + 1), (sid, x, y)

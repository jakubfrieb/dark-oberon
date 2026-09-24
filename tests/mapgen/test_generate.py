import re
import subprocess

from generate_map import RACES, generate
from map_check import check_map, read_map
from tests.mapgen.conftest import REPO, SCRIPTS

SCH = REPO / "schemes" / "plastic.sch"


def test_generated_map_passes_all_checks(tmp_path):
    out = tmp_path / "gen.map"
    generate(1, out, name="Gen test")
    assert check_map(out) == []
    r = subprocess.run(["python3", str(SCRIPTS / "validate_map.py"), str(out)], capture_output=True, text=True)
    assert r.returncode == 0, r.stdout + r.stderr


def test_header_players_and_races(tmp_path):
    out = tmp_path / "gen.map"
    generate(2, out, name="Gen test")
    text = out.read_text(encoding="latin-1")
    assert re.search(r'^name "Gen test"', text, re.M) and "width 160" in text and 'scheme "plastic"' in text
    assert "max_count 4" in text and len(re.findall(r"start_point_\d+ ", text)) == 4
    assert all(f'name "{r}"' in text for r in RACES) and len(RACES) == 6
    assert text.count('"townhall" 0 0') == 6


def test_underground_mirrors_water_and_air_is_empty(tmp_path):
    out = tmp_path / "gen.map"
    generate(3, out)
    text = out.read_text(encoding="latin-1")
    for seg in (0, 1, 2):
        s = text[text.index(f"<Segment {seg}>"):text.index(f"</Segment {seg}>")]
        assert s.count("fragment_") == 32 * 32
    m = read_map(out, SCH)
    seg0 = text[text.index("<Segment 0>"):text.index("</Segment 0>")]
    from mapgen_tiles import fragment_ids, ug_name
    inv0 = {v: k for k, v in fragment_ids(SCH, 0).items()}
    g0 = {(int(x) // 5, int(y) // 5): inv0[int(f)] for f, x, y in re.findall(r"fragment_\d+ (\d+) (\d+) (\d+)", seg0)}
    for (cx, cy), name in g0.items():
        assert name == ug_name(m.grid[cy, cx])
    seg2 = text[text.index("<Segment 2>"):text.index("</Segment 2>")]
    assert set(re.findall(r"fragment_\d+ (\d+) ", seg2)) == {"0"}


def test_deterministic(tmp_path):
    a, b = tmp_path / "a.map", tmp_path / "b.map"
    generate(4, a)
    generate(4, b)
    assert a.read_bytes() == b.read_bytes()


def test_render_preview(tmp_path):
    from render_map import render
    out = tmp_path / "gen.map"
    generate(1, out)
    png = tmp_path / "p.png"
    render(out, png, scale=0.25)
    from PIL import Image
    im = Image.open(png)
    assert im.width > 1000 and im.height > 500

import json
from pathlib import Path

import numpy as np
from PIL import Image

from attack_anim import (build_sheet, compose_frames, feet_anchor, key_magenta, patch_group,
                         shadow_and_figure, split_grid)

MAGENTA = (255, 0, 255)


def stay_frame():
    """64x64 idle frame: figure 10 wide x 20 tall standing at feet (32, 50) + grey ground shadow."""
    a = np.zeros((64, 64, 4), np.uint8)
    a[48:52, 18:32] = (0, 0, 0, 120)             # shadow on the ground (left of the feet)
    a[31:51, 27:37] = (60, 140, 50, 255)         # figure
    a[30, 27:37] = (90, 20, 20, 200)             # dark saturated outline pixel row (not shadow)
    return Image.fromarray(a, "RGBA")


def grid_cell(offset_x=0, sword=False):
    """512x512 magenta cell with a 100x200 figure, feet at bottom 400."""
    c = Image.new("RGB", (512, 512), MAGENTA)
    a = np.array(c)
    a[200:400, 206 + offset_x:306 + offset_x] = (60, 140, 50)
    if sword:
        a[250:270, 60:206 + offset_x] = (180, 180, 180)
    return Image.fromarray(a, "RGB")


def grid(cells):
    g = Image.new("RGB", (1024, 1024), MAGENTA)
    for i, c in enumerate(cells):
        g.paste(c, ((i % 2) * 512, (i // 2) * 512))
    return g


def test_key_magenta_makes_background_transparent_and_keeps_figure():
    out = key_magenta(grid_cell())
    assert out.getpixel((5, 5))[3] == 0
    assert out.getpixel((250, 300)) == (60, 140, 50, 255)


def test_key_magenta_removes_pink_spill_on_edges():
    img = Image.new("RGB", (4, 1), MAGENTA)
    img.putpixel((1, 0), (200, 90, 200))          # half magenta, half figure
    r, g, b, a = key_magenta(img).getpixel((1, 0))
    assert 0 < a < 255 and r <= g + 5 and b <= g + 5


def test_split_grid_reading_order():
    g = grid([grid_cell(i * 10) for i in range(4)])
    cells = split_grid(g, 2, 2)
    assert len(cells) == 4 and cells[0].size == (512, 512)
    assert np.array(cells[3])[300, 206 + 30].tolist() == [60, 140, 50]


def test_shadow_excludes_saturated_outline():
    shadow, figure = shadow_and_figure(stay_frame())
    assert shadow[49, 20] and not shadow[30, 30] and figure[40, 30]


def test_feet_anchor():
    m = np.zeros((64, 64), bool)
    m[31:51, 27:37] = True
    x, y, h = feet_anchor(m)
    assert abs(x - 31.5) < 0.01 and y == 50 and h == 20


def test_compose_frames_same_size_and_feet_as_idle():
    g = key_magenta(grid([grid_cell(0, sword=(i == 2)) for i in range(4)]))
    frames, scale = compose_frames(split_grid(g, 2, 2), stay_frame(), ref_index=3)
    assert len(frames) == 4 and all(f.size == (64, 64) for f in frames)
    assert abs(scale - 0.1) < 0.01                               # 200 px figure -> 20 px like idle
    for f in frames:
        m = np.asarray(f)[..., 3] >= 250
        ys, xs = np.nonzero(m & (np.asarray(f)[..., 1] > 100))
        assert abs(ys.max() - 50) <= 1                            # feet on the idle ground line
    assert np.asarray(frames[0])[49, 20, 3] == 120                # idle shadow kept
    assert np.asarray(frames[2])[..., 3].astype(bool).sum() > np.asarray(frames[0])[..., 3].astype(bool).sum()


def test_build_sheet_horizontal():
    s = build_sheet([Image.new("RGBA", (64, 64), (i, 0, 0, 255)) for i in range(4)])
    assert s.size == (256, 64) and s.getpixel((64 * 3 + 1, 1))[0] == 3


def test_patch_group_updates_manifest_and_texture(tmp_path):
    u = tmp_path / "u"; (u / "textures").mkdir(parents=True)
    Image.new("RGBA", (64, 64)).save(u / "textures/g1_t000_x_attack__x_attack1.tga", "TGA")
    m = {"format": "dark_oberon_dat", "version": 3, "texture_groups": [
        {"name": "x_attack", "textures": [{"id": "x_attack1", "hcount": 1, "vcount": 1, "atime": 0,
                                           "pointx": 32, "pointy": 10, "ttype": 0,
                                           "file": "textures/g1_t000_x_attack__x_attack1.tga"}]}], "sounds": []}
    (u / "manifest.json").write_text(json.dumps(m))
    sheet = Image.new("RGBA", (256, 64), (220, 30, 30, 255))
    patch_group(u, "x_attack", {1: sheet}, atime=1000,
                mapping={"hue": 220.0, "sat_scale": 1.0, "val_scale": 1.0})
    t = json.loads((u / "manifest.json").read_text())["texture_groups"][0]["textures"][0]
    assert (t["hcount"], t["vcount"], t["atime"], t["pointx"]) == (4, 1, 1000, 32)
    img = Image.open(u / t["file"]).convert("RGBA")
    assert img.size == (256, 64)
    r, g, b, a = img.getpixel((10, 10))
    assert b > r                                                  # recoloured to blue team colour


def test_generate_passes_absolute_image_paths(tmp_path, monkeypatch):
    import argparse, os
    import attack_anim
    import run_codex_board_batch
    work = tmp_path / "w"; (work / "in").mkdir(parents=True)
    Image.new("RGB", (8, 8)).save(work / "in" / "dir1.png")
    ref = tmp_path / "ref.png"; Image.new("RGB", (8, 8)).save(ref)
    seen = []

    def fake_run(prompt, images, expect, cwd, **kw):
        seen.append((images, expect, cwd))
        return True
    monkeypatch.setattr(run_codex_board_batch, "run_codex", fake_run)
    monkeypatch.chdir(tmp_path)
    a = argparse.Namespace(work=Path("w"), reference=Path("ref.png"), subject="x", parallel=1, only=None, force=False)
    assert attack_anim.cmd_generate(a) == 0
    images, expect, cwd = seen[0]
    assert all(Path(p).is_absolute() and Path(p).exists() for p in images)
    assert Path(expect).is_absolute() and Path(cwd).is_absolute()


def test_prompt_contains_view_hint_for_direction():
    from attack_anim import build_prompt, parse_views
    views = parse_views("5:back-left,6:back")
    assert views == {5: "back-left", 6: "back"}
    p = build_prompt("an orc", 6, views)
    assert "seen from the BACK" in p and "./raw/dir6.png" in p
    assert "seen from the" not in build_prompt("an orc", 2, views)

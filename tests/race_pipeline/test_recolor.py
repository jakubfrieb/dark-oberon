import subprocess

import numpy as np
from PIL import Image

from recolor_race import calibrate, recolor_image, recolor_unpacked, team_mask
from tests.race_pipeline.conftest import REPO, SCRIPTS

BLUE = {"hue": 220.0, "sat_scale": 1.0, "val_scale": 1.0}


def px(img, xy):
    return img.getpixel(xy)


def sample():
    a = np.zeros((1, 4, 4), np.uint8)
    a[0, 0] = (220, 30, 30, 255)    # team red
    a[0, 1] = (70, 140, 50, 255)    # orc green
    a[0, 2] = (120, 80, 40, 255)    # brown leather
    a[0, 3] = (150, 150, 150, 128)  # grey, semi transparent
    return Image.fromarray(a, "RGBA")


def test_mask_only_red():
    m = team_mask(np.asarray(sample().convert("RGB")))
    assert m.tolist() == [[True, False, False, False]]


def test_recolor_red_to_blue():
    out = recolor_image(sample(), BLUE)
    r, g, b, a = px(out, (0, 0))
    assert b > r and b > g and a == 255


def test_recolor_keeps_green_and_brown():
    src, out = sample(), recolor_image(sample(), BLUE)
    for x in (1, 2, 3):
        assert px(out, (x, 0)) == px(src, (x, 0))


def test_calibrate_on_synthetic(tmp_path):
    for name, col in (("red", (220, 30, 30)), ("blue", (30, 30, 220))):
        d = tmp_path / name / "textures"; d.mkdir(parents=True)
        a = np.zeros((2, 2, 4), np.uint8); a[..., 3] = 255
        a[0, 0, :3] = col; a[1, 1, :3] = (70, 140, 50)
        Image.fromarray(a, "RGBA").save(d / "t.tga")
    m = calibrate(tmp_path / "red", tmp_path / "blue")
    assert 230 <= m["hue"] <= 250


def test_pil_tga_roundtrip_header_matches(tmp_path):
    """PIL-written TGA must keep origin + alpha bits the engine expects."""
    unpacked = tmp_path / "u"
    subprocess.run(["python3", str(SCRIPTS / "do_dat_tool.py"), "unpack",
                    str(REPO / "races/human-red/human-red.dat"), "-o", str(unpacked)],
                   check=True, capture_output=True)
    src = sorted((unpacked / "textures").glob("*footman_stay*.tga"))[0]
    dst = tmp_path / "copy.tga"
    Image.open(src).save(dst, "TGA")
    h1, h2 = src.read_bytes()[:18], dst.read_bytes()[:18]
    assert h1[2] in (2, 10) and h2[2] in (2, 10)      # truecolor (raw or RLE)
    assert h1[16] == h2[16]                           # bits per pixel
    # engine (src/tga.cpp) reads alpha from bpp and ignores descriptor alpha bits;
    # only the origin bits (4-5) decide flipping
    assert (h1[17] & 0x30) == (h2[17] & 0x30)


def test_recolor_unpacked_copies_everything(tmp_path):
    src = tmp_path / "src"; (src / "textures").mkdir(parents=True); (src / "sounds").mkdir()
    (src / "manifest.json").write_text("{}"); (src / "sounds/s.ogg").write_bytes(b"x")
    sample().save(src / "textures/t.tga")
    n = recolor_unpacked(src, tmp_path / "dst", BLUE)
    assert n == 1
    assert (tmp_path / "dst/manifest.json").exists() and (tmp_path / "dst/sounds/s.ogg").exists()
    assert px(Image.open(tmp_path / "dst/textures/t.tga").convert("RGBA"), (0, 0))[2] > 150


def test_calibrate_matches_textures_by_group_and_id_not_index(tmp_path):
    for name, col, fname in (("red", (220, 30, 30), "g020_t000_x_stay__x1.tga"),
                             ("blue", (30, 30, 220), "g021_t000_x_stay__x1.tga")):
        d = tmp_path / name / "textures"; d.mkdir(parents=True)
        a = np.zeros((2, 2, 4), np.uint8); a[..., 3] = 255
        a[0, 0, :3] = col
        Image.fromarray(a, "RGBA").save(d / fname)
    m = calibrate(tmp_path / "red", tmp_path / "blue")
    assert 230 <= m["hue"] <= 250

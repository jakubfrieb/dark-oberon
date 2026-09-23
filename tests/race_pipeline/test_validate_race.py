import shutil
import subprocess

from PIL import Image

from tests.race_pipeline.conftest import REPO, SCRIPTS
from validate_race import validate_race


def copy_race(tmp_path, new_id):
    d = tmp_path / new_id; d.mkdir()
    shutil.copy(REPO / "races/human-red/human-red.dat", d / f"{new_id}.dat")
    shutil.copy(REPO / "races/human-red/human-red.rac", d / f"{new_id}.rac")
    return d


def test_identical_copy_is_valid(tmp_path):
    assert validate_race(copy_race(tmp_path, "orc-red"), REPO / "races/human-red") == []


def test_stat_change_is_reported(tmp_path):
    d = copy_race(tmp_path, "orc-red")
    rac = d / "orc-red.rac"
    t = rac.read_text(encoding="latin-1").replace("max_speed", "max_speed 99 #", 1)
    rac.write_text(t, encoding="latin-1")
    assert any("rac differs" in e for e in validate_race(d, REPO / "races/human-red"))


def test_texture_size_change_is_reported(tmp_path):
    d = copy_race(tmp_path, "orc-red")
    u = tmp_path / "u"
    subprocess.run(["python3", str(SCRIPTS / "do_dat_tool.py"), "unpack", str(d / "orc-red.dat"),
                    "-o", str(u)], check=True, capture_output=True)
    t = sorted((u / "textures").glob("*footman_stay*.tga"))[0]
    Image.open(t).resize((8, 8)).save(t, "TGA")
    subprocess.run(["python3", str(SCRIPTS / "do_dat_tool.py"), "pack", str(u),
                    "-o", str(d / "orc-red.dat")], check=True, capture_output=True)
    assert any("size" in e and "footman_stay" in e for e in validate_race(d, REPO / "races/human-red"))

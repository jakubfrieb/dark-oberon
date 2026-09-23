import json
import subprocess

import numpy as np
from PIL import Image

from tests.race_pipeline.conftest import SCRIPTS

TOOL = str(SCRIPTS / "do_dat_tool.py")


def test_pack_strips_tga2_footer_written_by_pil(tmp_path):
    """Engine reads TGAs sequentially from the .dat stream; any trailing
    bytes (PIL writes a TGA 2.0 'TRUEVISION-XFILE' footer) break loading."""
    src = tmp_path / "src"; (src / "textures").mkdir(parents=True)
    a = np.zeros((4, 6, 4), np.uint8); a[..., 0] = 200; a[..., 3] = 255
    Image.fromarray(a, "RGBA").save(src / "textures/t.tga", "TGA")
    assert (src / "textures/t.tga").read_bytes().endswith(b"TRUEVISION-XFILE.\x00")
    manifest = {"format": "dark_oberon_dat", "version": 3, "source_dat": "x",
                "texture_groups": [{"name": "g", "textures": [
                    {"id": "t", "hcount": 1, "vcount": 1, "atime": 0, "pointx": 0,
                     "pointy": 0, "ttype": 0, "file": "textures/t.tga"}]}],
                "sounds": []}
    (src / "manifest.json").write_text(json.dumps(manifest))
    subprocess.run(["python3", TOOL, "pack", str(src), "-o", str(tmp_path / "x.dat")],
                   check=True, capture_output=True)
    subprocess.run(["python3", TOOL, "unpack", str(tmp_path / "x.dat"), "-o", str(tmp_path / "out")],
                   check=True, capture_output=True)
    raw = next((tmp_path / "out/textures").glob("*.tga")).read_bytes()
    assert len(raw) == 18 + raw[0] + 6 * 4 * 4

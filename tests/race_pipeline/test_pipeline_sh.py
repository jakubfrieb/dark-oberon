import subprocess

from tests.race_pipeline.conftest import REPO, SCRIPTS
from validate_race import validate_race

SH = str(SCRIPTS / "race_pipeline.sh")


def run(*args):
    subprocess.run(["bash", SH, *args], check=True, capture_output=True)


def test_compose_finalize_variants_identity(tmp_path):
    work = tmp_path / "w"
    run("compose", "--source", str(REPO / "races/human-red"), "--work-dir", str(work))
    out = tmp_path / "races" / "orc-red"
    run("finalize", "--work-dir", str(work), "--output", str(out),
        "--race-id", "orc-red", "--race-name", "Plastic Orcs - Red",
        "--names-json", str(SCRIPTS / "orc_entities.json"))
    assert validate_race(out, REPO / "races/human-red") == []
    assert 'name "Grunt"' in (out / "orc-red.rac").read_text(encoding="latin-1")
    run("variants", "--source", str(out), "--race-prefix", "orc", "--name-prefix", "Plastic Orcs")
    for c in ("blue", "yellow"):
        d = tmp_path / "races" / f"orc-{c}"
        assert validate_race(d, REPO / "races/human-red") == []
        assert f'"Plastic Orcs - {c.capitalize()}"' in (d / f"orc-{c}.rac").read_text(encoding="latin-1")

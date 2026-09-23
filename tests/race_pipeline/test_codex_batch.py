import json

from PIL import Image

from run_codex_board_batch import Batch, codex_command

ENTS = {"footman": {"name": "Grunt", "description": "orc"},
        "peasant": {"name": "Peon", "description": "orc"}}


def make_work(tmp_path):
    w = tmp_path
    (w / "boards").mkdir()
    boards = []
    for ent in ("footman", "peasant"):
        for anim in ("stay", "move"):
            bf = f"{ent}__{anim}__b0.png"
            Image.new("RGBA", (16, 16), (200, 0, 0, 255)).save(w / "boards" / bf)
            boards.append({"board_id": bf[:-4], "board_file": bf, "entity_id": ent,
                           "entity_type": "unit", "animation": anim, "grid": [1, 1],
                           "slots": [{"idx": 0, "dst": [0, 0, 16, 16]}],
                           "prompt_hints": {"frame_info": "x"}})
    (w / "boards" / "_boards_manifest.json").write_text(json.dumps({"boards": boards}))
    return w


class FakeRunner:
    def __init__(self, fail=()):
        self.calls, self.fail = [], set(fail)

    def __call__(self, prompt, images, expect, work, **kw):
        self.calls.append((expect.name, [p.name for p in images]))
        if expect.name in self.fail:
            return False
        expect.parent.mkdir(parents=True, exist_ok=True)
        Image.new("RGB", (16, 16), (0, 200, 0)).save(expect)
        return True


def test_codex_command_passes_images_and_stdin(tmp_path):
    cmd = codex_command([tmp_path / "a.png", tmp_path / "b.png"], tmp_path)
    assert cmd[:2] == ["codex", "exec"]
    assert ["-s", "workspace-write"] == cmd[cmd.index("-s"):cmd.index("-s") + 2]
    assert cmd.count("-i") == 2 and cmd[-1] == "-"


def test_design_then_restyle_requires_approval(tmp_path):
    w = make_work(tmp_path)
    run = FakeRunner()
    b = Batch(w, ENTS, runner=run)
    b.design(["footman"])
    assert (w / "design/footman.png").exists()
    b.restyle()
    assert len(run.calls) == 1  # not approved yet -> no board jobs
    b.approve(["footman"])
    b.restyle()
    done = [c[0] for c in run.calls[1:]]
    assert sorted(done) == ["footman__move__b0.png", "footman__stay__b0.png"]
    for name, images in run.calls[1:]:
        assert images == [name, "footman.png"]
    assert (w / "codex_in/footman__stay__b0.png").exists()


def test_restyle_resumes_and_skips_done(tmp_path):
    w = make_work(tmp_path)
    b = Batch(w, ENTS, runner=FakeRunner(fail={"footman__move__b0.png"}))
    b.design(["footman"]); b.approve(["footman"])
    b.restyle()
    state = json.loads((w / "_codex_state.json").read_text())
    assert state["board:footman__stay__b0"]["status"] == "done"
    assert state["board:footman__move__b0"]["status"] == "failed"
    run2 = FakeRunner()
    Batch(w, ENTS, runner=run2).restyle()
    assert [c[0] for c in run2.calls] == ["footman__move__b0.png"]


def test_only_and_animation_filters_and_dry_run(tmp_path):
    w = make_work(tmp_path)
    run = FakeRunner()
    b = Batch(w, ENTS, runner=run)
    b.design(["footman", "peasant"]); b.approve(["footman", "peasant"])
    b.restyle(only={"peasant"}, animation="stay", dry_run=True)
    assert len(run.calls) == 2  # only the two design calls
    b.restyle(only={"peasant"}, animation="stay")
    assert run.calls[-1][0] == "peasant__stay__b0.png" and len(run.calls) == 3

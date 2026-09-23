import json

import numpy as np
from PIL import Image

from board_postprocess import (flatten_on_white, make_review, process_all,
                               restore_alpha, validate_board)

SIZE = 64


def human_board():
    """Transparent 64x64 board, one slot: red figure 20x30 at (20,10) + dark shadow."""
    a = np.zeros((SIZE, SIZE, 4), np.uint8)
    a[10:40, 20:40] = (220, 30, 30, 255)          # figure
    a[40:44, 14:40] = (0, 0, 0, 120)             # shadow
    return Image.fromarray(a, "RGBA")


BOARD = {"board_id": "x__stay__b0", "board_file": "x__stay__b0.png", "entity_id": "x",
         "slots": [{"idx": 0, "dst": [0, 0, SIZE, SIZE]}]}


def orc_generated(shift=0):
    img = flatten_on_white(human_board())
    a = np.array(img)
    a[10:40, 20:40] = 255
    a[10:40, 20 + shift:40 + shift] = (60, 140, 50)  # green orc in the same place
    return Image.fromarray(a, "RGB")


def test_flatten_is_rgb_white_background():
    f = flatten_on_white(human_board())
    assert f.mode == "RGB" and f.getpixel((0, 0)) == (255, 255, 255)


def test_restore_alpha_keeps_silhouette_and_shadow():
    orig = human_board()
    out = restore_alpha(orc_generated(), orig)
    assert out.size == orig.size and out.mode == "RGBA"
    assert out.getpixel((25, 20)) == (60, 140, 50, 255)   # orc colour, opaque
    assert out.getpixel((0, 0))[3] == 0                    # background transparent
    assert out.getpixel((15, 41)) == (0, 0, 0, 120)        # shadow untouched


def test_restore_alpha_resizes_generated():
    big = orc_generated().resize((SIZE * 2, SIZE * 2), Image.NEAREST)
    assert restore_alpha(big, human_board()).size == (SIZE, SIZE)


def test_validate_ok_for_good_board():
    assert validate_board(orc_generated(), human_board(), BOARD) == []


def test_validate_flags_shifted_sprite():
    issues = validate_board(orc_generated(shift=15), human_board(), BOARD)
    assert any(i.startswith("slot 0 coverage") for i in issues)
    assert any(i.startswith("slot 0 spill") for i in issues)


def test_validate_flags_unchanged():
    issues = validate_board(flatten_on_white(human_board()), human_board(), BOARD)
    assert "unchanged" in issues


def test_review_is_side_by_side():
    r = make_review(human_board(), restore_alpha(orc_generated(), human_board()))
    assert r.size == (SIZE * 2, SIZE)


def test_process_all_writes_only_valid(tmp_path):
    w = tmp_path
    (w / "boards").mkdir(); (w / "raw").mkdir()
    bad = dict(BOARD, board_id="y__stay__b0", board_file="y__stay__b0.png", entity_id="y")
    (w / "boards" / "_boards_manifest.json").write_text(json.dumps({"boards": [BOARD, bad]}))
    human_board().save(w / "boards" / BOARD["board_file"])
    human_board().save(w / "boards" / bad["board_file"])
    orc_generated().save(w / "raw" / BOARD["board_file"])
    orc_generated(shift=15).save(w / "raw" / bad["board_file"])
    report = process_all(w)
    assert report["x__stay__b0"]["ok"] is True
    assert report["y__stay__b0"]["ok"] is False
    assert (w / "boards/edited/x__stay__b0.png").exists()
    assert not (w / "boards/edited/y__stay__b0.png").exists()
    assert (w / "review/y__stay__b0.png").exists()
    assert json.loads((w / "_post_report.json").read_text())["y__stay__b0"]["ok"] is False


def test_restore_alpha_drops_white_background_inside_old_silhouette():
    gen = np.array(orc_generated())
    gen[10:40, 36:40] = 255          # orc is narrower: codex painted background there
    out = restore_alpha(Image.fromarray(gen, "RGB"), human_board())
    assert out.getpixel((38, 20))[3] == 0
    assert out.getpixel((25, 20)) == (60, 140, 50, 255)


PORTRAIT = {"board_id": "p__picture__b0", "board_file": "p__picture__b0.png", "entity_id": "p",
            "slots": [{"idx": 0, "dst": [4, 4, 40, 32]}]}


def test_slots_bbox_is_union_of_slots():
    from board_postprocess import slots_bbox
    b = {"slots": [{"dst": [4, 4, 40, 32]}, {"dst": [48, 4, 40, 32]}]}
    assert slots_bbox(b) == (4, 4, 88, 36)


def test_embed_generated_puts_crop_back_at_bbox():
    from board_postprocess import embed_generated
    crop = Image.new("RGB", (80, 64), (0, 200, 0))          # codex output, any size
    full = embed_generated(crop, PORTRAIT, (64, 64))
    assert full.size == (64, 64)
    assert full.getpixel((10, 10)) == (0, 200, 0)
    assert full.getpixel((50, 50)) == (255, 255, 255)


def test_embed_generated_keeps_full_size_board():
    from board_postprocess import embed_generated
    img = orc_generated()
    assert embed_generated(img, BOARD, (SIZE, SIZE)).tobytes() == img.tobytes()


def test_restore_alpha_keeps_white_detail_enclosed_in_figure():
    gen = np.array(orc_generated())
    gen[20:25, 25:30] = 255          # light courtyard / white detail fully inside the figure
    out = restore_alpha(Image.fromarray(gen, "RGB"), human_board())
    assert out.getpixel((27, 22)) == (255, 255, 255, 255)


def test_process_all_accepts_listed_board_despite_issues(tmp_path):
    w = tmp_path
    (w / "boards").mkdir(); (w / "raw").mkdir()
    (w / "boards" / "_boards_manifest.json").write_text(json.dumps({"boards": [BOARD]}))
    human_board().save(w / "boards" / BOARD["board_file"])
    orc_generated(shift=15).save(w / "raw" / BOARD["board_file"])
    report = process_all(w, accept={"x__stay__b0"})
    assert report["x__stay__b0"]["ok"] is True
    assert report["x__stay__b0"]["accepted"] is True
    assert report["x__stay__b0"]["issues"]            # issues are still recorded
    assert (w / "boards/edited/x__stay__b0.png").exists()


def test_accept_is_remembered_between_runs(tmp_path):
    w = tmp_path
    (w / "boards").mkdir(); (w / "raw").mkdir()
    (w / "boards" / "_boards_manifest.json").write_text(json.dumps({"boards": [BOARD]}))
    human_board().save(w / "boards" / BOARD["board_file"])
    orc_generated(shift=15).save(w / "raw" / BOARD["board_file"])
    process_all(w, accept={"x__stay__b0"})
    report = process_all(w)
    assert report["x__stay__b0"]["accepted"] is True
    assert (w / "boards/edited/x__stay__b0.png").exists()

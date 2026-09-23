from codex_prompts import STYLE_RULES, design_prompt, restyle_prompt

ENT = {"name": "Grunt", "description": "a stocky green orc warrior"}
BOARD = {"board_id": "footman__stay__b0", "animation": "stay", "grid": [3, 3],
         "entity_type": "unit",
         "slots": [{"idx": i} for i in range(8)],
         "prompt_hints": {"frame_info": "8 directions, 1 frame each"}}


def test_style_rules_are_plasticine():
    s = STYLE_RULES.lower()
    for word in ("plasticine", "clay", "matte", "no outlines", "pixel art"):
        assert word in s


def test_design_prompt_contents():
    p = design_prompt("footman", ENT, "unit", "design/footman.png")
    assert STYLE_RULES in p
    assert "Grunt" in p and "stocky green orc warrior" in p
    assert "./design/footman.png" in p
    assert "pure red" in p.lower()


def test_restyle_prompt_contents():
    p = restyle_prompt(BOARD, ENT, "raw/footman__stay__b0.png")
    assert STYLE_RULES in p
    assert "./raw/footman__stay__b0.png" in p
    assert "3 columns x 3 rows" in p and "8 sprites" in p
    assert "same position" in p.lower() and "1024x1024" in p
    assert "8 directions, 1 frame each" in p
    assert "white background" in p.lower()


def test_building_design_keeps_human_layout():
    p = design_prompt("barracks", ENT, "building", "design/barracks.png").lower()
    assert "same overall shape" in p and "footprint" in p


def test_restyle_prompt_says_design_is_only_for_materials():
    p = restyle_prompt(dict(BOARD, entity_type="building"), ENT, "raw/x.png").lower()
    assert "shape comes from image 1" in p


def test_restyle_prompt_uses_crop_size():
    b = dict(BOARD, grid=[1, 1], slots=[{"idx": 0, "dst": [4, 4, 400, 320]}])
    p = restyle_prompt(b, ENT, "raw/x.png")
    assert "400x320" in p and "1024x1024" not in p

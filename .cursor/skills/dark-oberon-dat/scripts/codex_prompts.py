"""Prompt texts for codex image generation of the Dark Oberon orc race."""

from __future__ import annotations

STYLE_RULES = """\
STYLE (mandatory, matches the existing game art):
- Everything looks hand-sculpted from matte plasticine / modelling clay, like a
  claymation figure photographed from above at an angle.
- Soft rounded shapes, chunky simplified proportions, no sharp edges, no fine
  detail (no hair strands, no fabric texture, no shiny metal). Details are
  stuck-on clay balls, rolls and spikes.
- Soft light from the top-left, gentle shading, slight sheen; keep the cast
  ground shadow exactly as in the original.
- Flat uniform clay colours: green orc skin (2-3 shades), brown leather/wood,
  grey bone/stone, and TEAM COLOUR parts in pure saturated red.
  No gradients, no noise, no photographic textures.
- FORBIDDEN: pixel art, cel shading, ink lines (no outlines at all),
  realistic render, 2D cartoon look, any text, any background other than
  plain white."""


def design_prompt(entity_id: str, entity: dict, entity_type: str, out_rel: str) -> str:
    kind = "unit" if entity_type == "unit" else "building"
    return f"""Use your image generation tool to create ONE image and save it as a PNG.

Attached image 1 shows the existing HUMAN {kind} '{entity_id}' from the game
Dark Oberon. Other attached images are orc concept references (use them only
for the orc look, NOT for their art style).

Create a character/model design sheet of its ORC counterpart "{entity['name']}":
{entity['description']}.
Show the same {kind} 3 times on a plain white background: front-left view,
back-right view, and a close-up of the parts painted in the team colour
(pure red). Same size and proportions as the human {kind} so it can replace it.

{STYLE_RULES}

Save to: ./{out_rel}  (1024x1024 PNG)"""


def restyle_prompt(board: dict, entity: dict, out_rel: str) -> str:
    cols, rows = board["grid"]
    n = len(board["slots"])
    info = board.get("prompt_hints", {}).get("frame_info", "")
    return f"""Use your image generation tool to EDIT attached image 1 and save the result as a PNG.

Attached image 1 is a 1024x1024 sprite board from the game Dark Oberon: {n} sprites
arranged in {cols} columns x {rows} rows on a white background. They are the
same human figure in the '{board['animation']}' animation ({info}).
Attached image 2 is the approved design sheet of the ORC that replaces it:
"{entity['name']}" - {entity['description']}.

Repaint EVERY sprite as this orc. Pixel precision matters: the game cuts the
sprites out of this board at fixed positions using the original outline.
- keep each sprite in the same position, same size, same pose, same facing
  direction and the same silhouette outline as in image 1;
- keep the cast shadows; keep the plain white background;
- keep the board 1024x1024 with the same {cols} columns x {rows} rows layout;
- do not add, remove, merge or move sprites; nothing outside the sprites;
- the orc must be identical across all sprites (same colours and gear as image 2).

{STYLE_RULES}

Save to: ./{out_rel}  (1024x1024 PNG)"""

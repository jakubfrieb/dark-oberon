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


LAYOUT_RULE = """\
IMPORTANT for buildings: keep the SAME OVERALL SHAPE, footprint and layout as the
human building in image 1 (same walls, towers, roofs, courtyard and gate
positions, same height). Only change materials, surfaces and decorations to
the orc style (rough logs, hides, bones, tusks, spikes, stone). The orc
building must fit exactly inside the human building's outline.
"""


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
{LAYOUT_RULE if kind == "building" else ""}
{STYLE_RULES}

Save to: ./{out_rel}  (1024x1024 PNG)"""


ANIMATION_HINTS = {
    "picture": "This is the in-game portrait icon: a close-up view cropped by the image "
               "borders. Keep the same framing and zoom - the subject fills the image "
               "edge to edge, no white background, do not shrink it into a small object.",
    "build": "These are construction stages of the building (foundation site, "
             "half-built, almost done). Keep each sprite exactly as unfinished as in "
             "image 1 - same scaffolding, bare ground and missing parts, only in orc "
             "materials. Never draw a finished building where image 1 is unfinished.",
    "projectile": "This is the flying projectile (missile), not the unit itself. "
                  "Repaint it as one simple round clay rock (grey/brown, a few bone "
                  "shards stuck in it), the same size and position as in image 1. "
                  "Draw nothing else.",
    "zombie": "These are the dead/ruined remains. Keep them broken and collapsed "
              "exactly like image 1, only in orc materials.",
}


def _crop_size(board: dict) -> str:
    rects = [s["dst"] for s in board["slots"] if "dst" in s]
    if not rects:
        size = board.get("board_size", 1024)
        return f"{size}x{size}"
    w = max(r[0] + r[2] for r in rects) - min(r[0] for r in rects)
    h = max(r[1] + r[3] for r in rects) - min(r[1] for r in rects)
    return f"{w}x{h}"


def restyle_prompt(board: dict, entity: dict, out_rel: str, hint: str | None = None) -> str:
    size = _crop_size(board)
    cols, rows = board["grid"]
    n = len(board["slots"])
    info = board.get("prompt_hints", {}).get("frame_info", "")
    return f"""Use your image generation tool to EDIT attached image 1 and save the result as a PNG.

Attached image 1 is a {size} sprite board from the game Dark Oberon: {n} sprites
arranged in {cols} columns x {rows} rows on a white background. They are the
same human figure in the '{board['animation']}' animation ({info}).
Attached image 2 is the approved design sheet of the ORC that replaces it:
"{entity['name']}" - {entity['description']}.

Repaint EVERY sprite as this orc. Pixel precision matters: the game cuts the
sprites out of this board at fixed positions using the original outline.
The shape comes from image 1; image 2 only defines materials, colours and
decorations. Each sprite may differ (e.g. damaged/ruined or under
construction) - keep those differences.
- keep each sprite in the same position, same size, same pose, same facing
  direction and the same silhouette outline as in image 1;
- keep the cast shadows; keep the plain white background;
- keep the image exactly {size} with the same {cols} columns x {rows} rows layout;
- do not add, remove, merge or move sprites; nothing outside the sprites;
- the orc must be identical across all sprites (same colours and gear as image 2).

{ANIMATION_HINTS.get(board["animation"], "")}
{("IMPORTANT: " + hint) if hint else ""}

{STYLE_RULES}

Save to: ./{out_rel}  ({size} PNG)"""

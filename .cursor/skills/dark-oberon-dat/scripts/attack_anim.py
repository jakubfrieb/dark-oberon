#!/usr/bin/env python3
"""
New multi-frame melee attack animation for a Dark Oberon unit.

The original races ship ONE static attack frame per direction (drawn larger than the
idle pose, so in game the unit just "grows"). This tool lets codex draw a 4-frame sword
swing per direction from the idle frame and turns it into an engine texture:

  prepare  - idle frames (8 directions) upscaled on white -> <work>/in/dir<N>.png
  generate - codex draws a 2x2 grid of swing frames per direction on magenta
             -> <work>/raw/dir<N>.png   (non-deterministic step)
  build    - key magenta, scale to the idle size, anchor feet on the idle ground line,
             add the idle ground shadow -> <work>/frames/dir<N>.png (4x64 px sheet)
             + <work>/preview/dir<N>.gif
  apply    - write the sheets into an unpacked .dat group (hcount=frames, atime),
             optionally recolouring the team colour (recolor_race mapping)

Usage:
  attack_anim.py prepare  --unpacked U --stay-group footman_stay --work W
  attack_anim.py generate --work W --reference REF.png --subject "..." [--parallel N] [--only 1,3] [--force]
  attack_anim.py build    --unpacked U --stay-group footman_stay --work W
  attack_anim.py apply    --unpacked U --group footman_attack --work W [--atime 1000] [--mapping team_blue.json]
"""

from __future__ import annotations

import argparse
import json
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter

MAGENTA = np.array([255.0, 0.0, 255.0])
KEY_IN = 90.0      # distance to magenta below which a pixel is background
KEY_SOFT = 70.0    # soft edge width
SHADOW_ALPHA = 250
SHADOW_LUMA = 80
SHADOW_MAX_SAT = 40
FRAME = 64

PROMPT = """Use your image generation tool to create ONE image and save it as a PNG.

Attached image 1: a game sprite of {subject}, a claymation / modelling-clay figure seen
from above at an angle, standing idle. Attached image 2: reference of the same character.

Draw a 2x2 grid of 4 equal square cells (each 512x512), 1024x1024 px total, showing this
SAME character performing ONE melee sword attack, cells in reading order (top-left,
top-right, bottom-left, bottom-right):
  1. wind-up: sword raised back over the shoulder, body twisted back
  2. swing: sword coming down/forward, body leaning forward
  3. strike: sword fully extended forward at hip height (impact)
  4. recover: returning toward the idle pose
Rules:
- exactly the same facing direction, camera angle, colours, gear and proportions as image 1;
  the feet stay on the same spot in every cell;
- matte plasticine / modelling clay look, soft light from top-left, chunky rounded shapes,
  no outlines, no pixel art;
- plain flat MAGENTA background (#FF00FF) everywhere outside the figure; NO ground shadow,
  no floor, no text;
- keep the whole figure INCLUDING the full sword blade inside its own cell with a wide empty
  margin (at least 80 px) to every cell border; the figure is centred in its cell and drawn
  at the same size in all 4 cells (about 45 % of the cell height).

Save to: ./raw/dir{direction}.png  (1024x1024 PNG)"""


# ---------------------------------------------------------------------------
# Image processing (deterministic)
# ---------------------------------------------------------------------------

def key_magenta(img: Image.Image) -> Image.Image:
    """Magenta background -> transparent, soft edges, magenta spill removed."""
    a = np.asarray(img.convert("RGB")).astype(np.float32)
    d = np.sqrt(((a - MAGENTA) ** 2).sum(-1))
    alpha = np.clip((d - KEY_IN) / KEY_SOFT, 0.0, 1.0)
    spill = np.minimum(a[..., 0], a[..., 2]) - a[..., 1]
    spill = np.where(alpha < 1.0, np.maximum(spill, 0.0), 0.0)
    a[..., 0] -= spill
    a[..., 2] -= spill
    out = np.dstack([np.clip(a, 0, 255), alpha * 255.0]).astype(np.uint8)
    return Image.fromarray(out, "RGBA")


def split_grid(img: Image.Image, cols: int, rows: int) -> list[Image.Image]:
    w, h = img.size[0] // cols, img.size[1] // rows
    return [img.crop((c * w, r * h, c * w + w, r * h + h)) for r in range(rows) for c in range(cols)]


def feet_anchor(mask: np.ndarray) -> tuple[float, float, float]:
    """(x of the feet = centre of the lowest 15 % rows, bottom row, figure height)."""
    ys, xs = np.nonzero(mask)
    bottom, top = int(ys.max()), int(ys.min())
    band = ys >= bottom - max(2, int((bottom - top) * 0.15))
    return float(xs[band].mean()), float(bottom), float(bottom - top + 1)


def shadow_and_figure(stay: Image.Image) -> tuple[np.ndarray, np.ndarray]:
    """Ground shadow = dark, grey, semi-transparent pixels; figure = the rest of the opaque pixels.
    Dark outline pixels hugging the figure above the feet are not shadow (they would stay
    visible as specks around a different pose)."""
    s = np.asarray(stay.convert("RGBA")).astype(int)
    alpha = s[..., 3]
    luma = (s[..., 0] * 299 + s[..., 1] * 587 + s[..., 2] * 114) // 1000
    sat = s[..., :3].max(-1) - s[..., :3].min(-1)
    cand = (alpha > 0) & (alpha < SHADOW_ALPHA) & (luma < SHADOW_LUMA) & (sat < SHADOW_MAX_SAT)
    figure = (alpha >= 128) & ~cand
    if figure.any():
        _, feet_y, _ = feet_anchor(figure)
        near = np.asarray(Image.fromarray((figure * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(3))) > 0
        rows = np.arange(s.shape[0])[:, None] < feet_y - 3
        cand = cand & ~(near & rows)
    return cand, figure


def compose_frames(cells: list[Image.Image], stay: Image.Image, ref_index: int = 3) -> tuple[list[Image.Image], float]:
    """Keyed cells -> 64x64 frames at the idle scale, feet on the idle ground point, idle shadow below.
    One scale per direction (from the recover cell) keeps the swing consistent."""
    shadow, figure = shadow_and_figure(stay)
    sx, sy, sh = feet_anchor(figure)
    ref = np.asarray(cells[ref_index])[..., 3] > 128
    _, _, rh = feet_anchor(ref)
    scale = sh / rh
    size = stay.size
    shadow_layer = np.zeros((size[1], size[0], 4), np.uint8)
    shadow_layer[shadow] = np.asarray(stay.convert("RGBA"))[shadow]
    frames = []
    for c in cells:
        small = c.resize((max(1, round(c.width * scale)), max(1, round(c.height * scale))), Image.LANCZOS)
        m = np.asarray(small)[..., 3] > 128
        fx, fy, _ = feet_anchor(m)
        canvas = Image.fromarray(shadow_layer.copy(), "RGBA")
        layer = Image.new("RGBA", size, (0, 0, 0, 0))
        layer.paste(small, (int(round(sx - fx)), int(round(sy - fy))), small)
        canvas.alpha_composite(layer)
        frames.append(canvas)
    return frames, scale


def build_sheet(frames: list[Image.Image]) -> Image.Image:
    w, h = frames[0].size
    sheet = Image.new("RGBA", (w * len(frames), h), (0, 0, 0, 0))
    for i, f in enumerate(frames):
        sheet.alpha_composite(f, (w * i, 0))
    return sheet


def patch_group(unpacked: Path, group: str, sheets: dict[int, Image.Image], atime: int,
                mapping: dict | None = None) -> int:
    """Replace textures of @p group (1-based index -> sheet) and set hcount/vcount/atime."""
    unpacked = Path(unpacked)
    mpath = unpacked / "manifest.json"
    manifest = json.loads(mpath.read_text("utf-8"))
    g = next((g for g in manifest["texture_groups"] if g["name"] == group), None)
    if g is None:
        raise KeyError(f"texture group {group} not in {mpath}")
    if mapping is not None:
        from recolor_race import recolor_image
    n = 0
    for idx, sheet in sheets.items():
        t = g["textures"][idx - 1]
        img = recolor_image(sheet, mapping) if mapping is not None else sheet.convert("RGBA")
        img.save(unpacked / t["file"], "TGA")
        t["hcount"] = img.width // img.height
        t["vcount"] = 1
        t["atime"] = int(atime)
        n += 1
    mpath.write_text(json.dumps(manifest, indent=2), "utf-8")
    return n


# ---------------------------------------------------------------------------
# CLI steps
# ---------------------------------------------------------------------------

def _group_textures(unpacked: Path, group: str) -> list[Path]:
    manifest = json.loads((unpacked / "manifest.json").read_text("utf-8"))
    g = next(g for g in manifest["texture_groups"] if g["name"] == group)
    return [unpacked / t["file"] for t in g["textures"]]


def cmd_prepare(a) -> int:
    (a.work / "in").mkdir(parents=True, exist_ok=True)
    for i, p in enumerate(_group_textures(a.unpacked, a.stay_group), 1):
        im = Image.open(p).convert("RGBA")
        bg = Image.new("RGBA", im.size, (255, 255, 255, 255))
        bg.alpha_composite(im)
        bg.convert("RGB").resize((im.width * 4, im.height * 4), Image.LANCZOS).save(a.work / "in" / f"dir{i}.png")
    print(f"prepared {i} directions in {a.work / 'in'}")
    return 0


def cmd_generate(a) -> int:
    from run_codex_board_batch import run_codex
    (a.work / "raw").mkdir(parents=True, exist_ok=True)
    dirs = sorted(int(p.stem[3:]) for p in (a.work / "in").glob("dir*.png"))
    if a.only:
        dirs = [d for d in dirs if d in {int(x) for x in a.only.split(",")}]

    def one(d: int) -> tuple[int, bool]:
        out = a.work / "raw" / f"dir{d}.png"
        if out.exists() and not a.force:
            return d, True
        prompt = PROMPT.format(subject=a.subject, direction=d)
        return d, run_codex(prompt, [a.work / "in" / f"dir{d}.png", a.reference.resolve()], out, a.work.resolve())

    with ThreadPoolExecutor(max_workers=max(1, a.parallel)) as ex:
        res = dict(ex.map(one, dirs))
    for d, ok in sorted(res.items()):
        print(f"  dir{d}: {'ok' if ok else 'FAILED'}")
    return 0 if all(res.values()) else 1


def cmd_build(a) -> int:
    (a.work / "frames").mkdir(parents=True, exist_ok=True)
    (a.work / "preview").mkdir(parents=True, exist_ok=True)
    grass = (96, 128, 64, 255)
    for i, p in enumerate(_group_textures(a.unpacked, a.stay_group), 1):
        raw = a.work / "raw" / f"dir{i}.png"
        if not raw.exists():
            print(f"  dir{i}: no raw grid, skipped")
            continue
        stay = Image.open(p).convert("RGBA")
        frames, scale = compose_frames(split_grid(key_magenta(Image.open(raw)), 2, 2), stay)
        build_sheet(frames).save(a.work / "frames" / f"dir{i}.png")
        gif = [Image.alpha_composite(Image.new("RGBA", f.size, grass), f).resize((f.width * 4, f.height * 4), Image.NEAREST)
               for f in frames]
        gif[0].save(a.work / "preview" / f"dir{i}.gif", save_all=True, append_images=gif[1:], duration=250, loop=0)
        print(f"  dir{i}: scale={scale:.3f}")
    return 0


def cmd_apply(a) -> int:
    sheets = {int(p.stem[3:]): Image.open(p).convert("RGBA") for p in sorted((a.work / "frames").glob("dir*.png"))}
    mapping = json.loads(a.mapping.read_text()) if a.mapping else None
    n = patch_group(a.unpacked, a.group, sheets, a.atime, mapping)
    print(f"patched {n} textures of {a.group} in {a.unpacked}")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("prepare")
    p.add_argument("--unpacked", type=Path, required=True)
    p.add_argument("--stay-group", required=True)
    p.add_argument("--work", type=Path, required=True)
    g = sub.add_parser("generate")
    g.add_argument("--work", type=Path, required=True)
    g.add_argument("--reference", type=Path, required=True)
    g.add_argument("--subject", required=True)
    g.add_argument("--parallel", type=int, default=4)
    g.add_argument("--only", default=None)
    g.add_argument("--force", action="store_true")
    b = sub.add_parser("build")
    b.add_argument("--unpacked", type=Path, required=True)
    b.add_argument("--stay-group", required=True)
    b.add_argument("--work", type=Path, required=True)
    y = sub.add_parser("apply")
    y.add_argument("--unpacked", type=Path, required=True)
    y.add_argument("--group", required=True)
    y.add_argument("--work", type=Path, required=True)
    y.add_argument("--atime", type=int, default=1000)
    y.add_argument("--mapping", type=Path, default=None)
    a = ap.parse_args()
    return {"prepare": cmd_prepare, "generate": cmd_generate, "build": cmd_build, "apply": cmd_apply}[a.cmd](a)


if __name__ == "__main__":
    sys.exit(main())

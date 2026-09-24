#!/usr/bin/env python3
"""Remove frame-to-frame shimmer from restyled animations.

Codex repaints every frame of an animation on its own, so parts that should stand still (a
building's walls, an airship's hull) come out slightly different in each frame and flicker in
game. The reference race (the original humans) shows which pixels are really meant to move:
this script takes frame 0 of the restyled texture everywhere except where the reference
animation changes (grown by a few pixels), and keeps the restyled frame there.

When the restyled art has nothing that should move where the reference moves (e.g. the orc
workshop has no wheel on the roof), --freeze makes every frame of those groups equal to frame 0.

Usage (unpacked .dat directories, see unpack_dat.sh):
    stabilize_frames.py RACE_UNPACKED REF_UNPACKED GROUP [GROUP ...] [--grow N] [--freeze GROUP ...]

Example:
    stabilize_frames.py races/orc-red/orc-red.dat-unpacked races/human-red/human-red.dat-unpacked \
        airship_move --freeze manufactory_stay
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter

DIFF_THRESHOLD = 40


def split(img: np.ndarray, h: int, v: int) -> list[np.ndarray]:
    fh, fw = img.shape[0] // v, img.shape[1] // h
    return [img[r * fh:(r + 1) * fh, c * fw:(c + 1) * fw].copy() for r in range(v) for c in range(h)]


def join(frames: list[np.ndarray], h: int, v: int) -> np.ndarray:
    rows = [np.concatenate(frames[r * h:(r + 1) * h], axis=1) for r in range(v)]
    return np.concatenate(rows, axis=0)


def motion_mask(ref_frames: list[np.ndarray], grow: int = 3) -> np.ndarray:
    """Pixels that change anywhere in the reference animation, grown by `grow` pixels."""
    base = ref_frames[0].astype(np.int16)
    mask = np.zeros(base.shape[:2], bool)
    for f in ref_frames[1:]:
        mask |= np.abs(base - f.astype(np.int16)).max(axis=2) > DIFF_THRESHOLD
    if grow > 0:
        img = Image.fromarray((mask * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(2 * grow + 1))
        mask = np.asarray(img) > 0
    return mask


def stabilize(frames: list[np.ndarray], ref_frames: list[np.ndarray], grow: int = 3) -> list[np.ndarray]:
    if len(frames) != len(ref_frames) or frames[0].shape != ref_frames[0].shape:
        raise ValueError("race and reference animations differ in frame count or size")
    mask = motion_mask(ref_frames, grow)[..., None]
    base = frames[0]
    return [base.copy()] + [np.where(mask, f, base) for f in frames[1:]]


def freeze(frames: list[np.ndarray]) -> list[np.ndarray]:
    return [frames[0].copy() for _ in frames]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("race")
    ap.add_argument("reference")
    ap.add_argument("groups", nargs="*")
    ap.add_argument("--freeze", nargs="+", default=[], metavar="GROUP")
    ap.add_argument("--grow", type=int, default=3)
    args = ap.parse_args()

    race, ref = Path(args.race), Path(args.reference)
    rm = json.loads((race / "manifest.json").read_text())
    fm = json.loads((ref / "manifest.json").read_text())
    ref_tex = {(g["name"], t["id"]): t for g in fm["texture_groups"] for t in g["textures"]}

    for g in rm["texture_groups"]:
        frozen = g["name"] in args.freeze
        if g["name"] not in args.groups and not frozen:
            continue
        for t in g["textures"]:
            h, v = t["hcount"], t["vcount"]
            if h * v < 2:
                continue
            path = race / t["file"]
            img = Image.open(path)
            frames = split(np.asarray(img.convert("RGBA")), h, v)
            if frozen:
                out = join(freeze(frames), h, v)
            else:
                rt = ref_tex.get((g["name"], t["id"]))
                if rt is None or (rt["hcount"], rt["vcount"]) != (h, v):
                    print(f"skip {t['id']}: no matching reference texture")
                    continue
                ref_frames = split(np.asarray(Image.open(ref / rt["file"]).convert("RGBA")), h, v)
                out = join(stabilize(frames, ref_frames, args.grow), h, v)
            # keep the TGA layout of the original (uncompressed, same row order)
            Image.fromarray(out, "RGBA").save(path, orientation=img.info.get("orientation", -1))
            print(f"{'froze' if frozen else 'stabilized'} {g['name']}/{t['id']} ({h * v} frames)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

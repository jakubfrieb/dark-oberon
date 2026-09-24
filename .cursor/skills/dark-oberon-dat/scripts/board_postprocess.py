#!/usr/bin/env python3
"""
Post-process codex-restyled AI boards.

For every board with a raw codex output in <work>/raw/, restore the original
alpha (silhouette + human shadow), validate that the sprites stayed in their
cells, write valid boards to <work>/boards/edited/ and a side-by-side review
image to <work>/review/.  Report goes to <work>/_post_report.json.

Usage:
    python3 board_postprocess.py <work_dir> [--only footman,peasant]
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter

WHITE = 235
BG_WHITE = 245
FRINGE_MIN = 210        # light grey outline fringe (min RGB)
FRINGE_MAX_SAT = 25
FRINGE_PASSES = 2
SHADOW_MAX_SAT = 40
ISLAND_LIGHT_MIN = 170
ISLAND_MAX_SAT = 40
SHADOW_ALPHA = 250
SHADOW_LUMA = 80
FIGURE_ALPHA = 128
MIN_COVERAGE = 0.85
MAX_SPILL = 0.20
SPILL_DILATE = 6
MIN_CHANGE = 12.0


def flatten_on_white(img: Image.Image) -> Image.Image:
    rgba = img.convert("RGBA")
    bg = Image.new("RGBA", rgba.size, (255, 255, 255, 255))
    return Image.alpha_composite(bg, rgba).convert("RGB")


def slots_bbox(board: dict) -> tuple[int, int, int, int]:
    """Union of all slot rectangles on the board as (x0, y0, x1, y1)."""
    rects = [s["dst"] for s in board["slots"]]
    return (min(r[0] for r in rects), min(r[1] for r in rects),
            max(r[0] + r[2] for r in rects), max(r[1] + r[3] for r in rects))


def embed_generated(generated: Image.Image, board: dict, size: tuple[int, int]) -> Image.Image:
    """Codex restyles only the slot bbox crop; put it back onto a full white board."""
    x0, y0, x1, y1 = slots_bbox(board)
    g = generated.convert("RGB")
    if (x0, y0, x1, y1) == (0, 0, *size) and g.size == size:
        return g
    full = Image.new("RGB", size, (255, 255, 255))
    full.paste(g.resize((x1 - x0, y1 - y0), Image.LANCZOS), (x0, y0))
    return full


def _masks(original: Image.Image) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """Return (alpha, shadow_mask, figure_mask) of the original board."""
    o = np.asarray(original.convert("RGBA")).astype(np.int32)
    alpha = o[..., 3]
    luma = (o[..., 0] * 299 + o[..., 1] * 587 + o[..., 2] * 114) // 1000
    sat = o[..., :3].max(-1) - o[..., :3].min(-1)
    # a ground shadow is dark AND grey; dark coloured edges (e.g. blue ore outline) are not
    shadow = (alpha > 0) & (alpha < SHADOW_ALPHA) & (luma < SHADOW_LUMA) & (sat < SHADOW_MAX_SAT)
    figure = (alpha >= FIGURE_ALPHA) & ~shadow
    return alpha, shadow, figure


def _fit(generated: Image.Image, size: tuple[int, int]) -> np.ndarray:
    g = generated.convert("RGB")
    if g.size != size:
        g = g.resize(size, Image.LANCZOS)
    return np.asarray(g).astype(np.int32)


def _grow(mask: np.ndarray) -> np.ndarray:
    g = mask.copy()
    g[1:] |= mask[:-1]; g[:-1] |= mask[1:]
    g[:, 1:] |= mask[:, :-1]; g[:, :-1] |= mask[:, 1:]
    return g


def _connected_to(candidate: np.ndarray, seed: np.ndarray) -> np.ndarray:
    """Pixels of ``candidate`` 4-connected to ``seed`` (flood fill without scipy)."""
    reached = _grow(seed) & candidate
    while True:
        nxt = _grow(reached) & candidate
        if nxt.sum() == reached.sum():
            return reached
        reached = nxt


def restore_alpha(generated: Image.Image, original: Image.Image) -> Image.Image:
    orig = np.asarray(original.convert("RGBA"))
    alpha, shadow, _ = _masks(original)
    gen = _fit(generated, original.size).astype(np.uint8)
    out = np.dstack([gen, alpha.astype(np.uint8)])
    out[shadow] = orig[shadow]
    # codex painted plain background inside the old silhouette (orc is narrower);
    # only white connected to the outside counts - enclosed light details stay
    candidate = (gen.min(axis=2) >= BG_WHITE) & ~shadow
    out[_connected_to(candidate, alpha == 0)] = 0
    # light grey fringe left on the outline by the old (lighter) human silhouette; cream/bone
    # details are more saturated and stay
    for _ in range(FRINGE_PASSES):
        transparent = out[..., 3] == 0
        g = out[..., :3].astype(np.int32)
        light = (g.min(axis=2) >= FRINGE_MIN) & (g.max(axis=2) - g.min(axis=2) < FRINGE_MAX_SAT) & ~shadow
        edge = _grow(transparent) & ~transparent                       # touches transparency
        out[light & edge] = 0
    # islands of the old silhouette (separate human objects) that codex filled with plain light
    # grey: an opaque part with no darker / coloured pixel at all is removed
    g = out[..., :3].astype(np.int32)
    opaque = (out[..., 3] > 0) & ~shadow
    light = (g.min(axis=2) >= ISLAND_LIGHT_MIN) & (g.max(axis=2) - g.min(axis=2) < ISLAND_MAX_SAT)
    keep = _connected_to(opaque, opaque & ~light)
    out[opaque & ~keep] = 0
    # semi-transparent outline pixels left behind by removed objects: not shadow and not next to
    # anything opaque any more
    solid = out[..., 3] >= FIGURE_ALPHA
    soft = (out[..., 3] > 0) & (out[..., 3] < FIGURE_ALPHA) & ~shadow
    out[soft & ~_grow(solid)] = 0
    out[alpha == 0] = 0
    return Image.fromarray(out, "RGBA")


def retint(img: Image.Image, rule: dict) -> Image.Image:
    """Move pixels whose hue lies in rule['from_hue'] (degrees) to rule['to_hue'] with saturation
    rule['sat'] (value kept) - e.g. the human blue boulder on the orc catapult -> grey-brown rock."""
    from colorsys import hsv_to_rgb
    a = np.asarray(img.convert("RGBA")).astype(np.float64)
    rgb = a[..., :3] / 255.0
    mx, mn = rgb.max(-1), rgb.min(-1)
    d = mx - mn
    h = np.zeros_like(mx)
    nz = d > 1e-9
    r_, g_, b_ = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    rm = nz & (mx == r_); gm = nz & (mx == g_) & ~rm; bm = nz & ~rm & ~gm
    h[rm] = ((g_ - b_)[rm] / d[rm]) % 6
    h[gm] = (b_ - r_)[gm] / d[gm] + 2
    h[bm] = (r_ - g_)[bm] / d[bm] + 4
    h *= 60.0
    s = np.where(mx > 0, d / np.where(mx > 0, mx, 1), 0)
    lo, hi = rule["from_hue"]
    sel = (h >= lo) & (h <= hi) & (s > 0.15) & (a[..., 3] > 0)
    out = a.copy()
    tr, tg, tb = hsv_to_rgb(rule["to_hue"] / 360.0, rule["sat"], 1.0)
    for i, c in enumerate((tr, tg, tb)):
        out[..., i][sel] = np.clip(mx[sel] * c * 255.0, 0, 255)
    return Image.fromarray(out.astype(np.uint8), "RGBA")


def validate_board(generated: Image.Image, original: Image.Image, board: dict) -> list[str]:
    issues: list[str] = []
    alpha, _, figure = _masks(original)
    gen = _fit(generated, original.size)
    content = gen.min(axis=2) < WHITE
    near = np.asarray(
        Image.fromarray(((alpha > 0) * 255).astype(np.uint8))
        .filter(ImageFilter.MaxFilter(2 * SPILL_DILATE + 1))) > 0

    for slot in board["slots"]:
        x, y, w, h = slot["dst"]
        f = figure[y:y + h, x:x + w]
        n = int(f.sum())
        if n == 0:
            continue
        cov = float((content[y:y + h, x:x + w] & f).sum()) / n
        spill = float((content[y:y + h, x:x + w] & ~near[y:y + h, x:x + w]).sum()) / n
        if cov < MIN_COVERAGE:
            issues.append(f"slot {slot['idx']} coverage {cov:.2f}")
        if spill > MAX_SPILL:
            issues.append(f"slot {slot['idx']} spill {spill:.2f}")

    if figure.any():
        flat = np.asarray(flatten_on_white(original)).astype(np.int32)
        diff = float(np.abs(gen - flat)[figure].mean())
        if diff < MIN_CHANGE:
            issues.append("unchanged")
    return issues


def make_review(original: Image.Image, processed: Image.Image) -> Image.Image:
    w, h = original.size
    out = Image.new("RGB", (w * 2, h), (255, 255, 255))
    out.paste(flatten_on_white(original), (0, 0))
    out.paste(flatten_on_white(processed), (w, 0))
    return out


def process_all(work: Path, only: set[str] | None = None,
                accept: set[str] | None = None) -> dict:
    """``accept``: board ids the user accepted despite validation issues."""
    accepted_path = work / "_accepted.json"
    remembered = set(json.loads(accepted_path.read_text("utf-8"))) if accepted_path.exists() else set()
    accept = remembered | set(accept or ())
    if accept != remembered:
        accepted_path.write_text(json.dumps(sorted(accept), indent=2), "utf-8")
    boards_dir = work / "boards"
    manifest = json.loads((boards_dir / "_boards_manifest.json").read_text("utf-8"))
    edited = boards_dir / "edited"
    review = work / "review"
    edited.mkdir(parents=True, exist_ok=True)
    review.mkdir(parents=True, exist_ok=True)
    report_path = work / "_post_report.json"
    report = json.loads(report_path.read_text("utf-8")) if report_path.exists() else {}
    retint_path = work / "_retint.json"       # {entity: {from_hue: [lo, hi], to_hue, sat}}
    retints = json.loads(retint_path.read_text("utf-8")) if retint_path.exists() else {}

    for board in manifest["boards"]:
        if only and board["entity_id"] not in only:
            continue
        raw = work / "raw" / board["board_file"]
        if not raw.exists():
            continue
        original = Image.open(boards_dir / board["board_file"])
        generated = embed_generated(Image.open(raw), board, original.size)
        issues = validate_board(generated, original, board)
        processed = restore_alpha(generated, original)
        if board["entity_id"] in retints:
            processed = retint(processed, retints[board["entity_id"]])
        make_review(original, processed).save(review / board["board_file"])
        out = edited / board["board_file"]
        accepted = bool(issues) and board["board_id"] in accept
        ok = not issues or accepted
        if ok:
            processed.save(out)
        else:
            out.unlink(missing_ok=True)
        report[board["board_id"]] = {"ok": ok, "issues": issues, "accepted": accepted}
        tag = "OK  " if not issues else ("ACPT" if accepted else "FAIL")
        print(f"  {tag} {board['board_id']} {' '.join(issues)}")

    report_path.write_text(json.dumps(report, indent=2), "utf-8")
    return report


def main() -> int:
    ap = argparse.ArgumentParser(description="Post-process codex boards")
    ap.add_argument("work_dir", type=Path)
    ap.add_argument("--only", default=None, help="comma-separated entity ids")
    ap.add_argument("--accept", default=None,
                    help="comma-separated board ids to keep despite validation issues")
    args = ap.parse_args()
    only = set(args.only.split(",")) if args.only else None
    accept = set(args.accept.split(",")) if args.accept else None
    report = process_all(args.work_dir.resolve(), only, accept)
    failed = [k for k, v in report.items() if not v["ok"]]
    print(f"\n{len(report) - len(failed)} ok, {len(failed)} failed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())

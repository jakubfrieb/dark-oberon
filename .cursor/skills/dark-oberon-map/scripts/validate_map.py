#!/usr/bin/env python3
"""Validate Dark Oberon .map files against a scheme (basic structural checks)."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from dark_oberon_maplib import (  # noqa: E402
    ParsedMap,
    SchemeSegment,
    find_repo_root,
    parse_map,
    parse_scheme,
    resolve_scheme_path,
)


def fragment_rect(
    fid: int, x: int, y: int, sch: SchemeSegment
) -> tuple[int, int, int, int] | None:
    frag = sch.fragments.get(fid)
    if not frag:
        return None
    w = h = frag.size
    return x, y, x + w - 1, y + h - 1


def rects_overlap(
    a: tuple[int, int, int, int], b: tuple[int, int, int, int]
) -> bool:
    ax0, ay0, ax1, ay1 = a
    bx0, by0, bx1, by1 = b
    return not (ax1 < bx0 or bx1 < ax0 or ay1 < by0 or by1 < ay0)


def validate_segment(
    sid: int,
    pmap: ParsedMap,
    sch: SchemeSegment,
    width: int,
    height: int,
) -> list[str]:
    msgs: list[str] = []
    seg = pmap.segments.get(sid)
    if seg is None:
        return [f"Segment {sid}: missing from map"]

    nfrag = len(seg.fragment_rows)
    if seg.fragments_count >= 0 and nfrag != seg.fragments_count:
        msgs.append(
            f"Segment {sid}: fragment row count {nfrag} != declared count {seg.fragments_count}"
        )

    nl = len(seg.layer_rows)
    if seg.layers_count >= 0 and nl != seg.layers_count:
        msgs.append(
            f"Segment {sid}: layer row count {nl} != declared layers count {seg.layers_count} "
            "(fix count or remove orphan layer_* lines)"
        )

    no = len(seg.object_rows)
    if seg.objects_count >= 0 and no != seg.objects_count:
        msgs.append(
            f"Segment {sid}: object row count {no} != declared objects count {seg.objects_count}"
        )

    frag_rects: list[tuple[int, int, int, int, int]] = []
    for fi, (_i, fid, x, y) in enumerate(seg.fragment_rows):
        r = fragment_rect(fid, x, y, sch)
        if r is None:
            msgs.append(f"Segment {sid}: unknown fragment id {fid} at ({x},{y})")
            continue
        x0, y0, x1, y1 = r
        if x0 < 0 or y0 < 0 or x1 >= width or y1 >= height:
            msgs.append(
                f"Segment {sid}: fragment {fid} at ({x},{y}) extends outside map "
                f"[{x0},{y0}]-[{x1},{y1}] vs size {width}x{height}"
            )
        frag_rects.append((fi, r[0], r[1], r[2], r[3]))

    for i, a in enumerate(frag_rects):
        _ai, ax0, ay0, ax1, ay1 = a
        ra = (ax0, ay0, ax1, ay1)
        for b in frag_rects[i + 1 :]:
            _bi, bx0, by0, bx1, by1 = b
            rb = (bx0, by0, bx1, by1)
            if rects_overlap(ra, rb):
                msgs.append(
                    f"Segment {sid}: overlapping fragments at rectangles {ra} and {rb}"
                )

    rects: list[tuple[int, int, int, int]] = []
    for _i, oid, x, y in seg.object_rows:
        obj = sch.objects.get(oid)
        if not obj:
            msgs.append(f"Segment {sid}: unknown object id {oid} at ({x},{y})")
            continue
        x1, y1 = x + obj.width - 1, y + obj.height - 1
        if x < 0 or y < 0 or x1 >= width or y1 >= height:
            msgs.append(
                f"Segment {sid}: object {oid} at ({x},{y}) extends outside map"
            )
        rects.append((x, y, x1, y1))

    for i, a in enumerate(rects):
        for b in rects[i + 1 :]:
            if rects_overlap(a, b):
                msgs.append(
                    f"Segment {sid}: terrain objects overlap rectangles {a} and {b}"
                )

    return msgs


def main() -> int:
    ap = argparse.ArgumentParser(description="Validate Dark Oberon .map file")
    ap.add_argument("map_file", type=Path, help="Path to .map file")
    ap.add_argument(
        "--scheme",
        type=Path,
        default=None,
        help="Path to .sch (default: schemes/<name>.sch)",
    )
    ap.add_argument(
        "--repo-root",
        type=Path,
        default=None,
        help="Repository root (default: walk up until schemes/ exists)",
    )
    args = ap.parse_args()

    repo = args.repo_root or find_repo_root(Path(__file__).parent)

    pmap = parse_map(args.map_file)
    scheme_path = args.scheme or resolve_scheme_path(pmap.header.scheme, repo)
    if not scheme_path.is_file():
        print(f"ERROR: scheme not found: {scheme_path}", file=sys.stderr)
        return 2

    sch = parse_scheme(scheme_path)
    w, h = pmap.header.width, pmap.header.height
    if w <= 0 or h <= 0:
        print("ERROR: invalid width/height in map header", file=sys.stderr)
        return 2

    all_msgs: list[str] = []
    for sid in sorted(pmap.segments.keys()):
        sseg = sch.segments.get(sid)
        if not sseg:
            all_msgs.append(f"Segment {sid}: not defined in scheme {scheme_path.name}")
            continue
        all_msgs.extend(validate_segment(sid, pmap, sseg, w, h))

    if all_msgs:
        for m in all_msgs:
            print(m)
        return 1
    print(f"OK: {args.map_file} + {scheme_path.name}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

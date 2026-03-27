"""
Minimal parsers for Dark Oberon .map and .sch files (plastic-style schemes).
Used by validate_map.py and map_preview.py — not a full game data loader.
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Tuple


def strip_hash_comments(text: str) -> str:
    lines = []
    for line in text.splitlines():
        if "#" in line:
            line = line[: line.index("#")]
        lines.append(line.rstrip())
    return "\n".join(lines)


def extract_section(text: str, section_name: str) -> Optional[str]:
    start_tag = f"<{section_name}>"
    end_tag = f"</{section_name}>"
    i = text.find(start_tag)
    if i < 0:
        return None
    i += len(start_tag)
    j = text.find(end_tag, i)
    if j < 0:
        return None
    return text[i:j]


def extract_all_segments(text: str) -> Dict[int, str]:
    out: Dict[int, str] = {}
    pat = re.compile(r"<Segment\s+(\d+)>", re.MULTILINE)
    for m in pat.finditer(text):
        sid = int(m.group(1))
        start = m.end()
        end_tag = f"</Segment {sid}>"
        end = text.find(end_tag, start)
        if end < 0:
            continue
        out[sid] = text[start:end]
    return out


@dataclass
class MapHeader:
    name: str = ""
    author: str = ""
    width: int = 0
    height: int = 0
    scheme: str = ""


@dataclass
class MapSegmentData:
    fragment_rows: List[Tuple[int, int, int, int]] = field(default_factory=list)
    layer_rows: List[Tuple[int, int, int, int]] = field(default_factory=list)
    object_rows: List[Tuple[int, int, int, int]] = field(default_factory=list)
    fragments_count: int = -1
    layers_count: int = -1
    objects_count: int = -1


@dataclass
class ParsedMap:
    header: MapHeader
    segments: Dict[int, MapSegmentData]
    raw_text: str


def parse_map_header(text: str) -> MapHeader:
    h = MapHeader()
    for line in text.splitlines():
        line = line.strip()
        if line.startswith('name "'):
            h.name = re.match(r'name\s+"([^"]*)"', line).group(1)
        elif line.startswith('author "'):
            h.author = re.match(r'author\s+"([^"]*)"', line).group(1)
        elif line.startswith("width "):
            h.width = int(line.split()[1])
        elif line.startswith("height "):
            h.height = int(line.split()[1])
        elif line.startswith('scheme "'):
            h.scheme = re.match(r'scheme\s+"([^"]*)"', line).group(1)
    return h


_RE_FRAG = re.compile(r"^\s*fragment_(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s*")
_RE_LAYER = re.compile(r"^\s*layer_(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s*")
_RE_OBJ = re.compile(r"^\s*object_(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s*")
_RE_COUNT = re.compile(r"^\s*count\s+(\d+)\s*$")


def _parse_subsection_counts_and_rows(
    seg_text: str, subsection: str
) -> Tuple[Optional[int], List[str]]:
    inner = extract_section(seg_text, subsection)
    if inner is None:
        return None, []
    lines = [ln.strip() for ln in inner.splitlines() if ln.strip()]
    count: Optional[int] = None
    body: List[str] = []
    for ln in lines:
        cm = _RE_COUNT.match(ln)
        if cm and count is None:
            count = int(cm.group(1))
            continue
        body.append(ln)
    return count, body


def parse_map(path: Path | str) -> ParsedMap:
    path = Path(path)
    raw = strip_hash_comments(path.read_text(encoding="utf-8", errors="replace"))
    header = parse_map_header(raw)
    segments: Dict[int, MapSegmentData] = {}

    for sid, seg_inner in extract_all_segments(raw).items():
        ms = MapSegmentData()
        fc, frag_lines = _parse_subsection_counts_and_rows(seg_inner, "Fragments")
        lc, lay_lines = _parse_subsection_counts_and_rows(seg_inner, "Layers")
        oc, obj_lines = _parse_subsection_counts_and_rows(seg_inner, "Objects")
        ms.fragments_count = fc if fc is not None else -1
        ms.layers_count = lc if lc is not None else -1
        ms.objects_count = oc if oc is not None else -1

        for idx, ln in enumerate(frag_lines):
            m = _RE_FRAG.match(ln)
            if m:
                _k, fid, x, y = map(int, m.groups())
                ms.fragment_rows.append((idx, fid, x, y))
        for idx, ln in enumerate(lay_lines):
            m = _RE_LAYER.match(ln)
            if m:
                _k, lid, x, y = map(int, m.groups())
                ms.layer_rows.append((idx, lid, x, y))
        for idx, ln in enumerate(obj_lines):
            m = _RE_OBJ.match(ln)
            if m:
                _k, oid, x, y = map(int, m.groups())
                ms.object_rows.append((idx, oid, x, y))

        segments[sid] = ms

    return ParsedMap(header=header, segments=segments, raw_text=raw)


@dataclass
class SchemeFragment:
    frag_id: int
    size: int
    terrain_ids: List[int]


@dataclass
class SchemeLayer:
    layer_id: int
    width: int
    height: int
    terrain_ids: List[int]


@dataclass
class SchemeObject:
    oid: int
    width: int
    height: int
    terrain_ids: List[int]


@dataclass
class SchemeSegment:
    frag_default_terrain_id: int = 0
    fragments: Dict[int, SchemeFragment] = field(default_factory=dict)
    layers: Dict[int, SchemeLayer] = field(default_factory=dict)
    objects: Dict[int, SchemeObject] = field(default_factory=dict)


@dataclass
class ParsedScheme:
    segments: Dict[int, SchemeSegment]


def _parse_int_list_after_keyword(block: str, keyword: str) -> List[int]:
    m = re.search(
        rf"{keyword}\s+((?:-?\d+\s+)+-?\d+)", block, re.DOTALL | re.MULTILINE
    )
    if not m:
        m = re.search(rf"{keyword}\s+(.+)", block)
        if not m:
            return []
        first_line = m.group(1).split("\n")[0]
        nums = re.findall(r"-?\d+", first_line)
        return [int(x) for x in nums]
    return [int(x) for x in re.findall(r"-?\d+", m.group(1))]


def _parse_fragment_blocks(seg_text: str) -> Tuple[int, Dict[int, SchemeFragment]]:
    frag_section = extract_section(seg_text, "Fragments")
    if frag_section is None:
        return 0, {}
    dm = re.search(r"default_terrain_id\s+(\d+)", frag_section)
    default_tid = int(dm.group(1)) if dm else 0
    frags: Dict[int, SchemeFragment] = {}
    for m in re.finditer(r"<Fragment\s+(\d+)>\s*(.*?)\s*</Fragment\s+\1>", frag_section, re.DOTALL):
        fid = int(m.group(1))
        blk = m.group(2)
        sm = re.search(r"size\s+(\d+)", blk)
        if not sm:
            continue
        size = int(sm.group(1))
        tids = _parse_int_list_after_keyword(blk, "terrain_id")
        expected = size * size
        if len(tids) != expected:
            if len(tids) < expected:
                tids = tids + [default_tid] * (expected - len(tids))
            else:
                tids = tids[:expected]
        frags[fid] = SchemeFragment(frag_id=fid, size=size, terrain_ids=tids)
    cm = re.search(r"count\s+(\d+)", frag_section)
    declared = int(cm.group(1)) if cm else len(frags)
    return declared, frags


def _parse_layer_blocks(seg_text: str) -> Dict[int, SchemeLayer]:
    lay_section = extract_section(seg_text, "Layers")
    if lay_section is None:
        return {}
    layers: Dict[int, SchemeLayer] = {}
    for m in re.finditer(r"<Layer\s+(\d+)>\s*(.*?)\s*</Layer\s+\1>", lay_section, re.DOTALL):
        lid = int(m.group(1))
        blk = m.group(2)
        wm = re.search(r"width\s+(\d+)", blk)
        hm = re.search(r"height\s+(\d+)", blk)
        if not wm or not hm:
            continue
        w, h = int(wm.group(1)), int(hm.group(1))
        tids = _parse_int_list_after_keyword(blk, "terrain_id")
        exp = w * h
        if len(tids) != exp:
            if len(tids) < exp:
                tids = tids + [0] * (exp - len(tids))
            else:
                tids = tids[:exp]
        layers[lid] = SchemeLayer(layer_id=lid, width=w, height=h, terrain_ids=tids)
    return layers


def _parse_object_blocks(seg_text: str) -> Dict[int, SchemeObject]:
    obj_section = extract_section(seg_text, "Objects")
    if obj_section is None:
        return {}
    objs: Dict[int, SchemeObject] = {}
    for m in re.finditer(r"<Object\s+(\d+)>\s*(.*?)\s*</Object\s+\1>", obj_section, re.DOTALL):
        oid = int(m.group(1))
        blk = m.group(2)
        wm = re.search(r"width\s+(\d+)", blk)
        hm = re.search(r"height\s+(\d+)", blk)
        if not wm or not hm:
            continue
        w, h = int(wm.group(1)), int(hm.group(1))
        tids = _parse_int_list_after_keyword(blk, "terrain_id")
        exp = w * h
        if len(tids) != exp:
            if len(tids) < exp:
                tids = tids + [0] * (exp - len(tids))
            else:
                tids = tids[:exp]
        objs[oid] = SchemeObject(oid=oid, width=w, height=h, terrain_ids=tids)
    return objs


def parse_scheme(path: Path | str) -> ParsedScheme:
    path = Path(path)
    text = strip_hash_comments(path.read_text(encoding="utf-8", errors="replace"))
    out: Dict[int, SchemeSegment] = {}
    for sid, seg_inner in extract_all_segments(text).items():
        ss = SchemeSegment()
        _decl, frags = _parse_fragment_blocks(seg_inner)
        ss.fragments = frags
        ss.layers = _parse_layer_blocks(seg_inner)
        ss.objects = _parse_object_blocks(seg_inner)
        dm = re.search(r"default_terrain_id\s+(\d+)", seg_inner)
        if dm:
            ss.frag_default_terrain_id = int(dm.group(1))
        out[sid] = ss
    return ParsedScheme(segments=out)


def resolve_scheme_path(scheme_name: str, repo_root: Path) -> Path:
    return repo_root / "schemes" / f"{scheme_name}.sch"


def find_repo_root(start: Path | None = None) -> Path:
    """Walk up from ``start`` until a directory containing ``schemes/`` exists."""
    p = (start or Path.cwd()).resolve()
    for _ in range(12):
        if (p / "schemes").is_dir():
            return p
        if p.parent == p:
            break
        p = p.parent
    return Path.cwd()


def blit_fragment(grid: List[List[Optional[int]]], x: int, y: int, frag: SchemeFragment) -> None:
    w = h = frag.size
    for row in range(h):
        for col in range(w):
            idx = row * w + col
            if idx >= len(frag.terrain_ids):
                continue
            val = frag.terrain_ids[idx]
            mx, my = x + col, y + row
            if 0 <= mx < len(grid) and 0 <= my < len(grid[0]):
                grid[mx][my] = val


def blit_layer(grid: List[List[Optional[int]]], x: int, y: int, layer: SchemeLayer) -> None:
    w, h = layer.width, layer.height
    for row in range(h):
        for col in range(w):
            idx = row * w + col
            val = layer.terrain_ids[idx]
            mx, my = x + col, y + row
            if 0 <= mx < len(grid) and 0 <= my < len(grid[0]):
                grid[mx][my] = val


def build_terrain_grid(
    width: int,
    height: int,
    seg_map: MapSegmentData,
    sch_seg: SchemeSegment,
    fill_default: bool = True,
) -> List[List[Optional[int]]]:
    grid: List[List[Optional[int]]] = [
        [None for _ in range(height)] for _ in range(width)
    ]

    for _ln, fid, x, y in seg_map.fragment_rows:
        frag = sch_seg.fragments.get(fid)
        if frag is None:
            continue
        blit_fragment(grid, x, y, frag)

    for _ln, lid, x, y in seg_map.layer_rows:
        lay = sch_seg.layers.get(lid)
        if lay is None:
            continue
        blit_layer(grid, x, y, lay)

    if fill_default and sch_seg.fragments:
        max_id = max(sch_seg.fragments.keys())
        df = sch_seg.fragments.get(max_id)
        dval = df.terrain_ids[0] if df and df.terrain_ids else 0
        for mx in range(width):
            for my in range(height):
                if grid[mx][my] is None:
                    grid[mx][my] = dval

    return grid

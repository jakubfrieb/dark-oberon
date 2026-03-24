#!/usr/bin/env python3
"""
Pack / unpack Dark Oberon .dat files (textures + optional sounds).

Binary layout matches src/dodata.cpp (TTEX_TABLE::Load, TSND_TABLE::Load):
  - Header: ASCII \"Dark Oberon data file\" (21 bytes, no NUL)
  - uint8 version (2 or 3)
  - uint32 textures_offset
  - uint32 sounds_offset (version > 2 only; reader overwrites single field twice)

Texture block at textures_offset:
  - uint32 group_count
  For each group:
    - Pascal string: uint8 len + len bytes (len may be 0)
    - uint32 texture_count
    For each texture:
      - Pascal id
      - uint8 hcount, uint8 vcount
      - int32 atime, pointx, pointy
      - uint8 ttype
      - uint32 dsize
      - dsize bytes (raw TGA, as stored in game files)

Sound block at sounds_offset (if non-zero):
  - uint32 sound_count
  For each sound:
    - Pascal id
    - int8 format, int8 stype  (stype: -1 module, 0 sample, 1 stream)
    - int32 dsize
    - dsize bytes payload (present for all types; stream uses offset in original loader)
"""

from __future__ import annotations

import argparse
import json
import os
import re
import struct
import sys
from pathlib import Path
from typing import Any, BinaryIO

HEADER = b"Dark Oberon data file"
HEADER_LEN = len(HEADER)  # 21
assert HEADER_LEN == 21

DAT_ST_MODULE = -1
DAT_ST_SAMPLE = 0
DAT_ST_STREAM = 1


def _read_u8(f: BinaryIO) -> int:
    b = f.read(1)
    if len(b) != 1:
        raise EOFError("unexpected EOF")
    return b[0]


def _read_pascal_str(f: BinaryIO) -> str:
    n = _read_u8(f)
    if n == 0:
        return ""
    raw = f.read(n)
    if len(raw) != n:
        raise EOFError("truncated pascal string")
    return raw.decode("latin-1", errors="replace")


def _read_i32(f: BinaryIO) -> int:
    d = f.read(4)
    if len(d) != 4:
        raise EOFError("expected int32")
    return struct.unpack("<i", d)[0]


def _read_u32(f: BinaryIO) -> int:
    d = f.read(4)
    if len(d) != 4:
        raise EOFError("expected uint32")
    return struct.unpack("<I", d)[0]


def _read_i8(f: BinaryIO) -> int:
    d = f.read(1)
    if len(d) != 1:
        raise EOFError("expected int8")
    return struct.unpack("<b", d)[0]


def _safe_name(s: str) -> str:
    s = re.sub(r"[^a-zA-Z0-9_.-]+", "_", s)
    return s.strip("_") or "unnamed"


def unpack_dat(dat_path: Path, out_dir: Path) -> None:
    dat_path = dat_path.resolve()
    out_dir = out_dir.resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    tex_dir = out_dir / "textures"
    snd_dir = out_dir / "sounds"
    tex_dir.mkdir(exist_ok=True)
    snd_dir.mkdir(exist_ok=True)

    with open(dat_path, "rb") as f:
        hdr = f.read(HEADER_LEN)
        if hdr != HEADER:
            raise ValueError(f"bad header (expected {HEADER!r})")
        version = _read_u8(f)
        if version < 2 or version > 3:
            raise ValueError(f"unsupported version {version} (expected 2 or 3)")
        tex_off = _read_u32(f)
        snd_off = 0
        if version > 2:
            snd_off = _read_u32(f)

        manifest: dict[str, Any] = {
            "format": "dark_oberon_dat",
            "version": version,
            "source_dat": dat_path.name,
            "texture_groups": [],
            "sounds": [],
        }

        f.seek(tex_off)
        gcount = _read_u32(f)
        for gi in range(gcount):
            gname = _read_pascal_str(f)
            tcount = _read_u32(f)
            group_entry: dict[str, Any] = {"name": gname, "textures": []}
            for ti in range(tcount):
                tid = _read_pascal_str(f)
                hcount = _read_u8(f)
                vcount = _read_u8(f)
                atime = _read_i32(f)
                pointx = _read_i32(f)
                pointy = _read_i32(f)
                ttype = _read_u8(f)
                dsize = _read_u32(f)
                tga_data = f.read(dsize)
                if len(tga_data) != dsize:
                    raise EOFError(f"texture {tid!r}: expected {dsize} bytes TGA")
                rel = os.path.join(
                    "textures",
                    f"g{gi:03d}_t{ti:03d}_{_safe_name(gname)}__{_safe_name(tid)}.tga",
                )
                out_path = out_dir / rel
                out_path.write_bytes(tga_data)
                group_entry["textures"].append(
                    {
                        "id": tid,
                        "hcount": hcount,
                        "vcount": vcount,
                        "atime": atime,
                        "pointx": pointx,
                        "pointy": pointy,
                        "ttype": ttype,
                        "file": rel.replace("\\", "/"),
                    }
                )
            manifest["texture_groups"].append(group_entry)

        if snd_off != 0:
            f.seek(snd_off)
            scount = _read_u32(f)
            for si in range(scount):
                sid = _read_pascal_str(f)
                sformat = _read_i8(f)
                stype = _read_i8(f)
                dsize = _read_i32(f)
                payload = f.read(dsize)
                if len(payload) != dsize:
                    raise EOFError(f"sound {sid!r}: expected {dsize} bytes")
                ext = {0: ".wav", 3: ".ogg", 2: ".mp3", 4: ".raw"}.get(sformat, ".bin")
                rel = os.path.join("sounds", f"s{si:04d}_{_safe_name(sid)}{ext}")
                (out_dir / rel).write_bytes(payload)
                manifest["sounds"].append(
                    {
                        "id": sid,
                        "format": sformat,
                        "stype": stype,
                        "file": rel.replace("\\", "/"),
                    }
                )

    (out_dir / "manifest.json").write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )
    print(f"Unpacked to {out_dir}")


def pack_dat(manifest_dir: Path, out_dat: Path) -> None:
    manifest_dir = manifest_dir.resolve()
    mf_path = manifest_dir / "manifest.json"
    if not mf_path.is_file():
        raise FileNotFoundError(f"missing {mf_path}")

    manifest = json.loads(mf_path.read_text(encoding="utf-8"))
    if manifest.get("format") != "dark_oberon_dat":
        raise ValueError("manifest.json: missing or wrong 'format'")

    version = int(manifest["version"])
    if version not in (2, 3):
        raise ValueError("manifest version must be 2 or 3")

    tex_blob = bytearray()
    groups = manifest["texture_groups"]
    tex_blob += struct.pack("<I", len(groups))
    for g in groups:
        gname = g["name"]
        textures = g["textures"]
        _write_pascal_str_to_buf(tex_blob, gname)
        tex_blob += struct.pack("<I", len(textures))
        for t in textures:
            rel = t["file"]
            tga_path = manifest_dir / rel
            if not tga_path.is_file():
                raise FileNotFoundError(f"texture file missing: {tga_path}")
            raw = tga_path.read_bytes()
            _write_pascal_str_to_buf(tex_blob, t["id"])
            tex_blob += struct.pack(
                "<BBiiiBI",
                int(t["hcount"]) & 0xFF,
                int(t["vcount"]) & 0xFF,
                int(t["atime"]),
                int(t["pointx"]),
                int(t["pointy"]),
                int(t["ttype"]) & 0xFF,
                len(raw),
            )
            tex_blob += raw

    snd_blob = bytearray()
    sounds = manifest.get("sounds") or []
    if sounds and version < 3:
        raise ValueError("sounds present but manifest version is 2; use version 3")
    if sounds:
        snd_blob += struct.pack("<I", len(sounds))
        for s in sounds:
            rel = s["file"]
            p = manifest_dir / rel
            if not p.is_file():
                raise FileNotFoundError(f"sound file missing: {p}")
            payload = p.read_bytes()
            _write_pascal_str_to_buf(snd_blob, s["id"])
            snd_blob += struct.pack(
                "<bbi", int(s["format"]), int(s["stype"]), len(payload)
            )
            snd_blob += payload

    body = bytearray(HEADER)
    body.append(version & 0xFF)
    if version > 2:
        body += b"\x00\x00\x00\x00\x00\x00\x00\x00"
    else:
        body += b"\x00\x00\x00\x00"

    tex_start = len(body)
    body.extend(tex_blob)
    snd_start = len(body)
    if version > 2:
        if sounds:
            body.extend(snd_blob)
        else:
            body += struct.pack("<I", 0)

    struct.pack_into("<I", body, HEADER_LEN + 1, tex_start)
    if version > 2:
        struct.pack_into("<I", body, HEADER_LEN + 1 + 4, snd_start)

    out_dat = out_dat.resolve()
    out_dat.parent.mkdir(parents=True, exist_ok=True)
    out_dat.write_bytes(body)
    print(f"Wrote {out_dat} ({len(body)} bytes)")


def _write_pascal_str_to_buf(buf: bytearray, s: str) -> None:
    b = s.encode("latin-1", errors="replace")
    if len(b) > 255:
        b = b[:255]
    buf.append(len(b))
    buf.extend(b)


def main() -> None:
    ap = argparse.ArgumentParser(description="Dark Oberon .dat unpack / pack")
    sub = ap.add_subparsers(dest="cmd", required=True)

    u = sub.add_parser("unpack", help="Extract textures/sounds + manifest.json")
    u.add_argument("dat_file", type=Path)
    u.add_argument(
        "-o",
        "--out",
        type=Path,
        required=True,
        help="output directory",
    )

    p = sub.add_parser("pack", help="Build .dat from directory with manifest.json")
    p.add_argument("manifest_dir", type=Path, help="directory containing manifest.json")
    p.add_argument(
        "-o",
        "--output",
        type=Path,
        required=True,
        help="output .dat path",
    )

    args = ap.parse_args()
    try:
        if args.cmd == "unpack":
            unpack_dat(args.dat_file, args.out)
        else:
            pack_dat(args.manifest_dir, args.output)
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()

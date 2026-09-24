#!/usr/bin/env python3
"""
Validate a generated Dark Oberon race against a reference race.

Checks: every tg_* group referenced in .rac exists in .dat; texture groups have
the same textures (order, ids, sizes) as the reference; .rac differs from the
reference only in name/author lines.

Usage:
    python3 validate_race.py races/orc-red --reference races/human-red
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent


def tga_trailing_bytes(raw: bytes) -> int:
    """Bytes after the pixel data of an uncompressed TGA (engine needs 0)."""
    if raw[2] not in (1, 2, 3):
        return 0
    cmapbytes = (raw[5] | (raw[6] << 8)) * ((raw[7] + 7) // 8) if raw[1] == 1 else 0
    w, h, bpp = raw[12] | (raw[13] << 8), raw[14] | (raw[15] << 8), (raw[16] + 7) // 8
    return max(0, len(raw) - (18 + raw[0] + cmapbytes + w * h * bpp))


def _unpack(dat: Path, out: Path) -> dict:
    subprocess.run([sys.executable, str(HERE / "do_dat_tool.py"), "unpack", str(dat), "-o", str(out)],
                   check=True, stdout=subprocess.DEVNULL)
    return json.loads((out / "manifest.json").read_text("utf-8"))


def _groups(manifest: dict, root: Path) -> dict[str, list[tuple[str, tuple[int, int]]]]:
    return {g["name"]: [(t["id"], Image.open(root / t["file"]).size) for t in g["textures"]]
            for g in manifest["texture_groups"]}


def _trailing(manifest: dict, root: Path) -> list[str]:
    return [f"texture {g['name']}/{t['id']}: {n} bytes after pixel data (engine misreads .dat)"
            for g in manifest["texture_groups"] for t in g["textures"]
            if (n := tga_trailing_bytes((root / t["file"]).read_bytes()))]


def validate_race(race_dir: Path, reference_dir: Path) -> list[str]:
    race_dir, reference_dir = Path(race_dir), Path(reference_dir)
    rid, ref_id = race_dir.name, reference_dir.name
    errors: list[str] = []
    rac = (race_dir / f"{rid}.rac").read_text(encoding="latin-1").splitlines()
    ref_rac = (reference_dir / f"{ref_id}.rac").read_text(encoding="latin-1").splitlines()
    if len(rac) != len(ref_rac):
        errors.append(f"rac differs: {len(rac)} lines vs {len(ref_rac)}")
    for i, (a, b) in enumerate(zip(rac, ref_rac), 1):
        if a != b and not a.strip().startswith(("name", "author")):
            errors.append(f"rac differs at line {i}: {a.strip()!r} vs {b.strip()!r}")

    with tempfile.TemporaryDirectory() as tmp:
        t = Path(tmp)
        manifest = _unpack(race_dir / f"{rid}.dat", t / "a")
        groups = _groups(manifest, t / "a")
        errors += _trailing(manifest, t / "a")
        ref_groups = _groups(_unpack(reference_dir / f"{ref_id}.dat", t / "b"), t / "b")

    for line in rac:
        m = re.match(r'\s*(tg_\w+)\s+"?([^"\s]+)"?', line)
        if m and m.group(2) != "none" and m.group(2) not in groups:
            errors.append(f"missing texture group {m.group(2)} ({m.group(1)})")
    for name, ref in ref_groups.items():
        got = groups.get(name)
        if got is None:
            errors.append(f"missing texture group {name}")
            continue
        if [i for i, _ in got] != [i for i, _ in ref]:
            errors.append(f"group {name}: texture ids differ")
            continue
        for (tid, size), (_, rsize) in zip(got, ref):
            if size != rsize:
                errors.append(f"group {name}/{tid}: size {size} != {rsize}")
    return errors


def main() -> int:
    ap = argparse.ArgumentParser(description="Validate a race against a reference")
    ap.add_argument("race_dir", type=Path)
    ap.add_argument("--reference", type=Path, required=True)
    args = ap.parse_args()
    errors = validate_race(args.race_dir, args.reference)
    for e in errors:
        print("ERROR", e)
    print("OK" if not errors else f"{len(errors)} error(s)")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())

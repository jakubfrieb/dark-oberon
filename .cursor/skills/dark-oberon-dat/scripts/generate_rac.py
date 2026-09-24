#!/usr/bin/env python3
"""
Generate a new .rac file from an existing one with updated race metadata.

Usage:
    python3 generate_rac.py <source.rac> <output.rac> --race-name "Name" [--author "Author"]
                            [--names-json orc_entities.json]

Copies the source .rac verbatim, replacing only the top-level ``name`` and
optionally ``author`` lines.  All entity definitions, texture group
references, and game-play values are preserved unchanged.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path


_BLOCK_RE = re.compile(r'(<(Unit|Building) (\d+)>)(.*?)(</\2 \3>)', re.S)


def apply_entity_names(text: str, names: dict[str, str]) -> str:
    """Replace ``name`` inside <Unit N>/<Building N> blocks matched by ``id``."""
    applied: set[str] = set()

    def repl(m: re.Match) -> str:
        body = m.group(4)
        idm = re.search(r'^\s*id\s+"([^"]+)"', body, re.M)
        if idm and idm.group(1) in names:
            new = names[idm.group(1)]
            body = re.sub(r'^(\s*name\s+)"[^"]*"',
                          lambda mm: f'{mm.group(1)}"{new}"',
                          body, count=1, flags=re.M)
            applied.add(idm.group(1))
        return m.group(1) + body + m.group(5)

    out = _BLOCK_RE.sub(repl, text)
    missing = sorted(set(names) - applied)
    if missing:
        raise ValueError(f"ids not found in .rac: {', '.join(missing)}")
    return out


def load_names(path: Path) -> dict[str, str]:
    data = json.loads(Path(path).read_text("utf-8"))
    return {k: v["name"] for k, v in data.items()}


def generate_rac(
    source: Path,
    output: Path,
    race_name: str,
    author: str | None = None,
    names: dict[str, str] | None = None,
) -> None:
    text = source.read_text(encoding="latin-1")

    text = re.sub(
        r'^(name\s+)"[^"]*"',
        rf'\1"{race_name}"',
        text, count=1, flags=re.MULTILINE,
    )

    if author:
        text = re.sub(
            r'^(author\s+)"[^"]*"',
            rf'\1"{author}"',
            text, count=1, flags=re.MULTILINE,
        )

    if names:
        text = apply_entity_names(text, names)

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(text, encoding="latin-1")
    print(f"Generated {output}")


def main():
    ap = argparse.ArgumentParser(
        description="Generate new .rac from an existing template")
    ap.add_argument("source_rac", type=Path, help="Source .rac file")
    ap.add_argument("output_rac", type=Path, help="Output .rac file")
    ap.add_argument("--race-name", required=True, help="New race display name")
    ap.add_argument("--author", default=None, help="New author string")
    ap.add_argument("--names-json", type=Path, default=None,
                    help="JSON {id: {name, description}} to rename entities")
    args = ap.parse_args()
    names = load_names(args.names_json) if args.names_json else None
    generate_rac(args.source_rac, args.output_rac, args.race_name, args.author, names)


if __name__ == "__main__":
    main()

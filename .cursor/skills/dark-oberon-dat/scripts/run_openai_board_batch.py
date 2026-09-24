#!/usr/bin/env python3
"""
Submit AI boards to OpenAI Batch API for image editing (restyling).

Reads the board manifest, uploads identity reference + board PNGs,
builds a JSONL batch input for /v1/images/edits, submits the batch,
polls for completion, and downloads results.

Requires: OPENAI_API_KEY environment variable.

Usage:
    python3 run_openai_board_batch.py submit <boards_dir> \
        --reference <identity.png> --style "dark orc warriors with bone armor" \
        [--quality medium] [--model gpt-image-1.5] [--entities footman,peasant]

    python3 run_openai_board_batch.py poll <boards_dir>

    python3 run_openai_board_batch.py download <boards_dir> [--out <edited_dir>]
"""

from __future__ import annotations

import argparse
import base64
import json
import os
import sys
import time
from pathlib import Path

try:
    from openai import OpenAI
except ImportError:
    sys.exit("openai SDK required: pip install openai")


PROMPT_TEMPLATE = """\
Restyle the isometric game sprites in the second image to match the \
character/style shown in the first reference image.

Entity: {entity_id} ({entity_type}).
Animation group: {animation} — {frame_info}.
Sprites shown ({n_sprites} total): {labels_str}.

RULES:
- Preserve the exact pose, silhouette, and facing direction of each sprite.
- Keep each sprite within its current grid position — do not move or swap them.
- Maintain transparent background between and around sprites.
- Preserve any internal animation frame grid layout within each sprite.
- Apply the visual style, colors, and materials from the reference image.
- Keep the same sprite art rendering style (pre-rendered 3D / detailed pixel art).

Style description: {style}"""


def _build_prompt(board: dict, style: str) -> str:
    hints = board["prompt_hints"]
    labels = hints["labels"]
    labels_str = ", ".join(labels) if len(labels) <= 12 else (
        ", ".join(labels[:6]) + f" ... ({len(labels)} total)"
    )
    return PROMPT_TEMPLATE.format(
        entity_id=board["entity_id"],
        entity_type=hints["entity_type"],
        animation=hints["animation"],
        frame_info=hints["frame_info"],
        n_sprites=len(labels),
        labels_str=labels_str,
        style=style,
    )


def _load_state(boards_dir: Path) -> dict:
    sp = boards_dir / "_batch_state.json"
    if sp.exists():
        return json.loads(sp.read_text("utf-8"))
    return {}


def _save_state(boards_dir: Path, state: dict) -> None:
    sp = boards_dir / "_batch_state.json"
    sp.write_text(json.dumps(state, indent=2) + "\n", "utf-8")


def cmd_submit(
    boards_dir: Path, reference_paths: list[Path], style: str,
    quality: str, model: str, entity_filter: list[str] | None,
) -> None:
    manifest_path = boards_dir / "_boards_manifest.json"
    if not manifest_path.exists():
        sys.exit(f"Manifest not found: {manifest_path}")
    manifest = json.loads(manifest_path.read_text("utf-8"))

    client = OpenAI()

    # Upload reference images
    print("Uploading reference images...")
    ref_file_ids = []
    for rp in reference_paths:
        if not rp.exists():
            sys.exit(f"Reference image not found: {rp}")
        f = client.files.create(file=open(rp, "rb"), purpose="vision")
        ref_file_ids.append(f.id)
        print(f"  {rp.name} -> {f.id}")

    # Filter boards
    boards = manifest["boards"]
    if entity_filter:
        boards = [b for b in boards if b["entity_id"] in entity_filter]
    if not boards:
        sys.exit("No boards to process after filtering.")

    # Upload board PNGs and build batch lines
    print(f"\nUploading {len(boards)} board(s)...")
    jsonl_lines = []
    upload_map = {}

    for board in boards:
        bid = board["board_id"]
        bp = boards_dir / board["board_file"]
        if not bp.exists():
            print(f"  SKIP {bid}: file missing", file=sys.stderr)
            continue

        f = client.files.create(file=open(bp, "rb"), purpose="vision")
        upload_map[bid] = f.id
        print(f"  {bid} -> {f.id}")

        images = [{"file_id": fid} for fid in ref_file_ids]
        images.append({"file_id": f.id})

        prompt = _build_prompt(board, style)

        line = {
            "custom_id": bid,
            "method": "POST",
            "url": "/v1/images/edits",
            "body": {
                "model": model,
                "images": images,
                "prompt": prompt,
                "background": "transparent",
                "input_fidelity": "high",
                "quality": quality,
                "size": "1024x1024",
                "output_format": "png",
                "n": 1,
            },
        }
        jsonl_lines.append(json.dumps(line, ensure_ascii=False))

    # Write JSONL
    jsonl_path = boards_dir / "_batch_input.jsonl"
    jsonl_path.write_text("\n".join(jsonl_lines) + "\n", "utf-8")
    print(f"\nBatch input: {jsonl_path} ({len(jsonl_lines)} requests)")

    # Upload JSONL and create batch
    print("Submitting batch...")
    batch_file = client.files.create(
        file=open(jsonl_path, "rb"), purpose="batch",
    )
    batch = client.batches.create(
        input_file_id=batch_file.id,
        endpoint="/v1/images/edits",
        completion_window="24h",
        metadata={"description": f"AI board restyle: {style[:60]}"},
    )
    print(f"Batch created: {batch.id} (status: {batch.status})")

    state = {
        "batch_id": batch.id,
        "input_file_id": batch_file.id,
        "ref_file_ids": ref_file_ids,
        "upload_map": upload_map,
        "model": model,
        "quality": quality,
        "style": style,
        "status": batch.status,
    }
    _save_state(boards_dir, state)
    print(f"State saved to {boards_dir / '_batch_state.json'}")
    print("\nRun 'poll' to check progress, or 'download' when complete.")


def cmd_poll(boards_dir: Path) -> None:
    state = _load_state(boards_dir)
    if "batch_id" not in state:
        sys.exit("No batch state found. Run 'submit' first.")

    client = OpenAI()
    batch_id = state["batch_id"]

    print(f"Polling batch {batch_id}...")
    while True:
        batch = client.batches.retrieve(batch_id)
        counts = batch.request_counts
        total = counts.total if counts else 0
        done = counts.completed if counts else 0
        failed = counts.failed if counts else 0

        print(f"  status={batch.status}  completed={done}/{total}  "
              f"failed={failed}")

        state["status"] = batch.status
        if batch.output_file_id:
            state["output_file_id"] = batch.output_file_id
        if batch.error_file_id:
            state["error_file_id"] = batch.error_file_id
        _save_state(boards_dir, state)

        if batch.status in ("completed", "failed", "expired", "cancelled"):
            break

        time.sleep(15)

    print(f"\nBatch {batch.status}.")
    if batch.status == "completed":
        print("Run 'download' to fetch results.")


def cmd_download(boards_dir: Path, out_dir: Path) -> None:
    state = _load_state(boards_dir)
    if "output_file_id" not in state:
        sys.exit("No output file. Run 'poll' first or batch not yet complete.")

    client = OpenAI()
    out_dir.mkdir(parents=True, exist_ok=True)

    print(f"Downloading results to {out_dir}...")
    content = client.files.content(state["output_file_id"])
    lines = content.text.strip().split("\n")

    success = 0
    errors = 0
    for line in lines:
        result = json.loads(line)
        cid = result["custom_id"]
        resp = result.get("response", {})
        status_code = resp.get("status_code", 0)

        if status_code != 200:
            err = result.get("error") or resp.get("body", {}).get("error", {})
            print(f"  FAIL {cid}: {err}", file=sys.stderr)
            errors += 1
            continue

        body = resp.get("body", {})
        data = body.get("data", [])
        if not data:
            print(f"  FAIL {cid}: no image data in response", file=sys.stderr)
            errors += 1
            continue

        img_b64 = data[0].get("b64_json", "")
        if not img_b64:
            print(f"  FAIL {cid}: empty b64_json", file=sys.stderr)
            errors += 1
            continue

        img_bytes = base64.b64decode(img_b64)
        out_path = out_dir / f"{cid}.png"
        out_path.write_bytes(img_bytes)
        success += 1

    print(f"\nDone. {success} boards downloaded, {errors} errors.")
    if success > 0:
        print(f"Edited boards in: {out_dir}")
        print("Run unpack_ai_boards.py to restore into entity sheets.")


def main():
    ap = argparse.ArgumentParser(
        description="OpenAI Batch API runner for AI board restyling",
    )
    sub = ap.add_subparsers(dest="cmd", required=True)

    s = sub.add_parser("submit", help="Upload boards and submit batch")
    s.add_argument("boards_dir", type=Path)
    s.add_argument("--reference", type=Path, nargs="+", required=True,
                   help="Identity reference image(s)")
    s.add_argument("--style", type=str, required=True,
                   help="Style description for the restyle prompt")
    s.add_argument("--quality", type=str, default="medium",
                   choices=["low", "medium", "high"],
                   help="Output quality (default: medium)")
    s.add_argument("--model", type=str, default="gpt-image-1.5",
                   help="Image model (default: gpt-image-1.5)")
    s.add_argument("--entities", type=str, default=None,
                   help="Comma-separated entity filter (e.g. footman,peasant)")

    p = sub.add_parser("poll", help="Poll batch status")
    p.add_argument("boards_dir", type=Path)

    d = sub.add_parser("download", help="Download completed batch results")
    d.add_argument("boards_dir", type=Path)
    d.add_argument("--out", type=Path, default=None,
                   help="Output dir for edited boards "
                        "(default: <boards_dir>/edited)")

    args = ap.parse_args()

    if args.cmd == "submit":
        entity_filter = (
            [e.strip() for e in args.entities.split(",")]
            if args.entities else None
        )
        cmd_submit(
            args.boards_dir.resolve(),
            [r.resolve() for r in args.reference],
            args.style, args.quality, args.model, entity_filter,
        )
    elif args.cmd == "poll":
        cmd_poll(args.boards_dir.resolve())
    elif args.cmd == "download":
        out = args.out or (args.boards_dir / "edited")
        cmd_download(args.boards_dir.resolve(), out.resolve())


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""
Restyle sprite boards via local Stable Diffusion (Automatic1111 API).

Uses img2img + ControlNet (depth for pose, optional IP-Adapter for reference).
Processes boards sequentially, restoring alpha from originals.

Usage:
  # Depth ControlNet only (prompt-driven):
  python3 run_sd_board_batch.py submit <boards-dir> \
    --style "green orc warrior, bone armor, claymation style"

  # With IP-Adapter reference image for consistent character look:
  python3 run_sd_board_batch.py submit <boards-dir> \
    --reference orc-concept.png \
    --style "orc warrior, bone armor" \
    --entities footman

  # Check progress / resume interrupted batch:
  python3 run_sd_board_batch.py submit <boards-dir> --resume

  # Override A1111 URL:
  python3 run_sd_board_batch.py submit <boards-dir> --url http://host:7860 ...
"""

import argparse, base64, io, json, sys, time
from pathlib import Path

try:
    import requests
    from PIL import Image
except ImportError:
    sys.exit("pip install requests Pillow")


DEFAULT_URL = "http://localhost:7860"
STATE_FILE = "_sd_batch_state.json"

# --- image helpers ---

def img_to_b64(path: str) -> str:
    with open(path, "rb") as f:
        return base64.b64encode(f.read()).decode()


def b64_to_pil(b64: str) -> Image.Image:
    return Image.open(io.BytesIO(base64.b64decode(b64)))


def restore_alpha(generated: Image.Image, original: Image.Image) -> Image.Image:
    """Apply alpha channel from original RGBA board onto generated RGB output."""
    gen = generated.convert("RGBA")
    if original.mode == "RGBA":
        alpha = original.split()[3]
    else:
        alpha = Image.new("L", original.size, 255)
    gen.putalpha(alpha)
    return gen


# --- A1111 helpers ---

def check_server(url: str):
    try:
        r = requests.get(f"{url}/sdapi/v1/options", timeout=10)
        r.raise_for_status()
        opts = r.json()
        model = opts.get("sd_model_checkpoint", "?")
        print(f"A1111 server: {url}")
        print(f"  Model: {model}")
        return opts
    except Exception as e:
        sys.exit(f"Cannot reach A1111 at {url}: {e}")


def get_controlnet_models(url: str) -> dict:
    r = requests.get(f"{url}/controlnet/model_list", timeout=10)
    r.raise_for_status()
    models = r.json().get("model_list", [])
    result = {}
    for m in models:
        name = m.lower()
        if "depth" in name:
            result["depth"] = m
        if "ip-adapter" in name:
            result["ip_adapter"] = m
    return result


def build_payload(board_b64: str, args, cn_models: dict, ref_b64: str = None) -> dict:
    cn_units = []

    # Unit 0: Depth ControlNet for pose preservation
    depth_model = cn_models.get("depth")
    if not depth_model:
        sys.exit("No depth ControlNet model found. Install controlnet-depth-sdxl.")
    cn_units.append({
        "enabled": True,
        "image": board_b64,
        "module": args.depth_preprocessor,
        "model": depth_model,
        "weight": args.depth_weight,
        "resize_mode": "Scale to Fit (Inner Fit)",
        "control_mode": "Balanced",
        "pixel_perfect": True,
    })

    # Unit 1: IP-Adapter for reference character look (optional)
    if ref_b64 and "ip_adapter" in cn_models:
        cn_units.append({
            "enabled": True,
            "image": ref_b64,
            "module": "ip-adapter_clip_sdxl_plus_vith",
            "model": cn_models["ip_adapter"],
            "weight": args.ip_weight,
            "resize_mode": "Scale to Fit (Inner Fit)",
            "control_mode": "ControlNet is more important",
            "pixel_perfect": True,
        })

    negative = args.negative or (
        "human, knight, blurry, deformed, watermark, grass, forest, "
        "realistic photo, text, signature"
    )

    return {
        "init_images": [board_b64],
        "prompt": args.style,
        "negative_prompt": negative,
        "steps": args.steps,
        "cfg_scale": args.cfg,
        "denoising_strength": args.denoise,
        "width": 1024,
        "height": 1024,
        "sampler_name": args.sampler,
        "seed": args.seed,
        "alwayson_scripts": {
            "controlnet": {
                "args": cn_units
            }
        },
    }


# --- state management ---

def load_state(boards_dir: Path) -> dict:
    p = boards_dir / STATE_FILE
    if p.exists():
        return json.loads(p.read_text())
    return {"completed": [], "failed": []}


def save_state(boards_dir: Path, state: dict):
    (boards_dir / STATE_FILE).write_text(json.dumps(state, indent=2))


# --- main ---

def cmd_submit(args):
    boards_dir = Path(args.boards_dir)
    if not boards_dir.is_dir():
        sys.exit(f"Not a directory: {boards_dir}")

    manifest_path = boards_dir / "_boards_manifest.json"
    if not manifest_path.exists():
        sys.exit(f"No _boards_manifest.json in {boards_dir}")

    opts = check_server(args.url)
    cn_models = get_controlnet_models(args.url)
    print(f"  ControlNet models: {cn_models}")

    # Load reference image if provided
    ref_b64 = None
    if args.reference:
        ref_path = Path(args.reference)
        if not ref_path.exists():
            sys.exit(f"Reference image not found: {ref_path}")
        ref_b64 = img_to_b64(str(ref_path))
        print(f"  Reference: {ref_path}")

    # Collect board PNGs
    all_boards = sorted(
        p for p in boards_dir.glob("*.png")
        if not p.name.startswith("_") and not p.name.startswith("test_")
    )

    # Filter by entities if specified
    if args.entities:
        entities = [e.strip() for e in args.entities.split(",")]
        all_boards = [
            b for b in all_boards
            if any(b.name.startswith(f"{e}__") for e in entities)
        ]

    # Create output directory
    out_dir = boards_dir / "edited"
    out_dir.mkdir(exist_ok=True)

    # Load state for resume
    state = load_state(boards_dir)
    completed = set(state["completed"])

    pending = [b for b in all_boards if b.name not in completed]
    total = len(all_boards)
    done = len(completed)

    if not pending:
        print(f"All {total} boards already processed.")
        return

    print(f"\nProcessing {len(pending)} boards ({done}/{total} already done)")
    print(f"  Style: {args.style}")
    print(f"  Steps: {args.steps}, Denoise: {args.denoise}, Seed: {args.seed}")
    print(f"  Sampler: {args.sampler}")
    print()

    for i, board_path in enumerate(pending):
        idx = done + i + 1
        print(f"[{idx}/{total}] {board_path.name} ...", end=" ", flush=True)

        board_b64 = img_to_b64(str(board_path))
        original = Image.open(board_path)
        payload = build_payload(board_b64, args, cn_models, ref_b64)

        t0 = time.time()
        try:
            r = requests.post(
                f"{args.url}/sdapi/v1/img2img",
                json=payload,
                timeout=args.timeout,
            )
            r.raise_for_status()
        except requests.exceptions.Timeout:
            print(f"TIMEOUT ({args.timeout}s)")
            state["failed"].append(board_path.name)
            save_state(boards_dir, state)
            continue
        except requests.exceptions.RequestException as e:
            print(f"ERROR: {e}")
            state["failed"].append(board_path.name)
            save_state(boards_dir, state)
            continue

        result = r.json()
        images = result.get("images", [])
        elapsed = time.time() - t0

        if not images:
            print(f"NO IMAGE ({elapsed:.0f}s)")
            state["failed"].append(board_path.name)
        else:
            generated = b64_to_pil(images[0])
            output = restore_alpha(generated, original)
            out_path = out_dir / board_path.name
            output.save(out_path)
            print(f"OK ({elapsed:.0f}s)")
            state["completed"].append(board_path.name)

        save_state(boards_dir, state)

    n_ok = len(state["completed"])
    n_fail = len(state["failed"])
    print(f"\nDone. {n_ok} succeeded, {n_fail} failed.")
    print(f"Edited boards: {out_dir}")
    if n_fail:
        print(f"Failed: {state['failed']}")
        print("Re-run with --resume to retry failed boards.")


def main():
    parser = argparse.ArgumentParser(
        description="Restyle sprite boards via local Stable Diffusion"
    )
    sub = parser.add_subparsers(dest="command")

    p_submit = sub.add_parser("submit", help="Process boards through A1111")
    p_submit.add_argument("boards_dir", help="Path to boards/ directory")
    p_submit.add_argument("--style", required=True,
        help="Positive prompt describing target style")
    p_submit.add_argument("--negative", default=None,
        help="Negative prompt (has sensible default)")
    p_submit.add_argument("--reference", action="append", default=[],
        help="Reference image for IP-Adapter (can specify multiple)")
    p_submit.add_argument("--entities", default=None,
        help="Comma-separated entity filter (e.g. footman,peasant)")
    p_submit.add_argument("--steps", type=int, default=15,
        help="Sampling steps (default: 15)")
    p_submit.add_argument("--cfg", type=float, default=7.0,
        help="CFG scale (default: 7.0)")
    p_submit.add_argument("--denoise", type=float, default=0.70,
        help="Denoising strength (default: 0.70)")
    p_submit.add_argument("--seed", type=int, default=12345,
        help="Fixed seed for consistency (default: 12345)")
    p_submit.add_argument("--sampler", default="Euler a",
        help="Sampler name (default: Euler a)")
    p_submit.add_argument("--depth-preprocessor", default="depth_anything_v2",
        help="Depth preprocessor (default: depth_anything_v2)")
    p_submit.add_argument("--depth-weight", type=float, default=1.0,
        help="Depth ControlNet weight (default: 1.0)")
    p_submit.add_argument("--ip-weight", type=float, default=0.8,
        help="IP-Adapter weight (default: 0.8)")
    p_submit.add_argument("--url", default=DEFAULT_URL,
        help=f"A1111 API URL (default: {DEFAULT_URL})")
    p_submit.add_argument("--timeout", type=int, default=900,
        help="Per-board timeout in seconds (default: 900)")
    p_submit.add_argument("--resume", action="store_true",
        help="Resume interrupted batch (skip completed boards)")

    args = parser.parse_args()
    if not args.command:
        parser.print_help()
        sys.exit(1)

    # Normalize --reference: accept multiple flags, pick first for now
    if isinstance(args.reference, list):
        args.reference = args.reference[0] if args.reference else None

    cmd_submit(args)


if __name__ == "__main__":
    main()

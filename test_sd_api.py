#!/usr/bin/env python3
"""Single-image A1111 img2img test: ControlNet depth (+ optional IP-Adapter), alpha restore.

Default board/ref paths live under ai-working/ (gitignored; present locally after unpack/pipeline).
"""

import argparse, base64, io, json, sys
from typing import Optional

import requests
from PIL import Image

DEFAULT_URL = "http://localhost:7860"
DEFAULT_BOARD = "ai-working/race-pipeline-human-red/boards/footman__stay__b0.png"
DEFAULT_OUT = "ai-working/race-pipeline-human-red/boards/test_output.png"
DEFAULT_REF = "ai-working/race-pipeline-human-red/orcs-racs/orc_warrior_2.png"


def img_to_b64(path: str) -> str:
    with open(path, "rb") as f:
        return base64.b64encode(f.read()).decode()


def restore_alpha(generated: Image.Image, original: Image.Image) -> Image.Image:
    gen = generated.convert("RGBA")
    if original.mode == "RGBA":
        gen.putalpha(original.split()[3])
    return gen


def fetch_cn_models(url: str) -> dict:
    r = requests.get(f"{url}/controlnet/model_list", timeout=15)
    r.raise_for_status()
    out = {}
    for m in r.json().get("model_list", []):
        ml = m.lower()
        if "depth" in ml:
            out["depth"] = m
        if "ip-adapter" in ml:
            out["ip_adapter"] = m
    return out


def build_payload(
    board_b64: str,
    url: str,
    steps: int,
    denoise: float,
    seed: int,
    prompt: str,
    negative: str,
    ref_b64: Optional[str],
    width: int,
    height: int,
) -> dict:
    cn = fetch_cn_models(url)
    depth_m = cn.get("depth")
    if not depth_m:
        sys.exit("No depth ControlNet model (need SDXL depth + SDXL checkpoint).")

    units = [
        {
            "enabled": True,
            "image": board_b64,
            "module": "depth_anything_v2",
            "model": depth_m,
            "weight": 1.0,
            "resize_mode": "Scale to Fit (Inner Fit)",
            "control_mode": "Balanced",
            "pixel_perfect": True,
        }
    ]
    if ref_b64 and cn.get("ip_adapter"):
        units.append(
            {
                "enabled": True,
                "image": ref_b64,
                "module": "ip-adapter_clip_sdxl_plus_vith",
                "model": cn["ip_adapter"],
                "weight": 0.8,
                "resize_mode": "Scale to Fit (Inner Fit)",
                "control_mode": "ControlNet is more important",
                "pixel_perfect": True,
            }
        )

    return {
        "init_images": [board_b64],
        "prompt": prompt,
        "negative_prompt": negative,
        "steps": steps,
        "cfg_scale": 7.0,
        "denoising_strength": denoise,
        "width": width,
        "height": height,
        "sampler_name": "Euler a",
        "seed": seed,
        "alwayson_scripts": {"controlnet": {"args": units}},
    }


def main():
    p = argparse.ArgumentParser(description="One img2img test against A1111")
    p.add_argument("--url", default=DEFAULT_URL)
    p.add_argument("-i", "--input", default=DEFAULT_BOARD, help="Board PNG (RGBA)")
    p.add_argument("-o", "--output", default=DEFAULT_OUT)
    p.add_argument("--reference", default=None, help="Optional IP-Adapter ref image")
    p.add_argument("--no-reference", action="store_true", help="Depth only")
    p.add_argument("--steps", type=int, default=15)
    p.add_argument("--denoise", type=float, default=0.70)
    p.add_argument("--seed", type=int, default=12345)
    p.add_argument(
        "--prompt",
        default=(
            "claymation style orc warrior sprite sheet, green skin, leather armor, "
            "bone details, isometric view, transparent background, masterpiece, high detail"
        ),
    )
    p.add_argument(
        "--negative",
        default=(
            "human, knight, blurry, deformed, watermark, grass, forest, "
            "realistic photo, text"
        ),
    )
    p.add_argument("--timeout", type=int, default=900)
    p.add_argument(
        "--width",
        type=int,
        default=1024,
        help="img2img resolution (1024 needs a lot of VRAM; try 512 if the desktop freezes)",
    )
    p.add_argument("--height", type=int, default=1024)
    args = p.parse_args()

    if args.width >= 1024 or args.height >= 1024:
        print(
            "Note: 1024² SDXL + ControlNet is very VRAM-heavy; if the whole session freezes, "
            "use e.g. --width 512 --height 512 and/or A1111 --medvram / --lowvram.",
            file=sys.stderr,
        )

    ref_path = None if args.no_reference else (args.reference or DEFAULT_REF)
    ref_b64 = None
    if ref_path:
        try:
            ref_b64 = img_to_b64(ref_path)
            print(f"Reference: {ref_path}")
        except OSError:
            print(f"No reference file ({ref_path}), depth only.")
            ref_b64 = None

    board_b64 = img_to_b64(args.input)
    original = Image.open(args.input)
    payload = build_payload(
        board_b64,
        args.url,
        args.steps,
        args.denoise,
        args.seed,
        args.prompt,
        args.negative,
        ref_b64,
        args.width,
        args.height,
    )

    print(f"POST {args.url}/sdapi/v1/img2img")
    print(f"Input: {args.input} {original.size} {original.mode}")
    r = requests.post(
        f"{args.url}/sdapi/v1/img2img", json=payload, timeout=args.timeout
    )
    if r.status_code != 200:
        print(f"Error {r.status_code}: {r.text[:800]}")
        sys.exit(1)

    result = r.json()
    images = result.get("images", [])
    print(f"Got {len(images)} image(s)")
    if not images:
        sys.exit(1)

    gen = Image.open(io.BytesIO(base64.b64decode(images[0])))
    out = restore_alpha(gen, original)
    out.save(args.output)
    print(f"Saved: {args.output} ({out.size} {out.mode})")
    info = json.loads(result.get("info", "{}"))
    print(f"Seed: {info.get('seed')}, Steps: {info.get('steps')}")


if __name__ == "__main__":
    main()

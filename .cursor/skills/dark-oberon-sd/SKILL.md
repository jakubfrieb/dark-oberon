---
name: dark-oberon-sd
description: >-
  Local Stable Diffusion (Automatic1111) integration for restyling Dark Oberon
  sprite boards.  Uses ControlNet (depth + IP-Adapter) via A1111 API at
  localhost:7860.  Alternative to the GPT Image cloud pipeline — runs fully
  offline on local AMD/NVIDIA GPU.  Use when the user mentions stable diffusion,
  A1111, local SD, ControlNet, IP-Adapter, sd_job, sd batch, or local restyling.
---

# Dark Oberon — Local Stable Diffusion pipeline

## Prerequisites

| Component | Details |
|-----------|---------|
| A1111 | Docker stack at `http://localhost:7860` (profile `auto-amd` or `auto`) |
| Checkpoint | SDXL recommended: `RealVisXL_V5.0_fp16.safetensors` or `sd_xl_base_1.0` |
| ControlNet models | `controlnet-depth-sdxl-fp16`, `ip-adapter-plus_sdxl_vit-h` |
| ControlNet units | >= 2 (Settings → ControlNet → Multi-ControlNet) |
| startup.sh fix | mediapipe==0.10.14 (auto-applied via `/data/config/auto/startup.sh`) |
| Python (host) | `pip install requests Pillow` |

## Quick start

### 1. Ensure A1111 is running

```bash
docker compose -f ~/webspace/ai-labbing/docker-stable-diffusion/docker-compose.yml \
  --profile auto-amd up -d
```

Verify:
```bash
curl -s http://localhost:7860/sdapi/v1/options | python3 -c \
  "import sys,json; print(json.load(sys.stdin)['sd_model_checkpoint'])"
```

### 2. Switch to SDXL (if not already)

```bash
curl -s -X POST http://localhost:7860/sdapi/v1/options \
  -H "Content-Type: application/json" \
  -d '{"sd_model_checkpoint": "RealVisXL_V5.0_fp16.safetensors"}'
```

### 3. Compose sprite boards

Use the race pipeline to extract 1024x1024 boards from the source race:

```bash
bash .cursor/skills/dark-oberon-dat/scripts/race_pipeline.sh compose \
  --source races/human-red
bash .cursor/skills/dark-oberon-dat/scripts/race_pipeline.sh boards \
  --work-dir ai-working/race-pipeline-human-red
```

### 4. Submit boards to local SD

```bash
python3 .cursor/skills/dark-oberon-dat/scripts/run_sd_board_batch.py submit \
  ai-working/race-pipeline-human-red/boards \
  --style "claymation orc warrior, green skin, leather armor, tusks, isometric" \
  --reference ai-working/race-pipeline-human-red/orcs-racs/orc_warrior_2.png \
  --entities footman \
  --steps 15 --denoise 0.72 --seed 12345
```

To process ALL entities (full race), omit `--entities`.

### 5. Monitor progress

```bash
python3 .cursor/skills/dark-oberon-sd/tools/sd_job_monitor.py \
  ai-working/race-pipeline-human-red/boards
```

### 6. Unpack and finalize

```bash
bash .cursor/skills/dark-oberon-dat/scripts/race_pipeline.sh unboards \
  --work-dir ai-working/race-pipeline-human-red
bash .cursor/skills/dark-oberon-dat/scripts/race_pipeline.sh finalize \
  --work-dir ai-working/race-pipeline-human-red \
  --output races/orc-green --race-id orc-green --race-name "Orc Horde"
```

## How it works

### ControlNet setup (2 units)

| Unit | Purpose | Preprocessor | Model | Weight |
|------|---------|-------------|-------|--------|
| 0 — Pose | Preserves sprite layout, direction, silhouette | `depth_anything_v2` | `controlnet-depth-sdxl-fp16` | 1.0 |
| 1 — Style | Transfers character look from reference image | `ip-adapter_clip_sdxl_plus_vith` | `ip-adapter-plus_sdxl_vit-h` | 0.8 |

Unit 0 ensures the orc stays in the same pose/direction as the original footman.
Unit 1 (optional) makes the orc look like the reference concept art.

### Key parameters

| Parameter | Default | Notes |
|-----------|---------|-------|
| `--steps` | 15 | More steps = better quality but slower. 15 is good for SDXL. |
| `--denoise` | 0.70 | 0.65–0.75 sweet spot. Lower = more original, higher = more creative. |
| `--seed` | 12345 | Fixed seed reduces flickering between animation frames. |
| `--cfg` | 7.0 | CFG scale. Higher = more prompt adherence. |
| `--sampler` | `Euler a` | Fast and reliable for SDXL. |
| `--ip-weight` | 0.8 | IP-Adapter influence. Higher = closer to reference. |
| `--depth-weight` | 1.0 | Depth ControlNet influence. Keep at 1.0 for pose fidelity. |
| `--timeout` | 900 | Per-board timeout in seconds. |

### Alpha restoration

A1111 outputs RGB (no transparency). The script automatically applies the alpha
channel from the original RGBA board onto the generated output, preserving the
transparent background needed for game sprites.

### Performance (AMD Radeon, ROCm 6.2)

| Resolution | Steps | Time/board | 63 boards |
|-----------|-------|------------|-----------|
| 1024x1024 | 15 | ~5 min | ~5.25 hours |
| 1024x1024 | 25 | ~8.5 min | ~9 hours |

Batch supports `--resume` — interrupted runs pick up where they left off.

### State files

| File | Location | Purpose |
|------|----------|---------|
| `_sd_batch_state.json` | `boards/` | Tracks completed/failed boards for resume |
| `_boards_manifest.json` | `boards/` | Board layout manifest (from pack step) |

## Recommended reference images

Place concept art in the work directory. Good references:
- Front-facing character portrait (clay/figurine style works best)
- Consistent lighting, neutral background
- Similar proportions to the original sprites

Available orc references: `ai-working/race-pipeline-human-red/orcs-racs/`

## Docker infrastructure

| Path | Purpose |
|------|---------|
| `~/webspace/ai-labbing/docker-stable-diffusion/` | Docker compose stack |
| `docker-compose.yml` profile `auto-amd` | ROCm service definition |
| `/data/config/auto/startup.sh` | Persistent startup fixes (mediapipe pin) |
| `/data/models/Stable-diffusion/` | Checkpoint storage |
| `/data/models/ControlNet/` | ControlNet model storage |

## Troubleshooting

### "Script 'controlnet' not found"
ControlNet script failed to load. Check container logs:
```bash
docker logs webui-docker-auto-amd-1 2>&1 | grep "Error loading script"
```
Usually caused by broken mediapipe. Verify startup.sh ran:
```bash
docker logs webui-docker-auto-amd-1 2>&1 | grep "\[startup.sh\]"
```

### GPU not detected
Check `HSA_OVERRIDE_GFX_VERSION` in docker-compose.yml matches your GPU.
Current: `11.0.0` (for Radeon 8060S / gfx1151 → gfx1100 compat).

### Timeout on generation
SDXL at 1024x1024 takes ~5 min/board on AMD. Increase `--timeout` or reduce
`--steps`. For SD 1.5 (much faster), you need SD 1.5 ControlNet models.

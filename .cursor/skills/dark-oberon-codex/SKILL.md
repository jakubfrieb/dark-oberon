---
name: dark-oberon-codex
description: >-
  Cloud image-generation pipeline for restyling Dark Oberon sprite boards using
  the OpenAI image stack — either via the OpenAI Batch API (default, scalable)
  or the local `codex exec` CLI (per-board re-rolls). Loads OPENAI_API_KEY from
  `.env` at the repo root. Use when the user mentions codex, gpt-image, OpenAI
  image, board batch, race restyle, or wants to regenerate sprite boards.
---

# Dark Oberon — Codex / OpenAI image pipeline

This skill restyles 1024x1024 sprite boards extracted from a Dark Oberon race
using OpenAI image models. There are two execution paths:

| Path | When | Script |
|------|------|--------|
| **OpenAI Batch API** (default) | Whole entity / whole race; pay-per-image with batch discount | `.cursor/skills/dark-oberon-dat/scripts/run_openai_board_batch.py` |
| **`codex exec` CLI** (opt-in) | Re-roll a single board, ad-hoc fixes, or when the user is logged in via ChatGPT subscription and wants to avoid API billing | `.cursor/skills/dark-oberon-codex/tools/run_codex_board.sh` |

## Prerequisites

| Component | Details |
|-----------|---------|
| Python | `pip install openai Pillow` |
| Auth (Batch API) | `OPENAI_API_KEY` in `.env` at the repo root (see `.env.example`). The wrapper auto-loads it. |
| Auth (codex CLI) | Either `codex login` (ChatGPT subscription) **or** `OPENAI_API_KEY` from `.env`. |
| Codex CLI | Install from https://github.com/openai/codex (only needed for the per-board path). |

`.env` is gitignored. Never commit the real key. If the user pastes a key in
chat, tell them to rotate it.

## Quick start

### 1. Configure secrets

```bash
cp .env.example .env
# edit .env and set OPENAI_API_KEY=sk-...
```

### 2. Compose sprite boards

Use the race pipeline to extract 1024x1024 boards from the source race:

```bash
bash .cursor/skills/dark-oberon-dat/scripts/race_pipeline.sh compose \
  --source races/human-red
bash .cursor/skills/dark-oberon-dat/scripts/race_pipeline.sh boards \
  --work-dir ai-working/race-pipeline-human-red
```

### 3. Submit boards (Batch API — default)

```bash
python3 .cursor/skills/dark-oberon-dat/scripts/run_openai_board_batch.py submit \
  ai-working/race-pipeline-human-red/boards \
  --reference ai-working/race-pipeline-human-red/orcs-racs/orc_warrior_2.png \
  --style "claymation orc warrior, green skin, leather armor, tusks, isometric" \
  --quality medium \
  --entities footman
```

To process ALL entities, omit `--entities`.

### 4. Poll and download

```bash
python3 .cursor/skills/dark-oberon-dat/scripts/run_openai_board_batch.py poll \
  ai-working/race-pipeline-human-red/boards

python3 .cursor/skills/dark-oberon-dat/scripts/run_openai_board_batch.py download \
  ai-working/race-pipeline-human-red/boards
```

Downloaded edited boards land in `ai-working/race-pipeline-human-red/boards/edited/`.

### 5. Monitor progress

```bash
python3 .cursor/skills/dark-oberon-codex/tools/codex_job_monitor.py \
  ai-working/race-pipeline-human-red/boards
```

Add `--watch` for live polling, `--reset` to clear state, `--preview` to open
the latest edited board.

### 6. Unpack and finalize

```bash
bash .cursor/skills/dark-oberon-dat/scripts/race_pipeline.sh unboards \
  --work-dir ai-working/race-pipeline-human-red
bash .cursor/skills/dark-oberon-dat/scripts/race_pipeline.sh finalize \
  --work-dir ai-working/race-pipeline-human-red \
  --output races/orc-green --race-id orc-green --race-name "Orc Horde"
```

## Per-board re-roll (codex CLI)

When the batch produces a bad board, re-roll just that one via codex:

```bash
.cursor/skills/dark-oberon-codex/tools/run_codex_board.sh \
  ai-working/race-pipeline-human-red/boards/footman__walk__n.png \
  ai-working/race-pipeline-human-red/orcs-racs/orc_warrior_2.png \
  ai-working/race-pipeline-human-red/boards/edited/footman__walk__n.png \
  "claymation orc warrior, green skin, leather armor, tusks, isometric"
```

The script:
- Sources `.env` (if present) so `OPENAI_API_KEY` is available to codex.
- Calls `codex exec --sandbox workspace-write` with a one-paragraph prompt.
- Codex picks the appropriate image model under the active account (no
  `--oss` / `--local-provider` unless the user explicitly asks).

## Prompt rules

- **One flowing positive paragraph.** Image models often invert `NEGATIVE:`
  blocks — express don't-wants as positives ("smooth painted illustration, no
  pixel-art look, daylight only").
- **Preserve pose & grid.** Always include: *"preserve exact pose, silhouette,
  facing direction; keep each sprite within its current grid cell."*
- **Transparent background guarantee.** Sprite boards must come back as PNG
  with alpha. The Batch script sets `background: "transparent"` and
  `input_fidelity: "high"` already — keep those.
- **Empty-of-text guarantee.** Append: *"no text, no letters, no watermark,
  no UI overlay, no signature."*

## Key parameters (Batch API)

| Flag | Default | Notes |
|------|---------|-------|
| `--quality` | `medium` | `low` / `medium` / `high`. Higher = more detail, more cost. |
| `--model` | `gpt-image-1.5` | Use `gpt-image-1` if org isn't verified. |
| `--reference` | required | One or more identity reference PNGs (concept art). |
| `--style` | required | Free-form style description appended to the prompt. |
| `--entities` | all | CSV filter, e.g. `footman,peasant`. |

Fixed in the request body: `size: 1024x1024`, `background: transparent`,
`input_fidelity: high`, `output_format: png`, `n: 1`.

## Cost guidance

`gpt-image-1.5` at `medium` quality is roughly $0.04–0.17 per 1024² image.
A full race of ~63 boards ≈ $3–$11. Confirm with the user before kicking off
a full-race batch on a fresh API key.

## State files

| File | Location | Purpose |
|------|----------|---------|
| `_batch_state.json` | `boards/` | OpenAI batch id, file ids, status (set by `run_openai_board_batch.py`) |
| `_batch_input.jsonl` | `boards/` | The JSONL batch payload submitted to OpenAI |
| `_boards_manifest.json` | `boards/` | Board layout manifest (from the pack step) |
| `edited/` | `boards/` | Downloaded restyled boards (custom_id == board filename) |

## Recommended reference images

Place concept art in the work directory. Good references:
- Front-facing character portrait (clay/figurine style works best).
- Consistent lighting, neutral background.
- Similar proportions to the original sprites.

Available orc references: `ai-working/race-pipeline-human-red/orcs-racs/`.

## Troubleshooting

### `openai.AuthenticationError`
- Check `.env` exists at repo root and contains a valid `OPENAI_API_KEY`.
- The Batch script auto-loads `.env` from the repo root; for codex, the wrapper
  shell-sources `.env` before invoking `codex exec`.

### Batch stuck in `validating` / `in_progress`
OpenAI batches complete within a 24h window. Use `poll` to keep checking, or
log in to https://platform.openai.com/batches to inspect manually.

### `gpt-image-1.5` not available
Your org may not be verified for the 1.5 family — pass `--model gpt-image-1`.

### Edited board has baked text or wrong style
- Re-roll that single board with the `codex` per-board script after tweaking
  the style string.
- If the whole batch is off, fix the `--style` argument and resubmit only the
  failing entities via `--entities`.

## Security

- `.env` is gitignored — never commit it.
- Never echo the API key to the chat or to log files.
- If a key is leaked (pasted in chat, accidentally committed), rotate it at
  https://platform.openai.com/api-keys before continuing.

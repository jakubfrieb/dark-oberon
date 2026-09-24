---
name: dark-oberon-race
description: >-
  Create new Dark Oberon races by restyling existing ones via sprite-sheet
  pipeline.  Covers the full workflow: extract graphics, compose sprite sheets,
  send to AI model for restyling, slice back, pack into DAT+RAC.  Includes an
  AI board pipeline for automated restyling via GPT Image (gpt-image-1.5)
  with OpenAI Batch API.  Use when the user mentions new race, race creation,
  prepare race data, restyle race, sprite sheets for race, orc race, AI boards,
  board pipeline, GPT Image restyling, or any race-related asset workflow.
---

# Dark Oberon — new race creation

## Quick start (agent workflow)

When the user asks to prepare data for a new race or create a new race:

1. **Ask which source race** to base it on (use AskQuestion if available).
   Currently available: `human-red` (`races/human-red/`).
2. **Ask how sprites should be edited** — manual paint, or AI restyling via
   the board pipeline (GPT Image).
3. **Run the compose phase** — produces per-entity PNG sprite sheets:
   ```bash
   bash .cursor/skills/dark-oberon-dat/scripts/race_pipeline.sh compose \
     --source races/human-red
   ```

**If manual editing:**

4. Show the user the sheets directory and list the generated PNGs.
5. When the user has edited sheets and provides `--race-id` / `--race-name`,
   run finalize.

**If AI board pipeline (GPT Image restyling):**

4. Pack AI boards, submit to OpenAI, download results, unpack — see
   [AI board pipeline](#ai-board-pipeline-gpt-image-restyling) below.
5. Run finalize.

**Finalize** (both paths):
```bash
bash .cursor/skills/dark-oberon-dat/scripts/race_pipeline.sh finalize \
  --work-dir <work-dir-from-compose> \
  --output races/<race-id> \
  --race-id <race-id> \
  --race-name "<Display Name>"
```

## Source races

| Race ID | Dir | Entities |
|---------|-----|----------|
| `human-red` | `races/human-red/` | 5 units (footman, peasant, moleman, catapult, airship) + 11 buildings |

Other color variants (`human-yellow`, `human-blue`) share the same DAT as human-red — only the `.rac` differs. Use `human-red` as the base.

## Pipeline overview

```
source.dat ──unpack──▸ manifest.json + TGAs
                         │
                    compose_sheets.py ──▸ per-entity PNG + layout JSON
                         │
              ┌──────────┴──────────┐
              │                     │
         [manual edit]    pack_ai_boards.py ──▸ 1024x1024 boards
                                    │
                          [GPT Image / Batch API edit]
                                    │
                          unpack_ai_boards.py
              │                     │
              └──────────┬──────────┘
                         │
                    slice_sheets.py ──▸ updated TGAs
                         │
                    pack_dat ──▸ new-race.dat
source.rac ──generate_rac.py──▸ new-race.rac
```

Scripts: `.cursor/skills/dark-oberon-dat/scripts/` — see [DAT skill](../dark-oberon-dat/SKILL.md) for individual script docs.

## AI board pipeline (GPT Image restyling)

Automated restyling via OpenAI `gpt-image-1.5` + Batch API.
Requires: `OPENAI_API_KEY` env var, `pip install openai Pillow`.

### Steps

1. **Compose** (same as manual flow):
   ```bash
   bash .cursor/skills/dark-oberon-dat/scripts/race_pipeline.sh compose \
     --source races/human-red
   ```

2. **Pack AI boards** — arranges sprites into 1024x1024 pose-filled canvases:
   ```bash
   bash .cursor/skills/dark-oberon-dat/scripts/race_pipeline.sh boards \
     --work-dir <work-dir>
   ```
   Output: `<work-dir>/boards/` with board PNGs + `_boards_manifest.json`.
   For `human-red` this produces **63 boards** across 16 entities.

3. **Submit to OpenAI Batch API** — sends boards + identity reference:
   ```bash
   python3 .cursor/skills/dark-oberon-dat/scripts/run_openai_board_batch.py \
     submit <work-dir>/boards \
     --reference identity.png \
     --style "dark orc warriors with green skin and bone armor" \
     [--entities footman,peasant] \
     [--quality medium] [--model gpt-image-1.5]
   ```
   Use `--entities` to pilot on a single unit first (recommended).
   Then poll and download:
   ```bash
   python3 .cursor/skills/dark-oberon-dat/scripts/run_openai_board_batch.py \
     poll <work-dir>/boards
   python3 .cursor/skills/dark-oberon-dat/scripts/run_openai_board_batch.py \
     download <work-dir>/boards
   ```
   Edited boards are saved to `<work-dir>/boards/edited/`.

4. **Unpack boards** — restores edited boards into entity sheets:
   ```bash
   bash .cursor/skills/dark-oberon-dat/scripts/race_pipeline.sh unboards \
     --work-dir <work-dir>
   ```

5. **Finalize** (same as manual flow).

### How boards work

Each board is a **1024x1024 transparent PNG** containing one animation group's
sprites arranged in a grid, scaled up to maximize canvas usage.  The packer
finds the optimal grid layout per group (e.g. 3x3 for 8 stay directions,
2x4 for 8 move strips).

Boards are named `{entity}__{animation}__b{n}.png`
(e.g. `footman__stay__b0.png`, `barracks__build__b0.png`).

The manifest `_boards_manifest.json` records slot source/destination rects
and scale factors for lossless unpacking.  Batch state is tracked in
`_batch_state.json`.

### Identity pack

Prepare one or more reference images that define the target character style.
These are sent as the first input(s) to every board edit request via
`input_fidelity: high` for visual consistency.  Good references:

- concept art / character portrait
- 8-direction turnaround sheet
- palette / material card

Multiple `--reference` paths can be passed to the submit command.

### Cost and quality

- Start with `--quality medium` (~$0.034/board at 1024x1024).  Upgrade to
  `high` (~$0.133/board) only if medium output is insufficient.
- Batch API gives **50% cost discount** vs synchronous calls.
- For `human-red` (63 boards at medium): estimated ~$2–4 total.
- Use `--entities footman` to pilot a single unit (~5 boards, ~$0.20) before
  committing to the full race.

## Sprite sheet structure

One PNG per entity (unit or building).

- **Rows** = texture groups (`tg_stay_id`, `tg_move_id`, `tg_attack_id`, …).
- **Columns** = 8 directions (S, SW, W, NW, N, NE, E, SE) for units, or build stages for buildings.
- **Guide borders**: 1px magenta (#FF00FF) between cells.
- **Label margin** (120 px left): group name shorthand.
- **Layout JSON** (`*_layout.json`): exact pixel coordinates for every cell — the slicer reads ONLY this, not the visual guides.

### Row statuses

| Status | Meaning | What the model should do |
|--------|---------|--------------------------|
| `present` | Sprites are in the cells | Restyle the sprites, keep exact cell dimensions |
| `alias` | This `tg_*` reuses another group (e.g. peasant move = stay) | Can be left empty or filled to create a new separate animation |
| `shared` | Group owned by another entity's sheet | Do not edit here — edit it on the owner's sheet |
| `missing` | `tg_*` is `none` in the .rac | Can be left empty or filled to add a new animation |

## Race file anatomy

A race consists of two files in `races/<id>/`:

| File | Content |
|------|---------|
| `<id>.rac` | Text config: entity definitions, stats, `tg_*` texture group refs, `snd_*` sound refs |
| `<id>.dat` | Binary bundle: texture groups (TGA) + sounds (WAV/OGG) |

### Entity types

| `item_type` | Class | Key tg_* groups |
|-------------|-------|-----------------|
| `f` | Fighter (TFORCE_ITEM) | picture, stay, move, attack, rotate, anchor, land, dying, zombie, projectile, burning |
| `w` | Worker (TWORKER_ITEM) | same as fighter + mine, repair |
| `b` | Building | picture, stay, build, dying, zombie, projectile, burning |
| `a` | Factory (building that produces units) | same as building + `<Products>` section |

### 8-direction sprites

Engine rule (from `TFORCE_UNIT::ChangeAnimation`):
- If a texture group has **>= 8 textures** → index 0..7 = directions S, SW, W, NW, N, NE, E, SE.
- If **< 8 textures** → always uses index 0 (single sprite, no rotation).

### Animation frames within one TGA

A single TGA can hold a grid of frames: `hcount` x `vcount` cells, played over `atime` ms total. Engine splits: `frame_width = tga_width / hcount`, `frame_height = tga_height / vcount`.

## Rules for the AI model / artist

When editing sprite sheets:

1. **Do NOT change PNG dimensions.** The slicer will reject mismatched sizes.
2. **Keep sprites within their cell boundaries** (magenta guides show where).
3. **Maintain alpha transparency** — the game uses RGBA compositing.
4. **Preserve animation frame layout** — if a cell contains a 3x2 frame grid (hcount=3, vcount=2), the new art must use the same grid.
5. **Power-of-two texture sizes** are preferred by the engine (64, 128, 256, 512). The compose/slice pipeline preserves original sizes.

## Testing the new race

After finalize:

1. Copy the output directory into `races/` (if not already there).
2. Edit a `.map` file to reference the new race:
   ```
   race "<race-id>"
   ```
3. Launch the game and load the map.
4. Check all units/buildings render correctly, animations play, directions work.

## Reference

- Full `.rac` field spec: [`docs/RACE_SPEC_FOR_AI.md`](../../docs/RACE_SPEC_FOR_AI.md)
- DAT format + scripts: [dark-oberon-dat skill](../dark-oberon-dat/SKILL.md)
- Engine loaders: [`src/doraces.cpp`](../../src/doraces.cpp), [`src/dodata.cpp`](../../src/dodata.cpp)

## Codex pipeline (modelína) — orci

Kreslí `codex exec` (obrázky generuje nativně ~1254 px a sám je zmenší); vše okolo je deterministické a pokryté testy (`tests/race_pipeline`).

```bash
S=.cursor/skills/dark-oberon-dat/scripts; W=$PWD/ai-working/race-pipeline-orc
bash $S/race_pipeline.sh compose --source $PWD/races/human-red --work-dir $W
bash $S/race_pipeline.sh boards  --work-dir $W
python3 $S/run_codex_board_batch.py design  $W --entities footman --refs ai-working/race-pipeline-human-red/orcs-racs
python3 $S/run_codex_board_batch.py approve $W footman          # až po schválení design sheetu
python3 $S/run_codex_board_batch.py restyle $W --only footman --parallel 4
bash $S/race_pipeline.sh codex-post --work-dir $W --only footman  # validace + alfa z originálu + unboards
bash $S/race_pipeline.sh finalize --work-dir $W --output races/orc-red --race-id orc-red \
  --race-name "Plastic Orcs - Red" --names-json $S/orc_entities.json
bash $S/race_pipeline.sh variants --source races/orc-red --race-prefix orc --name-prefix "Plastic Orcs"
```

Zjištění z pilotu (Grunt + War Camp):
- Codex dostává jen **výřez boardu ohraničený sloty** (jinak u portrétu nakreslí malý objekt do rohu).
- Budovy: design sheet i restyle musí **zachovat tvar lidské budovy** (jinak codex zkopíruje tvar z design sheetu).
- Prompt vysvětluje typ animace: `picture` = portrét od okraje k okraji, `build` = fáze stavby (nedokončené), `zombie` = trosky.
- Post-process bere alfu z lidského originálu, zachová lidský stín, bílé pozadí spojené s okolím zprůhlední.
- Neúspěšné boardy (`_post_report.json`, náhledy v `$W/review/`) přegeneruj `restyle --only <e> --animation <a> --force`.
- Nespouštěj dva procesy nad stejným `$W` bez nutnosti; stav se sice slučuje, ale každý proces zapisuje celý soubor.

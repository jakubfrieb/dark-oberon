# Orc Race (orc-red / orc-blue / orc-yellow) — Design

Date: 2026-09-23
Status: approved by the user (2026-09-23)

## Goal

Add a new playable **orc** race (green characters) to Dark Oberon that is a
**1:1 counterpart of the humans**: every human unit and building has an orc equivalent with
the same animations, frame counts, directions and statistics. Only the graphics and
displayed names change.

### What the user said
- An orc-like race, green characters, buildings and units as a complement to humans, including animations.
- Scope: 1:1 counterpart (not a new roster, no balance change).
- Graphics: restyle of the existing human boards via codex.
- Player colours: 3 variants red/blue/yellow.
- **The graphic style must be preserved: everything should look as if modelled from plasticine.**

### Assumptions (mine)
- Sounds are taken over from humans unchanged.
- Internal unit/building `id`s in the `.rac` stay the same as for humans (`footman`, `castle`, …) — maps, AI and `can_build` references work without changes. Only `name` changes.

## Definition of done

1. `races/orc-red/`, `races/orc-blue/`, `races/orc-yellow/` exist (each `<id>.rac` + `<id>.dat`).
2. The game loads all three races without errors in the log.
3. All 16 entities (5 units + 11 buildings) have complete orc graphics in all animations the humans have.
4. On a map, a player can be set to an orc race and a game can be played against humans/AI (building, mining, combat, death/zombie).
5. Visually: the characters are green orcs, the team colour is recognisable, scale and anchoring (position on the tile, shadow) match the humans.

## Visual style — plasticine (mandatory)

The orcs must look like they come from the same set of figurines as the humans: **hand-modelled from
plasticine** and photographed/rendered from above at an angle.

- **Material:** matte plasticine, softly rounded shapes, bold and simplified
  proportions, no sharp edges, no fine details (hair, fabric textures, metal
  with highlights). Details are "stuck-on balls and cylinders" (eyes, pauldrons, spikes).
- **Light:** soft light from the upper left, gentle shading, faint sheen; a cast shadow
  on the ground as with the humans (direction and length taken from the original).
- **Colours:** solid, saturated uniform areas like single-colour plasticine: green skin
  (2–3 shades), brown leather/wood, grey bones/stone, team colour pure red.
  No gradients, noise or photorealistic textures.
- **Scale and silhouette:** the same figurine size in the cell as the humans; an orc may be
  bulkier (wider shoulders, lowered head), but must not exceed the original's mask.
- **Forbidden:** pixel art, cel-shading/outlines, realistic render,
  cartoon 2D look, detailed textures, any background other than white.

These points are part of every codex prompt (design sheet and restyle) and of the
first inspection step in the pilot. The human boards also serve as a **style
reference**: the prompt always includes the original human board as well.

## Entity mapping

| id | Humans | Orcs (`name`) |
|----|------|---------------|
| peasant | Peasant | Peon |
| footman | Footman | Grunt |
| moleman | Moleman | Sapper (goblin digger) |
| catapult | Catapult | Bone Catapult |
| airship | Airship | Goblin Zeppelin |
| castle | Castle | Great Hall |
| townhall | Town Hall | Chieftain Hut |
| citadel | Citadel | Stronghold |
| fort | Fort | War Fort |
| barracks | Barracks | War Camp |
| farm | Farm | Pig Farm |
| shed | Shed | Lumber Hut |
| manufactory | Manufactory | Goblin Workshop |
| tower | Tower | Watchtower |
| cannontower | Cannon Tower | Skull Tower |
| wall | Wall | Palisade |

(The exact list of entities and animations is taken from the `compose` output over `human-red.rac`; the table above is only a naming proposal.)

## Architecture / pipeline

We build on the existing pipeline in `.cursor/skills/dark-oberon-dat/scripts/`
(`race_pipeline.sh compose → boards → unboards → finalize`). Only the codex
generation step and the colour variant step are new.

```
human-red.dat ──compose──► sheets ──boards──► boards/*.png (1024², humans)
                                                   │
             design sheet (codex, user approves)   ┤
                                                   ▼
                                   codex restyle (1 call / board)
                                                   ▼
                                    boards_orc/*.png ──post-process (alpha from original)
                                                   ▼
                                   unboards ──► finalize ──► races/orc-red/
                                                   ▼
                                   recolor team masks ──► orc-blue, orc-yellow
```

### 1. Clean working directory
The existing `ai-working/race-pipeline-human-red/` has absolute paths in `_pipeline_state.json`
from a different repo location → run `compose` again into `ai-working/race-pipeline-orc/`.

### 2. Design sheet per entity
Codex generates one reference image per entity (front/back view,
detail of team-coloured parts) in the plasticine style (see Visual style), with references `orcs-racs/*.png`
and the entity's human board. The user approves the design sheets (units individually,
buildings in groups). Stored in `ai-working/race-pipeline-orc/design/<entity>.png`.

### 3. Board restyle
New script `run_codex_board_batch.py` (next to `run_openai_board_batch.py`):
- for each board it builds a prompt: input human board + design sheet + rules
  ("keep the silhouette, pose, position in the cell, shadow and white background; team parts
  pure red; add nothing outside the cells"),
- runs `codex exec` (pattern `~/.claude/skills/iso-sprite/tools/codex_run.py`:
  checks that the output PNG was created, retry),
- in parallel (`--parallel N`), idempotently (skips finished boards), state in JSON,
- `--dry-run` and `--only <entity>`.

### 4. Post-processing (deterministic)
- scaling/alignment to the board size,
- alpha mask from the original human frame (slightly expanded by dilation) so that
  anchoring, footprint and selection area are preserved,
- validation: dimensions, mask coverage, non-empty content in every cell; failed boards
  go into a report for regeneration.

### 5. Finalize
`race_pipeline.sh finalize --race-id orc-red --race-name "Plastic Orcs - Red"`;
`generate_rac.py` takes the human `.rac` and rewrites unit/building `name`s according to the
mapping (new option / mapping JSON).

### 6. Colour variants
In the human races the team colour differs directly in the `.dat` (three different files).
New script `recolor_race.py`: finds team-colour pixels (red hue, saturation
above a threshold) and remaps the hue to blue/yellow; packs the result as
`orc-blue.dat` / `orc-yellow.dat` + `.rac` with an adjusted `name`. The exact method is
calibrated in the pilot by comparing `human-red` vs `human-blue`.

## Pilot (mandatory before the bulk run)

Only **Grunt (footman)**: design sheet + boards `stay`, `move`, `attack`, `picture`,
`zombie` → finalize as `orc-red` (other entities temporarily with human graphics) →
test map `orc-red` vs `human-blue` → check of scale, animation, anchoring,
team colour. Continue only after user approval.

## Errors and risks

| Risk | Mitigation |
|--------|----------|
| Inconsistency between frames/boards | design sheet as a shared reference; regenerating individual boards |
| Codex shifts the character in the cell | alpha from the original + coverage validation |
| Team colour "bleeding" into green skin | prompt requires pure red; recolor only above the saturation threshold and within the red hue range |
| Codex fails / does not create the file | retry, state in JSON, resume from the last finished one |
| Buildings look "human" | dedicated design sheets for buildings (wood, leather, bones, spikes) |
| Loss of the plasticine look (realistic/cartoon output) | style rules in every prompt, human board as reference, visual check in the pilot and on design sheets |

## Testing

- pytest for the deterministic parts: post-process (alpha/dimensions), recolor (team colour hue changes, green does not), name mapping in `generate_rac.py`.
- Round-trip: `unpack` → `pack` of the new `.dat` → `unpack` yields the same files.
- Manual: the game loads the race, pilot map, log check.

## Out of scope

New sounds, balance changes, new units/mechanics, new animations beyond the human ones.

# Generating races with codex — playbook (lessons from the orcs, 2026-09)

A summary of what worked and what didn't when creating the orc race (`races/orc-{red,blue,yellow}`), and how
to proceed next time — including a completely new race that will not be a 1:1 copy of the humans.

Tools: `.cursor/skills/dark-oberon-dat/scripts/` (`race_pipeline.sh`, `run_codex_board_batch.py`,
`board_postprocess.py`, `attack_anim.py`, `recolor_race.py`, `validate_race.py`), tests
`tests/race_pipeline/` (`python3 -m pytest tests/race_pipeline`).

---

## 1. The process that proved itself

1. **Lock the style up front.** Everything must look modelled from plasticine (matte material,
   rounded shapes, light from the top left, no outlines or pixel art). The rules live in
   `codex_prompts.py:STYLE_RULES` and go into every prompt.
2. **Pilot: one unit + one building**, the rest only after approval. The pilot uncovered most
   problems (portrait, building shape, construction stages, white fringes, TGA footer).
3. **Design sheet per entity → user approval → only then restyle** (`design`, `approve`,
   `restyle`). Approve in groups (units / main buildings / production / defence).
4. **Restyle existing boards 1:1** (human graphics as the template): codex gets the human board
   + design sheet and repaints every sprite in the same place, in the same pose and size.
5. **Deterministic post-process:** alpha from the human original (the sprite fits the cell exactly,
   anchor and shadow are kept), validation of `coverage` / `spill` / `unchanged`, preview in `review/`.
6. **Failed boards:** regenerate with `--force` (max 3×); when the result looks fine visually and only
   misses the threshold (typically small glowing objects), accept it deliberately with `board_postprocess.py --accept`
   (remembered in `_accepted.json`).
7. **Team colour:** generate only the red variant (team parts "pure red"),
   produce blue/yellow deterministically with `race_pipeline.sh variants` (calibrated mapping
   `team_blue.json`, `team_yellow.json`). Green skin / brown wood stay as they are.
8. **Check at in-game size** (64 px sprites on grass `(96,128,64)` next to the humans) — at full
   resolution everything looks fine; problems (fringes, holes, scale) only show up this way.
9. **Validation and engine:** `validate_race.py races/<id> --reference races/human-red`, then
   a headless server (`make server` in a temporary copy of `src/`, `addcpu`×2, `start`) — it must reach
   `Update: Running` without `Err:`.
10. **Test map for a new race** (one-off, do not commit): a copy of `maps/trial.map` with `name "<race>"`
    instead of `"human-red"`; to check combat animations, place both armies ~8 tiles apart
    (`start_point_0/1` close together, only soldiers in `<Units>`) — combat starts right after the start.

## 2. What didn't work and how it is handled

| Problem | Cause | Solution (already in the tools) |
|---|---|---|
| The game failed to load the race: `Error reading TGA data` | Pillow writes TGA 2.0 with a `TRUEVISION-XFILE` footer; the engine reads `.dat` sequentially and ignores `dsize` | `do_dat_tool.py pack` trims data past the pixels; `validate_race.py` checks for it |
| Portrait = tiny head in the corner | codex got the whole 1024² board, the slot took 400×320 | send codex only the slot crop (`slots_bbox`) and embed it back (`embed_generated`) |
| Building has a different shape than the human one | codex copied the shape from the design sheet | "keep the human building's shape/footprint" in both design and restyle prompts (`LAYOUT_RULE`) |
| Construction stages drawn as finished buildings | codex doesn't understand what the animation means | per-animation hints (`ANIMATION_HINTS`: `picture`, `build`, `zombie`, `projectile`) |
| White fringes around a narrower figure | codex painted background inside the old silhouette | make white transparent, but **only where connected to the surroundings** (flood-fill); otherwise holes appear (barracks courtyard) |
| Projectile (32 px) failed validation | the human stone has a blurred glow | `--accept` after a visual check |
| Parallel runs overwrote each other's state | each process wrote the whole JSON | state is merged before writing; still, don't run more processes on the same `W` than needed |
| Attack looked like "zooming in" | original data have 1 static, larger attack frame | new animation `attack_anim.py` (4 frames, `atime` = `offensive_feed_time`) |
| Grunt faced the wrong way while attacking | the reference was only the design sheet (front view) | reference = board with **all 8 directions** + `--views "5:back-left,6:back,7:back-right"` |
| Codex "failed" on every call | the ChatGPT plan limit ran out | `run_codex` now raises `CodexUsageLimit`; wait for the reset (the message includes the time) |
| Codex could not find input images | relative paths, codex runs with `-C work` | all paths passed to codex are absolute |
| Colour calibration paired things wrongly | human variants have different group indices (`g020_…`) | pair by `group__id` |
| Light tower (Watchtower) looks human | the design inherited light plaster | watch materials on the design sheet (dark wood/stone) — the user approved it knowingly |

| Construction stage looked human (grey stone) | codex took the material from the human board | `restyle --hint "…same orc materials as the finished building…"` |
| Leftover human items (blue ore, white stones, dotted outlines) | alpha from the human original keeps islands that codex fills with light colours | post-process: grey-only shadow (not coloured), removal of light islands, orphaned semi-transparent outlines and the light fringe on the outline |
| Blue stone on the orc catapult | human item, the hint didn't help | deterministically via `W/_retint.json` (`{"catapult": {"from_hue": [190,260], "to_hue": 30, "sat": 0.15}}`) |

**Reviewing a finished race:** metrics against the human race (white pixels, fringe, lost area, magenta,
team colour) + contact sheets of all textures on grass; compare suspicious pieces 1:1 with the human
original. After any post-process change always check for regressions (loss of opaque area > 2 %).
`finalize` validates before `attack_anim apply` → `footman_attack` size errors are
expected at that point; after `apply` validation must pass.

Operational details: the headless server does not respond to `quit` after the game starts (terminate it with
`timeout -k`); the server writes logs to the repository's `logs/` (don't confuse with the player's logs); the scratchpad
is wiped after a session restart (rebuild the server binaries); `finalize` rewrites the `.dat` from the sheets —
**then re-apply the attack animation** (`attack_anim.py apply`, frames stay in `ai-working/attack-*`)
and run `variants` again.

## 3. A completely new race (not a 1:1 copy of the humans) — recommended process

Difference from the orcs: for new units/buildings **there is no template** to take
alpha, anchor and frame counts from. Therefore:

1. **Content design:** list of units and buildings, roles, stats → a new `.rac` (id, `tg_*`
   groups, `can_build`, `products`, materials). Where possible, keep **cell sizes and frame
   counts** from the closest human entity (unit 64×64, 8 directions; buildings by `width/height`),
   so the same anchors (`pointx/pointy`) and shadows can be used.
2. **Shape template:** for each new entity pick a human entity of similar size as its
   "silhouette and scale" (anchor, size in the cell). Where the new entity should stay close in
   shape, the 1:1 restyle can still be used (most accurate).
3. **Generate new frames without a template** the same way as `attack_anim.py`: magenta background,
   2×2 grid with a wide margin, keying, uniform scale per direction (from a single reference
   frame), feet anchored to the ground point, shadow taken from the template (or synthesized).
   Reference for codex = board with all 8 directions + a view hint for the back directions.
4. **Order:** design sheets → approval → pilot of 1 unit + 1 building (including an in-game
   test) → the rest → colour variants → validation → headless test → temporary arena (item 10).
5. **The validator** currently compares 1:1 against the reference race (same groups and sizes). A new
   race will need a "consistency only" mode (every `tg_*` group exists, 8 textures for
   directional groups, `hcount*vcount` matches the size, no data past the pixels).
6. **AI** for a new race works without changes as long as the `.rac` respects the types (`w`/`f`/`b`/`a`), `can_build`
   and factories with `products` (the AI reads `build_list`, it has no hardcoded names).

## 4. Quick command reference

```bash
S=.cursor/skills/dark-oberon-dat/scripts; W=$PWD/ai-working/race-pipeline-<id>
bash $S/race_pipeline.sh compose --source $PWD/races/human-red --work-dir $W
bash $S/race_pipeline.sh boards  --work-dir $W
python3 $S/run_codex_board_batch.py design  $W --entities <e1,e2> --refs <ref-dir> --parallel 4
python3 $S/run_codex_board_batch.py approve $W <e1> <e2>
python3 $S/run_codex_board_batch.py restyle $W --only <e1,e2> --parallel 5
bash $S/race_pipeline.sh codex-post --work-dir $W            # validation + alpha + unboards
python3 $S/board_postprocess.py $W --only <e> --accept <board_id>   # deliberate acceptance
bash $S/race_pipeline.sh finalize --work-dir $W --output races/<id>-red --race-id <id>-red \
  --race-name "<Name> - Red" --names-json <names.json>
# attack animation (new frames)
python3 $S/attack_anim.py prepare  --unpacked <unpacked> --stay-group <unit>_stay --work ai-working/attack-<unit>
python3 $S/attack_anim.py generate --work ai-working/attack-<unit> --reference <board-8-dirs.png> \
  --subject "<description>" --views "5:back-left,6:back,7:back-right"
python3 $S/attack_anim.py build    --unpacked <unpacked> --stay-group <unit>_stay --work ai-working/attack-<unit>
python3 $S/attack_anim.py apply    --unpacked <unpacked> --group <unit>_attack --work ai-working/attack-<unit>
bash $S/race_pipeline.sh variants --source races/<id>-red --race-prefix <id> --name-prefix "<Name>"
python3 $S/validate_race.py races/<id>-red --reference races/human-red
```

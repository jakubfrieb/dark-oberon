# Race Format Specification (Dark Oberon 1.0.x) — for human authors and AI generators

This document describes what must be consistent between `*.rac`, `*.dat` and the map scheme. The primary source of truth is the code in `src/doraces.cpp`, `src/dodata.cpp`, `src/doforces.cpp`, `src/doworkers.cpp`, `src/dolayout.h`, `src/glgui.*`.

---

## 1. Files and paths

| Item | Rule |
|--------|-----------|
| Race ID | A string without spaces, e.g. `human-red`, `orc`. Folder = `races/<id>/`. |
| Files | `races/<id>/<id>.rac` and `races/<id>/<id>.dat` (same `<id>`). |
| Scheme | In the `.rac` header: `schemes "plastic"` must exactly match `scheme "plastic"` in the `.map` (see `doengine.cpp`). |
| Materials | The number of values for `materials` / `max_amount` / … = `scheme.materials_count` (for `plastic` there are 3: gold, wood, coal). |

---

## 2. `*.rac` header (global)

Required / typical entries at the start of the file (see `LoadRace`):

```text
name "Displayed race name"
author "Author"
schemes "plastic"

tg_food_id "food"           # texture group in .dat (not a file on disk)
tg_energy_id "energy"
tg_burning_id "building_burning"

# with SOUND=1 — sound IDs in .dat; "none" = no sound
snd_error error
snd_placement placement
snd_construction construction
snd_burning burning
snd_dead none
snd_explosion explosion1 explosion2 ...
snd_building_selected building_selected
```

- `tg_*` values are **group names** inside `<id>.dat`, not paths to TGA files.
- `snd_*` may list several IDs in a row — on playback one sample from the group is chosen at random.

---

## 3. Entity types (`item_type`)

| Char | Type | Structure in code |
|------|-----|------------------|
| `f` | Combat unit | `TFORCE_ITEM` |
| `w` | Worker | `TWORKER_ITEM` |
| `b` | Building (not a factory) | `TBUILDING_ITEM` |
| `a` | Factory (produces units) | `TFACTORY_ITEM` + `<Products>` section |

---

## 4. Units — `<Units>` / `<Unit N>` section

### 4.1 Load order

1. **All** units are loaded first (the "first" pass).
2. Then **buildings**, **sources** (scheme).
3. Then dependencies: `can_build`, `can_repair`, `allowed_materials`, … (second pass `LoadRacUnit(..., false)`).

The AI must generate **stable `id`s** (e.g. `footman`) that buildings and workers refer to.

### 4.2 `view` (important: it is **not** the number of sprite directions)

- `view` is the **sight range in map cells** (used for fog of war / visibility), see `LoadRacUnit` → `ReadSimpleRange(..., "view", 1, MAP_MAX_SIZE, 1)`.
- **Character directions** are not a "views" number in `.rac`; they come from the **number of textures in the group** in `.dat` (see §6).

### 4.3 Segments and terrain

- The game has **3 segments** (`DAT_SEGMENTS_COUNT`): typically underground / ground / air (indices 0, 1, 2).
- For each segment the following value sets are read:
  - `move_terrain_id` — twice per segment (min/max terrain type for movement),
  - `land_terrain_id` — the same for "landing",
  - `max_speed` — max. speed in the segment,
  - `max_rotation_speed` — **in degrees/s** (the engine converts to radians).

Others: `min_exist_segment_id`, `max_exist_segment_id`, `min_max_visible_segment_id` (6× — min/max visibility for each segment), `land_segment_id`, `energy`, `food`, `selection_height`, `burning_position x y`, `max_hided_units`, `features`, `heal_time`, combat parameters if `is_offensive true`, etc. — copy the exact list of fields from `human-red.rac` and from `LoadRacUnit`.

### 4.4 Unit textures (`tg_*`)

Group names in `.dat`; optional groups are often set to `none` (the engine calls `ReadTextureGroup` with `required=false`).

| Key | Purpose (simplified) |
|------|---------------------|
| `tg_picture_id` | Icon / portrait (required group in the load context) |
| `tg_stay_id` | Idle animation |
| `tg_move_id` | Movement (falls back to `tg_stay_id` if missing) |
| `tg_attack_id` | Attack |
| `tg_rotate_id` | Rotation |
| `tg_land_id` / `tg_anchor_id` | Landing / anchoring (air) |
| `tg_dying_id` / `tg_zombie_id` / `tg_burning_id` | Death / zombie / burning |
| `tg_projectile_id` | Only for an offensive unit (`is_offensive true`) — projectile |

Workers additionally: `tg_mine_id`, `tg_repair_id`.

### 4.5 Unit sounds (`snd_*`)

Typically: `snd_ready`, `snd_selected` (multiple variants), `snd_command`, `snd_dead`, `snd_burning`; for attackers `snd_fireon`, `snd_fireoff`, `snd_hit`; for workers `snd_workcomplete`, `snd_mine_material0` … according to the number of materials in the scheme.

---

## 5. Buildings — `<Buildings>` / `<Building N>`

- `width`, `height` (a rectangle, not just a square as for units).
- `view` — again the **range**, not directions.
- `item_type` `a` or `b`; a factory has `<Products>` with `product "<unit_text_id>"` and `product_time`.
- Textures: `tg_picture_id`, `tg_stay_id`, `tg_build_id`, `tg_dying_id`, `tg_zombie_id`, `tg_burning_id`, optionally `tg_projectile_id`.
- Dependencies in the second pass: `allowed_materials`, `ancestor`, `can_hide`, …

---

## 6. Character directions and textures (critical for AI art)

### 6.1 Eight directions in the plane

Defined in `dolayout.h` (index `look_direction` 0..7):

| Index | Constant | Direction (naming in code) |
|------|-----------|---------------------------|
| 0 | `LAY_SOUTH` | South |
| 1 | `LAY_SOUTH_WEST` | Southwest |
| 2 | `LAY_WEST` | West |
| 3 | `LAY_NORTH_WEST` | Northwest |
| 4 | `LAY_NORTH` | North |
| 5 | `LAY_NORTH_EAST` | Northeast |
| 6 | `LAY_EAST` | East |
| 7 | `LAY_SOUTH_EAST` | Southeast |

Directions 8 (`LAY_UP`) and 9 (`LAY_DOWN`) are between segments — **the sprite direction is taken only from 0..7**.

### 6.2 The "8 textures in a group" rule

In `TFORCE_UNIT::ChangeAnimation` and `TWORKER_UNIT::ChangeAnimation`:

```cpp
if (player->race->tex_table.groups[tg].count >= 8) tex = look_direction;
else tex = 0;
```

**What this means for art production:**

- If a group (e.g. `footman_stay`) has **≥ 8 textures**, then the texture at index **0** = direction `LAY_SOUTH`, index **1** = `LAY_SOUTH_WEST`, …, **7** = `LAY_SOUTH_EAST`.
- If a group has **fewer than 8** textures, the engine **always** uses only index **0** → a single "universal" sprite that does not turn with direction.

Common pattern for humans: **8 separate TGAs** in the group, each with `hcount=1`, `vcount=1` (or an animation within each direction).

### 6.3 Frame grid within one TGA (`hcount`, `vcount`, `atime`)

In `TTEX_TABLE::Load` (`dodata.cpp`):

- `frames_count = hcount * vcount`
- `frame_width = original_width / hcount`, `frame_height = original_height / vcount`
- `frame_time = atime_ms / (1000 * frames_count)` — the total `atime` in milliseconds is **divided** among all frames.

Frame rendering (`TGUI_TEXTURE::DrawFrame` in `glgui.cpp`):

- `frame = 0..frames_count-1`, position in the grid: row from the top, column from left to right:  
  `frame_x = frame % h_count`, `frame_y` from the bottom of the texture.

**Recommendation for AI / artists:** keep texture dimensions at **powers of 2** after loading (the engine can upscale a TGA to the next 2^N — see `TGA_RESCALE`).

### 6.4 Texture type (`ttype` in .dat)

| Value | Meaning (`TGUI_TEX_TYPE`) |
|---------|---------------------------|
| 0 | `GUI_TT_NORMAL` — rectangular sprite |
| 1 | `GUI_TT_RHOMBUS` — rhombus (isometric "diamond") |

---

## 7. Binary `*.dat` and texture groups

- Format: see `.cursor/skills/dark-oberon-dat/scripts/do_dat_tool.py` and the comment in the script header.
- Each **group** has a string `name` (identical to what is in quotes for `tg_*` in `.rac`).
- Inside a group: entries with their own `id` (technical string), TGA file, `hcount`, `vcount`, `atime`, `pointx`, `pointy`, `ttype`.
- `pointx` / `pointy` in the file are the **anchor** (the engine stores them with a sign as `-point` when drawing).

Workflow for AI:

1. Generate a **canonical list of groups and IDs** (from the planned `.rac`).
2. For each required group create the correct number of TGAs (e.g. 8× for a directional animation).
3. Assemble `manifest.json` + `pack` → `<id>.dat`.

---

## 8. Minimal "new unit" example (concept)

In `.rac` (inside `<Units>` increase `count` and add):

```text
  <Unit N>
    id "grunt"
    name "Grunt"
    item_type f
    size 1
    materials 500 1 0
    max_life 80
    max_speed 4 4 4
    max_rotation_speed 720 720 720
    selection_height 40
    burning_position 0 0
    view 6
    energy 0
    food -15
    move_terrain_id 0 0 8 25 0 0
    land_terrain_id 0 0 0 0 0 0
    min_exist_segment_id 1
    max_exist_segment_id 1
    min_max_visible_segment_id 1 2 1 2 1 2
    land_segment_id 1
    max_hided_units 0
    can_hide

    tg_picture_id grunt_picture
    tg_stay_id grunt_stay
    tg_anchor_id none
    tg_move_id grunt_move
    tg_land_id none
    tg_rotate_id none
    tg_attack_id grunt_attack
    tg_dying_id none
    tg_zombie_id grunt_zombie
    tg_projectile_id none
    tg_burning_id none

    # ... snd_* and combat parameters as for the footman ...
  </Unit N>
```

The `.dat` must contain groups such as `grunt_picture`, `grunt_stay`, `grunt_move`, `grunt_attack`, `grunt_zombie` with matching TGAs. For directions of `grunt_stay`: either **8 textures** in the `grunt_stay` group, or 1 texture (then no sprite rotation).

---

## 9. Checklist before launching the game

- [ ] `schemes` in `.rac` = map scheme.
- [ ] Number of values for `materials` / worker fields = number of materials in the scheme.
- [ ] Every `tg_*` (except `none`) has an existing **group** in `.dat`.
- [ ] Every `snd_*` (except `none`) has an existing sound in `.dat` (build with sound).
- [ ] `can_build` / `can_repair` / `product` refer to existing building/unit `id`s.
- [ ] Directional animations: either **≥ 8** textures in the group, or accept a non-rotating sprite.

---

## 10. Source references in the repository

- Race loading: `src/doraces.cpp` — `LoadRace`, `LoadRacUnit`, `LoadRacBuilding`, `LoadRacSource`.
- Texture selection by direction: `src/doforces.cpp` / `src/doworkers.cpp` — `ChangeAnimation`.
- Directions: `src/dolayout.h` — `LAY_SOUTH` … `LAY_SOUTH_EAST`.
- Frame grid: `src/glgui.cpp` — `TGUI_TEXTURE::DrawFrame`.
- Pack/unpack data: `.cursor/skills/dark-oberon-dat/scripts/do_dat_tool.py` (skill `dark-oberon-dat`).

---

*Document version: derived from the Dark Oberon 1.0.2-RC1 tree. When the engine changes, update this file according to `src/`.*

---
name: dark-oberon-map
description: >-
  Primary: generate new Dark Oberon RTS maps (*.map) from requirements (size, players,
  scheme, layout). Then validate and preview: segments, fragments, layers, water/coast,
  passability, connectivity, resource balance. Scripts in this skill's scripts/ folder.
  Use for new maps, map edits, schemes, coast/cliffs, validate_map.py, map_preview.py.
---

# Dark Oberon maps — generation first

**Primary task:** produce **new** `.map` files (layout, fragments, layers, players, objects) that match game rules and the chosen scheme.

**Always after generation or substantive edits:** run `validate_map.py` and optionally `map_preview.py`; fix issues until validation passes.

Scripts: `.cursor/skills/dark-oberon-map/scripts/` (run from **repository root**).

```bash
pip install -r .cursor/skills/dark-oberon-map/scripts/requirements.txt   # Pillow for preview only
python3 .cursor/skills/dark-oberon-map/scripts/validate_map.py maps/your.map
python3 .cursor/skills/dark-oberon-map/scripts/map_preview.py maps/your.map -o /tmp/p.png --scale 2
```

## Terminology

- **Segment** (`<Segment 0|1|2>`): underground / earth / air. Unit **z** in the map = segment index.
- **Fragment**: 5×5 (typ.) block from the scheme; `terrain_id` grid holds **layer numbers** per mapel.
- **Layers**: optional patches on top of fragments; overlaps allowed; **last wins**.

## Passability (plastic / typical land units)

`move_terrain_id` in `.rac` defines a **range of terrain type indices** per segment. For common units on segment 1, walkable types correspond roughly to **layer 10–20** (grass, swamp). Sea, beach, rocks (layer 30), unmoveable (255) block standard ground units. **Rocks are cliffs, not walkable plateaus.**

## Edit order

1. Header: `name`, `author`, `width`, `height`, `scheme`.
2. `<Players>`.
3. Per segment: `<Fragments>` (no overlap), `<Layers>` (`count` must match `layer_*` lines), `<Objects>` (no overlap).

Mirror coast/water between segment 0 and 1 when both are used.

## RTS checks (manual / review)

- All **start points** on walkable terrain; map **connected** for land units between starts.
- **Goldmines / forests** reachable and fairly distributed.
- Coast fragments form consistent closed contours; rock masses do not split the map into unreachable regions.

## Code references

- [`src/domap.cpp`](../../../src/domap.cpp) — load fragments, layers, objects.
- [`docs/User_documentation_SK.md`](../../../docs/User_documentation_SK.md) — map file sections.

## Shared library

- `dark_oberon_maplib.py` — parse `.map` / `.sch`, `build_terrain_grid`, `find_repo_root()` (walks up to directory containing `schemes/`).

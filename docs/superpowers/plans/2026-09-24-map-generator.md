# 4-Player Map Generator — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `generate_map.py --seed N` creates a playable 160×160 map for 4 players with properly bordered water and plateaus, resources and 6 races.

**Architecture:** A 32×32 cell grid (fragment = 5×5 fields). The layout works with region masks (water W, plateau P) on cells; autotiling turns region edges into fragments using the tables from the spec. Resources and starts are placed in fields. Checks: fragment adjacency against handmade maps, walkability, balance, `validate_map.py`.

**Tech Stack:** Python 3 (numpy, Pillow, pytest), existing `dark_oberon_maplib.py`, `do_dat_tool.py` (preview from textures), headless server.

**Spec:** `docs/superpowers/specs/2026-09-24-map-generator-design.md`

## Global Constraints

- Map 160×160, 32×32 cells, fragment 5×5; fragment coordinates = fields (multiples of 5); E = x+5, S = y+5.
- Edge tile tables exactly as in the spec (water: `coast_*`, plateau: `rocks_*`, ramps `rocks_*_end_*`).
- Rock never borders water (≥ 1 cell of grass between them, diagonally too).
- Walkable terrain for ground units: layers 10–25 (grass 10, swamp 20, rocky grass 25).
- Resources: `goldmine` 4×4, `coal` 4×4, `forest` 3×3; only on walkable terrain, no overlap, away from town halls.
- Races: `human-red`, `human-blue`, `human-yellow`, `orc-red`, `orc-blue`, `orc-yellow`; `max_count 4`; 4 starts.
- Output is deterministic for a given seed (`random.Random(seed)`, `numpy.random.default_rng(seed)`).
- Scripts in `.cursor/skills/dark-oberon-map/scripts/`, tests in `tests/mapgen/`.
- Commits end with `Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>`.

## Review Focus

1. **A region with a cell whose surroundings lie on opposite sides** (a narrow 1-cell neck) → must not exist after smoothing. Test: `test_smooth_removes_thin_necks` (Task 1).
2. **Diagonal saddle** (two regions touching at a corner) → removed. Test: `test_smooth_removes_saddles` (Task 1).
3. **Plateau next to water** → enforced gap. Test: `test_plateau_keeps_distance_from_water` (Task 3).
4. **Start enclosed by plateau / water** → connectivity enforced. Test: `test_all_starts_connected` (Task 3).
5. **The handmade map `sunnybay` must pass the adjacency check** (otherwise the check is wrong). Test: `test_adjacency_check_accepts_handmade_map` (Task 2).

---

### Task 1: `mapgen_tiles.py` — autotiling and smoothing
**Produces:** `WATER_TABLE`, `PLATEAU_TABLE` (dict: frozenset of surrounding sides → name), `outline_fragment(mask, cx, cy, table) -> str|None` (None = interior), `smooth_region(mask) -> mask` (3×3 opening, removal of necks and saddles, iterate until stable), `place_ramps(mask, rng, n) -> dict[(cx,cy)] -> name`, `fragment_ids(sch_path, segment) -> dict[name] -> index`, `ug_name(name) -> str` (`coast_*`→`ug_coast_*`, `sea`→`ug_sea`, otherwise `ug_grass`).
Tests (`tests/mapgen/test_tiles.py`): every row of both tables on a synthetic mask (6×6-cell square + a cut-out for inner corners), `test_smooth_removes_thin_necks`, `test_smooth_removes_saddles`, ramps yield the correct pair and only on a straight edge of length ≥ 4, `fragment_ids` contains all 80 tiles of segment 1 and `ug_*` in segment 0.

### Task 2: `map_check.py` — checks
**Produces:** `learn_adjacency(map_paths, sch) -> set[(a, dir, b)]`, `check_adjacency(frag_grid, allowed) -> list[str]`, `walkable_grid(frag_grid, sch) -> bool[160,160]` (+ resources/buildings as obstacles), `check_connectivity(walk, starts) -> list[str]`, `check_resources(sources, starts) -> list[str]`, CLI `map_check.py maps/x.map` (exit 1 on errors).
Adjacency rule: a pair is OK if it is in the set learned from `sunnybay`, `virgin_editor`, `trial`, or both tiles are from {`grass`, `sea`} and identical.
Tests: `test_adjacency_check_accepts_handmade_map` (sunnybay, learned from the other two + itself), `test_adjacency_check_flags_bad_pair` (`coast_n` next to `rocks_e`), connectivity on a synthetic grid, resource balance.

### Task 3: `mapgen_layout.py` — layout
**Produces:** `Layout(water, plateau, ramps, starts)`, `make_layout(seed, cells=32) -> Layout`.
Steps: starts (quadrants, jitter ±3 cells, min. 18 cells apart, 2 cells from the edge), 5×5-cell free zones around starts; a central lake (noise blob 6–9 cells) + 2–3 smaller ones (3–5 cells) outside the free zones; smooth; 3–5 plateaus (rectangles of 4–7 cells, possibly a union of two), smooth, distance from water ≥ 1 (8-neighbourhood); 1–2 ramps per plateau; start connectivity (BFS over cells outside W and the P outline + ramps); if not connected, remove a plateau or shrink a lake and repeat.
Tests: determinism for a seed, `test_plateau_keeps_distance_from_water`, `test_all_starts_connected`, starts outside W/P, minimum distances.

### Task 4: `mapgen_resources.py` — resources
**Produces:** `place_resources(layout, walk, rng) -> list[(id, x, y, life, amount)]`.
At each start: 1× goldmine 10–15 fields from the town hall (35,000), 1× coal 12–18 fields (8,000), 2–3 forest clusters (8–14 forests of 3 fields each) 8–20 fields away; centre/plateaus: 2–3 contested goldmines (40,000); scattered forests up to ~350 in total. Balance: the amount of gold/coal/forest within 25 fields of each start ±10 %.
Tests: no overlap, walkable only, balance, nothing within 6 fields of a start.

### Task 5: `generate_map.py` + `render_map.py` + integration
Writing the `.map` (header, Players with 6 races and a set of town hall + 4 workers + 2 soldiers, SchemeRace with resources, 3 segments: seg0 mirror of `ug_*`, seg1 content, seg2 fragment 0; Layers/Objects count 0), `--preview`. Integration: seeds 1–3 → `map_check` + `validate_map.py` OK, render, headless server `addcpu`×4 on the best map for 180 s with no `Err:` and the AI mining with workers; commit the best map as `maps/four_lakes.map`.

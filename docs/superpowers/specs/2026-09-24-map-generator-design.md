# 4-Player Map Generator — Design

Date: 2026-09-24
Status: approved by the user ("write it up and get going")

## Goal

A `generate_map.py` script that generates a playable `.map` for the `plastic` scheme from `--seed`:
**properly bordered water** (coastline), **properly bordered elevated areas** (rocky plateau
outlines with ramps), sensibly placed forests, gold and coal, and 4 starting positions.

### What the user said
- 4 players, 4 spawn points; "functional, meaningfully filled maps".
- Size 160×160; landscape "lakes + ridges"; **balanced, not symmetric**; races humans + orcs.

### Assumptions
- We only generate segment 1 (ground) with content; segment 0 (underground) mirrors the water with `ug_*` tiles,
  segment 2 (air) is entirely fragment 0.
- Map edge tiles (`grass_border_*`) are not required — the original maps also have plain grass on the edges.
- Rocks never border water (always ≥ 1 cell of grass between them) → we do not need the `cliff_*`
  and `rocks_*_coast_*` tiles.

## Fragment model (verified on `sunnybay`, `virgin_editor`, `trial`)

The map = a 32×32 grid of cells (fragment = 5×5 fields). Fragment coordinates = (x, y) in fields, multiples of 5;
E = x+5, S = y+5.

**Water** — region W (water cells). An edge cell of W (with land among its 8 neighbours) gets a tile depending on
where the land is:

| land | tile | | land | tile |
|---|---|---|---|---|
| N | `coast_n` | | N+E | `coast_wn` |
| W | `coast_e` | | N+W | `coast_en` |
| S | `coast_s` | | S+W | `coast_es` |
| E | `coast_w` | | S+E | `coast_ws` |
| diag NW only | `coast_ne` | | diag NE only | `coast_nw` |
| diag SW only | `coast_se` | | diag SE only | `coast_sw` |

Interior of W = `sea`, land = `grass`.

**Plateau** — region P. An edge cell of P gets a rock tile depending on the side where the surroundings are:

| surroundings | tile | | surroundings | tile |
|---|---|---|---|---|
| N | `rocks_s` | | N+W | `rocks_sw` |
| S | `rocks_n` | | N+E | `rocks_se` |
| W | `rocks_w` | | S+W | `rocks_nw` |
| E | `rocks_e` | | S+E | `rocks_ne` |
| diag NW only | `rocks_ws` | | diag NE only | `rocks_es` |
| diag SW only | `rocks_wn` | | diag SE only | `rocks_en` |

Interior of P = `grass`. **Ramp** = a pair of adjacent straight edge cells replaced by end tiles:
top edge `rocks_s_end_e` + `rocks_s_end_w` (left to right), bottom `rocks_n_end_e` + `rocks_n_end_w`,
left `rocks_w_end_n` + `rocks_w_end_s` (top to bottom), right `rocks_e_end_n` + `rocks_e_end_s`.

**Shape constraints:** a region must not have a cell with surroundings on two opposite sides, nor a "saddle"
(diagonally touching cells) — regions are smoothed with a 3×3 morphological opening and
diagonal saddles are removed.

## Architecture

`.cursor/skills/dark-oberon-map/scripts/`:
- `mapgen_tiles.py` — the tables above, `outline_fragment(region, x, y) -> name|None`, conversion of names to
  fragment indices from the `.sch`, mirroring into `ug_*`.
- `mapgen_layout.py` — layout: starts (quadrants + jitter, ≥ 90 fields apart), lakes (noise
  blob + central lake), plateaus (rectangle union ≥ 3×3 cells), free zones around starts,
  ramps; ensuring connectivity (BFS over walkable terrain, digging a passage if needed).
- `mapgen_resources.py` — resources: at each start gold (10–15 fields), 2–3 forest clusters, coal;
  2–3 contested gold mines; no overlaps, only on grass; balance ±10 %.
- `generate_map.py` — CLI `--seed --size 160 --players 4 -o maps/<name>.map [--preview out.png]`;
  writes the header, `<Players>` (6 races: human/orc × red/blue/yellow, set: town hall, 4 workers,
  2 soldiers, resources 1500 1000 1000), `<SchemeRace>` with resources, 3 segments.
- `map_check.py` — checks: (1) every fragment adjacency (E/S) is in the set of pairs from handmade maps
  or between the "grass/sea" base; (2) starts on walkable terrain and mutually reachable;
  (3) resources on grass, no overlap, balance; (4) `validate_map.py` passes.
- `render_map.py` — preview from the actual textures in `schemes/plastic.dat` (isometric), resources and starts as markers.

## Errors and edge cases
- Starts/resources cannot be placed → new attempt with a derived seed (max 20), otherwise an error.
- A region collapses to nothing after smoothing → skip it.
- Disconnected map → dig a 3-cell-wide passage (remove the plateau/water in the way).

## Testing
- pytest: outline tables (every case), smoothing (no opposite surroundings, no saddles),
  ramps, `ug_*` mirroring, connectivity, resources without overlap, deterministic output for a seed,
  adjacency check on the handmade map `sunnybay` (must pass = calibration of the check).
- Integration: 3 seeds → `map_check` OK, render for visual inspection, headless server with 4× `addcpu`
  loads the map and runs (AI mines/builds), finally a user test in the game.

## Out of scope
Cliffs (rock next to water), rivers, islands, decorations (`Objects`), layers (`Layers`), symmetric maps.

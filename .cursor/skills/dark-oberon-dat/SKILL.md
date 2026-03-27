---
name: dark-oberon-dat
description: >-
  Packs and unpacks Dark Oberon binary .dat resource archives (textures as embedded
  TGA, optional sounds). Use when editing schemes/races/dat assets, plastic.dat,
  human-*.dat, dat/*.dat, or when the user mentions unpack_dat, pack_dat, or do_dat_tool.
---

# Dark Oberon `.dat` archives

## What a `.dat` file is

A single binary bundle used by the game for **texture groups** (each texture is raw **TGA** bytes) and optionally **sounds**. The on-disk layout matches the C++ loaders in [`src/dodata.cpp`](../../../src/dodata.cpp) (`TTEX_TABLE::Load`, `TSND_TABLE::Load`) and constants in [`src/dodata.h`](../../../src/dodata.h).

**Header (all `.dat`):**

- ASCII exactly `Dark Oberon data file` (21 bytes, no NUL)
- `uint8` version (allowed **2** or **3**)
- `uint32` offset to texture table
- If version **> 2**: second `uint32` offset to sound table (0 = no sounds)

**Texture block:** group count, then per group: Pascal string name, texture count, per texture: Pascal id, frame counts, timing, type, `uint32` payload size, raw TGA data.

**Sound block (v3):** sound count, per sound: Pascal id, format, storage type, size, raw bytes.

## Unpack layout (convention)

- **Single file:** `unpack_dat.sh path/to/foo.dat` creates **`path/to/foo.dat-unpacked/`** next to the archive (unless you pass a second argument as the output path).
- **Directory batch:** `unpack_dat_dir.sh somedir` unpacks each `somedir/*.dat` to **`somedir/<same-filename>.dat-unpacked`**.

## Where `.dat` files live in the repo

- `dat/` — `fonts.dat`, `gui.dat`, `cursors.dat`, …
- `schemes/<name>.dat` — terrain / scheme graphics (e.g. `plastic.dat`)
- `races/<race-id>/<race-id>.dat` — race-specific assets

## Scripts (this skill)

All live under `.cursor/skills/dark-oberon-dat/scripts/` (run from **repository root** or pass absolute paths):

```bash
# Single file → same folder as the .dat: <name>.dat-unpacked/ (textures/, sounds/, manifest.json)
bash .cursor/skills/dark-oberon-dat/scripts/unpack_dat.sh schemes/plastic.dat
# → schemes/plastic.dat-unpacked/

# Optional explicit output directory (overrides default):
bash .cursor/skills/dark-oberon-dat/scripts/unpack_dat.sh schemes/plastic.dat /tmp/out

# Repack (after editing TGAs; keep manifest.json consistent)
bash .cursor/skills/dark-oberon-dat/scripts/pack_dat.sh schemes/plastic.dat-unpacked schemes/plastic.dat

# Batch: every *.dat in a directory → each <file>.dat-unpacked next to that file (default: repo/dat/)
bash .cursor/skills/dark-oberon-dat/scripts/unpack_dat_dir.sh
```

Direct Python:

```bash
python3 .cursor/skills/dark-oberon-dat/scripts/do_dat_tool.py unpack schemes/plastic.dat -o /tmp/plastic_out
python3 .cursor/skills/dark-oberon-dat/scripts/do_dat_tool.py pack /tmp/plastic_out -o schemes/plastic.dat
```

## Workflow

1. **Unpack** → edit files under `textures/` (and `sounds/` if present).
2. Do **not** rename entries in `manifest.json` casually; paths must match on disk.
3. **Pack** back to the original `.dat` path (or a copy) and test in-game.

## References

- Loader implementation: [`src/dodata.cpp`](../../../src/dodata.cpp)
- TGA handling: [`src/tga.cpp`](../../../src/tga.cpp)

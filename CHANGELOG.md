# Changelog

All notable changes to this Dark Oberon fork are documented here.
Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
versioning: [SemVer 2.0](https://semver.org/spec/v2.0.0.html).

This fork starts its own version line at `0.1.0`. Upstream Dark Oberon 1.1.0
is the historical baseline and not tracked here.

## [Unreleased]

## [0.2.1] - 2026-09-24

### Changed
- README describes what the fork actually is and how to build it; `docs/obvious_bugs.md` now has a status
  column (only the `dofile.cpp` `strcpy` item is still open).
- `.gitignore` / `.dockerignore` fixed: build stamps are no longer tracked, and `ai-working/` plus unpacked
  `.dat` directories stay out of the Docker build context.

### Removed
- ~2000 lines of dead C++ code in `src/`: functions, methods, header inlines, macros and globals with no
  callers, set-but-unused locals, commented-out code blocks, and the unused `TMAP_SURFACE` activity array.
  No gameplay change.
- The retired Stable Diffusion / A1111 pipeline (`dark-oberon-sd` skill, `run_sd_board_batch.py`,
  `test_sd_api.py`) and the unreferenced `export_frames.py`, `import_frames.py`, `export_board_sections.py`.
- The unused `udf_lib/` stub and its orphaned `tests/conftest.py`.
- CVS/SourceForge-era make targets (`devel_stats.html`, `update_web`, `prepare-release`),
  `src/create_makefile.sh`, `src/Makefile~` and `src/.doxygen.log`.

## [0.2.0] - 2026-09-24

### Added
- **Orc race** `orc-red`, `orc-blue`, `orc-yellow` — 1:1 counterpart of the humans (16 entities) in the
  plasticine style, restyled with codex; team colours by deterministic recolouring.
- **4-frame sword attack animation** for the human footman and the orc Grunt (was one static, enlarged frame).
- **CPU AI**: random personality per CPU (aggressive / commercial / calm / rusher / turtle, ±10 % noise),
  `ai_level` in `config.cfg` and `addcpu [easy|medium|hard]`, military state machine (gather → attack with
  superiority → retreat), proportional defence, target scoring, expiring retaliation, dedicated scouts,
  randomised build sites and weighted unit mix. Pure decision module `src/doai_logic.*` with `make test-ai`.
- **Map generator** (`.cursor/skills/dark-oberon-map/scripts/generate_map.py`) for 2/4/6 players with closed
  coasts, rock-outlined plateaus with ramps and balanced resources, plus `map_check.py` and a textured
  isometric preview; 7 generated maps (Twin Ponds, Ridge Duel, Lake Country, Long Water, Crossroads,
  Great Bay, Six Hills).
- Race sprite pipeline with codex (`run_codex_board_batch.py`, `board_postprocess.py`, `attack_anim.py`,
  `recolor_race.py`, `validate_race.py`) and the race generation playbook
  (`.cursor/skills/dark-oberon-race/PLAYBOOK.md`); Python tests in `tests/race_pipeline` and `tests/mapgen`.
- Cursor skill `versioning-changelog` defining SemVer policy and changelog workflow.
- `TFOLLOWER::HasMyAddress()` predicate so callers can tell whether the leader has echoed the follower's externally-visible address yet.
- `docs/obvious_bugs.md` triage report.
- `ARCHITECTURE_REFACTOR_PLAN.md` tracking multi-week architecture work (sim/renderer split, `dofile.cpp` rewrite, deterministic-lockstep network model).
- `RENDER_OBJECTS` variable in `src/Makefile` documenting the 6 pure-rendering objects that should eventually leave the dedicated-server build.

### Changed
- App version now comes from the root `VERSION` file (SemVer) and is shown in the menu; was the stale upstream `1.0.2-RC1` hard-coded in `src/Makefile`.
- `InitMemorySestem` renamed to `InitMemorySystem` across `doalloc.{h,cpp}` and `doberon.cpp` (typo fix on a public API).
- `TMAP_SURFACE` activity array now sized via `PL_MAX_PLAYERS` instead of a hardcoded `8`.
- `CreateGame()` `@@FIXME@@` replaced with an actual contract comment about `Disconnect()` semantics.

### Fixed
- AI: never left the "establish" phase when a map starts with only a town hall; blocked worker training on
  the global energy balance; never built energy (farms); kept every miner on gold; re-ordered unaffordable
  repairs every tick (economy deadlock).
- `.dat` packing strips the TGA 2.0 footer Pillow writes (the engine reads textures sequentially and failed
  with "Error reading TGA data").
- `TNET_MESSAGE::Init_receive` no longer copies a fixed 255 bytes; it now reads only the actual message size and rejects out-of-range sizes (was a stack over-read into the message buffer).
- `TFOLLOWER` initialises `my_address` / `my_port` to a deterministic `0.0.0.0:0` instead of leaving them uninitialised; the single caller in `doengine.cpp` now gates on `HasMyAddress()` before classifying a player as local vs remote.
- Four bare `new` sites (`dobuildings.cpp`, `dosources.cpp`, `donet.cpp`, `glfont.cpp`) switched to the `NEW` macro so allocations are visible to the memory tracker.
- `src/dofile.cpp`: 6 unbounded `sprintf` callsites into `TFILE_LINE` buffers (`AddValue`, `WriteStr`, `WriteInt`, `WriteFloat`, `WriteDouble`, `WriteSimple`, `WriteByte`) now use `snprintf(..., FILE_MAX_LINE_LENGTH, ...)`. Previously a long `value` passed to `WriteStr` could overflow the 1024-byte stack buffer.
- `src/dofile.cpp` `Reload()` long-line accumulator (`buff = strcat(buff, values)`): added explicit length checks against `sizeof(buffer)` (10 × `FILE_MAX_LINE_LENGTH`) before each `strcat`. A malformed `.rac` file with many `_`-continued lines could previously overflow the 10240-byte stack buffer.

### Removed
- Test maps `orc_test` and `attack_test` (the race playbook describes a throwaway arena instead).

### Security
- Closed the `Init_receive` stack over-read described above (low exploitability today, but an attack surface for any future caller passing a smaller buffer).

## [0.1.0] - 2026-05-09

First fork release — baseline of all changes since the upstream snapshot.

### Added
- Headless dedicated server and headless client mode.
- AI for the computer player, including rally-point support from factories.
- CPU opponents in network games.
- In-game map editor (v0.1) — add players, reorder/place tiles.
- Race / map / data unpack scripts for working with `.rac` and `.dat` archives.
- Cursor skills for the AI sprite-board pipeline:
  - `dark-oberon-dat` — `.dat`/`.rac` extraction, board composition, slicing, packing.
  - `dark-oberon-race` — race authoring workflow.
  - `dark-oberon-map` — map authoring workflow.
  - `dark-oberon-codex` — Codex / OpenAI image-generation pipeline (Batch API + `codex exec` CLI), reads `OPENAI_API_KEY` from `.env`.
- `.env.example` template; `.env` is gitignored.

### Changed
- Sound layer ported from FMOD to SDL_mixer.
- Threading model reworked.
- Engine timing refactored.
- Graphical layer modules replaced.

### Removed
- Retired the local Stable Diffusion / Automatic1111 pipeline in favour of the Codex / OpenAI image pipeline
  (this repository still keeps the `dark-oberon-sd` skill for offline use).
- Stopped tracking generated log files (`logs/full.log`, `logs/error.log`, `src/.doxygen.log`) — they remain on disk locally but are no longer committed.

### Fixed
- Editor: adding players no longer corrupts the player list.
- `src/dowalk.h` minor build fix.
- `.gitignore` typo: `src/doxygen.log` → `src/.doxygen.log` so the actual filename is matched.

### Security
- Repo-wide secret audit: no live API keys, tokens, or private keys present.
- `.env` added to `.gitignore`; `.env.example` ships only a placeholder.

[Unreleased]: https://github.com/jakubfrieb/dark-oberon/compare/v0.2.1...HEAD
[0.2.1]: https://github.com/jakubfrieb/dark-oberon/compare/v0.2.0...v0.2.1
[0.2.0]: https://github.com/jakubfrieb/dark-oberon/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/jakubfrieb/dark-oberon/releases/tag/v0.1.0

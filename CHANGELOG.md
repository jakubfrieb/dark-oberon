# Changelog

All notable changes to this Dark Oberon fork are documented here.
Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
versioning: [SemVer 2.0](https://semver.org/spec/v2.0.0.html).

This fork starts its own version line at `0.1.0`. Upstream Dark Oberon 1.1.0
is the historical baseline and not tracked here.

## [Unreleased]

### Added
- Cursor skill `versioning-changelog` defining SemVer policy and changelog workflow.
- `TFOLLOWER::HasMyAddress()` predicate so callers can tell whether the leader has echoed the follower's externally-visible address yet.
- `obvious_bugs.md` triage report.

### Changed
- `InitMemorySestem` renamed to `InitMemorySystem` across `doalloc.{h,cpp}` and `doberon.cpp` (typo fix on a public API).
- `TMAP_SURFACE` activity array now sized via `PL_MAX_PLAYERS` instead of a hardcoded `8`.
- `CreateGame()` `@@FIXME@@` replaced with an actual contract comment about `Disconnect()` semantics.

### Fixed
- `TNET_MESSAGE::Init_receive` no longer copies a fixed 255 bytes; it now reads only the actual message size and rejects out-of-range sizes (was a stack over-read into the message buffer).
- `TFOLLOWER` initialises `my_address` / `my_port` to a deterministic `0.0.0.0:0` instead of leaving them uninitialised; the single caller in `doengine.cpp` now gates on `HasMyAddress()` before classifying a player as local vs remote.
- Four bare `new` sites (`dobuildings.cpp`, `dosources.cpp`, `donet.cpp`, `glfont.cpp`) switched to the `NEW` macro so allocations are visible to the memory tracker.
- `src/dofile.cpp`: 6 unbounded `sprintf` callsites into `TFILE_LINE` buffers (`AddValue`, `WriteStr`, `WriteInt`, `WriteFloat`, `WriteDouble`, `WriteSimple`, `WriteByte`) now use `snprintf(..., FILE_MAX_LINE_LENGTH, ...)`. Previously a long `value` passed to `WriteStr` could overflow the 1024-byte stack buffer.
- `src/dofile.cpp` `Reload()` long-line accumulator (`buff = strcat(buff, values)`): added explicit length checks against `sizeof(buffer)` (10 × `FILE_MAX_LINE_LENGTH`) before each `strcat`. A malformed `.rac` file with many `_`-continued lines could previously overflow the 10240-byte stack buffer.

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
- Retired the local Stable Diffusion / Automatic1111 pipeline (`dark-oberon-sd` skill) in favour of the Codex / OpenAI image pipeline.
- Stopped tracking generated log files (`logs/full.log`, `logs/error.log`, `src/.doxygen.log`) — they remain on disk locally but are no longer committed.

### Fixed
- Editor: adding players no longer corrupts the player list.
- `src/dowalk.h` minor build fix.
- `.gitignore` typo: `src/doxygen.log` → `src/.doxygen.log` so the actual filename is matched.

### Security
- Repo-wide secret audit: no live API keys, tokens, or private keys present.
- `.env` added to `.gitignore`; `.env.example` ships only a placeholder.

[Unreleased]: https://github.com/jakubfrieb/dark-oberon/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/jakubfrieb/dark-oberon/releases/tag/v0.1.0

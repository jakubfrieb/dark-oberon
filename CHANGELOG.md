# Changelog

All notable changes to this Dark Oberon fork are documented here.
Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
versioning: [SemVer 2.0](https://semver.org/spec/v2.0.0.html).

This fork starts its own version line at `0.1.0`. Upstream Dark Oberon 1.1.0
is the historical baseline and not tracked here.

## [Unreleased]

### Added
- Cursor skill `versioning-changelog` defining SemVer policy and changelog workflow.

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

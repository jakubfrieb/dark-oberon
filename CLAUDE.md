# Dark Oberon fork — project instructions

- Communicate with the user in Czech.
- Race and map workflows: `.cursor/skills/dark-oberon-race/PLAYBOOK.md`, `.cursor/skills/dark-oberon-map/`.
- Tests: `make test-ai` (C++ AI logic), `python -m pytest tests/race_pipeline tests/mapgen`.

## Versioning & Changelog (SemVer) — BINDING
- Every **finished iteration** is versioned using Semantic Versioning **MAJOR.MINOR.PATCH**:
  - MAJOR = incompatible change, MINOR = new backward-compatible feature, PATCH = fix.
  - Detailed rules (what counts as breaking for `.dat`/`.rac`/maps/network protocol, entry categories):
    `.cursor/skills/versioning-changelog/SKILL.md`.
- Maintain **CHANGELOG.md** in the [Keep a Changelog](https://keepachangelog.com) format:
  add changes continuously under `## [Unreleased]` (Added/Changed/Fixed/Removed/Security); when an iteration
  is finished, move Unreleased to `## [X.Y.Z] - YYYY-MM-DD`, update the links at the end of the file and bump the version.
- **The single source of the version is the `VERSION` file** in the root (e.g. `0.2.0`). `src/Makefile` generates
  `build_info.h` from it (`DO_VERSION_STRING`, shown in the menu). Never hardcode the version anywhere else.
- "Finished iteration" = a state merged into `master` that builds and is playable.
- A change is not done until the CHANGELOG is updated and the version is bumped.
- Git tag `vX.Y.Z` and push only when the user asks.

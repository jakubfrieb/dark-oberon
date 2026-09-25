# Changelog

All notable changes to this Dark Oberon fork are documented here.
Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
versioning: [SemVer 2.0](https://semver.org/spec/v2.0.0.html).

This fork starts its own version line at `0.1.0`. Upstream Dark Oberon 1.1.0
is the historical baseline and not tracked here.

## [Unreleased]

## [0.5.0] - 2026-09-25

### Added
- Dev console `god`: your units take no damage; `god all` makes every player's units invulnerable (map
  objects such as trees and mines are not affected); `god off` turns it off. Local games only, like the
  other dev commands.
- Performance test `tests/cpp/perf_smoke.sh [units] [seconds] [warmup] [map]`: a headless server with CPU
  players places the units around the bases, sends them all to the middle of the map and measures the
  battle: simulation step time (events and AI; avg, p50, p95, p99, max, share over the 20 ms budget),
  path search jobs in the thread pool and the length of their queue. Optional limits
  `PERF_MAX_STEP_P99_MS` and `PERF_MAX_PATH_QUEUE` make it fail. New server commands behind it:
  `spawn <n> [fight]` and `perf [reset]`; statistics in `src/doperf.*` (tested in `make test-ai`).

### Changed
- Path finding uses one thread per CPU core minus one (at least the original 5, at most 8) instead of
  always 5; the A* jobs do not take the simulation lock. With 1200 fighting units 8 threads halve the
  queue of waiting path searches; more threads made each search and the simulation step slower.
  `DO_PATH_THREADS=<n>` overrides the count.

### Fixed
- A CPU soldier that got ahead of its army kept walking to a building and hitting it while a player
  was attacking it: attack and retreat decisions only looked at the army as a whole, and the army order
  kept re-sending the unit at its target. Each army unit now reacts on its own: it turns on the enemy
  attacking it, or falls back to the rally point when it is outnumbered where it stands
  (`TAI_UnitReaction`, personality `retreat_ratio`).
- CPU scouts walked into the enemy base and kept hitting the first building they reached, even while
  being attacked, because scouts are left out of defense and the army. Scouts now ignore enemies, run
  home as soon as they are hit or targeted, and keep away from that base for 60 s.

## [0.4.1] - 2026-09-25

### Changed
- `server/up-server` starts the lobby in the background (`up -d`) and then only follows the logs, so
  Ctrl+C or a closed terminal no longer stops the lobby and the running games. It prints the lobby URL
  from `.env` (`PROJECT_WEBSITE`), and `--no-logs` returns right after the start.

### Added
- Game connections use TCP keepalive (30 s idle, then 6 probes 10 s apart where the system allows it,
  the system default on Windows), so NATs and firewalls keep quiet connections open and a dead peer is
  detected in about a minute.
- Connection logging on the game server at Info/Warning level: every connection, clean close, reset,
  a message cut off, and a failed send, with the peer's IP and port and the reason.
- Lobby log: every game server start, stop (and why: owner, idle, max lifetime, lobby exiting) and any
  unexpected end with its exit code or signal.

### Fixed
- A player whose connection broke with an error (e.g. `Connection reset by peer`) stayed in the game as
  a ghost: the listener thread returned without reporting the disconnect.
- Lobby: the thread that reads a game server's log died silently on the first byte that was not valid
  UTF-8 (e.g. a player name with national characters). Nobody drained the pipe afterwards, and after
  about 64 KB of log the game server blocked and the game froze. The log is now decoded leniently and
  read until the server exits.
- Lobby: when the lobby worker exits, it stops its games and logs it, instead of the games ending
  silently when their stdin closes.
- README: the Oberon Cloud lobby is at `https://oberon-game.cloud.digitalmind.cz`.

## [0.4.0] - 2026-09-25

### Added
- Map scrolling with W, A, S, D (in addition to the arrow keys).

### Changed
- Faster game pace for all races: construction, upgrades and repairs are 3× faster and factories
  produce units 1.5× faster (`src/dopace.h`; the `.rac` files keep the original timings). A peon now
  builds a barracks in about 70 s instead of 200 s. Players of one network game must run the same version.
- Hotkeys: Stay moved from S to H (hold) and Attack from A to T, because S and A now scroll the map.

### Fixed
- Orcs: the Goblin Workshop and the flying Goblin Zeppelin flickered, because codex had repainted each
  animation frame separately. The zeppelin now changes only at its propeller (the part that moves in the
  original animation); the workshop, which has no wheel like the human Manufactory, stands still. All
  three orc colours. New pipeline script `stabilize_frames.py` (dark-oberon-dat skill, `--freeze` for
  groups without a moving part) does this against the human reference race.
- Dedicated server crashed (`terminate called after throwing an instance of 'int'`) when any TCP
  connection to the game port sent an invalid size byte or closed in the middle of a message, e.g. a
  port scanner on the public internet. Such a connection is now dropped with a warning; the game goes on.

## [0.3.1] - 2026-09-24

### Added
- Portable Linux release: `make linux-dist` builds the client in an Ubuntu 22.04 Docker container and packs
  it with the game data into `dist/dark-oberon-<version>-linux-x86_64.tar.gz`. It needs glibc 2.34 or newer
  (Debian 12, Ubuntu 22.04, Fedora 35 and later) plus SDL2, SDL2_mixer and GLU from the distro.

### Fixed
- The game did not build with SDL2_mixer older than 2.6 (e.g. Ubuntu 22.04): `dosound.h` forward-declared
  `struct Mix_Music`, which conflicts with the older `typedef struct _Mix_Music Mix_Music`.

### Changed
- `make dist` builds the Windows, Haiku and Linux packages in parallel.
- `make windows-clean` removes only the Windows packages from `dist/`, not the Linux and Haiku ones.

## [0.3.0] - 2026-09-24

### Added
- Windows (x86_64) build, cross-compiled with MinGW-w64: `make windows` produces
  `dist/dark-oberon-<version>-win64.zip` (exe, SDL2/SDL2_mixer DLLs, game data). The C/C++ runtime is
  linked statically. One-time setup: `sudo scripts/setup-windows-toolchain.sh` and
  `scripts/fetch-windows-deps.sh`.
- Haiku (x86_64) support: builds natively with the regular `make -C src SOUND=1` (needs
  `libsdl2_devel sdl2_mixer_devel glu_devel` from HaikuDepot/pkgman); sockets link against `libnetwork`.
  `make haiku` builds it on a Haiku machine over SSH (`HAIKU_HOST`, default `haiku`) and fetches
  `dist/dark-oberon-<version>-haiku-x86_64.zip`.
- Lobby accounts: registration and sign-in (name + password, no e-mail). Hosting a game needs an
  account, each account can run one game at a time, and only its owner can start or stop it. Joining
  still needs only the address. Accounts live in SQLite in the `lobby-data` Docker volume; sign-up and
  sign-in are rate limited per IP, forms and API calls are CSRF-protected.
- Lobby housekeeping: a game with no human player connected stops after `LOBBY_IDLE_MINUTES` (30), and
  any game after `LOBBY_MAX_GAME_HOURS` (8).
- New lobby settings in `server/.env.example`: `LOBBY_SECRET_KEY`, `TRUSTED_PROXIES`,
  `SESSION_COOKIE_SECURE`, `LOBBY_IDLE_MINUTES`, `LOBBY_MAX_GAME_HOURS`. Lobby tests in `tests/lobby`.

### Changed
- The lobby web page is restyled after the game's main menu (stone backdrop with the plasticine
  knights, the gold title, black panels, menu-style commands) and works on phones. The manual port
  field is gone; ports are assigned automatically.
- README: internet play explains accounts, hosting vs. joining and when games stop by themselves.
- README: the game is documented as running on Linux, Windows and Haiku, with OS badges, a platform table,
  a link to the release zips and build steps for each. The Linux steps now build with sound by default.
- The top-level `make` builds the Linux client with sound (SDL2_mixer) by default; `make SOUND=0` builds
  without it.

### Removed
- `build-classic.sh`: it only ran `make -C src -j$(nproc)`, and nothing referred to it.
- `docs/obvious_bugs.md`: all its bugs are fixed; the two design notes moved to
  `ARCHITECTURE_REFACTOR_PLAN.md`.

### Fixed
- Dedicated server: when its stdin closed (the lobby exited or restarted), the server spun at 100 % CPU
  forever instead of quitting, because `poll()` reported `POLLHUP` without `POLLIN`.
- Windows: crash on startup while listing maps. The `_findfirst` handle was stored in a 32-bit `long`,
  which truncates it on Win64 (same bug in race loading). A map file without an extension no longer
  crashes the map list either.
- Windows: variables named `near`/`far` in the CPU player code clashed with `windows.h` macros.
- Crash (double free) when leaving a network game, e.g. hosting and then connecting elsewhere: the
  dispatcher thread was joined twice, first by the listener that consumes its queue and then by the
  dispatcher itself. It showed up on Haiku, and on the other systems it was silent undefined behaviour.
- Config-file parser (`dofile.cpp`): values from `config.cfg`, maps, races and schemes were copied into
  fixed-size buffers without a length check. A player name over 20 characters or a map name over 30 could
  overwrite memory. Values are now copied with the buffer size and truncated, with a warning in the log.
  The config file path is no longer limited to 128 characters (deep Windows folders), and two `delete`
  calls on arrays were corrected to `delete[]`. Covered by `tests/cpp/test_dofile.cpp` (built with
  AddressSanitizer, part of `make test-ai`).
- `tests/cpp/ai_smoke.sh` failed to build since 0.2.0 because it did not copy `VERSION`.
- The map list no longer relies on `dirent::d_type` (missing on Haiku) and no longer crashes on a file
  without an extension in `maps/`.

## [0.2.4] - 2026-09-24

### Changed
- `README` rewritten as `README.md`: screenshots, what the fork adds (orc race, CPU players, map editor,
  new maps), how to play over the internet through the Oberon Cloud lobby, and thanks to the original
  authors.

### Fixed
- `docker compose up` in `server/` failed with `No rule to make target '../VERSION'`: the builder stage
  copied only `src/`. The Dockerfile now copies `VERSION` too, and `src/build_info.h` is excluded from the
  Docker context so a stale local build stamp can't end up in the image.

## [0.2.3] - 2026-09-24

### Changed
- `server/docker-compose.yml` is deployment-neutral: settings come from `server/.env` (project name, web/host
  ports, game port range, public game host, gunicorn access log) with defaults, and the lobby web UI is
  published directly on `WEB_HOST_PORT`. Hosting-specific config (reverse proxy labels, external networks,
  hostnames) was removed from the repository and belongs in a local, gitignored
  `server/docker-compose.override.yml`, which `server/up-server` loads automatically when present.
- Credits screen (`dat/gui.dat`, `bg_credits`): "Powered by" now shows SDL2 (www.libsdl.org) instead of
  GLFW, which the game no longer uses; "(digitalmind.cz)" added under the developers in the same
  calligraphic face as the names.

### Added
- `server/.env.example` documenting all Docker variables.

### Fixed
- Opening the Map Editor while a game was running silently failed and bounced back to the main menu
  (the click then fell through to Credits). The Map Editor button now asks to disconnect first, like
  the other menu actions that end a session.

## [0.2.2] - 2026-09-24

### Changed
- All Slovak/Czech source comments and debug/log strings in `src/` translated to English (no code changes).
- Documentation translated to English: the programmer, user and project manuals (`docs/*_SK.md` renamed to
  `docs/*_EN.md`; the original Slovak PDFs are kept), `RACE_SPEC_FOR_AI.md`, design specs and plans,
  `CLAUDE.md`, the race playbook, skills and server scripts.

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

[Unreleased]: https://github.com/jakubfrieb/dark-oberon/compare/v0.5.0...HEAD
[0.5.0]: https://github.com/jakubfrieb/dark-oberon/compare/v0.4.1...v0.5.0
[0.4.1]: https://github.com/jakubfrieb/dark-oberon/compare/v0.4.0...v0.4.1
[0.4.0]: https://github.com/jakubfrieb/dark-oberon/compare/v0.3.1...v0.4.0
[0.3.1]: https://github.com/jakubfrieb/dark-oberon/compare/v0.3.0...v0.3.1
[0.3.0]: https://github.com/jakubfrieb/dark-oberon/compare/v0.2.4...v0.3.0
[0.2.4]: https://github.com/jakubfrieb/dark-oberon/compare/v0.2.3...v0.2.4
[0.2.3]: https://github.com/jakubfrieb/dark-oberon/compare/v0.2.2...v0.2.3
[0.2.2]: https://github.com/jakubfrieb/dark-oberon/compare/v0.2.1...v0.2.2
[0.2.1]: https://github.com/jakubfrieb/dark-oberon/compare/v0.2.0...v0.2.1
[0.2.0]: https://github.com/jakubfrieb/dark-oberon/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/jakubfrieb/dark-oberon/releases/tag/v0.1.0

# Architecture refactor plan

Tracking the multi-week refactors that won't fit in a single PR but matter for
the fork's direction (especially: making `dark-oberon-server` a real headless
binary and shrinking the maintenance surface). Companion to `obvious_bugs.md`
(which lists drive-by fixes).

## Status legend
- ✅ done
- 🚧 in progress
- 📋 planned, not started
- ❌ blocked

---

## #1 — Delete custom DEBUG_MEMORY allocator ✅

**Done in commit `8080aae` (2026-05-10).** −418 lines + 174 `NEW`→`new` diffs;
`-fsanitize=address,undefined` is the recommended replacement for debug builds.

---

## #2 — Sim / renderer split (server target without -lGL) 📋

### Goal
The dedicated server (`dark-oberon-server`) currently links the entire
rendering surface — `glgui.o`, `glfont.o`, `dodraw.o`, `domouse.o`,
`doglfw_sdl.o`, `tga.o` (~7,500 lines, 247 `gl*` calls) plus `-lGL -lGLU` —
even though no GL function is ever invoked at runtime. The "headless" server
binary is therefore ~6.3 MB (vs. an estimated ~3 MB for a real headless build),
needs an X-able libGL on the host to even start, and inflates the link
dependency for the Python lobby / Codex pipeline use cases.

The architectural goal is two link units:

```
libdosim.a   (sim + network + data + config)   ← no GL, no SDL_video
                                                ↑
                ┌───────────────────────────────┴─────────────────────┐
                │                                                     │
       dark-oberon-server                                       dark-oberon
       (libdosim + doserver +                          (libdosim + doengine_*
        dohost + doleader)                              + dodraw + glgui +
                                                        glfont + tga + domouse
                                                        + doglfw_sdl)
```

The cheap test that the split is correct: **the server target must not pass
`-lGL -lGLU` to the linker.**

### Why it's blocked

Excluding `RENDER_OBJECTS` from `OBJECTS_SERVER` in `src/Makefile` produces
**98 distinct undefined references** at link time (regenerate with the
"Reproducing the analysis" snippet below). They split as:

| Bucket | Count | What they are |
|---|---|---|
| `TGUI_*` methods | 59 | `TGUI_BOX::Show()`, `TGUI_PANEL::AddButton(...)`, `TGUI_LABEL::SetFontColor(...)`, etc. — the GUI toolkit, defined in `glgui.cpp`, called from `doengine.cpp`, `dobuildings.cpp`, `dodata.cpp`, `dodevcheat.cpp` |
| Free render functions | ~13 | `DrawGame()`, `MouseToMap()`, `tgaRead()` — top-level render entry points |
| `glfw*` shims | 6 | `glfwSwapBuffers`, `glfwPollEvents`, etc. — defined in `doglfw_sdl.cpp`, called from `doengine.cpp` |
| `TFPS` / `TMOUSE` methods | 5 | `TFPS::Reset/Update`, `TMOUSE::RectSelect` |
| Globals | 21 | `mouse`, `gui`, `ost`, `projection`, `fps`, `fps_of_update`, `show_all`, `reduced_drawing`, `view_segment`, `typeinfo for TGUI_PANEL`, … |

The vast majority originate in **one file**: `src/doengine.cpp` (7,136 lines,
191 functions). Engine init, GUI-callback handlers, render loop, network
handlers, editor hooks, and config UI are all in one `.cpp`. Until that file
is split, `OBJECTS_SERVER` *needs* `doengine.o`, and `doengine.o` *needs* the
entire GUI toolkit.

### The unblocker: split `doengine.cpp`

`doengine.cpp` already has 30+ `//===` section markers that map cleanly onto
the natural cuts:

| New file | Current sections (approx) | Approx lines | Sim/render |
|---|---|---|---|
| `doengine_init.cpp` | InitAll, ShutdownIO, CreateGame, Disconnect, top-level globals | ~1,500 | sim |
| `doengine_net.cpp` | ProcessNetEvent, ProcessFunction, ProcessHello, lobby callbacks | ~2,000 | sim |
| `doengine_render.cpp` | Draw, DrawGame, DrawFps, frame loop, FPS class | ~1,500 | render |
| `doengine_gui.cpp` | All `*OnClick`, menu/editor handlers | ~2,000 | render |
| `doengine.cpp` (residual) | top-level glue, declarations | ~150 | sim |

After the split:
1. `OBJECTS_SERVER` includes `doengine_init.o`, `doengine_net.o` (sim halves)
   and excludes `doengine_render.o`, `doengine_gui.o`.
2. The remaining 98-symbol surface drops by ~70% because `doengine_*.cpp` is
   the biggest leaker. Most of the rest moves into `dobuildings.cpp` /
   `dodata.cpp` / `dodevcheat.cpp` — those files are also half sim, half
   presentation, but at smaller scale (1–10 leaks each).
3. The remaining cross-references get either:
   - **Stubbed** — for things the server doesn't need, provide an empty impl
     in a new `dohostonly_stubs.cpp` linked only into the server.
   - **Lifted** — for things both targets need, move the symbol from
     `glgui.cpp` into a new `dogui_state.cpp` (sim-side state, no GL).

### Recommended phasing

| Phase | What | Risk | Payoff |
|---|---|---|---|
| 2.0 | Split `doengine.cpp` along section markers, no behaviour change | medium | unblocks everything below |
| 2.1 | Move `mouse`, `gui`, `ost`, `view_segment`, `projection` globals into a small `gui_state.{cpp,h}` that has no GL deps | low | ~5 leaks gone |
| 2.2 | Stub `glfw*` shims for HEADLESS in `doglfw_sdl.cpp` (return immediately if no display); keep file in `OBJECTS_SERVER` | low | 6 leaks gone |
| 2.3 | Add empty HEADLESS stubs for the `TGUI_*` methods that sim code accidentally calls (or properly gate the calls) | medium | 59 leaks gone |
| 2.4 | Drop `RENDER_OBJECTS` from `OBJECTS_SERVER` and `-lGL -lGLU` from `SERVER_LIBRARIES` | low (just config) | server binary halves in size, no libGL runtime dep |

### Reproducing the analysis

```bash
cp src/Makefile /tmp/Makefile.bak
sed -i 's|OBJECTS_SERVER = $(filter-out doberon.o,$(OBJECTS)) doserver.o|OBJECTS_SERVER = $(filter-out doberon.o $(RENDER_OBJECTS),$(OBJECTS)) doserver.o|' src/Makefile
sed -i 's|^SERVER_LIBRARIES = -pthread -lGL -lGLU|SERVER_LIBRARIES = -pthread|' src/Makefile

make -C src clean -s
make -C src -j$(nproc) HEADLESS=1 SOUND=0 ../dark-oberon-server 2>&1 \
  | grep "undefined reference" \
  | sed 's/.*undefined reference to `//' | sed "s/'$//" \
  | sort -u > /tmp/boundary_symbols.txt
wc -l /tmp/boundary_symbols.txt   # should print 98 today

cp /tmp/Makefile.bak src/Makefile
make -C src clean -s
```

### Out of scope for #2

- Replacing `glfw*` shims with native SDL2 calls (the shim is short, leave it).
- Cleaning up the GUI toolkit itself (`TGUI_*` design is fine for the game's
  needs, no urgency to migrate to imgui).
- `tga.cpp` → `SDL_image` (do this separately when the file format support is
  the bottleneck, not because of the sim/render split).

---

## #3 — Replace `dofile.cpp` (2,123 lines) with a tiny INI parser 🚧

### Goal
`src/dofile.cpp` is a hand-rolled section/key/value parser with **16 unbounded
`strcpy` / `sprintf` / `strcat` callsites** (bug #3 in `obvious_bugs.md`). It
is the universal config reader (config files, `.rac` race files, `.dat` data
files). Every typed `Read*` / `Write*` method duplicates string→type parsing.

The cheap path: drop in [`inih`](https://github.com/benhoyt/inih) (~200-line
single-file C parser, public domain), keep the on-disk INI format unchanged,
replace `dofile.cpp`'s parsing internals with `ini_parse(...)` callbacks, keep
the `TFE_*` schema classes as a thin facade.

### Why this isn't being done in one shot

`dofile.cpp` is the I/O layer for **every** game-side config file
(`config.cfg`, all `.rac` race definitions, all `.dat` data tables, map
metadata). Drop-in replacing it requires:

1. Auditing every callsite (`grep -rln 'TFE_SECTION\|TCONF_FILE\|TFILE_LINE_PARSER' src/`)
   — there are dozens, scattered across `doconfig.cpp`, `doraces.cpp`,
   `domap.cpp`, `dodata.cpp`, `doschemes.cpp`, `dosound.cpp`.
2. Verifying parity on the shipped `.rac` files (orcs, humans).
3. Migrating the *write* paths too — `inih` only reads. We'd need either a
   tiny custom writer or a different lib (`mINI`, `tomlplusplus`).

### Recommended phasing

| Phase | What | Risk | Payoff |
|---|---|---|---|
| 3.0 | Vendor `inih` into `src/third_party/inih/` (header + .c) | none | preparation |
| 3.1 | Add a parallel `dofile2.cpp` that wraps `inih` with the same `TFE_SECTION` interface, off by default | low | API parity, can be tested side-by-side |
| 3.2 | Migrate `doconfig.cpp` to use `dofile2`; verify game settings round-trip | medium | smallest user-facing surface |
| 3.3 | Migrate the read-only paths (`doraces.cpp`, `dodata.cpp`, `doschemes.cpp`) | medium | covers ~80% of file I/O |
| 3.4 | Replace the writer (custom 50-line emitter or migrate to `mINI`) | low | covers the remaining 20% |
| 3.5 | Delete `dofile.cpp`; bug #3 in `obvious_bugs.md` is closed | none | −2,000 lines, no more `strcpy` on caller buffers |

### Out of scope for #3

- Migrating the on-disk format from INI to TOML/JSON. That's a user-facing
  break (existing user races stop working) and needs a converter — separate
  decision.

---

## #4 (deferred) — Deterministic lockstep network model

`AppGetTimeSeconds()` is read inside game logic in 14 files (see
`/graphify query "AppGetTimeSeconds"` for the full call graph). The current
network sync model relies on the leader broadcasting timestamped events;
followers replay them in order. This works but ties bandwidth to event volume
and prevents free deterministic replay.

A real lockstep model (broadcast inputs, not events; share a fixed RNG seed;
advance simulation in fixed ticks) would:

- Cut network bandwidth to a tiny fraction of today's.
- Make replays free (record inputs, not state).
- Make desync detection trivial (CRC the sim state every N ticks).

But it requires the sim to be **deterministic** — and reading wall-clock time
from `domapunits.cpp`, `doforces.cpp`, `doai.cpp`, etc. defeats that. The
prerequisite is replacing `AppGetTimeSeconds()` calls inside sim code with a
simulation tick counter.

This is multi-month, conceptually elegant, and unlocks high-end multiplayer
features. Do it after #2 has cleanly separated the simulation library.

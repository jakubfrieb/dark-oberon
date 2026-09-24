# Obvious Bugs — Dark Oberon Fork

Triage list of bugs / smells found during a code audit. Fixed items are kept
in the table for traceability; their write-ups and fixes are in the 0.2.0
CHANGELOG entry and git history.

| # | Severity | File | One-liner | Status |
|---|----------|------|-----------|--------|
| 1 | **HIGH** | `src/donet.cpp` | `TNET_MESSAGE::Init_receive` memcpy'd a fixed 255 bytes regardless of real message size | Fixed in 0.2.0 |
| 2 | **MEDIUM** | `src/doalloc.{h,cpp}` | Public API typo `InitMemorySestem` | Fixed in 0.2.0 |
| 3 | **MEDIUM** | `src/dofile.cpp` | Unbounded `strcpy` / `sprintf` / `strcat` into caller-supplied buffers | **Partly open** |
| 4 | **MEDIUM** | `src/domap.cpp` | `TMAP_SURFACE` hardcoded `8` instead of the player count | Fixed in 0.2.0 |
| 5 | **LOW** | `src/doengine.cpp` | `CreateGame()` carried an unresolved `@@FIXME@@` marker | Fixed in 0.2.0 |
| 6 | **LOW** | several | Bare `new` instead of the project's `NEW` macro | Fixed in 0.2.0 |
| 7 | **INFO** | `src/dofollower.h` | `XXX: have_my_address` — uninitialised address accessors | Fixed in 0.2.0 |

---

## 3. MEDIUM — Unbounded `strcpy` family in `dofile.cpp` (partly open)

**Done (0.2.0):** all `sprintf` writes into `TFILE_LINE` buffers use
`snprintf(..., FILE_MAX_LINE_LENGTH, ...)`, and the `Reload()` long-line
accumulator checks the remaining space before each `strcat`.

**Still open:** roughly a dozen `strcpy` calls into caller-supplied buffers
(`line`, `word`, `values`, `name`, `value`, `indent_string`, the default-value
copy in the `Read*` getters). Each one trusts the caller to have allocated a
large-enough buffer. The `.rac` / `.dat` / config parsers feed this code from
files on disk — including user-installed races — so a malformed file with a
long token can overrun a fixed-size `char name[…]` in the caller. Mostly a
stability concern today; an attack surface once maps/races are shared online.

### Fix
Pass buffer sizes through the API and switch to `snprintf` / bounded copy with
explicit NUL termination (or `std::string` for the public surface):

```cpp
// before
void TFILE_LINE_PARSER::ReadString(char *value, const char *val) {
    strcpy(value, val);
}

// after — pick one of:
void TFILE_LINE_PARSER::ReadString(char *value, size_t cap, const char *val) {
    snprintf(value, cap, "%s", val);
}
// or
std::string TFILE_LINE_PARSER::ReadString(const char *val) { return val; }
```

This is a multi-day refactor — start with the input-facing parsers
(`ReadString`, `GetWord`, `GetLine`) and let the typechecker walk the
callsites. Tracked as refactor #3 in `ARCHITECTURE_REFACTOR_PLAN.md`.

---

## Notes (not bugs)

- **`AppGetTimeSeconds()` as a god node** — design observation, not a defect. It is the deliberate single global clock used for both frame pacing and network event timestamps. The companion `AppSetTimeSeconds()` exists exactly so a follower / headless server can be slewed to the leader's clock. Make sure any new event-emitter stamps with `AppGetTimeSeconds()` (or routes through `SendEvent()`, which already does).
- **Switch fall-throughs** (`doforces.cpp`, `doworkers.cpp`, `domapunits.cpp`) — verified intentional case-grouping, not missing-`break` bugs.

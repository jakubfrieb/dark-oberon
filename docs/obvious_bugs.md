# Obvious Bugs — Dark Oberon Fork

Triage list of bugs / smells found during a code audit. Severity rationale is
honest about whether something is "exploitable today" vs "ticking time-bomb".

| # | Severity | File | One-liner |
|---|----------|------|-----------|
| 1 | **HIGH** | `src/donet.cpp:213` | `TNET_MESSAGE::Init_receive` blindly memcpys a fixed 255 bytes regardless of real message size |
| 2 | **MEDIUM** | `src/doalloc.{h,cpp}` + `src/doberon.cpp:141` | Public API typo `InitMemorySestem` (should be `System`) |
| 3 | **MEDIUM** | `src/dofile.cpp` | 16 occurrences of unbounded `strcpy` / `sprintf` into caller-supplied buffers |
| 4 | **MEDIUM** | `src/domap.cpp:383` | `TMAP_SURFACE` hardcodes `i = 8` instead of `player_array.GetCount()` (TODO left in code) |
| 5 | **LOW** | `src/doengine.cpp:1543` | `CreateGame()` carries an unresolved `@@FIXME@@` marker |
| 6 | **LOW** | `src/dobuildings.cpp:569`, `src/donet.cpp:862`, `src/glfont.cpp:93`, `src/dosources.cpp:319` | Bare `new` used instead of the project's `NEW` macro — bypasses memory tracking |
| 7 | **INFO** | `src/dofollower.h:78-79` | `XXX: have_my_address` — uninitialized accessor concern |

---

## 1. HIGH — `TNET_MESSAGE::Init_receive` over-reads stack and corrupts message buffer

**File:** `src/donet.cpp:211-217`
**FIXME comment by original author:** *"tu kopirovat iba velkost spravy"* (here copy only the message size).

```cpp
void TNET_MESSAGE::Init_receive (in_addr address, in_port_t port, int fd, T_BYTE *data) {
  /*** FIXME: tu kopirovat iba velkost spravy ***/
  memcpy (&this->size, data, max_net_message_size);   // ← always copies 255 bytes
  ...
}
```

### Why it's broken
The wire protocol (`donet.cpp:522-543`) reads a single byte = message size, then
recvs exactly `*size` more bytes into a stack-local `T_BYTE buf[255]`. So `data`
points at a buffer with **only `data[0]` valid for sure** and `data[0..*size-1]`
holding the actual message — typically far less than 255 bytes.

`Init_receive` then copies `max_net_message_size` (constant 255) bytes into the
struct, starting at `&this->size`. Struct layout is intentionally
`size; type; subtype; dest; buf[255]; …` (donet.h:241-249), so the copy fills
the header (4 bytes) plus 251 bytes of `buf`. The remaining 4 bytes of `buf`
keep stale stack data from the caller.

### Real impact
- **Information leak**: 4+ bytes of caller stack memory end up in `this->buf`. If a downstream consumer ever reads beyond `this->size` it sees stack garbage.
- **TCP path (listener_accept)**: caller buffer is `T_BYTE buf[255]` on the listener thread stack, so the over-read stays inside that buffer — no segfault, but values past the real payload are non-deterministic.
- **UDP / receive paths** that pass a smaller temporary buffer would over-read past the end. None such exist today (only `listener_accept` calls this with a `[255]` buffer), but any future caller is a landmine.
- The bug also makes the function impossible to unit-test deterministically since it always reads 255 bytes regardless of payload length.

### Fix
```cpp
void TNET_MESSAGE::Init_receive(in_addr address, in_port_t port, int fd, T_BYTE *data)
{
    // First byte is the size header (see TNET_LISTENER::listener_accept).
    T_BYTE msg_size = data[0];
    if (msg_size < GetHeaderSize() || msg_size > max_net_message_size) {
        Critical("Init_receive: invalid message size %d", msg_size);
        throw 0;
    }
    memcpy(&this->size, data, msg_size);

    this->address = address;
    this->port    = port;
    this->fd      = fd;
    this->extract_p = 0;
}
```

---

## 2. MEDIUM — Typo in public memory-system API: `InitMemorySestem`

**Files:**
- `src/doalloc.h:57` — declaration
- `src/doalloc.cpp:120` — definition
- `src/doberon.cpp:141` — sole caller

```cpp
void InitMemorySestem(void);   // doalloc.h
```

### Why it matters
- Cosmetic bug, but the function is public API in `doalloc.h`. Any future
  module that wants to opt into the custom allocator has to hand-paste a typo.
- Typo is asymmetric with `DoneMemorySystem()` (correctly spelled), which is
  itself a signal the typo was unintentional.

### Fix
Rename to `InitMemorySystem`. Add a short transitional alias only if anything
external could plausibly depend on the old name (unlikely — it's a leaf binary).

```cpp
// doalloc.h
void InitMemorySystem(void);
```

Update `doalloc.cpp:120` and `doberon.cpp:141`.

---

## 3. MEDIUM — Unbounded `strcpy` / `sprintf` family across `dofile.cpp`

**File:** `src/dofile.cpp` — at least 16 unsafe call sites.

Hot list:
| Line | Call |
|------|------|
| 145 | `strcpy(line, tmp.c_str())` |
| 204 | `strcpy(word, p)` |
| 250 | `strcpy(values, line)` |
| 317 / 321 / 352 / 383 / 459 / 734 / 798 | further `strcpy`s into caller buffers |
| 375 / 389 / 1084 / 1103 / 1123 | `sprintf` into caller buffers (no length cap) |
| 949 / 954 | `strcat(buff, values)` chained — classic overflow vector |

### Why it matters
- Every one of these trusts the caller to have allocated a large-enough buffer.
  The .rac/.dat/config parsers feed this code, and those formats come from
  files on disk — including user-installed races. A malformed race file with a
  long token can overrun a fixed-size `char name[…]` in the caller.
- This is the same class of bug that made e.g. Quake-engine mods exploitable
  via custom maps. For an indie RTS this is mostly a stability concern, but if
  you ever ship multiplayer with shared maps it becomes a real attack surface.

### Fix
Pass buffer sizes through the API and switch to `snprintf` / `strncpy` +
explicit NUL termination (or std::string for the public surface):

```cpp
// before
void TFILE_LINE_PARSER::ReadString(char *value, const char *val) {
    strcpy(value, val);   // dofile.cpp:734
}

// after — pick one of:
void TFILE_LINE_PARSER::ReadString(char *value, size_t cap, const char *val) {
    snprintf(value, cap, "%s", val);
}
// or
std::string TFILE_LINE_PARSER::ReadString(const char *val) { return val; }
```

This is a multi-day refactor — start with the input-facing parsers
(`ReadString`, `GetWord`, `GetLine`) and let the typechecker walk the callsites.

---

## 4. MEDIUM — `TMAP_SURFACE` hardcodes player count `8`

**File:** `src/domap.cpp:383-391`

```cpp
TMAP_SURFACE::TMAP_SURFACE() {
  //TODO
  int i = 8, j = 0;

  t_id = 0;
  unit = ghost = NULL;
  activity = NEW TNEURON_VALUE[8];   // <! Every player has his own activity. There should be player_array.GetCount()
  for (j = 0; j < i; j++)
    activity[j] = 0;
  ...
}
```

### Why it matters
- `PL_MAX_PLAYERS` is defined as `8` in `doplayers.h:51`, so the hardcoded `8`
  *happens* to match today. But:
  - The TODO already documents the intent: it should be
    `player_array.GetCount()` (per-game count) or at minimum `PL_MAX_PLAYERS`.
  - Loops that read this array use `player_array.GetCount()` as the upper
    bound (`domap.cpp:419, 438`). If `PL_MAX_PLAYERS` is ever bumped without
    touching this constructor, every map surface silently overflows
    `activity[]` on read **and** write.
  - Allocating 8 `TNEURON_VALUE` per surface tile multiplied across the
    whole map is a non-trivial waste when there are <8 players.

### Fix
Replace the magic constant; allocate per-game:

```cpp
TMAP_SURFACE::TMAP_SURFACE() {
  t_id = 0;
  unit = ghost = NULL;

  const int player_count = PL_MAX_PLAYERS;   // or player_array.GetCount()
  activity = NEW TNEURON_VALUE[player_count];
  for (int j = 0; j < player_count; ++j)
    activity[j] = 0;
  ...
}
```

If switching to dynamic `player_array.GetCount()`, audit every reader to make
sure they re-bound after any code path that adds/removes players mid-game (the
editor in particular).

---

## 5. LOW — `CreateGame()` carries an unresolved `@@FIXME@@`

**File:** `src/doengine.cpp:1541-1547`

```cpp
bool CreateGame()
{
  /*
   * @@FIXME@@
   */
  if (!Disconnect())
    return false;
  ...
}
```

The marker is opaque — the original author didn't say what's wrong. The
suspect line is `if (!Disconnect()) return false;` at the start of *creating*
a game: it forces a teardown of any prior session, but the caller doesn't get
told whether the failure was "no prior game to disconnect" (benign) vs "real
network error" (fatal).

### Action
Either resolve and remove the marker, or rewrite the comment to be specific
(*what* is broken, *what* the caller should expect). Right now it's
permanent low-grade noise that hides whatever the real issue was.

---

## 6. LOW — Bare `new` instead of the project `NEW` macro (allocator bypass)

**Files:**
- `src/dobuildings.cpp:569` — `new TBUILDING_UNIT(this, 0, false)`
- `src/dosources.cpp:319` — `new TSOURCE_UNIT(this, 0, false)`
- `src/donet.cpp:862` — `new TLOCK()`
- `src/glfont.cpp:93` — `new GLFfont`

### Why it matters
The codebase uses a `NEW` macro (defined in `doalloc.h`) so that the custom
memory tracker can record allocations when `DEBUG_MEMORY` is enabled. These
four sites bypass that, so leaks on those types are invisible to
`CheckMemory()`.

### Fix
Replace `new` with `NEW` at those four sites, e.g.:

```cpp
TBUILDING_UNIT *b = NEW TBUILDING_UNIT(this, 0, false);
```

Match the corresponding `delete` with whatever the project convention dictates
(`DELETE` macro if one exists, plain `delete` otherwise).

---

## 7. INFO — `XXX: have_my_address` placeholders in `TFOLLOWER`

**File:** `src/dofollower.h:78-79`

```cpp
in_addr   GetMyAddress () { /* XXX: have_my_address */ return my_address; }
in_port_t GetMyPort   () { /* XXX: have_my_address */ return my_port; }
```

The `XXX` flags that the caller has no way to know whether `my_address` /
`my_port` were ever assigned (no `have_my_address` flag is checked). If a
follower asks for its own address before the first server hello arrives, the
returned value is an uninitialized field — likely zero, but unspecified.

### Action
Add a `bool have_my_address = false;` member, set it on first assignment, and
have the getters return an `optional` / signal failure when not yet set. Or
initialize the fields in the constructor and document that "0.0.0.0:0" means
"not yet known".

---

## Out of scope (worth noting, not bugs)

- **`src/doxygen.log` was committed** — fixed in the recent cleanup, log file removed from tracking and `.gitignore` typo `src/doxygen.log` → `src/.doxygen.log` corrected.
- **`AppGetTimeSeconds()` as a god node** — design observation, not a defect. It is the deliberate single global clock used for both frame pacing (`dodraw.h:262`) and network event timestamps (`doleader.cpp:73,84,98`). The companion `AppSetTimeSeconds()` exists exactly so a follower / headless server can be slewed to the leader's clock. Keep an eye on any new event-emitter to make sure it stamps with `AppGetTimeSeconds()` (or routes through `SendEvent()` which already does).
- **Switch fall-throughs** (`doforces.cpp`, `doworkers.cpp`, `domapunits.cpp`) — verified intentional case-grouping, not missing-`break` bugs.

---

## Suggested order of attack

1. **Fix bug #1 (`Init_receive`)** — small, contained, removes a stack over-read and removes one of the few `FIXME`s with security flavour.
2. **Fix bug #2 (`InitMemorySestem` typo)** — trivial 3-file rename, kills a recurring papercut.
3. **Fix bug #6 (bare `new` → `NEW`)** — 4-line patch, restores allocator hygiene.
4. **Plan bug #3 (`dofile.cpp` strcpy)** — needs an API change; do it as a focused refactor, not a drive-by.
5. **Plan bug #4 (`TMAP_SURFACE` activity[8])** — couple lines, but audit readers first.
6. **Resolve / rewrite bugs #5 and #7** — cleanup pass.

# CPU AI — Variability and Military — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** CPU players with a random personality, difficulty from config and a military state machine (gather → attack with superiority → retreat, proportionate defence, target scoring, retaliation expiry, sensible scouting).

**Architecture:** Pure decision logic in the new `src/doai_logic.{h,cpp}` (no engine, tested by `make test-ai`); `src/doai.{h,cpp}` calls it and converts game state into `TAI_UNIT_SAMPLE`. Config `ai_level` and a per-slot override via `player_array`.

**Tech Stack:** C++ (gnu++17, g++), make, headless `dark-oberon-server` for integration.

**Spec:** `docs/superpowers/specs/2026-09-23-ai-variability-military-design.md`

## Global Constraints

- The AI must not call `rand()`/`srand()`; all randomness goes through `TAI_RNG`.
- `src/doai_logic.{h,cpp}` must not include any engine header (only `<cstdint>`, `<cmath>`, `<cstring>`, `<algorithm>`).
- `ai_level` values `easy|medium|hard`, default `medium`; unknown value → warning + medium.
- Personality presets: `aggressive`, `commercial`, `calm`, `rusher`, `turtle`; noise ±10 %.
- Retaliation expires after 90 s without a hit; defence within 12 fields of our buildings; rally point 8 fields from the base towards the enemy.
- Changes in `doai.cpp`/`doai.h` → update `docs/AI_SYSTEM.md` in the same branch (document rule).
- Commits end with `Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>`; commit only the task's files (do not add uncommitted foreign changes in the working tree — `src/dowalk.h`, `.gitignore`, `logs/full.log`, skill files).
- After changes the client is built with `make` in the root; the server in a copy (`git archive HEAD src` into the scratchpad + `make server`) so that the client's `.o` files are not overwritten.

## Review Focus

1. **No visible or living enemy** (all defeated, or fog) → the state machine must not crash or spam orders; it stays in GATHER / attacks the known base only with 1.5× `rally_size`. Test: `test_should_attack_unknown_enemy_needs_more_units` (Task 2) + integration (Task 6).
2. **Division by zero** in strength ratios (zero army, unarmed enemies) → defined behaviour. Test: `test_ratio_zero_cases` (Task 2).
3. **Map without starting positions (`initial_x < 0`)** → rally point from the first own building. Test: `test_rally_point_without_enemy_base` (Task 2) + fallback in Task 4.
4. **Unknown `ai_level`** in config or with `addcpu` → medium + warning. Test: `test_level_from_name` (Task 1).
5. **Repeated orders every think tick** (group move to the same target over and over) → a move is sent only when the target/state changes. Test: `test_military_order_dedup` (Task 2 — function `TAI_ORDER_MEMO`).

---

## File structure

| File | Responsibility |
|--------|-------------|
| `src/doai_logic.h`, `src/doai_logic.cpp` (new) | RNG, personalities, level from name, strength, decision functions, retaliation, order memory |
| `tests/cpp/test_ai_logic.cpp`, `tests/cpp/Makefile` (new) | unit tests of the pure logic |
| `Makefile` (root) | `test-ai` target |
| `src/Makefile` | `doai_logic.o` in `OBJECTS` + rule |
| `src/doconfig.h`, `src/doconfig.cpp` | `config.ai_level` read/write |
| `src/doplayers.h`, `src/doplayers.cpp` | `ai_level` per slot (`SetAiLevel`/`GetAiLevel`) |
| `src/doengine.cpp` | `addcpu [level]` on the server |
| `src/doai.h`, `src/doai.cpp` | wiring: level/personality, military state machine, scouting, buildings, production, diagnostics |
| `docs/AI_SYSTEM.md` | documentation |

---

### Task 1: `doai_logic` — RNG, personalities, levels + test infrastructure

**Files:**
- Create: `src/doai_logic.h`, `src/doai_logic.cpp`, `tests/cpp/test_ai_logic.cpp`, `tests/cpp/Makefile`
- Modify: `Makefile` (root), `src/Makefile`

**Interfaces — Produces:**
```cpp
struct TAI_FLAVOR_PARAMS;  // moved here from doai.h (same 3 floats)
class TAI_RNG { public: explicit TAI_RNG(uint64_t seed = 1); uint32_t NextU32(); float Uniform(float a, float b);
                bool Chance(float p); int Index(int n); int PickWeighted(const float *w, int n); };
enum TAI_LEVEL_ID { TAI_LV_EASY = 0, TAI_LV_MEDIUM = 1, TAI_LV_HARD = 2 };
TAI_LEVEL_ID TAI_LevelFromName(const char *name, bool *ok);   // NULL/unknown -> MEDIUM, *ok=false
const char *TAI_LevelName(TAI_LEVEL_ID lv);
struct TAI_PERSONALITY { const char *name; TAI_FLAVOR_PARAMS flavor; float attack_ratio; float retreat_ratio;
                         int rally_size; float defense_commit; float scout_count; };
extern const TAI_PERSONALITY TAI_PERSONALITY_PRESETS[5];  // aggressive, commercial, calm, rusher, turtle
TAI_PERSONALITY TAI_RollPersonality(TAI_RNG &rng);
```

- [ ] **Step 1: Test harness + failing tests** (`tests/cpp/test_ai_logic.cpp`)

```cpp
#include "doai_logic.h"
#include <cmath>
#include <cstdio>
#include <cstring>

static int g_fail = 0, g_run = 0;
#define CHECK(c) do { if (!(c)) { std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)
#define TEST(name) static void name(); struct name##_reg { name##_reg() { reg(#name, name); } } name##_inst; static void name()
typedef void (*TestFn)();
static struct { const char *n; TestFn f; } g_tests[128]; static int g_nt = 0;
static void reg(const char *n, TestFn f) { g_tests[g_nt].n = n; g_tests[g_nt].f = f; g_nt++; }

TEST(test_rng_deterministic_for_seed) {
  TAI_RNG a(42), b(42), c(43);
  bool diff = false;
  for (int i = 0; i < 16; i++) { uint32_t x = a.NextU32(); CHECK(x == b.NextU32()); if (x != c.NextU32()) diff = true; }
  CHECK(diff);
}

TEST(test_rng_uniform_and_index_ranges) {
  TAI_RNG r(7);
  for (int i = 0; i < 1000; i++) {
    float u = r.Uniform(2.f, 3.f); CHECK(u >= 2.f && u < 3.f);
    int k = r.Index(5); CHECK(k >= 0 && k < 5);
  }
  CHECK(r.Index(0) == 0);
}

TEST(test_pick_weighted_follows_weights) {
  TAI_RNG r(9);
  float w[3] = {0.f, 1.f, 3.f};
  int cnt[3] = {0, 0, 0};
  for (int i = 0; i < 4000; i++) cnt[r.PickWeighted(w, 3)]++;
  CHECK(cnt[0] == 0);
  CHECK(cnt[2] > cnt[1] * 2);
  float z[2] = {0.f, 0.f};
  CHECK(r.PickWeighted(z, 2) == 0);   // all-zero weights -> first
}

TEST(test_level_from_name) {
  bool ok = true;
  CHECK(TAI_LevelFromName("easy", &ok) == TAI_LV_EASY && ok);
  CHECK(TAI_LevelFromName("HARD", &ok) == TAI_LV_HARD && ok);
  CHECK(TAI_LevelFromName("nightmare", &ok) == TAI_LV_MEDIUM && !ok);
  CHECK(TAI_LevelFromName(NULL, &ok) == TAI_LV_MEDIUM && !ok);
  CHECK(std::strcmp(TAI_LevelName(TAI_LV_HARD), "hard") == 0);
}

TEST(test_personality_presets_names) {
  const char *names[5] = {"aggressive", "commercial", "calm", "rusher", "turtle"};
  for (int i = 0; i < 5; i++) CHECK(std::strcmp(TAI_PERSONALITY_PRESETS[i].name, names[i]) == 0);
}

TEST(test_rolled_personality_within_bounds_and_varied) {
  int seen[5] = {0, 0, 0, 0, 0};
  for (uint64_t s = 1; s <= 200; s++) {
    TAI_RNG r(s);
    TAI_PERSONALITY p = TAI_RollPersonality(r);
    CHECK(p.flavor.aggressivity >= 0.f && p.flavor.aggressivity <= 1.f);
    CHECK(p.flavor.defense_priority >= 0.f && p.flavor.defense_priority <= 1.f);
    CHECK(p.flavor.econ_focus >= 0.f && p.flavor.econ_focus <= 1.f);
    CHECK(p.rally_size >= 2);
    CHECK(p.attack_ratio > p.retreat_ratio);
    for (int i = 0; i < 5; i++) if (std::strcmp(p.name, TAI_PERSONALITY_PRESETS[i].name) == 0) seen[i]++;
  }
  for (int i = 0; i < 5; i++) CHECK(seen[i] > 10);
}

int main() {
  for (int i = 0; i < g_nt; i++) { int before = g_fail; g_tests[i].f(); g_run++;
    std::printf("%s %s\n", g_fail == before ? "ok  " : "FAIL", g_tests[i].n); }
  std::printf("%d tests, %d failures\n", g_run, g_fail);
  return g_fail ? 1 : 0;
}
```

`tests/cpp/Makefile`:
```make
CXX ?= g++
SRC = ../../src
test: test_ai_logic
	./test_ai_logic
test_ai_logic: test_ai_logic.cpp $(SRC)/doai_logic.cpp $(SRC)/doai_logic.h
	$(CXX) -std=gnu++17 -Wall -Wextra -O1 -I$(SRC) -o $@ test_ai_logic.cpp $(SRC)/doai_logic.cpp
clean:
	rm -f test_ai_logic
.PHONY: test clean
```
Root `Makefile`: add
```make
test-ai:
	$(MAKE) -C tests/cpp
```

- [ ] **Step 2: Run — expect FAIL** — `make test-ai` → compilation error (`doai_logic.h` does not exist).

- [ ] **Step 3: Implement** `src/doai_logic.h` (with `TAI_FLAVOR_PARAMS` moved from `doai.h` — in `doai.h` replace the definition with `#include "doai_logic.h"`) and `src/doai_logic.cpp`:

```cpp
// PCG32 (O'Neill), inc fixed odd constant
TAI_RNG::TAI_RNG(uint64_t seed) : state(0), inc(1442695040888963407ULL) { NextU32(); state += seed; NextU32(); }
uint32_t TAI_RNG::NextU32() {
  uint64_t old = state; state = old * 6364136223846793005ULL + inc;
  uint32_t xs = (uint32_t)(((old >> 18u) ^ old) >> 27u); uint32_t rot = (uint32_t)(old >> 59u);
  return (xs >> rot) | (xs << ((-rot) & 31));
}
float TAI_RNG::Uniform(float a, float b) { return a + (b - a) * (float)(NextU32() >> 8) * (1.0f / 16777216.0f); }
bool TAI_RNG::Chance(float p) { return Uniform(0.f, 1.f) < p; }
int TAI_RNG::Index(int n) { return n <= 0 ? 0 : (int)(NextU32() % (uint32_t)n); }
int TAI_RNG::PickWeighted(const float *w, int n) {
  float tot = 0; for (int i = 0; i < n; i++) if (w[i] > 0) tot += w[i];
  if (tot <= 0) return 0;
  float x = Uniform(0.f, tot);
  for (int i = 0; i < n; i++) { if (w[i] <= 0) continue; if (x < w[i]) return i; x -= w[i]; }
  for (int i = n - 1; i >= 0; i--) if (w[i] > 0) return i;
  return 0;
}
```
Presets (`name, {agg, def, eco}, attack_ratio, retreat_ratio, rally_size, defense_commit, scout_count`):
```cpp
const TAI_PERSONALITY TAI_PERSONALITY_PRESETS[5] = {
  {"aggressive", {0.90f, 0.20f, 0.30f}, 1.15f, 0.60f, 6, 1.3f, 1.5f},
  {"commercial", {0.20f, 0.60f, 0.90f}, 1.80f, 0.90f, 10, 1.8f, 1.0f},
  {"calm",       {0.50f, 0.50f, 0.50f}, 1.40f, 0.75f, 8, 1.5f, 1.0f},
  {"rusher",     {1.00f, 0.05f, 0.25f}, 1.00f, 0.50f, 4, 1.2f, 2.0f},
  {"turtle",     {0.40f, 0.95f, 0.60f}, 2.00f, 1.00f, 14, 2.0f, 1.0f},
};
```
`TAI_RollPersonality`: `p = PRESETS[rng.Index(5)]`; each flavor float `*= Uniform(0.9,1.1)` and clamp 0..1; `attack_ratio *= Uniform(0.9,1.1)`; `retreat_ratio *= Uniform(0.9,1.1)` and then `retreat_ratio = min(retreat_ratio, attack_ratio*0.9f)`; `rally_size += Index(3)-1`, min 2; `defense_commit *= Uniform(0.9,1.1)`.
`TAI_LevelFromName`: case-insensitive `easy|medium|hard` (own `tolower` loop), otherwise MEDIUM + `*ok=false` (ok may be NULL).

In `src/Makefile`: `doai_logic.o` into `OBJECTS` and the rule
```make
doai_logic.o: doai_logic.cpp doai_logic.h
	$(CPP) -c doai_logic.cpp
```
and `doai_logic.h` into the dependencies of `doai.o`, `doengine.o`, `doplayers.o`.

- [ ] **Step 4: Run — expect PASS** — `make test-ai` → `6 tests, 0 failures`; `make` (client) succeeds.
- [ ] **Step 5: Commit** `feat(ai): pure decision module with RNG, personalities and levels`

---

### Task 2: `doai_logic` — strength, decisions, targets, retaliation, order memory

**Files:** Modify `src/doai_logic.{h,cpp}`, `tests/cpp/test_ai_logic.cpp`

**Interfaces — Produces:**
```cpp
struct TAI_UNIT_SAMPLE { float life, max_life, dps; int x, y; bool structure, military, attacking_us; };
float TAI_ArmyPower(const TAI_UNIT_SAMPLE *u, int n);                 // (Σdps)·(Σlife)
float TAI_PowerRatio(float mine, float theirs);                        // theirs<=0 -> 1e9 (mine>0) / 0 (mine<=0)
bool  TAI_ShouldAttack(float my_power, float enemy_est, int my_count, const TAI_PERSONALITY &p, bool enemy_known);
bool  TAI_ShouldRetreat(float my_power, float enemy_local, const TAI_PERSONALITY &p);
int   TAI_DefenseCommitCount(float threat_power, const float *unit_power_by_distance, int n, float commit_factor);
float TAI_TargetScore(const TAI_UNIT_SAMPLE &t, float dist);
float TAI_EnemyPowerEstimate(float visible, float remembered, float seconds_since_seen);
void  TAI_RallyPoint(int bx, int by, int ex, int ey, int dist, int map_w, int map_h, int *ox, int *oy);
class TAI_RETALIATION { public: void Hit(int pid, double now); bool Active(double now) const; int Target() const;
                        void Clear(); static const double kExpire; /* 90.0 */ };
struct TAI_ORDER_MEMO { int kind, target, x, y; bool Changed(int kind, int target, int x, int y); void Reset(); };
```

- [ ] **Step 1: Failing tests** (add to `test_ai_logic.cpp`):
```cpp
static TAI_UNIT_SAMPLE S(float life, float dps, int x = 0, int y = 0, bool st = false, bool mil = true, bool atk = false) {
  TAI_UNIT_SAMPLE s; s.life = life; s.max_life = life; s.dps = dps; s.x = x; s.y = y; s.structure = st; s.military = mil; s.attacking_us = atk; return s;
}
TEST(test_army_power_lanchester) {
  TAI_UNIT_SAMPLE a[2] = {S(100, 10), S(100, 10)};
  CHECK(std::fabs(TAI_ArmyPower(a, 2) - 20.f * 200.f) < 1e-3f);
  CHECK(TAI_ArmyPower(a, 0) == 0.f);
  TAI_UNIT_SAMPLE w[1] = {S(50, 0)};            // worker: no dps
  CHECK(TAI_ArmyPower(w, 1) == 0.f);
}
TEST(test_ratio_zero_cases) {
  CHECK(TAI_PowerRatio(10.f, 0.f) > 1e8f);
  CHECK(TAI_PowerRatio(0.f, 0.f) == 0.f);
  CHECK(TAI_PowerRatio(0.f, 5.f) == 0.f);
  CHECK(std::fabs(TAI_PowerRatio(6.f, 3.f) - 2.f) < 1e-6f);
}
TEST(test_should_attack_thresholds) {
  TAI_PERSONALITY p = TAI_PERSONALITY_PRESETS[2];   // calm: ratio 1.4, rally 8
  CHECK(!TAI_ShouldAttack(1000.f, 100.f, 7, p, true));   // not enough units
  CHECK(TAI_ShouldAttack(141.f, 100.f, 8, p, true));
  CHECK(!TAI_ShouldAttack(139.f, 100.f, 8, p, true));
}
TEST(test_should_attack_unknown_enemy_needs_more_units) {
  TAI_PERSONALITY p = TAI_PERSONALITY_PRESETS[2];   // rally 8 -> needs 12
  CHECK(!TAI_ShouldAttack(1.f, 0.f, 11, p, false));
  CHECK(TAI_ShouldAttack(1.f, 0.f, 12, p, false));
}
TEST(test_should_retreat) {
  TAI_PERSONALITY p = TAI_PERSONALITY_PRESETS[2];   // retreat 0.75
  CHECK(TAI_ShouldRetreat(70.f, 100.f, p));
  CHECK(!TAI_ShouldRetreat(80.f, 100.f, p));
  CHECK(!TAI_ShouldRetreat(10.f, 0.f, p));
}
TEST(test_defense_commit_count) {
  float pw[4] = {50.f, 50.f, 50.f, 50.f};
  CHECK(TAI_DefenseCommitCount(60.f, pw, 4, 1.5f) == 2);   // need 90
  CHECK(TAI_DefenseCommitCount(1000.f, pw, 4, 1.5f) == 4); // everything
  CHECK(TAI_DefenseCommitCount(0.f, pw, 4, 1.5f) == 1);    // min 1
  CHECK(TAI_DefenseCommitCount(60.f, pw, 0, 1.5f) == 0);
}
TEST(test_target_score_order) {
  float attacker = TAI_TargetScore(S(100, 5, 0, 0, false, true, true), 10.f);
  float soldier  = TAI_TargetScore(S(100, 5), 10.f);
  float tower    = TAI_TargetScore(S(100, 5, 0, 0, true, false), 10.f);
  float farm     = TAI_TargetScore(S(100, 0, 0, 0, true, false), 10.f);
  CHECK(attacker > soldier && soldier > tower && tower > farm);
  CHECK(TAI_TargetScore(S(100, 5), 2.f) > TAI_TargetScore(S(100, 5), 20.f));
  TAI_UNIT_SAMPLE hurt = S(100, 5); hurt.life = 20.f;
  CHECK(TAI_TargetScore(hurt, 10.f) > soldier);
}
TEST(test_enemy_power_estimate_decay) {
  CHECK(TAI_EnemyPowerEstimate(50.f, 100.f, 0.f) == 100.f);
  CHECK(std::fabs(TAI_EnemyPowerEstimate(0.f, 100.f, 60.f) - 50.f) < 1e-3f);
  CHECK(TAI_EnemyPowerEstimate(80.f, 100.f, 120.f) == 80.f);
}
TEST(test_rally_point_clamped) {
  int x, y;
  TAI_RallyPoint(10, 10, 50, 10, 8, 80, 80, &x, &y); CHECK(x == 18 && y == 10);
  TAI_RallyPoint(2, 2, -40, -40, 8, 80, 80, &x, &y); CHECK(x >= 1 && y >= 1);
}
TEST(test_rally_point_without_enemy_base) {
  int x, y;
  TAI_RallyPoint(10, 10, -1, -1, 8, 80, 80, &x, &y);   // unknown enemy -> stay at base
  CHECK(x == 10 && y == 10);
}
TEST(test_retaliation_expires) {
  TAI_RETALIATION r;
  CHECK(!r.Active(0.0));
  r.Hit(3, 10.0); CHECK(r.Active(50.0) && r.Target() == 3);
  CHECK(!r.Active(10.0 + TAI_RETALIATION::kExpire + 0.1));
  r.Hit(2, 200.0); CHECK(r.Target() == 2);
  r.Clear(); CHECK(!r.Active(200.0));
}
TEST(test_military_order_dedup) {
  TAI_ORDER_MEMO m; m.Reset();
  CHECK(m.Changed(1, 5, 10, 10));
  CHECK(!m.Changed(1, 5, 10, 10));
  CHECK(m.Changed(1, 6, 10, 10));
  CHECK(m.Changed(2, 6, 10, 10));
}
```
- [ ] **Step 2: Run — expect FAIL** (`make test-ai`: undefined symbols).
- [ ] **Step 3: Implement**:
  - `TAI_ArmyPower`: `sdps += max(0,dps)`, `slife += max(0,life)`; return `sdps*slife`.
  - `TAI_PowerRatio`: as in the interface.
  - `TAI_ShouldAttack`: `if (my_count < p.rally_size) return false; if (!enemy_known) return my_count >= (p.rally_size*3+1)/2; return TAI_PowerRatio(my_power, enemy_est) >= p.attack_ratio;`
  - `TAI_ShouldRetreat`: `enemy_local > 0 && TAI_PowerRatio(my_power, enemy_local) < p.retreat_ratio`.
  - `TAI_DefenseCommitCount`: `if n<=0 return 0; need = commit_factor*threat_power; sum=0; for i: sum+=pw[i]; if (sum>=need) return max(1,i+1); return n;` (with `threat_power<=0` → 1).
  - `TAI_TargetScore`: `s = 0; if attacking_us s+=100; if military s+=40; else if structure && dps>0 s+=30; else if structure s+=10; s -= 2*dist; if max_life>0 s += (1 - life/max_life)*20; return s;` (with "soldier" without structure: military=true).
  - `TAI_EnemyPowerEstimate`: `max(visible, remembered*pow(0.5, t/60))`.
  - `TAI_RallyPoint`: if `ex<0||ey<0` → `(bx,by)`; otherwise vector `(ex-bx, ey-by)`, length L; if L<1 → base; `x = bx + round(dx/L*dist)`, same for y; clamp `[1, map_w-2]`, `[1, map_h-2]`.
  - `TAI_RETALIATION`: `pid=-1, last=-1e9`; `Hit`: `pid=p,last=now`; `Active`: `pid>=0 && now-last <= kExpire`; `kExpire=90.0`.
  - `TAI_ORDER_MEMO::Changed`: compares the 4 values, stores the new ones, returns whether they differed; `Reset` sets `kind=-1`.
- [ ] **Step 4: Run — expect PASS** (`make test-ai` → 17 tests, 0 failures).
- [ ] **Step 5: Commit** `feat(ai): power estimate, attack/retreat/defense decisions, target scoring`

---

### Task 3: Difficulty from config + `addcpu <level>` + personality for each CPU

**Files:** Modify `src/doconfig.{h,cpp}`, `src/doplayers.{h,cpp}`, `src/doengine.cpp`, `src/doai.{h,cpp}`

**Interfaces:**
- Consumes: `TAI_LevelFromName`, `TAI_RollPersonality`, `TAI_RNG` (Task 1).
- Produces: `config.ai_level` (`int`, `TAI_LEVEL_ID`); `TPLAYER_ARRAY::SetAiLevel(int idx, int lv)`, `int GetAiLevel(int idx)` (−1 = default); `TAI_PLAYER` with lazy init in the first `UpdateAI` (`player_id` is only known after `SetPlayerID`): `TAI_LEVEL *TAI_CreateLevel(TAI_LEVEL_ID)`; `TAI_CONTROLLER` holds `TAI_PERSONALITY personality; TAI_RNG rng;` and a getter `const TAI_PERSONALITY &GetPersonality() const`.

Steps:
- [ ] **Step 1: Test** — the engine-free testable logic extension is done in Task 1 (`test_level_from_name`); here verify by integration: write a script `tests/cpp/ai_smoke.sh` (headless: build the server in a copy, `addcpu hard`, `addcpu easy`, `start`, `logs 1`, `logs 2`, `quit`) and expect `level=hard`, `level=easy` and `personality=` for both slots in the output. Run it — FAIL (the text is not in `logs` yet).
- [ ] **Step 2: Implement config** — `doconfig.h`: `int ai_level;` + `#define CFG_DEF_AI_LEVEL "medium"`; `doconfig.cpp` default `ai_level = TAI_LV_MEDIUM`; reading `TFILE_LINE v; config.file->ReadStr(v, "ai_level", CFG_DEF_AI_LEVEL, true); bool ok; config.ai_level = TAI_LevelFromName(v, &ok); if (!ok) Warning(LogMsg("Unknown ai_level '%s', using medium", v));`; writing `config.file->WriteLine("# *** Computer players ***"); config.file->WriteStr("ai_level", CFG_DEF_AI_LEVEL);`.
- [ ] **Step 3: Implement per-slot level** — `TPLAYER_ARRAY::TPLAYER` + `int ai_level;` (`= -1` in `AddPlayer`), `SetAiLevel/GetAiLevel` (range check). `doengine.cpp` `addcpu`: after `AddComputerPlayer()` parse the optional argument `buf+6` (skip spaces), `if (*arg) { bool ok; TAI_LEVEL_ID lv = TAI_LevelFromName(arg,&ok); if (!ok) Warning(...); player_array.SetAiLevel(idx, lv); }`; output `addcpu: players=%d level=%s`. Add `addcpu [easy|medium|hard]` to the `Commands:` help.
- [ ] **Step 4: Implement TAI_PLAYER** — the constructor only does `SetPlayerType(PT_COMPUTER)`; `UpdateAI`: if `!controller` → `EnsureController()`: `int slot = GetPlayerID(); int lv = player_array.GetAiLevel(slot); if (lv < 0) lv = config.ai_level; owned_level = TAI_CreateLevel((TAI_LEVEL_ID)lv); TAI_RNG rng((uint64_t)time(NULL) ^ ((uint64_t)slot * 0x9E3779B97F4A7C15ULL) ^ (uint64_t)(uintptr_t)this); TAI_PERSONALITY pers = TAI_RollPersonality(rng); owned_strategy = NEW TAI_STRATEGY(pers.flavor); controller = NEW TAI_CONTROLLER(this, owned_level, owned_strategy, pers, rng.NextU32());`. `DumpAIDiagnostics`/`EmitAIDiagnosticLines` also call `EnsureController()`. `TAI_STRATEGY::GeneratePhases`: `phases[3].targets.attack_when_ready = params.aggressivity > 0.3f` stays, but add for `rusher` (aggressivity ≥ 0.95): `phases[2].targets.attack_when_ready = true` and `combat_phase = 2`. Info log on creation: `Info(LogMsg("CPU %d: level=%s personality=%s", slot, TAI_LevelName(lv), pers.name));`.
- [ ] **Step 5: Diagnostics** — `EmitDiagnosticLines`: line `AI: level=%s personality=%s attack_ratio=%.2f retreat_ratio=%.2f rally=%d` (keep the level name in the controller).
- [ ] **Step 6: Run** `bash tests/cpp/ai_smoke.sh` → PASS; `make test-ai` PASS; `make` client PASS.
- [ ] **Step 7: Commit** `feat(ai): per-CPU random personality, ai_level config and addcpu <level>`

---

### Task 4: Military state machine, defence, retaliation, targets

**Files:** Modify `src/doai.{h,cpp}`

**Interfaces — Consumes:** Task 2 functions. **Produces:** `enum TAI_MIL_STATE { MIL_GATHER, MIL_ATTACK, MIL_RETREAT };` in the controller: `TAI_MIL_STATE mil_state; double game_time; double mil_state_since; TAI_RETALIATION retaliation; TAI_ORDER_MEMO army_order; float remembered_enemy_power; double enemy_seen_at; int scout_ids[2]; int n_scouts;`; methods `void ManageArmy()`, `bool ManageDefense(int *committed_ids, int *n_committed)`, `TAI_UNIT_SAMPLE SampleUnit(TMAP_UNIT *u)`, `int ChooseEnemyPlayer()`, `void OrderGroup(TFORCE_UNIT **f, int n, int x, int y, TMAP_UNIT *attack_target)`.

Steps:
- [ ] **Step 1: Sampling** — `SampleUnit`: `TMAP_ITEM *it = (TMAP_ITEM*)u->GetPointerToItem(); life = ((TBASIC_UNIT*)u)->GetLife(); max_life = it->GetMaxLife(); dps = 0; TARMAMENT *ar = it->GetArmament(); if (ar && ar->GetOffensive()) { TGUN_POWER pw = ar->GetOffensive()->GetPower(); dps = 0.5f*(pw.min+pw.max); }`; `structure = IT_BUILDING||IT_FACTORY`; `military = IT_FORCE && !IT_WORKER`; `attacking_us = target && target->GetPlayerID()==my_id`.
- [ ] **Step 2: One map pass per tick** — replace the repeated `Find*` scans with a `ScanEnemies()` function filling an array (max 256) of visible enemy `TMAP_UNIT*` + their `TAI_UNIT_SAMPLE`; from these: threats near the base (Chebyshev ≤ 12 from any of our buildings), `visible_enemy_power`, attackers (`attacking_us`) → `retaliation.Hit(pid, game_time)`.
- [ ] **Step 3: Defence** (`ManageDefense`) — if threats exist: `threat_power = TAI_ArmyPower(threats)`; sort our combat units (excluding scouts) by distance to the best-scored threat (`TAI_TargetScore`), `k = TAI_DefenseCommitCount(threat_power, powers, n, personality.defense_commit)`, the first `k` get `StartAttacking(target)`; store their IDs (they are not used in `ManageArmy` this tick).
- [ ] **Step 4: State machine** (`ManageArmy`, remaining units):
  - `enemy_pid = retaliation.Active(game_time) ? retaliation.Target() : ChooseEnemyPlayer()` (nearest active player ≠ us, ≠ 0 by `initial_x/y`); if none → nothing.
  - rally = `TAI_RallyPoint(base, enemy_start, 8, map.width, map.height)`; base = `initial_x/y`, or the first own building.
  - `est = TAI_EnemyPowerEstimate(visible_enemy_power, remembered_enemy_power, game_time - enemy_seen_at)`; if `visible_enemy_power > 0` → `remembered = max(remembered*0.9, visible)`, `enemy_seen_at = game_time`.
  - GATHER: units farther than 4 fields from the rally point → `OrderGroup(... rally ...)` (dedup via `army_order.Changed(MIL_GATHER, -1, rx, ry)`); transition to ATTACK when `(phase.attack_when_ready || retaliation.Active(game_time)) && TAI_ShouldAttack(my_power, est, n, personality, est > 0)`.
  - ATTACK: target = best-scored visible enemy within ≤ 20 of the army's centroid, otherwise the enemy start; `OrderGroup(..., target)` (dedup by target ID / position); `local = TAI_ArmyPower(enemies within 10 fields of the centroid)`; `TAI_ShouldRetreat(my_power, local, personality)` → RETREAT; `n < max(2, rally_size/2)` → GATHER.
  - RETREAT: `OrderGroup(rally)`; after 20 s or when ≥ 70 % of the army is within 5 fields of the rally point → GATHER.
  - Log transitions to stderr with `logs on`: `Player %d military %s -> %s (my=%.0f enemy=%.0f n=%d)`.
- [ ] **Step 5: `OrderGroup`** — fix #9: if `n >= 2` and `tai_request_group_move_forces` succeeds, **do not call** individual `StartMoving`; otherwise (n==1 or failure) individual `StartMoving`/`StartAttacking`. If `attack_target` and the unit is ≤ `kTaiAssaultReleaseAttackDist` from the target → `StartAttacking`. Delete `TaiSendArmyTowardPosition` (replaced).
- [ ] **Step 6: Wiring in `Think()`** — replace the whole block from "Assault: any visible enemy…" (currently lines ~2051–2101) with: `game_time += interval` (accumulated time), `ScanEnemies(); ManageDefense(...); ManageArmy();`. Remove `retaliate_enemy_pid`, `retaliate_last_path_*`, `assault_group_path_target_id` and `FindThreateningVisibleEnemyForPlayer`/`FindVisibleEnemyStructureNearestTheirStart`/`FindVisibleEnemyCombatUnitOfPlayer` if unused. The `enemy_contacted` phase bump stays (uses the scan).
- [ ] **Step 7: Diagnostics** — `Military: state=%s since=%.0fs my=%.0f enemy_est=%.0f retaliation=%s(pid %d)`.
- [ ] **Step 8: Build + integration** — `make` client; server in a copy; extend `tests/cpp/ai_smoke.sh` with `logs on` and a 300 s run (`sleep 300` before `quit`) on `trial` with 2 CPUs: expect `military GATHER -> ATTACK` in the log at least once and no `Err:`/crash. If nobody attacks within 300 s, increase the time to 600 s and check `logs <slot>` (ruling into the ledger).
- [ ] **Step 9: Commit** `feat(ai): military state machine with rally, power-based attack/retreat and proportional defense`

---

### Task 5: Variability — scouting, buildings, production

**Files:** Modify `src/doai.{h,cpp}`

- [ ] **Step 1: Scouting** — rewrite `ManageScouting`: `want = clamp(round(personality.scout_count), 1, 2)`, only if `force_count >= want + 2` (scouting does not drain a small army); maintain `scout_ids[]` (IDs of living units; drop dead ones), refill from the lightest combat units; each scout that is idle (`UA_STAY`) gets a new target: with probability 0.5 a random enemy's start (`players[pid]->initial_x/y` ± `rng.Index(7)-3`), otherwise a random map point (`rng.Index(map.width-4)+2`). Scouts are excluded from defence/army (Task 4 filters by `scout_ids`). Delete the `for (si < scouts)` loop in `Think()` and `scout_phase`.
- [ ] **Step 2: Buildings** — `FindBuildPosition`: at the start `int orient = rng.Index(4)` (rotating the dx/dy order: `(dx,dy)`, `(-dx,dy)`, `(dx,-dy)`, `(-dx,-dy)`), and instead of returning the first valid spot collect up to 3 candidates within the same pass (`pi`) and up to radius `first_radius + 2`; return `cands[rng.Index(nc)]`.
- [ ] **Step 3: Production** — in `ManageFactories(BG_FORCE)` replace the first "heavy-prefer" sort with a weighted choice: for candidates without a blocker (`TAI_PredictProduceBlocker < 0`) weight `w = pow(score / max_score, 0.5f + personality.flavor.aggressivity)`, if `heavy_only` only heavy ones; `j = rng.PickWeighted(w, nc)`. The "affordable-light" and "force-queue" fallbacks stay.
- [ ] **Step 4: Integration** — 3 runs of `ai_smoke.sh` (300 s, trial, 2 CPUs): different `personality=` in the log and (`logs think on` for 60 s is enough) different `StartBuild ... ok` positions / order; no `Err:`.
- [ ] **Step 5: Commit** `feat(ai): dedicated scouts, randomized build sites and weighted unit mix`

---

### Task 6: Documentation + final verification

**Files:** Modify `docs/AI_SYSTEM.md`

- [ ] **Step 1:** Add sections: personalities (preset table + noise), `ai_level` + `addcpu [level]`, military state machine (GATHER/ATTACK/RETREAT diagram + defence), retaliation with expiry, scouting, `doai_logic` + `make test-ai`, new `logs` lines. Correct the claims about "Default build Easy + Aggressive" and about Commercial.
- [ ] **Step 2:** `make test-ai`, `make` (client), server build in a copy, `ai_smoke.sh` 300 s on both `trial` and `sunnybay` → no errors, ATTACK reached.
- [ ] **Step 3: Commit** `docs(ai): personalities, levels, military state machine`

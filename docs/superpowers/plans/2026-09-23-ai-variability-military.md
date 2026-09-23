# CPU AI — variabilita a vojsko — implementační plán

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** CPU hráči s náhodnou osobností, obtížností z configu a vojenským automatem (shromáždění → útok s převahou → ústup, přiměřená obrana, skórování cílů, vypršení odvety, rozumný průzkum).

**Architecture:** Čistá rozhodovací logika v novém `src/doai_logic.{h,cpp}` (bez enginu, testovaná `make test-ai`); `src/doai.{h,cpp}` ji volá a převádí herní stav na `TAI_UNIT_SAMPLE`. Config `ai_level` a per-slot override přes `player_array`.

**Tech Stack:** C++ (gnu++17, g++), make, headless `dark-oberon-server` pro integraci.

**Spec:** `docs/superpowers/specs/2026-09-23-ai-variability-military-design.md`

## Global Constraints

- AI nesmí volat `rand()`/`srand()`; veškerá náhoda přes `TAI_RNG`.
- `src/doai_logic.{h,cpp}` nesmí includovat žádný engine header (jen `<cstdint>`, `<cmath>`, `<cstring>`, `<algorithm>`).
- `ai_level` hodnoty `easy|medium|hard`, výchozí `medium`; neznámá hodnota → warning + medium.
- Presety osobností: `aggressive`, `commercial`, `calm`, `rusher`, `turtle`; šum ±10 %.
- Odveta vyprší po 90 s bez zásahu; obrana v okruhu 12 polí od našich staveb; shromaždiště 8 polí od základny směrem k nepříteli.
- Změny v `doai.cpp`/`doai.h` → aktualizovat `docs/AI_SYSTEM.md` ve stejné větvi (pravidlo dokumentu).
- Commity končí `Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>`; commitovat jen soubory tasku (necommitnuté cizí změny v pracovním stromu — `src/dowalk.h`, `.gitignore`, `logs/full.log`, skill soubory — nepřidávat).
- Klient se po změnách staví `make` v kořeni; server v kopii (`git archive HEAD src` do scratchpadu + `make server`), aby se nepřepsaly `.o` klienta.

## Review Focus

1. **Žádný viditelný ani živý nepřítel** (všichni poraženi, nebo mlha) → automat nesmí padat ani spamovat příkazy; zůstává GATHER/útočí na známou základnu jen s 1,5× `rally_size`. Test: `test_should_attack_unknown_enemy_needs_more_units` (Task 2) + integrace (Task 6).
2. **Dělení nulou** v poměrech síly (nulová armáda, neozbrojení nepřátelé) → definované chování. Test: `test_ratio_zero_cases` (Task 2).
3. **Mapa bez startovních pozic (`initial_x < 0`)** → shromaždiště z první vlastní stavby. Test: `test_rally_point_without_enemy_base` (Task 2) + fallback v Task 4.
4. **Neznámé `ai_level`** v configu nebo u `addcpu` → medium + warning. Test: `test_level_from_name` (Task 1).
5. **Opakované příkazy každý think tick** (skupinový přesun znovu a znovu ke stejnému cíli) → přesun se posílá jen při změně cíle/stavu. Test: `test_military_order_dedup` (Task 2 — funkce `TAI_ORDER_MEMO`).

---

## Struktura souborů

| Soubor | Odpovědnost |
|--------|-------------|
| `src/doai_logic.h`, `src/doai_logic.cpp` (nové) | RNG, osobnosti, level z názvu, síla, rozhodovací funkce, odveta, paměť příkazů |
| `tests/cpp/test_ai_logic.cpp`, `tests/cpp/Makefile` (nové) | unit testy čisté logiky |
| `Makefile` (kořen) | cíl `test-ai` |
| `src/Makefile` | `doai_logic.o` v `OBJECTS` + pravidlo |
| `src/doconfig.h`, `src/doconfig.cpp` | `config.ai_level` čtení/zápis |
| `src/doplayers.h`, `src/doplayers.cpp` | `ai_level` na slot (`SetAiLevel`/`GetAiLevel`) |
| `src/doengine.cpp` | `addcpu [level]` na serveru |
| `src/doai.h`, `src/doai.cpp` | napojení: level/osobnost, vojenský automat, průzkum, stavby, výroba, diagnostika |
| `docs/AI_SYSTEM.md` | dokumentace |

---

### Task 1: `doai_logic` — RNG, osobnosti, levely + testovací infrastruktura

**Files:**
- Create: `src/doai_logic.h`, `src/doai_logic.cpp`, `tests/cpp/test_ai_logic.cpp`, `tests/cpp/Makefile`
- Modify: `Makefile` (kořen), `src/Makefile`

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
Kořenový `Makefile`: přidat
```make
test-ai:
	$(MAKE) -C tests/cpp
```

- [ ] **Step 2: Run — expect FAIL** — `make test-ai` → chyba kompilace (`doai_logic.h` neexistuje).

- [ ] **Step 3: Implement** `src/doai_logic.h` (s `TAI_FLAVOR_PARAMS` přesunutým z `doai.h` — v `doai.h` nahradit definici `#include "doai_logic.h"`) a `src/doai_logic.cpp`:

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
Presety (`name, {agg, def, eco}, attack_ratio, retreat_ratio, rally_size, defense_commit, scout_count`):
```cpp
const TAI_PERSONALITY TAI_PERSONALITY_PRESETS[5] = {
  {"aggressive", {0.90f, 0.20f, 0.30f}, 1.15f, 0.60f, 6, 1.3f, 1.5f},
  {"commercial", {0.20f, 0.60f, 0.90f}, 1.80f, 0.90f, 10, 1.8f, 1.0f},
  {"calm",       {0.50f, 0.50f, 0.50f}, 1.40f, 0.75f, 8, 1.5f, 1.0f},
  {"rusher",     {1.00f, 0.05f, 0.25f}, 1.00f, 0.50f, 4, 1.2f, 2.0f},
  {"turtle",     {0.40f, 0.95f, 0.60f}, 2.00f, 1.00f, 14, 2.0f, 1.0f},
};
```
`TAI_RollPersonality`: `p = PRESETS[rng.Index(5)]`; každý flavor float `*= Uniform(0.9,1.1)` a clamp 0..1; `attack_ratio *= Uniform(0.9,1.1)`; `retreat_ratio *= Uniform(0.9,1.1)` a pak `retreat_ratio = min(retreat_ratio, attack_ratio*0.9f)`; `rally_size += Index(3)-1`, min 2; `defense_commit *= Uniform(0.9,1.1)`.
`TAI_LevelFromName`: case-insensitive `easy|medium|hard` (vlastní `tolower` smyčka), jinak MEDIUM + `*ok=false` (ok může být NULL).

Do `src/Makefile`: `doai_logic.o` do `OBJECTS` a pravidlo
```make
doai_logic.o: doai_logic.cpp doai_logic.h
	$(CPP) -c doai_logic.cpp
```
a `doai_logic.h` do závislostí `doai.o`, `doengine.o`, `doplayers.o`.

- [ ] **Step 4: Run — expect PASS** — `make test-ai` → `6 tests, 0 failures`; `make` (klient) projde.
- [ ] **Step 5: Commit** `feat(ai): pure decision module with RNG, personalities and levels`

---

### Task 2: `doai_logic` — síla, rozhodnutí, cíle, odveta, paměť příkazů

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

- [ ] **Step 1: Failing tests** (přidat do `test_ai_logic.cpp`):
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
  - `TAI_PowerRatio`: jak v interfaci.
  - `TAI_ShouldAttack`: `if (my_count < p.rally_size) return false; if (!enemy_known) return my_count >= (p.rally_size*3+1)/2; return TAI_PowerRatio(my_power, enemy_est) >= p.attack_ratio;`
  - `TAI_ShouldRetreat`: `enemy_local > 0 && TAI_PowerRatio(my_power, enemy_local) < p.retreat_ratio`.
  - `TAI_DefenseCommitCount`: `if n<=0 return 0; need = commit_factor*threat_power; sum=0; for i: sum+=pw[i]; if (sum>=need) return max(1,i+1); return n;` (s `threat_power<=0` → 1).
  - `TAI_TargetScore`: `s = 0; if attacking_us s+=100; if military s+=40; else if structure && dps>0 s+=30; else if structure s+=10; s -= 2*dist; if max_life>0 s += (1 - life/max_life)*20; return s;` (s „soldier“ bez structure: military=true).
  - `TAI_EnemyPowerEstimate`: `max(visible, remembered*pow(0.5, t/60))`.
  - `TAI_RallyPoint`: pokud `ex<0||ey<0` → `(bx,by)`; jinak vektor `(ex-bx, ey-by)`, délka L; pokud L<1 → base; `x = bx + round(dx/L*dist)`, totéž y; clamp `[1, map_w-2]`, `[1, map_h-2]`.
  - `TAI_RETALIATION`: `pid=-1, last=-1e9`; `Hit`: `pid=p,last=now`; `Active`: `pid>=0 && now-last <= kExpire`; `kExpire=90.0`.
  - `TAI_ORDER_MEMO::Changed`: porovná 4 hodnoty, uloží nové, vrátí zda se lišily; `Reset` nastaví `kind=-1`.
- [ ] **Step 4: Run — expect PASS** (`make test-ai` → 17 tests, 0 failures).
- [ ] **Step 5: Commit** `feat(ai): power estimate, attack/retreat/defense decisions, target scoring`

---

### Task 3: Obtížnost z configu + `addcpu <level>` + osobnost pro každého CPU

**Files:** Modify `src/doconfig.{h,cpp}`, `src/doplayers.{h,cpp}`, `src/doengine.cpp`, `src/doai.{h,cpp}`

**Interfaces:**
- Consumes: `TAI_LevelFromName`, `TAI_RollPersonality`, `TAI_RNG` (Task 1).
- Produces: `config.ai_level` (`int`, `TAI_LEVEL_ID`); `TPLAYER_ARRAY::SetAiLevel(int idx, int lv)`, `int GetAiLevel(int idx)` (−1 = výchozí); `TAI_PLAYER` s lazy init v prvním `UpdateAI` (`player_id` je známé až po `SetPlayerID`): `TAI_LEVEL *TAI_CreateLevel(TAI_LEVEL_ID)`; `TAI_CONTROLLER` drží `TAI_PERSONALITY personality; TAI_RNG rng;` a getter `const TAI_PERSONALITY &GetPersonality() const`.

Kroky:
- [ ] **Step 1: Test** — rozšíření logiky testovatelné bez enginu je hotové v Task 1 (`test_level_from_name`); tady ověření integrací: napiš skript `tests/cpp/ai_smoke.sh` (headless: build serveru v kopii, `addcpu hard`, `addcpu easy`, `start`, `logs 1`, `logs 2`, `quit`) a očekávej ve výstupu `level=hard`, `level=easy` a `personality=` u obou slotů. Spusť — FAIL (text v `logs` zatím není).
- [ ] **Step 2: Implement config** — `doconfig.h`: `int ai_level;` + `#define CFG_DEF_AI_LEVEL "medium"`; `doconfig.cpp` default `ai_level = TAI_LV_MEDIUM`; čtení `TFILE_LINE v; config.file->ReadStr(v, "ai_level", CFG_DEF_AI_LEVEL, true); bool ok; config.ai_level = TAI_LevelFromName(v, &ok); if (!ok) Warning(LogMsg("Unknown ai_level '%s', using medium", v));`; zápis `config.file->WriteLine("# *** Computer players ***"); config.file->WriteStr("ai_level", CFG_DEF_AI_LEVEL);`.
- [ ] **Step 3: Implement per-slot level** — `TPLAYER_ARRAY::TPLAYER` + `int ai_level;` (v `AddPlayer` `= -1`), `SetAiLevel/GetAiLevel` (range check). `doengine.cpp` `addcpu`: po `AddComputerPlayer()` parsuj volitelný argument `buf+6` (přeskoč mezery), `if (*arg) { bool ok; TAI_LEVEL_ID lv = TAI_LevelFromName(arg,&ok); if (!ok) Warning(...); player_array.SetAiLevel(idx, lv); }`; výpis `addcpu: players=%d level=%s`. Nápověda `Commands:` doplnit `addcpu [easy|medium|hard]`.
- [ ] **Step 4: Implement TAI_PLAYER** — konstruktor jen `SetPlayerType(PT_COMPUTER)`; `UpdateAI`: pokud `!controller` → `EnsureController()`: `int slot = GetPlayerID(); int lv = player_array.GetAiLevel(slot); if (lv < 0) lv = config.ai_level; owned_level = TAI_CreateLevel((TAI_LEVEL_ID)lv); TAI_RNG rng((uint64_t)time(NULL) ^ ((uint64_t)slot * 0x9E3779B97F4A7C15ULL) ^ (uint64_t)(uintptr_t)this); TAI_PERSONALITY pers = TAI_RollPersonality(rng); owned_strategy = NEW TAI_STRATEGY(pers.flavor); controller = NEW TAI_CONTROLLER(this, owned_level, owned_strategy, pers, rng.NextU32());`. `DumpAIDiagnostics`/`EmitAIDiagnosticLines` zavolají `EnsureController()` taky. `TAI_STRATEGY::GeneratePhases`: `phases[3].targets.attack_when_ready = params.aggressivity > 0.3f` zůstává, ale přidej pro `rusher` (aggressivity ≥ 0.95): `phases[2].targets.attack_when_ready = true` a `combat_phase = 2`. Info log při vzniku: `Info(LogMsg("CPU %d: level=%s personality=%s", slot, TAI_LevelName(lv), pers.name));`.
- [ ] **Step 5: Diagnostika** — `EmitDiagnosticLines`: řádek `AI: level=%s personality=%s attack_ratio=%.2f retreat_ratio=%.2f rally=%d` (level jméno drž v controlleru).
- [ ] **Step 6: Run** `bash tests/cpp/ai_smoke.sh` → PASS; `make test-ai` PASS; `make` klient PASS.
- [ ] **Step 7: Commit** `feat(ai): per-CPU random personality, ai_level config and addcpu <level>`

---

### Task 4: Vojenský automat, obrana, odveta, cíle

**Files:** Modify `src/doai.{h,cpp}`

**Interfaces — Consumes:** Task 2 funkce. **Produces:** `enum TAI_MIL_STATE { MIL_GATHER, MIL_ATTACK, MIL_RETREAT };` v controlleru: `TAI_MIL_STATE mil_state; double game_time; double mil_state_since; TAI_RETALIATION retaliation; TAI_ORDER_MEMO army_order; float remembered_enemy_power; double enemy_seen_at; int scout_ids[2]; int n_scouts;`; metody `void ManageArmy()`, `bool ManageDefense(int *committed_ids, int *n_committed)`, `TAI_UNIT_SAMPLE SampleUnit(TMAP_UNIT *u)`, `int ChooseEnemyPlayer()`, `void OrderGroup(TFORCE_UNIT **f, int n, int x, int y, TMAP_UNIT *attack_target)`.

Kroky:
- [ ] **Step 1: Sampling** — `SampleUnit`: `TMAP_ITEM *it = (TMAP_ITEM*)u->GetPointerToItem(); life = ((TBASIC_UNIT*)u)->GetLife(); max_life = it->GetMaxLife(); dps = 0; TARMAMENT *ar = it->GetArmament(); if (ar && ar->GetOffensive()) { TGUN_POWER pw = ar->GetOffensive()->GetPower(); dps = 0.5f*(pw.min+pw.max); }`; `structure = IT_BUILDING||IT_FACTORY`; `military = IT_FORCE && !IT_WORKER`; `attacking_us = target && target->GetPlayerID()==my_id`.
- [ ] **Step 2: Jeden průchod mapou za tick** — nahraď opakované `Find*` skeny funkcí `ScanEnemies()` naplňující pole (max 256) viditelných nepřátelských `TMAP_UNIT*` + jejich `TAI_UNIT_SAMPLE`; z nich: hrozby u základny (Chebyshev ≤ 12 od libovolné naší stavby), `visible_enemy_power`, útočníci (`attacking_us`) → `retaliation.Hit(pid, game_time)`.
- [ ] **Step 3: Obrana** (`ManageDefense`) — pokud existují hrozby: `threat_power = TAI_ArmyPower(hrozby)`; naše bojové jednotky (mimo průzkumníky) seřaď dle vzdálenosti k nejlépe skórované hrozbě (`TAI_TargetScore`), `k = TAI_DefenseCommitCount(threat_power, powers, n, personality.defense_commit)`, prvních `k` dostane `StartAttacking(target)`; ID si ulož (nepoužijí se v `ManageArmy` tento tick).
- [ ] **Step 4: Automat** (`ManageArmy`, zbylé jednotky):
  - `enemy_pid = retaliation.Active(game_time) ? retaliation.Target() : ChooseEnemyPlayer()` (nejbližší aktivní hráč ≠ my, ≠ 0 podle `initial_x/y`); pokud žádný → nic.
  - rally = `TAI_RallyPoint(base, enemy_start, 8, map.width, map.height)`; base = `initial_x/y`, nebo první vlastní stavba.
  - `est = TAI_EnemyPowerEstimate(visible_enemy_power, remembered_enemy_power, game_time - enemy_seen_at)`; pokud `visible_enemy_power > 0` → `remembered = max(remembered*0.9, visible)`, `enemy_seen_at = game_time`.
  - GATHER: jednotky dál než 4 pole od rally → `OrderGroup(... rally ...)` (dedup přes `army_order.Changed(MIL_GATHER, -1, rx, ry)`); přechod do ATTACK, když `(phase.attack_when_ready || retaliation.Active(game_time)) && TAI_ShouldAttack(my_power, est, n, personality, est > 0)`.
  - ATTACK: cíl = nejlépe skórovaný viditelný nepřítel ve vzdálenosti ≤ 20 od těžiště armády, jinak nepřátelský start; `OrderGroup(..., target)` (dedup podle ID cíle / pozice); `local = TAI_ArmyPower(nepřátelé do 10 polí od těžiště)`; `TAI_ShouldRetreat(my_power, local, personality)` → RETREAT; `n < max(2, rally_size/2)` → GATHER.
  - RETREAT: `OrderGroup(rally)`; po 20 s nebo když ≥ 70 % armády je do 5 polí od rally → GATHER.
  - Přechody logovat na stderr při `logs on`: `Player %d military %s -> %s (my=%.0f enemy=%.0f n=%d)`.
- [ ] **Step 5: `OrderGroup`** — oprava #9: pokud `n >= 2` a `tai_request_group_move_forces` uspěje, **nevolat** individuální `StartMoving`; jinak (n==1 nebo selhání) individuální `StartMoving`/`StartAttacking`. Pokud `attack_target` a jednotka ≤ `kTaiAssaultReleaseAttackDist` od cíle → `StartAttacking`. `TaiSendArmyTowardPosition` smazat (nahrazeno).
- [ ] **Step 6: Napojení v `Think()`** — celý blok od „Assault: any visible enemy…“ (dnešní ř. ~2051–2101) nahradit: `game_time += interval` (akumulovaný čas), `ScanEnemies(); ManageDefense(...); ManageArmy();`. Odstranit `retaliate_enemy_pid`, `retaliate_last_path_*`, `assault_group_path_target_id` a `FindThreateningVisibleEnemyForPlayer`/`FindVisibleEnemyStructureNearestTheirStart`/`FindVisibleEnemyCombatUnitOfPlayer` pokud nepoužité. `enemy_contacted` bump fáze zůstává (používá sken).
- [ ] **Step 7: Diagnostika** — `Military: state=%s since=%.0fs my=%.0f enemy_est=%.0f retaliation=%s(pid %d)`.
- [ ] **Step 8: Build + integrace** — `make` klient; server v kopii; `tests/cpp/ai_smoke.sh` rozšířit o `logs on` a běh 300 s (`sleep 300` před `quit`) na `trial` se 2 CPU: očekávat v logu `military GATHER -> ATTACK` aspoň jednou a žádný `Err:`/pád. Pokud za 300 s nikdo nezaútočí, zvyš čas na 600 s a zkontroluj `logs <slot>` (ruling do ledgeru).
- [ ] **Step 9: Commit** `feat(ai): military state machine with rally, power-based attack/retreat and proportional defense`

---

### Task 5: Variabilita — průzkum, stavby, výroba

**Files:** Modify `src/doai.{h,cpp}`

- [ ] **Step 1: Průzkum** — `ManageScouting` přepsat: `want = clamp(round(personality.scout_count), 1, 2)`, jen pokud `force_count >= want + 2` (průzkum neubírá malou armádu); udržuj `scout_ids[]` (ID živých jednotek; mrtvé vyřaď), doplň z nejlehčích bojových jednotek; každý průzkumník, který stojí (`UA_STAY`), dostane nový cíl: s pravděpodobností 0,5 start náhodného nepřítele (`players[pid]->initial_x/y` ± `rng.Index(7)-3`), jinak náhodný bod mapy (`rng.Index(map.width-4)+2`). Průzkumníci jsou vyřazeni z obrany/armády (Task 4 filtruje podle `scout_ids`). Smaž smyčku `for (si < scouts)` v `Think()` a `scout_phase`.
- [ ] **Step 2: Stavby** — `FindBuildPosition`: na začátku `int orient = rng.Index(4)` (otočení pořadí dx/dy: `(dx,dy)`, `(-dx,dy)`, `(dx,-dy)`, `(-dx,-dy)`), a místo návratu prvního platného místa sbírej až 3 kandidáty v rámci stejného průchodu (`pi`) a do rádiusu `první_rádius + 2`; vrať `cands[rng.Index(nc)]`.
- [ ] **Step 3: Výroba** — v `ManageFactories(BG_FORCE)` nahraď první řazení „heavy-prefer“ váženým výběrem: pro kandidáty bez blokátoru (`TAI_PredictProduceBlocker < 0`) váha `w = pow(score / max_score, 0.5f + personality.flavor.aggressivity)`, pokud `heavy_only` jen těžcí; `j = rng.PickWeighted(w, nc)`. Fallbacky „affordable-light“ a „force-queue“ zůstávají.
- [ ] **Step 4: Integrace** — 3 běhy `ai_smoke.sh` (300 s, trial, 2 CPU): v logu různé `personality=` a (`logs think on` stačí na 60 s) různé `StartBuild ... ok` pozice / pořadí; žádný `Err:`.
- [ ] **Step 5: Commit** `feat(ai): dedicated scouts, randomized build sites and weighted unit mix`

---

### Task 6: Dokumentace + finální ověření

**Files:** Modify `docs/AI_SYSTEM.md`

- [ ] **Step 1:** Doplň sekce: osobnosti (tabulka presetů + šum), `ai_level` + `addcpu [level]`, vojenský automat (diagram GATHER/ATTACK/RETREAT + obrana), odveta s vypršením, průzkum, `doai_logic` + `make test-ai`, nové řádky `logs`. Oprav tvrzení o „Default build Easy + Aggressive“ a o Commercial.
- [ ] **Step 2:** `make test-ai`, `make` (klient), build serveru v kopii, `ai_smoke.sh` 300 s na `trial` i `sunnybay` → bez chyb, ATTACK dosažen.
- [ ] **Step 3: Commit** `docs(ai): personalities, levels, military state machine`

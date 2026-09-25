/*
 * Unit tests for the engine-independent CPU AI decision logic (src/doai_logic.*).
 * Build and run: make test-ai
 */
#include "doai_logic.h"
#include <cmath>
#include <cstdio>
#include <cstring>

typedef void (*TestFn)();
static struct { const char *n; TestFn f; } g_tests[128];
static int g_nt = 0, g_fail = 0;
static void reg(const char *n, TestFn f) { g_tests[g_nt].n = n; g_tests[g_nt].f = f; g_nt++; }

#define CHECK(c) do { if (!(c)) { std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)
#define TEST(name) static void name(); \
  static struct name##_reg { name##_reg() { reg(#name, name); } } name##_inst; static void name()

TEST(test_rng_deterministic_for_seed) {
  TAI_RNG a(42), b(42), c(43);
  bool diff = false;
  for (int i = 0; i < 16; i++) {
    uint32_t x = a.NextU32();
    CHECK(x == b.NextU32());
    if (x != c.NextU32()) diff = true;
  }
  CHECK(diff);
}

TEST(test_rng_uniform_and_index_ranges) {
  TAI_RNG r(7);
  for (int i = 0; i < 1000; i++) {
    float u = r.Uniform(2.f, 3.f);
    CHECK(u >= 2.f && u < 3.f);
    int k = r.Index(5);
    CHECK(k >= 0 && k < 5);
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
  CHECK(r.PickWeighted(z, 2) == 0);
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
    for (int i = 0; i < 5; i++)
      if (std::strcmp(p.name, TAI_PERSONALITY_PRESETS[i].name) == 0) seen[i]++;
  }
  for (int i = 0; i < 5; i++) CHECK(seen[i] > 10);
}

static TAI_UNIT_SAMPLE S(float life, float dps, int x = 0, int y = 0, bool st = false, bool mil = true,
                         bool atk = false) {
  TAI_UNIT_SAMPLE s;
  s.life = life; s.max_life = life; s.dps = dps; s.x = x; s.y = y;
  s.structure = st; s.military = mil; s.attacking_us = atk;
  return s;
}

TEST(test_army_power_lanchester) {
  TAI_UNIT_SAMPLE a[2] = {S(100, 10), S(100, 10)};
  CHECK(std::fabs(TAI_ArmyPower(a, 2) - 20.f * 200.f) < 1e-3f);
  CHECK(TAI_ArmyPower(a, 0) == 0.f);
  TAI_UNIT_SAMPLE w[1] = {S(50, 0)};
  CHECK(TAI_ArmyPower(w, 1) == 0.f);
}

TEST(test_ratio_zero_cases) {
  CHECK(TAI_PowerRatio(10.f, 0.f) > 1e8f);
  CHECK(TAI_PowerRatio(0.f, 0.f) == 0.f);
  CHECK(TAI_PowerRatio(0.f, 5.f) == 0.f);
  CHECK(std::fabs(TAI_PowerRatio(6.f, 3.f) - 2.f) < 1e-6f);
}

TEST(test_should_attack_thresholds) {
  TAI_PERSONALITY p = TAI_PERSONALITY_PRESETS[2];  // calm: ratio 1.4, rally 8
  CHECK(!TAI_ShouldAttack(1000.f, 100.f, 7, p, true));
  CHECK(TAI_ShouldAttack(141.f, 100.f, 8, p, true));
  CHECK(!TAI_ShouldAttack(139.f, 100.f, 8, p, true));
}

TEST(test_should_attack_unknown_enemy_needs_more_units) {
  TAI_PERSONALITY p = TAI_PERSONALITY_PRESETS[2];  // rally 8 -> needs 12
  CHECK(!TAI_ShouldAttack(1.f, 0.f, 11, p, false));
  CHECK(TAI_ShouldAttack(1.f, 0.f, 12, p, false));
}

TEST(test_should_retreat) {
  TAI_PERSONALITY p = TAI_PERSONALITY_PRESETS[2];  // retreat 0.75
  CHECK(TAI_ShouldRetreat(70.f, 100.f, p));
  CHECK(!TAI_ShouldRetreat(80.f, 100.f, p));
  CHECK(!TAI_ShouldRetreat(10.f, 0.f, p));
}

TEST(test_defense_commit_count) {
  float pw[4] = {50.f, 50.f, 50.f, 50.f};
  CHECK(TAI_DefenseCommitCount(60.f, pw, 4, 1.5f) == 2);
  CHECK(TAI_DefenseCommitCount(1000.f, pw, 4, 1.5f) == 4);
  CHECK(TAI_DefenseCommitCount(0.f, pw, 4, 1.5f) == 1);
  CHECK(TAI_DefenseCommitCount(60.f, pw, 0, 1.5f) == 0);
}

TEST(test_target_score_order) {
  float attacker = TAI_TargetScore(S(100, 5, 0, 0, false, true, true), 10.f);
  float soldier = TAI_TargetScore(S(100, 5), 10.f);
  float tower = TAI_TargetScore(S(100, 5, 0, 0, true, false), 10.f);
  float farm = TAI_TargetScore(S(100, 0, 0, 0, true, false), 10.f);
  CHECK(attacker > soldier && soldier > tower && tower > farm);
  CHECK(TAI_TargetScore(S(100, 5), 2.f) > TAI_TargetScore(S(100, 5), 20.f));
  TAI_UNIT_SAMPLE hurt = S(100, 5);
  hurt.life = 20.f;
  CHECK(TAI_TargetScore(hurt, 10.f) > soldier);
}

TEST(test_enemy_power_estimate_decay) {
  CHECK(TAI_EnemyPowerEstimate(50.f, 100.f, 0.f) == 100.f);
  CHECK(std::fabs(TAI_EnemyPowerEstimate(0.f, 100.f, 60.f) - 50.f) < 1e-3f);
  CHECK(TAI_EnemyPowerEstimate(80.f, 100.f, 120.f) == 80.f);
}

TEST(test_rally_point_clamped) {
  int x, y;
  TAI_RallyPoint(10, 10, 50, 10, 8, 80, 80, &x, &y);
  CHECK(x == 18 && y == 10);
  TAI_RallyPoint(2, 2, -40, -40, 8, 80, 80, &x, &y);   // negative = unknown -> base
  CHECK(x == 2 && y == 2);
  TAI_RallyPoint(2, 2, 0, 0, 8, 80, 80, &x, &y);       // toward the corner, clamped
  CHECK(x >= 1 && y >= 1);
}

TEST(test_rally_point_without_enemy_base) {
  int x, y;
  TAI_RallyPoint(10, 10, -1, -1, 8, 80, 80, &x, &y);
  CHECK(x == 10 && y == 10);
}

TEST(test_retaliation_expires) {
  TAI_RETALIATION r;
  CHECK(!r.Active(0.0));
  r.Hit(3, 10.0);
  CHECK(r.Active(50.0) && r.Target() == 3);
  CHECK(!r.Active(10.0 + TAI_RETALIATION::kExpire + 0.1));
  r.Hit(2, 200.0);
  CHECK(r.Target() == 2);
  r.Clear();
  CHECK(!r.Active(200.0));
}

TEST(test_military_order_dedup) {
  TAI_ORDER_MEMO m;
  m.Reset();
  CHECK(m.Changed(1, 5, 10, 10));
  CHECK(!m.Changed(1, 5, 10, 10));
  CHECK(m.Changed(1, 6, 10, 10));
  CHECK(m.Changed(2, 6, 10, 10));
}

TEST(test_can_afford_repair) {
  float per_pt[3] = {0.5f, 0.2f, 0.f};
  float rich[3] = {100.f, 100.f, 0.f};
  float no_gold[3] = {0.f, 100.f, 0.f};
  float little[3] = {4.f, 100.f, 0.f};
  CHECK(TAI_CanAffordRepair(rich, per_pt, 3, 20.f));
  CHECK(!TAI_CanAffordRepair(no_gold, per_pt, 3, 20.f));
  CHECK(!TAI_CanAffordRepair(little, per_pt, 3, 20.f));   // 20 points need 10 gold
  float free_pt[3] = {0.f, 0.f, 0.f};
  CHECK(TAI_CanAffordRepair(no_gold, free_pt, 3, 20.f));
}

TEST(test_pick_target_prefers_near_and_keeps_current) {
  TAI_UNIT_SAMPLE e[3] = {S(100, 5, 10, 10), S(100, 5, 12, 10), S(100, 5, 60, 60)};
  // army at (10,10): nearest soldier wins, far one ignored while something is in radius
  CHECK(TAI_PickTarget(e, 3, 10, 10, 20, -1, 25.f) == 0);
  // current target 1 is alive in radius and not beaten by 25 points -> keep it (hysteresis)
  CHECK(TAI_PickTarget(e, 3, 10, 10, 20, 1, 25.f) == 1);
  // a unit attacking us beats the current target by > 25 -> switch
  e[0].attacking_us = true;
  CHECK(TAI_PickTarget(e, 3, 10, 10, 20, 1, 25.f) == 0);
}

TEST(test_pick_target_falls_back_to_anywhere) {
  TAI_UNIT_SAMPLE e[2] = {S(100, 0, 70, 70, true, false), S(100, 5, 50, 50)};
  // nothing within 20 of (10,10) -> best scored enemy anywhere instead of idling at an empty base
  CHECK(TAI_PickTarget(e, 2, 10, 10, 20, -1, 25.f) == 1);
  CHECK(TAI_PickTarget(e, 0, 10, 10, 20, -1, 25.f) == -1);
  // current index out of range is ignored
  CHECK(TAI_PickTarget(e, 2, 10, 10, 20, 7, 25.f) == 1);
}

TEST(test_sent_set_orders_each_unit_once_per_destination) {
  TAI_SENT_SET s;
  s.Reset(5, 5);
  CHECK(!s.Sent(11));
  s.Add(11);
  CHECK(s.Sent(11) && !s.Sent(12));
  CHECK(!s.SameDestination(6, 5) && s.SameDestination(5, 5));
  s.Reset(6, 5);
  CHECK(!s.Sent(11));
  for (int i = 0; i < 200; i++) s.Add(1000 + i);   // capacity is bounded, no overflow
  CHECK(s.Sent(1000));
}

TEST(test_rebalance_moves_miner_to_scarce_material) {
  float stock[3] = {2000.f, 150.f, 900.f};
  int miners[3] = {4, 0, 0};
  bool mineable[3] = {true, true, false};
  int from = -1, to = -1;
  CHECK(TAI_PickMinerRebalance(stock, miners, mineable, 3, 400.f, 3.f, &from, &to));
  CHECK(from == 0 && to == 1);
}

TEST(test_rebalance_keeps_last_miner_and_needs_rich_source) {
  float stock[2] = {2000.f, 150.f};
  bool mineable[2] = {true, true};
  int from = -1, to = -1;
  int one[2] = {1, 0};                       // never strip the only miner
  CHECK(!TAI_PickMinerRebalance(stock, one, mineable, 2, 400.f, 3.f, &from, &to));
  float close[2] = {1000.f, 380.f};          // 1000 < 3 x 400 -> not rich enough
  int two[2] = {3, 0};
  CHECK(!TAI_PickMinerRebalance(close, two, mineable, 2, 400.f, 3.f, &from, &to));
  float fine[2] = {2000.f, 600.f};           // nothing is scarce
  CHECK(!TAI_PickMinerRebalance(fine, two, mineable, 2, 400.f, 3.f, &from, &to));
  bool cannot[2] = {true, false};            // scarce material cannot be mined
  CHECK(!TAI_PickMinerRebalance(stock, two, cannot, 2, 400.f, 3.f, &from, &to));
}

TEST(test_scout_threatened_when_hit_or_targeted) {
  CHECK(TAI_ScoutThreatened(40.f, 50.f, 0));    // lost life since the last tick
  CHECK(TAI_ScoutThreatened(50.f, 50.f, 1));    // someone is attacking it
  CHECK(!TAI_ScoutThreatened(50.f, 50.f, 0));   // untouched
  CHECK(!TAI_ScoutThreatened(55.f, 50.f, 0));   // healed, nobody targets it
}

TEST(test_scout_probes_enemy_base_only_after_cooldown) {
  CHECK(TAI_ScoutMayProbeEnemyBase(100.0, 0.0));     // never chased away
  CHECK(!TAI_ScoutMayProbeEnemyBase(100.0, 130.0));  // chased away, cooling down
  CHECK(TAI_ScoutMayProbeEnemyBase(130.0, 130.0));   // cooldown over
}

TEST(test_unit_reaction_retaliates_or_falls_back) {
  TAI_PERSONALITY p = TAI_PERSONALITY_PRESETS[2];  // calm: retreat_ratio 0.75
  // nobody attacks the unit: keep the army order
  CHECK(TAI_UnitReaction(false, false, 10.f, 50.f, p) == TAI_REACT_KEEP);
  // attacked while hitting a building, fight is even: turn on the attacker
  CHECK(TAI_UnitReaction(true, false, 100.f, 100.f, p) == TAI_REACT_RETALIATE);
  // already fighting the attacker: nothing to change
  CHECK(TAI_UnitReaction(true, true, 100.f, 100.f, p) == TAI_REACT_KEEP);
  // attacked and clearly outnumbered where it stands: fall back to the army
  CHECK(TAI_UnitReaction(true, false, 30.f, 100.f, p) == TAI_REACT_FALL_BACK);
  CHECK(TAI_UnitReaction(true, true, 30.f, 100.f, p) == TAI_REACT_FALL_BACK);
}

int main() {
  for (int i = 0; i < g_nt; i++) {
    int before = g_fail;
    g_tests[i].f();
    std::printf("%s %s\n", g_fail == before ? "ok  " : "FAIL", g_tests[i].n);
  }
  std::printf("%d tests, %d failures\n", g_nt, g_fail);
  return g_fail ? 1 : 0;
}

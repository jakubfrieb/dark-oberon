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

int main() {
  for (int i = 0; i < g_nt; i++) {
    int before = g_fail;
    g_tests[i].f();
    std::printf("%s %s\n", g_fail == before ? "ok  " : "FAIL", g_tests[i].n);
  }
  std::printf("%d tests, %d failures\n", g_nt, g_fail);
  return g_fail ? 1 : 0;
}

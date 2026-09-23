/*
 * Dark Oberon — engine-independent CPU AI decision logic (see doai_logic.h).
 */

#include "doai_logic.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>

//=========================================================================
// TAI_RNG (PCG32, M.E. O'Neill)
//=========================================================================

TAI_RNG::TAI_RNG(uint64_t seed)
  : state(0), inc(1442695040888963407ULL)
{
  NextU32();
  state += seed;
  NextU32();
}

uint32_t TAI_RNG::NextU32()
{
  uint64_t old = state;
  state = old * 6364136223846793005ULL + inc;
  uint32_t xs = (uint32_t)(((old >> 18u) ^ old) >> 27u);
  uint32_t rot = (uint32_t)(old >> 59u);
  return (xs >> rot) | (xs << ((0u - rot) & 31u));
}

float TAI_RNG::Uniform(float a, float b)
{
  return a + (b - a) * (float)(NextU32() >> 8) * (1.0f / 16777216.0f);
}

bool TAI_RNG::Chance(float p)
{
  return Uniform(0.f, 1.f) < p;
}

int TAI_RNG::Index(int n)
{
  return n <= 0 ? 0 : (int)(NextU32() % (uint32_t)n);
}

int TAI_RNG::PickWeighted(const float *w, int n)
{
  float total = 0.f;
  for (int i = 0; i < n; i++)
    if (w[i] > 0.f)
      total += w[i];
  if (total <= 0.f)
    return 0;
  float x = Uniform(0.f, total);
  for (int i = 0; i < n; i++) {
    if (w[i] <= 0.f)
      continue;
    if (x < w[i])
      return i;
    x -= w[i];
  }
  for (int i = n - 1; i >= 0; i--)
    if (w[i] > 0.f)
      return i;
  return 0;
}

//=========================================================================
// Levels
//=========================================================================

static const char *const kLevelNames[3] = {"easy", "medium", "hard"};

TAI_LEVEL_ID TAI_LevelFromName(const char *name, bool *ok)
{
  if (name) {
    for (int lv = 0; lv < 3; lv++) {
      const char *a = name;
      const char *b = kLevelNames[lv];
      while (*a && *b && std::tolower((unsigned char)*a) == *b) {
        a++;
        b++;
      }
      if (!*a && !*b) {
        if (ok)
          *ok = true;
        return (TAI_LEVEL_ID)lv;
      }
    }
  }
  if (ok)
    *ok = false;
  return TAI_LV_MEDIUM;
}

const char *TAI_LevelName(TAI_LEVEL_ID lv)
{
  return (lv >= TAI_LV_EASY && lv <= TAI_LV_HARD) ? kLevelNames[lv] : "medium";
}

//=========================================================================
// Personalities
//=========================================================================

const TAI_PERSONALITY TAI_PERSONALITY_PRESETS[5] = {
  //  name        {agg,   def,   eco}   attack retreat rally commit scouts
  {"aggressive", {0.90f, 0.20f, 0.30f}, 1.15f, 0.60f,  6,   1.3f,  1.5f},
  {"commercial", {0.20f, 0.60f, 0.90f}, 1.80f, 0.90f, 10,   1.8f,  1.0f},
  {"calm",       {0.50f, 0.50f, 0.50f}, 1.40f, 0.75f,  8,   1.5f,  1.0f},
  {"rusher",     {1.00f, 0.05f, 0.25f}, 1.00f, 0.50f,  4,   1.2f,  2.0f},
  {"turtle",     {0.40f, 0.95f, 0.60f}, 2.00f, 1.00f, 14,   2.0f,  1.0f},
};

static float clamp01(float v)
{
  return v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
}

TAI_PERSONALITY TAI_RollPersonality(TAI_RNG &rng)
{
  TAI_PERSONALITY p = TAI_PERSONALITY_PRESETS[rng.Index(5)];
  p.flavor.aggressivity = clamp01(p.flavor.aggressivity * rng.Uniform(0.9f, 1.1f));
  p.flavor.defense_priority = clamp01(p.flavor.defense_priority * rng.Uniform(0.9f, 1.1f));
  p.flavor.econ_focus = clamp01(p.flavor.econ_focus * rng.Uniform(0.9f, 1.1f));
  p.attack_ratio *= rng.Uniform(0.9f, 1.1f);
  p.retreat_ratio = std::min(p.retreat_ratio * rng.Uniform(0.9f, 1.1f), p.attack_ratio * 0.9f);
  p.rally_size = std::max(2, p.rally_size + rng.Index(3) - 1);
  p.defense_commit *= rng.Uniform(0.9f, 1.1f);
  return p;
}

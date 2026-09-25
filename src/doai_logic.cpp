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

//=========================================================================
// Military decisions
//=========================================================================

float TAI_ArmyPower(const TAI_UNIT_SAMPLE *u, int n)
{
  float dps = 0.f, life = 0.f;
  for (int i = 0; i < n; i++) {
    dps += std::max(0.f, u[i].dps);
    life += std::max(0.f, u[i].life);
  }
  return dps * life;
}

float TAI_PowerRatio(float mine, float theirs)
{
  if (mine <= 0.f)
    return 0.f;
  if (theirs <= 0.f)
    return 1e9f;
  return mine / theirs;
}

bool TAI_ShouldAttack(float my_power, float enemy_est, int my_count, const TAI_PERSONALITY &p, bool enemy_known)
{
  if (my_count < p.rally_size)
    return false;
  if (!enemy_known)
    return my_count >= (p.rally_size * 3 + 1) / 2;
  return TAI_PowerRatio(my_power, enemy_est) >= p.attack_ratio;
}

bool TAI_ShouldRetreat(float my_power, float enemy_local, const TAI_PERSONALITY &p)
{
  return enemy_local > 0.f && TAI_PowerRatio(my_power, enemy_local) < p.retreat_ratio;
}

int TAI_DefenseCommitCount(float threat_power, const float *unit_power_by_distance, int n, float commit_factor)
{
  if (n <= 0)
    return 0;
  if (threat_power <= 0.f)
    return 1;
  const float need = commit_factor * threat_power;
  float sum = 0.f;
  for (int i = 0; i < n; i++) {
    sum += unit_power_by_distance[i];
    if (sum >= need)
      return i + 1;
  }
  return n;
}

float TAI_TargetScore(const TAI_UNIT_SAMPLE &t, float dist)
{
  float s = 0.f;
  if (t.attacking_us)
    s += 100.f;
  if (t.military)
    s += 40.f;
  else if (t.structure && t.dps > 0.f)
    s += 30.f;
  else if (t.structure)
    s += 10.f;
  s -= 2.f * dist;
  if (t.max_life > 0.f)
    s += (1.f - t.life / t.max_life) * 20.f;
  return s;
}

float TAI_EnemyPowerEstimate(float visible, float remembered, float seconds_since_seen)
{
  const float decayed = remembered * std::pow(0.5f, std::max(0.f, seconds_since_seen) / 60.f);
  return std::max(visible, decayed);
}

void TAI_RallyPoint(int bx, int by, int ex, int ey, int dist, int map_w, int map_h, int *ox, int *oy)
{
  int x = bx, y = by;
  if (ex >= 0 && ey >= 0) {
    const float dx = (float)(ex - bx), dy = (float)(ey - by);
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len >= 1.f) {
      x = bx + (int)std::lround(dx / len * (float)dist);
      y = by + (int)std::lround(dy / len * (float)dist);
    }
  }
  if (map_w > 2)
    x = std::min(std::max(x, 1), map_w - 2);
  if (map_h > 2)
    y = std::min(std::max(y, 1), map_h - 2);
  *ox = x;
  *oy = y;
}

static int cheb(int ax, int ay, int bx, int by)
{
  const int dx = std::abs(ax - bx), dy = std::abs(ay - by);
  return dx > dy ? dx : dy;
}

int TAI_PickTarget(const TAI_UNIT_SAMPLE *e, int n, int cx, int cy, int radius, int current, float margin)
{
  int best_in = -1, best_any = -1;
  float s_in = -1e30f, s_any = -1e30f;
  for (int i = 0; i < n; i++) {
    const int d = cheb(e[i].x, e[i].y, cx, cy);
    const float s = TAI_TargetScore(e[i], (float)d);
    if (s > s_any) {
      s_any = s;
      best_any = i;
    }
    if (d <= radius && s > s_in) {
      s_in = s;
      best_in = i;
    }
  }
  if (best_in < 0)
    return best_any;
  if (current >= 0 && current < n) {
    const int dc = cheb(e[current].x, e[current].y, cx, cy);
    if (dc <= radius && TAI_TargetScore(e[current], (float)dc) + margin >= s_in)
      return current;
  }
  return best_in;
}

bool TAI_SENT_SET::Sent(int id) const
{
  for (int i = 0; i < n; i++)
    if (ids[i] == id)
      return true;
  return false;
}

void TAI_SENT_SET::Add(int id)
{
  if (n < kCap && !Sent(id))
    ids[n++] = id;
}

bool TAI_PickMinerRebalance(const float *stock, const int *miners, const bool *mineable, int n_materials,
                            float low, float rich_factor, int *from, int *to)
{
  int scarce = -1, rich = -1;
  for (int i = 0; i < n_materials; i++) {
    if (mineable[i] && stock[i] < low && (scarce < 0 || stock[i] < stock[scarce]))
      scarce = i;
    if (miners[i] >= 2 && stock[i] >= rich_factor * low && (rich < 0 || stock[i] > stock[rich]))
      rich = i;
  }
  if (scarce < 0 || rich < 0 || scarce == rich)
    return false;
  *from = rich;
  *to = scarce;
  return true;
}

bool TAI_CanAffordRepair(const float *stored, const float *mat_per_pt, int n_materials, float points)
{
  for (int i = 0; i < n_materials; i++)
    if (mat_per_pt[i] > 0.f && stored[i] < mat_per_pt[i] * points)
      return false;
  return true;
}

const double TAI_RETALIATION::kExpire = 90.0;

bool TAI_ORDER_MEMO::Changed(int k, int t, int px, int py)
{
  if (k == kind && t == target && px == x && py == y)
    return false;
  kind = k;
  target = t;
  x = px;
  y = py;
  return true;
}

bool TAI_ScoutThreatened(float life_now, float life_last, int enemies_targeting)
{
  return enemies_targeting > 0 || life_now < life_last;
}

bool TAI_ScoutMayProbeEnemyBase(double now, double chased_until)
{
  return now >= chased_until;
}

TAI_UNIT_REACTION TAI_UnitReaction(bool attacked, bool fighting_attacker, float my_local, float enemy_local,
                                   const TAI_PERSONALITY &p)
{
  if (!attacked)
    return TAI_REACT_KEEP;
  if (TAI_ShouldRetreat(my_local, enemy_local, p))
    return TAI_REACT_FALL_BACK;
  return fighting_attacker ? TAI_REACT_KEEP : TAI_REACT_RETALIATE;
}

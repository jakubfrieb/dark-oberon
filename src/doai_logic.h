/*
 * Dark Oberon — CPU AI decision logic that does not depend on the engine.
 *
 * Everything here is pure (inputs -> outputs) so it can be unit-tested by
 * tests/cpp/test_ai_logic.cpp (make test-ai). The engine glue lives in doai.cpp.
 * The AI must never use the global rand(): the simulation relies on it.
 */

#ifndef __doai_logic_h__
#define __doai_logic_h__

#include <cstdint>

//! Tunables that replace the old Aggressive / Commercial / Calm flavor classes.
struct TAI_FLAVOR_PARAMS {
  float aggressivity;      //!< 0..1 army size, scout_ratio, willingness to attack
  float defense_priority;  //!< 0..1 towers/walls
  float econ_focus;        //!< 0..1 workers, farms
};

//! Private PCG32 generator of the AI (independent of rand()).
class TAI_RNG {
public:
  explicit TAI_RNG(uint64_t seed = 1);
  uint32_t NextU32();
  //! Uniform float in [a, b).
  float Uniform(float a, float b);
  bool Chance(float p);
  //! Uniform index in [0, n); 0 when n <= 0.
  int Index(int n);
  //! Index chosen proportionally to positive weights; 0 when all weights are <= 0.
  int PickWeighted(const float *w, int n);

private:
  uint64_t state;
  uint64_t inc;
};

enum TAI_LEVEL_ID { TAI_LV_EASY = 0, TAI_LV_MEDIUM = 1, TAI_LV_HARD = 2 };

//! "easy" / "medium" / "hard" (case-insensitive); anything else -> medium and *ok = false.
TAI_LEVEL_ID TAI_LevelFromName(const char *name, bool *ok);
const char *TAI_LevelName(TAI_LEVEL_ID lv);

//! Personality of one CPU player: economy flavor + military temperament.
struct TAI_PERSONALITY {
  const char *name;
  TAI_FLAVOR_PARAMS flavor;
  float attack_ratio;     //!< required my/enemy power ratio before attacking
  float retreat_ratio;    //!< retreat when my/enemy power ratio drops below this
  int rally_size;         //!< minimum army size before an attack
  float defense_commit;   //!< defenders sent = enough units to reach commit x threat power
  float scout_count;      //!< dedicated scouts (rounded, 1..2)
};

//! aggressive, commercial, calm, rusher, turtle
extern const TAI_PERSONALITY TAI_PERSONALITY_PRESETS[5];

//! Random preset with +-10 % noise on its parameters.
TAI_PERSONALITY TAI_RollPersonality(TAI_RNG &rng);

//=========================================================================
// Military decisions
//=========================================================================

//! What the AI knows about one unit (own or visible enemy).
struct TAI_UNIT_SAMPLE {
  float life, max_life;
  float dps;              //!< average gun power (0 = unarmed)
  int x, y;
  bool structure;         //!< building or factory
  bool military;          //!< combat unit (not worker, not structure)
  bool attacking_us;      //!< its current target belongs to the AI player
};

//! Lanchester-style strength: (sum of dps) x (sum of life).
float TAI_ArmyPower(const TAI_UNIT_SAMPLE *u, int n);
//! mine / theirs; theirs <= 0 -> huge (if mine > 0) or 0 (if mine <= 0).
float TAI_PowerRatio(float mine, float theirs);
//! Enough units and (known enemy: ratio >= attack_ratio; unknown: 1.5 x rally_size units).
bool TAI_ShouldAttack(float my_power, float enemy_est, int my_count, const TAI_PERSONALITY &p, bool enemy_known);
//! Local enemy present and ratio < retreat_ratio.
bool TAI_ShouldRetreat(float my_power, float enemy_local, const TAI_PERSONALITY &p);
//! How many of the nearest units (powers sorted by distance) to send so their power >= factor x threat.
int TAI_DefenseCommitCount(float threat_power, const float *unit_power_by_distance, int n, float commit_factor);
//! Higher = better target: attackers, soldiers, armed structures, other structures; near and wounded.
float TAI_TargetScore(const TAI_UNIT_SAMPLE &t, float dist);
//! max(visible, remembered halved every 60 s).
float TAI_EnemyPowerEstimate(float visible, float remembered, float seconds_since_seen);
//! Point @p dist tiles from base toward the enemy base, clamped to the map; base when enemy is unknown (<0).
void TAI_RallyPoint(int bx, int by, int ex, int ey, int dist, int map_w, int map_h, int *ox, int *oy);

//! Revenge target that expires when nobody hits us for kExpire seconds.
class TAI_RETALIATION {
public:
  static const double kExpire;
  TAI_RETALIATION() { Clear(); }
  void Hit(int pid, double now) { target = pid; last_hit = now; }
  bool Active(double now) const { return target >= 0 && now - last_hit <= kExpire; }
  int Target() const { return target; }
  void Clear() { target = -1; last_hit = -1e9; }

private:
  int target;
  double last_hit;
};

//! Remembers the last army order so the same order is not re-sent every think tick.
struct TAI_ORDER_MEMO {
  int kind, target, x, y;
  void Reset() { kind = -1; target = -1; x = y = -1; }
  bool Changed(int k, int t, int px, int py);
};

#endif

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

#endif

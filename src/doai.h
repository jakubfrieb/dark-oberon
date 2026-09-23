/*
 * Dark Oberon — computer player AI (Level × parametric phase strategy).
 */

#ifndef __doai_h__
#define __doai_h__

#include <stdio.h>

#include "doai_logic.h"
#include "dolayout.h"
#include "dounits.h"

enum TAI_BUILD_GOAL {
  BG_NONE,
  BG_WORKER,
  BG_FORCE,
  BG_FARM,
  BG_RESOURCE_BLDG,
  BG_FACTORY,
  BG_DEFENSE,
  BG_UPGRADE
};

//! One line of text (no trailing newline). Used for on-screen console and FILE sinks.
typedef void (*TAI_LineSink)(void *user, const char *line);

struct TAI_GAME_STATE {
  int worker_count;
  int idle_worker_count;
  int force_count;
  int factory_count;
  int has_military_factory;
  //! Completed factories that can train IT_FORCE (barracks, workshop, …).
  int military_factory_count;
  //! Military-type factories currently under construction (avoid duplicate foundations).
  int military_factory_placing;

  static const int kMaxIdleWorkers = 32;
  static const int kMaxIdleFactories = 16;
  static const int kMaxIdleForces = 64;

  TWORKER_UNIT *idle_workers[kMaxIdleWorkers];
  int idle_workers_len;
  TFACTORY_UNIT *idle_factories[kMaxIdleFactories];
  int idle_factories_len;
  TFORCE_UNIT *idle_forces[kMaxIdleForces];
  int idle_forces_len;

  float materials[SCH_MAX_MATERIALS_COUNT];
  int food_in;
  int food_out;
  int energy_in;
  int energy_out;

  bool has_source[SCH_MAX_MATERIALS_COUNT];

  //! Completed IT_BUILDING units (excludes factories).
  int building_count;
  //! Completed defensive IT_BUILDING (towers/walls/citadel per AI match rules).
  int defense_building_count;
  //! Per-material: player has a completed building that accepts this material for unload.
  bool can_unload_material[SCH_MAX_MATERIALS_COUNT];
  bool has_any_unload_building;
  //! energy_in >= energy_out (or no energy drain); factories need this for production per min_energy.
  bool energy_sufficient;
  //! Any building/factory still under construction (worker should not all go mining).
  bool has_unfinished_construction;
  //! RQ_PRODUCING stalled: need_id==0 (same gate as @c dofactories.cpp first food payment).
  bool any_factory_blocked_on_food;

  void Clear();
  void ScanFromPlayer(TPLAYER *p);
};

class TAI_LEVEL {
public:
  virtual ~TAI_LEVEL() {}
  virtual double GetThinkInterval() const = 0;
  virtual int GetMaxActionsPerTick() const = 0;
  virtual double GetReactionDelay() const = 0;
  virtual bool AllowMultipleBuilds() const = 0;
  virtual float GetResourceAwareness() const = 0;
  //! Max workers per unfinished building (engine adds build progress per worker in range).
  virtual int GetMaxConstructionHelpers() const = 0;
};

class TAI_LEVEL_EASY : public TAI_LEVEL {
public:
  double GetThinkInterval() const override { return 3.0; }
  int GetMaxActionsPerTick() const override { return 1; }
  double GetReactionDelay() const override { return 5.0; }
  bool AllowMultipleBuilds() const override { return false; }
  float GetResourceAwareness() const override { return 0.3f; }
  int GetMaxConstructionHelpers() const override { return 2; }
};

class TAI_LEVEL_MEDIUM : public TAI_LEVEL {
public:
  double GetThinkInterval() const override { return 1.5; }
  int GetMaxActionsPerTick() const override { return 3; }
  double GetReactionDelay() const override { return 2.5; }
  bool AllowMultipleBuilds() const override { return true; }
  float GetResourceAwareness() const override { return 0.6f; }
  int GetMaxConstructionHelpers() const override { return 3; }
};

class TAI_LEVEL_HARD : public TAI_LEVEL {
public:
  double GetThinkInterval() const override { return 0.5; }
  int GetMaxActionsPerTick() const override { return 16; }
  double GetReactionDelay() const override { return 0.5; }
  bool AllowMultipleBuilds() const override { return true; }
  float GetResourceAwareness() const override { return 1.0f; }
  int GetMaxConstructionHelpers() const override { return 4; }
};


struct TAI_PHASE_TARGETS {
  int min_workers;
  int min_forces;
  int min_factories;
  int min_buildings;
  int min_defense_buildings;
  //! If > 0, keep placing military factories until count >= this (TH does not count).
  int target_military_factories;
  //! If > 0 and a military factory exists, queue troops until force_count >= this (phase satisfaction uses min_forces only).
  int train_to_forces;
  float scout_ratio;
  bool attack_when_ready;
  bool build_farms;
};

struct TAI_PHASE {
  const char *name;
  TAI_PHASE_TARGETS targets;
  bool IsSatisfied(const TAI_GAME_STATE &s) const;
};

class TAI_STRATEGY {
public:
  static const int kMaxPhases = 6;

  //! @p rush: attack already in the militarize phase (rusher personality).
  explicit TAI_STRATEGY(const TAI_FLAVOR_PARAMS &p, bool rush = false);

  int GetPhaseCount() const { return phase_count; }
  const TAI_PHASE &GetPhase(int idx) const;
  int GetLoopPhase() const { return loop_phase; }
  int GetCombatPhase() const { return combat_phase; }
  const TAI_FLAVOR_PARAMS &GetFlavorParams() const { return params; }

private:
  TAI_FLAVOR_PARAMS params;
  bool rush;
  TAI_PHASE phases[kMaxPhases];
  int phase_count;
  int loop_phase;
  int combat_phase;
  void GeneratePhases();
};

//! Army behaviour of a CPU player (defense runs independently every tick).
enum TAI_MIL_STATE { MIL_GATHER, MIL_ATTACK, MIL_RETREAT };

class TAI_CONTROLLER {
public:
  TAI_CONTROLLER(TPLAYER *owner, TAI_LEVEL *level, TAI_STRATEGY *strategy,
                 const TAI_PERSONALITY &personality, TAI_LEVEL_ID level_id, uint64_t rng_seed);
  const TAI_PERSONALITY &GetPersonality() const { return personality; }
  ~TAI_CONTROLLER();

  void Think(double dt);

  static bool FactoryProducesMilitary(TFACTORY_ITEM *fi);
  static bool MatchesGoal(TBUILDING_ITEM *bi, TAI_BUILD_GOAL goal);

  //! Refreshes scanned state and prints phase, targets, deficit goal, timers (for dedicated-server `logs`).
  void DumpDiagnostics(FILE *f);
  void EmitDiagnosticLines(TAI_LineSink sink, void *user);

private:
  TPLAYER *player;
  TAI_LEVEL *level;
  TAI_STRATEGY *strategy;
  TAI_PERSONALITY personality;
  TAI_LEVEL_ID level_id;
  TAI_RNG rng;
  TAI_GAME_STATE state;
  double think_accumulator;
  unsigned mining_rr;
  unsigned scout_phase;
  int current_phase;
  bool enemy_contacted;
  //! On loop phase (assault): each fully satisfied tick bumps this so targets keep rising (no idle endgame).
  int target_escalation;

  //! Seconds of simulated time seen by this controller (sum of think intervals).
  double game_time;

  //! Visible enemy units of this think tick (pointers valid only within the tick).
  static const int kMaxEnemies = 256;
  TMAP_UNIT *enemy_units[kMaxEnemies];
  TAI_UNIT_SAMPLE enemy_samples[kMaxEnemies];
  bool enemy_near_base[kMaxEnemies];
  int n_enemies;
  float visible_enemy_power;
  float remembered_enemy_power;
  double enemy_seen_at;

  TAI_MIL_STATE mil_state;
  double mil_state_since;
  TAI_RETALIATION retaliation;
  TAI_ORDER_MEMO army_order;
  //! Units sent to defend this tick (excluded from the field army).
  int defender_ids[TAI_GAME_STATE::kMaxIdleForces];
  int n_defenders;
  //! Dedicated scouts (excluded from defense and the field army).
  int scout_ids[2];
  int n_scouts;
  //! Rotates which idle military factory is tried first (Workshop vs Barracks on budget=1).
  unsigned factory_military_rr;

  static bool CanPursueGoal(TAI_BUILD_GOAL goal, const TAI_GAME_STATE &s);
  static TAI_BUILD_GOAL ResolvePrerequisite(TAI_BUILD_GOAL goal, const TAI_GAME_STATE &s);
  static TAI_BUILD_GOAL ComputeHighestDeficit(const TAI_GAME_STATE &s, const TAI_PHASE_TARGETS &t);

  void HandleResourceShortage();
  bool AssignIdleWorkers(int max_assign, TAI_BUILD_GOAL pursued_build_goal);
  int ManageFactories(TAI_BUILD_GOAL prod_goal, int max_orders);
  bool ManageBuilding(TAI_BUILD_GOAL goal);
  int AssistUnfinishedConstruction(int max_assign);
  int AssistDamagedFriendlyStructures(int max_assign);
  void ScanEnemies();
  void ManageDefense();
  void ManageArmy();
  TAI_UNIT_SAMPLE SampleUnit(TMAP_UNIT *u) const;
  int ChooseEnemyPlayer() const;
  void GetBase(int *x, int *y) const;
  bool IsScout(int unit_id) const;
  bool IsDefender(int unit_id) const;
  //! Field army: military units that are neither scouts nor defenders this tick.
  int CollectArmy(TFORCE_UNIT **out, int max_out) const;
  void OrderGroup(TFORCE_UNIT **forces, int n, int x, int y, TMAP_UNIT *attack_target);
  void SetMilState(TAI_MIL_STATE s, float my_power, float enemy_power, int n);
  void ManageScouting();
  bool FindBuildPosition(TBUILDING_ITEM *item, TPOSITION &out_pos);
  //! Nearest source for which worker->CanMine is true (searches all deposits of that material).
  TSOURCE_UNIT *FindNearestMineableSource(TWORKER_UNIT *worker, int material);
  //! Idle worker, or stop one miner when blueprint materials/food are ready (else keep mining).
  TWORKER_UNIT *AcquireWorkerForConstruction(TAI_BUILD_GOAL goal);
  void TraceFactoryProductionNeeds();
};

class TAI_PLAYER : public TPLAYER {
public:
  TAI_PLAYER();
  ~TAI_PLAYER() override;

  void UpdateAI(double time_shift) override;

  void DumpAIDiagnostics(FILE *f);
  void EmitAIDiagnosticLines(TAI_LineSink sink, void *user);

private:
  //! Creates level/personality/controller on first use: the slot id is set after construction.
  void EnsureController();

  TAI_CONTROLLER *controller;
  TAI_LEVEL *owned_level;
  TAI_STRATEGY *owned_strategy;
};

//! stderr: phase changes as "Player … reached new phase …" when enabled.
//! New level object for @p lv (caller owns it).
TAI_LEVEL *TAI_CreateLevel(TAI_LEVEL_ID lv);

void TAI_SetPhaseTransitionLogging(bool enable);
bool TAI_GetPhaseTransitionLogging(void);

//! stderr: per-think-tick trace (deficit, build, mining attempts) when enabled.
void TAI_SetThinkTraceLogging(bool enable);
bool TAI_GetThinkTraceLogging(void);

//! List CPU player slots (dedicated server / headless console).
void TAI_LogListCpuPlayers(FILE *out);

//! Dump one CPU slot; no-op with message if invalid or not in game.
void TAI_LogDumpPlayerAI(int player_slot, FILE *out);

void TAI_EmitCpuPlayersListLines(TAI_LineSink sink, void *user);
void TAI_EmitPlayerAIDumpLines(int player_slot, TAI_LineSink sink, void *user);

#endif

/*
 * Dark Oberon — computer player AI implementation.
 */

#include "cfg.h"
#include "doalloc.h"

#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <cstdarg>
#include <algorithm>

#include "doai.h"
#include "domap.h"
#include "doschemes.h"
#include "dotime.h"
#include "doipc.h"
#include "doplayers.h"
#include "dowalk.h"
#include "doevents.h"
#include "dofight.h"
#include "doconfig.h"
#include "dologs.h"

#include <ctime>

extern TMAP map;
extern TPLAYER **players;

//! While assaulting, do not override group march with StartAttacking until this close (Chebyshev tiles).
static const int kTaiAssaultReleaseAttackDist = 10;

static int tai_cheb_dist(const TPOSITION_3D &a, const TPOSITION_3D &b)
{
  int dx = (int)a.x - (int)b.x;
  int dy = (int)a.y - (int)b.y;
  if (dx < 0)
    dx = -dx;
  if (dy < 0)
    dy = -dy;
  return dx > dy ? dx : dy;
}

static bool tai_unit_alive(TMAP_UNIT *u)
{
  return u && !u->TestState(US_DYING) && !u->TestState(US_ZOMBIE) && !u->TestState(US_DELETE);
}

static int tai_cheb_xy(int ax, int ay, int bx, int by)
{
  const int dx = std::abs(ax - bx), dy = std::abs(ay - by);
  return dx > dy ? dx : dy;
}

static void tai_free_path_sel_nodes(TPATH_INFO *path_info)
{
  TSEL_NODE *node = path_info->unit_list;
  while (node) {
    TSEL_NODE *nx = node->next;
    pool_sel_node->PutToPool(node);
    node = nx;
  }
  path_info->unit_list = NULL;
}

/**
 * One AI decision issues a single group-path job (DevideToGroups / MoveGroup), same pipeline as the human
 * multi-select move — not N independent pathfinds from StartAttacking.
 */
static bool tai_request_group_move_forces(TPLAYER *pl, TFORCE_UNIT **forces, int n, int goal_x, int goal_y)
{
  if (!pl || !pl->GetLocalMap() || n < 2 || !pool_path_info || !pool_sel_node || !threadpool_astar)
    return false;
  if (!map.IsInMap(goal_x, goal_y))
    return false;

  process_mutex->Lock();

  TPATH_INFO *path_info = pool_path_info->GetFromPool();
  if (!path_info) {
    process_mutex->Unlock();
    return false;
  }

  path_info->goal.x = goal_x;
  path_info->goal.y = goal_y;
  path_info->real_goal.x = goal_x;
  path_info->real_goal.y = goal_y;
  path_info->unit_list = NULL;
  path_info->loc_map = pl->GetLocalMap();
  path_info->event_type = ET_GROUP_MOVING;

  const double time_stamp = AppGetTimeSeconds();
  int added = 0;

  for (int i = 0; i < n; i++) {
    TFORCE_UNIT *raw = forces[i];
    if (!raw)
      continue;

    TSEL_NODE *new_node = pool_sel_node->GetFromPool();
    if (!new_node) {
      tai_free_path_sel_nodes(path_info);
      pool_path_info->PutToPool(path_info);
      process_mutex->Unlock();
      return false;
    }
    new_node->next = new_node->prev = NULL;

    SDL_LockMutex(delete_mutex);
    new_node->unit = static_cast<TFORCE_UNIT *>(raw->AcquirePointer());
    SDL_UnlockMutex(delete_mutex);

    if (!new_node->unit) {
      pool_sel_node->PutToPool(new_node);
      continue;
    }

    TFORCE_UNIT *fu = new_node->unit;
    if (!path_info->unit_list) {
      path_info->unit_list = new_node;
      fu->SendEvent(false, time_stamp, US_WAIT_FOR_PATH, 0);
      path_info->request_id = fu->pevent->GetRequestID();
    } else {
      path_info->unit_list->prev = new_node;
      new_node->next = path_info->unit_list;
      path_info->unit_list = new_node;
      fu->SendEvent(false, time_stamp, US_WAIT_FOR_PATH, path_info->request_id);
    }
    raw->SetWaitRequestId(path_info->request_id);
    added++;
  }

  if (added < 2) {
    tai_free_path_sel_nodes(path_info);
    pool_path_info->PutToPool(path_info);
    process_mutex->Unlock();
    return false;
  }

  threadpool_astar->AddRequest(path_info, &TA_STAR_ALG::DevideToGroups);
  process_mutex->Unlock();
  return true;
}

static bool tai_skip_assault_attack_for_approach(TFORCE_UNIT *fu, TMAP_UNIT *attack_target)
{
  if (fu->TestState(US_WAIT_FOR_PATH))
    return true;
  if (fu->TestState(US_NEXT_STEP) || fu->TestState(US_TRY_TO_MOVE) || fu->TestState(US_MOVE)) {
    if (tai_cheb_dist(fu->GetPosition(), attack_target->GetPosition()) > kTaiAssaultReleaseAttackDist)
      return true;
  }
  return false;
}

static bool g_tai_phase_transition_log = false;
static bool g_tai_think_trace_log = false;

void TAI_SetPhaseTransitionLogging(bool enable)
{
  g_tai_phase_transition_log = enable;
}

void TAI_SetThinkTraceLogging(bool enable)
{
  g_tai_think_trace_log = enable;
}

static void tai_ai_trace(int player_id, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

static void tai_ai_trace(int player_id, const char *fmt, ...)
{
  if (!g_tai_think_trace_log)
    return;
  char buf[512];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  fprintf(stderr, "[ai trace] pid=%d %s\n", player_id, buf);
}

static void tai_file_line_sink(void *user, const char *line)
{
  FILE *f = static_cast<FILE *>(user);
  if (f && line) {
    fputs(line, f);
    fputc('\n', f);
  }
}

static const int kTaiMaxTargetEscalation = 96;

//! Scale phase targets on the assault loop so economy and army keep growing after base goals are met.
static TAI_PHASE_TARGETS TaiApplyEscalation(const TAI_PHASE_TARGETS &base, int tier, const TAI_FLAVOR_PARAMS &fp)
{
  TAI_PHASE_TARGETS t = base;
  if (tier <= 0)
    return t;
  const int w = 2 + (int)(fp.econ_focus * 3.f);
  const int f = 3 + (int)(fp.aggressivity * 5.f);
  const int mil = 2 + (int)(fp.aggressivity * 2.f);
  /* Cap worker target so escalation does not drown army production (BG_FORCE never reached). */
  t.min_workers = base.min_workers + std::min(tier * w, 28);
  t.min_forces += tier * mil;
  if (t.train_to_forces > 0)
    t.train_to_forces += tier * f;
  if ((tier % 4) == 3)
    t.min_buildings += 1;
  return t;
}

static const char *GoalName(TAI_BUILD_GOAL g)
{
  switch (g) {
  case BG_NONE:
    return "BG_NONE";
  case BG_WORKER:
    return "BG_WORKER";
  case BG_FORCE:
    return "BG_FORCE";
  case BG_FARM:
    return "BG_FARM";
  case BG_RESOURCE_BLDG:
    return "BG_RESOURCE_BLDG";
  case BG_FACTORY:
    return "BG_FACTORY";
  case BG_DEFENSE:
    return "BG_DEFENSE";
  case BG_UPGRADE:
    return "BG_UPGRADE";
  case BG_ENERGY:
    return "BG_ENERGY";
  default:
    return "?";
  }
}

static bool TAI_BuildingMaterialsMet(TPLAYER *p, TBUILDING_ITEM *building)
{
  if (!p || !building)
    return false;
  for (int i = 0; i < scheme.materials_count; i++) {
    if (building->materials[i] > p->GetStoredMaterial(i))
      return false;
  }
  return true;
}

static bool TAI_BuildingFoodOk(TPLAYER *p, TBUILDING_ITEM *building)
{
  if (!p || !building)
    return false;
  if (((building->ancestor) && (p->GetInFood() - p->GetOutFood() + building->food - building->ancestor->food < 0))
      || ((!building->ancestor) && (building->food < 0)
          && (p->GetInFood() - p->GetOutFood() + building->food < 0)))
    return false;
  return true;
}

//! True if some blueprint for @p goal can be paid from stock (materials + food); map/position not checked.
static bool TAI_AnyAffordableBlueprint(TPLAYER *p, TAI_BUILD_GOAL goal)
{
  if (goal == BG_NONE || goal == BG_WORKER || goal == BG_FORCE)
    return false;
  for (TPLAYER_UNIT *u = p->units; u; u = u->GetNext()) {
    if (u->TestState(US_DYING) || u->TestState(US_ZOMBIE) || u->TestState(US_DELETE))
      continue;
    if (!u->TestItemType(IT_WORKER))
      continue;
    TWORKER_ITEM *wi = static_cast<TWORKER_ITEM *>(u->GetPointerToItem());
    if (!wi)
      continue;
    typedef TLIST<TBUILDING_ITEM>::TNODE<TBUILDING_ITEM> BNode;
    for (BNode *node = wi->build_list.GetFirst(); node; node = node->GetNext()) {
      TBUILDING_ITEM *bi = node->GetPitem();
      if (!TAI_CONTROLLER::MatchesGoal(bi, goal))
        continue;
      if (TAI_BuildingMaterialsMet(p, bi) && TAI_BuildingFoodOk(p, bi))
        return true;
    }
  }
  return false;
}

/** @return -1 ok, 0 food, 1 energy %, 2+i material index i */
static int TAI_PredictProduceBlocker(TPLAYER *p, TFACTORY_UNIT *fu, TFORCE_ITEM *prod)
{
  if (!p || !fu || !prod)
    return -1;
  TFACTORY_ITEM *itm = static_cast<TFACTORY_ITEM *>(fu->GetPointerToItem());
  if (!itm)
    return -1;
  if (p->GetPercentEnergy() < itm->min_energy)
    return 1;
  if ((prod->food < 0) && (p->GetInFood() - p->GetOutFood() + prod->food < 0))
    return 0;
  for (int i = 0; i < scheme.materials_count; i++) {
    if (prod->materials[i] / UNI_PRODUCING_COUNT > p->GetStoredMaterial(i))
      return 2 + i;
  }
  return -1;
}

static void TAI_TraceBuildingStockBlockers(int pid, TBUILDING_ITEM *bi, TPLAYER *p)
{
  if (!g_tai_think_trace_log || !bi || !p)
    return;
  char matbuf[160];
  int mp = 0;
  matbuf[0] = 0;
  for (int i = 0; i < scheme.materials_count && mp < (int)sizeof(matbuf) - 24; i++) {
    if (bi->materials[i] <= p->GetStoredMaterial(i))
      continue;
    float need = bi->materials[i] - p->GetStoredMaterial(i);
    const char *mn =
        (i >= 0 && i < scheme.materials_count && scheme.materials[i]) ? scheme.materials[i]->name : "?";
    mp += snprintf(matbuf + mp, sizeof(matbuf) - (size_t)mp, "%s%s:need~%.0f", mp ? "," : "", mn, need);
  }
  if (mp > 0)
    tai_ai_trace(pid, "build \"%s\": missing materials [%s] -> keep mining / unlock deposits", bi->name ? bi->name : "?",
                 matbuf);
  if (!TAI_BuildingFoodOk(p, bi))
    tai_ai_trace(pid, "build \"%s\": not enough food balance for upkeep -> next: BG_FARM when phase allows, or wait",
                 bi->name ? bi->name : "?");
}

//! True if a 1-tile ring around the footprint touches any completed IT_BUILDING / IT_FACTORY (forces a walkway).
static bool AiFootprintTouchesAdjacentStructure(const TPOSITION &pos, T_SIMPLE w, T_SIMPLE h)
{
  const int x0 = pos.x - w / 2;
  const int y0 = pos.y - h / 2;
  for (int j = -1; j <= h; j++) {
    for (int i = -1; i <= w; i++) {
      if (i >= 0 && i < w && j >= 0 && j < h)
        continue;
      const int tx = x0 + i;
      const int ty = y0 + j;
      if (!map.IsInMap(static_cast<T_SIMPLE>(tx), static_cast<T_SIMPLE>(ty)))
        continue;
      for (int seg = 0; seg < DAT_SEGMENTS_COUNT; seg++) {
        TMAP_UNIT *u = map.segments[seg].surface[tx][ty].unit;
        if (!u || u->TestState(US_DYING) || u->TestState(US_ZOMBIE) || u->TestState(US_DELETE))
          continue;
        if (u->TestItemType(IT_BUILDING) || u->TestItemType(IT_FACTORY))
          return true;
      }
    }
  }
  return false;
}

//! Keep 2-tile-wide N/S and E/W lanes from start (Chebyshev > exempt) so the base does not wall itself in.
static bool AiFootprintViolatesBoulevardLanes(TPLAYER *p, const TPOSITION &pos, T_SIMPLE w, T_SIMPLE h)
{
  if (!p || p->initial_x < 0)
    return false;
  const int ix = p->initial_x;
  const int iy = p->initial_y;
  const int exempt_r = 6;

  for (T_SIMPLE j = 0; j < h; j++) {
    for (T_SIMPLE i = 0; i < w; i++) {
      const int tx = pos.x + i - w / 2;
      const int ty = pos.y + j - h / 2;
      const int ax = std::abs(tx - ix);
      const int ay = std::abs(ty - iy);
      const int d = std::max(ax, ay);
      if (d <= exempt_r)
        continue;
      if ((tx == ix || tx == ix + 1) && (ty >= iy + 2 || ty <= iy - 2))
        return true;
      if ((ty == iy || ty == iy + 1) && (tx >= ix + 2 || tx <= ix - 2))
        return true;
    }
  }
  return false;
}

void TAI_EmitCpuPlayersListLines(TAI_LineSink sink, void *user)
{
  if (!sink)
    return;
  char buf[PL_MAX_PLAYER_NAME_LENGTH + 32];
  player_array.Lock();
  int n = player_array.GetCount();
  sink(user, "CPU players (slot / name):");
  int any = 0;
  for (int i = 0; i < n; i++) {
    if (!player_array.IsComputer(i))
      continue;
    any++;
    snprintf(buf, sizeof(buf), "  %d  %s", i, player_array.GetPlayerName(i).c_str());
    sink(user, buf);
  }
  if (!any)
    sink(user, "  (none)");
  player_array.Unlock();
}

void TAI_LogListCpuPlayers(FILE *out)
{
  if (!out)
    return;
  TAI_EmitCpuPlayersListLines(tai_file_line_sink, out);
}

void TAI_EmitPlayerAIDumpLines(int player_slot, TAI_LineSink sink, void *user)
{
  if (!sink)
    return;
  char buf[160];
  player_array.Lock();
  if (player_slot < 0 || player_slot >= player_array.GetCount()) {
    snprintf(buf, sizeof(buf), "logs: invalid player slot %d", player_slot);
    sink(user, buf);
    player_array.Unlock();
    return;
  }
  if (!player_array.IsComputer(player_slot)) {
    snprintf(buf, sizeof(buf), "logs: slot %d is not a CPU player", player_slot);
    sink(user, buf);
    player_array.Unlock();
    return;
  }
  player_array.Unlock();

  if (!players || !players[player_slot]) {
    snprintf(buf, sizeof(buf), "logs: slot %d has no live player object (game not started?)", player_slot);
    sink(user, buf);
    return;
  }

  TAI_PLAYER *ai = dynamic_cast<TAI_PLAYER *>(players[player_slot]);
  if (!ai) {
    snprintf(buf, sizeof(buf), "logs: slot %d is not TAI_PLAYER", player_slot);
    sink(user, buf);
    return;
  }
  ai->EmitAIDiagnosticLines(sink, user);
}

void TAI_LogDumpPlayerAI(int player_slot, FILE *out)
{
  if (!out)
    return;
  TAI_EmitPlayerAIDumpLines(player_slot, tai_file_line_sink, out);
}


static bool BuildingItemIsDefense(TBUILDING_ITEM *bi)
{
  if (!bi)
    return false;
  TDRAW_ITEM *di = static_cast<TDRAW_ITEM *>(bi);
  const char *tid = di->text_id;
  if (!tid)
    return false;
  return (strstr(tid, "tower") != NULL || strstr(tid, "wall") != NULL
          || strstr(tid, "citadel") != NULL);
}

//! How many scheme materials this building accepts for unload (0 if none / not a depot).
static int CountUnloadMaterialKinds(TBUILDING_ITEM *bi)
{
  if (!bi || !bi->AllowAnyMaterial())
    return 0;
  int n = 0;
  for (int m = 0; m < scheme.materials_count; m++) {
    if (bi->GetAllowedMaterial(static_cast<T_BYTE>(m)))
      n++;
  }
  return n;
}

static void RegisterUnloadCapabilities(TBUILDING_UNIT *bu, TAI_GAME_STATE *st)
{
  if (!bu || bu->TestState(US_IS_BEING_BUILT))
    return;
  TBUILDING_ITEM *bi = static_cast<TBUILDING_ITEM *>(bu->GetPointerToItem());
  if (!bi || !bi->AllowAnyMaterial())
    return;
  for (int m = 0; m < scheme.materials_count; m++) {
    if (bi->GetAllowedMaterial(static_cast<T_BYTE>(m))) {
      st->can_unload_material[m] = true;
      st->has_any_unload_building = true;
    }
  }
}

//! Hold one peasant back for construction only while something is actively being built.
//! (Reserving whenever !has_military_factory made max_workers=0 on Easy with 2 peasants — no mining.)
static bool NeedsIdleWorkerReserveForBuilding(const TAI_GAME_STATE &s)
{
  return s.has_unfinished_construction && s.idle_workers_len >= 2;
}

//! Higher score = heavier / costlier military unit (prefer for training when affordable).
static float TaiMilitaryUnitTrainingScore(TFORCE_ITEM *p)
{
  if (!p || p->GetItemType() == IT_WORKER)
    return -1.f;
  float s = (float)p->GetMaxLife();
  for (int i = 0; i < scheme.materials_count; i++)
    s += p->materials[i];
  TARMAMENT *ar = p->GetArmament();
  if (ar && ar->GetOffensive())
    s += 25.f;
  return s;
}

static float TaiFactoryBestMilitaryScore(TFACTORY_ITEM *fi)
{
  float best = -1e9f;
  if (!fi)
    return best;
  for (TPRODUCEABLE_NODE *pn = fi->GetProductsList().GetFirstNode(); pn; pn = pn->GetNextNode()) {
    TFORCE_ITEM *p = pn->GetProduceableItem();
    if (!p || p->GetItemType() == IT_WORKER)
      continue;
    const float sc = TaiMilitaryUnitTrainingScore(p);
    if (sc > best)
      best = sc;
  }
  return best;
}

//! Min/max training score over all military products this player can ever build (completed factories).
static void TaiPlayerMilitaryProductScoreRange(TPLAYER *p, float *out_min, float *out_max)
{
  *out_min = 1e9f;
  *out_max = -1e9f;
  bool any = false;
  if (!p)
    return;
  for (TPLAYER_UNIT *u = p->units; u; u = u->GetNext()) {
    if (u->TestState(US_DYING) || u->TestState(US_ZOMBIE) || u->TestState(US_DELETE))
      continue;
    if (!u->TestItemType(IT_FACTORY))
      continue;
    TFACTORY_UNIT *fu = static_cast<TFACTORY_UNIT *>(u);
    if (fu->TestState(US_IS_BEING_BUILT))
      continue;
    TFACTORY_ITEM *fi = static_cast<TFACTORY_ITEM *>(fu->GetPointerToItem());
    if (!TAI_CONTROLLER::FactoryProducesMilitary(fi))
      continue;
    for (TPRODUCEABLE_NODE *pn = fi->GetProductsList().GetFirstNode(); pn; pn = pn->GetNextNode()) {
      TFORCE_ITEM *pr = pn->GetProduceableItem();
      if (!pr || pr->GetItemType() == IT_WORKER)
        continue;
      const float sc = TaiMilitaryUnitTrainingScore(pr);
      if (sc < 0.f)
        continue;
      any = true;
      if (sc < *out_min)
        *out_min = sc;
      if (sc > *out_max)
        *out_max = sc;
    }
  }
  if (!any) {
    *out_min = *out_max = 0.f;
  }
}

static void TaiCountMilitaryForcesLightHeavy(TPLAYER *p, float heavy_threshold, int *n_light, int *n_heavy)
{
  *n_light = *n_heavy = 0;
  if (!p)
    return;
  for (TPLAYER_UNIT *u = p->units; u; u = u->GetNext()) {
    if (u->TestState(US_DYING) || u->TestState(US_ZOMBIE) || u->TestState(US_DELETE))
      continue;
    if (!u->TestItemType(IT_FORCE) || u->TestItemType(IT_WORKER))
      continue;
    TFORCE_ITEM *it = static_cast<TFORCE_ITEM *>(u->GetPointerToItem());
    if (!it || it->GetItemType() == IT_WORKER)
      continue;
    const float sc = TaiMilitaryUnitTrainingScore(it);
    if (sc >= heavy_threshold)
      (*n_heavy)++;
    else
      (*n_light)++;
  }
}

//! Repair points the stock must cover before workers are sent (else they stop at once and idle forever).
static const float kTaiRepairAffordPoints = 20.f;

static bool TaiStructureNeedsRepair(TPLAYER *p, TMAP_UNIT *u)
{
  if (!u || u->TestState(US_DYING) || u->TestState(US_ZOMBIE) || u->TestState(US_DELETE))
    return false;
  if (!u->TestItemType(IT_BUILDING) && !u->TestItemType(IT_FACTORY))
    return false;
  TBASIC_UNIT *bu = static_cast<TBASIC_UNIT *>(u);
  if (bu->TestState(US_IS_BEING_BUILT))
    return false;
  TMAP_ITEM *mi = static_cast<TMAP_ITEM *>(bu->GetPointerToItem());
  if (!mi)
    return false;
  const int mx = mi->GetMaxLife();
  if (mx <= 0)
    return false;
  if (bu->GetLife() >= (float)mx * 0.93f)
    return false;
  /* Engine stops a repairing worker when any material < mat_per_pt: never order a repair we cannot pay,
   * otherwise the same idle workers are re-ordered every tick and never go mining (economy deadlock). */
  TBASIC_ITEM *bi = static_cast<TBASIC_ITEM *>(bu->GetPointerToItem());
  float stored[SCH_MAX_MATERIALS_COUNT], per_pt[SCH_MAX_MATERIALS_COUNT];
  for (int i = 0; i < scheme.materials_count; i++) {
    stored[i] = p ? p->GetStoredMaterial(i) : 0.f;
    per_pt[i] = bi ? bi->mat_per_pt[i] : 0.f;
  }
  return TAI_CanAffordRepair(stored, per_pt, scheme.materials_count, kTaiRepairAffordPoints);
}

void TAI_GAME_STATE::Clear()
{
  worker_count = 0;
  idle_worker_count = 0;
  force_count = 0;
  factory_count = 0;
  has_military_factory = 0;
  military_factory_count = 0;
  military_factory_placing = 0;
  idle_workers_len = 0;
  idle_factories_len = 0;
  idle_forces_len = 0;
  for (int i = 0; i < SCH_MAX_MATERIALS_COUNT; i++) {
    materials[i] = 0;
    has_source[i] = false;
    can_unload_material[i] = false;
  }
  food_in = food_out = energy_in = energy_out = 0;
  building_count = 0;
  defense_building_count = 0;
  has_any_unload_building = false;
  energy_sufficient = true;
  has_unfinished_construction = false;
  any_factory_blocked_on_food = false;
}

void TAI_GAME_STATE::ScanFromPlayer(TPLAYER *p)
{
  Clear();

  if (!p || !p->race)
    return;

  food_in = p->GetInFood();
  food_out = p->GetOutFood();
  energy_in = p->GetInEnergy();
  energy_out = p->GetOutEnergy();
  energy_sufficient = (energy_out <= 0 || energy_in >= energy_out);

  for (int m = 0; m < scheme.materials_count; m++) {
    materials[m] = p->GetStoredMaterial(m);
    if (hyper_player && hyper_player->sources[m].GetLength() > 0)
      has_source[m] = true;
  }

  for (TPLAYER_UNIT *u = p->units; u; u = u->GetNext()) {
    if (u->TestState(US_DYING) || u->TestState(US_ZOMBIE) || u->TestState(US_DELETE))
      continue;

    if (u->TestItemType(IT_WORKER)) {
      worker_count++;
      TWORKER_UNIT *w = static_cast<TWORKER_UNIT *>(u);
      if (w->GetAction() == UA_STAY && idle_workers_len < kMaxIdleWorkers)
        idle_workers[idle_workers_len++] = w;
    } else if (u->TestItemType(IT_FORCE)) {
      force_count++;
      TFORCE_UNIT *fu = static_cast<TFORCE_UNIT *>(u);
      if (fu->GetAction() == UA_STAY && idle_forces_len < kMaxIdleForces)
        idle_forces[idle_forces_len++] = fu;
    } else if (u->TestItemType(IT_BUILDING)) {
      TBUILDING_UNIT *bu = static_cast<TBUILDING_UNIT *>(u);
      if (bu->TestState(US_IS_BEING_BUILT) && bu->GetPrepayed() > 0.f)
        has_unfinished_construction = true;
      if (!bu->TestState(US_IS_BEING_BUILT)) {
        building_count++;
        RegisterUnloadCapabilities(bu, this);
        TBUILDING_ITEM *bi = static_cast<TBUILDING_ITEM *>(bu->GetPointerToItem());
        if (bi && BuildingItemIsDefense(bi))
          defense_building_count++;
      }
    } else if (u->TestItemType(IT_FACTORY)) {
      factory_count++;
      TFACTORY_UNIT *f = static_cast<TFACTORY_UNIT *>(u);
      if (f->TestState(US_IS_BEING_BUILT) && f->GetPrepayed() > 0.f)
        has_unfinished_construction = true;
      TFACTORY_ITEM *fi = static_cast<TFACTORY_ITEM *>(f->GetPointerToItem());
      if (TAI_CONTROLLER::FactoryProducesMilitary(fi)) {
        if (!f->TestState(US_IS_BEING_BUILT)) {
          has_military_factory = 1;
          military_factory_count++;
        } else
          military_factory_placing++;
      }

      if (!f->TestState(US_IS_BEING_BUILT)) {
        RegisterUnloadCapabilities(f, this);
        if (f->GetOrderSize() > 0 && f->GetNeedID() == 0)
          any_factory_blocked_on_food = true;
      }

      if (f->GetOrderSize() == 0 && !f->TestState(US_IS_BEING_BUILT)
          && f->CanAddUnitToOrder(NULL) && idle_factories_len < kMaxIdleFactories)
        idle_factories[idle_factories_len++] = f;
    }
  }

  idle_worker_count = idle_workers_len;
}

bool TAI_CONTROLLER::FactoryProducesMilitary(TFACTORY_ITEM *fi)
{
  if (!fi)
    return false;
  for (TPRODUCEABLE_NODE *n = fi->GetProductsList().GetFirstNode(); n; n = n->GetNextNode()) {
    TFORCE_ITEM *prod = n->GetProduceableItem();
    if (prod && prod->GetItemType() == IT_FORCE)
      return true;
  }
  return false;
}

bool TAI_CONTROLLER::CanPursueGoal(TAI_BUILD_GOAL goal, const TAI_GAME_STATE &s)
{
  switch (goal) {
  case BG_NONE:
  case BG_UPGRADE:
    return false;
  case BG_RESOURCE_BLDG:
  case BG_FARM:
  case BG_ENERGY:
    return true;
  case BG_FACTORY:
    return s.has_any_unload_building;
  case BG_FORCE:
    return s.has_military_factory != 0 && s.has_any_unload_building;
  case BG_WORKER:
    return s.idle_factories_len > 0;
  case BG_DEFENSE:
    return s.has_any_unload_building;
  default:
    return false;
  }
}

TAI_BUILD_GOAL TAI_CONTROLLER::ResolvePrerequisite(TAI_BUILD_GOAL goal, const TAI_GAME_STATE &s)
{
  switch (goal) {
  case BG_FORCE:
    if (!s.has_military_factory)
      return BG_FACTORY;
    if (!s.has_any_unload_building)
      return BG_RESOURCE_BLDG;
    return BG_NONE;
  case BG_FACTORY:
    if (!s.has_any_unload_building)
      return BG_RESOURCE_BLDG;
    return BG_NONE;
  case BG_DEFENSE:
    if (!s.has_any_unload_building)
      return BG_RESOURCE_BLDG;
    return BG_NONE;
  case BG_WORKER:
    if (s.idle_factories_len <= 0)
      return BG_FACTORY;
    return BG_NONE;
  default:
    return BG_NONE;
  }
}

bool TAI_CONTROLLER::MatchesGoal(TBUILDING_ITEM *bi, TAI_BUILD_GOAL goal)
{
  if (!bi)
    return false;

  switch (goal) {
  case BG_FARM:
    return bi->food > 0;
  case BG_FACTORY:
    if (bi->GetItemType() != IT_FACTORY)
      return false;
    return FactoryProducesMilitary(static_cast<TFACTORY_ITEM *>(bi));
  case BG_RESOURCE_BLDG:
    return (bi->GetItemType() == IT_BUILDING || bi->GetItemType() == IT_FACTORY)
           && bi->AllowAnyMaterial();
  case BG_DEFENSE:
    return BuildingItemIsDefense(bi);
  case BG_ENERGY:
    return bi->energy > 0;
  case BG_UPGRADE:
    return false;
  default:
    return false;
  }
}

TSOURCE_UNIT *TAI_CONTROLLER::FindNearestMineableSource(TWORKER_UNIT *worker, int material)
{
  if (!worker || !hyper_player || material < 0 || material >= SCH_MAX_MATERIALS_COUNT)
    return NULL;

  typedef TLIST<TSOURCE_UNIT>::TNODE<TSOURCE_UNIT> TNode;
  TNode *node = hyper_player->sources[material].GetFirst();
  TSOURCE_UNIT *best = NULL;
  int best_dist = 0x7fffffff;
  TPOSITION_3D wpos = worker->GetPosition();

  for (; node; node = node->GetNext()) {
    TSOURCE_UNIT *s = node->GetPitem();
    if (!s || s->IsEmpty() || s->TestState(US_DYING) || s->TestState(US_ZOMBIE))
      continue;
    if (!worker->CanMine(s, false, true))
      continue;
    int dx = s->GetPosition().x - wpos.x;
    int dy = s->GetPosition().y - wpos.y;
    int dist = dx * dx + dy * dy;
    if (dist < best_dist) {
      best_dist = dist;
      best = s;
    }
  }
  return best;
}

bool TAI_CONTROLLER::FindBuildPosition(TBUILDING_ITEM *item, TPOSITION &out_pos)
{
  if (!item || !player)
    return false;

  int cx = player->initial_x;
  int cy = player->initial_y;
  if (cx < 0)
    cx = 0;
  if (cy < 0)
    cy = 0;

  static const struct {
    bool margin;
    bool lanes;
  } passes[4] = {{true, true}, {true, false}, {false, true}, {false, false}};
  /* Variability: random spiral orientation, then a random pick among the first few valid sites. */
  const int sx = rng.Chance(0.5f) ? 1 : -1;
  const int sy = rng.Chance(0.5f) ? 1 : -1;
  static const int kMaxSiteCandidates = 3;
  TPOSITION cands[kMaxSiteCandidates];

  for (int pi = 0; pi < 4; pi++) {
    const bool use_margin = passes[pi].margin;
    const bool use_lanes = passes[pi].lanes;
    int nc = 0;
    int first_radius = -1;
    for (int radius = 2; radius < 40; radius++) {
      if (first_radius >= 0 && radius > first_radius + 2)
        break;
      for (int ddx = -radius; ddx <= radius && nc < kMaxSiteCandidates; ddx++) {
        for (int ddy = -radius; ddy <= radius && nc < kMaxSiteCandidates; ddy++) {
          if (abs(ddx) != radius && abs(ddy) != radius)
            continue;
          const int dx = ddx * sx;
          const int dy = ddy * sy;
          int px = cx + dx;
          int py = cy + dy;
          if (!item->IsPositionAvailable(px, py, false))
            continue;
          TPOSITION cpos;
          cpos.SetPosition(px + item->GetWidth() / 2, py + item->GetHeight() / 2);
          if (use_margin && AiFootprintTouchesAdjacentStructure(cpos, item->GetWidth(), item->GetHeight()))
            continue;
          if (use_lanes && AiFootprintViolatesBoulevardLanes(player, cpos, item->GetWidth(), item->GetHeight()))
            continue;
          if (first_radius < 0)
            first_radius = radius;
          cands[nc++] = cpos;
        }
      }
    }
    if (nc > 0) {
      if (pi > 0 && g_tai_think_trace_log)
        tai_ai_trace((int)player->GetPlayerID(), "FindBuildPosition: using relaxed rules (margin=%s lanes=%s)",
                     use_margin ? "y" : "n", use_lanes ? "y" : "n");
      out_pos = cands[rng.Index(nc)];
      return true;
    }
    if (g_tai_think_trace_log && player && pi < 3)
      tai_ai_trace((int)player->GetPlayerID(),
                   "FindBuildPosition: no site with margin=%s lanes=%s; relaxing further",
                   passes[pi].margin ? "y" : "n", passes[pi].lanes ? "y" : "n");
  }
  return false;
}

TAI_STRATEGY::TAI_STRATEGY(const TAI_FLAVOR_PARAMS &p, bool rush_)
  : params(p), rush(rush_), phase_count(0), loop_phase(0), combat_phase(0)
{
  GeneratePhases();
}

void TAI_STRATEGY::GeneratePhases()
{
  phase_count = 4;
  /* After the last phase (assault) is satisfied, stay on assault. Using loop_phase=militarize(2) caused
   * 3→2→3 every think: phase 2 has no train_to cap so it re-satisfied immediately while assault was harder. */
  loop_phase = phase_count - 1;
  combat_phase = (params.aggressivity > 0.7f) ? 3 : 2;

  const int w_expand = 2 + (int)(params.econ_focus * 5.f) + (int)(params.aggressivity * 3.f);
  const int f_mil = 2 + (int)(params.aggressivity * 6.f);
  const int d_mil = (int)(params.defense_priority * 2.f);
  const int d_ass = (int)(params.defense_priority * 3.f);
  /* Aggressive armies need food upkeep; econ_focus alone left the aggressive preset without farms. */
  const bool farms = params.econ_focus > 0.5f || params.aggressivity > 0.75f;
  const int w_battle = std::min(28, w_expand + 3 + (int)(params.aggressivity * 6.f));

  phases[0].name = "establish";
  phases[0].targets.min_workers = 1;
  phases[0].targets.min_forces = 0;
  phases[0].targets.min_factories = 0;
  phases[0].targets.min_buildings = 1;
  phases[0].targets.min_defense_buildings = 0;
  phases[0].targets.target_military_factories = 0;
  phases[0].targets.train_to_forces = 0;
  phases[0].targets.scout_ratio = 0.15f * params.aggressivity;
  phases[0].targets.attack_when_ready = false;
  phases[0].targets.build_farms = false;

  phases[1].name = "expand";
  phases[1].targets.min_workers = w_expand;
  phases[1].targets.min_forces = 0;
  phases[1].targets.min_factories = 1;
  phases[1].targets.min_buildings = 1;
  phases[1].targets.min_defense_buildings = 0;
  phases[1].targets.target_military_factories = 0;
  phases[1].targets.train_to_forces = 2 + (int)(params.aggressivity * 6.f);
  phases[1].targets.scout_ratio = 0.2f + 0.15f * params.aggressivity;
  phases[1].targets.attack_when_ready = false;
  phases[1].targets.build_farms = farms;

  phases[2].name = "militarize";
  phases[2].targets.min_workers = w_battle;
  phases[2].targets.min_forces = f_mil;
  phases[2].targets.min_factories = 1;
  phases[2].targets.min_buildings = 1;
  phases[2].targets.min_defense_buildings = d_mil;
  phases[2].targets.target_military_factories = 2;
  phases[2].targets.train_to_forces = 0;
  phases[2].targets.scout_ratio = 0.3f + params.aggressivity * 0.2f;
  phases[2].targets.attack_when_ready = false;
  phases[2].targets.build_farms = farms;

  phases[3].name = "assault";
  phases[3].targets.min_workers = w_battle;
  phases[3].targets.min_forces = f_mil;
  phases[3].targets.min_factories = 1;
  phases[3].targets.min_buildings = 1;
  phases[3].targets.min_defense_buildings = d_ass;
  phases[3].targets.target_military_factories = 2;
  /* Well above min_forces so the army keeps growing while economy allows. */
  phases[3].targets.train_to_forces = f_mil + 6 + (int)(params.aggressivity * 10.f);
  phases[3].targets.scout_ratio = 0.3f + params.aggressivity * 0.2f;
  phases[3].targets.attack_when_ready = (params.aggressivity > 0.3f);
  phases[3].targets.build_farms = farms;

  if (rush) {
    /* Rusher: fight as soon as the first army is up, do not wait for the assault phase. */
    combat_phase = 2;
    phases[2].targets.attack_when_ready = true;
  }
}

const TAI_PHASE &TAI_STRATEGY::GetPhase(int idx) const
{
  if (idx < 0 || idx >= phase_count)
    idx = 0;
  return phases[idx];
}

bool TAI_PHASE::IsSatisfied(const TAI_GAME_STATE &s) const
{
  if (targets.min_buildings > 0 && !s.has_any_unload_building)
    return false;
  if (s.worker_count < targets.min_workers)
    return false;
  /* Without barracks, min_forces cannot be met — do not block phase (Compute pushes BG_FACTORY). */
  if (targets.min_forces > 0 && s.has_military_factory && s.force_count < targets.min_forces)
    return false;
  if (s.factory_count < targets.min_factories)
    return false;
  if (targets.min_factories > 0 && !s.has_military_factory)
    return false;
  if (targets.train_to_forces > 0 && s.has_military_factory && s.force_count < targets.train_to_forces)
    return false;
  /* A town hall is a factory (it trains workers) but it is the base's main building: maps that
   * start with only a town hall would otherwise never leave "establish" (no deficit builds one). */
  if (s.building_count + s.factory_count < targets.min_buildings)
    return false;
  if (s.defense_building_count < targets.min_defense_buildings)
    return false;
  if (targets.build_farms && s.food_out > 0 && s.food_in < s.food_out)
    return false;
  return true;
}

TAI_BUILD_GOAL TAI_CONTROLLER::ComputeHighestDeficit(const TAI_GAME_STATE &s, const TAI_PHASE_TARGETS &t)
{
  if (t.min_buildings > 0 && !s.has_any_unload_building)
    return BG_RESOURCE_BLDG;
  /* Town hall counts as factory_count but does not train military — need barracks etc. */
  /* First military building: need at least one peasant; wait if a barracks is already mid-build. */
  if (t.min_factories > 0 && s.has_any_unload_building && s.worker_count >= 1 && !s.has_military_factory
      && s.military_factory_placing == 0)
    return BG_FACTORY;
  if (s.factory_count < t.min_factories)
    return BG_FACTORY;
  /* Food before training; matches IsSatisfaction / factory stall on first payment. */
  if (t.build_farms
      && ((s.food_out > 0 && s.food_in < s.food_out)
          || (s.has_military_factory && s.any_factory_blocked_on_food)))
    return BG_FARM;
  /* Energy consumers (barracks, workshops) stall below their min_energy: add supply first. */
  if (s.energy_out > 0 && s.energy_in < s.energy_out)
    return BG_ENERGY;
  /* With a minimal economy, train before chasing escalated min_workers into peasant spam. */
  static const int kFloorWorkersBeforeArmy = 8;
  if (s.has_military_factory && s.worker_count >= kFloorWorkersBeforeArmy) {
    if (s.force_count < t.min_forces)
      return BG_FORCE;
    if (t.train_to_forces > 0 && s.force_count < t.train_to_forces)
      return BG_FORCE;
  }
  if (s.worker_count < t.min_workers)
    return BG_WORKER;
  if (t.target_military_factories > 0 && s.has_military_factory
      && s.military_factory_count + s.military_factory_placing < t.target_military_factories
      && s.has_any_unload_building && s.worker_count >= t.min_workers)
    return BG_FACTORY;
  if (s.force_count < t.min_forces && s.has_military_factory)
    return BG_FORCE;
  if (t.train_to_forces > 0 && s.has_military_factory && s.force_count < t.train_to_forces)
    return BG_FORCE;
  if (s.defense_building_count < t.min_defense_buildings)
    return BG_DEFENSE;
  return BG_NONE;
}

TAI_CONTROLLER::TAI_CONTROLLER(TPLAYER *owner, TAI_LEVEL *lvl, TAI_STRATEGY *strat,
                               const TAI_PERSONALITY &pers, TAI_LEVEL_ID lv_id, uint64_t rng_seed)
  : player(owner), level(lvl), strategy(strat), personality(pers), level_id(lv_id), rng(rng_seed),
    think_accumulator(0), mining_rr(0),
    current_phase(0), enemy_contacted(false), target_escalation(0), game_time(0.0), n_enemies(0),
    visible_enemy_power(0.f), remembered_enemy_power(0.f), enemy_seen_at(-1e9), mil_state(MIL_GATHER),
    mil_state_since(0.0), attack_target_id(-1), n_defenders(0), last_rebalance(-1e9), n_scouts(0),
    factory_military_rr(0)
{
  army_order.Reset();
  rally_sent.Reset(-1, -1);
}

TAI_CONTROLLER::~TAI_CONTROLLER() = default;

void TAI_CONTROLLER::HandleResourceShortage()
{
  const bool net_food_short = (state.food_out > 0 && state.food_in < state.food_out);
  if (net_food_short || state.any_factory_blocked_on_food) {
    bool farm = ManageBuilding(BG_FARM);
    if (g_tai_think_trace_log && player) {
      tai_ai_trace(player->GetPlayerID(),
                   "shortage food -> ManageBuilding(BG_FARM) %s (net=%s factory_need_food=%s)", farm ? "ok" : "noop",
                   net_food_short ? "y" : "n", state.any_factory_blocked_on_food ? "y" : "n");
    }
  }

  if (state.energy_out > 0 && state.energy_in < state.energy_out) {
    bool built = ManageBuilding(BG_ENERGY);
    if (g_tai_think_trace_log && player)
      tai_ai_trace(player->GetPlayerID(), "shortage energy (%d < %d) -> ManageBuilding(BG_ENERGY) %s",
                   state.energy_in, state.energy_out, built ? "ok" : "noop");
  }

  /* Rebuild workforce if nearly wiped (not only 0 — avoids long deadlock at 1 peasant). */
  if (state.worker_count < 2) {   /* town hall trains workers without energy */
    for (int i = 0; i < state.idle_factories_len; i++) {
      TFACTORY_UNIT *f = state.idle_factories[i];
      TFACTORY_ITEM *fi = static_cast<TFACTORY_ITEM *>(f->GetPointerToItem());
      for (TPRODUCEABLE_NODE *pn = fi->GetProductsList().GetFirstNode(); pn;
           pn = pn->GetNextNode()) {
        if (pn->GetProduceableItem()
            && pn->GetProduceableItem()->GetItemType() == IT_WORKER) {
          TFORCE_ITEM *prod = pn->GetProduceableItem();
          f->AddUnitToOrder(prod);
          if (g_tai_think_trace_log && player)
            tai_ai_trace(player->GetPlayerID(), "shortage no workers -> ordered %s",
                         prod->name ? prod->name : "?");
          return;
        }
      }
    }
  }
}

TWORKER_UNIT *TAI_CONTROLLER::AcquireWorkerForConstruction(TAI_BUILD_GOAL goal)
{
  if (!player)
    return NULL;
  const bool can_pay_blueprint = TAI_AnyAffordableBlueprint(player, goal);
  if (state.idle_workers_len > 0)
    return state.idle_workers[0];
  if (!can_pay_blueprint)
    return NULL;
  for (TPLAYER_UNIT *u = player->units; u; u = u->GetNext()) {
    if (u->TestState(US_DYING) || u->TestState(US_ZOMBIE) || u->TestState(US_DELETE))
      continue;
    if (!u->TestItemType(IT_WORKER))
      continue;
    TWORKER_UNIT *ww = static_cast<TWORKER_UNIT *>(u);
    if (ww->GetAction() != UA_MINE)
      continue;
    /* ClearActions() removes LIST_WORKING but does not AddToMap. Miners inside a source
     * (IsInsideMining) were DeleteFromMap'd — yanking them here leaves the unit off-map. */
    if (!ww->IsInMap()) {
      if (g_tai_think_trace_log)
        tai_ai_trace((int)player->GetPlayerID(),
                     "AcquireWorkerForConstruction(%s): skip miner held off-map (inside source/unload)",
                     GoalName(goal));
      continue;
    }
    ww->ClearActions();
    if (g_tai_think_trace_log && player)
      tai_ai_trace((int)player->GetPlayerID(), "AcquireWorkerForConstruction(%s): pulled worker from mining",
                   GoalName(goal));
    return ww;
  }
  return NULL;
}

bool TAI_CONTROLLER::AssignIdleWorkers(int max_assign, TAI_BUILD_GOAL pursued_build_goal)
{
  const int pid = player ? (int)player->GetPlayerID() : -1;

  int eligible[SCH_MAX_MATERIALS_COUNT];
  int ne = 0;
  for (int m = 0; m < scheme.materials_count; m++) {
    if (state.has_source[m] && state.can_unload_material[m])
      eligible[ne++] = m;
  }
  if (ne == 0) {
    if (g_tai_think_trace_log && player)
      tai_ai_trace(pid, "AssignIdleWorkers: skip (no eligible materials: need has_source && can_unload)");
    return false;
  }

  int order[SCH_MAX_MATERIALS_COUNT];
  for (int i = 0; i < ne; i++)
    order[i] = eligible[i];
  for (int a = 0; a < ne - 1; a++) {
    for (int b = a + 1; b < ne; b++) {
      float sa = state.materials[order[a]];
      float sb = state.materials[order[b]];
      if (sb < sa || (sb == sa && order[b] < order[a])) {
        int t = order[a];
        order[a] = order[b];
        order[b] = t;
      }
    }
  }

  int max_workers = max_assign;
  if (max_workers > state.idle_workers_len)
    max_workers = state.idle_workers_len;
  if (NeedsIdleWorkerReserveForBuilding(state) && state.idle_workers_len >= 2)
    max_workers--;
  /* Barracks affordable: keep a peasant off mining. If materials/food short, everyone mines until stock is ready. */
  const bool barracks_reserve = !state.has_military_factory && pursued_build_goal == BG_FACTORY
                                && player && TAI_AnyAffordableBlueprint(player, BG_FACTORY);
  if (barracks_reserve) {
    if (state.idle_workers_len >= 2) {
      const int cap = state.idle_workers_len - 1;
      if (max_workers > cap)
        max_workers = cap;
    }
    if (state.idle_workers_len == 1)
      max_workers = 0;
  }
  /* Easy: max_assign is 1; reserve can make max_workers=0. Bump to 1 unless we intentionally hold workers
   * for BG_FACTORY (barracks) placement. */
  if (max_workers < 1 && ne > 0 && state.idle_workers_len >= 1 && max_assign >= 1) {
    if (!barracks_reserve)
      max_workers = 1;
  }

  if (g_tai_think_trace_log && player) {
    char el[96];
    int p = 0;
    for (int e = 0; e < ne && p < (int)sizeof(el) - 8; e++)
      p += snprintf(el + p, sizeof(el) - (size_t)p, "%s%d", e ? "," : "", eligible[e]);
    char ord[96];
    p = 0;
    for (int e = 0; e < ne && p < (int)sizeof(ord) - 8; e++)
      p += snprintf(ord + p, sizeof(ord) - (size_t)p, "%s%d", e ? "," : "", order[e]);
    tai_ai_trace(pid,
                 "AssignIdleWorkers: ne=%d eligible=[%s] scarcity_order=[%s] max_workers=%d idle_w=%d mining_rr=%u",
                 ne, el, ord, max_workers, state.idle_workers_len, (unsigned)mining_rr);
  }

  bool any_mining = false;
  for (int i = 0; i < max_workers; i++) {
    TWORKER_UNIT *w = state.idle_workers[i];
    int start = (int)((mining_rr + (unsigned)i) % (unsigned)ne);
    bool started = false;
    for (int t = 0; t < ne && !started; t++) {
      int mat = order[(start + t) % ne];
      TSOURCE_UNIT *src = FindNearestMineableSource(w, mat);
      if (g_tai_think_trace_log && player) {
        if (!src)
          tai_ai_trace(pid, "  worker[%d] mat=%d: no mineable source", i, mat);
        else {
          TSOURCE_ITEM *sit = static_cast<TSOURCE_ITEM *>(src->GetPointerToItem());
          const char *sname = sit && sit->name ? sit->name : "?";
          tai_ai_trace(pid, "  worker[%d] try mat=%d src=%s", i, mat, sname);
        }
      }
      if (src && w->StartMine(src, true)) {
        started = true;
        any_mining = true;
        if (g_tai_think_trace_log && player) {
          TSOURCE_ITEM *sit = static_cast<TSOURCE_ITEM *>(src->GetPointerToItem());
          const char *sname = sit && sit->name ? sit->name : "?";
          tai_ai_trace(pid, "  worker[%d] StartMine(%s) ok", i, sname);
        }
      } else if (src && g_tai_think_trace_log && player) {
        tai_ai_trace(pid, "  worker[%d] StartMine failed (mat=%d)", i, mat);
      }
    }
    if (!started && g_tai_think_trace_log && player)
      tai_ai_trace(pid, "  worker[%d] no mining started", i);
  }
  return any_mining;
}

int TAI_CONTROLLER::ManageFactories(TAI_BUILD_GOAL prod_goal, int max_orders)
{
  /* No global energy gate: a town hall (min_energy 0) must keep training workers while the
   * barracks is short of energy; per-product energy is checked by TAI_PredictProduceBlocker. */
  const int pid = player ? (int)player->GetPlayerID() : -1;
  int placed = 0;

  if (prod_goal == BG_FORCE) {
    int mil_idx[TAI_GAME_STATE::kMaxIdleFactories];
    int nm = 0;
    for (int i = 0; i < state.idle_factories_len; i++) {
      TFACTORY_UNIT *f = state.idle_factories[i];
      if (!f->CanAddUnitToOrder(NULL)) {
        if (g_tai_think_trace_log && player) {
          TFACTORY_ITEM *fitem = static_cast<TFACTORY_ITEM *>(f->GetPointerToItem());
          const char *fn = fitem && fitem->name ? fitem->name : "?";
          tai_ai_trace((int)player->GetPlayerID(),
                       "ManageFactories(%s): skip \"%s\" (queue full, under construction, or invalid state)",
                       GoalName(prod_goal), fn);
        }
        continue;
      }
      TFACTORY_ITEM *fi = static_cast<TFACTORY_ITEM *>(f->GetPointerToItem());
      if (!FactoryProducesMilitary(fi))
        continue;
      mil_idx[nm++] = i;
    }

    if (nm > 0) {
      float pmin = 0.f, pmax = 0.f;
      TaiPlayerMilitaryProductScoreRange(player, &pmin, &pmax);
      const float spread = pmax - pmin;
      const bool multi_tier = spread >= 20.f;
      const float heavy_thr = pmin + spread * 0.55f;
      int n_light = 0, n_heavy = 0;
      TaiCountMilitaryForcesLightHeavy(player, heavy_thr, &n_light, &n_heavy);
      const bool need_heavy_quota = multi_tier && n_light >= 10 && (n_heavy * 10 < n_light);
      if (g_tai_think_trace_log && player && need_heavy_quota)
        tai_ai_trace(pid, "military quota: light=%d heavy=%d -> try heavy-only first (thr~%.0f)", n_light, n_heavy,
                     heavy_thr);

      std::sort(mil_idx, mil_idx + nm, [&](int ia, int ib) {
        TFACTORY_ITEM *fa = static_cast<TFACTORY_ITEM *>(state.idle_factories[ia]->GetPointerToItem());
        TFACTORY_ITEM *fb = static_cast<TFACTORY_ITEM *>(state.idle_factories[ib]->GetPointerToItem());
        return TaiFactoryBestMilitaryScore(fa) > TaiFactoryBestMilitaryScore(fb);
      });
      const unsigned off = factory_military_rr % (unsigned)nm;
      const int quota_passes = need_heavy_quota ? 2 : 1;
      for (int qpass = 0; qpass < quota_passes && placed < max_orders; qpass++) {
        const bool heavy_only = need_heavy_quota && (qpass == 0);
        for (int k = 0; k < nm && placed < max_orders; k++) {
          const int i = mil_idx[(size_t)((off + (unsigned)k) % (unsigned)nm)];
          TFACTORY_UNIT *f = state.idle_factories[i];
          TFACTORY_ITEM *fi = static_cast<TFACTORY_ITEM *>(f->GetPointerToItem());

          struct Cand {
            TFORCE_ITEM *p;
            float score;
          };
          Cand cands[32];
          int nc = 0;
          for (TPRODUCEABLE_NODE *pn = fi->GetProductsList().GetFirstNode(); pn; pn = pn->GetNextNode()) {
            TFORCE_ITEM *product = pn->GetProduceableItem();
            if (!product || product->GetItemType() == IT_WORKER)
              continue;
            const float sc = TaiMilitaryUnitTrainingScore(product);
            if (sc < 0.f || nc >= 32)
              continue;
            cands[nc].p = product;
            cands[nc].score = sc;
            nc++;
          }
          if (heavy_only && multi_tier) {
            int nw = 0;
            for (int x = 0; x < nc; x++) {
              if (cands[x].score >= heavy_thr)
                cands[nw++] = cands[x];
            }
            nc = nw;
          }
          if (nc == 0)
            continue;
          std::sort(cands, cands + nc, [](const Cand &a, const Cand &b) { return a.score > b.score; });

          bool ordered = false;
          /* Variability: weighted random pick among affordable products instead of always the heaviest.
           * Aggressive personalities lean harder towards heavy units (higher exponent). */
          {
            float w[32];
            const float max_sc = cands[0].score > 0.f ? cands[0].score : 1.f;
            const float expo = 0.5f + personality.flavor.aggressivity;
            for (int j = 0; j < nc; j++) {
              const int pb = TAI_PredictProduceBlocker(player, f, cands[j].p);
              w[j] = (pb >= 0) ? 0.f : std::pow(std::max(cands[j].score, 1.f) / max_sc, expo);
              if (pb >= 0 && g_tai_think_trace_log && player)
                tai_ai_trace(pid, "ManageFactories: want %s @ %s but blocked (%s)",
                             cands[j].p->name ? cands[j].p->name : "?", fi->name ? fi->name : "?",
                             pb == 0 ? "food" : (pb == 1 ? "energy" : "material"));
            }
            for (int attempt = 0; attempt < nc && !ordered; attempt++) {
              const int j = rng.PickWeighted(w, nc);
              if (w[j] <= 0.f)
                break;
              if (f->AddUnitToOrder(cands[j].p)) {
                placed++;
                ordered = true;
                if (g_tai_think_trace_log && player)
                  tai_ai_trace(pid, "ManageFactories(%s): queue %s @ %s (%s weighted)", GoalName(prod_goal),
                               cands[j].p->name ? cands[j].p->name : "?", fi->name ? fi->name : "?",
                               heavy_only ? "quota-heavy" : "mix");
              } else
                w[j] = 0.f;
            }
          }
          if (!ordered && nc > 0) {
            std::sort(cands, cands + nc, [](const Cand &a, const Cand &b) { return a.score < b.score; });
            for (int j = 0; j < nc && !ordered; j++) {
              TFORCE_ITEM *product = cands[j].p;
              const int pb = TAI_PredictProduceBlocker(player, f, product);
              if (pb >= 0)
                continue;
              if (f->AddUnitToOrder(product)) {
                placed++;
                ordered = true;
                if (g_tai_think_trace_log && player) {
                  const char *pnm = product->name ? product->name : "?";
                  const char *fn = fi->name ? fi->name : "?";
                  tai_ai_trace(pid, "ManageFactories(%s): queue %s @ %s (affordable-light)", GoalName(prod_goal), pnm, fn);
                }
              }
            }
          }
          if (!ordered && nc > 0) {
            std::sort(cands, cands + nc, [](const Cand &a, const Cand &b) { return a.score > b.score; });
            for (int j = 0; j < nc && !ordered; j++) {
              if (f->AddUnitToOrder(cands[j].p)) {
                placed++;
                ordered = true;
                if (g_tai_think_trace_log && player) {
                  const char *pnm = cands[j].p->name ? cands[j].p->name : "?";
                  const char *fn = fi->name ? fi->name : "?";
                  tai_ai_trace(pid, "ManageFactories(%s): queue %s @ %s (force-queue)", GoalName(prod_goal), pnm, fn);
                }
              }
            }
          }
        }
      }
    }
    factory_military_rr++;
    return placed;
  }

  for (int i = 0; i < state.idle_factories_len && placed < max_orders; i++) {
    TFACTORY_UNIT *f = state.idle_factories[i];
    if (!f->CanAddUnitToOrder(NULL)) {
      if (g_tai_think_trace_log && player) {
        TFACTORY_ITEM *fitem = static_cast<TFACTORY_ITEM *>(f->GetPointerToItem());
        const char *fn = fitem && fitem->name ? fitem->name : "?";
        tai_ai_trace((int)player->GetPlayerID(),
                     "ManageFactories(%s): skip \"%s\" (queue full, under construction, or invalid state)",
                     GoalName(prod_goal), fn);
      }
      continue;
    }

    TFACTORY_ITEM *fi = static_cast<TFACTORY_ITEM *>(f->GetPointerToItem());

    for (TPRODUCEABLE_NODE *pn = fi->GetProductsList().GetFirstNode(); pn; pn = pn->GetNextNode()) {
      TFORCE_ITEM *product = pn->GetProduceableItem();
      if (!product)
        continue;
      bool is_worker = (product->GetItemType() == IT_WORKER);
      if (prod_goal != BG_WORKER || !is_worker)
        continue;
      const int pb = TAI_PredictProduceBlocker(player, f, product);
      if (pb >= 0 && g_tai_think_trace_log && player) {
        if (pb == 0)
          tai_ai_trace(pid,
                       "ManageFactories: want %s @ %s but food balance too low for first payment -> next: BG_FARM if "
                       "enabled, else gather / wait",
                       product->name ? product->name : "?", fi->name ? fi->name : "?");
        else if (pb == 1)
          tai_ai_trace(pid,
                       "ManageFactories: want %s @ %s but energy%% below factory min -> more generators / reduce drain",
                       product->name ? product->name : "?", fi->name ? fi->name : "?");
        else if (pb >= 2) {
          int mid = pb - 2;
          const char *mn = (mid >= 0 && mid < scheme.materials_count && scheme.materials[mid])
                               ? scheme.materials[mid]->name
                               : "?";
          tai_ai_trace(pid,
                       "ManageFactories: want %s @ %s but short on %s for first payment -> mine / trade that material",
                       product->name ? product->name : "?", fi->name ? fi->name : "?", mn);
        }
      }
      if (f->AddUnitToOrder(product)) {
        placed++;
        if (g_tai_think_trace_log && player) {
          const char *pnm = product->name ? product->name : "?";
          const char *fn = fi->name ? fi->name : "?";
          tai_ai_trace(pid, "ManageFactories(%s): queue %s @ %s", GoalName(prod_goal), pnm, fn);
        }
        break;
      }
    }
  }
  return placed;
}

bool TAI_CONTROLLER::ManageBuilding(TAI_BUILD_GOAL goal)
{
  if (goal == BG_NONE || goal == BG_WORKER || goal == BG_FORCE)
    return false;
  if (!player)
    return false;

  TWORKER_UNIT *w = AcquireWorkerForConstruction(goal);
  if (!w)
    return false;
  TWORKER_ITEM *wi = static_cast<TWORKER_ITEM *>(w->GetPointerToItem());
  if (!wi)
    return false;

  typedef TLIST<TBUILDING_ITEM>::TNODE<TBUILDING_ITEM> BNode;
  /* BG_RESOURCE_BLDG: can_build order often lists workshop (wood-only) before town hall (gold+wood).
   * Prefer depots that accept more material types first so the CPU gets a full warehouse. */
  const int max_pass = (goal == BG_RESOURCE_BLDG) ? 2 : 1;
  for (int pass = 0; pass < max_pass; pass++) {
    for (BNode *node = wi->build_list.GetFirst(); node; node = node->GetNext()) {
      TBUILDING_ITEM *bi = node->GetPitem();
      if (!MatchesGoal(bi, goal))
        continue;
      if (goal == BG_RESOURCE_BLDG) {
        int kinds = CountUnloadMaterialKinds(bi);
        if (pass == 0 && kinds < 2)
          continue;
        if (pass == 1 && kinds >= 2)
          continue;
      }

      TPOSITION build_pos;
      if (!FindBuildPosition(bi, build_pos)) {
        if (g_tai_think_trace_log && player) {
          const char *bn = bi->name ? bi->name : "?";
          tai_ai_trace((int)player->GetPlayerID(), "ManageBuilding(%s): skip %s (no build position)", GoalName(goal), bn);
        }
        continue;
      }
      if (!w->CanBuild(bi, build_pos, NULL, false, true)) {
        if (g_tai_think_trace_log && player) {
          const int pid = (int)player->GetPlayerID();
          const char *bn = bi->name ? bi->name : "?";
          if (!TAI_BuildingMaterialsMet(player, bi) || !TAI_BuildingFoodOk(player, bi))
            TAI_TraceBuildingStockBlockers(pid, bi, player);
          else
            tai_ai_trace(pid, "ManageBuilding(%s): skip %s (terrain, fog, upgrade-on-ancestor, or other map rule)",
                         GoalName(goal), bn);
        }
        continue;
      }
      TBUILDING_UNIT *site = w->StartBuild(bi, build_pos, true);
      if (site) {
        w->StartRepair(static_cast<TBASIC_UNIT *>(site), true);
        if (g_tai_think_trace_log && player) {
          const char *bn = bi->name ? bi->name : "?";
          tai_ai_trace((int)player->GetPlayerID(), "ManageBuilding(%s): StartBuild %s ok at (%d,%d)", GoalName(goal), bn,
                       (int)build_pos.x, (int)build_pos.y);
        }
        return true;
      }
    }
  }
  return false;
}

static int CountWorkersBuildingSite(TPLAYER *p, TBASIC_UNIT *site)
{
  int n = 0;
  for (TPLAYER_UNIT *u = p->units; u; u = u->GetNext()) {
    if (!u->TestItemType(IT_WORKER))
      continue;
    if (u->TestState(US_DYING) || u->TestState(US_ZOMBIE) || u->TestState(US_DELETE))
      continue;
    TWORKER_UNIT *w = static_cast<TWORKER_UNIT *>(u);
    if (w->GetBuiltOrRepairedUnit() == site)
      n++;
  }
  return n;
}

int TAI_CONTROLLER::AssistUnfinishedConstruction(int max_assign)
{
  if (!player || max_assign <= 0)
    return 0;

  const int cap = level->GetMaxConstructionHelpers();
  int done = 0;

  while (done < max_assign && state.idle_workers_len > 0) {
    TBUILDING_UNIT *best_site = NULL;
    TWORKER_UNIT *best_worker = NULL;
    int best_d2 = 0x7fffffff;

    for (TPLAYER_UNIT *u = player->units; u; u = u->GetNext()) {
      if (u->TestState(US_DYING) || u->TestState(US_ZOMBIE) || u->TestState(US_DELETE))
        continue;
      if (!u->TestItemType(IT_BUILDING) && !u->TestItemType(IT_FACTORY))
        continue;

      TBUILDING_UNIT *site = static_cast<TBUILDING_UNIT *>(u);
      if (!site->TestState(US_IS_BEING_BUILT) || site->GetPrepayed() <= 0.f)
        continue;

      if (CountWorkersBuildingSite(player, static_cast<TBASIC_UNIT *>(site)) >= cap)
        continue;

      TPOSITION_3D sp = site->GetPosition();
      for (int i = 0; i < state.idle_workers_len; i++) {
        TWORKER_UNIT *w = state.idle_workers[i];
        TPOSITION_3D wp = w->GetPosition();
        int dx = wp.x - sp.x;
        int dy = wp.y - sp.y;
        int d2 = dx * dx + dy * dy;
        if (d2 < best_d2) {
          best_d2 = d2;
          best_site = site;
          best_worker = w;
        }
      }
    }

    if (!best_site || !best_worker)
      break;
    if (!best_worker->CanBuildOrRepair(static_cast<TBASIC_UNIT *>(best_site), false, true))
      break;
    if (!best_worker->StartRepair(static_cast<TBASIC_UNIT *>(best_site), true))
      break;

    done++;
    state.ScanFromPlayer(player);
  }

  return done;
}

int TAI_CONTROLLER::AssistDamagedFriendlyStructures(int max_assign)
{
  if (!player || max_assign <= 0)
    return 0;

  const int cap = level->GetMaxConstructionHelpers();
  int done = 0;

  while (done < max_assign && state.idle_workers_len > 0) {
    TBASIC_UNIT *best_site = NULL;
    TWORKER_UNIT *best_worker = NULL;
    int best_d2 = 0x7fffffff;

    for (TPLAYER_UNIT *u = player->units; u; u = u->GetNext()) {
      if (u->TestState(US_DYING) || u->TestState(US_ZOMBIE) || u->TestState(US_DELETE))
        continue;
      if (!u->TestItemType(IT_BUILDING) && !u->TestItemType(IT_FACTORY))
        continue;
      TMAP_UNIT *mu = static_cast<TMAP_UNIT *>(u);
      if (!TaiStructureNeedsRepair(player, mu))
        continue;

      TBASIC_UNIT *site = static_cast<TBASIC_UNIT *>(u);
      if (CountWorkersBuildingSite(player, site) >= cap)
        continue;

      TPOSITION_3D sp = site->GetPosition();
      for (int i = 0; i < state.idle_workers_len; i++) {
        TWORKER_UNIT *w = state.idle_workers[i];
        TPOSITION_3D wp = w->GetPosition();
        int dx = wp.x - sp.x;
        int dy = wp.y - sp.y;
        int d2 = dx * dx + dy * dy;
        if (d2 < best_d2) {
          best_d2 = d2;
          best_site = site;
          best_worker = w;
        }
      }
    }

    if (!best_site || !best_worker)
      break;
    if (!best_worker->CanBuildOrRepair(best_site, false, true))
      break;
    if (!best_worker->StartRepair(best_site, true))
      break;

    done++;
    state.ScanFromPlayer(player);
  }

  return done;
}

//! Stock below this is "scarce"; a material is "plentiful" at kTaiRebalanceRich x this.
static const float kTaiRebalanceLow = 400.f;
static const float kTaiRebalanceRich = 3.f;
static const double kTaiRebalanceCooldown = 10.0;

bool TAI_CONTROLLER::RebalanceMiners()
{
  if (!player || game_time - last_rebalance < kTaiRebalanceCooldown)
    return false;
  int miners[SCH_MAX_MATERIALS_COUNT] = {0};
  bool mineable[SCH_MAX_MATERIALS_COUNT];
  for (int m = 0; m < scheme.materials_count; m++)
    mineable[m] = state.has_source[m] && state.can_unload_material[m];
  for (TPLAYER_UNIT *u = player->units; u; u = u->GetNext()) {
    if (!u->TestItemType(IT_WORKER) || !tai_unit_alive(static_cast<TMAP_UNIT *>(u)))
      continue;
    TWORKER_UNIT *w = static_cast<TWORKER_UNIT *>(u);
    const int mat = (int)(signed char)w->GetMaterial();
    if (w->GetAction() == UA_MINE && mat >= 0 && mat < scheme.materials_count)
      miners[mat]++;
  }
  int from = -1, to = -1;
  if (!TAI_PickMinerRebalance(state.materials, miners, mineable, scheme.materials_count, kTaiRebalanceLow,
                              kTaiRebalanceRich, &from, &to))
    return false;
  for (TPLAYER_UNIT *u = player->units; u; u = u->GetNext()) {
    if (!u->TestItemType(IT_WORKER) || !tai_unit_alive(static_cast<TMAP_UNIT *>(u)))
      continue;
    TWORKER_UNIT *w = static_cast<TWORKER_UNIT *>(u);
    /* miners inside a source / unloading are off-map (see AcquireWorkerForConstruction) */
    if (w->GetAction() != UA_MINE || (int)(signed char)w->GetMaterial() != from || !w->IsInMap())
      continue;
    TSOURCE_UNIT *src = FindNearestMineableSource(w, to);
    if (!src)
      return false;
    w->ClearActions();
    if (!w->StartMine(src, true))
      continue;
    last_rebalance = game_time;
    if (g_tai_think_trace_log)
      tai_ai_trace((int)player->GetPlayerID(), "rebalance: miner %d material %d (%.0f) -> %d (%.0f)",
                   (int)w->GetUnitID(), from, state.materials[from], to, state.materials[to]);
    return true;
  }
  return false;
}

void TAI_CONTROLLER::ManageScouting()
{
  if (!player || !player->GetLocalMap())
    return;

  /* Drop dead scouts. */
  int kept = 0;
  for (int i = 0; i < n_scouts; i++) {
    bool alive = false;
    for (TPLAYER_UNIT *u = player->units; u && !alive; u = u->GetNext())
      alive = (int)u->GetUnitID() == scout_ids[i] && tai_unit_alive(static_cast<TMAP_UNIT *>(u));
    if (alive)
      scout_ids[kept++] = scout_ids[i];
  }
  n_scouts = kept;

  int want = (int)std::lround(personality.scout_count);
  want = std::max(1, std::min(2, want));
  /* Scouting must not strip a small army. */
  if (state.force_count < want + 2)
    want = 0;
  if (n_scouts > want)
    n_scouts = want;

  /* Recruit the lightest combat units as scouts. */
  while (n_scouts < want) {
    TFORCE_UNIT *pick = NULL;
    float pick_score = 1e30f;
    for (TPLAYER_UNIT *u = player->units; u; u = u->GetNext()) {
      if (!u->TestItemType(IT_FORCE) || u->TestItemType(IT_WORKER))
        continue;
      TFORCE_UNIT *fu = static_cast<TFORCE_UNIT *>(u);
      if (!tai_unit_alive(fu) || IsScout(fu->GetUnitID()) || IsDefender(fu->GetUnitID()))
        continue;
      const float sc = TaiMilitaryUnitTrainingScore(static_cast<TFORCE_ITEM *>(fu->GetPointerToItem()));
      if (sc < pick_score) {
        pick_score = sc;
        pick = fu;
      }
    }
    if (!pick)
      break;
    scout_ids[n_scouts++] = pick->GetUnitID();
  }

  /* Every scout that stands still gets a new random destination. */
  for (int i = 0; i < n_scouts; i++) {
    TFORCE_UNIT *fu = NULL;
    for (TPLAYER_UNIT *u = player->units; u && !fu; u = u->GetNext())
      if ((int)u->GetUnitID() == scout_ids[i])
        fu = static_cast<TFORCE_UNIT *>(u);
    if (!fu || fu->GetAction() != UA_STAY)
      continue;
    int tx = -1, ty = -1;
    if (rng.Chance(0.5f)) {
      const int foe = ChooseEnemyPlayer();
      if (foe >= 0 && players[foe]->initial_x >= 0) {
        tx = players[foe]->initial_x + rng.Index(7) - 3;
        ty = players[foe]->initial_y + rng.Index(7) - 3;
      }
    }
    if (tx < 0) {
      tx = 2 + rng.Index(std::max(1, (int)map.width - 4));
      ty = 2 + rng.Index(std::max(1, (int)map.height - 4));
    }
    tx = std::max(2, std::min(tx, (int)map.width - 3));
    ty = std::max(2, std::min(ty, (int)map.height - 3));
    TPOSITION_3D goal;
    goal.SetPosition(tx, ty, fu->GetPosition().segment);
    fu->StartMoving(goal, true);
    if (g_tai_think_trace_log)
      tai_ai_trace((int)player->GetPlayerID(), "scout %d -> (%d,%d)", scout_ids[i], tx, ty);
  }
}

static const char *MilStateName(TAI_MIL_STATE s)
{
  switch (s) {
  case MIL_GATHER:
    return "GATHER";
  case MIL_ATTACK:
    return "ATTACK";
  case MIL_RETREAT:
    return "RETREAT";
  default:
    return "?";
  }
}

//! Radius (Chebyshev tiles) around own structures where enemies count as a threat to the base.
static const int kTaiBaseThreatRadius = 12;
//! Rally point distance from base toward the enemy.
static const int kTaiRallyDist = 8;
//! Army considered "at the rally point" within this radius.
static const int kTaiRallyRadius = 4;
//! Local enemy power for retreat decisions is measured around the army centroid.
static const int kTaiLocalRadius = 10;
//! Attack targets are picked near the army (else march to the enemy base).
static const int kTaiTargetRadius = 20;
//! A new attack target must score this much more than the current one (no re-targeting every tick).
static const float kTaiRetargetMargin = 25.f;
//! Retreat lasts at most this long before gathering again.
static const double kTaiRetreatSeconds = 20.0;

TAI_UNIT_SAMPLE TAI_CONTROLLER::SampleUnit(TMAP_UNIT *u) const
{
  TAI_UNIT_SAMPLE s;
  std::memset(&s, 0, sizeof(s));
  if (!u)
    return s;
  TMAP_ITEM *it = static_cast<TMAP_ITEM *>(u->GetPointerToItem());
  s.life = u->GetLife();
  s.max_life = it ? (float)it->GetMaxLife() : s.life;
  if (it && it->GetArmament() && it->GetArmament()->GetOffensive()) {
    const TGUN_POWER pw = it->GetArmament()->GetOffensive()->GetPower();
    s.dps = 0.5f * (float)(pw.min + pw.max);
  }
  s.x = u->GetPosition().x;
  s.y = u->GetPosition().y;
  s.structure = u->TestItemType(IT_BUILDING) || u->TestItemType(IT_FACTORY);
  s.military = u->TestItemType(IT_FORCE) && !u->TestItemType(IT_WORKER);
  TMAP_UNIT *tg = u->GetTarget();
  s.attacking_us = player && tai_unit_alive(tg) && tg->GetPlayerID() == player->GetPlayerID();
  return s;
}

void TAI_CONTROLLER::GetBase(int *x, int *y) const
{
  *x = player->initial_x;
  *y = player->initial_y;
  if (*x >= 0 && *y >= 0)
    return;
  for (TPLAYER_UNIT *u = player->units; u; u = u->GetNext()) {
    TMAP_UNIT *mu = static_cast<TMAP_UNIT *>(u);
    if (!tai_unit_alive(mu))
      continue;
    if (!u->TestItemType(IT_BUILDING) && !u->TestItemType(IT_FACTORY) && *x >= 0)
      continue;
    *x = mu->GetPosition().x;
    *y = mu->GetPosition().y;
    if (u->TestItemType(IT_BUILDING) || u->TestItemType(IT_FACTORY))
      return;
  }
  if (*x < 0)
    *x = map.width / 2;
  if (*y < 0)
    *y = map.height / 2;
}

int TAI_CONTROLLER::ChooseEnemyPlayer() const
{
  if (!players)
    return -1;
  int bx, by;
  GetBase(&bx, &by);
  const int me = (int)player->GetPlayerID();
  int best = -1, best_d = 0x7fffffff;
  const int count = player_array.GetCount();
  for (int i = 1; i < count && i < PL_MAX_PLAYERS; i++) {
    if (i == me || !players[i] || !players[i]->active)
      continue;
    int d = 0x7fffff;
    if (players[i]->initial_x >= 0 && players[i]->initial_y >= 0)
      d = tai_cheb_xy(bx, by, players[i]->initial_x, players[i]->initial_y);
    if (d < best_d) {
      best_d = d;
      best = i;
    }
  }
  return best;
}

bool TAI_CONTROLLER::IsScout(int unit_id) const
{
  for (int i = 0; i < n_scouts; i++)
    if (scout_ids[i] == unit_id)
      return true;
  return false;
}

bool TAI_CONTROLLER::IsDefender(int unit_id) const
{
  for (int i = 0; i < n_defenders; i++)
    if (defender_ids[i] == unit_id)
      return true;
  return false;
}

int TAI_CONTROLLER::CollectArmy(TFORCE_UNIT **out, int max_out) const
{
  int n = 0;
  for (TPLAYER_UNIT *u = player->units; u && n < max_out; u = u->GetNext()) {
    if (!u->TestItemType(IT_FORCE) || u->TestItemType(IT_WORKER))
      continue;
    TFORCE_UNIT *fu = static_cast<TFORCE_UNIT *>(u);
    if (!tai_unit_alive(fu))
      continue;
    const int id = fu->GetUnitID();
    if (IsScout(id) || IsDefender(id))
      continue;
    out[n++] = fu;
  }
  return n;
}

void TAI_CONTROLLER::ScanEnemies()
{
  n_enemies = 0;
  visible_enemy_power = 0.f;
  if (!player || !player->GetLocalMap())
    return;

  /* Own structures: threat radius is measured from them. */
  int sx[64], sy[64], ns = 0;
  for (TPLAYER_UNIT *u = player->units; u && ns < 64; u = u->GetNext()) {
    if (!u->TestItemType(IT_BUILDING) && !u->TestItemType(IT_FACTORY))
      continue;
    TMAP_UNIT *mu = static_cast<TMAP_UNIT *>(u);
    if (!tai_unit_alive(mu))
      continue;
    sx[ns] = mu->GetPosition().x;
    sy[ns] = mu->GetPosition().y;
    ns++;
  }

  const T_BYTE me = player->GetPlayerID();
  for (int seg = 0; seg < DAT_SEGMENTS_COUNT; seg++) {
    for (T_SIMPLE x = 0; x < map.width; x++) {
      for (T_SIMPLE y = 0; y < map.height; y++) {
        TMAP_UNIT *u = map.segments[seg].surface[x][y].unit;
        if (!u || n_enemies >= kMaxEnemies)
          continue;
        const T_BYTE pid = u->GetPlayerID();
        if (pid == 0 || pid == me || !tai_unit_alive(u))
          continue;
        if (!player->GetLocalMap()->GetAreaVisibility(u->GetPosition(), u->GetUnitWidth(), u->GetUnitHeight()))
          continue;
        bool dup = false; /* multi-tile units appear on several fields */
        for (int k = n_enemies - 1; k >= 0 && !dup; k--)
          dup = (enemy_units[k] == u);
        if (dup)
          continue;

        TAI_UNIT_SAMPLE s = SampleUnit(u);
        bool near = false;
        for (int k = 0; k < ns && !near; k++)
          near = tai_cheb_xy(s.x, s.y, sx[k], sy[k]) <= kTaiBaseThreatRadius;
        enemy_units[n_enemies] = u;
        enemy_samples[n_enemies] = s;
        enemy_near_base[n_enemies] = near;
        n_enemies++;
        if (s.attacking_us)
          retaliation.Hit((int)pid, game_time);
      }
    }
  }

  TAI_UNIT_SAMPLE armed[kMaxEnemies];
  int na = 0;
  for (int i = 0; i < n_enemies; i++)
    if (enemy_samples[i].dps > 0.f)
      armed[na++] = enemy_samples[i];
  visible_enemy_power = TAI_ArmyPower(armed, na);
  if (visible_enemy_power > 0.f) {
    remembered_enemy_power = std::max(remembered_enemy_power * 0.9f, visible_enemy_power);
    enemy_seen_at = game_time;
  }
}

void TAI_CONTROLLER::ManageDefense()
{
  n_defenders = 0;
  if (!player)
    return;

  TAI_UNIT_SAMPLE threats[kMaxEnemies];
  int nt = 0, best = -1;
  float best_score = -1e30f;
  int bx, by;
  GetBase(&bx, &by);
  for (int i = 0; i < n_enemies; i++) {
    const TAI_UNIT_SAMPLE &s = enemy_samples[i];
    if (!enemy_near_base[i] || (!s.military && !s.attacking_us))
      continue;
    threats[nt++] = s;
    const float sc = TAI_TargetScore(s, (float)tai_cheb_xy(s.x, s.y, bx, by));
    if (sc > best_score) {
      best_score = sc;
      best = i;
    }
  }
  if (nt == 0 || best < 0)
    return;

  TMAP_UNIT *target = enemy_units[best];
  const int tx = enemy_samples[best].x, ty = enemy_samples[best].y;

  TFORCE_UNIT *cand[TAI_GAME_STATE::kMaxIdleForces];
  int nc = 0;
  for (TPLAYER_UNIT *u = player->units; u && nc < TAI_GAME_STATE::kMaxIdleForces; u = u->GetNext()) {
    if (!u->TestItemType(IT_FORCE) || u->TestItemType(IT_WORKER))
      continue;
    TFORCE_UNIT *fu = static_cast<TFORCE_UNIT *>(u);
    if (tai_unit_alive(fu) && !IsScout(fu->GetUnitID()))
      cand[nc++] = fu;
  }
  std::sort(cand, cand + nc, [&](TFORCE_UNIT *a, TFORCE_UNIT *b) {
    return tai_cheb_xy(a->GetPosition().x, a->GetPosition().y, tx, ty)
           < tai_cheb_xy(b->GetPosition().x, b->GetPosition().y, tx, ty);
  });
  float powers[TAI_GAME_STATE::kMaxIdleForces];
  for (int i = 0; i < nc; i++) {
    TAI_UNIT_SAMPLE s = SampleUnit(cand[i]);
    powers[i] = TAI_ArmyPower(&s, 1);
  }
  const float threat_power = TAI_ArmyPower(threats, nt);
  const int k = TAI_DefenseCommitCount(threat_power, powers, nc, personality.defense_commit);
  for (int i = 0; i < k; i++) {
    defender_ids[n_defenders++] = cand[i]->GetUnitID();
    if (cand[i]->GetTarget() != target)
      cand[i]->StartAttacking(target, true);
  }
  if (g_tai_think_trace_log)
    tai_ai_trace((int)player->GetPlayerID(), "defense: threats=%d power=%.0f -> %d of %d defenders", nt,
                 threat_power, k, nc);
}

void TAI_CONTROLLER::OrderGroup(TFORCE_UNIT **forces, int n, int x, int y, TMAP_UNIT *attack_target)
{
  if (n <= 0 || !map.IsInMap(x, y))
    return;
  /* Units close to the target attack it directly; only the others share one group path (individual
   * StartMoving would override it, and re-pathing units already in combat interrupts them). */
  TFORCE_UNIT *movers[TAI_GAME_STATE::kMaxIdleForces];
  bool close_flags[TAI_GAME_STATE::kMaxIdleForces];
  int nm = 0;
  for (int i = 0; i < n && i < TAI_GAME_STATE::kMaxIdleForces; i++) {
    close_flags[i] = attack_target
                     && tai_cheb_dist(forces[i]->GetPosition(), attack_target->GetPosition())
                            <= kTaiAssaultReleaseAttackDist;
    if (!close_flags[i])
      movers[nm++] = forces[i];
  }
  const bool grouped = nm >= 2 && tai_request_group_move_forces(player, movers, nm, x, y);
  for (int i = 0; i < n && i < TAI_GAME_STATE::kMaxIdleForces; i++) {
    TFORCE_UNIT *fu = forces[i];
    const bool close = close_flags[i];
    if (close) {
      if (fu->GetTarget() != attack_target)
        fu->StartAttacking(attack_target, true);
    } else if (!grouped) {
      if (attack_target)
        fu->StartAttacking(attack_target, true);
      else {
        TPOSITION_3D goal;
        goal.SetPosition(x, y, fu->GetPosition().segment);
        fu->StartMoving(goal, true);
      }
    }
  }
}

void TAI_CONTROLLER::SetMilState(TAI_MIL_STATE s, float my_power, float enemy_power, int n)
{
  if (s == mil_state)
    return;
  if (g_tai_phase_transition_log)
    fprintf(stderr, "Player %d (%s) military %s -> %s (my=%.0f enemy=%.0f n=%d t=%.0fs)\n",
            (int)player->GetPlayerID(), player->name, MilStateName(mil_state), MilStateName(s), my_power,
            enemy_power, n, game_time);
  mil_state = s;
  mil_state_since = game_time;
  army_order.Reset();
  rally_sent.Reset(-1, -1);
  attack_target_id = -1;
}

void TAI_CONTROLLER::ManageArmy()
{
  TFORCE_UNIT *army[TAI_GAME_STATE::kMaxIdleForces];
  const int n = CollectArmy(army, TAI_GAME_STATE::kMaxIdleForces);
  if (n == 0) {
    SetMilState(MIL_GATHER, 0.f, visible_enemy_power, 0);
    return;
  }

  const int enemy_pid = retaliation.Active(game_time) ? retaliation.Target() : ChooseEnemyPlayer();
  if (enemy_pid < 0 || !players[enemy_pid] || !players[enemy_pid]->active) {
    retaliation.Clear();
    SetMilState(MIL_GATHER, 0.f, 0.f, n);
    return;
  }
  const int ex = players[enemy_pid]->initial_x, ey = players[enemy_pid]->initial_y;
  int bx, by, rx, ry;
  GetBase(&bx, &by);
  TAI_RallyPoint(bx, by, ex, ey, kTaiRallyDist, map.width, map.height, &rx, &ry);

  TAI_UNIT_SAMPLE samples[TAI_GAME_STATE::kMaxIdleForces];
  long cx = 0, cy = 0;
  for (int i = 0; i < n; i++) {
    samples[i] = SampleUnit(army[i]);
    cx += samples[i].x;
    cy += samples[i].y;
  }
  cx /= n;
  cy /= n;
  const float my_power = TAI_ArmyPower(samples, n);
  const float est = TAI_EnemyPowerEstimate(visible_enemy_power, remembered_enemy_power,
                                           (float)(game_time - enemy_seen_at));

  switch (mil_state) {
  case MIL_GATHER: {
    if (!rally_sent.SameDestination(rx, ry))
      rally_sent.Reset(rx, ry);
    TFORCE_UNIT *far[TAI_GAME_STATE::kMaxIdleForces];
    int nf = 0;
    for (int i = 0; i < n; i++)
      if (army[i]->GetAction() == UA_STAY && !rally_sent.Sent(army[i]->GetUnitID())
          && tai_cheb_xy(samples[i].x, samples[i].y, rx, ry) > kTaiRallyRadius) {
        far[nf++] = army[i];
        rally_sent.Add(army[i]->GetUnitID());
      }
    OrderGroup(far, nf, rx, ry, NULL);

    const TAI_PHASE &ph = strategy->GetPhase(current_phase);
    const bool may_attack = ph.targets.attack_when_ready || retaliation.Active(game_time);
    if (may_attack && TAI_ShouldAttack(my_power, est, n, personality, est > 0.f))
      SetMilState(MIL_ATTACK, my_power, est, n);
    break;
  }

  case MIL_ATTACK: {
    TAI_UNIT_SAMPLE local[kMaxEnemies];
    int nl = 0, current = -1;
    for (int i = 0; i < n_enemies; i++) {
      const TAI_UNIT_SAMPLE &s = enemy_samples[i];
      if (tai_cheb_xy(s.x, s.y, (int)cx, (int)cy) <= kTaiLocalRadius && s.dps > 0.f)
        local[nl++] = s;
      if ((int)enemy_units[i]->GetUnitID() == attack_target_id)
        current = i;
    }
    const float local_power = TAI_ArmyPower(local, nl);
    if (TAI_ShouldRetreat(my_power, local_power, personality)) {
      SetMilState(MIL_RETREAT, my_power, local_power, n);
      break;
    }
    if (n < std::max(2, personality.rally_size / 2)) {
      SetMilState(MIL_GATHER, my_power, local_power, n);
      break;
    }
    /* Near target with hysteresis; otherwise any visible enemy on the map (never idle at an empty base). */
    const int best = TAI_PickTarget(enemy_samples, n_enemies, (int)cx, (int)cy, kTaiTargetRadius, current,
                                    kTaiRetargetMargin);
    if (best >= 0) {
      TMAP_UNIT *target = enemy_units[best];
      attack_target_id = (int)target->GetUnitID();
      if (army_order.Changed(MIL_ATTACK, attack_target_id, 0, 0))
        OrderGroup(army, n, enemy_samples[best].x, enemy_samples[best].y, target);
      else /* reinforcements and units that arrived: engage when close */
        for (int i = 0; i < n; i++)
          if (!tai_skip_assault_attack_for_approach(army[i], target) && army[i]->GetTarget() != target)
            army[i]->StartAttacking(target, true);
    } else if (ex >= 0 && ey >= 0) {
      attack_target_id = -1;
      if (army_order.Changed(MIL_ATTACK, -1, ex, ey))
        OrderGroup(army, n, ex, ey, NULL);
      else {
        TFORCE_UNIT *idle[TAI_GAME_STATE::kMaxIdleForces];
        int ni = 0;
        for (int i = 0; i < n; i++)
          if (army[i]->GetAction() == UA_STAY && tai_cheb_xy(samples[i].x, samples[i].y, ex, ey) > kTaiRallyRadius)
            idle[ni++] = army[i];
        OrderGroup(idle, ni, ex, ey, NULL);
      }
    } else {
      SetMilState(MIL_GATHER, my_power, 0.f, n);
    }
    break;
  }

  case MIL_RETREAT: {
    if (army_order.Changed(MIL_RETREAT, -1, rx, ry))
      OrderGroup(army, n, rx, ry, NULL);
    int home = 0;
    for (int i = 0; i < n; i++)
      if (tai_cheb_xy(samples[i].x, samples[i].y, rx, ry) <= kTaiRallyRadius + 1)
        home++;
    if (game_time - mil_state_since > kTaiRetreatSeconds || home * 10 >= n * 7)
      SetMilState(MIL_GATHER, my_power, est, n);
    break;
  }
  }

  if (g_tai_think_trace_log)
    tai_ai_trace((int)player->GetPlayerID(), "army: state=%s n=%d my=%.0f est=%.0f visible=%.0f rally=(%d,%d) foe=%d",
                 MilStateName(mil_state), n, my_power, est, visible_enemy_power, rx, ry, enemy_pid);
}

void TAI_CONTROLLER::EmitDiagnosticLines(TAI_LineSink sink, void *user)
{
  if (!sink || !player || !level || !strategy)
    return;

  char buf[768];

  state.ScanFromPlayer(player);

  const TAI_PHASE &ph = strategy->GetPhase(current_phase);
  TAI_PHASE_TARGETS diag_eff = ph.targets;
  if (current_phase == strategy->GetLoopPhase())
    diag_eff = TaiApplyEscalation(ph.targets, target_escalation, strategy->GetFlavorParams());
  TAI_BUILD_GOAL raw = ComputeHighestDeficit(state, diag_eff);
  TAI_BUILD_GOAL eff = raw;
  for (int guard = 0; guard < 8 && eff != BG_NONE && !CanPursueGoal(eff, state); guard++)
    eff = ResolvePrerequisite(eff, state);

  double interval = level->GetThinkInterval();
  double to_next = interval - think_accumulator;
  if (to_next < 0)
    to_next = 0;

  const TAI_FLAVOR_PARAMS &fp = strategy->GetFlavorParams();

  snprintf(buf, sizeof(buf), "--- AI slot %u (%s) ---", (unsigned)player->GetPlayerID(), player->name);
  sink(user, buf);
  snprintf(buf, sizeof(buf), "Phase: %d / %d  \"%s\"  satisfied=%s  enemy_contacted=%s", current_phase,
          strategy->GetPhaseCount() - 1, ph.name ? ph.name : "?",
          ph.IsSatisfied(state) ? "yes" : "no", enemy_contacted ? "yes" : "no");
  sink(user, buf);
  snprintf(buf, sizeof(buf), "AI: level=%s personality=%s attack_ratio=%.2f retreat_ratio=%.2f rally=%d "
          "defense_commit=%.2f scouts=%.1f", TAI_LevelName(level_id), personality.name, personality.attack_ratio,
          personality.retreat_ratio, personality.rally_size, personality.defense_commit, personality.scout_count);
  sink(user, buf);
  snprintf(buf, sizeof(buf), "Strategy loop_phase=%d combat_phase=%d  flavor(agg=%.2f def=%.2f eco=%.2f)",
          strategy->GetLoopPhase(), strategy->GetCombatPhase(), fp.aggressivity, fp.defense_priority,
          fp.econ_focus);
  sink(user, buf);
  snprintf(buf, sizeof(buf),
          "Targets (effective): workers>=%d forces>=%d factories>=%d target_mil_fac>=%d buildings>=%d defense>=%d "
          "train_to_f=%d scout=%.2f attack=%s farms=%s  assault_esc=%d",
          diag_eff.min_workers, diag_eff.min_forces, diag_eff.min_factories,
          diag_eff.target_military_factories, diag_eff.min_buildings, diag_eff.min_defense_buildings,
          diag_eff.train_to_forces, ph.targets.scout_ratio, ph.targets.attack_when_ready ? "yes" : "no",
          ph.targets.build_farms ? "yes" : "no", target_escalation);
  sink(user, buf);
  snprintf(buf, sizeof(buf),
          "Military: state=%s since=%.0fs visible_enemy=%.0f remembered=%.0f retaliation=%s(pid %d) t=%.0fs",
          MilStateName(mil_state), game_time - mil_state_since, visible_enemy_power, remembered_enemy_power,
          retaliation.Active(game_time) ? "on" : "off", retaliation.Target(), game_time);
  sink(user, buf);
  snprintf(buf, sizeof(buf), "Deficit: raw=%s  after_prereq=%s", GoalName(raw), GoalName(eff));
  sink(user, buf);
  snprintf(buf, sizeof(buf),
          "State: workers=%d idle_w=%d forces=%d idle_f=%d factories=%d idle_fac=%d mil_factories=%d has_mil=%s "
          "bld=%d def_bld=%d unload=%s unfinished=%s energy=%s",
          state.worker_count, state.idle_worker_count, state.force_count, state.idle_forces_len,
          state.factory_count, state.idle_factories_len, state.military_factory_count,
          state.has_military_factory ? "y" : "n",
          state.building_count, state.defense_building_count,
          state.has_any_unload_building ? "y" : "n",
          state.has_unfinished_construction ? "y" : "n", state.energy_sufficient ? "y" : "n");
  sink(user, buf);
  {
    int pos = 0;
    pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "Materials:");
    for (int m = 0; m < scheme.materials_count && pos < (int)sizeof(buf) - 20; m++)
      pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, " [%d]=%.0f", m, state.materials[m]);
    sink(user, buf);
  }
  {
    int pos = 0;
    pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "Mining eligible (has_src & can_unload):");
    for (int m = 0; m < scheme.materials_count && pos < (int)sizeof(buf) - 20; m++)
      pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, " [%d]=%s", m,
                        (state.has_source[m] && state.can_unload_material[m]) ? "y" : "n");
    sink(user, buf);
  }
  snprintf(buf, sizeof(buf), "Think: interval=%.2fs  accumulator=%.2fs  next_think_in~%.2fs", interval,
          think_accumulator, to_next);
  sink(user, buf);
  sink(user, "stderr: logs think on | logs think off — per-tick AI trace (deficit, mining, builds).");
  sink(user, "Phase advances at end of a think tick when targets are met; one tick every "
             "'interval' seconds (Easy=3s Medium=1.5s Hard=0.5s by default).");
}

void TAI_CONTROLLER::Think(double dt)
{
  if (!player || !level || !strategy || !player->active)
    return;

  const int phase_at_entry = current_phase;

  think_accumulator += dt;
  if (think_accumulator < level->GetThinkInterval())
    return;
  game_time += think_accumulator;
  think_accumulator = 0;

  state.ScanFromPlayer(player);

  int budget = level->GetMaxActionsPerTick();
  mining_rr++;

  HandleResourceShortage();

  state.ScanFromPlayer(player);

  const TAI_PHASE &phase = strategy->GetPhase(current_phase);
  TAI_PHASE_TARGETS eff_targets = phase.targets;
  if (current_phase == strategy->GetLoopPhase())
    eff_targets = TaiApplyEscalation(phase.targets, target_escalation, strategy->GetFlavorParams());
  const int trace_pid = (int)player->GetPlayerID();
  TAI_BUILD_GOAL raw_deficit = ComputeHighestDeficit(state, eff_targets);
  TAI_BUILD_GOAL effective = raw_deficit;
  for (int guard = 0; guard < 8 && effective != BG_NONE && !CanPursueGoal(effective, state);
       guard++)
    effective = ResolvePrerequisite(effective, state);

  if (g_tai_think_trace_log) {
    tai_ai_trace(trace_pid,
                 "tick phase=%d \"%s\" deficit_raw=%s deficit=%s pursue=%s "
                 "idle_w=%d idle_fac=%d workers=%d forces=%d bld=%d fac=%d def_bld=%d unload=%s budget=%d",
                 current_phase, phase.name ? phase.name : "?", GoalName(raw_deficit), GoalName(effective),
                 (effective == BG_NONE || CanPursueGoal(effective, state)) ? "yes" : "no",
                 state.idle_workers_len, state.idle_factories_len, state.worker_count, state.force_count,
                 state.building_count, state.factory_count, state.defense_building_count,
                 state.has_any_unload_building ? "y" : "n", budget);
  }

  /* Barracks/first military factory before assist+mining: otherwise Easy budget=1 + all miners => never builds. */
  bool barracks_prio_attempted = false;
  bool barracks_prio_placed = false;
  if (budget > 0 && effective == BG_FACTORY && !state.has_military_factory) {
    barracks_prio_attempted = true;
    barracks_prio_placed = ManageBuilding(effective);
    if (barracks_prio_placed)
      budget--;
    state.ScanFromPlayer(player);
  }

  /* With budget=1 (Easy), run construction help before other new blueprints. */
  if (budget > 0) {
    int helped = AssistUnfinishedConstruction(budget);
    if (g_tai_think_trace_log && helped > 0)
      tai_ai_trace(trace_pid, "AssistUnfinishedConstruction: %d worker(s) sent to build sites", helped);
    budget -= helped;
  }

  if (budget > 0) {
    int rep = AssistDamagedFriendlyStructures(budget);
    if (g_tai_think_trace_log && rep > 0)
      tai_ai_trace(trace_pid, "AssistDamagedFriendlyStructures: %d worker(s) repairing", rep);
    budget -= rep;
  }

  state.ScanFromPlayer(player);

  if (budget > 0 && effective != BG_NONE && effective != BG_WORKER && effective != BG_FORCE) {
    if (!barracks_prio_attempted || !barracks_prio_placed) {
      if (ManageBuilding(effective))
        budget--;
    }
  }

  state.ScanFromPlayer(player);

  if (budget > 0 && state.idle_workers_len > 0) {
    int n = level->GetMaxActionsPerTick();
    if (AssignIdleWorkers(n, effective))
      budget--;
  }

  state.ScanFromPlayer(player);

  if (RebalanceMiners())
    state.ScanFromPlayer(player);

  if (budget > 0 && state.idle_factories_len > 0) {
    TAI_BUILD_GOAL prod_raw = ComputeHighestDeficit(state, eff_targets);
    TAI_BUILD_GOAL prod_goal = prod_raw;
    for (int guard = 0; guard < 8 && prod_goal != BG_NONE && !CanPursueGoal(prod_goal, state);
         guard++)
      prod_goal = ResolvePrerequisite(prod_goal, state);
    /* Energy is checked inside ManageFactories; keep prod_goal so trace shows intent. */
    if (prod_goal == BG_NONE && state.has_military_factory && CanPursueGoal(BG_FORCE, state)) {
      const int cap = eff_targets.train_to_forces;
      if (cap > 0 && state.force_count < cap)
        prod_goal = BG_FORCE;
    }
    if (g_tai_think_trace_log) {
      tai_ai_trace(trace_pid, "factories deficit_raw=%s prod_goal=%s pursue=%s energy=%s",
                   GoalName(prod_raw), GoalName(prod_goal),
                   (prod_goal == BG_NONE || CanPursueGoal(prod_goal, state)) ? "yes" : "no",
                   state.energy_sufficient ? "ok" : "low");
    }
    int n = level->AllowMultipleBuilds() ? level->GetMaxActionsPerTick() : 1;
    const int fac_placed = ManageFactories(prod_goal, n);
    if (fac_placed > 0)
      budget--;
    else if (g_tai_think_trace_log && prod_goal != BG_NONE && prod_goal != BG_FACTORY)
      tai_ai_trace(trace_pid, "ManageFactories: placed=0 for prod_goal=%s (idle_fac=%d)", GoalName(prod_goal),
                   state.idle_factories_len);
  }

  state.ScanFromPlayer(player);

  if (g_tai_think_trace_log)
    TraceFactoryProductionNeeds();

  /* Military: one enemy scan per tick, proportional defense first, then the field army state machine. */
  state.ScanFromPlayer(player);
  ScanEnemies();
  ManageScouting();
  ManageDefense();
  ManageArmy();

  state.ScanFromPlayer(player);

  /* Phase advance: 0→1→2→3, then loop_phase (also 3) so endgame does not drop back to militarize. */
  const TAI_PHASE &ph_done = strategy->GetPhase(current_phase);
  if (ph_done.IsSatisfied(state)) {
    if (current_phase < strategy->GetPhaseCount() - 1)
      current_phase++;
    else
      current_phase = strategy->GetLoopPhase();
  }

  if (current_phase == strategy->GetLoopPhase()) {
    TAI_PHASE_TARGETS et = TaiApplyEscalation(strategy->GetPhase(current_phase).targets, target_escalation,
                                              strategy->GetFlavorParams());
    if (ComputeHighestDeficit(state, et) == BG_NONE && target_escalation < kTaiMaxTargetEscalation)
      target_escalation++;
  }

  if (!enemy_contacted && n_enemies > 0) {
    enemy_contacted = true;
    int cp = strategy->GetCombatPhase();
    if (cp > current_phase)
      current_phase = cp;
  }

  if (g_tai_phase_transition_log && current_phase != phase_at_entry) {
    const TAI_PHASE &pn = strategy->GetPhase(current_phase);
    fprintf(stderr, "Player %d (%s) reached new phase %d \"%s\"\n", (int)player->GetPlayerID(),
            player->name, current_phase, pn.name ? pn.name : "?");
  }
}

void TAI_CONTROLLER::TraceFactoryProductionNeeds()
{
  if (!g_tai_think_trace_log || !player)
    return;
  const int pid = (int)player->GetPlayerID();
  for (TPLAYER_UNIT *u = player->units; u; u = u->GetNext()) {
    if (!u->TestItemType(IT_FACTORY))
      continue;
    TFACTORY_UNIT *f = static_cast<TFACTORY_UNIT *>(u);
    if (f->TestState(US_IS_BEING_BUILT) || f->TestState(US_DYING) || f->TestState(US_ZOMBIE)
        || f->TestState(US_DELETE))
      continue;
    if (f->GetOrderSize() <= 0)
      continue;
    signed char nid = f->GetNeedID();
    if (nid < 0)
      continue;
    TFACTORY_ITEM *fitem = static_cast<TFACTORY_ITEM *>(f->GetPointerToItem());
    const char *fn = fitem && fitem->name ? fitem->name : "?";
    if (nid == 0)
      tai_ai_trace(pid,
                   "factory \"%s\": production stalled — food -> next: BG_FARM when phase allows farms; else wait / "
                   "expand food income",
                   fn);
    else if (nid == 1)
      tai_ai_trace(pid,
                   "factory \"%s\": production stalled — energy%% below min -> add power or cut consumers; AI already "
                   "gates on energy_sufficient for new orders",
                   fn);
    else if (nid >= 2) {
      int mid = nid - 2;
      const char *mn =
          (mid >= 0 && mid < scheme.materials_count && scheme.materials[mid]) ? scheme.materials[mid]->name : "?";
      tai_ai_trace(pid, "factory \"%s\": production stalled — material %s -> send miners to that resource", fn, mn);
    }
  }
}

TAI_LEVEL *TAI_CreateLevel(TAI_LEVEL_ID lv)
{
  switch (lv) {
  case TAI_LV_EASY:
    return NEW TAI_LEVEL_EASY();
  case TAI_LV_HARD:
    return NEW TAI_LEVEL_HARD();
  case TAI_LV_MEDIUM:
  default:
    return NEW TAI_LEVEL_MEDIUM();
  }
}

TAI_PLAYER::TAI_PLAYER()
  : controller(NULL), owned_level(NULL), owned_strategy(NULL)
{
  SetPlayerType(PT_COMPUTER);
}

void TAI_PLAYER::EnsureController()
{
  if (controller)
    return;
  const int slot = (int)GetPlayerID();
  int lv = player_array.GetAiLevel(slot);
  if (lv < 0)
    lv = config.ai_level;
  const TAI_LEVEL_ID level_id = (TAI_LEVEL_ID)lv;

  /* Leader-only randomness (AI runs only where the slot is local); never touches rand(). */
  const uint64_t seed = (uint64_t)time(NULL) ^ ((uint64_t)(slot + 1) * 0x9E3779B97F4A7C15ULL)
                        ^ (uint64_t)(uintptr_t)this;
  TAI_RNG rng(seed);
  const TAI_PERSONALITY pers = TAI_RollPersonality(rng);

  owned_level = TAI_CreateLevel(level_id);
  owned_strategy = NEW TAI_STRATEGY(pers.flavor, std::strcmp(pers.name, "rusher") == 0);
  controller = NEW TAI_CONTROLLER(this, owned_level, owned_strategy, pers, level_id, rng.NextU32());
  Info(LogMsg("CPU player %d: level=%s personality=%s", slot, TAI_LevelName(level_id), pers.name));
}

TAI_PLAYER::~TAI_PLAYER()
{
  if (controller) {
    delete controller;
    controller = NULL;
  }
  if (owned_level) {
    delete owned_level;
    owned_level = NULL;
  }
  if (owned_strategy) {
    delete owned_strategy;
    owned_strategy = NULL;
  }
}

void TAI_PLAYER::UpdateAI(double time_shift)
{
  EnsureController();
  if (controller)
    controller->Think(time_shift);
}

void TAI_PLAYER::EmitAIDiagnosticLines(TAI_LineSink sink, void *user)
{
  EnsureController();
  if (controller)
    controller->EmitDiagnosticLines(sink, user);
}

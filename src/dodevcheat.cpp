#include "dodevcheat.h"
#include "dopace.h"

#include "dodraw.h"
#include "doplayers.h"
#include "doschemes.h"
#include "dosimpletypes.h"
#include "dounits.h"

bool dev_fast_timers = false;
TDEV_GOD_MODE dev_god_mode = DEV_GOD_OFF;

void DevCheatsSetRevealMap(bool on)
{
  show_all = on;
}

void DevCheatsApplyResources(TPLAYER *p)
{
  if (!p)
    return;
  for (int i = 0; i < scheme.materials_count; i++)
    p->IncStoredMaterial(i, 10000.f);
}

double DevCheatsEffectiveProductionDelta(double production_time)
{
  if (dev_fast_timers)
    return 1.0 / static_cast<double>(UNI_PRODUCING_COUNT);
  return production_time / PACE_PRODUCTION_SPEED / static_cast<double>(UNI_PRODUCING_COUNT);
}

double DevCheatsEffectiveRepairDelta(double repairing_time, float work_remaining)
{
  double paced = repairing_time / PACE_BUILD_SPEED;
  if (!dev_fast_timers || work_remaining <= 0.f)
    return paced;
  double fast = 1.0 / static_cast<double>(work_remaining);
  return MIN(paced, fast);
}

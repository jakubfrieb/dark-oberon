#include "dodevcheat.h"

#include "dodraw.h"
#include "doplayers.h"
#include "doschemes.h"
#include "dosimpletypes.h"
#include "dounits.h"

bool dev_fast_timers = false;

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
  return production_time / static_cast<double>(UNI_PRODUCING_COUNT);
}

double DevCheatsEffectiveRepairDelta(double repairing_time, float work_remaining)
{
  if (!dev_fast_timers || work_remaining <= 0.f)
    return repairing_time;
  double fast = 1.0 / static_cast<double>(work_remaining);
  return MIN(repairing_time, fast);
}

/*
 * Local developer cheats (single-player / testing). See plan: no network sync.
 */
#pragma once

class TPLAYER;

extern bool dev_fast_timers;

void DevCheatsSetRevealMap(bool on);
void DevCheatsApplyResources(TPLAYER *p);
double DevCheatsEffectiveProductionDelta(double production_time);
double DevCheatsEffectiveRepairDelta(double repairing_time, float work_remaining);

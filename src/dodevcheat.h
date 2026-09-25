/*
 * Local developer cheats (single-player / testing). See plan: no network sync.
 */
#pragma once

class TPLAYER;

extern bool dev_fast_timers;
/** Dev console `god`: whose units take no damage. */
enum TDEV_GOD_MODE { DEV_GOD_OFF, DEV_GOD_ME, DEV_GOD_ALL };
extern TDEV_GOD_MODE dev_god_mode;

void DevCheatsSetRevealMap(bool on);
void DevCheatsApplyResources(TPLAYER *p);
double DevCheatsEffectiveProductionDelta(double production_time);
double DevCheatsEffectiveRepairDelta(double repairing_time, float work_remaining);

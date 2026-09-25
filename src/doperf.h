/*
 * Timing of the game simulation loop (ProcessFunction), for performance tests.
 *
 * Every simulation step records how long it worked: processing the event queue and running
 * the CPU players. Path searches run in a thread pool beside it and are recorded separately
 * (job duration, queue length). The dedicated server prints a summary with `perf`
 * (tests/cpp/perf_smoke.sh).
 */
#pragma once

#include <string>
#include <vector>

//! Target length of one simulation step (ProcessFunction sleeps up to 20 ms per step).
const double PERF_STEP_BUDGET_MS = 20.0;

//! Summary of a series of durations (milliseconds).
struct TPERF_SUMMARY {
  int count;
  double avg, p50, p95, p99, max;
};

//! Average, nearest-rank percentiles and maximum; all zero for an empty series.
TPERF_SUMMARY PerfSummarize(std::vector<double> values);

//! Records one simulation step (seconds spent on events and on the AI, events processed, path
//! searches waiting in the queue).
void PerfRecordStep(double events_s, double ai_s, int events, int path_queue);
//! Records one path search job (runs in the path finding thread pool, not in the step above).
void PerfRecordJob(double seconds);
//! Forgets all recorded steps (e.g. after spawning units and a warm-up).
void PerfReset();
//! Multi-line report of the steps recorded since the last reset; @p units = units on the map.
std::string PerfReport(int units);

#include "doperf.h"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

TPERF_SUMMARY PerfSummarize(std::vector<double> v)
{
  TPERF_SUMMARY s = {0, 0.0, 0.0, 0.0, 0.0, 0.0};
  if (v.empty())
    return s;
  std::sort(v.begin(), v.end());
  s.count = (int)v.size();
  double sum = 0.0;
  for (double x : v)
    sum += x;
  s.avg = sum / v.size();
  // nearest rank: the smallest value with at least p % of the values at or below it
  auto rank = [&](double p) {
    size_t k = (size_t)std::ceil(p / 100.0 * v.size());
    return v[k == 0 ? 0 : k - 1];
  };
  s.p50 = rank(50);
  s.p95 = rank(95);
  s.p99 = rank(99);
  s.max = v.back();
  return s;
}

namespace {

struct TSTEP {
  float events_ms, ai_ms;
  int events, path_queue;
};

// Bounded: 50 steps/s, so this keeps the last 20 minutes.
const size_t kMaxSteps = 60000;

std::vector<TSTEP> g_steps;
size_t g_next = 0;              // ring position once full
std::vector<float> g_jobs;      // path search durations (ms)
size_t g_jobs_next = 0;
double g_first_at = -1.0, g_last_at = -1.0;

SDL_mutex *Lock()
{
  // game thread and path finding threads record concurrently; C++11 makes this init thread-safe
  static SDL_mutex *lock = SDL_CreateMutex();
  return lock;
}

std::string Line(const char *name, const TPERF_SUMMARY &s, const char *extra)
{
  char buf[256];
  snprintf(buf, sizeof(buf), "perf: %-6s ms avg=%.2f p50=%.2f p95=%.2f p99=%.2f max=%.2f%s\n", name, s.avg, s.p50,
           s.p95, s.p99, s.max, extra);
  return buf;
}

} // namespace

void PerfRecordJob(double seconds)
{
  SDL_mutex *m = Lock();
  SDL_LockMutex(m);
  const float ms = (float)(seconds * 1000.0);
  if (g_jobs.size() < kMaxSteps)
    g_jobs.push_back(ms);
  else {
    g_jobs[g_jobs_next] = ms;
    g_jobs_next = (g_jobs_next + 1) % kMaxSteps;
  }
  SDL_UnlockMutex(m);
}

void PerfRecordStep(double events_s, double ai_s, int events, int path_queue)
{
  SDL_mutex *m = Lock();
  SDL_LockMutex(m);
  TSTEP st = {(float)(events_s * 1000.0), (float)(ai_s * 1000.0), events, path_queue};
  if (g_steps.size() < kMaxSteps)
    g_steps.push_back(st);
  else {
    g_steps[g_next] = st;
    g_next = (g_next + 1) % kMaxSteps;
  }
  double now = SDL_GetTicks() / 1000.0;
  if (g_first_at < 0)
    g_first_at = now;
  g_last_at = now;
  SDL_UnlockMutex(m);
}

void PerfReset()
{
  SDL_mutex *m = Lock();
  SDL_LockMutex(m);
  g_steps.clear();
  g_next = 0;
  g_jobs.clear();
  g_jobs_next = 0;
  g_first_at = g_last_at = -1.0;
  SDL_UnlockMutex(m);
}

std::string PerfReport(int units)
{
  SDL_mutex *m = Lock();
  SDL_LockMutex(m);
  std::vector<double> total, events, ai, queue, jobs(g_jobs.begin(), g_jobs.end());
  double n_events = 0;
  int over = 0;
  for (const TSTEP &s : g_steps) {
    queue.push_back(s.path_queue);
    const double t = (double)s.events_ms + s.ai_ms;
    total.push_back(t);
    events.push_back(s.events_ms);
    ai.push_back(s.ai_ms);
    n_events += s.events;
    if (t > PERF_STEP_BUDGET_MS)
      over++;
  }
  const double span = g_last_at > g_first_at ? g_last_at - g_first_at : 0.0;
  SDL_UnlockMutex(m);

  char buf[256];
  std::string out;
  const size_t n = total.size();
  snprintf(buf, sizeof(buf), "perf: %zu steps in %.1f s (%.1f steps/s), units=%d\n", n, span,
           span > 0 ? n / span : 0.0, units);
  out += buf;
  snprintf(buf, sizeof(buf), "  over %.0f ms budget: %.1f %%", PERF_STEP_BUDGET_MS, n ? 100.0 * over / n : 0.0);
  out += Line("step", PerfSummarize(total), buf);
  snprintf(buf, sizeof(buf), "  events/step avg=%.1f", n ? n_events / n : 0.0);
  out += Line("events", PerfSummarize(events), buf);
  out += Line("ai", PerfSummarize(ai), "");
  snprintf(buf, sizeof(buf), "  jobs=%zu (%.1f/s)", jobs.size(), span > 0 ? jobs.size() / span : 0.0);
  out += Line("path", PerfSummarize(jobs), buf);
  const TPERF_SUMMARY q = PerfSummarize(queue);
  snprintf(buf, sizeof(buf), "perf: path queue avg=%.1f p99=%.0f max=%.0f waiting searches\n", q.avg, q.p99, q.max);
  out += buf;
  return out;
}

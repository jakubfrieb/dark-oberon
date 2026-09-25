/*
 * Unit tests for the simulation step statistics (src/doperf.*).
 * Build and run: make test-ai
 */
#include "doperf.h"
#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)
static bool near(double a, double b) { return std::fabs(a - b) < 1e-9; }

int main() {
  // empty series
  TPERF_SUMMARY e = PerfSummarize({});
  CHECK(e.count == 0 && e.avg == 0.0 && e.max == 0.0);

  // 1..100 in random order: nearest-rank percentiles
  std::vector<double> v;
  for (int i = 100; i >= 1; i--) v.push_back(i);
  TPERF_SUMMARY s = PerfSummarize(v);
  CHECK(s.count == 100);
  CHECK(near(s.avg, 50.5));
  CHECK(near(s.p50, 50) && near(s.p95, 95) && near(s.p99, 99) && near(s.max, 100));

  // one slow step shows in max and p99 of a short series, not in the median
  std::vector<double> w(10, 2.0);
  w[3] = 40.0;
  TPERF_SUMMARY t = PerfSummarize(w);
  CHECK(near(t.p50, 2.0) && near(t.max, 40.0) && near(t.p99, 40.0));

  // recorder: steps and report
  PerfReset();
  PerfRecordStep(0.004, 0.001, 12, 0);
  PerfRecordStep(0.030, 0.002, 40, 7);
  PerfRecordJob(0.003);
  std::string r = PerfReport(600);
  CHECK(r.find("jobs=1") != std::string::npos);
  CHECK(r.find("max=7 waiting") != std::string::npos);
  CHECK(r.find("2 steps") != std::string::npos);
  CHECK(r.find("units=600") != std::string::npos);
  CHECK(r.find("50.0 %") != std::string::npos);   // one of two steps over the 20 ms budget

  std::printf("%d failures\n", g_fail);
  return g_fail ? 1 : 0;
}

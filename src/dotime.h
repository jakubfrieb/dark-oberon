//========================================================================
// App monotonic time (SDL2 performance counter). Replaces GLFW glfwGetTime /
// glfwSetTime / glfwSleep for phase 2 migration. Requires SDL_Init(SDL_INIT_TIMER).
//========================================================================
#pragma once

#include <atomic>

#include "dosdl.h"

namespace app_time_detail {

struct Clock {
  Uint64 origin = 0;
  std::atomic<double> base{0.0};   // AppAdjustTimeSeconds() changes it while other threads read
  bool inited = false;

  double get()
  {
    if (!inited) {
      origin = SDL_GetPerformanceCounter();
      inited = true;
      base = 0.0;
    }
    const Uint64 freq = SDL_GetPerformanceFrequency();
    if (freq == 0)
      return base;
    return base + (SDL_GetPerformanceCounter() - origin) / static_cast<double>(freq);
  }

  void set(double t)
  {
    origin = SDL_GetPerformanceCounter();
    base = t;
    inited = true;
  }
};

inline Clock &clock()
{
  static Clock c;
  return c;
}

} // namespace app_time_detail

/** Seconds since clock start, adjusted by AppSetTimeSeconds (GLFW-compatible). */
inline double AppGetTimeSeconds()
{
  return app_time_detail::clock().get();
}

inline void AppSetTimeSeconds(double t)
{
  app_time_detail::clock().set(t);
}

/** Moves the clock by @p delta seconds. Unlike AppSetTimeSeconds it changes one
 *  value only, so threads reading the clock meanwhile never see a torn time. */
inline void AppAdjustTimeSeconds(double delta)
{
  app_time_detail::Clock &c = app_time_detail::clock();
  c.base.store(c.base.load() + delta);
}

/** Seconds of a monotonic clock that AppSetTimeSeconds/AppAdjustTimeSeconds
 *  never change (for measuring intervals such as round trip times). */
inline double AppGetRawSeconds()
{
  const Uint64 freq = SDL_GetPerformanceFrequency();
  return freq ? SDL_GetPerformanceCounter() / static_cast<double>(freq) : 0.0;
}

/** Sleep (millisecond granularity; same role as glfwSleep). */
inline void AppSleepSeconds(double sec)
{
  if (sec <= 0.0)
    return;
  Uint32 ms = static_cast<Uint32>(sec * 1000.0 + 0.5);
  if (ms == 0)
    ms = 1;
  SDL_Delay(ms);
}

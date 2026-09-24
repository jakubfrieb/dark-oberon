//========================================================================
// App monotonic time (SDL2 performance counter). Replaces GLFW glfwGetTime /
// glfwSetTime / glfwSleep for phase 2 migration. Requires SDL_Init(SDL_INIT_TIMER).
//========================================================================
#pragma once

#include "dosdl.h"

namespace app_time_detail {

struct Clock {
  Uint64 origin = 0;
  double base = 0.0;
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

#pragma once

#include <SDL3/SDL.h>

#include <string_view>

namespace flb
{
using TimePoint = Uint64;
using Duration = Uint64;

static const double frequency = static_cast<double>(SDL_GetPerformanceFrequency());

static TimePoint now() { return SDL_GetPerformanceCounter(); }

static double toSeconds(Duration duration) { return static_cast<double>(duration) / frequency; }

static double toMilliseconds(Duration duration) { return (static_cast<double>(duration) * 1000.0) / frequency; }

class Timer
{
public:
  Timer(std::string_view msg) : msg(msg), start(now()) { }

  ~Timer()
  {
    const TimePoint end = now();
    const Duration duration = end - start;
    SDL_Log("%s: %.6f seconds", msg.data(), toSeconds(duration));
  }

private:
  std::string_view msg;
  TimePoint start;
};
} // namespace flb

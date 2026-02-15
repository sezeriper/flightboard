#pragma once

#include <SDL3/SDL.h>

#include <string_view>

namespace flb
{
namespace Time
{
using TimePoint = Uint64;
using Duration = Uint64;

static const double frequency = static_cast<double>(SDL_GetPerformanceFrequency());

static TimePoint now()
{
  return SDL_GetPerformanceCounter();
}

static double toSeconds(Duration duration)
{
  return static_cast<double>(duration) / frequency;
}
}

class Timer
{
public:
  Timer(std::string_view msg) : msg(msg), start(Time::now()) {}

  ~Timer()
  {
    const Time::TimePoint end = Time::now();
    const Time::Duration duration = end - start;
    SDL_Log("%s: %.6f seconds", msg.data(), Time::toSeconds(duration));
  }

private:
  std::string_view msg;
  Time::TimePoint start;
};
}

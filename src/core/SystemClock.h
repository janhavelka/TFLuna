#pragma once

#include <stdint.h>

#ifdef ARDUINO
#include <SystemChrono/SystemChrono.h>
#if __has_include(<SystemChrono/Version.h>)
#include <SystemChrono/Version.h>
static_assert(SystemChrono::VERSION_CODE >= 10200,
              "CO2Control requires SystemChrono >= v1.2.0");
#endif
#else
#include <chrono>
#endif

namespace CO2Control {
namespace SystemClock {

inline uint64_t nowMs64() {
#ifdef ARDUINO
  return SystemChrono::millis64();
#else
  using clock = std::chrono::steady_clock;
  static const clock::time_point start = clock::now();
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start);
  return static_cast<uint64_t>(elapsed.count());
#endif
}

inline uint32_t nowMs() {
#ifdef ARDUINO
  return static_cast<uint32_t>(SystemChrono::millis64());
#else
  return static_cast<uint32_t>(nowMs64());
#endif
}

inline uint32_t nowUs() {
#ifdef ARDUINO
  return static_cast<uint32_t>(SystemChrono::micros64());
#else
  using clock = std::chrono::steady_clock;
  static const clock::time_point start = clock::now();
  const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(clock::now() - start);
  return static_cast<uint32_t>(elapsed.count());
#endif
}

}  // namespace SystemClock
}  // namespace CO2Control

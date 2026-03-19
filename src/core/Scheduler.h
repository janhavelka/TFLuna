#pragma once

#include <stdint.h>

namespace TFLunaControl {

/**
 * @brief Simple periodic scheduler helper.
 */
class PeriodicTimer {
 public:
  /// @brief Set interval in milliseconds.
  void setInterval(uint32_t intervalMs) { _intervalMs = intervalMs; }

  /// @brief Reset schedule based on current time.
  void reset(uint32_t nowMs) {
    _nextMs = nowMs + _intervalMs;
    _initialized = true;
  }

  /// @brief Check if timer is due; advances schedule if due.
  /// @return true if due
  bool isDue(uint32_t nowMs) {
    if (!_initialized) {
      reset(nowMs);
      return false;
    }
    if (static_cast<int32_t>(nowMs - _nextMs) >= 0) {
      _nextMs = nowMs + _intervalMs;
      return true;
    }
    return false;
  }

 private:
  uint32_t _intervalMs = 0;
  uint32_t _nextMs = 0;
  bool _initialized = false;
};

}  // namespace TFLunaControl

#pragma once

#include <stdint.h>

namespace TFLunaControl {

/**
 * @brief Hysteresis and min on/off control logic.
 *
 * Works on any scalar input (CO2 ppm, temperature, etc.).
 * When value >= onThreshold the output turns ON;
 * when value <= offThreshold the output turns OFF.
 */
class OutputLogic {
 public:
  struct Rule {
    float onThreshold = 1200.0f;   ///< Value at or above which output turns ON.
    float offThreshold = 900.0f;   ///< Value at or below which output turns OFF.
    uint32_t minOnMs = 30000;
    uint32_t minOffMs = 30000;
  };

  /// @brief Configure control rule.
  void configure(const Rule& rule) { _rule = rule; }

  /// @brief Update state based on current reading.
  /// @param value Current sensor value
  /// @param valid True if the value is trustworthy
  /// @param nowMs Current time in milliseconds
  /// @return New output state
  bool update(float value, bool valid, uint32_t nowMs);

  /// @brief Reset internal state and timing.
  /// @param state Initial output state
  /// @param nowMs Current time in milliseconds
  void reset(bool state, uint32_t nowMs);

  /// @brief Get current output state.
  bool state() const { return _state; }

  /// @brief Get timestamp of last change.
  uint32_t lastChangeMs() const { return _lastChangeMs; }

 private:
  Rule _rule{};
  bool _state = false;
  uint32_t _lastChangeMs = 0;
};

}  // namespace TFLunaControl

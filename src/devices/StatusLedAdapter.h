#pragma once

#include <stdint.h>

#include "CO2Control/HardwareSettings.h"
#include "CO2Control/Status.h"

#ifdef ARDUINO
#include <StatusLed/StatusLed.h>
#if __has_include(<StatusLed/Version.h>)
#include <StatusLed/Version.h>
static_assert(StatusLed::VERSION_CODE >= 10300, "CO2Control requires StatusLED >= v1.3.0");
#endif
#endif

namespace CO2Control {

/**
 * @brief WS2812 status LED controller.
 */
class StatusLedAdapter {
 public:
  enum class WifiState : uint8_t {
    AP_OFF = 0,
    WAITING,
    CONNECTED,
    WEB_ACTIVE
  };

  enum class HealthState : uint8_t {
    INIT = 0,
    OK,
    DEGRADED,
    FAULT
  };

  struct HealthDebounceState {
    HealthState stable = HealthState::INIT;
    HealthState pending = HealthState::INIT;
    uint32_t pendingSinceMs = 0;
    bool initialized = false;
  };

  /// @brief Debounce health transitions to avoid LED flicker.
  /// @param target Requested health state for this tick.
  /// @param nowMs Current time.
  /// @param debounceMs Required stable duration before non-fault transition is applied.
  /// @param state Debounce state storage.
  /// @return Debounced/stable health state for LED rendering.
  static HealthState debounceHealth(HealthState target,
                                    uint32_t nowMs,
                                    uint32_t debounceMs,
                                    HealthDebounceState& state);

  Status begin(const HardwareSettings& config);
  void end();

  void setWifiState(WifiState state);
  void setHealthState(HealthState state);

  /// @brief Flash all LEDs with Success preset briefly (settings saved).
  /// @param durationMs How long the flash lasts before reverting.
  void flashSuccess(uint32_t durationMs = 800);

  /// @brief Force retransmit LED data on next tick (post-reinit safety).
  void forceRefresh();

  void tick(uint32_t nowMs);

 private:
  uint16_t _count = 0;
  uint8_t _wifiIndex = 0;
  uint8_t _healthIndex = 1;
  uint8_t _brightness = 20;
  int _pin = -1;
  uint16_t _tickCadenceMs = 20;
  uint32_t _nextTickDueMs = 0;

  WifiState _wifiState = WifiState::AP_OFF;
  HealthState _healthState = HealthState::INIT;

  bool _dirty = false;
  bool _tickKick = false;

#ifdef ARDUINO
  static Status mapBackendStatus(const ::StatusLed::Status& status);
  Status applyWifiState();
  Status applyHealthState();
  ::StatusLed::StatusLed* _strip = nullptr;
#endif
};

}  // namespace CO2Control

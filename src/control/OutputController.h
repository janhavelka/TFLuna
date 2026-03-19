#pragma once

#include <stddef.h>
#include <stdint.h>

#include "TFLunaControl/HardwareSettings.h"
#include "TFLunaControl/RuntimeSettings.h"
#include "TFLunaControl/Status.h"
#include "TFLunaControl/Types.h"
#include "control/OutputLogic.h"

namespace TFLunaControl {

/**
 * @brief Hardware output controller for MOSFETs and relays.
 */
class OutputController {
 public:
  Status begin(const HardwareSettings& config, const RuntimeSettings& settings);
  void end();

  void applySettings(const RuntimeSettings& settings);
  void tick(const Sample& sample, uint32_t nowMs);
  void setOverrideMode(OutputOverrideMode mode, uint32_t nowMs);
  Status setChannelTestOverride(size_t index, bool enabled, bool state, uint32_t nowMs);
  OutputOverrideMode overrideMode() const { return _overrideMode; }

  bool channelState(size_t index) const;
  bool channelPresent(size_t index) const;
  uint8_t presentMask() const { return _presentMask; }
  uint32_t lastChangeMs() const { return _lastOutputChangeMs; }
  bool valveState() const { return _valveState; }
  bool fanState() const { return _fanState; }
  uint8_t valveChannel() const { return _valveChannel; }
  uint8_t fanChannel() const { return _fanChannel; }
  bool valvePoweredCloses() const { return _valvePoweredCloses; }
  uint8_t fanAppliedPercent() const { return _fanAppliedPercent; }
  uint8_t testOverrideEnabledMask() const;
  uint8_t testOverrideStateMask() const;

 private:
  struct Channel {
    int pin = -1;
    bool activeHigh = true;
    bool pwmCapable = false;
    bool state = false;
    bool testOverrideEnabled = false;
    bool testOverrideState = false;
    uint32_t testOverrideUntilMs = 0;
  };

  static bool isDisabledChannel(uint8_t index);
  bool isChannelSelectable(uint8_t index) const;
  bool canApplyPwm(uint8_t index) const;
  bool canApplyFanPwm(uint8_t index) const;
  uint8_t mapFanPwmPercentToEffective(uint8_t userPercent) const;
  bool fanIntervalGate(uint32_t nowMs) const;

  OutputLogic _logic{};
  Channel _channels[HardwareSettings::OUTPUT_CHANNEL_COUNT] = {};
  bool _initialized = false;
  bool _outputsEnabled = false;
  bool _logicInitialized = false;
  OutputOverrideMode _overrideMode = OutputOverrideMode::AUTO;
  OutputSource _outputSource = OutputSource::CO2;
  uint8_t _presentMask = 0;
  uint8_t _valveChannel = RuntimeSettings::OUTPUT_CHANNEL_DISABLED;
  uint8_t _fanChannel = RuntimeSettings::OUTPUT_CHANNEL_DISABLED;
  bool _valvePoweredCloses = true;
  uint8_t _fanUserPercent = 0;
  uint8_t _fanAppliedPercent = 0;
  uint32_t _fanPeriodMs = RuntimeSettings::OUTPUT_FAN_PERIOD_DISABLED_MS;
  uint32_t _fanOnMs = 0;
  bool _valveState = false;
  bool _fanState = false;
  uint32_t _lastOutputChangeMs = 0;
};

}  // namespace TFLunaControl

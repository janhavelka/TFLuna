#include "control/OutputController.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace CO2Control {

namespace {
static constexpr uint32_t OUTPUT_TEST_OVERRIDE_HOLD_MS = 15000UL;
static constexpr uint32_t FAN_ANALOGWRITE_MAX_DUTY = 255U;
}  // namespace

bool OutputController::isDisabledChannel(uint8_t index) {
  return index == RuntimeSettings::OUTPUT_CHANNEL_DISABLED;
}

bool OutputController::isChannelSelectable(uint8_t index) const {
  return !isDisabledChannel(index) &&
         index < HardwareSettings::OUTPUT_CHANNEL_COUNT &&
         _channels[index].pin >= 0;
}

bool OutputController::canApplyPwm(uint8_t index) const {
  return isChannelSelectable(index) && _channels[index].pwmCapable;
}

bool OutputController::canApplyFanPwm(uint8_t index) const {
  // Valve actuation is always binary; never reuse PWM semantics on valve channel.
  if (!isDisabledChannel(_valveChannel) && index == _valveChannel) {
    return false;
  }
  return canApplyPwm(index);
}

uint8_t OutputController::mapFanPwmPercentToEffective(uint8_t userPercent) const {
  if (userPercent == 0U) {
    return 0U;
  }
  if (userPercent >= 100U) {
    return 100U;
  }
  const uint8_t minPct = RuntimeSettings::OUTPUT_FAN_PWM_EFFECTIVE_MIN_PERCENT;
  if (minPct >= 100U) {
    return 100U;
  }
  const uint16_t span = static_cast<uint16_t>(100U - minPct);
  // Map user range [1..99] to [0..span] with ceiling division:
  // (userPercent-1) * span gives the proportional value in 0-based input,
  // +98U / 99U performs ceiling integer division by 99 (input range width).
  const uint16_t scaled = static_cast<uint16_t>(
      ((static_cast<uint16_t>(userPercent - 1U) * span) + 98U) / 99U);
  return static_cast<uint8_t>(minPct + scaled);
}

bool OutputController::fanIntervalGate(uint32_t nowMs) const {
  if (_fanPeriodMs == RuntimeSettings::OUTPUT_FAN_PERIOD_DISABLED_MS) {
    return true;
  }
  if (_fanOnMs == 0U) {
    return false;
  }
  if (_fanOnMs >= _fanPeriodMs) {
    return true;
  }
  return (nowMs % _fanPeriodMs) < _fanOnMs;
}

Status OutputController::begin(const HardwareSettings& config, const RuntimeSettings& settings) {
  _channels[0].pin = config.mosfet1Pin;
  _channels[0].activeHigh = config.mosfet1ActiveHigh;
  _channels[0].pwmCapable = true;
  _channels[1].pin = config.mosfet2Pin;
  _channels[1].activeHigh = config.mosfet2ActiveHigh;
  _channels[1].pwmCapable = true;
  _channels[2].pin = config.relay1Pin;
  _channels[2].activeHigh = config.relay1ActiveHigh;
  _channels[2].pwmCapable = false;
  _channels[3].pin = config.relay2Pin;
  _channels[3].activeHigh = config.relay2ActiveHigh;
  _channels[3].pwmCapable = false;

  OutputLogic::Rule rule;
  rule.minOnMs = settings.minOnMs;
  rule.minOffMs = settings.minOffMs;
  _outputSource = static_cast<OutputSource>(settings.outputSource);
  switch (_outputSource) {
    case OutputSource::TEMP:
      rule.onThreshold = settings.tempOnC;
      rule.offThreshold = settings.tempOffC;
      break;
    case OutputSource::RH:
      rule.onThreshold = settings.rhOnPct;
      rule.offThreshold = settings.rhOffPct;
      break;
    case OutputSource::CO2:
    default:
      rule.onThreshold = settings.co2OnPpm;
      rule.offThreshold = settings.co2OffPpm;
      break;
  }
  _logic.configure(rule);

  _outputsEnabled = settings.outputsEnabled;
  _logicInitialized = false;
  _overrideMode = OutputOverrideMode::AUTO;
  _presentMask = 0;
  _valveChannel = settings.outputValveChannel;
  _fanChannel = settings.outputFanChannel;
  if (!isChannelSelectable(_valveChannel)) {
    _valveChannel = RuntimeSettings::OUTPUT_CHANNEL_DISABLED;
  }
  if (!isChannelSelectable(_fanChannel)) {
    _fanChannel = RuntimeSettings::OUTPUT_CHANNEL_DISABLED;
  }
  if (_valveChannel == _fanChannel && !isDisabledChannel(_valveChannel)) {
    _fanChannel = RuntimeSettings::OUTPUT_CHANNEL_DISABLED;
  }
  _valvePoweredCloses = settings.outputValvePoweredClosed;
  _fanUserPercent = settings.outputFanPwmPercent;
  _fanAppliedPercent = 0;
  _fanPeriodMs = settings.outputFanPeriodMs;
  _fanOnMs = settings.outputFanOnMs;
  _valveState = false;
  _fanState = false;
  _lastOutputChangeMs = 0;

  for (size_t i = 0; i < HardwareSettings::OUTPUT_CHANNEL_COUNT; ++i) {
    Channel& ch = _channels[i];
    ch.state = false;
    ch.testOverrideEnabled = false;
    ch.testOverrideState = false;
    ch.testOverrideUntilMs = 0;
    if (ch.pin >= 0 && i < 8U) {
      _presentMask = static_cast<uint8_t>(_presentMask | (1U << i));
    }
  }

#ifdef ARDUINO
  for (auto& ch : _channels) {
    if (ch.pin >= 0) {
      pinMode(static_cast<uint8_t>(ch.pin), OUTPUT);
      // Deterministic boot state: drive all outputs to inactive level.
      const int inactiveLevel = ch.activeHigh ? LOW : HIGH;
      if (ch.pwmCapable) {
        const uint32_t duty = (inactiveLevel == HIGH) ? FAN_ANALOGWRITE_MAX_DUTY : 0U;
        analogWrite(static_cast<uint8_t>(ch.pin), static_cast<int>(duty));
      } else {
        digitalWrite(static_cast<uint8_t>(ch.pin), inactiveLevel);
      }
      ch.state = false;
    }
  }
#endif

  _initialized = true;
  return Ok();
}

void OutputController::end() {
  if (!_initialized) {
    return;
  }
#ifdef ARDUINO
  for (auto& ch : _channels) {
    if (ch.pin >= 0) {
      const int inactiveLevel = ch.activeHigh ? LOW : HIGH;
      if (ch.pwmCapable) {
        const uint32_t duty = (inactiveLevel == HIGH) ? FAN_ANALOGWRITE_MAX_DUTY : 0U;
        analogWrite(static_cast<uint8_t>(ch.pin), static_cast<int>(duty));
      } else {
        digitalWrite(static_cast<uint8_t>(ch.pin), inactiveLevel);
      }
      ch.state = false;
    }
    ch.testOverrideEnabled = false;
    ch.testOverrideState = false;
    ch.testOverrideUntilMs = 0;
  }
#else
  for (auto& ch : _channels) {
    ch.state = false;
    ch.testOverrideEnabled = false;
    ch.testOverrideState = false;
    ch.testOverrideUntilMs = 0;
  }
#endif
  _presentMask = 0;
  _valveState = false;
  _fanState = false;
  _fanAppliedPercent = 0;
  _lastOutputChangeMs = 0;
  _initialized = false;
}

void OutputController::applySettings(const RuntimeSettings& settings) {
  OutputLogic::Rule rule;
  rule.minOnMs = settings.minOnMs;
  rule.minOffMs = settings.minOffMs;
  _outputSource = static_cast<OutputSource>(settings.outputSource);
  switch (_outputSource) {
    case OutputSource::TEMP:
      rule.onThreshold = settings.tempOnC;
      rule.offThreshold = settings.tempOffC;
      break;
    case OutputSource::RH:
      rule.onThreshold = settings.rhOnPct;
      rule.offThreshold = settings.rhOffPct;
      break;
    case OutputSource::CO2:
    default:
      rule.onThreshold = settings.co2OnPpm;
      rule.offThreshold = settings.co2OffPpm;
      break;
  }
  _logic.configure(rule);
  _outputsEnabled = settings.outputsEnabled;
  _logicInitialized = false;
  _valveChannel = settings.outputValveChannel;
  _fanChannel = settings.outputFanChannel;
  if (!isChannelSelectable(_valveChannel)) {
    _valveChannel = RuntimeSettings::OUTPUT_CHANNEL_DISABLED;
  }
  if (!isChannelSelectable(_fanChannel)) {
    _fanChannel = RuntimeSettings::OUTPUT_CHANNEL_DISABLED;
  }
  if (_valveChannel == _fanChannel && !isDisabledChannel(_valveChannel)) {
    _fanChannel = RuntimeSettings::OUTPUT_CHANNEL_DISABLED;
  }
  _valvePoweredCloses = settings.outputValvePoweredClosed;
  _fanUserPercent = settings.outputFanPwmPercent;
  _fanAppliedPercent = 0;
  _fanPeriodMs = settings.outputFanPeriodMs;
  _fanOnMs = settings.outputFanOnMs;
  _valveState = false;
  _fanState = false;
}

void OutputController::tick(const Sample& sample, uint32_t nowMs) {
  if (!_initialized) {
    return;
  }

  bool valveDesiredOn = false;
  bool fanRequested = false;
  uint8_t fanEffectivePercent = 0;

  if (_overrideMode == OutputOverrideMode::FORCE_ON) {
    valveDesiredOn = true;
    fanRequested = true;
    fanEffectivePercent = 100U;
  } else if (_overrideMode == OutputOverrideMode::FORCE_OFF) {
    valveDesiredOn = false;
    fanRequested = false;
    fanEffectivePercent = 0U;
  } else {
    if (!_logicInitialized) {
      _logic.reset(false, nowMs);
      _logicInitialized = true;
    }
    if (_outputsEnabled) {
      float value = 0.0f;
      bool valid = false;
      switch (_outputSource) {
        case OutputSource::TEMP:
          value = sample.tempC;
          valid = (sample.validMask & VALID_TEMP) != 0U;
          break;
        case OutputSource::RH:
          value = sample.rhPct;
          valid = (sample.validMask & VALID_RH) != 0U;
          break;
        case OutputSource::CO2:
        default:
          value = sample.co2ppm;
          valid = (sample.validMask & VALID_CO2) != 0U;
          break;
      }
      valveDesiredOn = _logic.update(value, valid, nowMs);
    } else {
      _logic.update(0.0f, false, nowMs);
      valveDesiredOn = false;
    }
    if (_outputsEnabled && _fanUserPercent > 0U && isChannelSelectable(_fanChannel)) {
      fanRequested = true;
      fanEffectivePercent =
          canApplyFanPwm(_fanChannel) ? mapFanPwmPercentToEffective(_fanUserPercent) : 100U;
    }
  }

  const bool valveAppliedOn = valveDesiredOn && isChannelSelectable(_valveChannel);
  bool fanAppliedOn = fanRequested && isChannelSelectable(_fanChannel);
  if (fanAppliedOn) {
    fanAppliedOn = fanIntervalGate(nowMs);
  }

  _valveState = valveAppliedOn;
  _fanState = fanRequested && isChannelSelectable(_fanChannel);
  _fanAppliedPercent = fanAppliedOn ? fanEffectivePercent : 0U;

  bool anyChanged = false;

#ifdef ARDUINO
  for (size_t i = 0; i < HardwareSettings::OUTPUT_CHANNEL_COUNT; ++i) {
    Channel& ch = _channels[i];
    if (ch.pin < 0) {
      if (ch.state) {
        anyChanged = true;
      }
      ch.state = false;
      continue;
    }
    if (ch.testOverrideEnabled &&
        static_cast<int32_t>(nowMs - ch.testOverrideUntilMs) >= 0) {
      ch.testOverrideEnabled = false;
    }
    bool channelState = false;
    if (ch.testOverrideEnabled) {
      channelState = ch.testOverrideState;
    } else if (!isDisabledChannel(_valveChannel) && i == _valveChannel) {
      channelState = valveAppliedOn;
    } else if (!isDisabledChannel(_fanChannel) && i == _fanChannel) {
      channelState = fanAppliedOn;
    }
    if (ch.state != channelState) {
      anyChanged = true;
    }
    ch.state = channelState;
    const bool fanPwmOutput = (!ch.testOverrideEnabled &&
                               !isDisabledChannel(_fanChannel) &&
                               i == _fanChannel &&
                               canApplyFanPwm(_fanChannel));
    if (fanPwmOutput) {
      const uint32_t dutyRaw = (static_cast<uint32_t>(_fanAppliedPercent) * FAN_ANALOGWRITE_MAX_DUTY + 50U) / 100U;
      uint32_t duty = dutyRaw;
      if (!ch.activeHigh) {
        duty = FAN_ANALOGWRITE_MAX_DUTY - dutyRaw;
      }
      analogWrite(static_cast<uint8_t>(ch.pin), static_cast<int>(duty));
    } else {
      const int level = (ch.state == ch.activeHigh) ? HIGH : LOW;
      if (ch.pwmCapable) {
        const uint32_t duty = (level == HIGH) ? FAN_ANALOGWRITE_MAX_DUTY : 0U;
        analogWrite(static_cast<uint8_t>(ch.pin), static_cast<int>(duty));
      } else {
        digitalWrite(static_cast<uint8_t>(ch.pin), level);
      }
    }
  }
#else
  for (size_t i = 0; i < HardwareSettings::OUTPUT_CHANNEL_COUNT; ++i) {
    Channel& ch = _channels[i];
    if (ch.pin < 0) {
      if (ch.state) {
        anyChanged = true;
      }
      ch.state = false;
      continue;
    }
    if (ch.testOverrideEnabled &&
        static_cast<int32_t>(nowMs - ch.testOverrideUntilMs) >= 0) {
      ch.testOverrideEnabled = false;
    }
    bool channelState = false;
    if (ch.testOverrideEnabled) {
      channelState = ch.testOverrideState;
    } else if (!isDisabledChannel(_valveChannel) && i == _valveChannel) {
      channelState = valveAppliedOn;
    } else if (!isDisabledChannel(_fanChannel) && i == _fanChannel) {
      channelState = fanAppliedOn;
    }
    if (ch.state != channelState) {
      anyChanged = true;
    }
    ch.state = channelState;
  }
#endif

  if (anyChanged) {
    _lastOutputChangeMs = nowMs;
  }
}

void OutputController::setOverrideMode(OutputOverrideMode mode, uint32_t nowMs) {
  if (_overrideMode == mode) {
    return;
  }
  _overrideMode = mode;
  if (_overrideMode == OutputOverrideMode::AUTO) {
    _logic.reset(_valveState, nowMs);
    _logicInitialized = true;
  }
}

bool OutputController::channelState(size_t index) const {
  if (index >= HardwareSettings::OUTPUT_CHANNEL_COUNT) {
    return false;
  }
  return _channels[index].state;
}

bool OutputController::channelPresent(size_t index) const {
  if (index >= HardwareSettings::OUTPUT_CHANNEL_COUNT) {
    return false;
  }
  return _channels[index].pin >= 0;
}

uint8_t OutputController::testOverrideEnabledMask() const {
  uint8_t mask = 0;
  for (size_t i = 0; i < HardwareSettings::OUTPUT_CHANNEL_COUNT && i < 8U; ++i) {
    const Channel& ch = _channels[i];
    if (ch.pin >= 0 && ch.testOverrideEnabled) {
      mask = static_cast<uint8_t>(mask | (1U << i));
    }
  }
  return mask;
}

uint8_t OutputController::testOverrideStateMask() const {
  uint8_t mask = 0;
  for (size_t i = 0; i < HardwareSettings::OUTPUT_CHANNEL_COUNT && i < 8U; ++i) {
    const Channel& ch = _channels[i];
    if (ch.pin >= 0 && ch.testOverrideEnabled && ch.testOverrideState) {
      mask = static_cast<uint8_t>(mask | (1U << i));
    }
  }
  return mask;
}

Status OutputController::setChannelTestOverride(size_t index, bool enabled, bool state, uint32_t nowMs) {
  if (!_initialized) {
    return Status(Err::NOT_INITIALIZED, 0, "outputs not initialized");
  }
  if (index >= HardwareSettings::OUTPUT_CHANNEL_COUNT) {
    return Status(Err::INVALID_CONFIG, 0, "output index out of range");
  }
  Channel& ch = _channels[index];
  if (ch.pin < 0) {
    return Status(Err::NOT_INITIALIZED, 0, "output channel not present");
  }
  if (!enabled) {
    ch.testOverrideEnabled = false;
    ch.testOverrideState = false;
    ch.testOverrideUntilMs = 0;
    return Ok();
  }
  ch.testOverrideEnabled = true;
  ch.testOverrideState = state;
  ch.testOverrideUntilMs = nowMs + OUTPUT_TEST_OVERRIDE_HOLD_MS;
  return Ok();
}

}  // namespace CO2Control

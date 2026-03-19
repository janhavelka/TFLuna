#include "devices/EndstopAdapter.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace TFLunaControl {

bool EndstopAdapter::computeTriggered(bool rawHigh, bool activeLow) {
  return activeLow ? !rawHigh : rawHigh;
}

Status EndstopAdapter::begin(const HardwareSettings& config) {
  _snapshot.upperPin = config.endstopUpperPin;
  _snapshot.upperConfigured = (config.endstopUpperPin >= 0);
  _snapshot.upperActiveLow = config.endstopUpperActiveLow;
  _snapshot.upperRawHigh = false;
  _snapshot.upperTriggered = false;
  _snapshot.upperLastChangeMs = 0;

  _snapshot.lowerPin = config.endstopLowerPin;
  _snapshot.lowerConfigured = (config.endstopLowerPin >= 0);
  _snapshot.lowerActiveLow = config.endstopLowerActiveLow;
  _snapshot.lowerRawHigh = false;
  _snapshot.lowerTriggered = false;
  _snapshot.lowerLastChangeMs = 0;

#ifdef ARDUINO
  if (_snapshot.upperConfigured) {
    pinMode(static_cast<uint8_t>(_snapshot.upperPin),
            _snapshot.upperActiveLow ? INPUT_PULLUP : INPUT);
    _snapshot.upperRawHigh = (digitalRead(static_cast<uint8_t>(_snapshot.upperPin)) == HIGH);
    _snapshot.upperTriggered = computeTriggered(_snapshot.upperRawHigh, _snapshot.upperActiveLow);
  }
  if (_snapshot.lowerConfigured) {
    pinMode(static_cast<uint8_t>(_snapshot.lowerPin),
            _snapshot.lowerActiveLow ? INPUT_PULLUP : INPUT);
    _snapshot.lowerRawHigh = (digitalRead(static_cast<uint8_t>(_snapshot.lowerPin)) == HIGH);
    _snapshot.lowerTriggered = computeTriggered(_snapshot.lowerRawHigh, _snapshot.lowerActiveLow);
  }
#endif

  _initialized = true;
  _lastStatus = (_snapshot.upperConfigured || _snapshot.lowerConfigured)
      ? Ok()
      : Status(Err::NOT_INITIALIZED, 0, "endstops disabled");
  return Ok();
}

void EndstopAdapter::sampleChannel(bool upper, uint32_t nowMs) {
#ifndef ARDUINO
  (void)upper;
  (void)nowMs;
  return;
#else
  bool& configured = upper ? _snapshot.upperConfigured : _snapshot.lowerConfigured;
  int& pin = upper ? _snapshot.upperPin : _snapshot.lowerPin;
  bool& activeLow = upper ? _snapshot.upperActiveLow : _snapshot.lowerActiveLow;
  bool& rawHigh = upper ? _snapshot.upperRawHigh : _snapshot.lowerRawHigh;
  bool& triggered = upper ? _snapshot.upperTriggered : _snapshot.lowerTriggered;
  uint32_t& lastChangeMs = upper ? _snapshot.upperLastChangeMs : _snapshot.lowerLastChangeMs;

  if (!configured || pin < 0) {
    return;
  }

  const bool nextRawHigh = (digitalRead(static_cast<uint8_t>(pin)) == HIGH);
  const bool nextTriggered = computeTriggered(nextRawHigh, activeLow);
  if (nextRawHigh != rawHigh || nextTriggered != triggered) {
    rawHigh = nextRawHigh;
    triggered = nextTriggered;
    lastChangeMs = nowMs;
  }
#endif
}

void EndstopAdapter::tick(uint32_t nowMs) {
  if (!_initialized) {
    return;
  }
  sampleChannel(true, nowMs);
  sampleChannel(false, nowMs);
}

void EndstopAdapter::end() {
  _initialized = false;
}

}  // namespace TFLunaControl

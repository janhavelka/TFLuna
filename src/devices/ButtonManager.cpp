#include "devices/ButtonManager.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace CO2Control {

Status ButtonManager::begin(const HardwareSettings& config) {
  _pin = config.buttonPin;
  _activeLow = config.buttonActiveLow;
  _debounceMs = config.buttonDebounceMs;
  _longPressMs = config.buttonLongPressMs;
  _multiPressWindowMs = config.buttonMultiPressWindowMs;
  _multiPressCountTarget = config.buttonMultiPressCount;

  _lastRaw = false;
  _stable = false;
  _lastChangeMs = 0;
  _pressStartMs = 0;
  _longPressFired = false;
  _pressCount = 0;
  _firstPressMs = 0;
  _shortPressEvent = false;
  _longPressEvent = false;
  _multiPressEvent = false;

#ifdef ARDUINO
  if (_pin >= 0) {
    pinMode(static_cast<uint8_t>(_pin), _activeLow ? INPUT_PULLUP : INPUT);
  }
#endif
  return Ok();
}

void ButtonManager::end() {
  _pin = -1;
}

void ButtonManager::tick(uint32_t nowMs) {
  if (_pin < 0) {
    return;
  }

#ifdef ARDUINO
  const int rawLevel = digitalRead(static_cast<uint8_t>(_pin));
  const bool rawPressed = _activeLow ? (rawLevel == LOW) : (rawLevel == HIGH);
#else
  const bool rawPressed = false;
#endif

  if (rawPressed != _lastRaw) {
    _lastRaw = rawPressed;
    _lastChangeMs = nowMs;
  }

  if ((nowMs - _lastChangeMs) >= _debounceMs) {
    if (rawPressed != _stable) {
      _stable = rawPressed;
      if (_stable) {
        _pressStartMs = nowMs;
        _longPressFired = false;
      } else {
        if (!_longPressFired) {
          _shortPressEvent = true;
          if (_pressCount == 0) {
            _firstPressMs = nowMs;
          }
          _pressCount++;
          if ((nowMs - _firstPressMs) <= _multiPressWindowMs &&
              _pressCount >= _multiPressCountTarget) {
            _multiPressEvent = true;
            _pressCount = 0;
            _firstPressMs = 0;
          }
        }
      }
    }
  }

  if (_stable && !_longPressFired) {
    if ((nowMs - _pressStartMs) >= _longPressMs) {
      _longPressFired = true;
      _longPressEvent = true;
      _pressCount = 0;
      _firstPressMs = 0;
    }
  }

  if (_pressCount > 0 && (nowMs - _firstPressMs) > _multiPressWindowMs) {
    _pressCount = 0;
    _firstPressMs = 0;
  }
}

bool ButtonManager::consumeShortPress() {
  if (_shortPressEvent) {
    _shortPressEvent = false;
    return true;
  }
  return false;
}

bool ButtonManager::consumeLongPress() {
  if (_longPressEvent) {
    _longPressEvent = false;
    return true;
  }
  return false;
}

bool ButtonManager::consumeMultiPress() {
  if (_multiPressEvent) {
    _multiPressEvent = false;
    return true;
  }
  return false;
}

}  // namespace CO2Control

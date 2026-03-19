#pragma once

#include <stdint.h>

#include "TFLunaControl/HardwareSettings.h"
#include "TFLunaControl/Status.h"

namespace TFLunaControl {

/**
 * @brief Debounced button handler with short/long/multi-press detection.
 */
class ButtonManager {
 public:
  Status begin(const HardwareSettings& config);
  void tick(uint32_t nowMs);
  void end();

  bool consumeShortPress();
  bool consumeLongPress();
  bool consumeMultiPress();

 private:
  int _pin = -1;
  bool _activeLow = true;
  uint32_t _debounceMs = 30;
  uint32_t _longPressMs = 2000;
  uint32_t _multiPressWindowMs = 8000;
  uint8_t _multiPressCountTarget = 5;

  bool _lastRaw = false;
  bool _stable = false;
  uint32_t _lastChangeMs = 0;
  uint32_t _pressStartMs = 0;
  bool _longPressFired = false;

  uint8_t _pressCount = 0;
  uint32_t _firstPressMs = 0;

  bool _shortPressEvent = false;
  bool _longPressEvent = false;
  bool _multiPressEvent = false;
};

}  // namespace TFLunaControl

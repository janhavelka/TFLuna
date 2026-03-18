#pragma once

#include <stdint.h>

namespace CO2Control {

class I2cGpioProbe {
 public:
  void configure(int sdaPin, int sclPin, uint8_t debounceMs);
  bool isBusPhysicallyStuck(uint32_t nowMs) const;

 private:
  bool isLineLow(int pin) const;

  int _sdaPin = -1;
  int _sclPin = -1;
  uint8_t _debounceMs = 3;
};

}  // namespace CO2Control

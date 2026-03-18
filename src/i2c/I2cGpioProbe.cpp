#include "i2c/I2cGpioProbe.h"

#include "core/SystemClock.h"

#ifdef ARDUINO
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

namespace CO2Control {

void I2cGpioProbe::configure(int sdaPin, int sclPin, uint8_t debounceMs) {
  _sdaPin = sdaPin;
  _sclPin = sclPin;
  _debounceMs = debounceMs;
}

bool I2cGpioProbe::isLineLow(int pin) const {
  if (pin < 0) {
    return false;
  }
#ifdef ARDUINO
  // Do not reconfigure SDA/SCL pin mode at runtime while I2C peripheral owns
  // the pins. Reapplying GPIO mode can disrupt active bus muxing on ESP32-S3.
  return digitalRead(static_cast<uint8_t>(pin)) == LOW;
#else
  return false;
#endif
}

bool I2cGpioProbe::isBusPhysicallyStuck(uint32_t nowMs) const {
  (void)nowMs;
  if (_sdaPin < 0 || _sclPin < 0) {
    return false;
  }

#ifdef ARDUINO
  const bool lowNow = isLineLow(_sdaPin) || isLineLow(_sclPin);
  if (!lowNow) {
    return false;
  }

  const uint32_t startMs = SystemClock::nowMs();
  while ((SystemClock::nowMs() - startMs) < static_cast<uint32_t>(_debounceMs)) {
    if (!isLineLow(_sdaPin) && !isLineLow(_sclPin)) {
      return false;
    }
    vTaskDelay(1);
  }
  return true;
#else
  return false;
#endif
}

}  // namespace CO2Control

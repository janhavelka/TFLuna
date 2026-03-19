#pragma once

#include <stdint.h>

#include "TFLunaControl/HardwareSettings.h"
#include "TFLunaControl/Status.h"
#include "TFLunaControl/Types.h"

namespace TFLunaControl {

class I2cOrchestrator;

/**
 * @brief RTC adapter (software fallback until hardware integration).
 */
class RtcAdapter {
 public:
  Status begin(const HardwareSettings& config, I2cOrchestrator* orchestrator);
  Status setTime(const RtcTime& time, uint64_t nowMs);
  Status getTime(uint64_t nowMs, RtcTime& out) const;
  bool isValid() const { return _hasTime; }
  uint32_t consecutiveFailures() const { return _consecutiveFailures; }

 private:
  I2cOrchestrator* _orchestrator = nullptr;
  bool _hasTime = false;
  uint32_t _baseUnix = 0;
  uint64_t _baseMillis = 0;
  mutable uint32_t _consecutiveFailures = 0;
};

}  // namespace TFLunaControl

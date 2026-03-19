#pragma once

#include "TFLunaControl/HardwareSettings.h"
#include "TFLunaControl/Health.h"
#include "TFLunaControl/Status.h"
#include "TFLunaControl/Types.h"

namespace TFLunaControl {

class I2cOrchestrator;

/**
 * @brief Environmental sensor adapter (BME280 or SHT31).
 */
class EnvSensorAdapter {
 public:
  Status begin(const HardwareSettings& config, I2cOrchestrator* orchestrator);
  Status readOnce(Sample& sample, uint32_t nowMs);
  HealthState health() const { return _health; }
  Status lastStatus() const { return _lastStatus; }
  uint32_t consecutiveFailures() const { return _consecutiveFailures; }

 private:
  bool _configured = false;
  I2cOrchestrator* _orchestrator = nullptr;
  HealthState _health = HealthState::UNKNOWN;
  Status _lastStatus = Ok();
  uint32_t _consecutiveFailures = 0;
};

}  // namespace TFLunaControl

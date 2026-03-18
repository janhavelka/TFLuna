#pragma once

#include "CO2Control/HardwareSettings.h"
#include "CO2Control/Health.h"
#include "CO2Control/Status.h"
#include "CO2Control/Types.h"

namespace CO2Control {

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

}  // namespace CO2Control

#include "devices/EnvSensorAdapter.h"

#include "i2c/I2cOrchestrator.h"

namespace CO2Control {

Status EnvSensorAdapter::begin(const HardwareSettings& config, I2cOrchestrator* orchestrator) {
  _orchestrator = orchestrator;
  _configured = (config.i2cSda >= 0 && config.i2cScl >= 0);
  if (!_configured || _orchestrator == nullptr) {
    _health = HealthState::DEGRADED;
    _lastStatus = Status(Err::INVALID_CONFIG, 0, "I2C env config invalid");
    _consecutiveFailures = 1;
    return _lastStatus;
  }
  _health = HealthState::DEGRADED;
  _lastStatus = Status(Err::NOT_INITIALIZED, 0, "Env waiting first sample");
  _consecutiveFailures = 0;
  return Ok();
}

Status EnvSensorAdapter::readOnce(Sample& sample, uint32_t nowMs) {
  if (!_configured || _orchestrator == nullptr) {
    _lastStatus = Status(Err::NOT_INITIALIZED, 0, "Env adapter not initialized");
    _health = HealthState::FAULT;
    _consecutiveFailures++;
    return _lastStatus;
  }

  const Status st = _orchestrator->fillEnvSample(sample, nowMs);
  _lastStatus = st;
  _health = _orchestrator->envHealth();
  _consecutiveFailures = _orchestrator->envConsecutiveFailures();
  return _lastStatus;
}

}  // namespace CO2Control

#include "devices/LidarAdapter.h"

#ifdef ARDUINO
#include <Arduino.h>
#if __has_include(<TFMPlus.h>)
#include <TFMPlus.h>
#define TFLUNACTRL_HAS_TFMPROBE 1
#else
#define TFLUNACTRL_HAS_TFMPROBE 0
#endif
#else
#define TFLUNACTRL_HAS_TFMPROBE 0
#endif

namespace TFLunaControl {

namespace {

#if TFLUNACTRL_HAS_TFMPROBE
Status tfmpStatusToStatus(uint8_t status, bool hadData) {
  switch (status) {
    case TFMP_READY:
      return Ok();
    case TFMP_WEAK:
      return Status(Err::COMM_FAILURE, 0, "TFMPlus probe weak signal");
    case TFMP_STRONG:
      return Status(Err::COMM_FAILURE, 0, "TFMPlus probe saturated signal");
    case TFMP_FLOOD:
      return Status(Err::COMM_FAILURE, 0, "TFMPlus probe ambient flood");
    case TFMP_CHECKSUM:
      return Status(Err::DATA_CORRUPT, 0, "TFMPlus probe checksum");
    case TFMP_HEADER:
      return Status(Err::COMM_FAILURE, 0, "TFMPlus probe header timeout");
    case TFMP_SERIAL:
      return Status(Err::TIMEOUT, 0, hadData ? "TFMPlus probe serial timeout"
                                             : "TFMPlus probe no serial data");
    default:
      return Status(Err::COMM_FAILURE, static_cast<int32_t>(status), "TFMPlus probe failed");
  }
}
#endif

}  // namespace

LidarAdapter::LidarAdapter()
#if defined(ARDUINO)
    : _serial(1)
#endif
{
}

void LidarAdapter::applySettings(const RuntimeSettings& settings, uint32_t nowMs) {
  (void)nowMs;
  _serviceIntervalMs = settings.lidarServiceMs;
  _frameStaleMs = settings.lidarFrameStaleMs;
  _parser.configure(settings.lidarMinStrength, settings.lidarMaxDistanceCm);
}

Status LidarAdapter::begin(const HardwareSettings& config) {
  _configured = (config.lidarRx >= 0 && config.lidarTx >= 0);
  _rxPin = config.lidarRx;
  _txPin = config.lidarTx;
  _haveLatest = false;
  _latest = LidarMeasurement{};
  _parser.reset();

  if (!_configured) {
    _health = HealthState::DEGRADED;
    _lastStatus = Status(Err::INVALID_CONFIG, 0, "TF-Luna UART pins not set");
    return _lastStatus;
  }

  if (config.lidarUartIndex != 1U) {
    _health = HealthState::DEGRADED;
    _lastStatus = Status(Err::INVALID_CONFIG, static_cast<int32_t>(config.lidarUartIndex),
                         "TF-Luna adapter supports UART1 only");
    return _lastStatus;
  }

  return reinitializeUart(0U, false);
}

void LidarAdapter::tick(uint32_t nowMs) {
  if (!_configured) {
    return;
  }
  if (_nextServiceMs != 0U && static_cast<int32_t>(nowMs - _nextServiceMs) < 0) {
    return;
  }
  _nextServiceMs = nowMs + _serviceIntervalMs;

#if defined(ARDUINO)
  size_t budget = 64U;
  while (budget > 0U && _serial.available() > 0) {
    LidarMeasurement measurement{};
    const uint8_t value = static_cast<uint8_t>(_serial.read());
    if (_parser.pushByte(value, nowMs, measurement)) {
      _latest = measurement;
      _haveLatest = true;
      _stats.recordMeasurement(measurement);
      if (measurement.signalOk) {
        _health = HealthState::OK;
        _lastStatus = Ok();
      } else {
        _health = HealthState::DEGRADED;
        _lastStatus = Status(Err::COMM_FAILURE, 0, "TF-Luna weak/invalid signal");
      }
    }
    --budget;
  }
#endif

  (void)evaluateHealth(nowMs);
}

Status LidarAdapter::forceRecover(uint32_t nowMs) {
  if (!_configured) {
    _health = HealthState::DEGRADED;
    _lastStatus = Status(Err::NOT_INITIALIZED, 0, "TF-Luna adapter not initialized");
    return _lastStatus;
  }
  return reinitializeUart(nowMs, true);
}

Status LidarAdapter::readOnce(Sample& sample, uint32_t nowMs) {
  tick(nowMs);
  const Status healthStatus = evaluateHealth(nowMs);
  if (!_haveLatest) {
    return healthStatus;
  }

  sample.distanceCm = _latest.distanceCm;
  sample.strength = _latest.strength;
  sample.lidarTempC = _latest.temperatureC;
  sample.validFrame = _latest.validFrame;
  sample.signalOk = _latest.signalOk && healthStatus.ok();
  sample.co2ppm = static_cast<float>(_latest.distanceCm);
  if (sample.signalOk) {
    sample.validMask |= VALID_CO2;
  }
  return healthStatus;
}

Status LidarAdapter::probeOnce(Sample& sample, uint32_t nowMs) {
#if TFLUNACTRL_HAS_TFMPROBE
  if (!_configured) {
    _health = HealthState::DEGRADED;
    _lastStatus = Status(Err::INVALID_CONFIG, 0, "TF-Luna UART pins not set");
    return _lastStatus;
  }

  TFMPlus probe;
  const bool beginOk = probe.begin(&_serial);
  if (!beginOk) {
    _health = HealthState::DEGRADED;
    _lastStatus = tfmpStatusToStatus(probe.status, false);
    return _lastStatus;
  }

  int16_t distanceCm = 0;
  int16_t strength = 0;
  int16_t temperatureC = 0;
  const bool gotFrame = probe.getData(distanceCm, strength, temperatureC);
  if (!gotFrame) {
    _health = HealthState::DEGRADED;
    _lastStatus = tfmpStatusToStatus(probe.status, true);
    return _lastStatus;
  }

  LidarMeasurement measurement{};
  measurement.distanceCm = (distanceCm > 0) ? static_cast<uint16_t>(distanceCm) : 0U;
  measurement.strength = (strength > 0) ? static_cast<uint16_t>(strength) : 0U;
  measurement.temperatureC = static_cast<float>(temperatureC);
  measurement.validFrame = true;
  measurement.signalOk = (measurement.distanceCm > 0U);
  measurement.capturedMs = nowMs;

  _latest = measurement;
  _haveLatest = true;
  _stats.recordMeasurement(measurement);

  sample.distanceCm = measurement.distanceCm;
  sample.strength = measurement.strength;
  sample.lidarTempC = measurement.temperatureC;
  sample.validFrame = measurement.validFrame;
  sample.signalOk = measurement.signalOk;
  sample.co2ppm = static_cast<float>(measurement.distanceCm);
  if (sample.signalOk) {
    sample.validMask |= VALID_CO2;
  }

  _health = measurement.signalOk ? HealthState::OK : HealthState::DEGRADED;
  _lastStatus = measurement.signalOk ? Ok()
                                     : Status(Err::COMM_FAILURE, 0, "TFMPlus probe weak/invalid");
  return _lastStatus;
#else
  return readOnce(sample, nowMs);
#endif
}

void LidarAdapter::resetStats() {
  _stats.reset();
}

bool LidarAdapter::latestMeasurement(LidarMeasurement& out) const {
  if (!_haveLatest) {
    return false;
  }
  out = _latest;
  return true;
}

LidarStatsSnapshot LidarAdapter::statsSnapshot() const {
  return _stats.snapshot();
}

uint32_t LidarAdapter::framesParsed() const {
  return _parser.framesParsed();
}

uint32_t LidarAdapter::checksumErrors() const {
  return _parser.checksumErrors();
}

uint32_t LidarAdapter::syncLossCount() const {
  return _parser.syncLossCount();
}

Status LidarAdapter::reinitializeUart(uint32_t nowMs, bool explicitRecover) {
#if defined(ARDUINO)
  _serial.end();
  _serial.setRxBufferSize(512U);
  _serial.begin(_baudRate, SERIAL_8N1, _rxPin, _txPin);
  while (_serial.available() > 0) {
    (void)_serial.read();
  }
  _parser.reset();
  _stats.reset();
  _haveLatest = false;
  _latest = LidarMeasurement{};
  _nextServiceMs = nowMs;
  _health = HealthState::DEGRADED;
  _lastStatus = explicitRecover
      ? Status(Err::RESOURCE_BUSY, 0, "TF-Luna UART reinitialized")
      : Status(Err::NOT_INITIALIZED, 0, "TF-Luna waiting first frame");
  return explicitRecover ? Ok() : _lastStatus;
#else
  (void)nowMs;
  (void)explicitRecover;
  _health = HealthState::DEGRADED;
  _lastStatus = Status(Err::NOT_INITIALIZED, 0, "Arduino UART unavailable");
  return _lastStatus;
#endif
}

Status LidarAdapter::evaluateHealth(uint32_t nowMs) {
  if (!_configured) {
    _health = HealthState::DEGRADED;
    _lastStatus = Status(Err::INVALID_CONFIG, 0, "TF-Luna UART pins not set");
    return _lastStatus;
  }
  if (!_haveLatest) {
    _health = HealthState::DEGRADED;
    if (_lastStatus.ok()) {
      _lastStatus = Status(Err::NOT_INITIALIZED, 0, "TF-Luna waiting first frame");
    }
    return _lastStatus;
  }

  const uint32_t ageMs = nowMs - _latest.capturedMs;
  if (ageMs > (_frameStaleMs * 4U)) {
    _health = HealthState::FAULT;
    _lastStatus = Status(Err::TIMEOUT, static_cast<int32_t>(ageMs), "TF-Luna frame stream stale");
    return _lastStatus;
  }
  if (ageMs > _frameStaleMs) {
    _health = HealthState::DEGRADED;
    _lastStatus = Status(Err::COMM_FAILURE, static_cast<int32_t>(ageMs), "TF-Luna frame stale");
    return _lastStatus;
  }
  if (!_latest.signalOk) {
    _health = HealthState::DEGRADED;
    _lastStatus = Status(Err::COMM_FAILURE, 0, "TF-Luna weak/invalid signal");
    return _lastStatus;
  }

  _health = HealthState::OK;
  _lastStatus = Ok();
  return _lastStatus;
}

}  // namespace TFLunaControl

#include "devices/Co2Adapter.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace CO2Control {

#if defined(ARDUINO) && CO2CONTROL_HAS_EE871_LIB
namespace {

static uint8_t clampU8(uint8_t value, uint8_t minValue, uint8_t maxValue) {
  if (value < minValue) {
    return minValue;
  }
  if (value > maxValue) {
    return maxValue;
  }
  return value;
}

static uint16_t clampU16(uint16_t value, uint16_t minValue, uint16_t maxValue) {
  if (value < minValue) {
    return minValue;
  }
  if (value > maxValue) {
    return maxValue;
  }
  return value;
}

static uint32_t clampU32(uint32_t value, uint32_t minValue, uint32_t maxValue) {
  if (value < minValue) {
    return minValue;
  }
  if (value > maxValue) {
    return maxValue;
  }
  return value;
}

static int8_t clampI8(int8_t value, int8_t minValue, int8_t maxValue) {
  if (value < minValue) {
    return minValue;
  }
  if (value > maxValue) {
    return maxValue;
  }
  return value;
}

static int16_t clampI16(int16_t value, int16_t minValue, int16_t maxValue) {
  if (value < minValue) {
    return minValue;
  }
  if (value > maxValue) {
    return maxValue;
  }
  return value;
}

}  // namespace
#endif

void Co2Adapter::applySettings(const RuntimeSettings& settings, uint32_t nowMs) {
#if defined(ARDUINO) && CO2CONTROL_HAS_EE871_LIB
  const uint8_t newAddress = clampU8(settings.e2Address, RuntimeSettings::MIN_E2_ADDR, RuntimeSettings::MAX_E2_ADDR);
  const uint16_t newClockLowUs =
      clampU16(settings.e2ClockLowUs, RuntimeSettings::MIN_E2_CLOCK_US, RuntimeSettings::MAX_E2_CLOCK_US);
  const uint16_t newClockHighUs =
      clampU16(settings.e2ClockHighUs, RuntimeSettings::MIN_E2_CLOCK_US, RuntimeSettings::MAX_E2_CLOCK_US);
  const uint16_t newStartHoldUs =
      clampU16(settings.e2StartHoldUs, RuntimeSettings::MIN_E2_HOLD_US, RuntimeSettings::MAX_E2_HOLD_US);
  const uint16_t newStopHoldUs =
      clampU16(settings.e2StopHoldUs, RuntimeSettings::MIN_E2_HOLD_US, RuntimeSettings::MAX_E2_HOLD_US);
  uint32_t newBitTimeoutUs = clampU32(settings.e2BitTimeoutUs,
                                      RuntimeSettings::MIN_E2_BIT_TIMEOUT_US,
                                      RuntimeSettings::MAX_E2_BIT_TIMEOUT_US);
  uint32_t newByteTimeoutUs = clampU32(settings.e2ByteTimeoutUs,
                                       RuntimeSettings::MIN_E2_BYTE_TIMEOUT_US,
                                       RuntimeSettings::MAX_E2_BYTE_TIMEOUT_US);
  if (newByteTimeoutUs < newBitTimeoutUs) {
    newByteTimeoutUs = newBitTimeoutUs;
  }
  const uint32_t newWriteDelayMs = clampU32(settings.e2WriteDelayMs,
                                            RuntimeSettings::MIN_E2_WRITE_DELAY_MS,
                                            RuntimeSettings::MAX_E2_WRITE_DELAY_MS);
  const uint32_t newIntervalWriteDelayMs =
      clampU32(settings.e2IntervalWriteDelayMs,
               RuntimeSettings::MIN_E2_INTERVAL_WRITE_DELAY_MS,
               RuntimeSettings::MAX_E2_INTERVAL_WRITE_DELAY_MS);
  uint32_t newRecoveryBackoffMs = clampU32(settings.e2RecoveryBackoffMs,
                                           RuntimeSettings::MIN_E2_RECOVERY_BACKOFF_MS,
                                           RuntimeSettings::MAX_E2_RECOVERY_BACKOFF_MS);
  uint32_t newRecoveryBackoffMaxMs = clampU32(settings.e2RecoveryBackoffMaxMs,
                                              RuntimeSettings::MIN_E2_RECOVERY_BACKOFF_MAX_MS,
                                              RuntimeSettings::MAX_E2_RECOVERY_BACKOFF_MAX_MS);
  if (newRecoveryBackoffMaxMs < newRecoveryBackoffMs) {
    newRecoveryBackoffMaxMs = newRecoveryBackoffMs;
  }

  const uint8_t newOfflineThreshold = clampU8(settings.e2OfflineThreshold,
                                              RuntimeSettings::MIN_E2_OFFLINE_THRESHOLD,
                                              RuntimeSettings::MAX_E2_OFFLINE_THRESHOLD);
  uint16_t newConfigIntervalDs = settings.e2ConfigIntervalDs;
  if (newConfigIntervalDs != 0U) {
    newConfigIntervalDs = clampU16(newConfigIntervalDs,
                                   RuntimeSettings::MIN_E2_CONFIG_INTERVAL_DS,
                                   RuntimeSettings::MAX_E2_CONFIG_INTERVAL_DS);
  }
  int8_t newConfigCo2IntervalFactor = settings.e2ConfigCo2IntervalFactor;
  if (newConfigCo2IntervalFactor != RuntimeSettings::E2_CONFIG_INTERVAL_FACTOR_DISABLED) {
    newConfigCo2IntervalFactor = clampI8(newConfigCo2IntervalFactor,
                                         RuntimeSettings::MIN_E2_CONFIG_INTERVAL_FACTOR,
                                         RuntimeSettings::MAX_E2_CONFIG_INTERVAL_FACTOR);
  }
  uint8_t newConfigFilter = settings.e2ConfigFilter;
  if (newConfigFilter != RuntimeSettings::E2_CONFIG_FILTER_DISABLED) {
    newConfigFilter = clampU8(newConfigFilter,
                              RuntimeSettings::MIN_E2_CONFIG_FILTER,
                              RuntimeSettings::MAX_E2_CONFIG_FILTER);
  }
  uint8_t newConfigOperatingMode = settings.e2ConfigOperatingMode;
  if (newConfigOperatingMode != RuntimeSettings::E2_CONFIG_OPERATING_MODE_DISABLED) {
    newConfigOperatingMode =
        clampU8(newConfigOperatingMode,
                RuntimeSettings::MIN_E2_CONFIG_OPERATING_MODE,
                RuntimeSettings::MAX_E2_CONFIG_OPERATING_MODE);
  }
  int16_t newConfigOffsetPpm = settings.e2ConfigOffsetPpm;
  if (newConfigOffsetPpm != RuntimeSettings::E2_CONFIG_OFFSET_PPM_DISABLED) {
    newConfigOffsetPpm = clampI16(newConfigOffsetPpm,
                                  RuntimeSettings::MIN_E2_CONFIG_OFFSET_PPM,
                                  RuntimeSettings::MAX_E2_CONFIG_OFFSET_PPM);
  }
  uint16_t newConfigGain = settings.e2ConfigGain;
  if (newConfigGain != RuntimeSettings::E2_CONFIG_GAIN_DISABLED) {
    newConfigGain = clampU16(newConfigGain, RuntimeSettings::MIN_E2_CONFIG_GAIN, RuntimeSettings::MAX_E2_CONFIG_GAIN);
  }

  const bool transportChanged =
      (_e2Address != newAddress) ||
      (_e2ClockLowUs != newClockLowUs) ||
      (_e2ClockHighUs != newClockHighUs) ||
      (_e2StartHoldUs != newStartHoldUs) ||
      (_e2StopHoldUs != newStopHoldUs) ||
      (_e2BitTimeoutUs != newBitTimeoutUs) ||
      (_e2ByteTimeoutUs != newByteTimeoutUs) ||
      (_e2WriteDelayMs != newWriteDelayMs) ||
      (_e2IntervalWriteDelayMs != newIntervalWriteDelayMs) ||
      (_e2OfflineThreshold != newOfflineThreshold);
  const bool managedChanged =
      (_e2ConfigIntervalDs != newConfigIntervalDs) ||
      (_e2ConfigCo2IntervalFactor != newConfigCo2IntervalFactor) ||
      (_e2ConfigFilter != newConfigFilter) ||
      (_e2ConfigOperatingMode != newConfigOperatingMode) ||
      (_e2ConfigOffsetPpm != newConfigOffsetPpm) ||
      (_e2ConfigGain != newConfigGain);
  const bool retryChanged =
      (_e2RecoveryBackoffMs != newRecoveryBackoffMs) ||
      (_e2RecoveryBackoffMaxMs != newRecoveryBackoffMaxMs);

  _e2Address = newAddress;
  _e2ClockLowUs = newClockLowUs;
  _e2ClockHighUs = newClockHighUs;
  _e2StartHoldUs = newStartHoldUs;
  _e2StopHoldUs = newStopHoldUs;
  _e2BitTimeoutUs = newBitTimeoutUs;
  _e2ByteTimeoutUs = newByteTimeoutUs;
  _e2WriteDelayMs = newWriteDelayMs;
  _e2IntervalWriteDelayMs = newIntervalWriteDelayMs;
  _e2OfflineThreshold = newOfflineThreshold;
  _e2RecoveryBackoffMs = newRecoveryBackoffMs;
  _e2RecoveryBackoffMaxMs = newRecoveryBackoffMaxMs;
  _e2ConfigIntervalDs = newConfigIntervalDs;
  _e2ConfigCo2IntervalFactor = newConfigCo2IntervalFactor;
  _e2ConfigFilter = newConfigFilter;
  _e2ConfigOperatingMode = newConfigOperatingMode;
  _e2ConfigOffsetPpm = newConfigOffsetPpm;
  _e2ConfigGain = newConfigGain;

  if (managedChanged) {
    _managedSettingsDirty = true;
    _managedSettingsStage = 1;
  }

  if (!transportChanged && !retryChanged && !managedChanged) {
    return;
  }

  if (!transportChanged) {
    if (_managedSettingsDirty && _driverInitialized) {
      _health = HealthState::DEGRADED;
      _lastStatus = Status(Err::RESOURCE_BUSY, 0, "EE871 managed config pending");
    }
    return;
  }

  if (_driverInitialized && _configured) {
    _driver.end();
    _driverInitialized = false;
  }

  _recoveryPending = true;
  _nextRetryMs = nowMs;
  _retryDelayMs = _e2RecoveryBackoffMs;
  _health = HealthState::DEGRADED;
  _lastStatus = Status(Err::RESOURCE_BUSY, 0, "EE871 reconfigure pending");
#else
  (void)settings;
  (void)nowMs;
#endif
}

Status Co2Adapter::begin(const HardwareSettings& config) {
  _configured = (config.e2Tx >= 0 && config.e2Rx >= 0);
#if defined(ARDUINO) && CO2CONTROL_HAS_EE871_LIB
  _driverInitialized = false;
  _managedSettingsDirty = true;
  _managedSettingsStage = 1;
  resetRetryState();
#endif
  if (!_configured) {
    _health = HealthState::DEGRADED;
    _lastStatus = Status(Err::INVALID_CONFIG, 0, "E2 pins not set");
    return _lastStatus;
  }

#if defined(ARDUINO) && CO2CONTROL_HAS_EE871_LIB
  _sclPin = config.e2Tx;
  _sdaPin = config.e2Rx;
  _enPin = config.e2En;

  if (_enPin >= 0) {
    pinMode(static_cast<uint8_t>(_enPin), OUTPUT);
    digitalWrite(static_cast<uint8_t>(_enPin), HIGH);
  }

  setLine(_sclPin, true);
  setLine(_sdaPin, true);

  _recoveryPending = true;
  _nextRetryMs = 0;
  _retryDelayMs = _e2RecoveryBackoffMs;
  return attemptBeginOrRecover(0);
#else
  _health = HealthState::DEGRADED;
  _lastStatus = Status(Err::NOT_INITIALIZED, 0, "EE871 library unavailable");
  return _lastStatus;
#endif
}

Status Co2Adapter::forceRecover(uint32_t nowMs) {
#if defined(ARDUINO) && CO2CONTROL_HAS_EE871_LIB
  if (!_configured) {
    _lastStatus = Status(Err::NOT_INITIALIZED, 0, "CO2 adapter not initialized");
    _health = HealthState::FAULT;
    return _lastStatus;
  }

  if (!_driverInitialized) {
    _recoveryPending = true;
    _nextRetryMs = nowMs;
    _retryDelayMs = (_e2RecoveryBackoffMs == 0U) ? 1U : _e2RecoveryBackoffMs;
  }

  return attemptBeginOrRecover(nowMs);
#else
  (void)nowMs;
  _lastStatus = Status(Err::NOT_INITIALIZED, 0, "EE871 library unavailable");
  _health = HealthState::DEGRADED;
  return _lastStatus;
#endif
}

Status Co2Adapter::readOnce(Sample& sample, uint32_t nowMs) {
  (void)sample;

#if defined(ARDUINO) && CO2CONTROL_HAS_EE871_LIB
  if (!_configured) {
    _lastStatus = Status(Err::NOT_INITIALIZED, 0, "CO2 adapter not initialized");
    _health = HealthState::FAULT;
    return _lastStatus;
  }

  _driver.tick(nowMs);
  if ((_recoveryPending || !_driverInitialized) && retryDue(nowMs)) {
    const Status recoverStatus = attemptBeginOrRecover(nowMs);
    if (!recoverStatus.ok()) {
      return recoverStatus;
    }
  } else if (_recoveryPending || !_driverInitialized) {
    _lastStatus = Status(Err::RESOURCE_BUSY, 0, "EE871 recovery backoff");
    _health = HealthState::DEGRADED;
    return _lastStatus;
  }

  if (_managedSettingsDirty) {
    const Status managedStatus = applyManagedDeviceSettings();
    if (!managedStatus.ok()) {
      _lastStatus = managedStatus;
      _health = HealthState::DEGRADED;
      return _lastStatus;
    }
  }

  uint16_t ppm = 0;
  EE871Api::Status driverStatus = _driver.readCo2Average(ppm);
  if (!driverStatus.ok()) {
    // Only fall back to Fast read for data-level errors (PEC, timeout
    // waiting for measurement data).  Bus-level failures (NACK, device
    // not found, bus stuck) mean no sensor is responding -- retrying
    // with a different command would burn another full write-delay
    // cycle for no benefit.
    const EE871Api::Err ec = driverStatus.code;
    const bool busFault = (ec == EE871Api::Err::NACK ||
                           ec == EE871Api::Err::DEVICE_NOT_FOUND ||
                           ec == EE871Api::Err::BUS_STUCK ||
                           ec == EE871Api::Err::E2_ERROR);
    if (!busFault) {
      driverStatus = _driver.readCo2Fast(ppm);
    }
  }

  _lastStatus = mapDriverStatus(driverStatus);
  _health = mapDriverHealth(_driver.state());
  if (!driverStatus.ok()) {
    if (shouldRetry(driverStatus)) {
      scheduleNextRetry(nowMs);
    }
    if (_health == HealthState::OK) {
      _health = HealthState::DEGRADED;
    }
    return _lastStatus;
  }

  resetRetryState();
  sample.co2ppm = static_cast<float>(ppm);
  sample.validMask |= VALID_CO2;
  _lastStatus = Ok();
  return _lastStatus;
#else
  (void)nowMs;
  _lastStatus = Status(Err::NOT_INITIALIZED, 0, "EE871 library unavailable");
  _health = HealthState::DEGRADED;
  return _lastStatus;
#endif
}

#if defined(ARDUINO) && CO2CONTROL_HAS_EE871_LIB
EE871Api::Config Co2Adapter::makeDriverConfig() const {
  EE871Api::Config driverCfg{};
  driverCfg.setScl = &Co2Adapter::e2SetScl;
  driverCfg.setSda = &Co2Adapter::e2SetSda;
  driverCfg.readScl = &Co2Adapter::e2ReadScl;
  driverCfg.readSda = &Co2Adapter::e2ReadSda;
  driverCfg.delayUs = &Co2Adapter::e2DelayUs;
  driverCfg.busUser = const_cast<Co2Adapter*>(this);
  driverCfg.deviceAddress = _e2Address;
  driverCfg.clockLowUs = _e2ClockLowUs;
  driverCfg.clockHighUs = _e2ClockHighUs;
  driverCfg.startHoldUs = _e2StartHoldUs;
  driverCfg.stopHoldUs = _e2StopHoldUs;
  driverCfg.bitTimeoutUs = _e2BitTimeoutUs;
  driverCfg.byteTimeoutUs = _e2ByteTimeoutUs;
  driverCfg.writeDelayMs = _e2WriteDelayMs;
  driverCfg.intervalWriteDelayMs = _e2IntervalWriteDelayMs;
  driverCfg.offlineThreshold = _e2OfflineThreshold;
  return driverCfg;
}

// Managed-settings stages (one EEPROM read+write per tick max).
static constexpr uint8_t MANAGED_STAGE_INTERVAL        = 1U;
static constexpr uint8_t MANAGED_STAGE_INTERVAL_FACTOR  = 2U;
static constexpr uint8_t MANAGED_STAGE_FILTER           = 3U;
static constexpr uint8_t MANAGED_STAGE_OPERATING_MODE   = 4U;
static constexpr uint8_t MANAGED_STAGE_OFFSET           = 5U;
static constexpr uint8_t MANAGED_STAGE_GAIN             = 6U;

Status Co2Adapter::applyManagedDeviceSettings() {
  if (!_driverInitialized || !_managedSettingsDirty) {
    return Ok();
  }

  // Process one stage per call.  Disabled stages are skipped without
  // bus I/O so several no-ops may collapse into a single call.  The
  // first stage that performs bus I/O returns Ok() immediately so the
  // caller can proceed to a CO2 read in the same tick.
  while (_managedSettingsStage <= MANAGED_STAGE_GAIN) {
    const uint8_t stage = _managedSettingsStage;
    _managedSettingsStage = static_cast<uint8_t>(stage + 1U);

    switch (stage) {
      case MANAGED_STAGE_INTERVAL: {
        if (_e2ConfigIntervalDs == 0U) break;
        if (!_driver.hasGlobalInterval()) {
          _managedSettingsDirty = false;
          _managedSettingsStage = 0;
          return Status(Err::INVALID_CONFIG, 0, "EE871 global interval unsupported");
        }
        uint16_t current = 0;
        EE871Api::Status st = _driver.readMeasurementInterval(current);
        if (!st.ok()) return mapDriverStatus(st);
        if (current != _e2ConfigIntervalDs) {
          st = _driver.writeMeasurementInterval(_e2ConfigIntervalDs);
          if (!st.ok()) return mapDriverStatus(st);
        }
        return Ok();  // yield after bus I/O
      }

      case MANAGED_STAGE_INTERVAL_FACTOR: {
        if (_e2ConfigCo2IntervalFactor == RuntimeSettings::E2_CONFIG_INTERVAL_FACTOR_DISABLED) break;
        if (!_driver.hasSpecificInterval()) {
          _managedSettingsDirty = false;
          _managedSettingsStage = 0;
          return Status(Err::INVALID_CONFIG, 0, "EE871 CO2 interval factor unsupported");
        }
        int8_t current = 0;
        EE871Api::Status st = _driver.readCo2IntervalFactor(current);
        if (!st.ok()) return mapDriverStatus(st);
        if (current != _e2ConfigCo2IntervalFactor) {
          st = _driver.writeCo2IntervalFactor(_e2ConfigCo2IntervalFactor);
          if (!st.ok()) return mapDriverStatus(st);
        }
        return Ok();
      }

      case MANAGED_STAGE_FILTER: {
        if (_e2ConfigFilter == RuntimeSettings::E2_CONFIG_FILTER_DISABLED) break;
        if (!_driver.hasFilterConfig()) {
          _managedSettingsDirty = false;
          _managedSettingsStage = 0;
          return Status(Err::INVALID_CONFIG, 0, "EE871 filter configuration unsupported");
        }
        uint8_t current = 0;
        EE871Api::Status st = _driver.readCo2Filter(current);
        if (!st.ok()) return mapDriverStatus(st);
        if (current != _e2ConfigFilter) {
          st = _driver.writeCo2Filter(_e2ConfigFilter);
          if (!st.ok()) return mapDriverStatus(st);
        }
        return Ok();
      }

      case MANAGED_STAGE_OPERATING_MODE: {
        if (_e2ConfigOperatingMode == RuntimeSettings::E2_CONFIG_OPERATING_MODE_DISABLED) break;
        const bool requestLowPower = (_e2ConfigOperatingMode & 0x01U) != 0U;
        const bool requestE2Priority = (_e2ConfigOperatingMode & 0x02U) != 0U;
        if (requestLowPower && !_driver.hasLowPowerMode()) {
          _managedSettingsDirty = false;
          _managedSettingsStage = 0;
          return Status(Err::INVALID_CONFIG, 0, "EE871 low-power mode unsupported");
        }
        if (requestE2Priority && !_driver.hasE2Priority()) {
          _managedSettingsDirty = false;
          _managedSettingsStage = 0;
          return Status(Err::INVALID_CONFIG, 0, "EE871 E2-priority mode unsupported");
        }
        uint8_t current = 0;
        EE871Api::Status st = _driver.readOperatingMode(current);
        if (!st.ok()) return mapDriverStatus(st);
        if (current != _e2ConfigOperatingMode) {
          st = _driver.writeOperatingMode(_e2ConfigOperatingMode);
          if (!st.ok()) return mapDriverStatus(st);
        }
        return Ok();
      }

      case MANAGED_STAGE_OFFSET: {
        if (_e2ConfigOffsetPpm == RuntimeSettings::E2_CONFIG_OFFSET_PPM_DISABLED) break;
        int16_t current = 0;
        EE871Api::Status st = _driver.readCo2Offset(current);
        if (!st.ok()) return mapDriverStatus(st);
        if (current != _e2ConfigOffsetPpm) {
          st = _driver.writeCo2Offset(_e2ConfigOffsetPpm);
          if (!st.ok()) return mapDriverStatus(st);
        }
        return Ok();
      }

      case MANAGED_STAGE_GAIN: {
        if (_e2ConfigGain == RuntimeSettings::E2_CONFIG_GAIN_DISABLED) break;
        uint16_t current = 0;
        EE871Api::Status st = _driver.readCo2Gain(current);
        if (!st.ok()) return mapDriverStatus(st);
        if (current != _e2ConfigGain) {
          st = _driver.writeCo2Gain(_e2ConfigGain);
          if (!st.ok()) return mapDriverStatus(st);
        }
        return Ok();
      }

      default:
        break;
    }
  }

  // All stages complete.
  _managedSettingsDirty = false;
  _managedSettingsStage = 0;
  return Ok();
}

Status Co2Adapter::attemptBeginOrRecover(uint32_t nowMs) {
  EE871Api::Status driverStatus = EE871Api::Status::Ok();
  if (!_driverInitialized) {
    driverStatus = _driver.begin(makeDriverConfig());
  } else {
    driverStatus = _driver.recover();
  }

  _lastStatus = mapDriverStatus(driverStatus);
  _health = mapDriverHealth(_driver.state());
  if (driverStatus.ok()) {
    _driverInitialized = true;
    // Managed settings will be applied one-stage-per-tick via readOnce().
    // Reset stage so the full sequence runs from the beginning.
    if (_managedSettingsDirty) {
      _managedSettingsStage = 1;
    }
    resetRetryState();
    _lastStatus = Ok();
    return _lastStatus;
  }

  if (shouldRetry(driverStatus)) {
    scheduleNextRetry(nowMs);
  } else {
    _recoveryPending = false;
    _nextRetryMs = 0;
    _retryDelayMs = 0;
  }

  if (_health == HealthState::OK) {
    _health = HealthState::DEGRADED;
  }
  return _lastStatus;
}

void Co2Adapter::resetRetryState() {
  _recoveryPending = false;
  _nextRetryMs = 0;
  _retryDelayMs = 0;
}

void Co2Adapter::scheduleNextRetry(uint32_t nowMs) {
  const uint32_t initialDelay = (_e2RecoveryBackoffMs == 0) ? 1U : _e2RecoveryBackoffMs;
  const uint32_t maxDelay = (_e2RecoveryBackoffMaxMs < initialDelay)
                                ? initialDelay
                                : _e2RecoveryBackoffMaxMs;
  uint32_t delayMs = (_retryDelayMs == 0) ? initialDelay : _retryDelayMs;
  if (delayMs < initialDelay) {
    delayMs = initialDelay;
  }
  if (delayMs > maxDelay) {
    delayMs = maxDelay;
  }

  _recoveryPending = true;
  _nextRetryMs = nowMs + delayMs;

  uint64_t nextDelay = static_cast<uint64_t>(delayMs) * 2ULL;
  if (nextDelay > static_cast<uint64_t>(maxDelay)) {
    _retryDelayMs = maxDelay;
  } else {
    _retryDelayMs = static_cast<uint32_t>(nextDelay);
  }
}

bool Co2Adapter::retryDue(uint32_t nowMs) const {
  if (!_recoveryPending) {
    return false;
  }
  return (_nextRetryMs == 0) || (static_cast<int32_t>(nowMs - _nextRetryMs) >= 0);
}

bool Co2Adapter::shouldRetry(const EE871Api::Status& status) {
  switch (status.code) {
    case EE871Api::Err::OK:
    case EE871Api::Err::BUSY:
    case EE871Api::Err::IN_PROGRESS:
    case EE871Api::Err::INVALID_CONFIG:
    case EE871Api::Err::INVALID_PARAM:
    case EE871Api::Err::OUT_OF_RANGE:
    case EE871Api::Err::ALREADY_INITIALIZED:
    case EE871Api::Err::NOT_SUPPORTED:
      return false;
    case EE871Api::Err::NOT_INITIALIZED:
    case EE871Api::Err::E2_ERROR:
    case EE871Api::Err::TIMEOUT:
    case EE871Api::Err::DEVICE_NOT_FOUND:
    case EE871Api::Err::PEC_MISMATCH:
    case EE871Api::Err::NACK:
    case EE871Api::Err::BUS_STUCK:
    default:
      return true;
  }
}

Status Co2Adapter::mapDriverStatus(const EE871Api::Status& status) {
  const int32_t detail = (status.detail != 0) ? status.detail : static_cast<int32_t>(status.code);
  switch (status.code) {
    case EE871Api::Err::OK:
      return Ok();
    case EE871Api::Err::NOT_INITIALIZED:
      return Status(Err::NOT_INITIALIZED, detail, "EE871 not initialized");
    case EE871Api::Err::INVALID_CONFIG:
    case EE871Api::Err::INVALID_PARAM:
    case EE871Api::Err::OUT_OF_RANGE:
      return Status(Err::INVALID_CONFIG, detail, "EE871 invalid config");
    case EE871Api::Err::TIMEOUT:
      return Status(Err::TIMEOUT, detail, "EE871 timeout");
    case EE871Api::Err::BUS_STUCK:
      return Status(Err::BUS_STUCK, detail, "EE871 bus stuck");
    case EE871Api::Err::BUSY:
    case EE871Api::Err::IN_PROGRESS:
    case EE871Api::Err::ALREADY_INITIALIZED:
      return Status(Err::RESOURCE_BUSY, detail, "EE871 busy");
    case EE871Api::Err::DEVICE_NOT_FOUND:
      return Status(Err::COMM_FAILURE, detail, "EE871 device not found");
    case EE871Api::Err::PEC_MISMATCH:
      return Status(Err::DATA_CORRUPT, detail, "EE871 PEC mismatch");
    case EE871Api::Err::E2_ERROR:
    case EE871Api::Err::NACK:
      return Status(Err::COMM_FAILURE, detail, "EE871 communication error");
    case EE871Api::Err::NOT_SUPPORTED:
    default:
      return Status(Err::EXTERNAL_LIB_ERROR, detail, "EE871 external error");
  }
}

HealthState Co2Adapter::mapDriverHealth(EE871Api::DriverState state) {
  switch (state) {
    case EE871Api::DriverState::READY:
      return HealthState::OK;
    case EE871Api::DriverState::DEGRADED:
      return HealthState::DEGRADED;
    case EE871Api::DriverState::OFFLINE:
      return HealthState::FAULT;
    case EE871Api::DriverState::UNINIT:
    default:
      return HealthState::DEGRADED;
  }
}

void Co2Adapter::setLine(int pin, bool level) {
  if (pin < 0) {
    return;
  }

  if (level) {
    pinMode(static_cast<uint8_t>(pin), INPUT_PULLUP);
  } else {
    pinMode(static_cast<uint8_t>(pin), OUTPUT);
    digitalWrite(static_cast<uint8_t>(pin), LOW);
  }
}

bool Co2Adapter::readLine(int pin) const {
  if (pin < 0) {
    return true;
  }
  return digitalRead(static_cast<uint8_t>(pin)) == HIGH;
}

void Co2Adapter::e2SetScl(bool level, void* user) {
  Co2Adapter* self = static_cast<Co2Adapter*>(user);
  if (self != nullptr) {
    self->setLine(self->_sclPin, level);
  }
}

void Co2Adapter::e2SetSda(bool level, void* user) {
  Co2Adapter* self = static_cast<Co2Adapter*>(user);
  if (self != nullptr) {
    self->setLine(self->_sdaPin, level);
  }
}

bool Co2Adapter::e2ReadScl(void* user) {
  Co2Adapter* self = static_cast<Co2Adapter*>(user);
  return (self != nullptr) ? self->readLine(self->_sclPin) : true;
}

bool Co2Adapter::e2ReadSda(void* user) {
  Co2Adapter* self = static_cast<Co2Adapter*>(user);
  return (self != nullptr) ? self->readLine(self->_sdaPin) : true;
}

void Co2Adapter::e2DelayUs(uint32_t us, void* user) {
  (void)user;
  while (us > 1000U) {
    delayMicroseconds(1000U);
    us -= 1000U;
  }
  if (us > 0U) {
    delayMicroseconds(us);
  }
}
#endif

}  // namespace CO2Control



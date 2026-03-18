#include "i2c/I2cOrchestrator.h"
#include "i2c/I2cPayloadParsers.h"

#include <string.h>

#include "core/TimeUtil.h"

#ifndef CO2CONTROL_ENABLE_DISPLAY
#define CO2CONTROL_ENABLE_DISPLAY 0
#endif

namespace CO2Control {

/// @brief Boot grace: delay first I2C polls to let peripherals stabilize.
static constexpr uint32_t kBootGraceMs = 500;

static uint32_t nextNonZeroToken(uint32_t& nextToken) {
  uint32_t token = nextToken++;
  if (token == 0) {
    token = nextToken++;
  }
  if (nextToken == 0) {
    nextToken = 1;
  }
  return token;
}

static I2cRawOp toRawOp(I2cOpType op) {
  switch (op) {
    case I2cOpType::WRITE:
      return I2cRawOp::WRITE;
    case I2cOpType::READ:
      return I2cRawOp::READ;
    case I2cOpType::WRITE_READ:
      return I2cRawOp::WRITE_READ;
    case I2cOpType::PROBE:
      return I2cRawOp::PROBE;
    default:
      return I2cRawOp::NONE;
  }
}

Status I2cOrchestrator::begin(const HardwareSettings& config,
                              const AppSettings& appSettings,
                              const RuntimeSettings& settings,
                              II2cRequestPort* port) {
  _port = port;
  _config = config;
  _appSettings = appSettings;
  _settings = settings;
  _enabled = (_port != nullptr && config.i2cSda >= 0 && config.i2cScl >= 0);
  _nextToken = 1;
  _requestOverflowCount = 0;
  _lastOverflowMs = 0;
  _staleResultCount = 0;
  _busRecoverPending = false;
  _busRecoverActiveToken = 0;
  _busRecoverActiveDeadlineMs = 0;
  _busRecoverStatus = _enabled ? Status(Err::NOT_INITIALIZED, 0, "I2C recover idle")
                               : Status(Err::NOT_INITIALIZED, 0, "I2C disabled");

  _rtcHasTime = false;
  _rtcTime = RtcTime{};
  _rtcLastSuccessMs = 0;
  _rtcNextPollMs = kBootGraceMs;
  _rtcActiveReadToken = 0;
  _rtcActiveReadDeadlineMs = 0;
  _rtcActiveSetToken = 0;
  _rtcActiveSetDeadlineMs = 0;
  _rtcSetPending = false;
  _rtcConsecutiveFailures = 0;
  _rtcConsecutiveSuccesses = 0;
  _rtcHealth = _enabled ? HealthState::DEGRADED : HealthState::FAULT;
  _rtcStatus = _enabled ? Status(Err::NOT_INITIALIZED, 0, "RTC waiting first sample")
                        : Status(Err::NOT_INITIALIZED, 0, "I2C disabled");

  _envPhase = EnvPhase::IDLE;
  _envPhaseDeadlineMs = 0;
  _envNextPollMs = kBootGraceMs;
  _envActiveTriggerToken = 0;
  _envActiveTriggerDeadlineMs = 0;
  _envActiveReadToken = 0;
  _envActiveReadDeadlineMs = 0;
  _envHasData = false;
  _envLastSuccessMs = 0;
  _envTempC = 0.0f;
  _envRhPct = 0.0f;
  _envPressureHpa = 0.0f;
  _envConsecutiveFailures = 0;
  _envConsecutiveSuccesses = 0;
  _envHealth = _enabled ? HealthState::DEGRADED : HealthState::FAULT;
  _envStatus = _enabled ? Status(Err::NOT_INITIALIZED, 0, "ENV waiting first sample")
                        : Status(Err::NOT_INITIALIZED, 0, "I2C disabled");

#if CO2CONTROL_ENABLE_DISPLAY
  _displayEnabled = (_enabled && _appSettings.enableDisplay);
#else
  _displayEnabled = false;
#endif
  _displayNextPollMs = kBootGraceMs;
  _displayCo2Valid = false;
  _displayCo2Ppm = 0.0f;
  _displayCo2SampleMs = 0;
  _displayOutputMask = 0;
  _displayOutputMode = OutputOverrideMode::AUTO;
  _displayOutputsEnabled = false;
  _displayLoggingEnabled = false;
  _displaySdMounted = false;
  _displayLoggingHealthy = false;
  _displayLogSamplesWritten = 0;
  _displaySystemHealth = HealthState::UNKNOWN;

  _scanPending = false;
  _scanAddress = 0x03;
  _scanActiveToken = 0;
  _scanActiveDeadlineMs = 0;
  _scanNextProbeMs = 0;
  _scan = I2cScanSnapshot{};
  _scan.nextAddress = 0x03;
  _scan.lastStatus = _enabled ? Status(Err::NOT_INITIALIZED, 0, "scan idle")
                              : Status(Err::NOT_INITIALIZED, 0, "I2C disabled");

  _rawPending = false;
  _rawRequest = I2cRequest{};
  _rawActiveToken = 0;
  _rawActiveDeadlineMs = 0;
  _raw = I2cRawSnapshot{};
  _raw.lastStatus = _enabled ? Status(Err::NOT_INITIALIZED, 0, "raw idle")
                             : Status(Err::NOT_INITIALIZED, 0, "I2C disabled");
  return Ok();
}

void I2cOrchestrator::end() {
  _enabled = false;
  _displayEnabled = false;
  _displayCo2Valid = false;
  _displayCo2Ppm = 0.0f;
  _displayCo2SampleMs = 0;
  _displayOutputMask = 0;
  _displayOutputMode = OutputOverrideMode::AUTO;
  _displayOutputsEnabled = false;
  _displayLoggingEnabled = false;
  _displaySdMounted = false;
  _displayLoggingHealthy = false;
  _displayLogSamplesWritten = 0;
  _displaySystemHealth = HealthState::UNKNOWN;
  _scanPending = false;
  _scan.active = false;
  _scanActiveToken = 0;
  _scanActiveDeadlineMs = 0;
  _rawPending = false;
  _raw.queued = false;
  _raw.active = false;
  _rawActiveToken = 0;
  _rawActiveDeadlineMs = 0;
  _port = nullptr;
}

void I2cOrchestrator::applySettings(const RuntimeSettings& settings) {
  _settings = settings;
#if CO2CONTROL_ENABLE_DISPLAY
  // Push a fresh display frame promptly after any settings update
  // (notably SSID/password changes), regardless of display poll interval.
  _displayNextPollMs = 0;
#endif
}

void I2cOrchestrator::setDisplayCo2Snapshot(float ppm, bool valid, uint32_t sampleMs) {
  _displayCo2Valid = valid;
  _displayCo2Ppm = ppm;
  _displayCo2SampleMs = sampleMs;
}

void I2cOrchestrator::setDisplayOutputSnapshot(uint8_t channelMask,
                                               OutputOverrideMode mode,
                                               bool outputsEnabled) {
  _displayOutputMask = static_cast<uint8_t>(channelMask & 0x0FU);
  _displayOutputMode = mode;
  _displayOutputsEnabled = outputsEnabled;
}

void I2cOrchestrator::setDisplaySystemSnapshot(bool loggingEnabled,
                                               bool sdMounted,
                                               bool loggingHealthy,
                                               uint32_t samplesWritten,
                                               HealthState systemHealth) {
  _displayLoggingEnabled = loggingEnabled;
  _displaySdMounted = sdMounted;
  _displayLoggingHealthy = loggingHealthy;
  _displayLogSamplesWritten = samplesWritten;
  _displaySystemHealth = systemHealth;
}

bool I2cOrchestrator::isExpired(uint32_t nowMs, uint32_t deadlineMs) const {
  if (deadlineMs == 0) {
    return false;
  }
  return static_cast<int32_t>(nowMs - deadlineMs) > 0;
}

uint32_t I2cOrchestrator::computeDeadlineMs(uint32_t nowMs, uint32_t timeoutMs) const {
  uint32_t effectiveTimeoutMs = timeoutMs;
  if (effectiveTimeoutMs == 0) {
    effectiveTimeoutMs = _settings.i2cOpTimeoutMs;
  }
  if (effectiveTimeoutMs < 5U) {
    effectiveTimeoutMs = 5U;
  }

  I2cBusMetrics metrics{};
  if (_port != nullptr) {
    metrics = _port->getMetrics();
  }

  const uint32_t queueDepth = static_cast<uint32_t>(metrics.requestQueueDepth + metrics.resultQueueDepth);
  const uint32_t queueSlots =
      queueDepth + static_cast<uint32_t>(_settings.i2cRequestsPerTick) + 2U;

  uint32_t slotBudgetMs = effectiveTimeoutMs + _settings.i2cTaskWaitMs + 20U;
  if (slotBudgetMs < 40U) {
    slotBudgetMs = 40U;
  }

  uint64_t queueBudgetMs64 = static_cast<uint64_t>(queueSlots) * static_cast<uint64_t>(slotBudgetMs);
  if (!metrics.deterministicTimeout) {
    queueBudgetMs64 *= 2ULL;
  }

  const uint32_t backendSlackMs = metrics.deterministicTimeout ? 120U : 250U;
  uint64_t deadlineDeltaMs64 =
      queueBudgetMs64 +
      (static_cast<uint64_t>(effectiveTimeoutMs) * 6ULL) +
      static_cast<uint64_t>(backendSlackMs);

  if (deadlineDeltaMs64 < 400ULL) {
    deadlineDeltaMs64 = 400ULL;
  }
  if (deadlineDeltaMs64 > 8000ULL) {
    deadlineDeltaMs64 = 8000ULL;
  }

  const uint32_t deadlineDeltaMs = static_cast<uint32_t>(deadlineDeltaMs64);
  return nowMs + deadlineDeltaMs;
}

bool I2cOrchestrator::enqueueRequest(I2cRequest& request, uint32_t nowMs) {
  if (!_enabled || _port == nullptr) {
    return false;
  }

  request.createdMs = nowMs;
  if (request.timeoutMs == 0) {
    request.timeoutMs = _settings.i2cOpTimeoutMs;
  }

  request.token = nextNonZeroToken(_nextToken);
  request.deadlineMs = computeDeadlineMs(nowMs, request.timeoutMs);

  const Status st = _port->enqueue(request, nowMs);
  if (!st.ok()) {
    _requestOverflowCount++;
    _lastOverflowMs = nowMs;
    return false;
  }

  return true;
}

void I2cOrchestrator::updateRtcSuccess(const RtcTime& time, uint32_t nowMs) {
  const bool firstTime = !_rtcHasTime;
  _rtcTime = time;
  _rtcHasTime = true;
  _rtcLastSuccessMs = nowMs;
  _rtcConsecutiveFailures = 0;
  _rtcConsecutiveSuccesses++;
  if (_rtcHealth == HealthState::OK) {
    _rtcStatus = Ok();
    return;
  }
  if (firstTime) {
    // First successful read — go straight to OK without hysteresis.
    _rtcHealth = HealthState::OK;
    _rtcStatus = Ok();
    return;
  }
  const uint32_t threshold = (_settings.i2cMaxConsecutiveFailures > 2U)
                                 ? _settings.i2cMaxConsecutiveFailures
                                 : 2U;
  if (_rtcConsecutiveSuccesses >= threshold) {
    _rtcHealth = HealthState::OK;
    _rtcStatus = Ok();
  } else if (_rtcHealth == HealthState::FAULT) {
    _rtcHealth = HealthState::DEGRADED;
    _rtcStatus = Status(Err::COMM_FAILURE, 0, "RTC recovering");
  } else {
    _rtcStatus = Status(Err::COMM_FAILURE, 0, "RTC recovering");
  }
}

void I2cOrchestrator::updateRtcFailure(const Status& status, uint32_t nowMs) {
  _rtcConsecutiveFailures++;
  _rtcConsecutiveSuccesses = 0;
  _rtcStatus = status;
  if (_rtcConsecutiveFailures >= (_settings.i2cMaxConsecutiveFailures * 2U)) {
    _rtcHealth = HealthState::FAULT;
    // Exponential backoff: double poll interval each failure, cap at 30 s.
    const uint32_t base = _settings.i2cRtcPollMs;
    const uint32_t shift = _rtcConsecutiveFailures < 16 ? _rtcConsecutiveFailures : 16;
    const uint64_t backoff64 = static_cast<uint64_t>(base) << shift;
    const uint32_t capped = (backoff64 > 30000ULL) ? 30000U : static_cast<uint32_t>(backoff64);
    _rtcNextPollMs = nowMs + capped;
  } else {
    _rtcHealth = HealthState::DEGRADED;
  }
}

void I2cOrchestrator::updateEnvFailure(const Status& status, uint32_t nowMs) {
  _envConsecutiveFailures++;
  _envConsecutiveSuccesses = 0;
  _envStatus = status;
  _envPhase = EnvPhase::IDLE;
  _envActiveTriggerToken = 0;
  _envActiveReadToken = 0;
  if (_envConsecutiveFailures >= (_settings.i2cMaxConsecutiveFailures * 2U)) {
    _envHealth = HealthState::FAULT;
    // Exponential backoff: double poll interval each failure, cap at 30 s.
    const uint32_t base = _settings.i2cEnvPollMs;
    const uint32_t shift = _envConsecutiveFailures < 16 ? _envConsecutiveFailures : 16;
    const uint64_t backoff64 = static_cast<uint64_t>(base) << shift;
    const uint32_t capped = (backoff64 > 30000ULL) ? 30000U : static_cast<uint32_t>(backoff64);
    _envNextPollMs = nowMs + capped;
  } else {
    _envHealth = HealthState::DEGRADED;
  }
}

void I2cOrchestrator::handleExpiredInFlight(uint32_t nowMs) {
  if (_rtcActiveReadToken != 0 && isExpired(nowMs, _rtcActiveReadDeadlineMs)) {
    _rtcActiveReadToken = 0;
    _rtcActiveReadDeadlineMs = 0;
    updateRtcFailure(Status(Err::TIMEOUT, 0, "RTC read deadline exceeded"), nowMs);
  }

  if (_rtcActiveSetToken != 0 && isExpired(nowMs, _rtcActiveSetDeadlineMs)) {
    _rtcActiveSetToken = 0;
    _rtcActiveSetDeadlineMs = 0;
    updateRtcFailure(Status(Err::TIMEOUT, 0, "RTC set deadline exceeded"), nowMs);
  }

  if (_envActiveTriggerToken != 0 && isExpired(nowMs, _envActiveTriggerDeadlineMs)) {
    _envActiveTriggerToken = 0;
    _envActiveTriggerDeadlineMs = 0;
    _envPhase = EnvPhase::IDLE;
    updateEnvFailure(Status(Err::TIMEOUT, 0, "ENV trigger deadline exceeded"), nowMs);
  }

  if (_envActiveReadToken != 0 && isExpired(nowMs, _envActiveReadDeadlineMs)) {
    _envActiveReadToken = 0;
    _envActiveReadDeadlineMs = 0;
    _envPhase = EnvPhase::IDLE;
    updateEnvFailure(Status(Err::TIMEOUT, 0, "ENV read deadline exceeded"), nowMs);
  }

  if (_busRecoverActiveToken != 0 && isExpired(nowMs, _busRecoverActiveDeadlineMs)) {
    _busRecoverActiveToken = 0;
    _busRecoverActiveDeadlineMs = 0;
    _busRecoverStatus = Status(Err::TIMEOUT, 0, "I2C recover deadline exceeded");
  }

  if (_rawActiveToken != 0 && isExpired(nowMs, _rawActiveDeadlineMs)) {
    _rawActiveToken = 0;
    _rawActiveDeadlineMs = 0;
    _raw.queued = false;
    _raw.active = false;
    _raw.complete = true;
    _raw.rxLen = 0;
    _raw.lastStatus = Status(Err::TIMEOUT, 0, "I2C raw deadline exceeded");
    _raw.updatedMs = nowMs;
  }

  if (_scanActiveToken != 0 && isExpired(nowMs, _scanActiveDeadlineMs)) {
    _scanActiveToken = 0;
    _scanActiveDeadlineMs = 0;
    _scan.probesTotal++;
    _scan.probesTimeout++;
    _scan.lastStatus = Status(Err::TIMEOUT, 0, "I2C scan probe deadline exceeded");
    _scan.updatedMs = nowMs;
    if (_scanAddress < 0x77U) {
      _scanAddress++;
      _scan.nextAddress = _scanAddress;
    } else {
      _scan.active = false;
      _scan.complete = true;
      _scan.nextAddress = 0x77;
      if (_scan.probesError == 0U && _scan.probesTimeout == 0U) {
        _scan.lastStatus = Ok();
      }
    }
  }
}

void I2cOrchestrator::processResult(const I2cResult& result, uint32_t nowMs) {
  if (result.late || isExpired(nowMs, result.requestDeadlineMs)) {
    _staleResultCount++;

    if (result.token == _rawActiveToken) {
      _rawActiveToken = 0;
      _rawActiveDeadlineMs = 0;
      _raw.queued = false;
      _raw.active = false;
      _raw.complete = true;
      _raw.rxLen = 0;
      _raw.lastStatus = Status(Err::TIMEOUT, 0, "I2C raw late result ignored");
      _raw.updatedMs = nowMs;
    }

    if (result.deviceId == DeviceId::RTC) {
      if (result.token == _rtcActiveReadToken) {
        _rtcActiveReadToken = 0;
        _rtcActiveReadDeadlineMs = 0;
        updateRtcFailure(Status(Err::TIMEOUT, 0, "RTC late result ignored"), nowMs);
      }
      if (result.token == _rtcActiveSetToken) {
        _rtcActiveSetToken = 0;
        _rtcActiveSetDeadlineMs = 0;
        updateRtcFailure(Status(Err::TIMEOUT, 0, "RTC late result ignored"), nowMs);
      }
    }

    if (result.deviceId == DeviceId::ENV) {
      if (result.token == _envActiveTriggerToken) {
        _envActiveTriggerToken = 0;
        _envActiveTriggerDeadlineMs = 0;
        _envPhase = EnvPhase::IDLE;
        updateEnvFailure(Status(Err::TIMEOUT, 0, "ENV late trigger ignored"), nowMs);
      }
      if (result.token == _envActiveReadToken) {
        _envActiveReadToken = 0;
        _envActiveReadDeadlineMs = 0;
        _envPhase = EnvPhase::IDLE;
        updateEnvFailure(Status(Err::TIMEOUT, 0, "ENV late read ignored"), nowMs);
      }
    }

    return;
  }

  if (result.deviceId == DeviceId::RTC) {
    if (result.token == _rtcActiveSetToken && result.op == I2cOpType::RTC_SET_TIME) {
      _rtcActiveSetToken = 0;
      _rtcActiveSetDeadlineMs = 0;
      if (result.status.ok()) {
        updateRtcSuccess(_rtcSetPendingValue, nowMs);
      } else {
        updateRtcFailure(result.status, nowMs);
      }
      return;
    }

    if (result.token == _rtcActiveReadToken &&
        (result.op == I2cOpType::READ || result.op == I2cOpType::WRITE_READ)) {
      _rtcActiveReadToken = 0;
      _rtcActiveReadDeadlineMs = 0;
      if (!result.status.ok()) {
        updateRtcFailure(result.status, nowMs);
        return;
      }

      RtcTime parsed{};
      const Status parseStatus = parseRtcTimePayload(result.data, result.dataLen, parsed);
      if (!parseStatus.ok()) {
        updateRtcFailure(parseStatus, nowMs);
        return;
      }

      updateRtcSuccess(parsed, nowMs);
      return;
    }
  }

  if (result.deviceId == DeviceId::ENV) {
    if (result.token == _envActiveTriggerToken && result.op == I2cOpType::ENV_TRIGGER_ONESHOT) {
      _envActiveTriggerToken = 0;
      _envActiveTriggerDeadlineMs = 0;
      if (!result.status.ok()) {
        _envPhase = EnvPhase::IDLE;
        if (result.status.code == Err::RESOURCE_BUSY) {
          // Sensor busy — not a real failure, will retry next poll.
          _envStatus = result.status;
        } else {
          updateEnvFailure(result.status, nowMs);
        }
      }
      return;
    }

    if (result.token == _envActiveReadToken && result.op == I2cOpType::ENV_READ_ONESHOT) {
      _envActiveReadToken = 0;
      _envActiveReadDeadlineMs = 0;
      if (!result.status.ok()) {
        _envPhase = EnvPhase::IDLE;
        if (result.status.code == Err::RESOURCE_BUSY) {
          // Measurement not ready — not a real failure, will retry next poll.
          _envStatus = result.status;
        } else {
          updateEnvFailure(result.status, nowMs);
        }
        return;
      }
      if (result.address != _settings.i2cEnvAddress) {
        _envPhase = EnvPhase::IDLE;
        _staleResultCount++;
        return;
      }
      EnvDecodedSample parsed{};
      const Status parseStatus =
          parseEnvSamplePayload(result.address, result.data, result.dataLen, parsed);
      if (!parseStatus.ok()) {
        _envPhase = EnvPhase::IDLE;
        updateEnvFailure(parseStatus, nowMs);
        return;
      }

      _envTempC = parsed.tempC;
      _envRhPct = parsed.rhPct;
      _envPressureHpa = parsed.pressureHpa;
      const bool firstData = !_envHasData;
      _envHasData = true;
      _envLastSuccessMs = nowMs;
      _envConsecutiveFailures = 0;
      _envConsecutiveSuccesses++;
      if (_envHealth == HealthState::OK) {
        _envStatus = Ok();
      } else if (firstData) {
        // First successful read — go straight to OK without hysteresis.
        // Hysteresis is for recovery after failures, not initial startup.
        _envHealth = HealthState::OK;
        _envStatus = Ok();
      } else {
        const uint32_t threshold = (_settings.i2cMaxConsecutiveFailures > 2U)
                                       ? _settings.i2cMaxConsecutiveFailures
                                       : 2U;
        if (_envConsecutiveSuccesses >= threshold) {
          _envHealth = HealthState::OK;
          _envStatus = Ok();
        } else if (_envHealth == HealthState::FAULT) {
          _envHealth = HealthState::DEGRADED;
          _envStatus = Status(Err::COMM_FAILURE, 0, "ENV recovering");
        } else {
          _envStatus = Status(Err::COMM_FAILURE, 0, "ENV recovering");
        }
      }
      _envPhase = EnvPhase::IDLE;
      return;
    }
  }

  if (result.token == _rawActiveToken) {
    _rawActiveToken = 0;
    _rawActiveDeadlineMs = 0;
    _raw.queued = false;
    _raw.active = false;
    _raw.complete = true;
    _raw.updatedMs = nowMs;
    _raw.lastStatus = result.status;
    _raw.rxLen = (result.dataLen > static_cast<uint8_t>(I2cRawSnapshot::MAX_BYTES))
                     ? static_cast<uint8_t>(I2cRawSnapshot::MAX_BYTES)
                     : result.dataLen;
    if (_raw.rxLen > 0U) {
      memcpy(_raw.rx, result.data, _raw.rxLen);
    }
    return;
  }

  if (result.deviceId == DeviceId::I2C_BUS &&
      result.op == I2cOpType::PROBE &&
      result.token == _scanActiveToken) {
    _scanActiveToken = 0;
    _scanActiveDeadlineMs = 0;
    _scan.probesTotal++;
    _scan.updatedMs = nowMs;
    if (result.status.ok()) {
      _scan.lastStatus = Ok();
      if (_scan.foundCount < static_cast<uint8_t>(I2cScanSnapshot::MAX_FOUND)) {
        _scan.foundAddresses[_scan.foundCount++] = result.address;
      }
    } else if (result.status.code == Err::TIMEOUT) {
      _scan.probesTimeout++;
      _scan.lastStatus = result.status;
    } else if (result.status.code == Err::COMM_FAILURE) {
      _scan.probesNack++;
    } else if (result.status.code != Err::COMM_FAILURE) {
      _scan.probesError++;
      _scan.lastStatus = result.status;
    }

    if (_scanAddress < 0x77U) {
      _scanAddress++;
      _scan.nextAddress = _scanAddress;
    } else {
      _scan.active = false;
      _scan.complete = true;
      _scan.nextAddress = 0x77;
      if (_scan.probesError == 0U && _scan.probesTimeout == 0U) {
        _scan.lastStatus = Status(Err::OK, 0, "I2C scan complete");
      }
    }
    return;
  }

  if (result.op == I2cOpType::RECOVER && result.token == _busRecoverActiveToken) {
    _busRecoverActiveToken = 0;
    _busRecoverActiveDeadlineMs = 0;
    _busRecoverStatus = result.status.ok()
                            ? Ok()
                            : result.status;
    return;
  }
}

void I2cOrchestrator::scheduleRequests(uint32_t nowMs) {
  if (!_enabled || _port == nullptr) {
    return;
  }

  uint8_t budget = _settings.i2cRequestsPerTick;

  if (budget > 0 && _busRecoverPending && _busRecoverActiveToken == 0) {
    I2cRequest req{};
    req.op = I2cOpType::RECOVER;
    req.deviceId = DeviceId::I2C_BUS;
    req.address = _settings.i2cRtcAddress;
    req.timeoutMs = _settings.i2cRecoverTimeoutMs;
    if (enqueueRequest(req, nowMs)) {
      _busRecoverActiveToken = req.token;
      _busRecoverActiveDeadlineMs = req.deadlineMs;
      _busRecoverStatus = Status(Err::RESOURCE_BUSY, 0, "I2C recover in progress");
    } else {
      _busRecoverStatus = Status(Err::RESOURCE_BUSY, 0, "I2C recover queue full");
    }
    _busRecoverPending = false;
    budget--;
  }

  if (budget > 0 && _rawPending && _rawActiveToken == 0) {
    I2cRequest req = _rawRequest;
    req.deviceId = DeviceId::I2C_BUS;
    if (enqueueRequest(req, nowMs)) {
      _rawPending = false;
      _rawActiveToken = req.token;
      _rawActiveDeadlineMs = req.deadlineMs;
      _raw.queued = false;
      _raw.active = true;
      _raw.complete = false;
      _raw.updatedMs = nowMs;
      _raw.lastStatus = Status(Err::RESOURCE_BUSY, 0, "I2C raw in progress");
    } else {
      _raw.queued = true;
      _raw.active = false;
      _raw.complete = false;
      _raw.updatedMs = nowMs;
      _raw.lastStatus = Status(Err::RESOURCE_BUSY, 0, "I2C raw queue full");
    }
    budget--;
  }

  if (budget > 0 && _rtcSetPending && _rtcActiveSetToken == 0) {
    I2cRequest req{};
    req.op = I2cOpType::RTC_SET_TIME;
    req.deviceId = DeviceId::RTC;
    req.address = _settings.i2cRtcAddress;
    req.txLen = 8;
    req.tx[0] = 0x00;
    req.tx[1] = rtcDecToBcd(_rtcSetPendingValue.second);
    req.tx[2] = rtcDecToBcd(_rtcSetPendingValue.minute);
    req.tx[3] = rtcDecToBcd(_rtcSetPendingValue.hour);
    req.tx[4] = 0;
    req.tx[5] = rtcDecToBcd(_rtcSetPendingValue.day);
    req.tx[6] = rtcDecToBcd(_rtcSetPendingValue.month);
    req.tx[7] = rtcDecToBcd(static_cast<uint8_t>(_rtcSetPendingValue.year % 100U));
    req.timeoutMs = rtcI2cTimeoutMs(_settings);
    if (enqueueRequest(req, nowMs)) {
      _rtcActiveSetToken = req.token;
      _rtcActiveSetDeadlineMs = req.deadlineMs;
      _rtcSetPending = false;
    } else {
      updateRtcFailure(Status(Err::RESOURCE_BUSY, 0, "I2C queue full"), nowMs);
    }
    budget--;
  }

  if (budget > 0 && _envPhase == EnvPhase::WAIT_CONVERSION &&
      static_cast<int32_t>(nowMs - _envPhaseDeadlineMs) >= 0 &&
      _envActiveReadToken == 0 && _envActiveTriggerToken == 0) {
    I2cRequest req{};
    req.op = I2cOpType::ENV_READ_ONESHOT;
    req.deviceId = DeviceId::ENV;
    req.address = _settings.i2cEnvAddress;
    req.rxLen = 6;
    req.timeoutMs = _settings.i2cOpTimeoutMs;
    if (enqueueRequest(req, nowMs)) {
      _envActiveReadToken = req.token;
      _envActiveReadDeadlineMs = req.deadlineMs;
      _envPhase = EnvPhase::WAIT_READ_RESULT;
    } else {
      _envPhase = EnvPhase::IDLE;
      updateEnvFailure(Status(Err::RESOURCE_BUSY, 0, "I2C queue full"), nowMs);
    }
    budget--;
  }

  if (budget > 0 && _rtcActiveReadToken == 0 &&
      static_cast<int32_t>(nowMs - _rtcNextPollMs) >= 0) {
    I2cRequest req{};
    req.op = I2cOpType::WRITE_READ;
    req.deviceId = DeviceId::RTC;
    req.address = _settings.i2cRtcAddress;
    req.txLen = 1;
    req.tx[0] = 0x00;
    req.rxLen = 7;
    req.timeoutMs = rtcI2cTimeoutMs(_settings);
    if (enqueueRequest(req, nowMs)) {
      _rtcActiveReadToken = req.token;
      _rtcActiveReadDeadlineMs = req.deadlineMs;
      _rtcNextPollMs = nowMs + _settings.i2cRtcPollMs;
    } else {
      updateRtcFailure(Status(Err::RESOURCE_BUSY, 0, "I2C queue full"), nowMs);
    }
    budget--;
  }

  if (budget > 0 && _envPhase == EnvPhase::IDLE && _envActiveTriggerToken == 0 &&
      static_cast<int32_t>(nowMs - _envNextPollMs) >= 0) {
    I2cRequest req{};
    req.op = I2cOpType::ENV_TRIGGER_ONESHOT;
    req.deviceId = DeviceId::ENV;
    req.address = _settings.i2cEnvAddress;
    if (isEnvBme280Address(_settings.i2cEnvAddress)) {
      req.txLen = 0;
    } else {
      req.txLen = 2;
      req.tx[0] = 0x24;
      req.tx[1] = 0x00;
    }
    req.timeoutMs = _settings.i2cOpTimeoutMs;
    if (enqueueRequest(req, nowMs)) {
      _envActiveTriggerToken = req.token;
      _envActiveTriggerDeadlineMs = req.deadlineMs;
      _envPhase = EnvPhase::WAIT_CONVERSION;
      _envPhaseDeadlineMs = nowMs + _settings.i2cEnvConversionWaitMs;
      _envNextPollMs = nowMs + _settings.i2cEnvPollMs;
    } else {
      updateEnvFailure(Status(Err::RESOURCE_BUSY, 0, "I2C queue full"), nowMs);
    }
    budget--;
  }

  if (_displayEnabled &&
      budget > 0 &&
      static_cast<int32_t>(nowMs - _displayNextPollMs) >= 0) {
    I2cRequest req{};
    req.op = I2cOpType::DISPLAY_REFRESH;
    req.deviceId = DeviceId::I2C_BUS;
    req.address = _settings.i2cDisplayAddress;
    uint32_t co2X10 = 0;
    if (_displayCo2Valid && _displayCo2Ppm >= 0.0f) {
      const float scaled = (_displayCo2Ppm * 10.0f) + 0.5f;
      if (scaled > 0.0f) {
        if (scaled > 1000000.0f) {
          co2X10 = 1000000U;
        } else {
          co2X10 = static_cast<uint32_t>(scaled);
        }
      }
    }
    req.tx[0] = _displayCo2Valid ? 1U : 0U;
    memcpy(&req.tx[1], &co2X10, sizeof(co2X10));
    memcpy(&req.tx[5], &_displayLogSamplesWritten, sizeof(_displayLogSamplesWritten));
    req.tx[9] = static_cast<uint8_t>(_displayOutputMask & 0x0FU);
    req.tx[10] = static_cast<uint8_t>(_displayOutputMode);
    req.tx[11] = _displayOutputsEnabled ? 1U : 0U;
    uint8_t logFlags = 0;
    if (_displayLoggingEnabled) {
      logFlags = static_cast<uint8_t>(logFlags | 0x01U);
    }
    if (_displaySdMounted) {
      logFlags = static_cast<uint8_t>(logFlags | 0x02U);
    }
    if (_displayLoggingHealthy) {
      logFlags = static_cast<uint8_t>(logFlags | 0x04U);
    }
    req.tx[12] = logFlags;
    req.tx[13] = static_cast<uint8_t>(_displaySystemHealth);
    req.txLen = 14;
    uint32_t displayTimeoutMs = _settings.i2cOpTimeoutMs * 4U;
    if (displayTimeoutMs < _settings.i2cOpTimeoutMs) {
      displayTimeoutMs = _settings.i2cOpTimeoutMs;
    }
    if (displayTimeoutMs < 80U) {
      displayTimeoutMs = 80U;
    }
    if (displayTimeoutMs > 500U) {
      displayTimeoutMs = 500U;
    }
    req.timeoutMs = displayTimeoutMs;
    enqueueRequest(req, nowMs);
    _displayNextPollMs = nowMs + _settings.i2cDisplayPollMs;
    budget--;
  }

  if (budget > 0 &&
      (_scanPending || _scan.active) &&
      _scanActiveToken == 0 &&
      static_cast<int32_t>(nowMs - _scanNextProbeMs) >= 0) {
    if (_scanPending) {
      _scanPending = false;
      _scan.active = true;
      _scan.complete = false;
      _scan.startedMs = nowMs;
      _scan.updatedMs = nowMs;
      _scan.probesTotal = 0;
      _scan.probesTimeout = 0;
      _scan.probesError = 0;
      _scan.probesNack = 0;
      _scan.foundCount = 0;
      _scanAddress = 0x03;
      _scan.nextAddress = _scanAddress;
      _scan.lastStatus = Status(Err::RESOURCE_BUSY, 0, "I2C scan started");
    }

    if (_scan.active) {
      I2cRequest req{};
      req.op = I2cOpType::PROBE;
      req.deviceId = DeviceId::I2C_BUS;
      req.address = _scanAddress;
      req.timeoutMs = _settings.i2cOpTimeoutMs;
      if (req.timeoutMs < 10U) {
        req.timeoutMs = 10U;
      }
      if (enqueueRequest(req, nowMs)) {
        _scanActiveToken = req.token;
        _scanActiveDeadlineMs = req.deadlineMs;
      } else {
        _scan.lastStatus = Status(Err::RESOURCE_BUSY, 0, "I2C scan queue full");
        _scan.updatedMs = nowMs;
        _scan.probesError++;
      }
      _scanNextProbeMs = nowMs + 5U;
      budget--;
    }
  }
}

void I2cOrchestrator::tick(uint32_t nowMs) {
  if (!_enabled || _port == nullptr) {
    return;
  }

  _port->tick(nowMs);

  size_t drained = 0;
  I2cResult result{};
  while (drained < static_cast<size_t>(_settings.i2cMaxResultsPerTick) && _port->dequeueResult(result)) {
    processResult(result, nowMs);
    drained++;
  }

  handleExpiredInFlight(nowMs);
  scheduleRequests(nowMs);
}

Status I2cOrchestrator::queueRtcSet(const RtcTime& time, uint32_t nowMs) {
  if (!_enabled || _port == nullptr) {
    return Status(Err::NOT_INITIALIZED, 0, "I2C disabled");
  }
  if (!isValidDateTime(time)) {
    return Status(Err::INVALID_CONFIG, 0, "RTC time invalid");
  }
  if (_rtcSetPending || _rtcActiveSetToken != 0) {
    return Status(Err::RESOURCE_BUSY, 0, "RTC set pending");
  }

  _rtcSetPending = true;
  _rtcSetPendingValue = time;
  _rtcStatus = Status(Err::RESOURCE_BUSY, 0, "RTC set queued");
  _rtcHealth = HealthState::DEGRADED;
  (void)nowMs;
  return Ok();
}

Status I2cOrchestrator::queueBusRecover(uint32_t nowMs) {
  (void)nowMs;
  if (!_enabled || _port == nullptr) {
    return Status(Err::NOT_INITIALIZED, 0, "I2C disabled");
  }
  if (_busRecoverPending || _busRecoverActiveToken != 0) {
    return Status(Err::RESOURCE_BUSY, 0, "I2C recover pending");
  }
  _busRecoverPending = true;
  _busRecoverStatus = Status(Err::RESOURCE_BUSY, 0, "I2C recover queued");
  return Ok();
}

Status I2cOrchestrator::queueBusScan(uint32_t nowMs) {
  if (!_enabled || _port == nullptr) {
    return Status(Err::NOT_INITIALIZED, 0, "I2C disabled");
  }
  if (_scanPending || _scan.active || _scanActiveToken != 0) {
    return Status(Err::RESOURCE_BUSY, 0, "I2C scan already running");
  }

  _scanPending = true;
  _scanAddress = 0x03;
  _scanActiveToken = 0;
  _scanActiveDeadlineMs = 0;
  _scanNextProbeMs = nowMs;
  _scan.active = false;
  _scan.complete = false;
  _scan.nextAddress = _scanAddress;
  _scan.startedMs = nowMs;
  _scan.updatedMs = nowMs;
  _scan.probesTotal = 0;
  _scan.probesTimeout = 0;
  _scan.probesError = 0;
  _scan.probesNack = 0;
  _scan.foundCount = 0;
  _scan.lastStatus = Status(Err::RESOURCE_BUSY, 0, "I2C scan queued");
  return Ok();
}

Status I2cOrchestrator::queueRawRequest(const I2cRequest& request, uint32_t nowMs) {
  if (!_enabled || _port == nullptr) {
    return Status(Err::NOT_INITIALIZED, 0, "I2C disabled");
  }
  if (_rawPending || _rawActiveToken != 0) {
    return Status(Err::RESOURCE_BUSY, 0, "I2C raw pending");
  }
  if (request.address < 0x01U || request.address > 0x7FU) {
    return Status(Err::INVALID_CONFIG, 0, "I2C address out of range");
  }

  const I2cRawOp rawOp = toRawOp(request.op);
  if (rawOp == I2cRawOp::NONE) {
    return Status(Err::INVALID_CONFIG, 0, "I2C raw op unsupported");
  }

  I2cRequest queued = request;
  queued.deviceId = DeviceId::I2C_BUS;
  if (queued.txLen > static_cast<uint8_t>(sizeof(queued.tx))) {
    return Status(Err::INVALID_CONFIG, 0, "I2C tx too long");
  }
  if (queued.rxLen > static_cast<uint8_t>(sizeof(queued.tx))) {
    return Status(Err::INVALID_CONFIG, 0, "I2C rx too long");
  }
  if (rawOp == I2cRawOp::WRITE && queued.txLen == 0U) {
    return Status(Err::INVALID_CONFIG, 0, "I2C write needs payload");
  }
  if (rawOp == I2cRawOp::READ && queued.rxLen == 0U) {
    return Status(Err::INVALID_CONFIG, 0, "I2C read length missing");
  }
  if (rawOp == I2cRawOp::WRITE_READ && (queued.txLen == 0U || queued.rxLen == 0U)) {
    return Status(Err::INVALID_CONFIG, 0, "I2C write_read requires tx+rx");
  }

  _rawPending = true;
  _rawRequest = queued;
  _rawActiveToken = 0;
  _rawActiveDeadlineMs = 0;
  _raw.queued = true;
  _raw.active = false;
  _raw.complete = false;
  _raw.op = rawOp;
  _raw.address = queued.address;
  _raw.txLen = queued.txLen;
  _raw.rxRequested = queued.rxLen;
  _raw.rxLen = 0;
  if (queued.txLen > 0U) {
    memcpy(_raw.tx, queued.tx, queued.txLen);
  }
  _raw.queuedMs = nowMs;
  _raw.updatedMs = nowMs;
  _raw.lastStatus = Status(Err::RESOURCE_BUSY, 0, "I2C raw queued");
  return Ok();
}

Status I2cOrchestrator::getRtcTime(uint32_t nowMs, RtcTime& out) const {
  (void)nowMs;
  if (_rtcHasTime) {
    out = _rtcTime;
    if (_rtcHealth == HealthState::OK) {
      return Ok();
    }
    return Status(Err::COMM_FAILURE, 0, "RTC stale data");
  }

  out.valid = false;
  return _rtcStatus;
}

Status I2cOrchestrator::fillEnvSample(Sample& sample, uint32_t nowMs) const {
  (void)nowMs;
  if (!_envHasData) {
    return _envStatus;
  }

  sample.tempC = _envTempC;
  sample.rhPct = _envRhPct;
  if (_envPressureHpa > 0.0f) {
    sample.pressureHpa = _envPressureHpa;
    sample.validMask |= VALID_PRESSURE;
  }
  sample.validMask |= VALID_TEMP;
  sample.validMask |= VALID_RH;

  if (_envHealth == HealthState::OK) {
    return Ok();
  }
  return Status(Err::COMM_FAILURE, 0, "ENV stale data");
}

HealthState I2cOrchestrator::busHealth() const {
  if (_port == nullptr) {
    return HealthState::FAULT;
  }
  return _port->health();
}

I2cBusMetrics I2cOrchestrator::busMetrics() const {
  if (_port == nullptr) {
    return I2cBusMetrics{};
  }

  I2cBusMetrics metrics = _port->getMetrics();
  metrics.requestOverflowCount += _requestOverflowCount;
  metrics.staleResultCount += _staleResultCount;
  return metrics;
}

Status I2cOrchestrator::busStatus() const {
  if (_port == nullptr) {
    return Status(Err::NOT_INITIALIZED, 0, "I2C task missing");
  }

  const I2cBusMetrics metrics = _port->getMetrics();
  if (!metrics.lastError.ok()) {
    return metrics.lastError;
  }

  if (busHealth() != HealthState::OK) {
    return Status(Err::COMM_FAILURE, 0, "I2C bus degraded");
  }

  return Ok();
}

}  // namespace CO2Control

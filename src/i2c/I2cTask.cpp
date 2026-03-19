#include "i2c/I2cTask.h"

#include <math.h>
#include <limits>
#include <stdio.h>

#include "core/SystemClock.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace TFLunaControl {

namespace {

inline uint32_t readNowMs() {
  return SystemClock::nowMs();
}

inline uint32_t nextNonZeroToken(uint32_t& nextToken) {
  uint32_t token = nextToken++;
  if (token == 0) {
    token = nextToken++;
  }
  if (nextToken == 0) {
    nextToken = 1;
  }
  return token;
}

inline bool isBme280Address(uint8_t address) {
  return address == 0x76U || address == 0x77U;
}

inline bool isSht3xAddress(uint8_t address) {
  return address == 0x44U || address == 0x45U;
}

inline uint8_t bcdToDec(uint8_t value) {
  return static_cast<uint8_t>(((value >> 4U) * 10U) + (value & 0x0FU));
}

inline int16_t clampInt16(int32_t value) {
  if (value < static_cast<int32_t>(std::numeric_limits<int16_t>::min())) {
    return std::numeric_limits<int16_t>::min();
  }
  if (value > static_cast<int32_t>(std::numeric_limits<int16_t>::max())) {
    return std::numeric_limits<int16_t>::max();
  }
  return static_cast<int16_t>(value);
}

inline uint16_t clampUint16(int32_t value) {
  if (value < 0) {
    return 0U;
  }
  if (value > static_cast<int32_t>(std::numeric_limits<uint16_t>::max())) {
    return std::numeric_limits<uint16_t>::max();
  }
  return static_cast<uint16_t>(value);
}

inline int32_t roundToInt32(float value) {
  if (value >= static_cast<float>(std::numeric_limits<int32_t>::max())) {
    return std::numeric_limits<int32_t>::max();
  }
  if (value <= static_cast<float>(std::numeric_limits<int32_t>::min())) {
    return std::numeric_limits<int32_t>::min();
  }
  return static_cast<int32_t>(lroundf(value));
}

#if TFLUNACTRL_HAS_BME280_LIB
inline BME280::Mode toBmeMode(uint8_t raw) {
  switch (raw) {
    case 0:
      return BME280::Mode::SLEEP;
    case 1:
      return BME280::Mode::FORCED;
    case 3:
      return BME280::Mode::NORMAL;
    default:
      return BME280::Mode::FORCED;
  }
}

inline BME280::Oversampling toBmeOversampling(uint8_t raw) {
  switch (raw) {
    case 0:
      return BME280::Oversampling::SKIP;
    case 1:
      return BME280::Oversampling::X1;
    case 2:
      return BME280::Oversampling::X2;
    case 3:
      return BME280::Oversampling::X4;
    case 4:
      return BME280::Oversampling::X8;
    case 5:
      return BME280::Oversampling::X16;
    default:
      return BME280::Oversampling::X1;
  }
}

inline BME280::Filter toBmeFilter(uint8_t raw) {
  switch (raw) {
    case 0:
      return BME280::Filter::OFF;
    case 1:
      return BME280::Filter::X2;
    case 2:
      return BME280::Filter::X4;
    case 3:
      return BME280::Filter::X8;
    case 4:
      return BME280::Filter::X16;
    default:
      return BME280::Filter::OFF;
  }
}

inline BME280::Standby toBmeStandby(uint8_t raw) {
  switch (raw) {
    case 0:
      return BME280::Standby::MS_0_5;
    case 1:
      return BME280::Standby::MS_62_5;
    case 2:
      return BME280::Standby::MS_125;
    case 3:
      return BME280::Standby::MS_250;
    case 4:
      return BME280::Standby::MS_500;
    case 5:
      return BME280::Standby::MS_1000;
    case 6:
      return BME280::Standby::MS_10;
    case 7:
      return BME280::Standby::MS_20;
    default:
      return BME280::Standby::MS_125;
  }
}
#endif

#if TFLUNACTRL_HAS_SHT3X_LIB
inline SHT3x::Mode toShtMode(uint8_t raw) {
  switch (raw) {
    case 0:
      return SHT3x::Mode::SINGLE_SHOT;
    case 1:
      return SHT3x::Mode::PERIODIC;
    case 2:
      return SHT3x::Mode::ART;
    default:
      return SHT3x::Mode::SINGLE_SHOT;
  }
}

inline SHT3x::Repeatability toShtRepeatability(uint8_t raw) {
  switch (raw) {
    case 0:
      return SHT3x::Repeatability::LOW_REPEATABILITY;
    case 1:
      return SHT3x::Repeatability::MEDIUM_REPEATABILITY;
    case 2:
      return SHT3x::Repeatability::HIGH_REPEATABILITY;
    default:
      return SHT3x::Repeatability::HIGH_REPEATABILITY;
  }
}

inline SHT3x::PeriodicRate toShtPeriodicRate(uint8_t raw) {
  switch (raw) {
    case 0:
      return SHT3x::PeriodicRate::MPS_0_5;
    case 1:
      return SHT3x::PeriodicRate::MPS_1;
    case 2:
      return SHT3x::PeriodicRate::MPS_2;
    case 3:
      return SHT3x::PeriodicRate::MPS_4;
    case 4:
      return SHT3x::PeriodicRate::MPS_10;
    default:
      return SHT3x::PeriodicRate::MPS_1;
  }
}

inline SHT3x::ClockStretching toShtClockStretching(uint8_t raw) {
  return (raw == 1U) ? SHT3x::ClockStretching::STRETCH_ENABLED
                     : SHT3x::ClockStretching::STRETCH_DISABLED;
}
#endif

#if TFLUNACTRL_HAS_RV3032_LIB
inline RV3032::BackupSwitchMode toRtcBackupMode(uint8_t raw) {
  switch (raw) {
    case 0:
      return RV3032::BackupSwitchMode::Off;
    case 1:
      return RV3032::BackupSwitchMode::Level;
    case 2:
      return RV3032::BackupSwitchMode::Direct;
    default:
      return RV3032::BackupSwitchMode::Level;
  }
}

constexpr uint8_t kRtcRegStatus = 0x0DU;
constexpr uint8_t kRtcRegTempLsb = 0x0EU;
constexpr uint8_t kRtcRegPmu = 0xC0U;
constexpr uint8_t kRtcStatusVlfBit = 0U;
constexpr uint8_t kRtcStatusPorfBit = 1U;
constexpr uint8_t kRtcTempBsfBit = 0U;
constexpr uint8_t kRtcPmuBackupMask = 0x30U;
constexpr uint8_t kRtcPmuBackupLevel = 0x20U;
constexpr uint8_t kRtcPmuBackupDirect = 0x10U;

inline uint8_t decodeRtcBackupMode(uint8_t pmuReg, bool& valid) {
  const uint8_t mode = static_cast<uint8_t>(pmuReg & kRtcPmuBackupMask);
  valid = true;
  if (mode == 0U) {
    return 0U;
  }
  if (mode == kRtcPmuBackupLevel) {
    return 1U;
  }
  if (mode == kRtcPmuBackupDirect) {
    return 2U;
  }
  valid = false;
  return 0U;
}
#endif

inline const char* errShortLabel(Err code) {
  switch (code) {
    case Err::OK:
      return "ok";
    case Err::INVALID_CONFIG:
      return "cfg";
    case Err::TIMEOUT:
      return "tout";
    case Err::BUS_STUCK:
      return "stuck";
    case Err::RESOURCE_BUSY:
      return "busy";
    case Err::COMM_FAILURE:
      return "comm";
    case Err::NOT_INITIALIZED:
      return "init";
    case Err::OUT_OF_MEMORY:
      return "oom";
    case Err::HARDWARE_FAULT:
      return "hw";
    case Err::EXTERNAL_LIB_ERROR:
      return "ext";
    case Err::DATA_CORRUPT:
      return "data";
    case Err::INTERNAL_ERROR:
      return "int";
    default:
      return "unk";
  }
}

#if TFLUNACTRL_HAS_SSD1315_LIB
inline const char* healthShortLabel(HealthState health) {
  switch (health) {
    case HealthState::OK:
      return "OK";
    case HealthState::DEGRADED:
      return "DEG";
    case HealthState::FAULT:
      return "FAULT";
    case HealthState::UNKNOWN:
    default:
      return "UNK";
  }
}
#endif

}  // namespace

II2cBackend* I2cTask::selectBackend() {
  if (_backendOverride != nullptr) {
    return _backendOverride;
  }
  return &_idfBackend;
}

Status I2cTask::begin(const HardwareSettings& config, const RuntimeSettings& settings) {
  _config = config;
  _settings = settings;
  _enabled = (config.i2cSda >= 0 && config.i2cScl >= 0);
  _running = false;
  _nextToken = 1;
  _metrics = I2cBusMetrics{};
  _metrics.powerCycleConfigured = false;
  _metrics.lastPowerCycleStatus = Status(Err::NOT_INITIALIZED, 0, "not configured");
  _lastRequestOverflowMs = 0;
  _lastResultDropMs = 0;
  _lastStaleResultMs = 0;
  _slowWindowStartMs = UINT32_MAX;
  _slowWindowCount = 0;
#if TFLUNACTRL_HAS_BME280_LIB
  _envBme.end();
  _envBmeInitialized = false;
  _envBmeAddress = 0;
  _envBmeTimeoutMs = 0;
#endif
#if TFLUNACTRL_HAS_SHT3X_LIB
  _envSht.end();
  _envShtInitialized = false;
  _envShtAddress = 0;
  _envShtTimeoutMs = 0;
#endif
#if TFLUNACTRL_HAS_RV3032_LIB
  _rtcRv3032.end();
  _rtcRv3032Initialized = false;
  _rtcRv3032Address = 0;
  _rtcRv3032TimeoutMs = 0;
#endif
  resetRtcDebugSnapshot(0);
#if TFLUNACTRL_HAS_SSD1315_LIB
  _display.end();
  _displayInitialized = false;
  _displayAddress = 0;
  _displayTimeoutMs = 0;
  _displayOfflineThreshold = 0;
  _displayFlipX = false;
  _displayFlipY = false;
  _displayNextRecoverMs = 0;
  _displayRtcStatus = Status(Err::NOT_INITIALIZED, 0, "RTC waiting first sample");
  _displayEnvStatus = Status(Err::NOT_INITIALIZED, 0, "ENV waiting first sample");
  _displayRtcValid = false;
  _displayRtc = RtcTime{};
  _displayRtcSampleMs = 0;
  _displayEnvValid = false;
  _displayEnvTempC = 0.0f;
  _displayEnvRhPct = 0.0f;
  _displayEnvPressureHpa = 0.0f;
  _displayEnvHasPressure = false;
  _displayEnvSampleMs = 0;
  _displayCo2Valid = false;
  _displayCo2Ppm = 0.0f;
  _displayCo2SampleMs = 0;
  _displayOutputMask = 0;
  _displayOutputMode = OutputOverrideMode::AUTO;
  _displayOutputsEnabled = false;
  _displayLogEnabled = false;
  _displayLogMounted = false;
  _displayLogHealthy = false;
  _displayLogSamplesWritten = 0;
  _displaySystemHealth = HealthState::UNKNOWN;
#endif

  _recoveryPolicy.configure(settings.i2cMaxConsecutiveFailures,
                            settings.i2cRecoveryBackoffMs,
                            settings.i2cRecoveryBackoffMaxMs);
  _gpioProbe.configure(config.i2cSda, config.i2cScl, settings.i2cStuckDebounceMs);

  if (!_enabled) {
    _metrics.lastError = Status(Err::INVALID_CONFIG, 0, "I2C pins not set");
    return _metrics.lastError;
  }

  _backendConfig.sdaPin = config.i2cSda;
  _backendConfig.sclPin = config.i2cScl;
  _backendConfig.freqHz = settings.i2cFreqHz;
  _backendConfig.timeoutMs = settings.i2cOpTimeoutMs;
  _powerHook = (_powerHookOverride != nullptr) ? _powerHookOverride : _config.i2cPowerCycleHook;
  _powerHookContext =
      (_powerHookOverride != nullptr) ? _powerHookContextOverride : _config.i2cPowerCycleContext;
  _metrics.powerCycleConfigured = (_powerHook != nullptr);

  _backend = selectBackend();
  if (_backend == nullptr || !_backend->isAvailable()) {
    _metrics.lastError = Status(Err::NOT_INITIALIZED, 0, "I2C backend unavailable");
    return _metrics.lastError;
  }
  _metrics.backendName = _backend->name();
  _metrics.deterministicTimeout = _backend->supportsDeterministicTimeout();

#ifdef ARDUINO
  if (_metricsMutex == nullptr) {
    _metricsMutex = xSemaphoreCreateMutex();
    if (_metricsMutex == nullptr) {
      return Status(Err::OUT_OF_MEMORY, 0, "I2C metrics mutex alloc failed");
    }
  }

  if (_completionSem == nullptr) {
    _completionSem = xSemaphoreCreateBinary();
    if (_completionSem == nullptr) {
      return Status(Err::OUT_OF_MEMORY, 0, "I2C completion sem alloc failed");
    }
  }

  if (_requestQueue == nullptr) {
    _requestQueue = xQueueCreate(REQUEST_QUEUE_CAPACITY, sizeof(I2cRequest));
    if (_requestQueue == nullptr) {
      return Status(Err::OUT_OF_MEMORY, 0, "I2C request queue alloc failed");
    }
  }

  if (_resultQueue == nullptr) {
    _resultQueue = xQueueCreate(RESULT_QUEUE_CAPACITY, sizeof(I2cResult));
    if (_resultQueue == nullptr) {
      return Status(Err::OUT_OF_MEMORY, 0, "I2C result queue alloc failed");
    }
  }
#else
  _requestQueueNative.clear();
  _resultQueueNative.clear();
#endif

  Status beginStatus = _backend->begin(_backendConfig);
  if (!beginStatus.ok()) {
    updateMetricsError(beginStatus, 0);
    _recoveryPolicy.onFailure();
  }

  if (isBusStuck(0)) {
    beginStatus = Status(Err::BUS_STUCK, 0, "I2C bus stuck at boot");
    updateMetricsError(beginStatus, 0);
    _recoveryPolicy.onFailure();
    const Status recover = recoverBus(0);
    if (!recover.ok()) {
      beginStatus = recover;
    }
  }

#ifdef ARDUINO
  _running = true;
  if (_taskHandle == nullptr) {
    const BaseType_t created = xTaskCreatePinnedToCore(taskEntry,
                                                       "co2_i2c",
                                                       _config.i2cTaskStack,
                                                       this,
                                                       _config.i2cTaskPriority,
                                                       &_taskHandle,
                                                       0);
    if (created != pdPASS) {
      _running = false;
      _taskHandle = nullptr;
      return Status(Err::OUT_OF_MEMORY, 0, "I2C task create failed");
    }
  }
#else
  _running = true;
#endif

  updateTaskHeartbeat(0);
  return beginStatus;
}

void I2cTask::end() {
  _running = false;

#ifdef ARDUINO
  // M3 fix: wait for the task to self-exit via its own vTaskDelete(nullptr)
  // instead of killing it from outside, which can leave the I2C driver in
  // an inconsistent state if the task is mid-transfer.
  if (_taskHandle != nullptr && _completionSem != nullptr) {
    if (xSemaphoreTake(_completionSem, pdMS_TO_TICKS(2000)) != pdTRUE) {
      // Task did not exit in time -- force-delete as last resort.
      vTaskDelete(_taskHandle);
    }
    _taskHandle = nullptr;
  } else if (_taskHandle != nullptr) {
    vTaskDelete(_taskHandle);
    _taskHandle = nullptr;
  }

  if (_requestQueue != nullptr) {
    vQueueDelete(_requestQueue);
    _requestQueue = nullptr;
  }

  if (_resultQueue != nullptr) {
    vQueueDelete(_resultQueue);
    _resultQueue = nullptr;
  }

  if (_completionSem != nullptr) {
    vSemaphoreDelete(_completionSem);
    _completionSem = nullptr;
  }

  if (_metricsMutex != nullptr) {
    vSemaphoreDelete(_metricsMutex);
    _metricsMutex = nullptr;
  }
#else
  _requestQueueNative.clear();
  _resultQueueNative.clear();
#endif

#if TFLUNACTRL_HAS_SSD1315_LIB
  _display.end();
  _displayInitialized = false;
  _displayAddress = 0;
  _displayTimeoutMs = 0;
  _displayOfflineThreshold = 0;
  _displayFlipX = false;
  _displayFlipY = false;
  _displayNextRecoverMs = 0;
  _displayCo2Valid = false;
  _displayCo2Ppm = 0.0f;
  _displayCo2SampleMs = 0;
  _displayOutputMask = 0;
  _displayOutputMode = OutputOverrideMode::AUTO;
  _displayOutputsEnabled = false;
  _displayLogEnabled = false;
  _displayLogMounted = false;
  _displayLogHealthy = false;
  _displayLogSamplesWritten = 0;
  _displaySystemHealth = HealthState::UNKNOWN;
#endif

  if (_backend != nullptr) {
    _backend->end();
  }

#if TFLUNACTRL_HAS_BME280_LIB
  _envBme.end();
  _envBmeInitialized = false;
  _envBmeAddress = 0;
  _envBmeTimeoutMs = 0;
#endif
#if TFLUNACTRL_HAS_SHT3X_LIB
  _envSht.end();
  _envShtInitialized = false;
  _envShtAddress = 0;
  _envShtTimeoutMs = 0;
#endif
#if TFLUNACTRL_HAS_RV3032_LIB
  _rtcRv3032.end();
  _rtcRv3032Initialized = false;
  _rtcRv3032Address = 0;
  _rtcRv3032TimeoutMs = 0;
#endif
  resetRtcDebugSnapshot(readNowMs());
}

Status I2cTask::enqueue(const I2cRequest& request, uint32_t nowMs) {
  if (!_running || !_enabled) {
    return Status(Err::NOT_INITIALIZED, 0, "I2C task not running");
  }

  I2cRequest queued = request;
  queued.createdMs = nowMs;
  if (queued.timeoutMs == 0) {
    queued.timeoutMs = _settings.i2cOpTimeoutMs;
  }
  if (queued.deadlineMs == 0) {
    queued.deadlineMs = nowMs + queued.timeoutMs;
  }
  if (queued.token == 0) {
    queued.token = nextNonZeroToken(_nextToken);
  }

#ifdef ARDUINO
  if (_requestQueue == nullptr) {
    return Status(Err::NOT_INITIALIZED, 0, "I2C request queue missing");
  }

  if (xQueueSend(_requestQueue, &queued, 0) != pdTRUE) {
    markRequestOverflow(nowMs);
    return Status(Err::RESOURCE_BUSY, 0, "I2C request queue full");
  }
#else
  if (!_requestQueueNative.push(queued, nowMs)) {
    markRequestOverflow(nowMs);
    return Status(Err::RESOURCE_BUSY, 0, "I2C request queue full");
  }
#endif

  return Ok();
}

bool I2cTask::dequeueResult(I2cResult& out) {
  if (!_running || !_enabled) {
    return false;
  }

#ifdef ARDUINO
  if (_resultQueue == nullptr) {
    return false;
  }
  return xQueueReceive(_resultQueue, &out, 0) == pdTRUE;
#else
  return _resultQueueNative.pop(out);
#endif
}

void I2cTask::tick(uint32_t nowMs) {
#ifndef ARDUINO
  if (!_running || !_enabled) {
    return;
  }
  updateTaskHeartbeat(nowMs);

  I2cRequest request{};
  if (_requestQueueNative.pop(request)) {
    const I2cResult result = processRequest(request, nowMs);
    queueResult(result, nowMs);
  }
#else
  (void)nowMs;
#endif
}

void I2cTask::applySettings(const RuntimeSettings& settings, uint32_t nowMs) {
  RuntimeSettings oldSettings{};

  // H2 fix: protect _settings and _backendConfig writes against concurrent
  // reads from the I2C FreeRTOS task (torn-read prevention).
#ifdef ARDUINO
  if (_metricsMutex && xSemaphoreTake(_metricsMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    oldSettings = _settings;
    _settings = settings;
    _backendConfig.freqHz = settings.i2cFreqHz;
    _backendConfig.timeoutMs = settings.i2cOpTimeoutMs;
    xSemaphoreGive(_metricsMutex);
  } else {
    oldSettings = _settings;
    _settings = settings;
    _backendConfig.freqHz = settings.i2cFreqHz;
    _backendConfig.timeoutMs = settings.i2cOpTimeoutMs;
  }
#else
  oldSettings = _settings;
  _settings = settings;
  _backendConfig.freqHz = settings.i2cFreqHz;
  _backendConfig.timeoutMs = settings.i2cOpTimeoutMs;
#endif

  _recoveryPolicy.configure(settings.i2cMaxConsecutiveFailures,
                            settings.i2cRecoveryBackoffMs,
                            settings.i2cRecoveryBackoffMaxMs);
  _gpioProbe.configure(_config.i2cSda, _config.i2cScl, settings.i2cStuckDebounceMs);

  // H1 fix: do not call _backend->applyConfig() directly from the main task.
  // Instead, enqueue a SET_FREQ request so reconfiguration happens exclusively
  // in the I2C task context, avoiding a data race with concurrent transfers.
  if (_backend != nullptr && _running && _enabled) {
    I2cRequest freqReq{};
    freqReq.op = I2cOpType::SET_FREQ;
    freqReq.deviceId = DeviceId::I2C_BUS;
    freqReq.timeoutMs = settings.i2cOpTimeoutMs;
    freqReq.deadlineMs = nowMs + settings.i2cOpTimeoutMs;
    enqueue(freqReq, nowMs);
  }

  // SAFETY: Do NOT call library .end() methods here.  applySettings() runs
  // in the main-task context, but the I2C driver handle is owned exclusively
  // by the I2C FreeRTOS task.  Library .end() methods may issue I2C
  // transactions (e.g. BME280 sleep command) which would dereference a null
  // driver handle and crash.  Instead, just mark the driver as
  // uninitialized; the next ensureXxxReady() call inside the I2C task will
  // safely call .end() + .begin() in the correct task context.

#if TFLUNACTRL_HAS_BME280_LIB
  if (_envBmeInitialized &&
      (oldSettings.i2cEnvAddress != settings.i2cEnvAddress ||
       oldSettings.i2cOpTimeoutMs != settings.i2cOpTimeoutMs ||
       oldSettings.i2cMaxConsecutiveFailures != settings.i2cMaxConsecutiveFailures ||
       oldSettings.i2cEnvBmeMode != settings.i2cEnvBmeMode ||
       oldSettings.i2cEnvBmeOsrsT != settings.i2cEnvBmeOsrsT ||
       oldSettings.i2cEnvBmeOsrsP != settings.i2cEnvBmeOsrsP ||
       oldSettings.i2cEnvBmeOsrsH != settings.i2cEnvBmeOsrsH ||
       oldSettings.i2cEnvBmeFilter != settings.i2cEnvBmeFilter ||
       oldSettings.i2cEnvBmeStandby != settings.i2cEnvBmeStandby)) {
    _envBmeInitialized = false;
    _envBmeAddress = 0;
    _envBmeTimeoutMs = 0;
  }
#endif
#if TFLUNACTRL_HAS_SHT3X_LIB
  if (_envShtInitialized &&
      (oldSettings.i2cEnvAddress != settings.i2cEnvAddress ||
       oldSettings.i2cOpTimeoutMs != settings.i2cOpTimeoutMs ||
       oldSettings.i2cMaxConsecutiveFailures != settings.i2cMaxConsecutiveFailures ||
       oldSettings.i2cRecoveryBackoffMs != settings.i2cRecoveryBackoffMs ||
       oldSettings.i2cEnvShtMode != settings.i2cEnvShtMode ||
       oldSettings.i2cEnvShtRepeatability != settings.i2cEnvShtRepeatability ||
       oldSettings.i2cEnvShtPeriodicRate != settings.i2cEnvShtPeriodicRate ||
       oldSettings.i2cEnvShtClockStretching != settings.i2cEnvShtClockStretching ||
       oldSettings.i2cEnvShtLowVdd != settings.i2cEnvShtLowVdd ||
       oldSettings.i2cEnvShtCommandDelayMs != settings.i2cEnvShtCommandDelayMs ||
       oldSettings.i2cEnvShtNotReadyTimeoutMs != settings.i2cEnvShtNotReadyTimeoutMs ||
       oldSettings.i2cEnvShtPeriodicFetchMarginMs != settings.i2cEnvShtPeriodicFetchMarginMs ||
       oldSettings.i2cEnvShtAllowGeneralCallReset != settings.i2cEnvShtAllowGeneralCallReset ||
       oldSettings.i2cEnvShtRecoverUseBusReset != settings.i2cEnvShtRecoverUseBusReset ||
       oldSettings.i2cEnvShtRecoverUseSoftReset != settings.i2cEnvShtRecoverUseSoftReset ||
       oldSettings.i2cEnvShtRecoverUseHardReset != settings.i2cEnvShtRecoverUseHardReset)) {
    _envShtInitialized = false;
    _envShtAddress = 0;
    _envShtTimeoutMs = 0;
  }
#endif
#if TFLUNACTRL_HAS_RV3032_LIB
  if (_rtcRv3032Initialized &&
      (oldSettings.i2cRtcAddress != settings.i2cRtcAddress ||
       oldSettings.i2cOpTimeoutMs != settings.i2cOpTimeoutMs ||
       oldSettings.i2cRtcBackupMode != settings.i2cRtcBackupMode ||
       oldSettings.i2cRtcEnableEepromWrites != settings.i2cRtcEnableEepromWrites ||
       oldSettings.i2cRtcEepromTimeoutMs != settings.i2cRtcEepromTimeoutMs ||
       oldSettings.i2cRtcOfflineThreshold != settings.i2cRtcOfflineThreshold)) {
    _rtcRv3032Initialized = false;
    _rtcRv3032Address = 0;
    _rtcRv3032TimeoutMs = 0;
  }
#endif
#if TFLUNACTRL_HAS_SSD1315_LIB
  if (_displayInitialized &&
      (oldSettings.i2cDisplayAddress != settings.i2cDisplayAddress ||
       oldSettings.i2cOpTimeoutMs != settings.i2cOpTimeoutMs ||
       oldSettings.i2cMaxConsecutiveFailures != settings.i2cMaxConsecutiveFailures)) {
    _displayInitialized = false;
    _displayAddress = 0;
    _displayTimeoutMs = 0;
    _displayOfflineThreshold = 0;
    _displayFlipX = false;
    _displayFlipY = false;
    _displayNextRecoverMs = 0;
  }
#endif
  resetRtcDebugSnapshot(nowMs);
}

I2cBusMetrics I2cTask::getMetrics() const {
#ifdef ARDUINO
  I2cBusMetrics copy{};
  if (_metricsMutex && xSemaphoreTake(_metricsMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
    copy = _metrics;
    copy.requestQueueDepth = (_requestQueue != nullptr) ? uxQueueMessagesWaiting(_requestQueue) : 0;
    copy.resultQueueDepth = (_resultQueue != nullptr) ? uxQueueMessagesWaiting(_resultQueue) : 0;
    if (_taskHandle != nullptr) {
      copy.taskStackFreeBytes = uxTaskGetStackHighWaterMark(_taskHandle) * sizeof(StackType_t);
    }
    xSemaphoreGive(_metricsMutex);
  }
  return copy;
#else
  I2cBusMetrics copy = _metrics;
  copy.requestQueueDepth = _requestQueueNative.depth();
  copy.resultQueueDepth = _resultQueueNative.depth();
  return copy;
#endif
}

RtcDebugSnapshot I2cTask::getRtcDebugSnapshot() const {
#ifdef ARDUINO
  RtcDebugSnapshot copy{};
  if (_metricsMutex && xSemaphoreTake(_metricsMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
    copy = _rtcDebug;
    xSemaphoreGive(_metricsMutex);
  } else {
    copy = _rtcDebug;
  }
  return copy;
#else
  return _rtcDebug;
#endif
}

RtcDebugSnapshot I2cTask::makeRtcDebugBase(uint32_t nowMs) const {
  RtcDebugSnapshot snapshot{};
#if TFLUNACTRL_HAS_RV3032_LIB
  snapshot.supported = true;
  snapshot.initialized = _rtcRv3032Initialized;
  snapshot.address =
      (_rtcRv3032Address != 0U) ? _rtcRv3032Address : _settings.i2cRtcAddress;
  snapshot.effectiveI2cTimeoutMs =
      (_rtcRv3032TimeoutMs != 0U) ? _rtcRv3032TimeoutMs : rtcI2cTimeoutMs(_settings);
#else
  snapshot.address = _settings.i2cRtcAddress;
  snapshot.effectiveI2cTimeoutMs = rtcI2cTimeoutMs(_settings);
#endif
  snapshot.enabled = _enabled;
  snapshot.requestedI2cTimeoutMs = _settings.i2cOpTimeoutMs;
  snapshot.requestedBackupMode = _settings.i2cRtcBackupMode;
  snapshot.requestedEepromWrites = _settings.i2cRtcEnableEepromWrites;
  snapshot.effectiveEepromWrites = rtcEepromWritesEnabled(_settings);
  snapshot.eepromTimeoutMs = _settings.i2cRtcEepromTimeoutMs;
  snapshot.offlineThreshold = _settings.i2cRtcOfflineThreshold;
#if TFLUNACTRL_HAS_RV3032_LIB
  snapshot.driverState = static_cast<uint8_t>(_rtcRv3032.state());
  if (_rtcRv3032Initialized) {
    snapshot.eepromBusy = _rtcRv3032.isEepromBusy();
    snapshot.eepromStatus = mapRtcStatus(_rtcRv3032.getEepromStatus());
    snapshot.eepromWriteCount = _rtcRv3032.eepromWriteCount();
    snapshot.eepromWriteFailures = _rtcRv3032.eepromWriteFailures();
    snapshot.eepromQueueDepth = _rtcRv3032.eepromQueueDepth();
  }
#endif
  snapshot.updatedMs = nowMs;
  return snapshot;
}

void I2cTask::storeRtcDebugSnapshot(const RtcDebugSnapshot& snapshot) {
#ifdef ARDUINO
  if (_metricsMutex && xSemaphoreTake(_metricsMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
    _rtcDebug = snapshot;
    xSemaphoreGive(_metricsMutex);
    return;
  }
#endif
  _rtcDebug = snapshot;
}

void I2cTask::resetRtcDebugSnapshot(uint32_t nowMs) {
  storeRtcDebugSnapshot(makeRtcDebugBase(nowMs));
}

void I2cTask::refreshRtcDebugSnapshot(uint32_t nowMs) {
  RtcDebugSnapshot snapshot = makeRtcDebugBase(nowMs);
#if TFLUNACTRL_HAS_RV3032_LIB
  if (_rtcRv3032Initialized) {
    uint8_t value = 0;
    RV3032::Status rtcStatus = _rtcRv3032.readRegister(kRtcRegStatus, value);
    snapshot.statusRegStatus = mapRtcStatus(rtcStatus);
    if (rtcStatus.ok()) {
      snapshot.hasStatusReg = true;
      snapshot.rawStatusReg = value;
      snapshot.powerOnReset = ((value >> kRtcStatusPorfBit) & 0x01U) != 0U;
      snapshot.voltageLow = ((value >> kRtcStatusVlfBit) & 0x01U) != 0U;
    }

    value = 0;
    rtcStatus = _rtcRv3032.readRegister(kRtcRegTempLsb, value);
    snapshot.tempLsbStatus = mapRtcStatus(rtcStatus);
    if (rtcStatus.ok()) {
      snapshot.hasTempLsb = true;
      snapshot.rawTempLsb = value;
      snapshot.backupSwitched = ((value >> kRtcTempBsfBit) & 0x01U) != 0U;
    }

    value = 0;
    rtcStatus = _rtcRv3032.readRegister(kRtcRegPmu, value);
    snapshot.pmuStatus = mapRtcStatus(rtcStatus);
    if (rtcStatus.ok()) {
      snapshot.hasPmuReg = true;
      snapshot.rawPmuReg = value;
      snapshot.effectiveBackupMode =
          decodeRtcBackupMode(value, snapshot.hasEffectiveBackupMode);
    }

    snapshot.timeInvalid = snapshot.powerOnReset || snapshot.voltageLow;
  }
#endif
  storeRtcDebugSnapshot(snapshot);
}

bool I2cTask::isRecent(uint32_t nowMs, uint32_t timestampMs, uint32_t windowMs) const {
  if (timestampMs == 0) {
    return false;
  }
  return (nowMs - timestampMs) <= windowMs;
}

HealthState I2cTask::health() const {
  const uint32_t nowMs = readNowMs();

  const I2cBusMetrics m = getMetrics();
  if (!_running || !_enabled) {
    return HealthState::FAULT;
  }

  if (m.taskAliveMs != 0 &&
      static_cast<int32_t>(nowMs - m.taskAliveMs) >
          static_cast<int32_t>(_settings.i2cTaskHeartbeatTimeoutMs *
                               static_cast<uint32_t>(_settings.i2cHealthStaleTaskMultiplier))) {
    return HealthState::FAULT;
  }

  if (m.consecutiveErrors >= (_settings.i2cMaxConsecutiveFailures * 2U)) {
    return HealthState::FAULT;
  }

  const bool recentQueueIssue =
      isRecent(nowMs, _lastRequestOverflowMs, _settings.i2cHealthRecentWindowMs) ||
      isRecent(nowMs, _lastResultDropMs, _settings.i2cHealthRecentWindowMs) ||
      isRecent(nowMs, _lastStaleResultMs, _settings.i2cHealthRecentWindowMs);

  const bool slowDegraded = m.recentSlowOpCount >= _settings.i2cSlowOpDegradeCount;
  if (m.consecutiveErrors > 0 || recentQueueIssue || slowDegraded) {
    return HealthState::DEGRADED;
  }

  return HealthState::OK;
}

void I2cTask::updateTaskHeartbeat(uint32_t nowMs) {
#ifdef ARDUINO
  if (_metricsMutex && xSemaphoreTake(_metricsMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
    _metrics.taskAliveMs = nowMs;
    xSemaphoreGive(_metricsMutex);
  }
#else
  _metrics.taskAliveMs = nowMs;
#endif
}

void I2cTask::updateMetricsSuccess(uint32_t durationUs, uint32_t nowMs) {
#ifdef ARDUINO
  if (_metricsMutex && xSemaphoreTake(_metricsMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
    _metrics.transactionCount++;
    _metrics.totalDurationUs += durationUs;
    if (durationUs > _metrics.maxDurationUs) {
      _metrics.maxDurationUs = durationUs;
    }
    if (_slowWindowStartMs == UINT32_MAX ||
        static_cast<int32_t>(nowMs - _slowWindowStartMs) >=
            static_cast<int32_t>(_settings.i2cSlowWindowMs)) {
      _slowWindowStartMs = nowMs;
      _slowWindowCount = 0;
      _metrics.rollingMaxDurationUs = durationUs;
      _metrics.slowWindowStartMs = nowMs;
    } else if (durationUs > _metrics.rollingMaxDurationUs) {
      _metrics.rollingMaxDurationUs = durationUs;
    }
    if (durationUs >= _settings.i2cSlowOpThresholdUs) {
      _metrics.slowOpCount++;
      if (_slowWindowCount < 0xFFFFFFFFUL) {
        _slowWindowCount++;
      }
    }
    _metrics.recentSlowOpCount = _slowWindowCount;
    _metrics.consecutiveErrors = 0;
    _metrics.lastError = Ok();
    xSemaphoreGive(_metricsMutex);
  }
#else
  _metrics.transactionCount++;
  _metrics.totalDurationUs += durationUs;
  if (durationUs > _metrics.maxDurationUs) {
    _metrics.maxDurationUs = durationUs;
  }
  if (_slowWindowStartMs == UINT32_MAX ||
      static_cast<int32_t>(nowMs - _slowWindowStartMs) >=
          static_cast<int32_t>(_settings.i2cSlowWindowMs)) {
    _slowWindowStartMs = nowMs;
    _slowWindowCount = 0;
    _metrics.rollingMaxDurationUs = durationUs;
    _metrics.slowWindowStartMs = nowMs;
  } else if (durationUs > _metrics.rollingMaxDurationUs) {
    _metrics.rollingMaxDurationUs = durationUs;
  }
  if (durationUs >= _settings.i2cSlowOpThresholdUs) {
    _metrics.slowOpCount++;
    if (_slowWindowCount < 0xFFFFFFFFUL) {
      _slowWindowCount++;
    }
  }
  _metrics.recentSlowOpCount = _slowWindowCount;
  _metrics.consecutiveErrors = 0;
  _metrics.lastError = Ok();
#endif
}

void I2cTask::updateMetricsError(const Status& status, uint32_t nowMs) {
#ifdef ARDUINO
  if (_metricsMutex && xSemaphoreTake(_metricsMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
    _metrics.errorCount++;
    _metrics.consecutiveErrors++;
    _metrics.lastError = status;
    _metrics.lastErrorMs = nowMs;
    xSemaphoreGive(_metricsMutex);
  }
#else
  _metrics.errorCount++;
  _metrics.consecutiveErrors++;
  _metrics.lastError = status;
  _metrics.lastErrorMs = nowMs;
#endif
}

void I2cTask::markRequestOverflow(uint32_t nowMs) {
#ifdef ARDUINO
  if (_metricsMutex && xSemaphoreTake(_metricsMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
    _metrics.requestOverflowCount++;
    xSemaphoreGive(_metricsMutex);
  }
#else
  _metrics.requestOverflowCount++;
#endif
  _lastRequestOverflowMs = nowMs;
}

void I2cTask::markResultDrop(uint32_t nowMs) {
#ifdef ARDUINO
  if (_metricsMutex && xSemaphoreTake(_metricsMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
    _metrics.resultDroppedCount++;
    xSemaphoreGive(_metricsMutex);
  }
#else
  _metrics.resultDroppedCount++;
#endif
  _lastResultDropMs = nowMs;
}

bool I2cTask::queueResult(const I2cResult& result, uint32_t nowMs) {
#ifdef ARDUINO
  if (_resultQueue == nullptr) {
    return false;
  }

  if (xQueueSend(_resultQueue, &result, 0) != pdTRUE) {
    markResultDrop(nowMs);
    return false;
  }
  return true;
#else
  if (!_resultQueueNative.push(result, nowMs)) {
    markResultDrop(nowMs);
    return false;
  }
  return true;
#endif
}

bool I2cTask::isBusStuck(uint32_t nowMs) const {
  if (_forceBusStuck) {
    return true;
  }
  return _gpioProbe.isBusPhysicallyStuck(nowMs);
}

Status I2cTask::recoverBus(uint32_t nowMs) {
  if (!_enabled || _backend == nullptr) {
    return Status(Err::NOT_INITIALIZED, 0, "I2C disabled");
  }

  auto recordPowerCycleStatus = [this, nowMs](const Status& status) {
#ifdef ARDUINO
    if (_metricsMutex && xSemaphoreTake(_metricsMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
      _metrics.lastPowerCycleStatus = status;
      _metrics.lastPowerCycleMs = nowMs;
      xSemaphoreGive(_metricsMutex);
    }
#else
    _metrics.lastPowerCycleStatus = status;
    _metrics.lastPowerCycleMs = nowMs;
#endif
  };

  Status recoveryStatus = _backend->reset(_backendConfig);
  I2cRecoveryStage stage = I2cRecoveryStage::RESET;

  if (isBusStuck(nowMs) && _config.i2cScl >= 0 && _config.i2cSda >= 0) {
#ifdef ARDUINO
    stage = I2cRecoveryStage::SCL_PULSE;
    pinMode(static_cast<uint8_t>(_config.i2cScl), OUTPUT);
    pinMode(static_cast<uint8_t>(_config.i2cSda), INPUT_PULLUP);
    for (uint8_t i = 0; i < _config.i2cRecoveryPulses; ++i) {
      digitalWrite(static_cast<uint8_t>(_config.i2cScl), HIGH);
      delayMicroseconds(_config.i2cRecoveryPulseHighUs);
      digitalWrite(static_cast<uint8_t>(_config.i2cScl), LOW);
      delayMicroseconds(_config.i2cRecoveryPulseLowUs);
    }
    pinMode(static_cast<uint8_t>(_config.i2cScl), INPUT_PULLUP);
#endif
    const Status resetAfterPulse = _backend->reset(_backendConfig);
    if (!resetAfterPulse.ok()) {
      recoveryStatus = resetAfterPulse;
    }
  }

  if (isBusStuck(nowMs)) {
    if (_powerHook != nullptr) {
      stage = I2cRecoveryStage::POWER_CYCLE;
      const Status hookStatus = _powerHook(nowMs, _powerHookContext);
      recordPowerCycleStatus(hookStatus);
      if (!hookStatus.ok()) {
        recoveryStatus = hookStatus;
      }
#ifdef ARDUINO
      if (_metricsMutex && xSemaphoreTake(_metricsMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        _metrics.powerCycleAttempts++;
        xSemaphoreGive(_metricsMutex);
      }
#else
      _metrics.powerCycleAttempts++;
#endif
      const Status resetAfterPower = _backend->reset(_backendConfig);
      if (!resetAfterPower.ok()) {
        recoveryStatus = resetAfterPower;
      }
    } else {
      recordPowerCycleStatus(Status(Err::NOT_INITIALIZED, 0, "not configured"));
    }

    if (isBusStuck(nowMs)) {
      recoveryStatus = Status(Err::BUS_STUCK, 0, "I2C recover stuck SDA/SCL");
    }
  }

#ifdef ARDUINO
  if (_metricsMutex && xSemaphoreTake(_metricsMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
    _metrics.recoveryCount++;
    _metrics.lastRecoveryMs = nowMs;
    _metrics.lastRecoveryStage = stage;
    xSemaphoreGive(_metricsMutex);
  }
#else
  _metrics.recoveryCount++;
  _metrics.lastRecoveryMs = nowMs;
  _metrics.lastRecoveryStage = stage;
#endif

  _recoveryPolicy.onRecovery(nowMs);
  return recoveryStatus;
}

#if TFLUNACTRL_HAS_BME280_LIB
BME280::Status I2cTask::bmeI2cWriteThunk(uint8_t addr,
                                         const uint8_t* data,
                                         size_t len,
                                         uint32_t timeoutMs,
                                         void* user) {
  I2cTask* self = static_cast<I2cTask*>(user);
  if (self == nullptr) {
    return BME280::Status::Error(BME280::Err::INVALID_CONFIG, "BME user context missing");
  }
  return self->bmeI2cWrite(addr, data, len, timeoutMs);
}

BME280::Status I2cTask::bmeI2cWriteReadThunk(uint8_t addr,
                                             const uint8_t* txData,
                                             size_t txLen,
                                             uint8_t* rxData,
                                             size_t rxLen,
                                             uint32_t timeoutMs,
                                             void* user) {
  I2cTask* self = static_cast<I2cTask*>(user);
  if (self == nullptr) {
    return BME280::Status::Error(BME280::Err::INVALID_CONFIG, "BME user context missing");
  }
  return self->bmeI2cWriteRead(addr, txData, txLen, rxData, rxLen, timeoutMs);
}

BME280::Status I2cTask::bmeI2cWrite(uint8_t addr,
                                    const uint8_t* data,
                                    size_t len,
                                    uint32_t timeoutMs) {
  if (_backend == nullptr) {
    return BME280::Status::Error(BME280::Err::NOT_INITIALIZED, "I2C backend unavailable");
  }
  if (data == nullptr || len == 0) {
    return BME280::Status::Error(BME280::Err::INVALID_PARAM, "BME write buffer invalid");
  }

  I2cTransfer transfer{};
  transfer.address = addr;
  transfer.txData = data;
  transfer.txLen = len;
  transfer.rxData = nullptr;
  transfer.rxLen = 0;
  transfer.timeoutMs = timeoutMs;
  transfer.sendStop = true;

  uint32_t durationUs = 0;
  const Status st = _backend->transfer(transfer, durationUs);
  if (st.ok()) {
    return BME280::Status::Ok();
  }
  if (st.code == Err::TIMEOUT) {
    return BME280::Status::Error(BME280::Err::TIMEOUT, "BME write timeout", st.detail);
  }
  return BME280::Status::Error(BME280::Err::I2C_ERROR, "BME write failed", st.detail);
}

BME280::Status I2cTask::bmeI2cWriteRead(uint8_t addr,
                                        const uint8_t* txData,
                                        size_t txLen,
                                        uint8_t* rxData,
                                        size_t rxLen,
                                        uint32_t timeoutMs) {
  if (_backend == nullptr) {
    return BME280::Status::Error(BME280::Err::NOT_INITIALIZED, "I2C backend unavailable");
  }
  if (txData == nullptr || txLen == 0 || (rxLen > 0 && rxData == nullptr)) {
    return BME280::Status::Error(BME280::Err::INVALID_PARAM, "BME write-read buffer invalid");
  }

  I2cTransfer transfer{};
  transfer.address = addr;
  transfer.txData = txData;
  transfer.txLen = txLen;
  transfer.rxData = rxData;
  transfer.rxLen = rxLen;
  transfer.timeoutMs = timeoutMs;
  transfer.sendStop = true;

  uint32_t durationUs = 0;
  const Status st = _backend->transfer(transfer, durationUs);
  if (st.ok()) {
    return BME280::Status::Ok();
  }
  if (st.code == Err::TIMEOUT) {
    return BME280::Status::Error(BME280::Err::TIMEOUT, "BME read timeout", st.detail);
  }
  return BME280::Status::Error(BME280::Err::I2C_ERROR, "BME read failed", st.detail);
}

Status I2cTask::mapBmeStatus(const BME280::Status& status) const {
  switch (status.code) {
    case BME280::Err::OK:
      return Ok();
    case BME280::Err::NOT_INITIALIZED:
      return Status(Err::NOT_INITIALIZED, static_cast<int32_t>(status.code), "BME280 not initialized");
    case BME280::Err::INVALID_CONFIG:
    case BME280::Err::INVALID_PARAM:
      return Status(Err::INVALID_CONFIG, static_cast<int32_t>(status.code), "BME280 invalid config");
    case BME280::Err::TIMEOUT:
      return Status(Err::TIMEOUT, static_cast<int32_t>(status.code), "BME280 timeout");
    case BME280::Err::BUSY:
    case BME280::Err::IN_PROGRESS:
    case BME280::Err::MEASUREMENT_NOT_READY:
      return Status(Err::RESOURCE_BUSY, static_cast<int32_t>(status.code), "BME280 busy");
    case BME280::Err::CHIP_ID_MISMATCH:
    case BME280::Err::CALIBRATION_INVALID:
    case BME280::Err::COMPENSATION_ERROR:
      return Status(Err::DATA_CORRUPT, static_cast<int32_t>(status.code), "BME280 data invalid");
    case BME280::Err::DEVICE_NOT_FOUND:
    case BME280::Err::I2C_ERROR:
      return Status(Err::COMM_FAILURE, static_cast<int32_t>(status.code), "BME280 communication failed");
    default:
      return Status(Err::EXTERNAL_LIB_ERROR, static_cast<int32_t>(status.code), "BME280 external error");
  }
}

Status I2cTask::ensureBmeReady(uint8_t address, uint32_t nowMs) {
  if (!isBme280Address(address)) {
    return Status(Err::INVALID_CONFIG, 0, "BME280 address invalid");
  }

  if (_envBmeInitialized &&
      _envBmeAddress == address &&
      _envBmeTimeoutMs == _settings.i2cOpTimeoutMs) {
    return Ok();
  }

  _envBme.end();
  _envBmeInitialized = false;
  _envBmeAddress = address;
  _envBmeTimeoutMs = _settings.i2cOpTimeoutMs;

  BME280::Config cfg{};
  cfg.i2cWrite = &I2cTask::bmeI2cWriteThunk;
  cfg.i2cWriteRead = &I2cTask::bmeI2cWriteReadThunk;
  cfg.i2cUser = this;
  cfg.i2cAddress = address;
  cfg.i2cTimeoutMs = _settings.i2cOpTimeoutMs;
  cfg.mode = toBmeMode(_settings.i2cEnvBmeMode);
  cfg.osrsT = toBmeOversampling(_settings.i2cEnvBmeOsrsT);
  cfg.osrsP = toBmeOversampling(_settings.i2cEnvBmeOsrsP);
  cfg.osrsH = toBmeOversampling(_settings.i2cEnvBmeOsrsH);
  cfg.filter = toBmeFilter(_settings.i2cEnvBmeFilter);
  cfg.standby = toBmeStandby(_settings.i2cEnvBmeStandby);
  cfg.offlineThreshold = _settings.i2cMaxConsecutiveFailures;

  const BME280::Status beginStatus = _envBme.begin(cfg);
  const Status mapped = mapBmeStatus(beginStatus);
  if (!mapped.ok()) {
    return mapped;
  }

  _envBmeInitialized = true;
  _envBme.tick(nowMs);
  return Ok();
}

Status I2cTask::handleBmeTrigger(const I2cRequest& request, I2cResult& result, uint32_t nowMs) {
  (void)result;

  Status st = ensureBmeReady(request.address, nowMs);
  if (!st.ok()) {
#if TFLUNACTRL_HAS_SSD1315_LIB
    _displayEnvStatus = st;
#endif
    return st;
  }

  _envBme.tick(nowMs);
  BME280::Status bmeStatus = _envBme.requestMeasurement();
  if (bmeStatus.code == BME280::Err::BUSY &&
      _envBme.state() == BME280::DriverState::OFFLINE) {
    bmeStatus = _envBme.recover();
    if (!bmeStatus.ok()) {
      return mapBmeStatus(bmeStatus);
    }
    bmeStatus = _envBme.requestMeasurement();
  }

  if (bmeStatus.ok() ||
      bmeStatus.code == BME280::Err::IN_PROGRESS ||
      bmeStatus.code == BME280::Err::BUSY) {
#if TFLUNACTRL_HAS_SSD1315_LIB
    _displayEnvStatus = Ok();
#endif
    return Ok();
  }

  const Status mapped = mapBmeStatus(bmeStatus);
#if TFLUNACTRL_HAS_SSD1315_LIB
  _displayEnvStatus = mapped;
#endif
  return mapped;
}

Status I2cTask::handleBmeRead(const I2cRequest& request, I2cResult& result, uint32_t nowMs) {
  Status st = ensureBmeReady(request.address, nowMs);
  if (!st.ok()) {
#if TFLUNACTRL_HAS_SSD1315_LIB
    _displayEnvStatus = st;
#endif
    return st;
  }

  _envBme.tick(nowMs);
  BME280::Measurement measurement{};
  const BME280::Status readStatus = _envBme.getMeasurement(measurement);
  if (!readStatus.ok()) {
    if (readStatus.code == BME280::Err::MEASUREMENT_NOT_READY) {
      const Status notReady =
          Status(Err::RESOURCE_BUSY, static_cast<int32_t>(readStatus.code), "BME280 measurement not ready");
#if TFLUNACTRL_HAS_SSD1315_LIB
      _displayEnvStatus = notReady;
#endif
      return notReady;
    }
    const Status mapped = mapBmeStatus(readStatus);
#if TFLUNACTRL_HAS_SSD1315_LIB
    _displayEnvStatus = mapped;
#endif
    return mapped;
  }

  const float pressureHpa = measurement.pressurePa / 100.0f;
  if (!isfinite(measurement.temperatureC) ||
      !isfinite(measurement.humidityPct) ||
      !isfinite(pressureHpa)) {
    const Status nonFinite = Status(Err::DATA_CORRUPT, 0, "BME280 non-finite sample");
#if TFLUNACTRL_HAS_SSD1315_LIB
    _displayEnvStatus = nonFinite;
#endif
    return nonFinite;
  }

  const int16_t tempX100 =
      clampInt16(static_cast<int32_t>(lroundf(measurement.temperatureC * 100.0f)));
  const uint16_t rhX100 =
      clampUint16(static_cast<int32_t>(lroundf(measurement.humidityPct * 100.0f)));
  const uint16_t pressureX10 =
      clampUint16(static_cast<int32_t>(lroundf(pressureHpa * 10.0f)));

  result.data[0] = static_cast<uint8_t>((tempX100 >> 8) & 0xFF);
  result.data[1] = static_cast<uint8_t>(tempX100 & 0xFF);
  result.data[2] = static_cast<uint8_t>((rhX100 >> 8) & 0xFF);
  result.data[3] = static_cast<uint8_t>(rhX100 & 0xFF);
  result.data[4] = static_cast<uint8_t>((pressureX10 >> 8) & 0xFF);
  result.data[5] = static_cast<uint8_t>(pressureX10 & 0xFF);
  result.dataLen = 6;

#if TFLUNACTRL_HAS_SSD1315_LIB
  _displayEnvStatus = Ok();
  _displayEnvValid = true;
  _displayEnvTempC = measurement.temperatureC;
  _displayEnvRhPct = measurement.humidityPct;
  _displayEnvPressureHpa = pressureHpa;
  _displayEnvHasPressure = true;
  _displayEnvSampleMs = nowMs;
#endif

  return Ok();
}
#endif

#if TFLUNACTRL_HAS_SHT3X_LIB
SHT3x::Status I2cTask::shtI2cWriteThunk(uint8_t addr,
                                        const uint8_t* data,
                                        size_t len,
                                        uint32_t timeoutMs,
                                        void* user) {
  I2cTask* self = static_cast<I2cTask*>(user);
  if (self == nullptr) {
    return SHT3x::Status::Error(SHT3x::Err::INVALID_CONFIG, "SHT user context missing");
  }
  return self->shtI2cWrite(addr, data, len, timeoutMs);
}

SHT3x::Status I2cTask::shtI2cWriteReadThunk(uint8_t addr,
                                            const uint8_t* txData,
                                            size_t txLen,
                                            uint8_t* rxData,
                                            size_t rxLen,
                                            uint32_t timeoutMs,
                                            void* user) {
  I2cTask* self = static_cast<I2cTask*>(user);
  if (self == nullptr) {
    return SHT3x::Status::Error(SHT3x::Err::INVALID_CONFIG, "SHT user context missing");
  }
  return self->shtI2cWriteRead(addr, txData, txLen, rxData, rxLen, timeoutMs);
}

SHT3x::Status I2cTask::shtI2cWrite(uint8_t addr,
                                   const uint8_t* data,
                                   size_t len,
                                   uint32_t timeoutMs) {
  if (_backend == nullptr) {
    return SHT3x::Status::Error(SHT3x::Err::NOT_INITIALIZED, "I2C backend unavailable");
  }
  if (data == nullptr || len == 0) {
    return SHT3x::Status::Error(SHT3x::Err::INVALID_PARAM, "SHT write buffer invalid");
  }

  I2cTransfer transfer{};
  transfer.address = addr;
  transfer.txData = data;
  transfer.txLen = len;
  transfer.rxData = nullptr;
  transfer.rxLen = 0;
  transfer.timeoutMs = timeoutMs;
  transfer.sendStop = true;

  uint32_t durationUs = 0;
  const Status st = _backend->transfer(transfer, durationUs);
  if (st.ok()) {
    return SHT3x::Status::Ok();
  }
  if (st.code == Err::TIMEOUT) {
    return SHT3x::Status::Error(SHT3x::Err::I2C_TIMEOUT, "SHT write timeout", st.detail);
  }
  if (st.code == Err::BUS_STUCK) {
    return SHT3x::Status::Error(SHT3x::Err::I2C_BUS, "SHT bus stuck", st.detail);
  }
  if (st.code == Err::NOT_INITIALIZED) {
    return SHT3x::Status::Error(SHT3x::Err::NOT_INITIALIZED, "SHT backend not initialized", st.detail);
  }
  if (st.code == Err::INVALID_CONFIG) {
    return SHT3x::Status::Error(SHT3x::Err::INVALID_CONFIG, "SHT transport invalid", st.detail);
  }
  return SHT3x::Status::Error(SHT3x::Err::I2C_ERROR, "SHT write failed", st.detail);
}

SHT3x::Status I2cTask::shtI2cWriteRead(uint8_t addr,
                                       const uint8_t* txData,
                                       size_t txLen,
                                       uint8_t* rxData,
                                       size_t rxLen,
                                       uint32_t timeoutMs) {
  if (_backend == nullptr) {
    return SHT3x::Status::Error(SHT3x::Err::NOT_INITIALIZED, "I2C backend unavailable");
  }
  if (txLen != 0U) {
    return SHT3x::Status::Error(SHT3x::Err::INVALID_PARAM, "SHT read must use txLen=0");
  }
  if (rxData == nullptr || rxLen == 0) {
    return SHT3x::Status::Error(SHT3x::Err::INVALID_PARAM, "SHT read buffer invalid");
  }

  I2cTransfer transfer{};
  transfer.address = addr;
  transfer.txData = txData;
  transfer.txLen = 0;
  transfer.rxData = rxData;
  transfer.rxLen = rxLen;
  transfer.timeoutMs = timeoutMs;
  transfer.sendStop = true;

  uint32_t durationUs = 0;
  const Status st = _backend->transfer(transfer, durationUs);
  if (st.ok()) {
    return SHT3x::Status::Ok();
  }
  if (st.code == Err::TIMEOUT) {
    return SHT3x::Status::Error(SHT3x::Err::I2C_TIMEOUT, "SHT read timeout", st.detail);
  }
  if (st.code == Err::BUS_STUCK) {
    return SHT3x::Status::Error(SHT3x::Err::I2C_BUS, "SHT bus stuck", st.detail);
  }
  if (st.code == Err::NOT_INITIALIZED) {
    return SHT3x::Status::Error(SHT3x::Err::NOT_INITIALIZED, "SHT backend not initialized", st.detail);
  }
  if (st.code == Err::INVALID_CONFIG) {
    return SHT3x::Status::Error(SHT3x::Err::INVALID_CONFIG, "SHT transport invalid", st.detail);
  }
  return SHT3x::Status::Error(SHT3x::Err::I2C_ERROR, "SHT read failed", st.detail);
}

Status I2cTask::mapShtStatus(const SHT3x::Status& status) const {
  switch (status.code) {
    case SHT3x::Err::OK:
      return Ok();
    case SHT3x::Err::NOT_INITIALIZED:
      return Status(Err::NOT_INITIALIZED, static_cast<int32_t>(status.code), "SHT3x not initialized");
    case SHT3x::Err::INVALID_CONFIG:
    case SHT3x::Err::INVALID_PARAM:
    case SHT3x::Err::UNSUPPORTED:
      return Status(Err::INVALID_CONFIG, static_cast<int32_t>(status.code), "SHT3x invalid config");
    case SHT3x::Err::TIMEOUT:
    case SHT3x::Err::I2C_TIMEOUT:
      return Status(Err::TIMEOUT, static_cast<int32_t>(status.code), "SHT3x timeout");
    case SHT3x::Err::BUSY:
    case SHT3x::Err::IN_PROGRESS:
    case SHT3x::Err::MEASUREMENT_NOT_READY:
      return Status(Err::RESOURCE_BUSY, static_cast<int32_t>(status.code), "SHT3x busy");
    case SHT3x::Err::CRC_MISMATCH:
    case SHT3x::Err::COMMAND_FAILED:
    case SHT3x::Err::WRITE_CRC_ERROR:
      return Status(Err::DATA_CORRUPT, static_cast<int32_t>(status.code), "SHT3x data invalid");
    case SHT3x::Err::DEVICE_NOT_FOUND:
    case SHT3x::Err::I2C_ERROR:
    case SHT3x::Err::I2C_NACK_ADDR:
    case SHT3x::Err::I2C_NACK_DATA:
    case SHT3x::Err::I2C_NACK_READ:
    case SHT3x::Err::I2C_BUS:
      return Status(Err::COMM_FAILURE, static_cast<int32_t>(status.code), "SHT3x communication failed");
    default:
      return Status(Err::EXTERNAL_LIB_ERROR, static_cast<int32_t>(status.code), "SHT3x external error");
  }
}

Status I2cTask::ensureShtReady(uint8_t address, uint32_t nowMs) {
  if (!isSht3xAddress(address)) {
    return Status(Err::INVALID_CONFIG, 0, "SHT3x address invalid");
  }

  if (_envShtInitialized &&
      _envShtAddress == address &&
      _envShtTimeoutMs == _settings.i2cOpTimeoutMs) {
    return Ok();
  }

  _envSht.end();
  _envShtInitialized = false;
  _envShtAddress = address;
  _envShtTimeoutMs = _settings.i2cOpTimeoutMs;

  SHT3x::Config cfg{};
  cfg.i2cWrite = &I2cTask::shtI2cWriteThunk;
  cfg.i2cWriteRead = &I2cTask::shtI2cWriteReadThunk;
  cfg.i2cUser = this;
  cfg.i2cAddress = address;
  cfg.i2cTimeoutMs = _settings.i2cOpTimeoutMs;
  cfg.transportCapabilities = SHT3x::TransportCapability::NONE;
  cfg.mode = toShtMode(_settings.i2cEnvShtMode);
  cfg.repeatability = toShtRepeatability(_settings.i2cEnvShtRepeatability);
  cfg.periodicRate = toShtPeriodicRate(_settings.i2cEnvShtPeriodicRate);
  cfg.clockStretching = toShtClockStretching(_settings.i2cEnvShtClockStretching);
  cfg.lowVdd = _settings.i2cEnvShtLowVdd;
  cfg.commandDelayMs = _settings.i2cEnvShtCommandDelayMs;
  cfg.notReadyTimeoutMs = _settings.i2cEnvShtNotReadyTimeoutMs;
  cfg.periodicFetchMarginMs = _settings.i2cEnvShtPeriodicFetchMarginMs;
  cfg.offlineThreshold = _settings.i2cMaxConsecutiveFailures;
  cfg.recoverBackoffMs = _settings.i2cRecoveryBackoffMs;
  cfg.allowGeneralCallReset = _settings.i2cEnvShtAllowGeneralCallReset;
  cfg.recoverUseBusReset = _settings.i2cEnvShtRecoverUseBusReset;
  cfg.recoverUseSoftReset = _settings.i2cEnvShtRecoverUseSoftReset;
  cfg.recoverUseHardReset = _settings.i2cEnvShtRecoverUseHardReset;

  const SHT3x::Status beginStatus = _envSht.begin(cfg);
  const Status mapped = mapShtStatus(beginStatus);
  if (!mapped.ok()) {
    return mapped;
  }

  _envShtInitialized = true;
  _envSht.tick(nowMs);
  return Ok();
}

Status I2cTask::handleShtTrigger(const I2cRequest& request, I2cResult& result, uint32_t nowMs) {
  (void)result;

  Status st = ensureShtReady(request.address, nowMs);
  if (!st.ok()) {
#if TFLUNACTRL_HAS_SSD1315_LIB
    _displayEnvStatus = st;
#endif
    return st;
  }

  _envSht.tick(nowMs);
  SHT3x::Status shtStatus = _envSht.requestMeasurement();
  if (!shtStatus.ok() && _envSht.state() == SHT3x::DriverState::OFFLINE) {
    shtStatus = _envSht.recover();
    if (!shtStatus.ok()) {
      return mapShtStatus(shtStatus);
    }
    shtStatus = _envSht.requestMeasurement();
  }

  if (shtStatus.ok() ||
      shtStatus.code == SHT3x::Err::IN_PROGRESS ||
      shtStatus.code == SHT3x::Err::BUSY) {
#if TFLUNACTRL_HAS_SSD1315_LIB
    _displayEnvStatus = Ok();
#endif
    return Ok();
  }

  const Status mapped = mapShtStatus(shtStatus);
#if TFLUNACTRL_HAS_SSD1315_LIB
  _displayEnvStatus = mapped;
#endif
  return mapped;
}

Status I2cTask::handleShtRead(const I2cRequest& request, I2cResult& result, uint32_t nowMs) {
  Status st = ensureShtReady(request.address, nowMs);
  if (!st.ok()) {
#if TFLUNACTRL_HAS_SSD1315_LIB
    _displayEnvStatus = st;
#endif
    return st;
  }

  _envSht.tick(nowMs);
  SHT3x::RawSample raw{};
  const SHT3x::Status readStatus = _envSht.getRawSample(raw);
  if (!readStatus.ok()) {
    if (readStatus.code == SHT3x::Err::MEASUREMENT_NOT_READY) {
      const Status notReady =
          Status(Err::RESOURCE_BUSY, static_cast<int32_t>(readStatus.code), "SHT3x measurement not ready");
#if TFLUNACTRL_HAS_SSD1315_LIB
      _displayEnvStatus = notReady;
#endif
      return notReady;
    }
    const Status mapped = mapShtStatus(readStatus);
#if TFLUNACTRL_HAS_SSD1315_LIB
    _displayEnvStatus = mapped;
#endif
    return mapped;
  }

  result.data[0] = static_cast<uint8_t>((raw.rawTemperature >> 8) & 0xFF);
  result.data[1] = static_cast<uint8_t>(raw.rawTemperature & 0xFF);
  result.data[2] = 0x00;
  result.data[3] = static_cast<uint8_t>((raw.rawHumidity >> 8) & 0xFF);
  result.data[4] = static_cast<uint8_t>(raw.rawHumidity & 0xFF);
  result.data[5] = 0x00;
  result.dataLen = 6;

#if TFLUNACTRL_HAS_SSD1315_LIB
  const float tempC = (-45.0f + 175.0f * (static_cast<float>(raw.rawTemperature) / 65535.0f));
  const float rhPct = (100.0f * (static_cast<float>(raw.rawHumidity) / 65535.0f));
  _displayEnvStatus = Ok();
  _displayEnvValid = true;
  _displayEnvTempC = tempC;
  _displayEnvRhPct = rhPct;
  _displayEnvPressureHpa = 0.0f;
  _displayEnvHasPressure = false;
  _displayEnvSampleMs = nowMs;
#endif

  return Ok();
}
#endif

#if TFLUNACTRL_HAS_RV3032_LIB
RV3032::Status I2cTask::rtcI2cWriteThunk(uint8_t addr,
                                         const uint8_t* data,
                                         size_t len,
                                         uint32_t timeoutMs,
                                         void* user) {
  I2cTask* self = static_cast<I2cTask*>(user);
  if (self == nullptr) {
    return RV3032::Status::Error(RV3032::Err::INVALID_CONFIG, "RTC user context missing");
  }
  return self->rtcI2cWrite(addr, data, len, timeoutMs);
}

RV3032::Status I2cTask::rtcI2cWriteReadThunk(uint8_t addr,
                                             const uint8_t* txData,
                                             size_t txLen,
                                             uint8_t* rxData,
                                             size_t rxLen,
                                             uint32_t timeoutMs,
                                             void* user) {
  I2cTask* self = static_cast<I2cTask*>(user);
  if (self == nullptr) {
    return RV3032::Status::Error(RV3032::Err::INVALID_CONFIG, "RTC user context missing");
  }
  return self->rtcI2cWriteRead(addr, txData, txLen, rxData, rxLen, timeoutMs);
}

RV3032::Status I2cTask::rtcI2cWrite(uint8_t addr,
                                    const uint8_t* data,
                                    size_t len,
                                    uint32_t timeoutMs) {
  if (_backend == nullptr) {
    return RV3032::Status::Error(RV3032::Err::NOT_INITIALIZED, "I2C backend unavailable");
  }
  if (data == nullptr || len == 0) {
    return RV3032::Status::Error(RV3032::Err::INVALID_PARAM, "RTC write buffer invalid");
  }

  I2cTransfer transfer{};
  transfer.address = addr;
  transfer.txData = data;
  transfer.txLen = len;
  transfer.rxData = nullptr;
  transfer.rxLen = 0;
  transfer.timeoutMs = timeoutMs;
  transfer.sendStop = true;

  uint32_t durationUs = 0;
  const Status st = _backend->transfer(transfer, durationUs);
  if (st.ok()) {
    return RV3032::Status::Ok();
  }
  if (st.code == Err::TIMEOUT) {
    return RV3032::Status::Error(RV3032::Err::TIMEOUT, "RTC write timeout", st.detail);
  }
  if (st.code == Err::NOT_INITIALIZED) {
    return RV3032::Status::Error(RV3032::Err::NOT_INITIALIZED, "RTC backend not initialized", st.detail);
  }
  if (st.code == Err::INVALID_CONFIG) {
    return RV3032::Status::Error(RV3032::Err::INVALID_CONFIG, "RTC transport invalid", st.detail);
  }
  return RV3032::Status::Error(RV3032::Err::I2C_ERROR, "RTC write failed", st.detail);
}

RV3032::Status I2cTask::rtcI2cWriteRead(uint8_t addr,
                                        const uint8_t* txData,
                                        size_t txLen,
                                        uint8_t* rxData,
                                        size_t rxLen,
                                        uint32_t timeoutMs) {
  if (_backend == nullptr) {
    return RV3032::Status::Error(RV3032::Err::NOT_INITIALIZED, "I2C backend unavailable");
  }
  if ((txLen > 0 && txData == nullptr) || (rxLen > 0 && rxData == nullptr)) {
    return RV3032::Status::Error(RV3032::Err::INVALID_PARAM, "RTC write-read buffer invalid");
  }

  I2cTransfer transfer{};
  transfer.address = addr;
  transfer.txData = txData;
  transfer.txLen = txLen;
  transfer.rxData = rxData;
  transfer.rxLen = rxLen;
  transfer.timeoutMs = timeoutMs;
  transfer.sendStop = true;

  uint32_t durationUs = 0;
  const Status st = _backend->transfer(transfer, durationUs);
  if (st.ok()) {
    return RV3032::Status::Ok();
  }
  if (st.code == Err::TIMEOUT) {
    return RV3032::Status::Error(RV3032::Err::TIMEOUT, "RTC read timeout", st.detail);
  }
  if (st.code == Err::NOT_INITIALIZED) {
    return RV3032::Status::Error(RV3032::Err::NOT_INITIALIZED, "RTC backend not initialized", st.detail);
  }
  if (st.code == Err::INVALID_CONFIG) {
    return RV3032::Status::Error(RV3032::Err::INVALID_CONFIG, "RTC transport invalid", st.detail);
  }
  return RV3032::Status::Error(RV3032::Err::I2C_ERROR, "RTC read failed", st.detail);
}

Status I2cTask::mapRtcStatus(const RV3032::Status& status) const {
  switch (status.code) {
    case RV3032::Err::OK:
      return Ok();
    case RV3032::Err::NOT_INITIALIZED:
      return Status(Err::NOT_INITIALIZED, static_cast<int32_t>(status.code), "RV3032 not initialized");
    case RV3032::Err::INVALID_CONFIG:
    case RV3032::Err::INVALID_PARAM:
    case RV3032::Err::INVALID_DATETIME:
      return Status(Err::INVALID_CONFIG, static_cast<int32_t>(status.code), "RV3032 invalid config");
    case RV3032::Err::TIMEOUT:
      return Status(Err::TIMEOUT, static_cast<int32_t>(status.code), "RV3032 timeout");
    case RV3032::Err::BUSY:
    case RV3032::Err::IN_PROGRESS:
    case RV3032::Err::QUEUE_FULL:
      return Status(Err::RESOURCE_BUSY, static_cast<int32_t>(status.code), "RV3032 busy");
    case RV3032::Err::DEVICE_NOT_FOUND:
    case RV3032::Err::I2C_ERROR:
    case RV3032::Err::REGISTER_READ_FAILED:
    case RV3032::Err::REGISTER_WRITE_FAILED:
      return Status(Err::COMM_FAILURE, static_cast<int32_t>(status.code), "RV3032 communication failed");
    case RV3032::Err::EEPROM_WRITE_FAILED:
      return Status(Err::EXTERNAL_LIB_ERROR, static_cast<int32_t>(status.code), "RV3032 EEPROM write failed");
    default:
      return Status(Err::EXTERNAL_LIB_ERROR, static_cast<int32_t>(status.code), "RV3032 external error");
  }
}

Status I2cTask::ensureRtcReady(uint8_t address, uint32_t nowMs) {
  if (address < 1U || address > 0x7FU) {
    return Status(Err::INVALID_CONFIG, 0, "RV3032 address invalid");
  }

  const uint32_t rtcTimeoutMs = rtcI2cTimeoutMs(_settings);

  if (_rtcRv3032Initialized &&
      _rtcRv3032Address == address &&
      _rtcRv3032TimeoutMs == rtcTimeoutMs) {
    return Ok();
  }

  _rtcRv3032.end();
  _rtcRv3032Initialized = false;
  _rtcRv3032Address = address;
  _rtcRv3032TimeoutMs = rtcTimeoutMs;

  RV3032::Config cfg{};
  cfg.i2cWrite = &I2cTask::rtcI2cWriteThunk;
  cfg.i2cWriteRead = &I2cTask::rtcI2cWriteReadThunk;
  cfg.i2cUser = this;
  cfg.i2cAddress = address;
  cfg.i2cTimeoutMs = rtcTimeoutMs;
  cfg.backupMode = toRtcBackupMode(_settings.i2cRtcBackupMode);
  cfg.enableEepromWrites = rtcEepromWritesEnabled(_settings);
  cfg.eepromTimeoutMs = _settings.i2cRtcEepromTimeoutMs;
  cfg.offlineThreshold = _settings.i2cRtcOfflineThreshold;

  const RV3032::Status beginStatus = _rtcRv3032.begin(cfg);
  const Status mapped = mapRtcStatus(beginStatus);
  if (!mapped.ok()) {
    resetRtcDebugSnapshot(nowMs);
    return mapped;
  }

  _rtcRv3032Initialized = true;
  _rtcRv3032.tick(nowMs);
  refreshRtcDebugSnapshot(nowMs);
  return Ok();
}

Status I2cTask::handleRtcRead(const I2cRequest& request, I2cResult& result, uint32_t nowMs) {
  Status st = ensureRtcReady(request.address, nowMs);
  if (!st.ok()) {
#if TFLUNACTRL_HAS_SSD1315_LIB
    _displayRtcStatus = st;
#endif
    return st;
  }

  _rtcRv3032.tick(nowMs);
  RV3032::DateTime dateTime{};
  RV3032::Status rtcStatus = _rtcRv3032.readTime(dateTime);
  if (!rtcStatus.ok() && _rtcRv3032.state() == RV3032::DriverState::OFFLINE) {
    rtcStatus = _rtcRv3032.recover();
    if (rtcStatus.ok()) {
      rtcStatus = _rtcRv3032.readTime(dateTime);
    }
  }
  if (!rtcStatus.ok()) {
    const Status mapped = mapRtcStatus(rtcStatus);
    refreshRtcDebugSnapshot(nowMs);
#if TFLUNACTRL_HAS_SSD1315_LIB
    _displayRtcStatus = mapped;
#endif
    return mapped;
  }

  const uint8_t weekday = static_cast<uint8_t>((dateTime.weekday % 7U) + 1U);
  result.data[0] = static_cast<uint8_t>(((dateTime.second / 10U) << 4U) | (dateTime.second % 10U));
  result.data[1] = static_cast<uint8_t>(((dateTime.minute / 10U) << 4U) | (dateTime.minute % 10U));
  result.data[2] = static_cast<uint8_t>(((dateTime.hour / 10U) << 4U) | (dateTime.hour % 10U));
  result.data[3] = static_cast<uint8_t>(((weekday / 10U) << 4U) | (weekday % 10U));
  result.data[4] = static_cast<uint8_t>(((dateTime.day / 10U) << 4U) | (dateTime.day % 10U));
  result.data[5] = static_cast<uint8_t>(((dateTime.month / 10U) << 4U) | (dateTime.month % 10U));
  result.data[6] = static_cast<uint8_t>((((dateTime.year % 100U) / 10U) << 4U) | ((dateTime.year % 100U) % 10U));
  result.dataLen = 7;

#if TFLUNACTRL_HAS_SSD1315_LIB
  _displayRtcStatus = Ok();
  _displayRtcValid = true;
  _displayRtc.year = dateTime.year;
  _displayRtc.month = dateTime.month;
  _displayRtc.day = dateTime.day;
  _displayRtc.hour = dateTime.hour;
  _displayRtc.minute = dateTime.minute;
  _displayRtc.second = dateTime.second;
  _displayRtc.valid = true;
  _displayRtcSampleMs = nowMs;
#endif
  refreshRtcDebugSnapshot(nowMs);

  return Ok();
}

Status I2cTask::handleRtcSet(const I2cRequest& request, I2cResult& result, uint32_t nowMs) {
  (void)result;

  Status st = ensureRtcReady(request.address, nowMs);
  if (!st.ok()) {
#if TFLUNACTRL_HAS_SSD1315_LIB
    _displayRtcStatus = st;
#endif
    return st;
  }
  if (request.txLen < 8U) {
    const Status shortPayload = Status(Err::INVALID_CONFIG, 0, "RTC set payload too short");
#if TFLUNACTRL_HAS_SSD1315_LIB
    _displayRtcStatus = shortPayload;
#endif
    return shortPayload;
  }

  RV3032::DateTime dateTime{};
  dateTime.second = bcdToDec(static_cast<uint8_t>(request.tx[1] & 0x7F));
  dateTime.minute = bcdToDec(static_cast<uint8_t>(request.tx[2] & 0x7F));
  dateTime.hour = bcdToDec(static_cast<uint8_t>(request.tx[3] & 0x3F));
  dateTime.day = bcdToDec(static_cast<uint8_t>(request.tx[5] & 0x3F));
  dateTime.month = bcdToDec(static_cast<uint8_t>(request.tx[6] & 0x1F));
  dateTime.year = static_cast<uint16_t>(2000U + bcdToDec(request.tx[7]));
  dateTime.weekday = RV3032::RV3032::computeWeekday(dateTime.year, dateTime.month, dateTime.day);

  if (!RV3032::RV3032::isValidDateTime(dateTime)) {
    const Status invalidTime = Status(Err::INVALID_CONFIG, 0, "RTC time invalid");
#if TFLUNACTRL_HAS_SSD1315_LIB
    _displayRtcStatus = invalidTime;
#endif
    return invalidTime;
  }

  _rtcRv3032.tick(nowMs);
  RV3032::Status rtcStatus = _rtcRv3032.setTime(dateTime);
  if (!rtcStatus.ok() && _rtcRv3032.state() == RV3032::DriverState::OFFLINE) {
    rtcStatus = _rtcRv3032.recover();
    if (rtcStatus.ok()) {
      rtcStatus = _rtcRv3032.setTime(dateTime);
    }
  }
  if (!rtcStatus.ok()) {
    const Status mapped = mapRtcStatus(rtcStatus);
    refreshRtcDebugSnapshot(nowMs);
#if TFLUNACTRL_HAS_SSD1315_LIB
    _displayRtcStatus = mapped;
#endif
    return mapped;
  }

  // RV3032 keeps PORF/VLF validity flags latched until firmware clears them.
  // After a confirmed manual set, clear those sticky flags so retained time is
  // reported as valid on subsequent boots instead of looking permanently stale.
  RV3032::ValidityFlags validity{};
  rtcStatus = _rtcRv3032.readValidity(validity);
  if (!rtcStatus.ok() && _rtcRv3032.state() == RV3032::DriverState::OFFLINE) {
    rtcStatus = _rtcRv3032.recover();
    if (rtcStatus.ok()) {
      rtcStatus = _rtcRv3032.readValidity(validity);
    }
  }
  if (!rtcStatus.ok()) {
    const Status mapped = mapRtcStatus(rtcStatus);
    refreshRtcDebugSnapshot(nowMs);
#if TFLUNACTRL_HAS_SSD1315_LIB
    _displayRtcStatus = mapped;
#endif
    return mapped;
  }

  if (validity.powerOnReset) {
    rtcStatus = _rtcRv3032.clearPowerOnResetFlag();
    if (!rtcStatus.ok() && _rtcRv3032.state() == RV3032::DriverState::OFFLINE) {
      rtcStatus = _rtcRv3032.recover();
      if (rtcStatus.ok()) {
        rtcStatus = _rtcRv3032.clearPowerOnResetFlag();
      }
    }
    if (!rtcStatus.ok()) {
      const Status mapped = mapRtcStatus(rtcStatus);
      refreshRtcDebugSnapshot(nowMs);
#if TFLUNACTRL_HAS_SSD1315_LIB
      _displayRtcStatus = mapped;
#endif
      return mapped;
    }
  }

  if (validity.voltageLow) {
    rtcStatus = _rtcRv3032.clearVoltageLowFlag();
    if (!rtcStatus.ok() && _rtcRv3032.state() == RV3032::DriverState::OFFLINE) {
      rtcStatus = _rtcRv3032.recover();
      if (rtcStatus.ok()) {
        rtcStatus = _rtcRv3032.clearVoltageLowFlag();
      }
    }
    if (!rtcStatus.ok()) {
      const Status mapped = mapRtcStatus(rtcStatus);
      refreshRtcDebugSnapshot(nowMs);
#if TFLUNACTRL_HAS_SSD1315_LIB
      _displayRtcStatus = mapped;
#endif
      return mapped;
    }
  }

  _rtcRv3032.tick(nowMs);

#if TFLUNACTRL_HAS_SSD1315_LIB
  _displayRtcStatus = Ok();
  _displayRtcValid = true;
  _displayRtc.year = dateTime.year;
  _displayRtc.month = dateTime.month;
  _displayRtc.day = dateTime.day;
  _displayRtc.hour = dateTime.hour;
  _displayRtc.minute = dateTime.minute;
  _displayRtc.second = dateTime.second;
  _displayRtc.valid = true;
  _displayRtcSampleMs = nowMs;
#endif
  refreshRtcDebugSnapshot(nowMs);

  return Ok();
}
#endif

#if TFLUNACTRL_HAS_SSD1315_LIB
SSD1315Api::Status I2cTask::displayI2cWriteThunk(uint8_t addr,
                                              const uint8_t* data,
                                              size_t len,
                                              uint32_t timeoutMs,
                                              void* user) {
  I2cTask* self = static_cast<I2cTask*>(user);
  if (self == nullptr) {
    return SSD1315Api::Status(SSD1315Api::Err::INVALID_CONFIG, "display user context missing");
  }
  return self->displayI2cWrite(addr, data, len, timeoutMs);
}

SSD1315Api::Status I2cTask::displayI2cWrite(uint8_t addr,
                                         const uint8_t* data,
                                         size_t len,
                                         uint32_t timeoutMs) {
  if (_backend == nullptr) {
    return SSD1315Api::Status(SSD1315Api::Err::NOT_INITIALIZED, "I2C backend unavailable");
  }
  if (data == nullptr || len == 0U) {
    return SSD1315Api::Status(SSD1315Api::Err::INVALID_CONFIG, "display write buffer invalid");
  }

  I2cTransfer transfer{};
  transfer.address = addr;
  transfer.txData = data;
  transfer.txLen = len;
  transfer.rxData = nullptr;
  transfer.rxLen = 0U;
  transfer.timeoutMs = timeoutMs;
  transfer.sendStop = true;

  uint32_t durationUs = 0;
  const Status st = _backend->transfer(transfer, durationUs);
  if (st.ok()) {
    return SSD1315Api::Ok();
  }
  if (st.code == Err::TIMEOUT) {
    return SSD1315Api::Status(SSD1315Api::Err::I2C_TIMEOUT, st.detail, "display write timeout");
  }
  if (st.code == Err::NOT_INITIALIZED) {
    return SSD1315Api::Status(SSD1315Api::Err::NOT_INITIALIZED, st.detail, "display backend not initialized");
  }
  if (st.code == Err::INVALID_CONFIG) {
    return SSD1315Api::Status(SSD1315Api::Err::INVALID_CONFIG, st.detail, "display transport invalid");
  }
  if (st.code == Err::COMM_FAILURE && st.detail == 2) {
    return SSD1315Api::Status(SSD1315Api::Err::I2C_NACK_ADDR, st.detail, "display address NACK");
  }
  if (st.code == Err::COMM_FAILURE && st.detail == 3) {
    return SSD1315Api::Status(SSD1315Api::Err::I2C_NACK_DATA, st.detail, "display data NACK");
  }
  return SSD1315Api::Status(SSD1315Api::Err::I2C_BUS_ERROR, st.detail, "display write failed");
}

Status I2cTask::mapDisplayStatus(const SSD1315Api::Status& status) const {
  switch (status.code) {
    case SSD1315Api::Err::OK:
      return Ok();
    case SSD1315Api::Err::INVALID_CONFIG:
    case SSD1315Api::Err::INVALID_DIMENSIONS:
    case SSD1315Api::Err::INVALID_PAGE_COUNT:
      return Status(Err::INVALID_CONFIG, static_cast<int32_t>(status.code), "SSD1315 invalid config");
    case SSD1315Api::Err::NOT_INITIALIZED:
      return Status(Err::NOT_INITIALIZED, static_cast<int32_t>(status.code), "SSD1315 not initialized");
    case SSD1315Api::Err::STATE_ERROR:
    case SSD1315Api::Err::BUSY:
    case SSD1315Api::Err::PANEL_NOT_READY:
    case SSD1315Api::Err::IN_PROGRESS:
      return Status(Err::RESOURCE_BUSY, static_cast<int32_t>(status.code), "SSD1315 busy");
    case SSD1315Api::Err::I2C_TIMEOUT:
    case SSD1315Api::Err::TIMEOUT:
      return Status(Err::TIMEOUT, static_cast<int32_t>(status.code), "SSD1315 timeout");
    case SSD1315Api::Err::I2C_NACK_ADDR:
    case SSD1315Api::Err::I2C_NACK_DATA:
    case SSD1315Api::Err::I2C_BUS_ERROR:
    case SSD1315Api::Err::DEVICE_NOT_FOUND:
      return Status(Err::COMM_FAILURE, static_cast<int32_t>(status.code), "SSD1315 communication failed");
    case SSD1315Api::Err::BUFFER_OVERFLOW:
      return Status(Err::DATA_CORRUPT, static_cast<int32_t>(status.code), "SSD1315 buffer overflow");
    case SSD1315Api::Err::UNSUPPORTED:
      return Status(Err::INVALID_CONFIG, static_cast<int32_t>(status.code), "SSD1315 unsupported");
    case SSD1315Api::Err::INTERNAL_ERROR:
      return Status(Err::EXTERNAL_LIB_ERROR, static_cast<int32_t>(status.code), "SSD1315 external error");
    default:
      return Status(Err::EXTERNAL_LIB_ERROR, static_cast<int32_t>(status.code), "SSD1315 external error");
  }
}

Status I2cTask::ensureDisplayReady(uint8_t address, uint32_t nowMs) {
  if (address < 1U || address > 0x7FU) {
    return Status(Err::INVALID_CONFIG, 0, "SSD1315 address invalid");
  }

  if (_displayInitialized &&
      _displayAddress == address &&
      _displayTimeoutMs == _settings.i2cOpTimeoutMs &&
      _displayOfflineThreshold == _settings.i2cMaxConsecutiveFailures &&
      _displayFlipX == _config.displayFlipX &&
      _displayFlipY == _config.displayFlipY) {
    return Ok();
  }

  _display.end();
  _displayInitialized = false;
  _displayAddress = address;
  _displayTimeoutMs = _settings.i2cOpTimeoutMs;
  _displayOfflineThreshold = _settings.i2cMaxConsecutiveFailures;
  _displayFlipX = _config.displayFlipX;
  _displayFlipY = _config.displayFlipY;
  _displayNextRecoverMs = 0;

  SSD1315Api::Config cfg{};
  cfg.width = 128;
  cfg.height = 64;
  cfg.i2cAddress = address;
  cfg.i2cWrite = &I2cTask::displayI2cWriteThunk;
  cfg.i2cUser = this;
  cfg.pageBufferPages = 8;
  cfg.byteBudgetPerTick = 128;
  cfg.i2cTimeoutMs = _settings.i2cOpTimeoutMs;
  // Flush is progressed cooperatively from DISPLAY_REFRESH requests, not from a
  // high-frequency loop. Keep this timeout disabled to avoid false timeouts
  // when display poll interval is longer than a single flush step cadence.
  cfg.flushTimeoutMs = 0;
  cfg.displayOnDelayMs = 100;
  cfg.inactivitySleepMs = 0;
  cfg.pageCycleMs = 0;
  cfg.offlineThreshold = _settings.i2cMaxConsecutiveFailures;
  cfg.flipX = _config.displayFlipX;
  cfg.flipY = _config.displayFlipY;

  const SSD1315Api::Status beginStatus = _display.begin(cfg);
  const Status mapped = mapDisplayStatus(beginStatus);
  if (!mapped.ok()) {
    return mapped;
  }

  _displayInitialized = true;
  _display.tick(nowMs);
  return Ok();
}

void I2cTask::renderDisplayFrame(uint32_t nowMs) {
  _display.clear();

  char line[32] = {0};

  if (_displayRtcValid && _displayRtc.valid) {
    snprintf(line,
             sizeof(line),
             "%04u-%02u-%02u %02u:%02u:%02u",
             static_cast<unsigned>(_displayRtc.year),
             static_cast<unsigned>(_displayRtc.month),
             static_cast<unsigned>(_displayRtc.day),
             static_cast<unsigned>(_displayRtc.hour),
             static_cast<unsigned>(_displayRtc.minute),
             static_cast<unsigned>(_displayRtc.second));
    _display.drawText(0, 0, line);
  } else {
    snprintf(line, sizeof(line), "RTC %s", errShortLabel(_displayRtcStatus.code));
    _display.drawText(0, 0, line);
  }

  if (_settings.apSsid[0] == '\0') {
    snprintf(line, sizeof(line), "SSID <unset>");
  } else {
    snprintf(line, sizeof(line), "SSID %.14s", _settings.apSsid);
  }
  _display.drawText(0, 8, line);

  if (_settings.apPass[0] == '\0') {
    snprintf(line, sizeof(line), "PASS <open>");
  } else {
    snprintf(line, sizeof(line), "PASS %.14s", _settings.apPass);
  }
  _display.drawText(0, 16, line);

  if (_displayCo2Valid) {
    int32_t distanceCm = roundToInt32(_displayCo2Ppm);
    if (distanceCm < 0) {
      distanceCm = 0;
    } else if (distanceCm > 120000) {
      distanceCm = 120000;
    }
    snprintf(line, sizeof(line), "DIST %ldcm", static_cast<long>(distanceCm));
  } else {
    snprintf(line, sizeof(line), "DIST n/a");
  }
  _display.drawText(0, 24, line);

  if (_displayEnvValid) {
    int32_t tempX10 = roundToInt32(_displayEnvTempC * 10.0f);
    if (tempX10 < -999) {
      tempX10 = -999;
    } else if (tempX10 > 999) {
      tempX10 = 999;
    }
    int32_t rhX10 = roundToInt32(_displayEnvRhPct * 10.0f);
    if (rhX10 < 0) {
      rhX10 = 0;
    } else if (rhX10 > 1000) {
      rhX10 = 1000;
    }
    const int32_t tempAbsX10 = (tempX10 < 0) ? -tempX10 : tempX10;
    const int32_t rhAbsX10 = rhX10;
    snprintf(line,
             sizeof(line),
             "T%s%ld.%ldC RH%ld.%ld",
             (tempX10 < 0) ? "-" : "",
             static_cast<long>(tempAbsX10 / 10),
             static_cast<long>(tempAbsX10 % 10),
             static_cast<long>(rhAbsX10 / 10),
             static_cast<long>(rhAbsX10 % 10));
    _display.drawText(0, 32, line);

    if (!_displayLogEnabled) {
      snprintf(line, sizeof(line), "LOG OFF");
    } else if (!_displayLogMounted) {
      snprintf(line, sizeof(line), "LOG WAIT SD");
    } else if (!_displayLogHealthy) {
      snprintf(line, sizeof(line), "LOG DEG");
    } else {
      snprintf(line, sizeof(line), "LOG ON");
    }
    _display.drawText(0, 40, line);
  } else {
    snprintf(line, sizeof(line), "ENV %s", errShortLabel(_displayEnvStatus.code));
    _display.drawText(0, 32, line);
    if (!_displayLogEnabled) {
      snprintf(line, sizeof(line), "LOG OFF");
    } else if (!_displayLogMounted) {
      snprintf(line, sizeof(line), "LOG WAIT SD");
    } else if (!_displayLogHealthy) {
      snprintf(line, sizeof(line), "LOG DEG");
    } else {
      snprintf(line, sizeof(line), "LOG ON");
    }
    _display.drawText(0, 40, line);
  }

  snprintf(line, sizeof(line), "SAMPLES %lu", static_cast<unsigned long>(_displayLogSamplesWritten));
  _display.drawText(0, 48, line);

  snprintf(line, sizeof(line), "SYSTEM %s", healthShortLabel(_displaySystemHealth));
  _display.drawText(0, 56, line);

  (void)nowMs;
}

Status I2cTask::handleDisplayRefresh(const I2cRequest& request, I2cResult& result, uint32_t nowMs) {
  (void)result;

  if (request.txLen >= 9U) {
    _displayCo2Valid = (request.tx[0] != 0U);
    uint32_t co2X10 = 0;
    uint32_t samplesWritten = 0;
    memcpy(&co2X10, &request.tx[1], sizeof(co2X10));
    memcpy(&samplesWritten, &request.tx[5], sizeof(samplesWritten));
    _displayCo2Ppm = static_cast<float>(co2X10) * 0.1f;
    _displayLogSamplesWritten = samplesWritten;
  } else {
    // Intentional safeguard: reset display fields on every short-payload
    // refresh so stale data from a previous valid payload is never shown
    // if the orchestrator starts sending truncated requests.
    _displayCo2Valid = false;
    _displayCo2Ppm = 0.0f;
    _displayLogSamplesWritten = 0;
  }
  if (request.txLen >= 12U) {
    _displayOutputMask = static_cast<uint8_t>(request.tx[9] & 0x0FU);
    _displayOutputsEnabled = (request.tx[11] != 0U);
    const uint8_t mode = request.tx[10];
    if (mode == static_cast<uint8_t>(OutputOverrideMode::FORCE_ON)) {
      _displayOutputMode = OutputOverrideMode::FORCE_ON;
    } else if (mode == static_cast<uint8_t>(OutputOverrideMode::FORCE_OFF)) {
      _displayOutputMode = OutputOverrideMode::FORCE_OFF;
    } else {
      _displayOutputMode = OutputOverrideMode::AUTO;
    }
  }
  if (request.txLen >= 14U) {
    const uint8_t flags = request.tx[12];
    _displayLogEnabled = (flags & 0x01U) != 0U;
    _displayLogMounted = (flags & 0x02U) != 0U;
    _displayLogHealthy = (flags & 0x04U) != 0U;

    switch (static_cast<HealthState>(request.tx[13])) {
      case HealthState::OK:
      case HealthState::DEGRADED:
      case HealthState::FAULT:
      case HealthState::UNKNOWN:
        _displaySystemHealth = static_cast<HealthState>(request.tx[13]);
        break;
      default:
        _displaySystemHealth = HealthState::UNKNOWN;
        break;
    }
  } else {
    _displayLogEnabled = false;
    _displayLogMounted = false;
    _displayLogHealthy = false;
    _displaySystemHealth = HealthState::UNKNOWN;
  }

  Status st = ensureDisplayReady(request.address, nowMs);
  if (!st.ok()) {
    return st;
  }

  const uint32_t failuresBefore = _display.totalFailures();
  constexpr uint8_t kDisplayTickBurst = 24U;
  for (uint8_t i = 0; i < kDisplayTickBurst; ++i) {
    _display.tick(readNowMs());
    if (!_display.isFlushing()) {
      break;
    }
  }

  if (_display.state() == SSD1315Api::DriverState::OFFLINE) {
    if (_displayNextRecoverMs == 0 ||
        static_cast<int32_t>(nowMs - _displayNextRecoverMs) >= 0) {
      const SSD1315Api::Status recoverStatus = _display.recover();
      if (!recoverStatus.ok()) {
        _displayNextRecoverMs = nowMs + _settings.i2cRecoveryBackoffMs;
        return mapDisplayStatus(recoverStatus);
      }
      _displayNextRecoverMs = 0;
    } else {
      return Status(Err::RESOURCE_BUSY, 0, "SSD1315 recovery backoff");
    }
  }

  if (!_display.isFlushing()) {
    renderDisplayFrame(nowMs);
    const SSD1315Api::Status flushStatus = _display.requestFlush();
    if (!flushStatus.ok() &&
        flushStatus.code != SSD1315Api::Err::BUSY &&
        flushStatus.code != SSD1315Api::Err::IN_PROGRESS) {
      return mapDisplayStatus(flushStatus);
    }
  }

  for (uint8_t i = 0; i < kDisplayTickBurst; ++i) {
    _display.tick(readNowMs());
    if (!_display.isFlushing()) {
      break;
    }
  }

  if (_display.totalFailures() > failuresBefore) {
    return mapDisplayStatus(_display.lastError());
  }
  return Ok();
}
#endif

I2cResult I2cTask::processRequest(const I2cRequest& request, uint32_t nowMs) {
  I2cResult result{};
  result.op = request.op;
  result.deviceId = request.deviceId;
  result.address = request.address;
  result.token = request.token;
  result.completedMs = nowMs;
  result.requestDeadlineMs = request.deadlineMs;

  if (!_enabled || _backend == nullptr) {
    result.status = Status(Err::NOT_INITIALIZED, 0, "I2C disabled");
    return result;
  }

  if (request.deadlineMs != 0 && static_cast<int32_t>(nowMs - request.deadlineMs) > 0) {
    result.status = Status(Err::TIMEOUT, 0, "I2C request expired before execution");
    result.late = true;
    // Don't call updateMetricsError / recoveryPolicy.onFailure here â€”
    // the orchestrator independently counts the failure via
    // handleExpiredInFlight / processResult.  Counting here as well
    // double-inflates bus consecutiveErrors and recovery counters.
    _lastStaleResultMs = nowMs;
#ifdef ARDUINO
    if (_metricsMutex && xSemaphoreTake(_metricsMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
      _metrics.staleResultCount++;
      xSemaphoreGive(_metricsMutex);
    }
#else
    _metrics.staleResultCount++;
#endif
    return result;
  }

  if (isBusStuck(nowMs)) {
    result.status = Status(Err::BUS_STUCK, 0, "I2C bus physically stuck");
    updateMetricsError(result.status, nowMs);
#ifdef ARDUINO
    if (_metricsMutex && xSemaphoreTake(_metricsMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
      _metrics.stuckSdaCount++;
      _metrics.stuckBusFastFailCount++;
      xSemaphoreGive(_metricsMutex);
    }
#else
    _metrics.stuckSdaCount++;
    _metrics.stuckBusFastFailCount++;
#endif
    _recoveryPolicy.onFailure();
    if (_recoveryPolicy.shouldRecover(nowMs)) {
      recoverBus(nowMs);
    }
    return result;
  }

  Status opStatus = Ok();
  const uint32_t timeoutMs = (request.timeoutMs > 0) ? request.timeoutMs : _settings.i2cOpTimeoutMs;

  if (request.op == I2cOpType::SET_FREQ) {
    // Read _backendConfig under mutex -- it was updated by applySettings()
    // from the main task.
    I2cBackendConfig cfg{};
#ifdef ARDUINO
    if (_metricsMutex && xSemaphoreTake(_metricsMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
      cfg = _backendConfig;
      xSemaphoreGive(_metricsMutex);
    } else {
      cfg = _backendConfig;
    }
#else
    cfg = _backendConfig;
#endif
    opStatus = _backend->applyConfig(cfg);
    result.durationUs = 0;
  } else if (request.op == I2cOpType::RECOVER) {
    opStatus = recoverBus(nowMs);
    result.durationUs = 0;
#if TFLUNACTRL_HAS_RV3032_LIB
  } else if (request.deviceId == DeviceId::RTC &&
             request.op == I2cOpType::WRITE_READ) {
    const uint32_t startedUs = SystemClock::nowUs();
    opStatus = handleRtcRead(request, result, nowMs);
    result.durationUs = SystemClock::nowUs() - startedUs;
  } else if (request.deviceId == DeviceId::RTC &&
             request.op == I2cOpType::RTC_SET_TIME) {
    const uint32_t startedUs = SystemClock::nowUs();
    opStatus = handleRtcSet(request, result, nowMs);
    result.durationUs = SystemClock::nowUs() - startedUs;
#endif
#if TFLUNACTRL_HAS_BME280_LIB
  } else if ((request.op == I2cOpType::ENV_TRIGGER_ONESHOT ||
              request.op == I2cOpType::ENV_READ_ONESHOT) &&
             isBme280Address(request.address)) {
    const uint32_t startedUs = SystemClock::nowUs();
    if (request.op == I2cOpType::ENV_TRIGGER_ONESHOT) {
      opStatus = handleBmeTrigger(request, result, nowMs);
    } else {
      opStatus = handleBmeRead(request, result, nowMs);
    }
    result.durationUs = SystemClock::nowUs() - startedUs;
#endif
#if TFLUNACTRL_HAS_SHT3X_LIB
  } else if ((request.op == I2cOpType::ENV_TRIGGER_ONESHOT ||
              request.op == I2cOpType::ENV_READ_ONESHOT) &&
             isSht3xAddress(request.address)) {
    const uint32_t startedUs = SystemClock::nowUs();
    if (request.op == I2cOpType::ENV_TRIGGER_ONESHOT) {
      opStatus = handleShtTrigger(request, result, nowMs);
    } else {
      opStatus = handleShtRead(request, result, nowMs);
    }
    result.durationUs = SystemClock::nowUs() - startedUs;
#endif
#if TFLUNACTRL_HAS_SSD1315_LIB
  } else if (request.op == I2cOpType::DISPLAY_REFRESH) {
    const uint32_t startedUs = SystemClock::nowUs();
    opStatus = handleDisplayRefresh(request, result, nowMs);
    result.durationUs = SystemClock::nowUs() - startedUs;
#endif
  } else {
    I2cTransfer transfer{};
    transfer.address = request.address;
    transfer.timeoutMs = timeoutMs;
    transfer.sendStop = true;

    switch (request.op) {
      case I2cOpType::PROBE:
        transfer.txData = nullptr;
        transfer.txLen = 0;
        transfer.rxData = nullptr;
        transfer.rxLen = 0;
        break;
      case I2cOpType::WRITE:
      case I2cOpType::RTC_SET_TIME:
      case I2cOpType::ENV_TRIGGER_ONESHOT:
      case I2cOpType::DISPLAY_REFRESH:
        transfer.txData = request.tx;
        transfer.txLen = (request.txLen > sizeof(request.tx)) ? sizeof(request.tx) : request.txLen;
        transfer.rxData = nullptr;
        transfer.rxLen = 0;
        break;
      case I2cOpType::READ:
      case I2cOpType::ENV_READ_ONESHOT:
        transfer.txData = nullptr;
        transfer.txLen = 0;
        transfer.rxData = result.data;
        transfer.rxLen = (request.rxLen > sizeof(result.data)) ? sizeof(result.data) : request.rxLen;
        break;
      case I2cOpType::WRITE_READ:
        transfer.txData = request.tx;
        transfer.txLen = (request.txLen > sizeof(request.tx)) ? sizeof(request.tx) : request.txLen;
        transfer.rxData = result.data;
        transfer.rxLen = (request.rxLen > sizeof(result.data)) ? sizeof(result.data) : request.rxLen;
        break;
      default:
        opStatus = Status(Err::INVALID_CONFIG, 0, "I2C op unsupported");
        break;
    }

    if (opStatus.ok()) {
      opStatus = _backend->transfer(transfer, result.durationUs);
      result.dataLen = static_cast<uint8_t>(transfer.rxLen);
    }
  }

  result.completedMs = readNowMs();
  if (result.completedMs == 0) {
    result.completedMs = nowMs;
  }

  if (request.deadlineMs != 0 && static_cast<int32_t>(result.completedMs - request.deadlineMs) > 0) {
    result.late = true;
    if (opStatus.ok()) {
      opStatus = Status(Err::TIMEOUT, 0, "I2C result exceeded deadline");
    }
#ifdef ARDUINO
    if (_metricsMutex && xSemaphoreTake(_metricsMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
      _metrics.staleResultCount++;
      xSemaphoreGive(_metricsMutex);
    }
#else
    _metrics.staleResultCount++;
#endif
    _lastStaleResultMs = result.completedMs;
  }

  result.status = opStatus;

  const bool probeExpectedNack =
      (request.op == I2cOpType::PROBE && result.status.code == Err::COMM_FAILURE);

  // ENV measurement-not-ready (RESOURCE_BUSY from trigger/read) is a normal
  // timing condition, not a bus error.  Don't count it as either a bus-level
  // failure or a recovery trigger.
  const bool envNotReady =
      ((request.op == I2cOpType::ENV_READ_ONESHOT || request.op == I2cOpType::ENV_TRIGGER_ONESHOT) &&
       result.status.code == Err::RESOURCE_BUSY);

  if (result.status.ok() || probeExpectedNack || envNotReady) {
    updateMetricsSuccess(result.durationUs, result.completedMs);
    if (!probeExpectedNack && !envNotReady && result.durationUs >= _settings.i2cSlowOpThresholdUs) {
      _recoveryPolicy.onFailure();
      if (_recoveryPolicy.shouldRecover(result.completedMs)) {
        recoverBus(result.completedMs);
      }
    } else {
      _recoveryPolicy.onSuccess();
    }
  } else {
    updateMetricsError(result.status, result.completedMs);
    _recoveryPolicy.onFailure();
    if (result.op != I2cOpType::RECOVER && _recoveryPolicy.shouldRecover(result.completedMs)) {
      recoverBus(result.completedMs);
    }
  }

  return result;
}

#ifdef ARDUINO
void I2cTask::taskEntry(void* arg) {
  I2cTask* self = static_cast<I2cTask*>(arg);
  self->taskLoop();
}

void I2cTask::taskLoop() {
  while (_running) {
    const uint32_t loopNowMs = readNowMs();
    updateTaskHeartbeat(loopNowMs);

    I2cRequest request{};
    // H2 fix: snapshot wait time under mutex to avoid torn read.
    uint32_t taskWaitMs = 10;
    if (_metricsMutex && xSemaphoreTake(_metricsMutex, pdMS_TO_TICKS(2)) == pdTRUE) {
      taskWaitMs = _settings.i2cTaskWaitMs;
      xSemaphoreGive(_metricsMutex);
    }
    if (_requestQueue != nullptr &&
        xQueueReceive(_requestQueue, &request, pdMS_TO_TICKS(taskWaitMs)) == pdTRUE) {
      const uint32_t nowMs = readNowMs();
      if (request.deadlineMs != 0 && static_cast<int32_t>(nowMs - request.deadlineMs) > 0) {
        I2cResult expired{};
        expired.op = request.op;
        expired.deviceId = request.deviceId;
        expired.address = request.address;
        expired.token = request.token;
        expired.completedMs = nowMs;
        expired.requestDeadlineMs = request.deadlineMs;
        expired.late = true;
        expired.status = Status(Err::TIMEOUT, 0, "I2C request expired in queue");
        updateMetricsError(expired.status, nowMs);
        _recoveryPolicy.onFailure();
        if (_metricsMutex && xSemaphoreTake(_metricsMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
          _metrics.staleResultCount++;
          xSemaphoreGive(_metricsMutex);
        }
        _lastStaleResultMs = nowMs;
        queueResult(expired, nowMs);
        continue;
      }

      const I2cResult result = processRequest(request, nowMs);
      queueResult(result, result.completedMs);
    }
  }

  // M3 fix: signal completion so end() can proceed without force-deleting.
  if (_completionSem != nullptr) {
    xSemaphoreGive(_completionSem);
  }
  _taskHandle = nullptr;
  vTaskDelete(nullptr);
}
#endif

}  // namespace TFLunaControl

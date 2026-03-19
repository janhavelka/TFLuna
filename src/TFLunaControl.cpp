#include "TFLunaControl/TFLunaControl.h"

#if __has_include("TFLunaControl/Version.h")
#include "TFLunaControl/Version.h"
#else
namespace TFLunaControl { static constexpr const char* VERSION = "dev"; }
#endif

#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <new>

#include "control/OutputController.h"
#include "core/CommandQueue.h"
#include "core/DynamicRingBuffer.h"
#include "core/PsramSupport.h"
#include "core/QueueHealth.h"
#include "core/Scheduler.h"
#include "core/SystemClock.h"
#include "devices/ButtonManager.h"
#include "devices/LidarAdapter.h"
#include "devices/EnvSensorAdapter.h"
#include "devices/RtcAdapter.h"
#include "devices/StatusLedAdapter.h"
#include "i2c/I2cOrchestrator.h"
#include "i2c/I2cTask.h"
#include "logging/SdLogger.h"
#include "settings/SettingsStore.h"
#include "core/TimeUtil.h"
#include "web/WebServer.h"

#ifdef ARDUINO
#include <Arduino.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#if __has_include(<esp_mac.h>)
#include <esp_mac.h>
#elif __has_include(<esp_system.h>)
#include <esp_system.h>
#endif
#endif

namespace TFLunaControl {

enum EventCode : uint16_t {
  EVENT_BOOT = 1,
  EVENT_SETTINGS_APPLIED = 2,
  EVENT_SETTINGS_RESET = 3,
  EVENT_SD_REMOUNT_REQUESTED = 4,
  EVENT_SD_REMOUNT_RESULT = 5,
  EVENT_I2C_RECOVERY = 6,
  EVENT_LOGGER_DROP = 7,
  EVENT_LOGGER_ERROR = 8,
  EVENT_FACTORY_RESET_FAILED = 9,
  EVENT_OUTPUT_OVERRIDE = 10,
  EVENT_I2C_RECOVERY_REQUESTED = 11,
  EVENT_LIDAR_RECOVERY_REQUESTED = 12,
  EVENT_OUTPUT_TEST = 13,
  EVENT_ENV_HEALTH_CHANGE = 14,
  EVENT_RTC_HEALTH_CHANGE = 15
};

static constexpr uint32_t LOGGER_EVENT_THROTTLE_MS = 10000UL;

static const char* healthName(HealthState h) {
  switch (h) {
    case HealthState::OK:       return "ok";
    case HealthState::DEGRADED: return "degraded";
    case HealthState::FAULT:    return "fault";
    default:                    return "unknown";
  }
}

enum class AppCommandType : uint8_t {
  APPLY_SETTINGS = 0,
  SET_WIFI_AP_ENABLED,
  SET_RTC_TIME,
  REMOUNT_SD,
  SET_OUTPUT_OVERRIDE,
  SET_OUTPUT_CHANNEL_TEST,
  RECOVER_I2C,
  RECOVER_LIDAR,
  PROBE_LIDAR,
  PROBE_SD,
  SCAN_I2C,
  RAW_I2C
};

struct AppCommand {
  AppCommandType type = AppCommandType::REMOUNT_SD;
  RuntimeSettings settings{};
  char settingsChangeHint[sizeof(Event::msg)] = {0};
  bool apEnabled = false;
  RtcTime rtcTime{};
  OutputOverrideMode outputOverride = OutputOverrideMode::AUTO;
  uint8_t outputChannelIndex = 0;
  bool outputChannelTestEnabled = false;
  bool outputChannelState = false;
  I2cOpType i2cOp = I2cOpType::NONE;
  uint8_t i2cAddress = 0;
  uint8_t i2cTx[HardwareSettings::I2C_PAYLOAD_BYTES] = {0};
  uint8_t i2cTxLen = 0;
  uint8_t i2cRxLen = 0;
  bool persist = false;
};

struct TFLunaControl::Impl {
  DynamicRingBuffer<Sample> samples;
  DynamicRingBuffer<Event> events;
  PeriodicTimer sampleTimer;
  CommandQueue<AppCommand, HardwareSettings::COMMAND_QUEUE_CAPACITY> commandQueue;

  EnvSensorAdapter env;
  LidarAdapter lidar;
  RtcAdapter rtc;
  I2cTask i2cTask;
  I2cOrchestrator i2cOrchestrator;
  OutputController outputs;
  SdLogger sdLogger;
  SettingsStore settingsStore;
  ButtonManager button;
  StatusLedAdapter leds;
  WebServer web;

  uint32_t bootMs = 0;
  uint32_t lastSampleMs = 0;
  uint32_t sampleCount = 0;
  uint32_t lastClientMs = 0;
  uint32_t lastStationSeenMs = 0;
  uint32_t lastNowMs = 0;
  uint32_t nextApStartRetryMs = 0;
  uint32_t tickDurationUs = 0;
  uint32_t tickMaxDurationUs = 0;
  uint64_t tickTotalDurationUs = 0;
  uint64_t tickCount = 0;
  uint32_t tickSlowCount = 0;
  uint32_t tickLastSlowMs = 0;
  uint32_t tickMaxAtMs = 0;
  uint32_t tickMaxPhaseUsCmd = 0;
  uint32_t tickMaxPhaseUsCo2 = 0;
  uint32_t tickMaxPhaseUsI2c = 0;
  uint32_t tickMaxPhaseUsSd = 0;
  uint32_t tickMaxPhaseUsIo = 0;
  uint32_t tickMaxPhaseUsStatus = 0;
  uint32_t tickMaxPhaseUsLed = 0;
  uint32_t tickSlowDomCmdCount = 0;
  uint32_t tickSlowDomCo2Count = 0;
  uint32_t tickSlowDomI2cCount = 0;
  uint32_t tickSlowDomSdCount = 0;
  uint32_t tickSlowDomIoCount = 0;
  uint32_t tickSlowDomStatusCount = 0;
  uint32_t tickSlowDomLedCount = 0;
  uint32_t tickSlowDomOtherCount = 0;
  uint8_t webOverrunBurst = 0;
  bool webThrottled = false;
  uint32_t webThrottleUntilMs = 0;
  uint32_t webSkipCount = 0;
  uint32_t lastCommandErrorMs = 0;
  Status lastCommandStatus = Ok();
  Status lastLogEnqueueStatus = Ok();
  Status wifiLastStatus = Ok();
  Status buttonLastStatus = Ok();
  Status outputsLastStatus = Ok();
  bool buttonEnabled = false;
  bool lastSdMounted = false;
  uint32_t lastValidCo2Ms = 0;
  uint32_t lastValidEnvMs = 0;
  uint32_t lastI2cRecoveryCount = 0;

  // Cached heap / stack metrics ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â refreshed once per second to avoid
  // calling expensive heap_caps_get_largest_free_block / stack watermark
  // APIs on every tick.
  uint32_t lastHeapRefreshMs = 0;
  uint32_t cachedHeapFreeBytes = 0;
  uint32_t cachedHeapMinFreeBytes = 0;
  uint32_t cachedHeapTotalBytes = 0;
  uint32_t cachedHeapMaxAllocBytes = 0;
  bool psramAvailableAtBoot = false;
  uint32_t cachedPsramTotalBytes = 0;
  uint32_t cachedPsramFreeBytes = 0;
  uint32_t cachedPsramMinFreeBytes = 0;
  uint32_t cachedPsramMaxAllocBytes = 0;
  uint32_t cachedMainStackFreeBytes = 0;
  uint32_t cachedI2cStackFreeBytes = 0;
  I2cRecoveryStage lastI2cRecoveryStage = I2cRecoveryStage::NONE;
  HealthState lastEnvHealthEvent = HealthState::UNKNOWN;
  HealthState lastRtcHealthEvent = HealthState::UNKNOWN;
  uint32_t lastLoggedDropCount = 0;
  uint32_t lastLoggedDropEventMs = 0;
  Err lastLoggedLoggerErrorCode = Err::OK;
  int32_t lastLoggedLoggerErrorDetail = 0;
  uint32_t lastLoggedLoggerErrorMs = 0;
  uint32_t sdIssueLastMs = 0;
  uint32_t sdIssueLastDroppedCount = 0;
  uint32_t sdIssueLastEventDroppedCount = 0;
  uint32_t sdIssueLastSampleWriteFailureCount = 0;
  uint32_t sdIssueLastEventWriteFailureCount = 0;
  uint32_t sdIssueLastBudgetExceededCount = 0;
  bool initComplete = false;
  bool apEnabled = false;
  bool initEventLogged = false;
  bool settingsLoadedFromNvs = false;
  bool ledOk = true;
  StatusLedAdapter::HealthDebounceState ledHealthDebounce{};
  uint32_t stateMutexTimeoutMs = 10;
  DeviceStatus statusScratch[DEVICE_COUNT]{};

  // Deferred blocking operations ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â executed in processDeferred(), outside tick timing.
  bool nvsSavePending = false;
  RuntimeSettings nvsSavePayload{};
  bool deferredApStop = false;
  bool deferredApStart = false;
  RuntimeSettings deferredApStartSettings{};

#ifdef ARDUINO
  SemaphoreHandle_t stateMutex = nullptr;
  portMUX_TYPE commandQueueMux = portMUX_INITIALIZER_UNLOCKED;
#endif

  bool initMutexes() {
#ifdef ARDUINO
    stateMutex = xSemaphoreCreateMutex();
    if (stateMutex == nullptr) {
      return false;
    }
#endif
    return true;
  }

  void deinitMutexes() {
#ifdef ARDUINO
    if (stateMutex != nullptr) {
      vSemaphoreDelete(stateMutex);
      stateMutex = nullptr;
    }
#endif
  }

  bool lockState() {
#ifdef ARDUINO
    if (stateMutex == nullptr) {
      return false;
    }
    TickType_t waitTicks = pdMS_TO_TICKS(stateMutexTimeoutMs);
    if (waitTicks < 1) {
      waitTicks = 1;
    }
    return xSemaphoreTake(stateMutex, waitTicks) == pdTRUE;
#else
    return true;
#endif
  }

  bool tryLockState() {
#ifdef ARDUINO
    if (stateMutex == nullptr) {
      return false;
    }
    return xSemaphoreTake(stateMutex, 0) == pdTRUE;
#else
    return true;
#endif
  }

  void unlockState() {
#ifdef ARDUINO
    if (stateMutex != nullptr) {
      xSemaphoreGive(stateMutex);
    }
#endif
  }

  bool enqueueCommand(const AppCommand& command, uint32_t nowMs) {
#ifdef ARDUINO
    taskENTER_CRITICAL(&commandQueueMux);
#endif
    const bool ok = commandQueue.push(command, nowMs);
#ifdef ARDUINO
    taskEXIT_CRITICAL(&commandQueueMux);
#endif
    return ok;
  }

  bool dequeueCommand(AppCommand& command) {
#ifdef ARDUINO
    taskENTER_CRITICAL(&commandQueueMux);
#endif
    const bool ok = commandQueue.pop(command);
#ifdef ARDUINO
    taskEXIT_CRITICAL(&commandQueueMux);
#endif
    return ok;
  }

  size_t queueDepth() {
#ifdef ARDUINO
    taskENTER_CRITICAL(&commandQueueMux);
#endif
    const size_t depth = commandQueue.depth();
#ifdef ARDUINO
    taskEXIT_CRITICAL(&commandQueueMux);
#endif
    return depth;
  }

  uint32_t queueOverflowCount() {
#ifdef ARDUINO
    taskENTER_CRITICAL(&commandQueueMux);
#endif
    const uint32_t count = commandQueue.overflowCount();
#ifdef ARDUINO
    taskEXIT_CRITICAL(&commandQueueMux);
#endif
    return count;
  }

  uint32_t queueLastOverflowMs() {
#ifdef ARDUINO
    taskENTER_CRITICAL(&commandQueueMux);
#endif
    const uint32_t ms = commandQueue.lastOverflowMs();
#ifdef ARDUINO
    taskEXIT_CRITICAL(&commandQueueMux);
#endif
    return ms;
  }
};

static const char* kDeviceNames[DEVICE_COUNT] = {
    "system", "i2c_bus", "sd", "env", "rtc", "lidar", "outputs", "wifi", "web", "leds", "button"};

static void updateDeviceStatus(DeviceStatus& ds, HealthState health, const Status& st, uint32_t nowMs) {
  ds.health = health;
  ds.lastStatus = st;
  ds.lastActivityMs = nowMs;
  if (st.ok()) {
    ds.lastOkMs = nowMs;
  } else {
    ds.lastErrorMs = nowMs;
    ds.errorCount++;
  }
}

static void applyDefaultSsid(RuntimeSettings& settings) {
  if (strncmp(settings.apSsid, "TFLuna-XXXX", sizeof(settings.apSsid)) != 0) {
    return;
  }
#ifdef ARDUINO
  // Read MAC from eFuse directly ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â works before WiFi.begin().
  // WiFi.macAddress() returns all-zeros until the radio is started.
  uint8_t mac[6] = {0};
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  snprintf(settings.apSsid, sizeof(settings.apSsid), "TFLuna-%02X%02X", mac[4], mac[5]);
#endif
}

static HealthState worstHealth(const DeviceStatus* devices, size_t count,
                               const HardwareSettings& /*config*/) {
  bool anyDegraded = false;
  for (size_t i = 0; i < count; ++i) {
    if (devices[i].id == DeviceId::SYSTEM) {
      continue;
    }
    if (devices[i].optional) {
      continue;
    }
    if (devices[i].health == HealthState::FAULT) {
      return HealthState::FAULT;
    }
    if (devices[i].health == HealthState::DEGRADED) {
      anyDegraded = true;
    }
  }
  return anyDegraded ? HealthState::DEGRADED : HealthState::OK;
}

static StatusLedAdapter::HealthState toLedHealth(HealthState health) {
  switch (health) {
    case HealthState::FAULT:
      return StatusLedAdapter::HealthState::FAULT;
    case HealthState::DEGRADED:
      return StatusLedAdapter::HealthState::DEGRADED;
    case HealthState::OK:
      return StatusLedAdapter::HealthState::OK;
    case HealthState::UNKNOWN:
    default:
      return StatusLedAdapter::HealthState::DEGRADED;
  }
}

static void setEventMessage(Event& event, uint16_t code, const char* msg) {
  event.code = code;
  if (msg == nullptr) {
    event.msg[0] = '\0';
    return;
  }
  strncpy(event.msg, msg, sizeof(event.msg) - 1);
  event.msg[sizeof(event.msg) - 1] = '\0';
}

static Status validateAppSettings(const AppSettings& appSettings) {
  if (appSettings.enableWeb) {
    if (appSettings.webBroadcastMs == 0U ||
        appSettings.webBroadcastMs > static_cast<uint32_t>(INT32_MAX)) {
      return Status(Err::INVALID_CONFIG, 0, "webBroadcastMs out of range");
    }
  }

  if (!appSettings.enableSd) {
    return Ok();
  }

  if (appSettings.sdWorkerStackBytes == 0U) {
    return Status(Err::INVALID_CONFIG, 0, "sdWorkerStackBytes invalid");
  }
  if (appSettings.sdWorkerIdleMs == 0U) {
    return Status(Err::INVALID_CONFIG, 0, "sdWorkerIdleMs invalid");
  }
  if (appSettings.sdRequestQueueDepth == 0U || appSettings.sdResultQueueDepth == 0U) {
    return Status(Err::INVALID_CONFIG, 0, "sd queue depth invalid");
  }
  if (appSettings.sdMaxOpenFiles == 0U) {
    return Status(Err::INVALID_CONFIG, 0, "sdMaxOpenFiles invalid");
  }
  if (appSettings.sdMaxPathLength == 0U) {
    return Status(Err::INVALID_CONFIG, 0, "sdMaxPathLength invalid");
  }
  if (appSettings.sdCopyWriteSlots > 0U && appSettings.sdMaxCopyWriteBytes == 0U) {
    return Status(Err::INVALID_CONFIG, 0, "sd copy-write config invalid");
  }
  if (appSettings.sdLockTimeoutMs == 0U ||
      appSettings.sdMountTimeoutMs == 0U ||
      appSettings.sdOpTimeoutMs == 0U ||
      appSettings.sdIoTimeoutMs == 0U ||
      appSettings.sdShutdownTimeoutMs == 0U) {
    return Status(Err::INVALID_CONFIG, 0, "sd timeout config invalid");
  }
  if (appSettings.sdIoChunkBytes == 0U) {
    return Status(Err::INVALID_CONFIG, 0, "sdIoChunkBytes invalid");
  }
  if (appSettings.sdSpiFrequencyHz == 0U) {
    return Status(Err::INVALID_CONFIG, 0, "sdSpiFrequencyHz invalid");
  }
  return Ok();
}

void TFLunaControl::pushEvent(uint32_t nowMs, uint16_t code, const char* msg) {
  if (_impl == nullptr) {
    return;
  }

  Event event{};
  if (_impl->lockState()) {
    const Sample* latest = _impl->samples.latest();
    if (latest != nullptr) {
      event.tsUnix = latest->tsUnix;
      strncpy(event.tsLocal, latest->tsLocal, sizeof(event.tsLocal) - 1);
      event.tsLocal[sizeof(event.tsLocal) - 1] = '\0';
    }
    _impl->unlockState();
  }
  setEventMessage(event, code, msg);

  if (_impl->lockState()) {
    _impl->events.push(event);
    _impl->unlockState();
  }

  if (_appSettings.enableSd) {
    _impl->sdLogger.logEvent(event, nowMs);
  }
}

Status TFLunaControl::begin(const HardwareSettings& config) {
  return begin(config, AppSettings());
}

Status TFLunaControl::begin(const HardwareSettings& config, const AppSettings& appSettings) {
  if (_initialized) {
    return Status(Err::RESOURCE_BUSY, 0, "already initialized");
  }

  const Status appSettingsValidation = validateAppSettings(appSettings);
  if (!appSettingsValidation.ok()) {
    return appSettingsValidation;
  }

  if (appSettings.enableSd) {
    if (config.sdCs < 0 || config.spiSck < 0 || config.spiMiso < 0 || config.spiMosi < 0) {
      return Status(Err::INVALID_CONFIG, 0, "SD pins not set");
    }
  }

  _impl = new (std::nothrow) Impl();
  if (!_impl) {
    return Status(Err::OUT_OF_MEMORY, 0, "alloc failed");
  }

  if (!_impl->initMutexes()) {
    delete _impl;
    _impl = nullptr;
    return Status(Err::OUT_OF_MEMORY, 0, "mutex alloc failed");
  }

  _config = config;
  _appSettings = appSettings;
  _impl->stateMutexTimeoutMs = _appSettings.stateMutexTimeoutMs;
  _impl->psramAvailableAtBoot = PsramSupport::isAvailable();
  _impl->cachedPsramTotalBytes = PsramSupport::totalBytes();
  _impl->cachedPsramFreeBytes = PsramSupport::freeBytes();
  _impl->cachedPsramMinFreeBytes = PsramSupport::minFreeBytes();
  _impl->cachedPsramMaxAllocBytes = PsramSupport::maxAllocBytes();
  _settings.restoreDefaults();
  applyDefaultSsid(_settings);

  Status st = _impl->settingsStore.begin(_appSettings.enableNvs);
  if (!st.ok()) {
    // Continue without persistence
  }

  st = _impl->settingsStore.load(_settings);
  _impl->settingsLoadedFromNvs = st.ok();
  if (!st.ok()) {
    _settings.restoreDefaults();
    applyDefaultSsid(_settings);
  }

  st = _settings.validate();
  if (!st.ok()) {
    _settings.restoreDefaults();
    applyDefaultSsid(_settings);
    _impl->settingsLoadedFromNvs = false;
  }
  _settings.i2cRtcAddress = 0x51;
  applyDefaultSsid(_settings);

  _impl->apEnabled = _settings.wifiEnabled;
  if (!_appSettings.enableWeb) {
    _impl->apEnabled = false;
  }

  for (size_t i = 0; i < DEVICE_COUNT; ++i) {
    _deviceStatus[i].id = static_cast<DeviceId>(i);
    _deviceStatus[i].name = kDeviceNames[i];
    _deviceStatus[i].health = HealthState::UNKNOWN;
  }

  _impl->bootMs = 0;
  _impl->lastSampleMs = 0;
  _impl->sampleCount = 0;
  _impl->lastClientMs = 0;
  _impl->lastStationSeenMs = 0;
  _impl->nextApStartRetryMs = 0;
  _impl->tickDurationUs = 0;
  _impl->tickMaxDurationUs = 0;
  _impl->tickTotalDurationUs = 0;
  _impl->tickCount = 0;
  _impl->tickSlowCount = 0;
  _impl->tickLastSlowMs = 0;
  _impl->tickMaxAtMs = 0;
  _impl->tickMaxPhaseUsCmd = 0;
  _impl->tickMaxPhaseUsCo2 = 0;
  _impl->tickMaxPhaseUsI2c = 0;
  _impl->tickMaxPhaseUsSd = 0;
  _impl->tickMaxPhaseUsIo = 0;
  _impl->tickMaxPhaseUsStatus = 0;
  _impl->tickMaxPhaseUsLed = 0;
  _impl->tickSlowDomCmdCount = 0;
  _impl->tickSlowDomCo2Count = 0;
  _impl->tickSlowDomI2cCount = 0;
  _impl->tickSlowDomSdCount = 0;
  _impl->tickSlowDomIoCount = 0;
  _impl->tickSlowDomStatusCount = 0;
  _impl->tickSlowDomLedCount = 0;
  _impl->tickSlowDomOtherCount = 0;
  _impl->webOverrunBurst = 0;
  _impl->webThrottled = false;
  _impl->webThrottleUntilMs = 0;
  _impl->webSkipCount = 0;
  _impl->lastCommandErrorMs = 0;
  _impl->lastCommandStatus = Ok();
  _impl->lastLogEnqueueStatus = Ok();
  _impl->wifiLastStatus = Ok();
  _impl->buttonLastStatus = Ok();
  _impl->outputsLastStatus = Ok();
  _impl->buttonEnabled = (_config.buttonPin >= 0);
  _impl->lastSdMounted = false;
  _impl->lastValidCo2Ms = 0;
  _impl->lastValidEnvMs = 0;
  _impl->lastI2cRecoveryCount = 0;
  _impl->lastI2cRecoveryStage = I2cRecoveryStage::NONE;
  _impl->lastLoggedDropCount = 0;
  _impl->lastLoggedDropEventMs = 0;
  _impl->lastLoggedLoggerErrorCode = Err::OK;
  _impl->lastLoggedLoggerErrorDetail = 0;
  _impl->lastLoggedLoggerErrorMs = 0;
  _impl->sdIssueLastMs = 0;
  _impl->sdIssueLastDroppedCount = 0;
  _impl->sdIssueLastEventDroppedCount = 0;
  _impl->sdIssueLastSampleWriteFailureCount = 0;
  _impl->sdIssueLastEventWriteFailureCount = 0;
  _impl->sdIssueLastBudgetExceededCount = 0;
  _impl->initComplete = false;
  _impl->initEventLogged = false;

  const size_t desiredSampleCapacity = _impl->psramAvailableAtBoot
      ? HardwareSettings::SAMPLE_HISTORY_CAPACITY_PSRAM
      : HardwareSettings::SAMPLE_HISTORY_CAPACITY;
  const size_t desiredEventCapacity = _impl->psramAvailableAtBoot
      ? HardwareSettings::EVENT_HISTORY_CAPACITY_PSRAM
      : HardwareSettings::EVENT_HISTORY_CAPACITY;
  Status sampleRingSt = _impl->samples.begin(desiredSampleCapacity, _impl->psramAvailableAtBoot);
  if (!sampleRingSt.ok() && _impl->psramAvailableAtBoot) {
    const size_t midSampleCapacity = HardwareSettings::SAMPLE_HISTORY_CAPACITY_PSRAM / 2U;
    if (midSampleCapacity >= HardwareSettings::SAMPLE_HISTORY_CAPACITY) {
      sampleRingSt = _impl->samples.begin(midSampleCapacity, true);
    }
  }
  if (!sampleRingSt.ok() && _impl->psramAvailableAtBoot) {
    sampleRingSt = _impl->samples.begin(HardwareSettings::SAMPLE_HISTORY_CAPACITY, false);
  }
  if (!sampleRingSt.ok()) {
    _impl->deinitMutexes();
    delete _impl;
    _impl = nullptr;
    return sampleRingSt;
  }
  Status eventRingSt = _impl->events.begin(desiredEventCapacity, _impl->psramAvailableAtBoot);
  if (!eventRingSt.ok() && _impl->psramAvailableAtBoot) {
    const size_t midEventCapacity = HardwareSettings::EVENT_HISTORY_CAPACITY_PSRAM / 2U;
    if (midEventCapacity >= HardwareSettings::EVENT_HISTORY_CAPACITY) {
      eventRingSt = _impl->events.begin(midEventCapacity, true);
    }
  }
  if (!eventRingSt.ok() && _impl->psramAvailableAtBoot) {
    eventRingSt = _impl->events.begin(HardwareSettings::EVENT_HISTORY_CAPACITY, false);
  }
  if (!eventRingSt.ok()) {
    _impl->samples.end();
    _impl->deinitMutexes();
    delete _impl;
    _impl = nullptr;
    return eventRingSt;
  }

  _impl->samples.clear();
  _impl->events.clear();
  _impl->commandQueue.clear();
  _impl->ledHealthDebounce = StatusLedAdapter::HealthDebounceState{};

  _impl->sampleTimer.setInterval(_settings.sampleIntervalMs);

  const Status outputsBeginSt = _impl->outputs.begin(_config, _settings);
  _impl->outputsLastStatus = outputsBeginSt;
  updateDeviceStatus(_deviceStatus[static_cast<size_t>(DeviceId::OUTPUTS)],
                     outputsBeginSt.ok() ? HealthState::OK : HealthState::FAULT,
                     outputsBeginSt,
                     0);

  Status i2cTaskSt = _impl->i2cTask.begin(_config, _settings);
  Status i2cOrchSt = _impl->i2cOrchestrator.begin(_config, _appSettings, _settings, &_impl->i2cTask);
  HealthState i2cHealth = _impl->i2cOrchestrator.busHealth();
  Status i2cStatus = i2cTaskSt.ok() ? i2cOrchSt : i2cTaskSt;
  if (i2cStatus.ok() && i2cHealth != HealthState::OK) {
    i2cStatus = _impl->i2cOrchestrator.busStatus();
  }
  updateDeviceStatus(_deviceStatus[static_cast<size_t>(DeviceId::I2C_BUS)], i2cHealth, i2cStatus, 0);

  Status rtcBeginSt = _impl->rtc.begin(_config, &_impl->i2cOrchestrator);
  updateDeviceStatus(_deviceStatus[static_cast<size_t>(DeviceId::RTC)],
                     rtcBeginSt.ok() ? HealthState::DEGRADED : HealthState::FAULT, rtcBeginSt, 0);

  Status envSt = _impl->env.begin(_config, &_impl->i2cOrchestrator);
  updateDeviceStatus(_deviceStatus[static_cast<size_t>(DeviceId::ENV)], _impl->env.health(), envSt, 0);
  _impl->lidar.applySettings(_settings, 0);
  Status co2St = _impl->lidar.begin(_config);
  updateDeviceStatus(_deviceStatus[static_cast<size_t>(DeviceId::LIDAR)], _impl->lidar.health(), co2St, 0);

  Status sdSt = _impl->sdLogger.begin(_config, _appSettings, _settings);
  HealthState sdHealth =
      (!_appSettings.enableSd) ? HealthState::OK : (sdSt.ok() ? HealthState::OK : HealthState::DEGRADED);
  updateDeviceStatus(_deviceStatus[static_cast<size_t>(DeviceId::SD)], sdHealth, sdSt, 0);

  _impl->buttonLastStatus = _impl->button.begin(_config);
  if (!_impl->buttonEnabled) {
    _impl->buttonLastStatus = Status(Err::NOT_INITIALIZED, 0, "button disabled");
  }
  updateDeviceStatus(_deviceStatus[static_cast<size_t>(DeviceId::BUTTON)],
                     _impl->buttonEnabled ? HealthState::OK : HealthState::DEGRADED,
                     _impl->buttonLastStatus,
                     0);
  Status ledSt = _impl->leds.begin(_config);
  _impl->ledOk = ledSt.ok() || (_config.ledPin < 0);
  updateDeviceStatus(_deviceStatus[static_cast<size_t>(DeviceId::LEDS)],
                     ledSt.ok() ? HealthState::OK : HealthState::DEGRADED, ledSt, 0);

  if (_appSettings.enableWeb) {
    _impl->web.setPsramAvailable(_impl->psramAvailableAtBoot);
    _impl->web.setPort(_appSettings.webPort);
    _impl->web.setUiRefreshTiming(_appSettings.webUiWsReconnectMs,
                                  _appSettings.webUiGraphRefreshMs,
                                  _appSettings.webUiEventsRefreshMs);
    _impl->web.setUiEventFetchCount(_appSettings.webUiEventFetchCount);
    const Status webBeginSt = _impl->web.begin(this);
    _impl->wifiLastStatus = webBeginSt;
    if (webBeginSt.ok()) {
      _impl->web.setBroadcastInterval(_appSettings.webBroadcastMs);
    } else {
      // Fail-safe: do not keep requesting AP start if web server failed to initialize.
      _impl->apEnabled = false;
    }
  } else {
    _impl->wifiLastStatus = Status(Err::NOT_INITIALIZED, 0, "web disabled");
  }
  updateDeviceStatus(_deviceStatus[static_cast<size_t>(DeviceId::WIFI)],
                     _appSettings.enableWeb ? HealthState::DEGRADED : HealthState::OK,
                     _impl->wifiLastStatus,
                     0);

  _systemStatus.health = HealthState::UNKNOWN;
  _systemStatus.lastStatus = Ok();

  _initialized = true;
  return Ok();
}

void TFLunaControl::end() {
  if (!_initialized) {
    return;
  }

  _impl->web.end();
  _impl->leds.end();
  _impl->outputs.end();
  _impl->i2cOrchestrator.end();
  _impl->i2cTask.end();
  _impl->sdLogger.end();
  _impl->samples.end();
  _impl->events.end();
  _impl->deinitMutexes();

  delete _impl;
  _impl = nullptr;
  _initialized = false;
}

void TFLunaControl::tick(uint32_t nowMs) {
  if (!_initialized) {
    return;
  }

  const uint32_t tickStartUs = SystemClock::nowUs();
  uint32_t tickPhaseCmdUs = 0;
  uint32_t tickPhaseCo2Us = 0;
  uint32_t tickPhaseI2cUs = 0;
  uint32_t tickPhaseSdUs = 0;
  uint32_t tickPhaseIoUs = 0;
  uint32_t tickPhaseStatusUs = 0;
  uint32_t tickPhaseLedUs = 0;
  uint32_t tickPhaseCmdOverlapUs = 0;

  _impl->lastNowMs = nowMs;

  if (!_impl->initEventLogged) {
    pushEvent(nowMs,
              EVENT_BOOT,
              _impl->settingsLoadedFromNvs ? "boot settings=nvs" : "boot settings=defaults");
    _impl->initEventLogged = true;
  }

  auto updateStatusLocked = [this, nowMs](DeviceId id, HealthState health, const Status& st) {
    if (!_impl->lockState()) {
      return;
    }
    updateDeviceStatus(_deviceStatus[static_cast<size_t>(id)], health, st, nowMs);
    _impl->unlockState();
  };

  if (_impl->bootMs == 0) {
    _impl->bootMs = nowMs;
  }

  const uint32_t uptimeMs = nowMs - _impl->bootMs;

  _impl->button.tick(nowMs);

  RuntimeSettings currentSettings = getSettings();
  if (_impl->button.consumeMultiPress()) {
    RuntimeSettings updated = currentSettings;
    strncpy(updated.apSsid, "TFLuna-XXXX", sizeof(updated.apSsid) - 1);
    updated.apSsid[sizeof(updated.apSsid) - 1] = '\0';
    strncpy(updated.apPass, "tflunactrl", sizeof(updated.apPass) - 1);
    updated.apPass[sizeof(updated.apPass) - 1] = '\0';
    applyDefaultSsid(updated);
    updateSettings(updated, true);
  }

  if (_impl->button.consumeShortPress()) {
    if (!_impl->web.isApRunning()) {
      _impl->apEnabled = true;
    }
  }

  if (_impl->button.consumeLongPress()) {
    _impl->apEnabled = false;
  }

  const RuntimeSettings commandSettings = getSettings();
  const uint32_t commandPhaseStartUs = SystemClock::nowUs();
  for (size_t i = 0; i < static_cast<size_t>(commandSettings.commandDrainPerTick); ++i) {
    AppCommand command;
    if (!_impl->dequeueCommand(command)) {
      break;
    }

    Status commandStatus = Ok();
    if (command.type == AppCommandType::APPLY_SETTINGS) {
      const char* changeHint = (command.settingsChangeHint[0] != '\0') ? command.settingsChangeHint : nullptr;
      commandStatus = updateSettings(command.settings, command.persist, changeHint);
      if (commandStatus.ok()) {
        _impl->leds.flashSuccess();
      }
    } else if (command.type == AppCommandType::SET_WIFI_AP_ENABLED) {
      _impl->apEnabled = command.apEnabled;
      commandStatus = Ok();
    } else if (command.type == AppCommandType::SET_RTC_TIME) {
      commandStatus = setRtcTime(command.rtcTime);
    } else if (command.type == AppCommandType::REMOUNT_SD) {
      pushEvent(nowMs, EVENT_SD_REMOUNT_REQUESTED, "sd remount requested");
      commandStatus = remountSd();
      pushEvent(nowMs,
                EVENT_SD_REMOUNT_RESULT,
                commandStatus.ok() ? "sd remount queued" : "sd remount failed");
    } else if (command.type == AppCommandType::SET_OUTPUT_OVERRIDE) {
      _impl->outputs.setOverrideMode(command.outputOverride, nowMs);
      commandStatus = Ok();
      if (command.outputOverride == OutputOverrideMode::FORCE_ON) {
        pushEvent(nowMs, EVENT_OUTPUT_OVERRIDE, "outputs override on");
      } else if (command.outputOverride == OutputOverrideMode::FORCE_OFF) {
        pushEvent(nowMs, EVENT_OUTPUT_OVERRIDE, "outputs override off");
      } else {
        pushEvent(nowMs, EVENT_OUTPUT_OVERRIDE, "outputs override auto");
      }
    } else if (command.type == AppCommandType::SET_OUTPUT_CHANNEL_TEST) {
      commandStatus = _impl->outputs.setChannelTestOverride(command.outputChannelIndex,
                                                            command.outputChannelTestEnabled,
                                                            command.outputChannelState,
                                                            nowMs);
      if (commandStatus.ok()) {
        pushEvent(nowMs, EVENT_OUTPUT_TEST, "output test updated");
        _impl->leds.flashSuccess();
      }
    } else if (command.type == AppCommandType::RECOVER_I2C) {
      pushEvent(nowMs, EVENT_I2C_RECOVERY_REQUESTED, "i2c recovery requested");
      commandStatus = _impl->i2cOrchestrator.queueBusRecover(nowMs);
    } else if (command.type == AppCommandType::RECOVER_LIDAR) {
      pushEvent(nowMs, EVENT_LIDAR_RECOVERY_REQUESTED, "lidar recovery requested");
      commandStatus = _impl->lidar.forceRecover(nowMs);
    } else if (command.type == AppCommandType::PROBE_LIDAR) {
      Sample probeSample{};
      const uint32_t co2PhaseStartUs = SystemClock::nowUs();
      commandStatus = _impl->lidar.probeOnce(probeSample, nowMs);
      const uint32_t co2ElapsedUs = (SystemClock::nowUs() - co2PhaseStartUs);
      tickPhaseCo2Us += co2ElapsedUs;
      tickPhaseCmdOverlapUs += co2ElapsedUs;
      updateStatusLocked(DeviceId::LIDAR, _impl->lidar.health(), commandStatus);
    } else if (command.type == AppCommandType::PROBE_SD) {
      commandStatus = _impl->sdLogger.probe(nowMs);
      updateStatusLocked(DeviceId::SD, commandStatus.ok() ? HealthState::OK : HealthState::DEGRADED, commandStatus);
    } else if (command.type == AppCommandType::SCAN_I2C) {
      commandStatus = _impl->i2cOrchestrator.queueBusScan(nowMs);
    } else if (command.type == AppCommandType::RAW_I2C) {
      I2cRequest req{};
      req.op = command.i2cOp;
      req.deviceId = DeviceId::I2C_BUS;
      req.address = command.i2cAddress;
      req.txLen = command.i2cTxLen;
      req.rxLen = command.i2cRxLen;
      if (req.txLen > 0U) {
        memcpy(req.tx, command.i2cTx, req.txLen);
      }
      req.timeoutMs = commandSettings.i2cOpTimeoutMs;
      commandStatus = _impl->i2cOrchestrator.queueRawRequest(req, nowMs);
    }

    if (!commandStatus.ok()) {
      _impl->lastCommandStatus = commandStatus;
      _impl->lastCommandErrorMs = nowMs;
    } else {
      _impl->lastCommandStatus = Ok();
    }
  }
  const uint32_t commandElapsedUs = (SystemClock::nowUs() - commandPhaseStartUs);
  tickPhaseCmdUs += (commandElapsedUs > tickPhaseCmdOverlapUs)
                        ? (commandElapsedUs - tickPhaseCmdOverlapUs)
                        : 0U;

  const RuntimeSettings tickSettings = getSettings();

  const uint32_t i2cPhaseStartUs = SystemClock::nowUs();
  _impl->i2cTask.tick(nowMs);
  _impl->i2cOrchestrator.tick(nowMs);
  tickPhaseI2cUs += (SystemClock::nowUs() - i2cPhaseStartUs);

  if (_appSettings.enableWeb) {
    if (_impl->apEnabled) {
      if (!_impl->web.isApRunning() && !_impl->deferredApStart) {
        if (_impl->nextApStartRetryMs == 0 ||
            static_cast<int32_t>(nowMs - _impl->nextApStartRetryMs) >= 0) {
          // Defer WiFi AP start to processDeferred() to avoid blocking tick.
          _impl->deferredApStart = true;
          _impl->deferredApStartSettings = tickSettings;
        }
      }
    } else {
      if (_impl->web.isApRunning() && !_impl->deferredApStop) {
        _impl->deferredApStop = true;
        _impl->wifiLastStatus = Ok();
      }
      _impl->lastStationSeenMs = 0;
      _impl->nextApStartRetryMs = 0;
    }

    if (_impl->web.isApRunning()) {
      const uint8_t stations = _impl->web.stationCount();
      const size_t webClients = _impl->web.webClientCount();
      if (stations > 0U) {
        _impl->lastStationSeenMs = nowMs;
      }
      if (stations > 0 || webClients > 0) {
        _impl->lastClientMs = nowMs;
      }
      if (stations == 0 && webClients == 0) {
        if (tickSettings.apAutoOffMs > 0U && (nowMs - _impl->lastClientMs) >= tickSettings.apAutoOffMs) {
          _impl->apEnabled = false;
          if (!_impl->deferredApStop) {
            _impl->deferredApStop = true;
          }
          _impl->wifiLastStatus = Ok();
        }
      }
    }
  } else {
    if (_impl->web.isApRunning() && !_impl->deferredApStop) {
      _impl->deferredApStop = true;
    }
    _impl->wifiLastStatus = Status(Err::NOT_INITIALIZED, 0, "web disabled");
    _impl->lastStationSeenMs = 0;
    _impl->nextApStartRetryMs = 0;
  }

  if (_impl->sampleTimer.isDue(nowMs)) {
    Sample sample{};
    sample.uptimeMs = uptimeMs;
    sample.sampleIndex = _impl->sampleCount + 1U;

    RtcTime rtcTime;
    const uint32_t rtcPhaseStartUs = SystemClock::nowUs();
    Status rtcSt = _impl->rtc.getTime(SystemClock::nowMs64(), rtcTime);
    tickPhaseI2cUs += (SystemClock::nowUs() - rtcPhaseStartUs);
    const bool rtcHasTime = rtcTime.valid;
    if (rtcHasTime) {
      sample.tsUnix = toUnixSeconds(rtcTime);
      formatLocalTime(rtcTime, sample.tsLocal, sizeof(sample.tsLocal));
    } else {
      sample.tsUnix = 0;
      sample.tsLocal[0] = '\0';
    }

    HealthState rtcHealth = HealthState::OK;
    if (!rtcSt.ok()) {
      if (_impl->rtc.consecutiveFailures() >= (tickSettings.i2cMaxConsecutiveFailures * 2U)) {
        rtcHealth = HealthState::FAULT;
      } else {
        rtcHealth = HealthState::DEGRADED;
      }
    }

    const uint32_t envPhaseStartUs = SystemClock::nowUs();
    Status envSt = _impl->env.readOnce(sample, nowMs);
    tickPhaseI2cUs += (SystemClock::nowUs() - envPhaseStartUs);
    const HealthState envHealth = _impl->env.health();
    if (((sample.validMask & VALID_TEMP) != 0U && isfinite(sample.tempC)) ||
        ((sample.validMask & VALID_RH) != 0U && isfinite(sample.rhPct))) {
      _impl->lastValidEnvMs = nowMs;
    }

    const uint32_t co2PhaseStartUs = SystemClock::nowUs();
    Status co2St = _impl->lidar.readOnce(sample, nowMs);
    tickPhaseCo2Us += (SystemClock::nowUs() - co2PhaseStartUs);
    const HealthState co2Health = _impl->lidar.health();
    if (sample.signalOk && sample.distanceCm > 0U) {
      _impl->lastValidCo2Ms = nowMs;
    }

    // Batch: update RTC/ENV/CO2 device status + push sample under one lock.
    if (_impl->lockState()) {
      updateDeviceStatus(_deviceStatus[static_cast<size_t>(DeviceId::RTC)], rtcHealth, rtcSt, nowMs);
      updateDeviceStatus(_deviceStatus[static_cast<size_t>(DeviceId::ENV)], envHealth, envSt, nowMs);
      updateDeviceStatus(_deviceStatus[static_cast<size_t>(DeviceId::LIDAR)], co2Health, co2St, nowMs);
      _impl->samples.push(sample);
      _impl->unlockState();
    }

    // Log ENV/RTC health state transitions.
    if (envHealth != _impl->lastEnvHealthEvent) {
      char buf[48];
      snprintf(buf, sizeof(buf), "env %s -> %s",
               healthName(_impl->lastEnvHealthEvent), healthName(envHealth));
      pushEvent(nowMs, EVENT_ENV_HEALTH_CHANGE, buf);
      _impl->lastEnvHealthEvent = envHealth;
    }
    if (rtcHealth != _impl->lastRtcHealthEvent) {
      char buf[48];
      snprintf(buf, sizeof(buf), "rtc %s -> %s",
               healthName(_impl->lastRtcHealthEvent), healthName(rtcHealth));
      pushEvent(nowMs, EVENT_RTC_HEALTH_CHANGE, buf);
      _impl->lastRtcHealthEvent = rtcHealth;
    }

    _impl->lastSampleMs = nowMs;
    _impl->sampleCount++;

    if (_appSettings.enableSd) {
      const uint32_t sdPhaseStartUs = SystemClock::nowUs();
      const Status logSt = _impl->sdLogger.logSample(sample, nowMs);
      tickPhaseSdUs += (SystemClock::nowUs() - sdPhaseStartUs);
      _impl->lastLogEnqueueStatus = logSt;
    }
  }

  {
    const uint32_t lidarServiceStartUs = SystemClock::nowUs();
    _impl->lidar.tick(nowMs);
    tickPhaseCo2Us += (SystemClock::nowUs() - lidarServiceStartUs);
  }

  if (_appSettings.enableSd) {
    const uint32_t sdPhaseStartUs = SystemClock::nowUs();
    _impl->sdLogger.tick(nowMs);
    const bool sdMounted = _impl->sdLogger.isMounted();
    if (sdMounted != _impl->lastSdMounted) {
      pushEvent(nowMs,
                EVENT_SD_REMOUNT_RESULT,
                sdMounted ? "sd mounted" : "sd offline");
      _impl->lastSdMounted = sdMounted;
    }
    tickPhaseSdUs += (SystemClock::nowUs() - sdPhaseStartUs);
  }

  const uint32_t ioPhaseStartUs = SystemClock::nowUs();
  const Sample* latest = _impl->samples.latest();
  Sample empty;
  if (!latest) {
    empty = Sample{};
    latest = &empty;
  }
  const bool latestCo2Valid =
      latest->signalOk && (latest->distanceCm > 0U);
  _impl->i2cOrchestrator.setDisplayCo2Snapshot(
      static_cast<float>(latest->distanceCm), latestCo2Valid, _impl->lastSampleMs);
  _impl->outputs.tick(*latest, nowMs);
  const uint8_t outputPresentMask = _impl->outputs.presentMask();
  uint8_t outputMask = 0;
  for (size_t i = 0; i < HardwareSettings::OUTPUT_CHANNEL_COUNT && i < 8U; ++i) {
    if (_impl->outputs.channelState(i)) {
      outputMask = static_cast<uint8_t>(outputMask | (1U << i));
    }
  }
  _impl->i2cOrchestrator.setDisplayOutputSnapshot(
      outputMask, _impl->outputs.overrideMode(), tickSettings.outputsEnabled);

  if (_appSettings.enableWeb) {
    // Reset throttle when hold period expires.
    if (_impl->webThrottled &&
        static_cast<int32_t>(nowMs - _impl->webThrottleUntilMs) >= 0) {
      _impl->webThrottled = false;
      _impl->webOverrunBurst = 0;
    }

    if (_impl->webThrottled) {
      _impl->webSkipCount++;
    } else {
      const uint32_t webStartUs = SystemClock::nowUs();
      _impl->web.tick(nowMs);
      const uint32_t webUs = SystemClock::nowUs() - webStartUs;
      if (webUs > tickSettings.webOverrunThresholdUs) {
        if (_impl->webOverrunBurst < 255U) {
          _impl->webOverrunBurst++;
        }
        if (!_impl->webThrottled &&
            _impl->webOverrunBurst >= tickSettings.webOverrunBurstThreshold) {
          _impl->webThrottled = true;
          _impl->webThrottleUntilMs = nowMs + tickSettings.webOverrunThrottleMs;
        }
      } else if (_impl->webOverrunBurst > 0U) {
        _impl->webOverrunBurst--;
      }
    }
  }

  StatusLedAdapter::WifiState wifiState = StatusLedAdapter::WifiState::AP_OFF;
  if (_appSettings.enableWeb && _impl->web.isApRunning()) {
    const uint8_t stations = _impl->web.stationCount();
    const size_t webClients = _impl->web.webClientCount();
    const bool recentUiActivity = _impl->web.hasRecentUiActivity(nowMs, 3000U);
    // 3 s grace period after last station seen ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â the ESP32 AP inactive
    // timeout (120 s) already provides the main hysteresis.
    const bool stationConnected =
        (stations > 0U) ||
        (_impl->lastStationSeenMs != 0U &&
         static_cast<int32_t>(nowMs - _impl->lastStationSeenMs) <= static_cast<int32_t>(3000U));
    if (webClients > 0U || recentUiActivity) {
      wifiState = StatusLedAdapter::WifiState::WEB_ACTIVE;
    } else if (stationConnected) {
      wifiState = StatusLedAdapter::WifiState::CONNECTED;
    } else {
      wifiState = StatusLedAdapter::WifiState::WAITING;
    }
  }
  tickPhaseIoUs += (SystemClock::nowUs() - ioPhaseStartUs);
  // --- Compute device health/status (lock-free) ---
  const uint32_t statusPhaseStartUs = SystemClock::nowUs();

  HealthState dsHealth[DEVICE_COUNT]{};
  Status dsStatus[DEVICE_COUNT]{};

  // I2C bus
  {
    dsHealth[static_cast<size_t>(DeviceId::I2C_BUS)] = _impl->i2cOrchestrator.busHealth();
    dsStatus[static_cast<size_t>(DeviceId::I2C_BUS)] = _impl->i2cOrchestrator.busStatus();
    auto& h = dsHealth[static_cast<size_t>(DeviceId::I2C_BUS)];
    auto& s = dsStatus[static_cast<size_t>(DeviceId::I2C_BUS)];
    if (s.ok() && h == HealthState::DEGRADED) {
      s = Status(Err::COMM_FAILURE, 0, "I2C bus degraded");
    } else if (s.ok() && h == HealthState::FAULT) {
      s = Status(Err::HARDWARE_FAULT, 0, "I2C bus fault");
    }
  }

  // SD
  {
    HealthState& sdH = dsHealth[static_cast<size_t>(DeviceId::SD)];
    Status& sdS = dsStatus[static_cast<size_t>(DeviceId::SD)];
    if (!_appSettings.enableSd) {
      sdH = HealthState::OK;
      sdS = Ok();
    } else if (!_impl->sdLogger.isMounted()) {
      sdH = HealthState::DEGRADED;
      sdS = Status(Err::COMM_FAILURE, 0, "SD not mounted");
    } else {
      const bool needDaily = tickSettings.logDailyEnabled;
      const bool needAll = tickSettings.logAllEnabled;
      const bool dailyOk = _impl->sdLogger.dailyOk();
      const bool allOk = _impl->sdLogger.allOk();
      const uint32_t droppedCount = _impl->sdLogger.droppedCount();
      const uint32_t eventDroppedCount = _impl->sdLogger.eventDroppedCount();
      const uint32_t sampleWriteFailureCount = _impl->sdLogger.sampleWriteFailureCount();
      const uint32_t eventWriteFailureCount = _impl->sdLogger.eventWriteFailureCount();
      const uint32_t budgetExceededCount = _impl->sdLogger.budgetExceededCount();
      const bool droppedIncreased = droppedCount > _impl->sdIssueLastDroppedCount;
      const bool eventDroppedIncreased = eventDroppedCount > _impl->sdIssueLastEventDroppedCount;
      const bool sampleWriteFailureIncreased =
          sampleWriteFailureCount > _impl->sdIssueLastSampleWriteFailureCount;
      const bool eventWriteFailureIncreased =
          eventWriteFailureCount > _impl->sdIssueLastEventWriteFailureCount;
      const bool budgetExceededIncreased = budgetExceededCount > _impl->sdIssueLastBudgetExceededCount;
      const bool issueIncreased = droppedIncreased || eventDroppedIncreased ||
                                  sampleWriteFailureIncreased || eventWriteFailureIncreased ||
                                  budgetExceededIncreased;
      if (issueIncreased) {
        _impl->sdIssueLastMs = nowMs;
      }
      _impl->sdIssueLastDroppedCount = droppedCount;
      _impl->sdIssueLastEventDroppedCount = eventDroppedCount;
      _impl->sdIssueLastSampleWriteFailureCount = sampleWriteFailureCount;
      _impl->sdIssueLastEventWriteFailureCount = eventWriteFailureCount;
      _impl->sdIssueLastBudgetExceededCount = budgetExceededCount;
      const bool issueRecent =
          (_impl->sdIssueLastMs != 0U) &&
          (static_cast<int32_t>(nowMs - _impl->sdIssueLastMs) >= 0) &&
          (static_cast<uint32_t>(nowMs - _impl->sdIssueLastMs) <=
           tickSettings.commandQueueDegradedWindowMs);
      const bool enqueueFailed = !_impl->lastLogEnqueueStatus.ok();
      const bool loggerHasError = !_impl->sdLogger.lastError().ok();
      if ((needDaily && !dailyOk) || (needAll && !allOk) || enqueueFailed ||
          loggerHasError || issueRecent) {
        sdH = HealthState::DEGRADED;
        if (enqueueFailed) {
          sdS = _impl->lastLogEnqueueStatus;
        } else if (loggerHasError) {
          sdS = _impl->sdLogger.lastError();
        } else if ((needDaily && !dailyOk) || (needAll && !allOk)) {
          sdS = Status(Err::COMM_FAILURE, 0, "SD logging stream degraded");
        } else if (droppedIncreased || eventDroppedIncreased) {
          uint64_t totalDroppedRaw = static_cast<uint64_t>(droppedCount) +
                                     static_cast<uint64_t>(eventDroppedCount);
          if (totalDroppedRaw > 0x7FFFFFFFULL) {
            totalDroppedRaw = 0x7FFFFFFFULL;
          }
          const int32_t totalDropped = static_cast<int32_t>(totalDroppedRaw);
          sdS = Status(Err::COMM_FAILURE, totalDropped, "SD logger dropped records");
        } else if (sampleWriteFailureIncreased || eventWriteFailureIncreased) {
          uint64_t totalWriteFailuresRaw = static_cast<uint64_t>(sampleWriteFailureCount) +
                                           static_cast<uint64_t>(eventWriteFailureCount);
          if (totalWriteFailuresRaw > 0x7FFFFFFFULL) {
            totalWriteFailuresRaw = 0x7FFFFFFFULL;
          }
          const int32_t totalWriteFailures = static_cast<int32_t>(totalWriteFailuresRaw);
          sdS = Status(Err::COMM_FAILURE, totalWriteFailures, "SD logger write failures");
        } else if (budgetExceededIncreased) {
          sdS = Status(Err::TIMEOUT, static_cast<int32_t>(budgetExceededCount),
                       "SD logger budget exceeded");
        } else {
          sdS = Status(Err::COMM_FAILURE, 0, "SD logger recovering");
        }
      } else {
        sdH = HealthState::OK;
        sdS = Ok();
      }
    }
  }

  // Outputs
  {
    HealthState& oH = dsHealth[static_cast<size_t>(DeviceId::OUTPUTS)];
    Status& oS = dsStatus[static_cast<size_t>(DeviceId::OUTPUTS)];
    oS = _impl->outputsLastStatus;
    if (!_impl->outputsLastStatus.ok()) {
      oH = HealthState::FAULT;
    } else if (tickSettings.outputsEnabled &&
               tickSettings.outputValveChannel != RuntimeSettings::OUTPUT_CHANNEL_DISABLED) {
      uint64_t staleWindow = static_cast<uint64_t>(tickSettings.sampleIntervalMs) * 2ULL;
      if (staleWindow > 0xFFFFFFFFULL) {
        staleWindow = 0xFFFFFFFFULL;
      }
      uint32_t staleWindowMs = static_cast<uint32_t>(staleWindow);
      if (staleWindowMs < tickSettings.outputDataStaleMinMs) {
        staleWindowMs = tickSettings.outputDataStaleMinMs;
      }
      const OutputSource source = static_cast<OutputSource>(tickSettings.outputSource);
      const bool envSource = (source == OutputSource::TEMP || source == OutputSource::RH);
      const uint32_t lastDataMs = envSource ? _impl->lastValidEnvMs : _impl->lastValidCo2Ms;
      if (lastDataMs == 0 ||
          static_cast<int32_t>(nowMs - lastDataMs) > static_cast<int32_t>(staleWindowMs)) {
        oH = HealthState::DEGRADED;
        const char* staleMsg = "LiDAR data stale";
        if (source == OutputSource::TEMP) {
          staleMsg = "Temp data stale";
        } else if (source == OutputSource::RH) {
          staleMsg = "Humidity data stale";
        }
        oS = Status(Err::COMM_FAILURE, 0, staleMsg);
      } else {
        oH = HealthState::OK;
        oS = Ok();
      }
    } else {
      oH = HealthState::OK;
      oS = Ok();
    }
  }

  // WiFi
  {
    HealthState& wH = dsHealth[static_cast<size_t>(DeviceId::WIFI)];
    Status& wS = dsStatus[static_cast<size_t>(DeviceId::WIFI)];
    wS = _impl->wifiLastStatus;
    if (_appSettings.enableWeb) {
      if (_impl->apEnabled && !_impl->web.isApRunning()) {
        wH = HealthState::DEGRADED;
        if (wS.ok()) {
          wS = Status(Err::COMM_FAILURE, 0, "AP requested but not running");
        }
      } else {
        wH = HealthState::OK;
        wS = Ok();
      }
    } else {
      wH = HealthState::OK;
      wS = Status(Err::NOT_INITIALIZED, 0, "web disabled");
    }
  }

  // Web / command queue
  const size_t cmdDepth = _impl->queueDepth();
  const uint32_t cmdOverflowCount = _impl->queueOverflowCount();
  const uint32_t cmdLastOverflowMs = _impl->queueLastOverflowMs();
  const bool cmdHealthDegraded = isCommandQueueDegraded(nowMs,
                                                        cmdLastOverflowMs,
                                                        cmdDepth,
                                                        tickSettings.commandQueueDegradedWindowMs,
                                                        tickSettings.commandQueueDegradedDepthThreshold);
  {
    HealthState& webH = dsHealth[static_cast<size_t>(DeviceId::WEB)];
    Status& webS = dsStatus[static_cast<size_t>(DeviceId::WEB)];
    if (cmdHealthDegraded) {
      webH = HealthState::DEGRADED;
      webS = Status(Err::RESOURCE_BUSY, static_cast<int32_t>(cmdOverflowCount),
                    "command queue degraded");
    } else if (!_impl->lastCommandStatus.ok()) {
      webH = HealthState::DEGRADED;
      webS = _impl->lastCommandStatus;
    }
  }

  // LEDs, Button
  dsHealth[static_cast<size_t>(DeviceId::LEDS)] =
      _impl->ledOk ? HealthState::OK : HealthState::DEGRADED;
  dsStatus[static_cast<size_t>(DeviceId::LEDS)] =
      _impl->ledOk ? Ok() : Status(Err::HARDWARE_FAULT, 0, "LED init failed");
  dsHealth[static_cast<size_t>(DeviceId::BUTTON)] =
      _impl->buttonEnabled ? HealthState::OK : HealthState::DEGRADED;
  dsStatus[static_cast<size_t>(DeviceId::BUTTON)] = _impl->buttonLastStatus;

  // --- Apply all device updates under one lock ---
  size_t statusCount = 0;
  bool haveStatusSnapshot = false;
  if (_impl->lockState()) {
    for (size_t i = 0; i < DEVICE_COUNT; ++i) {
      // Only update devices that were evaluated above (skip SYSTEM ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â set later).
      if (static_cast<DeviceId>(i) == DeviceId::SYSTEM) continue;
      // Skip RTC/ENV/CO2 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â they are updated during sample acquisition.
      if (static_cast<DeviceId>(i) == DeviceId::RTC) continue;
      if (static_cast<DeviceId>(i) == DeviceId::ENV) continue;
      if (static_cast<DeviceId>(i) == DeviceId::LIDAR) continue;
      if (dsHealth[i] == HealthState::UNKNOWN && dsStatus[i].ok()) continue;
      updateDeviceStatus(_deviceStatus[i], dsHealth[i], dsStatus[i], nowMs);
    }
    for (size_t i = 0; i < DEVICE_COUNT; ++i) {
      _impl->statusScratch[i] = _deviceStatus[i];
    }
    statusCount = DEVICE_COUNT;
    haveStatusSnapshot = true;
    _impl->unlockState();
  }

  const HealthState sysHealth = haveStatusSnapshot
                                    ? worstHealth(_impl->statusScratch, statusCount, _config)
                                    : HealthState::DEGRADED;
  tickPhaseStatusUs += (SystemClock::nowUs() - statusPhaseStartUs);
  StatusLedAdapter::HealthState healthState = StatusLedAdapter::HealthState::INIT;
  if (!_impl->initComplete) {
    if (uptimeMs >= tickSettings.ledHealthInitMs) {
      _impl->initComplete = true;
      _impl->ledHealthDebounce = StatusLedAdapter::HealthDebounceState{};
    }
  } else {
    const StatusLedAdapter::HealthState target = toLedHealth(sysHealth);
    healthState = StatusLedAdapter::debounceHealth(
        target, nowMs, tickSettings.ledHealthDebounceMs, _impl->ledHealthDebounce);
  }
  const uint32_t ledPhaseStartUs = SystemClock::nowUs();
  _impl->leds.setWifiState(wifiState);
  _impl->leds.setHealthState(healthState);
  _impl->leds.tick(nowMs);
  tickPhaseLedUs += (SystemClock::nowUs() - ledPhaseStartUs);

  SystemStatus nextStatus{};
  nextStatus.health = sysHealth;
  nextStatus.lastSampleMs = _impl->lastSampleMs;
  nextStatus.sampleCount = _impl->sampleCount;
  nextStatus.sdMounted = _impl->sdLogger.isMounted();
  nextStatus.sdInfoValid = _impl->sdLogger.infoValid();
  nextStatus.sdUsageValid = _impl->sdLogger.usageValid();
  nextStatus.sdFsCapacityBytes = _impl->sdLogger.fsCapacityBytes();
  nextStatus.sdFsUsedBytes = _impl->sdLogger.fsUsedBytes();
  nextStatus.sdFsFreeBytes = _impl->sdLogger.fsFreeBytes();
  nextStatus.sdCardCapacityBytes = _impl->sdLogger.cardCapacityBytes();
  nextStatus.sdInfoLastUpdateMs = _impl->sdLogger.infoLastUpdateMs();
  nextStatus.sdInfoAgeMs = (nextStatus.sdInfoLastUpdateMs == 0U)
                               ? 0U
                               : (nowMs - nextStatus.sdInfoLastUpdateMs);
  nextStatus.sdFsType = _impl->sdLogger.fsTypeCode();
  nextStatus.sdCardType = _impl->sdLogger.cardTypeCode();
  nextStatus.logDailyOk = tickSettings.logDailyEnabled ? _impl->sdLogger.dailyOk() : true;
  nextStatus.logAllOk = tickSettings.logAllEnabled ? _impl->sdLogger.allOk() : true;
  nextStatus.wifiApRunning = _impl->web.isApRunning();
  nextStatus.wifiRssiDbm = _impl->web.averageStationRssiDbm();
  nextStatus.wifiChannel = _impl->web.apChannel();
  nextStatus.wifiStationCount = _impl->web.stationCount();
  nextStatus.webClientCount = _impl->web.webClientCount();
  nextStatus.logDroppedCount = _impl->sdLogger.droppedCount();
  nextStatus.logQueueDepth = _impl->sdLogger.queueDepth();
  nextStatus.logQueueCapacity = _impl->sdLogger.queueCapacity();
  nextStatus.logQueueUsingPsram = _impl->sdLogger.queueUsingPsram();
  nextStatus.logLastWriteMs = _impl->sdLogger.lastWriteMs();
  nextStatus.logLastWriteAgeMs = (nextStatus.logLastWriteMs == 0)
                                     ? 0
                                     : (nowMs - nextStatus.logLastWriteMs);
  nextStatus.logLastErrorMs = _impl->sdLogger.lastErrorMs();
  nextStatus.logLastErrorMsg = _impl->sdLogger.lastError().msg;
  nextStatus.logLastErrorDetail = _impl->sdLogger.lastError().detail;
  nextStatus.logIoBudgetMs = _impl->sdLogger.ioBudgetMs();
  nextStatus.logLastTickElapsedMs = _impl->sdLogger.lastTickElapsedMs();
  nextStatus.logBudgetExceededCount = _impl->sdLogger.budgetExceededCount();
  const I2cBusMetrics i2cMetrics = _impl->i2cOrchestrator.busMetrics();
  nextStatus.i2cErrorCount = i2cMetrics.errorCount;
  nextStatus.i2cConsecutiveErrors = i2cMetrics.consecutiveErrors;
  nextStatus.i2cRecoveryCount = i2cMetrics.recoveryCount;
  nextStatus.i2cLastErrorMs = i2cMetrics.lastErrorMs;
  nextStatus.i2cLastRecoveryMs = i2cMetrics.lastRecoveryMs;
  nextStatus.i2cStuckSdaCount = i2cMetrics.stuckSdaCount;
  nextStatus.i2cStuckBusFastFailCount = i2cMetrics.stuckBusFastFailCount;
  nextStatus.i2cRequestOverflowCount = i2cMetrics.requestOverflowCount;
  nextStatus.i2cResultDroppedCount = i2cMetrics.resultDroppedCount;
  nextStatus.i2cStaleResultCount = i2cMetrics.staleResultCount;
  nextStatus.i2cSlowOpCount = i2cMetrics.slowOpCount;
  nextStatus.i2cRecentSlowOpCount = i2cMetrics.recentSlowOpCount;
  nextStatus.i2cRequestQueueDepth = i2cMetrics.requestQueueDepth;
  nextStatus.i2cResultQueueDepth = i2cMetrics.resultQueueDepth;
  nextStatus.i2cMaxDurationUs = i2cMetrics.maxDurationUs;
  nextStatus.i2cRollingMaxDurationUs = i2cMetrics.rollingMaxDurationUs;
  nextStatus.i2cMeanDurationUs =
      (i2cMetrics.transactionCount == 0) ? 0 : static_cast<uint32_t>(i2cMetrics.totalDurationUs / i2cMetrics.transactionCount);
  nextStatus.i2cTaskAliveMs = i2cMetrics.taskAliveMs;
  nextStatus.i2cTaskAliveAgeMs = (i2cMetrics.taskAliveMs == 0) ? 0 : (nowMs - i2cMetrics.taskAliveMs);
  nextStatus.i2cPowerCycleAttempts = i2cMetrics.powerCycleAttempts;
  nextStatus.i2cLastPowerCycleMs = i2cMetrics.lastPowerCycleMs;
  nextStatus.i2cPowerCycleConfigured = i2cMetrics.powerCycleConfigured;
  nextStatus.i2cPowerCycleLastCode = static_cast<uint16_t>(i2cMetrics.lastPowerCycleStatus.code);
  nextStatus.i2cPowerCycleLastDetail = i2cMetrics.lastPowerCycleStatus.detail;
  nextStatus.i2cPowerCycleLastMsg = i2cMetrics.lastPowerCycleStatus.msg;
  nextStatus.i2cLastRecoveryStage = static_cast<uint8_t>(i2cMetrics.lastRecoveryStage);
  nextStatus.i2cBackendName = i2cMetrics.backendName;
  nextStatus.i2cDeterministicTimeout = i2cMetrics.deterministicTimeout;
  nextStatus.i2cRtcConsecutiveFailures = _impl->rtc.consecutiveFailures();
  nextStatus.i2cEnvConsecutiveFailures = _impl->env.consecutiveFailures();
  nextStatus.commandQueueDepth = cmdDepth;
  nextStatus.commandQueueCapacity = HardwareSettings::COMMAND_QUEUE_CAPACITY;
  nextStatus.commandQueueOverflowCount = cmdOverflowCount;
  nextStatus.commandQueueLastOverflowMs = cmdLastOverflowMs;
#ifdef ARDUINO
  // Heap / stack metrics are refreshed in processDeferred() at 1 Hz
  // to keep the expensive heap_caps_get_largest_free_block and
  // uxTaskGetStackHighWaterMark calls out of cooperative tick timing.
  // I2C task stack is already obtained from i2cMetrics in the I2C task.
  _impl->cachedI2cStackFreeBytes = i2cMetrics.taskStackFreeBytes;
  nextStatus.heapFreeBytes = _impl->cachedHeapFreeBytes;
  nextStatus.heapMinFreeBytes = _impl->cachedHeapMinFreeBytes;
  nextStatus.heapTotalBytes = _impl->cachedHeapTotalBytes;
  nextStatus.heapMaxAllocBytes = _impl->cachedHeapMaxAllocBytes;
  nextStatus.mainTaskStackFreeBytes = _impl->cachedMainStackFreeBytes;
#endif
  nextStatus.i2cTaskStackFreeBytes = _impl->cachedI2cStackFreeBytes;

  // Output state
  nextStatus.outputPresentMask = outputPresentMask;
  nextStatus.outputChannelMask = outputMask;
  nextStatus.outputOverrideMode = static_cast<uint8_t>(_impl->outputs.overrideMode());
  nextStatus.outputsEnabled = tickSettings.outputsEnabled;
  nextStatus.outputLogicState = _impl->outputs.valveState();
  nextStatus.outputValveChannel = _impl->outputs.valveChannel();
  nextStatus.outputValvePoweredCloses = _impl->outputs.valvePoweredCloses();
  nextStatus.outputValveState = _impl->outputs.valveState();
  nextStatus.outputFanChannel = _impl->outputs.fanChannel();
  nextStatus.outputFanState = _impl->outputs.fanState();
  nextStatus.outputFanPwmPercent = _impl->outputs.fanAppliedPercent();
  nextStatus.outputTestActiveMask = _impl->outputs.testOverrideEnabledMask();
  nextStatus.outputTestStateMask = _impl->outputs.testOverrideStateMask();
  nextStatus.outputLastChangeMs = _impl->outputs.lastChangeMs();
  nextStatus.fwVersion = VERSION;

  nextStatus.uptimeMs = uptimeMs;
  nextStatus.rtcTimeActive = false;
  nextStatus.timeSource = "uptime";
  nextStatus.tickLastDurationUs = _impl->tickDurationUs;
  nextStatus.tickMaxDurationUs = _impl->tickMaxDurationUs;
  nextStatus.tickMeanDurationUs = (_impl->tickCount == 0) ? 0 : static_cast<uint32_t>(_impl->tickTotalDurationUs / _impl->tickCount);
  nextStatus.tickSlowCount = _impl->tickSlowCount;
  nextStatus.tickLastSlowMs = _impl->tickLastSlowMs;
  nextStatus.tickPhaseUsCmd = tickPhaseCmdUs;
  nextStatus.tickPhaseUsCo2 = tickPhaseCo2Us;
  nextStatus.tickPhaseUsI2c = tickPhaseI2cUs;
  nextStatus.tickPhaseUsSd = tickPhaseSdUs;
  nextStatus.tickPhaseUsIo = tickPhaseIoUs;
  nextStatus.tickPhaseUsStatus = tickPhaseStatusUs;
  nextStatus.tickPhaseUsLed = tickPhaseLedUs;
  nextStatus.tickMaxAtMs = _impl->tickMaxAtMs;
  nextStatus.tickMaxPhaseUsCmd = _impl->tickMaxPhaseUsCmd;
  nextStatus.tickMaxPhaseUsCo2 = _impl->tickMaxPhaseUsCo2;
  nextStatus.tickMaxPhaseUsI2c = _impl->tickMaxPhaseUsI2c;
  nextStatus.tickMaxPhaseUsSd = _impl->tickMaxPhaseUsSd;
  nextStatus.tickMaxPhaseUsIo = _impl->tickMaxPhaseUsIo;
  nextStatus.tickMaxPhaseUsStatus = _impl->tickMaxPhaseUsStatus;
  nextStatus.tickMaxPhaseUsLed = _impl->tickMaxPhaseUsLed;
  nextStatus.tickSlowDomCmdCount = _impl->tickSlowDomCmdCount;
  nextStatus.tickSlowDomCo2Count = _impl->tickSlowDomCo2Count;
  nextStatus.tickSlowDomI2cCount = _impl->tickSlowDomI2cCount;
  nextStatus.tickSlowDomSdCount = _impl->tickSlowDomSdCount;
  nextStatus.tickSlowDomIoCount = _impl->tickSlowDomIoCount;
  nextStatus.tickSlowDomStatusCount = _impl->tickSlowDomStatusCount;
  nextStatus.tickSlowDomLedCount = _impl->tickSlowDomLedCount;
  nextStatus.tickSlowDomOtherCount = _impl->tickSlowDomOtherCount;
  nextStatus.webThrottled = _impl->webThrottled;
  nextStatus.webSkipCount = _impl->webSkipCount;
  nextStatus.webOverrunBurst = _impl->webOverrunBurst;
  nextStatus.lidarFramesParsed = _impl->lidar.framesParsed();
  nextStatus.lidarChecksumErrors = _impl->lidar.checksumErrors();
  nextStatus.lidarSyncLossCount = _impl->lidar.syncLossCount();
  nextStatus.lidarStats = _impl->lidar.statsSnapshot();
  LidarMeasurement latestLidar{};
  if (_impl->lidar.latestMeasurement(latestLidar)) {
    nextStatus.lidar = latestLidar;
    nextStatus.lidarLastFrameMs = latestLidar.capturedMs;
    nextStatus.lidarFrameAgeMs = nowMs - latestLidar.capturedMs;
  } else {
    nextStatus.lidar = LidarMeasurement{};
    nextStatus.lidarLastFrameMs = 0U;
    nextStatus.lidarFrameAgeMs = 0U;
  }
  nextStatus.lastStatus = Ok();
  nextStatus.logLastErrorAgeMs = (nextStatus.logLastErrorMs == 0)
                                     ? 0
                                     : (nowMs - nextStatus.logLastErrorMs);
  nextStatus.logEventDroppedCount = _impl->sdLogger.eventDroppedCount();
  nextStatus.logEventQueueDepth = _impl->sdLogger.eventQueueDepth();
  nextStatus.logEventQueueCapacity = _impl->sdLogger.eventQueueCapacity();
  nextStatus.logEventQueueUsingPsram = _impl->sdLogger.eventQueueUsingPsram();
  nextStatus.sampleHistoryDepth = _impl->samples.size();
  nextStatus.sampleHistoryCapacity = _impl->samples.capacity();
  nextStatus.sampleHistoryUsingPsram = _impl->samples.usingPsram();
  nextStatus.eventHistoryDepth = _impl->events.size();
  nextStatus.eventHistoryCapacity = _impl->events.capacity();
  nextStatus.eventHistoryUsingPsram = _impl->events.usingPsram();
  nextStatus.webScratchUsingPsram = _impl->web.webScratchUsingPsram();
  nextStatus.webGraphScratchCapacity = static_cast<uint32_t>(_impl->web.graphScratchCapacity());
  nextStatus.webEventScratchCapacity = static_cast<uint32_t>(_impl->web.eventScratchCapacity());
  nextStatus.logSessionActive = _impl->sdLogger.sessionActive();
  nextStatus.logSessionDir = _impl->sdLogger.sessionDirPath();
  nextStatus.logCurrentSampleFile = _impl->sdLogger.currentAllFilePath();
  nextStatus.logCurrentEventFile = _impl->sdLogger.currentEventFilePath();
  nextStatus.logCurrentSampleFilePart = _impl->sdLogger.currentAllFilePart();
  nextStatus.logCurrentEventFilePart = _impl->sdLogger.currentEventFilePart();
  nextStatus.logSampleCurrentDataLine = _impl->sdLogger.sampleLinesCurrentFile();
  nextStatus.logEventCurrentDataLine = _impl->sdLogger.eventLinesCurrentFile();
  nextStatus.logSampleWrittenTotal = _impl->sdLogger.sampleLinesTotal();
  nextStatus.logEventWrittenTotal = _impl->sdLogger.eventLinesTotal();
  nextStatus.logSampleWriteSuccessCount = _impl->sdLogger.sampleWriteSuccessCount();
  nextStatus.logSampleWriteFailureCount = _impl->sdLogger.sampleWriteFailureCount();
  nextStatus.logEventWriteSuccessCount = _impl->sdLogger.eventWriteSuccessCount();
  nextStatus.logEventWriteFailureCount = _impl->sdLogger.eventWriteFailureCount();
  nextStatus.logSampleRotateCount = _impl->sdLogger.sampleRotateCount();
  nextStatus.logEventRotateCount = _impl->sdLogger.eventRotateCount();
  nextStatus.psramAvailable = _impl->psramAvailableAtBoot;
  nextStatus.psramTotalBytes = _impl->cachedPsramTotalBytes;
  nextStatus.psramFreeBytes = _impl->cachedPsramFreeBytes;
  nextStatus.psramMinFreeBytes = _impl->cachedPsramMinFreeBytes;
  nextStatus.psramMaxAllocBytes = _impl->cachedPsramMaxAllocBytes;

  if (latest != nullptr && latest->tsLocal[0] != '\0') {
    nextStatus.rtcTimeActive = true;
    nextStatus.timeSource = "rtc";
  }

  const bool displayLoggingEnabled = _appSettings.enableSd &&
                                     (tickSettings.logDailyEnabled || tickSettings.logAllEnabled);
  const bool displaySdMounted = _impl->sdLogger.isMounted();
  bool displayLoggingHealthy = false;
  if (displayLoggingEnabled && displaySdMounted) {
    displayLoggingHealthy = nextStatus.logDailyOk &&
                            nextStatus.logAllOk &&
                            _impl->lastLogEnqueueStatus.ok() &&
                            _impl->sdLogger.lastError().ok();
  }
  _impl->i2cOrchestrator.setDisplaySystemSnapshot(displayLoggingEnabled,
                                                  displaySdMounted,
                                                  displayLoggingHealthy,
                                                  nextStatus.logSampleWrittenTotal,
                                                  sysHealth);

  if (i2cMetrics.recoveryCount > _impl->lastI2cRecoveryCount) {
    const I2cRecoveryStage stage = i2cMetrics.lastRecoveryStage;
    const char* stageMsg = "i2c recovery";
    if (stage == I2cRecoveryStage::RESET) {
      stageMsg = "i2c recovery reset";
    } else if (stage == I2cRecoveryStage::SCL_PULSE) {
      stageMsg = "i2c recovery scl pulse";
    } else if (stage == I2cRecoveryStage::POWER_CYCLE) {
      stageMsg = "i2c recovery power cycle";
    }
    pushEvent(nowMs, EVENT_I2C_RECOVERY, stageMsg);
    _impl->lastI2cRecoveryCount = i2cMetrics.recoveryCount;
    _impl->lastI2cRecoveryStage = stage;
  }

  const uint32_t dropped = _impl->sdLogger.droppedCount();
  if (dropped > _impl->lastLoggedDropCount) {
    const bool dropEventDue = (_impl->lastLoggedDropEventMs == 0U) ||
                              (static_cast<int32_t>(nowMs - _impl->lastLoggedDropEventMs) >=
                               static_cast<int32_t>(LOGGER_EVENT_THROTTLE_MS));
    if (dropEventDue) {
      pushEvent(nowMs, EVENT_LOGGER_DROP, "logger dropped record");
      _impl->lastLoggedDropEventMs = nowMs;
    }
    _impl->lastLoggedDropCount = dropped;
  }

  const Status loggerError = _impl->sdLogger.lastError();
  const bool loggerErrorChanged = (loggerError.code != _impl->lastLoggedLoggerErrorCode) ||
                                  (loggerError.detail != _impl->lastLoggedLoggerErrorDetail);
  const bool loggerErrorDue = (_impl->lastLoggedLoggerErrorMs == 0U) ||
                              (static_cast<int32_t>(nowMs - _impl->lastLoggedLoggerErrorMs) >=
                               static_cast<int32_t>(LOGGER_EVENT_THROTTLE_MS));
  if (!loggerError.ok() && loggerErrorChanged && loggerErrorDue) {
    pushEvent(nowMs, EVENT_LOGGER_ERROR, loggerError.msg);
    _impl->lastLoggedLoggerErrorMs = nowMs;
  }
  if (loggerErrorChanged) {
    _impl->lastLoggedLoggerErrorCode = loggerError.code;
    _impl->lastLoggedLoggerErrorDetail = loggerError.detail;
  }

  const uint32_t tickElapsedUs = SystemClock::nowUs() - tickStartUs;
  _impl->tickDurationUs = tickElapsedUs;
  if (tickPhaseCmdUs > _impl->tickMaxPhaseUsCmd) {
    _impl->tickMaxPhaseUsCmd = tickPhaseCmdUs;
  }
  if (tickPhaseCo2Us > _impl->tickMaxPhaseUsCo2) {
    _impl->tickMaxPhaseUsCo2 = tickPhaseCo2Us;
  }
  if (tickPhaseI2cUs > _impl->tickMaxPhaseUsI2c) {
    _impl->tickMaxPhaseUsI2c = tickPhaseI2cUs;
  }
  if (tickPhaseSdUs > _impl->tickMaxPhaseUsSd) {
    _impl->tickMaxPhaseUsSd = tickPhaseSdUs;
  }
  if (tickPhaseIoUs > _impl->tickMaxPhaseUsIo) {
    _impl->tickMaxPhaseUsIo = tickPhaseIoUs;
  }
  if (tickPhaseStatusUs > _impl->tickMaxPhaseUsStatus) {
    _impl->tickMaxPhaseUsStatus = tickPhaseStatusUs;
  }
  if (tickPhaseLedUs > _impl->tickMaxPhaseUsLed) {
    _impl->tickMaxPhaseUsLed = tickPhaseLedUs;
  }
  if (tickElapsedUs > _impl->tickMaxDurationUs) {
    _impl->tickMaxDurationUs = tickElapsedUs;
    _impl->tickMaxAtMs = nowMs;
  }
  _impl->tickTotalDurationUs += tickElapsedUs;
  _impl->tickCount++;
  if (tickElapsedUs > tickSettings.mainTickSlowThresholdUs) {
    _impl->tickSlowCount++;
    _impl->tickLastSlowMs = nowMs;
    const uint64_t instrumentedUs64 =
        static_cast<uint64_t>(tickPhaseCmdUs) +
        static_cast<uint64_t>(tickPhaseCo2Us) +
        static_cast<uint64_t>(tickPhaseI2cUs) +
        static_cast<uint64_t>(tickPhaseSdUs) +
        static_cast<uint64_t>(tickPhaseIoUs) +
        static_cast<uint64_t>(tickPhaseStatusUs) +
        static_cast<uint64_t>(tickPhaseLedUs);
    const uint32_t instrumentedUs =
        (instrumentedUs64 > 0xFFFFFFFFULL) ? 0xFFFFFFFFUL : static_cast<uint32_t>(instrumentedUs64);
    const uint32_t otherUs = (tickElapsedUs > instrumentedUs)
                                 ? (tickElapsedUs - instrumentedUs)
                                 : 0U;
    enum class SlowDom : uint8_t {
      CMD = 0,
      CO2,
      I2C,
      SD,
      IO,
      STATUS,
      LED,
      OTHER
    };
    SlowDom dom = SlowDom::OTHER;
    uint32_t domUs = otherUs;
    if (tickPhaseCmdUs > domUs) {
      dom = SlowDom::CMD;
      domUs = tickPhaseCmdUs;
    }
    if (tickPhaseCo2Us > domUs) {
      dom = SlowDom::CO2;
      domUs = tickPhaseCo2Us;
    }
    if (tickPhaseI2cUs > domUs) {
      dom = SlowDom::I2C;
      domUs = tickPhaseI2cUs;
    }
    if (tickPhaseSdUs > domUs) {
      dom = SlowDom::SD;
      domUs = tickPhaseSdUs;
    }
    if (tickPhaseIoUs > domUs) {
      dom = SlowDom::IO;
      domUs = tickPhaseIoUs;
    }
    if (tickPhaseStatusUs > domUs) {
      dom = SlowDom::STATUS;
      domUs = tickPhaseStatusUs;
    }
    if (tickPhaseLedUs > domUs) {
      dom = SlowDom::LED;
      domUs = tickPhaseLedUs;
    }
    // Attribute a slow tick to a concrete phase only when that phase represents
    // a meaningful portion of the total tick; otherwise classify as OTHER.
    static constexpr uint32_t SLOW_DOM_MIN_SHARE_PERCENT = 20U;
    if (dom != SlowDom::OTHER) {
      const uint64_t domShareScaled = static_cast<uint64_t>(domUs) * 100ULL;
      const uint64_t minShareScaled =
          static_cast<uint64_t>(tickElapsedUs) * static_cast<uint64_t>(SLOW_DOM_MIN_SHARE_PERCENT);
      if (domShareScaled < minShareScaled) {
        dom = SlowDom::OTHER;
      }
    }
    switch (dom) {
      case SlowDom::CMD:
        _impl->tickSlowDomCmdCount++;
        break;
      case SlowDom::CO2:
        _impl->tickSlowDomCo2Count++;
        break;
      case SlowDom::I2C:
        _impl->tickSlowDomI2cCount++;
        break;
      case SlowDom::SD:
        _impl->tickSlowDomSdCount++;
        break;
      case SlowDom::IO:
        _impl->tickSlowDomIoCount++;
        break;
      case SlowDom::STATUS:
        _impl->tickSlowDomStatusCount++;
        break;
      case SlowDom::LED:
        _impl->tickSlowDomLedCount++;
        break;
      case SlowDom::OTHER:
      default:
        _impl->tickSlowDomOtherCount++;
        break;
    }
  }

  nextStatus.tickLastDurationUs = _impl->tickDurationUs;
  nextStatus.tickMaxDurationUs = _impl->tickMaxDurationUs;
  nextStatus.tickMeanDurationUs =
      (_impl->tickCount == 0) ? 0 : static_cast<uint32_t>(_impl->tickTotalDurationUs / _impl->tickCount);
  nextStatus.tickSlowCount = _impl->tickSlowCount;
  nextStatus.tickLastSlowMs = _impl->tickLastSlowMs;
  nextStatus.tickPhaseUsCmd = tickPhaseCmdUs;
  nextStatus.tickPhaseUsCo2 = tickPhaseCo2Us;
  nextStatus.tickPhaseUsI2c = tickPhaseI2cUs;
  nextStatus.tickPhaseUsSd = tickPhaseSdUs;
  nextStatus.tickPhaseUsIo = tickPhaseIoUs;
  nextStatus.tickPhaseUsStatus = tickPhaseStatusUs;
  nextStatus.tickPhaseUsLed = tickPhaseLedUs;
  nextStatus.tickMaxAtMs = _impl->tickMaxAtMs;
  nextStatus.tickMaxPhaseUsCmd = _impl->tickMaxPhaseUsCmd;
  nextStatus.tickMaxPhaseUsCo2 = _impl->tickMaxPhaseUsCo2;
  nextStatus.tickMaxPhaseUsI2c = _impl->tickMaxPhaseUsI2c;
  nextStatus.tickMaxPhaseUsSd = _impl->tickMaxPhaseUsSd;
  nextStatus.tickMaxPhaseUsIo = _impl->tickMaxPhaseUsIo;
  nextStatus.tickMaxPhaseUsStatus = _impl->tickMaxPhaseUsStatus;
  nextStatus.tickMaxPhaseUsLed = _impl->tickMaxPhaseUsLed;
  nextStatus.tickSlowDomCmdCount = _impl->tickSlowDomCmdCount;
  nextStatus.tickSlowDomCo2Count = _impl->tickSlowDomCo2Count;
  nextStatus.tickSlowDomI2cCount = _impl->tickSlowDomI2cCount;
  nextStatus.tickSlowDomSdCount = _impl->tickSlowDomSdCount;
  nextStatus.tickSlowDomIoCount = _impl->tickSlowDomIoCount;
  nextStatus.tickSlowDomStatusCount = _impl->tickSlowDomStatusCount;
  nextStatus.tickSlowDomLedCount = _impl->tickSlowDomLedCount;
  nextStatus.tickSlowDomOtherCount = _impl->tickSlowDomOtherCount;
  nextStatus.webThrottled = _impl->webThrottled;
  nextStatus.webSkipCount = _impl->webSkipCount;
  nextStatus.webOverrunBurst = _impl->webOverrunBurst;

  if (_impl->lockState()) {
    updateDeviceStatus(_deviceStatus[static_cast<size_t>(DeviceId::SYSTEM)], sysHealth, Ok(), nowMs);
    _systemStatus = nextStatus;
    _impl->unlockState();
  }
}

void TFLunaControl::processDeferred() {
  if (!_initialized || !_impl) {
    return;
  }

  // WiFi stop takes priority ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â cancel any pending start when also stopping.
  if (_impl->deferredApStop) {
    _impl->deferredApStop = false;
    _impl->deferredApStart = false;
    if (_impl->web.isApRunning()) {
      _impl->web.stopAp();
    }
  }

  if (_impl->deferredApStart) {
    _impl->deferredApStart = false;
    if (!_impl->web.isApRunning()) {
      const Status apStartSt = _impl->web.startAp(_impl->deferredApStartSettings);
      _impl->wifiLastStatus = apStartSt;
      if (apStartSt.ok()) {
        _impl->lastClientMs = _impl->lastNowMs;
        _impl->nextApStartRetryMs = 0;
      } else {
        _impl->nextApStartRetryMs =
            _impl->lastNowMs + _impl->deferredApStartSettings.apStartRetryBackoffMs;
      }
    }
  }

  if (_impl->nvsSavePending) {
    _impl->nvsSavePending = false;
    const Status saveSt = _impl->settingsStore.save(_impl->nvsSavePayload);
    if (!saveSt.ok()) {
      _impl->lastCommandStatus = saveSt;
      _impl->lastCommandErrorMs = _impl->lastNowMs;
    }
  }

  // Deferred SD mount/remount ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â _sd.end() and _sd.begin() block for
  // hundreds of milliseconds and must not run inside tick timing.
  _impl->sdLogger.processDeferred(_impl->lastNowMs);

  // Heap / stack metrics ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â refreshed at 1 Hz.  ESP.getMaxAllocHeap()
  // calls heap_caps_get_largest_free_block() which walks the entire free
  // block list (0.5-5+ ms depending on heap fragmentation).
  // uxTaskGetStackHighWaterMark() scans the stack region (0.1-0.5 ms).
  // Both are deferred here so the variable latency does not inflate
  // cooperative tick timing.
#ifdef ARDUINO
  {
    static constexpr uint32_t kHeapRefreshMs = 1000U;
    const uint32_t nowMs = _impl->lastNowMs;
    if (_impl->lastHeapRefreshMs == 0U ||
        static_cast<int32_t>(nowMs - _impl->lastHeapRefreshMs) >= static_cast<int32_t>(kHeapRefreshMs)) {
      _impl->lastHeapRefreshMs = nowMs;
      _impl->cachedHeapFreeBytes = ESP.getFreeHeap();
      _impl->cachedHeapMinFreeBytes = ESP.getMinFreeHeap();
      _impl->cachedHeapTotalBytes = ESP.getHeapSize();
      _impl->cachedHeapMaxAllocBytes = ESP.getMaxAllocHeap();
      _impl->cachedPsramTotalBytes = PsramSupport::totalBytes();
      _impl->cachedPsramFreeBytes = PsramSupport::freeBytes();
      _impl->cachedPsramMinFreeBytes = PsramSupport::minFreeBytes();
      _impl->cachedPsramMaxAllocBytes = PsramSupport::maxAllocBytes();
      _impl->cachedMainStackFreeBytes = uxTaskGetStackHighWaterMark(nullptr) * sizeof(StackType_t);
    }
  }
#endif

  // WS broadcast runs here because AsyncTCP's tcp_write / tcp_output use
  // tcpip_api_call() ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â a synchronous cross-thread call to the lwIP task.
  // When the lwIP thread is busy (WiFi events, TCP retransmissions,
  // incoming packets), the calling thread blocks for 100-340 ms.
  // Running this outside tick timing keeps the cooperative tick budget clean.
  if (_appSettings.enableWeb) {
    _impl->web.broadcastDeferred(_impl->lastNowMs);
  }
}

RuntimeSettings TFLunaControl::getSettings() const {
  if (!_impl) {
    return RuntimeSettings{};
  }
  RuntimeSettings out{};
  if (_impl->lockState()) {
    out = _settings;
    _impl->unlockState();
  }
  return out;
}

SystemStatus TFLunaControl::getSystemStatus() const {
  if (!_impl) {
    return SystemStatus{};
  }
  SystemStatus out{};
  if (_impl->lockState()) {
    out = _systemStatus;
    _impl->unlockState();
  }
  return out;
}

bool TFLunaControl::tryGetStatusSnapshot(SystemStatus& outStatus, Sample& outLatest, bool& outHasLatest) const {
  outHasLatest = false;
  if (!_impl) {
    return false;
  }
  if (!_impl->tryLockState()) {
    if (!_impl->lockState()) {
      return false;
    }
  }
  outStatus = _systemStatus;
  const Sample* latest = _impl->samples.latest();
  if (latest) {
    outLatest = *latest;
    outHasLatest = true;
  }
  _impl->unlockState();
  return true;
}

const DeviceStatus& TFLunaControl::getDeviceStatus(DeviceId id) const {
  const size_t idx = static_cast<size_t>(id);
  if (idx >= DEVICE_COUNT) {
    return _deviceStatus[0];
  }
  return _deviceStatus[idx];
}

bool TFLunaControl::tryGetSettingsSnapshot(RuntimeSettings& out) const {
  if (!_impl) {
    return false;
  }
  if (!_impl->tryLockState()) {
    // Use bounded wait fallback for settings snapshots to reduce transient
    // lock contention under concurrent web polling + settings updates.
    if (!_impl->lockState()) {
      return false;
    }
  }
  out = _settings;
  _impl->unlockState();
  return true;
}

bool TFLunaControl::tryCopyDeviceStatuses(DeviceStatus* out, size_t max, size_t& outCount) const {
  outCount = 0;
  if (!out || max == 0 || !_impl) {
    return false;
  }
  if (!_impl->tryLockState()) {
    if (!_impl->lockState()) {
      return false;
    }
  }
  const size_t count = (max < DEVICE_COUNT) ? max : DEVICE_COUNT;
  for (size_t i = 0; i < count; ++i) {
    out[i] = _deviceStatus[i];
  }
  outCount = count;
  _impl->unlockState();
  return true;
}

bool TFLunaControl::getLatestSample(Sample& out) const {
  if (!_impl) {
    return false;
  }
  if (!_impl->lockState()) {
    return false;
  }
  const Sample* latest = _impl->samples.latest();
  if (!latest) {
    _impl->unlockState();
    return false;
  }
  out = *latest;
  _impl->unlockState();
  return true;
}

size_t TFLunaControl::copySamples(Sample* out, size_t max, bool oldestFirst) const {
  if (!_impl) {
    return 0;
  }
  if (!_impl->lockState()) {
    return 0;
  }
  const size_t copied = _impl->samples.copy(out, max, oldestFirst);
  _impl->unlockState();
  return copied;
}

bool TFLunaControl::tryCopySamples(Sample* out, size_t max, bool oldestFirst, size_t& outCount) const {
  outCount = 0;
  if (!_impl) {
    return false;
  }
  if (!_impl->tryLockState()) {
    if (!_impl->lockState()) {
      return false;
    }
  }
  outCount = _impl->samples.copy(out, max, oldestFirst);
  _impl->unlockState();
  return true;
}

bool TFLunaControl::tryCopyEvents(Event* out, size_t max, bool oldestFirst, size_t& outCount) const {
  outCount = 0;
  if (!_impl || out == nullptr || max == 0) {
    return false;
  }
  if (!_impl->tryLockState()) {
    if (!_impl->lockState()) {
      return false;
    }
  }
  outCount = _impl->events.copy(out, max, oldestFirst);
  _impl->unlockState();
  return true;
}

Status TFLunaControl::updateSettings(const RuntimeSettings& settings, bool persist, const char* changeHint) {
  if (!_initialized) {
    return Status(Err::NOT_INITIALIZED, 0, "not initialized");
  }
  RuntimeSettings normalized = settings;
  normalized.i2cRtcAddress = 0x51;
  Status st = normalized.validate();
  if (!st.ok()) {
    return st;
  }

  RuntimeSettings previous = getSettings();
  const bool wifiCredChanged = (strncmp(previous.apSsid, normalized.apSsid, sizeof(previous.apSsid)) != 0) ||
                               (strncmp(previous.apPass, normalized.apPass, sizeof(previous.apPass)) != 0);
  const bool wifiEnabledChanged = (previous.wifiEnabled != normalized.wifiEnabled);

  if (!_impl->lockState()) {
    return Status(Err::RESOURCE_BUSY, 0, "state lock busy");
  }
  _settings = normalized;
  _impl->unlockState();

  _impl->sampleTimer.setInterval(normalized.sampleIntervalMs);
  _impl->sampleTimer.reset(_impl->lastNowMs);
  _impl->outputs.applySettings(normalized);
  _impl->sdLogger.applySettings(normalized);
  _impl->i2cTask.applySettings(normalized, _impl->lastNowMs);
  _impl->i2cOrchestrator.applySettings(normalized);
  _impl->lidar.applySettings(normalized, _impl->lastNowMs);

  if (persist && _appSettings.enableNvs) {
    // Defer NVS flash write to processDeferred() so it does not block tick().
    _impl->nvsSavePending = true;
    _impl->nvsSavePayload = normalized;
  }

  if (wifiEnabledChanged) {
    _impl->apEnabled = normalized.wifiEnabled;
  }

  if ((wifiCredChanged || wifiEnabledChanged) && _appSettings.enableWeb) {
    // Defer AP lifecycle to processDeferred() to avoid blocking tick().
    if (_impl->web.isApRunning() && !_impl->deferredApStop) {
      _impl->deferredApStop = true;
    }
    _impl->wifiLastStatus = Ok();
    _impl->apEnabled = normalized.wifiEnabled;
    if (_impl->apEnabled) {
      _impl->nextApStartRetryMs = _impl->lastNowMs + 250U;
    } else {
      _impl->nextApStartRetryMs = 0;
    }
  }

  char eventMsg[sizeof(Event::msg)] = {0};
  if (changeHint != nullptr && changeHint[0] != '\0') {
    snprintf(eventMsg, sizeof(eventMsg), "settings applied: %.45s", changeHint);
  } else {
    strncpy(eventMsg, "settings applied", sizeof(eventMsg) - 1);
    eventMsg[sizeof(eventMsg) - 1] = '\0';
  }
  pushEvent(_impl->lastNowMs, EVENT_SETTINGS_APPLIED, eventMsg);
  return Ok();
}

Status TFLunaControl::factoryResetSettings(bool persist) {
  RuntimeSettings defaults;
  defaults.restoreDefaults();
  applyDefaultSsid(defaults);
  const Status st = updateSettings(defaults, persist);
  if (st.ok()) {
    pushEvent(_impl->lastNowMs, EVENT_SETTINGS_RESET, "settings reset");
  } else {
    pushEvent(_impl->lastNowMs, EVENT_FACTORY_RESET_FAILED, st.msg);
  }
  return st;
}

Status TFLunaControl::setRtcTime(const RtcTime& time) {
  if (!_initialized) {
    return Status(Err::NOT_INITIALIZED, 0, "not initialized");
  }
  Status st = _impl->rtc.setTime(time, SystemClock::nowMs64());
  HealthState rtcHealth = st.ok() ? HealthState::OK : HealthState::DEGRADED;
  if (_impl->rtc.consecutiveFailures() >= (getSettings().i2cMaxConsecutiveFailures * 2U)) {
    rtcHealth = HealthState::FAULT;
  }
  if (_impl->lockState()) {
    updateDeviceStatus(_deviceStatus[static_cast<size_t>(DeviceId::RTC)],
                       rtcHealth, st, _impl->lastNowMs);
    _impl->unlockState();
  }
  return st;
}

static uint32_t currentMs() {
  return SystemClock::nowMs();
}

static bool isValidI2cAddress7Bit(uint8_t address) {
  return address >= 0x01U && address <= 0x7FU;
}

static bool isOutputChannelConfigured(const HardwareSettings& config, size_t index) {
  switch (index) {
    case 0:
      return config.mosfet1Pin >= 0;
    case 1:
      return config.mosfet2Pin >= 0;
    case 2:
      return config.relay1Pin >= 0;
    case 3:
      return config.relay2Pin >= 0;
    default:
      return false;
  }
}

Status TFLunaControl::remountSd() {
  if (!_initialized) {
    return Status(Err::NOT_INITIALIZED, 0, "not initialized");
  }
  return _impl->sdLogger.remount(_impl->lastNowMs);
}

Status TFLunaControl::enqueueApplySettings(const RuntimeSettings& settings,
                                        bool persist,
                                        const char* changeHint) {
  if (!_initialized || !_impl) {
    return Status(Err::NOT_INITIALIZED, 0, "not initialized");
  }
  const Status validation = settings.validate();
  if (!validation.ok()) {
    return validation;
  }

  AppCommand cmd;
  cmd.type = AppCommandType::APPLY_SETTINGS;
  cmd.settings = settings;
  cmd.persist = persist;
  if (changeHint != nullptr) {
    strncpy(cmd.settingsChangeHint, changeHint, sizeof(cmd.settingsChangeHint) - 1);
    cmd.settingsChangeHint[sizeof(cmd.settingsChangeHint) - 1] = '\0';
  } else {
    cmd.settingsChangeHint[0] = '\0';
  }

  if (!_impl->enqueueCommand(cmd, currentMs())) {
    return Status(Err::RESOURCE_BUSY, 0, "command queue full");
  }
  return Ok();
}

Status TFLunaControl::enqueueSetWifiApEnabled(bool enabled) {
  if (!_initialized || !_impl) {
    return Status(Err::NOT_INITIALIZED, 0, "not initialized");
  }

  AppCommand cmd;
  cmd.type = AppCommandType::SET_WIFI_AP_ENABLED;
  cmd.apEnabled = enabled;
  cmd.persist = false;

  if (!_impl->enqueueCommand(cmd, currentMs())) {
    return Status(Err::RESOURCE_BUSY, 0, "command queue full");
  }
  return Ok();
}

Status TFLunaControl::enqueueSetRtcTime(const RtcTime& time) {
  if (!_initialized || !_impl) {
    return Status(Err::NOT_INITIALIZED, 0, "not initialized");
  }

  AppCommand cmd;
  cmd.type = AppCommandType::SET_RTC_TIME;
  cmd.rtcTime = time;
  cmd.persist = false;

  if (!_impl->enqueueCommand(cmd, currentMs())) {
    return Status(Err::RESOURCE_BUSY, 0, "command queue full");
  }
  return Ok();
}

Status TFLunaControl::enqueueRemountSd() {
  if (!_initialized || !_impl) {
    return Status(Err::NOT_INITIALIZED, 0, "not initialized");
  }

  AppCommand cmd;
  cmd.type = AppCommandType::REMOUNT_SD;
  cmd.persist = false;

  if (!_impl->enqueueCommand(cmd, currentMs())) {
    return Status(Err::RESOURCE_BUSY, 0, "command queue full");
  }
  return Ok();
}

Status TFLunaControl::enqueueSetOutputOverride(OutputOverrideMode mode) {
  if (!_initialized || !_impl) {
    return Status(Err::NOT_INITIALIZED, 0, "not initialized");
  }

  AppCommand cmd;
  cmd.type = AppCommandType::SET_OUTPUT_OVERRIDE;
  cmd.outputOverride = mode;
  cmd.persist = false;

  if (!_impl->enqueueCommand(cmd, currentMs())) {
    return Status(Err::RESOURCE_BUSY, 0, "command queue full");
  }
  return Ok();
}

Status TFLunaControl::enqueueSetOutputChannelTest(size_t index, bool enabled, bool state) {
  if (!_initialized || !_impl) {
    return Status(Err::NOT_INITIALIZED, 0, "not initialized");
  }
  if (index >= HardwareSettings::OUTPUT_CHANNEL_COUNT) {
    return Status(Err::INVALID_CONFIG, 0, "output index out of range");
  }
  if (!isOutputChannelConfigured(_config, index)) {
    return Status(Err::NOT_INITIALIZED, 0, "output channel not configured");
  }

  AppCommand cmd;
  cmd.type = AppCommandType::SET_OUTPUT_CHANNEL_TEST;
  cmd.outputChannelIndex = static_cast<uint8_t>(index);
  cmd.outputChannelTestEnabled = enabled;
  cmd.outputChannelState = state;
  cmd.persist = false;

  if (!_impl->enqueueCommand(cmd, currentMs())) {
    return Status(Err::RESOURCE_BUSY, 0, "command queue full");
  }
  return Ok();
}

Status TFLunaControl::enqueueRecoverI2cBus() {
  if (!_initialized || !_impl) {
    return Status(Err::NOT_INITIALIZED, 0, "not initialized");
  }

  AppCommand cmd;
  cmd.type = AppCommandType::RECOVER_I2C;
  cmd.persist = false;

  if (!_impl->enqueueCommand(cmd, currentMs())) {
    return Status(Err::RESOURCE_BUSY, 0, "command queue full");
  }
  return Ok();
}

Status TFLunaControl::enqueueRecoverLidarSensor() {
  if (!_initialized || !_impl) {
    return Status(Err::NOT_INITIALIZED, 0, "not initialized");
  }

  AppCommand cmd;
  cmd.type = AppCommandType::RECOVER_LIDAR;
  cmd.persist = false;

  if (!_impl->enqueueCommand(cmd, currentMs())) {
    return Status(Err::RESOURCE_BUSY, 0, "command queue full");
  }
  return Ok();
}

Status TFLunaControl::enqueueProbeLidarSensor() {
  if (!_initialized || !_impl) {
    return Status(Err::NOT_INITIALIZED, 0, "not initialized");
  }

  AppCommand cmd;
  cmd.type = AppCommandType::PROBE_LIDAR;
  cmd.persist = false;

  if (!_impl->enqueueCommand(cmd, currentMs())) {
    return Status(Err::RESOURCE_BUSY, 0, "command queue full");
  }
  return Ok();
}

Status TFLunaControl::enqueueProbeSdCard() {
  if (!_initialized || !_impl) {
    return Status(Err::NOT_INITIALIZED, 0, "not initialized");
  }

  AppCommand cmd;
  cmd.type = AppCommandType::PROBE_SD;
  cmd.persist = false;

  if (!_impl->enqueueCommand(cmd, currentMs())) {
    return Status(Err::RESOURCE_BUSY, 0, "command queue full");
  }
  return Ok();
}

Status TFLunaControl::enqueueScanI2cBus() {
  if (!_initialized || !_impl) {
    return Status(Err::NOT_INITIALIZED, 0, "not initialized");
  }

  AppCommand cmd;
  cmd.type = AppCommandType::SCAN_I2C;
  cmd.persist = false;

  if (!_impl->enqueueCommand(cmd, currentMs())) {
    return Status(Err::RESOURCE_BUSY, 0, "command queue full");
  }
  return Ok();
}

Status TFLunaControl::enqueueI2cRawWrite(uint8_t address, const uint8_t* tx, uint8_t txLen) {
  if (!_initialized || !_impl) {
    return Status(Err::NOT_INITIALIZED, 0, "not initialized");
  }
  if (!isValidI2cAddress7Bit(address)) {
    return Status(Err::INVALID_CONFIG, 0, "I2C address out of range");
  }
  if (tx == nullptr || txLen == 0U || txLen > static_cast<uint8_t>(HardwareSettings::I2C_PAYLOAD_BYTES)) {
    return Status(Err::INVALID_CONFIG, 0, "I2C write payload invalid");
  }

  AppCommand cmd;
  cmd.type = AppCommandType::RAW_I2C;
  cmd.i2cOp = I2cOpType::WRITE;
  cmd.i2cAddress = address;
  cmd.i2cTxLen = txLen;
  memcpy(cmd.i2cTx, tx, txLen);
  cmd.i2cRxLen = 0;

  if (!_impl->enqueueCommand(cmd, currentMs())) {
    return Status(Err::RESOURCE_BUSY, 0, "command queue full");
  }
  return Ok();
}

Status TFLunaControl::enqueueI2cRawRead(uint8_t address, uint8_t rxLen) {
  if (!_initialized || !_impl) {
    return Status(Err::NOT_INITIALIZED, 0, "not initialized");
  }
  if (!isValidI2cAddress7Bit(address)) {
    return Status(Err::INVALID_CONFIG, 0, "I2C address out of range");
  }
  if (rxLen == 0U || rxLen > static_cast<uint8_t>(HardwareSettings::I2C_PAYLOAD_BYTES)) {
    return Status(Err::INVALID_CONFIG, 0, "I2C read length invalid");
  }

  AppCommand cmd;
  cmd.type = AppCommandType::RAW_I2C;
  cmd.i2cOp = I2cOpType::READ;
  cmd.i2cAddress = address;
  cmd.i2cTxLen = 0;
  cmd.i2cRxLen = rxLen;

  if (!_impl->enqueueCommand(cmd, currentMs())) {
    return Status(Err::RESOURCE_BUSY, 0, "command queue full");
  }
  return Ok();
}

Status TFLunaControl::enqueueI2cRawWriteRead(uint8_t address,
                                          const uint8_t* tx,
                                          uint8_t txLen,
                                          uint8_t rxLen) {
  if (!_initialized || !_impl) {
    return Status(Err::NOT_INITIALIZED, 0, "not initialized");
  }
  if (!isValidI2cAddress7Bit(address)) {
    return Status(Err::INVALID_CONFIG, 0, "I2C address out of range");
  }
  if (tx == nullptr || txLen == 0U || txLen > static_cast<uint8_t>(HardwareSettings::I2C_PAYLOAD_BYTES)) {
    return Status(Err::INVALID_CONFIG, 0, "I2C write_read tx invalid");
  }
  if (rxLen == 0U || rxLen > static_cast<uint8_t>(HardwareSettings::I2C_PAYLOAD_BYTES)) {
    return Status(Err::INVALID_CONFIG, 0, "I2C write_read rx invalid");
  }

  AppCommand cmd;
  cmd.type = AppCommandType::RAW_I2C;
  cmd.i2cOp = I2cOpType::WRITE_READ;
  cmd.i2cAddress = address;
  cmd.i2cTxLen = txLen;
  memcpy(cmd.i2cTx, tx, txLen);
  cmd.i2cRxLen = rxLen;

  if (!_impl->enqueueCommand(cmd, currentMs())) {
    return Status(Err::RESOURCE_BUSY, 0, "command queue full");
  }
  return Ok();
}

Status TFLunaControl::enqueueI2cProbeAddress(uint8_t address) {
  if (!_initialized || !_impl) {
    return Status(Err::NOT_INITIALIZED, 0, "not initialized");
  }
  if (!isValidI2cAddress7Bit(address)) {
    return Status(Err::INVALID_CONFIG, 0, "I2C address out of range");
  }

  AppCommand cmd;
  cmd.type = AppCommandType::RAW_I2C;
  cmd.i2cOp = I2cOpType::PROBE;
  cmd.i2cAddress = address;
  cmd.i2cTxLen = 0;
  cmd.i2cRxLen = 0;

  if (!_impl->enqueueCommand(cmd, currentMs())) {
    return Status(Err::RESOURCE_BUSY, 0, "command queue full");
  }
  return Ok();
}

bool TFLunaControl::tryGetI2cScanSnapshot(I2cScanSnapshot& out) const {
  if (!_initialized || !_impl) {
    return false;
  }
  if (!_impl->tryLockState()) {
    return false;
  }
  out = _impl->i2cOrchestrator.scanSnapshot();
  _impl->unlockState();
  return true;
}

bool TFLunaControl::tryGetI2cRawSnapshot(I2cRawSnapshot& out) const {
  if (!_initialized || !_impl) {
    return false;
  }
  if (!_impl->tryLockState()) {
    return false;
  }
  out = _impl->i2cOrchestrator.rawSnapshot();
  _impl->unlockState();
  return true;
}

bool TFLunaControl::tryGetRtcDebugSnapshot(RtcDebugSnapshot& out) const {
  if (!_initialized || !_impl) {
    return false;
  }
  if (!_impl->tryLockState()) {
    return false;
  }
  out = _impl->i2cTask.getRtcDebugSnapshot();
  _impl->unlockState();
  return true;
}

OutputOverrideMode TFLunaControl::getOutputOverrideMode() const {
  if (!_initialized || !_impl) {
    return OutputOverrideMode::AUTO;
  }
  return _impl->outputs.overrideMode();
}

bool TFLunaControl::tryGetOutputChannelState(size_t index, bool& outState) const {
  outState = false;
  if (!_initialized || !_impl || index >= HardwareSettings::OUTPUT_CHANNEL_COUNT) {
    return false;
  }
  if (!_impl->tryLockState()) {
    return false;
  }
  outState = _impl->outputs.channelState(index);
  _impl->unlockState();
  return true;
}

}  // namespace TFLunaControl

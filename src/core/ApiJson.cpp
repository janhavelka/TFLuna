#include "core/ApiJson.h"

#include <math.h>

#include "TFLunaControl/HardwareSettings.h"
#if __has_include("TFLunaControl/Version.h")
#include "TFLunaControl/Version.h"
#endif

#if __has_include("EE871/Version.h")
#include "EE871/Version.h"
#endif

#if __has_include("BME280/Version.h")
#include "BME280/Version.h"
#endif

#if __has_include("SHT3x/Version.h")
#include "SHT3x/Version.h"
#endif

#if __has_include("RV3032/Version.h")
#include "RV3032/Version.h"
#endif

#if __has_include("ssd1315/Version.h")
#include "ssd1315/Version.h"
#endif

#if __has_include("AsyncSD/Version.h")
#include "AsyncSD/Version.h"
#endif

#if __has_include("SystemChrono/Version.h")
#include "SystemChrono/Version.h"
#endif

#if __has_include("StatusLed/Version.h")
#include "StatusLed/Version.h"
#endif

#ifndef TFLUNACTRL_DEP_EE871_VERSION
#define TFLUNACTRL_DEP_EE871_VERSION "v0.3.0"
#endif

#ifndef TFLUNACTRL_DEP_BME280_VERSION
#define TFLUNACTRL_DEP_BME280_VERSION "v1.2.1"
#endif

#ifndef TFLUNACTRL_DEP_SHT3X_VERSION
#define TFLUNACTRL_DEP_SHT3X_VERSION "v1.4.0"
#endif

#ifndef TFLUNACTRL_DEP_RV3032_VERSION
#define TFLUNACTRL_DEP_RV3032_VERSION "v1.3.0"
#endif

#ifndef TFLUNACTRL_DEP_SSD1315_VERSION
#define TFLUNACTRL_DEP_SSD1315_VERSION "v1.1.0"
#endif

#ifndef TFLUNACTRL_DEP_ASYNCSD_VERSION
#define TFLUNACTRL_DEP_ASYNCSD_VERSION "v1.3.0"
#endif

#ifndef TFLUNACTRL_DEP_SYSTEMCHRONO_VERSION
#define TFLUNACTRL_DEP_SYSTEMCHRONO_VERSION "v1.2.0"
#endif

#ifndef TFLUNACTRL_DEP_STATUSLED_VERSION
#define TFLUNACTRL_DEP_STATUSLED_VERSION "v1.3.0"
#endif

#ifndef TFLUNACTRL_DEP_ARDUINOJSON_VERSION
#define TFLUNACTRL_DEP_ARDUINOJSON_VERSION "^6.21.3"
#endif

#ifndef TFLUNACTRL_DEP_ESPASYNCWEBSERVER_VERSION
#define TFLUNACTRL_DEP_ESPASYNCWEBSERVER_VERSION "v3.9.6"
#endif

#ifndef TFLUNACTRL_DEP_ASYNCTCP_VERSION
#define TFLUNACTRL_DEP_ASYNCTCP_VERSION "v3.4.10"
#endif

#ifndef TFLUNACTRL_STR_IMPL
#define TFLUNACTRL_STR_IMPL(x) #x
#define TFLUNACTRL_STR(x) TFLUNACTRL_STR_IMPL(x)
#endif

namespace TFLunaControl {

static void setFloatOrNull(JsonDocument& doc, const char* key, float value, bool valid) {
  if (valid && isfinite(value)) {
    doc[key] = value;
  } else {
    doc[key] = nullptr;
  }
}

static void setU16OrNull(JsonDocument& doc, const char* key, uint16_t value, bool valid) {
  if (valid) {
    doc[key] = value;
  } else {
    doc[key] = nullptr;
  }
}

static uint8_t rssiToPercent(int16_t dbm) {
  if (dbm >= -30) {
    return 100;
  }
  if (dbm <= -95) {
    return 0;
  }
  const int32_t shifted = static_cast<int32_t>(dbm) + 95;
  return static_cast<uint8_t>((shifted * 100) / 65);
}

static uint8_t sdUsagePercent(const SystemStatus& sys) {
  if (!sys.sdUsageValid || sys.sdFsCapacityBytes == 0) {
    return 0;
  }
  const uint64_t pct = (sys.sdFsUsedBytes * 100ULL) / sys.sdFsCapacityBytes;
  return static_cast<uint8_t>((pct > 100ULL) ? 100ULL : pct);
}

static const char* sdFsTypeToString(uint8_t code) {
  switch (code) {
    case 1:
      return "fat12";
    case 2:
      return "fat16";
    case 3:
      return "fat32";
    case 4:
      return "exfat";
    default:
      return "unknown";
  }
}

static const char* sdCardTypeToString(uint8_t code) {
  switch (code) {
    case 1:
      return "sd1";
    case 2:
      return "sd2";
    case 3:
      return "sdhc";
    default:
      return "unknown";
  }
}

static const char* mcuVariantString() {
#if defined(CONFIG_IDF_TARGET_ESP32S2)
  return "ESP32-S2";
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
  return "ESP32-S3";
#elif defined(CONFIG_IDF_TARGET_ESP32)
  return "ESP32";
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
  return "ESP32-C3";
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
  return "ESP32-C6";
#elif defined(CONFIG_IDF_TARGET_ESP32H2)
  return "ESP32-H2";
#else
  return "unknown";
#endif
}

static const char* libEe871Version() {
#if __has_include("EE871/Version.h")
  return EE871::VERSION;
#else
  return TFLUNACTRL_DEP_EE871_VERSION;
#endif
}

static const char* libBme280Version() {
#if __has_include("BME280/Version.h")
  return BME280::VERSION;
#else
  return TFLUNACTRL_DEP_BME280_VERSION;
#endif
}

static const char* libSht3xVersion() {
#if __has_include("SHT3x/Version.h")
  return SHT3x::VERSION;
#else
  return TFLUNACTRL_DEP_SHT3X_VERSION;
#endif
}

static const char* libRv3032Version() {
#if __has_include("RV3032/Version.h")
  return RV3032::VERSION;
#else
  return TFLUNACTRL_DEP_RV3032_VERSION;
#endif
}

static const char* libSsd1315Version() {
#if __has_include("ssd1315/Version.h")
  return SSD1315::VERSION;
#else
  return TFLUNACTRL_DEP_SSD1315_VERSION;
#endif
}

static const char* libAsyncSdVersion() {
#if __has_include("AsyncSD/Version.h")
  return AsyncSD::VERSION;
#else
  return TFLUNACTRL_DEP_ASYNCSD_VERSION;
#endif
}

static const char* libSystemChronoVersion() {
#if __has_include("SystemChrono/Version.h")
  return SystemChrono::VERSION;
#else
  return TFLUNACTRL_DEP_SYSTEMCHRONO_VERSION;
#endif
}

static const char* libStatusLedVersion() {
#if __has_include("StatusLed/Version.h")
  return StatusLed::VERSION;
#else
  return TFLUNACTRL_DEP_STATUSLED_VERSION;
#endif
}

static const char* libArduinoJsonVersion() {
#if defined(ARDUINOJSON_VERSION_MAJOR) && defined(ARDUINOJSON_VERSION_MINOR) && defined(ARDUINOJSON_VERSION_REVISION)
  return TFLUNACTRL_STR(ARDUINOJSON_VERSION_MAJOR) "." TFLUNACTRL_STR(ARDUINOJSON_VERSION_MINOR) "." TFLUNACTRL_STR(ARDUINOJSON_VERSION_REVISION);
#else
  return TFLUNACTRL_DEP_ARDUINOJSON_VERSION;
#endif
}

const char* healthToString(HealthState health) {
  switch (health) {
    case HealthState::OK:
      return "OK";
    case HealthState::DEGRADED:
      return "DEGRADED";
    case HealthState::FAULT:
      return "FAULT";
    case HealthState::UNKNOWN:
    default:
      return "UNKNOWN";
  }
}

void populateStatusJson(JsonDocument& doc, const SystemStatus& sys, const Sample* sample) {
  doc["type"] = "status";
  doc["health"] = healthToString(sys.health);
  doc["uptime_ms"] = sys.uptimeMs;
  doc["tick_last_duration_us"] = sys.tickLastDurationUs;
  doc["tick_max_duration_us"] = sys.tickMaxDurationUs;
  doc["tick_mean_duration_us"] = sys.tickMeanDurationUs;
  doc["tick_slow_count"] = sys.tickSlowCount;
  doc["tick_last_slow_ms"] = sys.tickLastSlowMs;
  doc["tick_phase_us_cmd"] = sys.tickPhaseUsCmd;
  doc["tick_phase_us_co2"] = sys.tickPhaseUsCo2;
  doc["tick_phase_us_i2c"] = sys.tickPhaseUsI2c;
  doc["tick_phase_us_sd"] = sys.tickPhaseUsSd;
  doc["tick_phase_us_io"] = sys.tickPhaseUsIo;
  doc["tick_phase_us_status"] = sys.tickPhaseUsStatus;
  doc["tick_phase_us_led"] = sys.tickPhaseUsLed;
  doc["tick_max_at_ms"] = sys.tickMaxAtMs;
  doc["tick_max_phase_us_cmd"] = sys.tickMaxPhaseUsCmd;
  doc["tick_max_phase_us_co2"] = sys.tickMaxPhaseUsCo2;
  doc["tick_max_phase_us_i2c"] = sys.tickMaxPhaseUsI2c;
  doc["tick_max_phase_us_sd"] = sys.tickMaxPhaseUsSd;
  doc["tick_max_phase_us_io"] = sys.tickMaxPhaseUsIo;
  doc["tick_max_phase_us_status"] = sys.tickMaxPhaseUsStatus;
  doc["tick_max_phase_us_led"] = sys.tickMaxPhaseUsLed;
  doc["tick_slow_dom_cmd_count"] = sys.tickSlowDomCmdCount;
  doc["tick_slow_dom_co2_count"] = sys.tickSlowDomCo2Count;
  doc["tick_slow_dom_i2c_count"] = sys.tickSlowDomI2cCount;
  doc["tick_slow_dom_sd_count"] = sys.tickSlowDomSdCount;
  doc["tick_slow_dom_io_count"] = sys.tickSlowDomIoCount;
  doc["tick_slow_dom_status_count"] = sys.tickSlowDomStatusCount;
  doc["tick_slow_dom_led_count"] = sys.tickSlowDomLedCount;
  doc["tick_slow_dom_other_count"] = sys.tickSlowDomOtherCount;
  doc["web_throttled"] = sys.webThrottled;
  doc["web_skip_count"] = sys.webSkipCount;
  doc["web_overrun_burst"] = sys.webOverrunBurst;
  doc["sample_count"] = sys.sampleCount;
  doc["last_sample_ms"] = sys.lastSampleMs;
  doc["time_source"] = sys.timeSource;
  doc["frame_age_ms"] = sys.lidarFrameAgeMs;
  doc["total_frames"] = sys.lidarStats.totalFrames;
  doc["valid_sample_count"] = sys.lidarStats.validSamples;
  doc["invalid_sample_count"] = sys.lidarStats.invalidSamples;
  doc["weak_sample_count"] = sys.lidarStats.weakSamples;
  doc["checksum_errors"] = sys.lidarChecksumErrors;
  doc["sync_loss_count"] = sys.lidarSyncLossCount;
  setFloatOrNull(doc, "min_distance_cm", sys.lidarStats.minDistanceCm, sys.lidarStats.hasDistanceStats);
  setFloatOrNull(doc, "max_distance_cm", sys.lidarStats.maxDistanceCm, sys.lidarStats.hasDistanceStats);
  setFloatOrNull(doc, "mean_distance_cm", sys.lidarStats.meanDistanceCm, sys.lidarStats.hasDistanceStats);
  setFloatOrNull(doc, "stddev_distance_cm", sys.lidarStats.stddevDistanceCm, sys.lidarStats.hasDistanceStats);
  setFloatOrNull(doc, "distance_range_cm", sys.lidarStats.rangeDistanceCm, sys.lidarStats.hasDistanceStats);
  setFloatOrNull(doc, "mean_strength", sys.lidarStats.meanStrength, sys.lidarStats.hasDistanceStats);
  setFloatOrNull(doc, "stddev_strength", sys.lidarStats.stddevStrength, sys.lidarStats.hasDistanceStats);
  doc["sd_mounted"] = sys.sdMounted;
  doc["sd_info_valid"] = sys.sdInfoValid;
  doc["sd_usage_valid"] = sys.sdUsageValid;
  doc["sd_fs_capacity_bytes"] = sys.sdFsCapacityBytes;
  doc["sd_fs_used_bytes"] = sys.sdFsUsedBytes;
  doc["sd_fs_free_bytes"] = sys.sdFsFreeBytes;
  doc["sd_card_capacity_bytes"] = sys.sdCardCapacityBytes;
  doc["sd_info_last_update_ms"] = sys.sdInfoLastUpdateMs;
  doc["sd_info_age_ms"] = sys.sdInfoAgeMs;
  doc["sd_fs_type_code"] = sys.sdFsType;
  doc["sd_card_type_code"] = sys.sdCardType;
  doc["sd_fs_type"] = sdFsTypeToString(sys.sdFsType);
  doc["sd_card_type"] = sdCardTypeToString(sys.sdCardType);
  doc["sd_usage_pct"] = sdUsagePercent(sys);
  doc["log_daily_ok"] = sys.logDailyOk;
  doc["log_all_ok"] = sys.logAllOk;
  doc["logging_ok"] = sys.sdMounted && (sys.logDailyOk || sys.logAllOk);
  doc["log_file"] = sys.logCurrentSampleFile;
  doc["log_lines_written"] = sys.logSampleWrittenTotal;
  doc["log_dropped_lines"] = sys.logDroppedCount;
  doc["ap_running"] = sys.wifiApRunning;
  doc["wifi_rssi_dbm"] = sys.wifiRssiDbm;
  doc["wifi_signal_pct"] = rssiToPercent(sys.wifiRssiDbm);
  doc["wifi_channel"] = sys.wifiChannel;
  doc["stations"] = sys.wifiStationCount;
  doc["web_clients"] = static_cast<uint32_t>(sys.webClientCount);
  doc["log_dropped_count"] = sys.logDroppedCount;
  doc["log_queue_depth"] = static_cast<uint32_t>(sys.logQueueDepth);
  doc["log_queue_capacity"] = static_cast<uint32_t>(sys.logQueueCapacity);
  doc["log_queue_using_psram"] = sys.logQueueUsingPsram;
  doc["log_last_write_ms"] = sys.logLastWriteMs;
  doc["log_last_write_age_ms"] = sys.logLastWriteAgeMs;
  doc["log_last_error_ms"] = sys.logLastErrorMs;
  doc["log_last_error_age_ms"] = sys.logLastErrorAgeMs;
  doc["log_last_error_msg"] = sys.logLastErrorMsg;
  doc["log_last_error_detail"] = sys.logLastErrorDetail;
  doc["log_io_budget_ms"] = sys.logIoBudgetMs;
  doc["log_last_tick_elapsed_ms"] = sys.logLastTickElapsedMs;
  doc["log_budget_exceeded_count"] = sys.logBudgetExceededCount;
  doc["log_event_dropped_count"] = sys.logEventDroppedCount;
  doc["log_event_queue_depth"] = static_cast<uint32_t>(sys.logEventQueueDepth);
  doc["log_event_queue_capacity"] = static_cast<uint32_t>(sys.logEventQueueCapacity);
  doc["log_event_queue_using_psram"] = sys.logEventQueueUsingPsram;
  doc["sample_history_depth"] = static_cast<uint32_t>(sys.sampleHistoryDepth);
  doc["sample_history_capacity"] = static_cast<uint32_t>(sys.sampleHistoryCapacity);
  doc["sample_history_using_psram"] = sys.sampleHistoryUsingPsram;
  doc["event_history_depth"] = static_cast<uint32_t>(sys.eventHistoryDepth);
  doc["event_history_capacity"] = static_cast<uint32_t>(sys.eventHistoryCapacity);
  doc["event_history_using_psram"] = sys.eventHistoryUsingPsram;
  doc["web_scratch_using_psram"] = sys.webScratchUsingPsram;
  doc["web_graph_scratch_capacity"] = sys.webGraphScratchCapacity;
  doc["web_event_scratch_capacity"] = sys.webEventScratchCapacity;
  doc["log_session_active"] = sys.logSessionActive;
  doc["log_session_dir"] = sys.logSessionDir;
  doc["log_current_sample_file"] = sys.logCurrentSampleFile;
  doc["log_current_event_file"] = sys.logCurrentEventFile;
  doc["log_current_sample_file_part"] = sys.logCurrentSampleFilePart;
  doc["log_current_event_file_part"] = sys.logCurrentEventFilePart;
  doc["log_sample_current_data_line"] = sys.logSampleCurrentDataLine;
  doc["log_event_current_data_line"] = sys.logEventCurrentDataLine;
  doc["log_sample_written_total"] = sys.logSampleWrittenTotal;
  doc["log_event_written_total"] = sys.logEventWrittenTotal;
  doc["log_sample_write_success_count"] = sys.logSampleWriteSuccessCount;
  doc["log_sample_write_failure_count"] = sys.logSampleWriteFailureCount;
  doc["log_event_write_success_count"] = sys.logEventWriteSuccessCount;
  doc["log_event_write_failure_count"] = sys.logEventWriteFailureCount;
  doc["log_sample_rotate_count"] = sys.logSampleRotateCount;
  doc["log_event_rotate_count"] = sys.logEventRotateCount;
  doc["i2c_error_count"] = sys.i2cErrorCount;
  doc["i2c_consecutive_errors"] = sys.i2cConsecutiveErrors;
  doc["i2c_recovery_count"] = sys.i2cRecoveryCount;
  doc["i2c_last_error_ms"] = sys.i2cLastErrorMs;
  doc["i2c_last_recovery_ms"] = sys.i2cLastRecoveryMs;
  doc["i2c_stuck_sda_count"] = sys.i2cStuckSdaCount;
  doc["i2c_stuck_fast_fail_count"] = sys.i2cStuckBusFastFailCount;
  doc["i2c_request_overflow_count"] = sys.i2cRequestOverflowCount;
  doc["i2c_result_dropped_count"] = sys.i2cResultDroppedCount;
  doc["i2c_stale_result_count"] = sys.i2cStaleResultCount;
  doc["i2c_slow_op_count"] = sys.i2cSlowOpCount;
  doc["i2c_recent_slow_op_count"] = sys.i2cRecentSlowOpCount;
  doc["i2c_request_queue_depth"] = static_cast<uint32_t>(sys.i2cRequestQueueDepth);
  doc["i2c_result_queue_depth"] = static_cast<uint32_t>(sys.i2cResultQueueDepth);
  doc["i2c_max_duration_us"] = sys.i2cMaxDurationUs;
  doc["i2c_rolling_max_duration_us"] = sys.i2cRollingMaxDurationUs;
  doc["i2c_mean_duration_us"] = sys.i2cMeanDurationUs;
  doc["i2c_task_alive_ms"] = sys.i2cTaskAliveMs;
  doc["i2c_task_alive_age_ms"] = sys.i2cTaskAliveAgeMs;
  doc["i2c_power_cycle_attempts"] = sys.i2cPowerCycleAttempts;
  doc["i2c_last_power_cycle_ms"] = sys.i2cLastPowerCycleMs;
  doc["i2c_power_cycle_configured"] = sys.i2cPowerCycleConfigured;
  doc["i2c_power_cycle_last_code"] = sys.i2cPowerCycleLastCode;
  doc["i2c_power_cycle_last_detail"] = sys.i2cPowerCycleLastDetail;
  doc["i2c_power_cycle_last_msg"] = sys.i2cPowerCycleLastMsg;
  doc["i2c_last_recovery_stage"] = sys.i2cLastRecoveryStage;
  doc["i2c_backend"] = sys.i2cBackendName;
  doc["i2c_deterministic_timeout"] = sys.i2cDeterministicTimeout;
  doc["i2c_rtc_consecutive_failures"] = sys.i2cRtcConsecutiveFailures;
  doc["i2c_env_consecutive_failures"] = sys.i2cEnvConsecutiveFailures;
  doc["cmd_queue_depth"] = static_cast<uint32_t>(sys.commandQueueDepth);
  doc["cmd_queue_capacity"] = static_cast<uint32_t>(sys.commandQueueCapacity);
  doc["cmd_queue_overflow_count"] = sys.commandQueueOverflowCount;
  doc["cmd_queue_last_overflow_ms"] = sys.commandQueueLastOverflowMs;
  doc["endstop_upper_pin"] = sys.endstopUpperPin;
  doc["endstop_upper_configured"] = sys.endstopUpperConfigured;
  doc["endstop_upper_active_low"] = sys.endstopUpperActiveLow;
  doc["endstop_upper_raw_high"] = sys.endstopUpperRawHigh;
  doc["endstop_upper_triggered"] = sys.endstopUpperTriggered;
  doc["endstop_upper_last_change_ms"] = sys.endstopUpperLastChangeMs;
  doc["endstop_lower_pin"] = sys.endstopLowerPin;
  doc["endstop_lower_configured"] = sys.endstopLowerConfigured;
  doc["endstop_lower_active_low"] = sys.endstopLowerActiveLow;
  doc["endstop_lower_raw_high"] = sys.endstopLowerRawHigh;
  doc["endstop_lower_triggered"] = sys.endstopLowerTriggered;
  doc["endstop_lower_last_change_ms"] = sys.endstopLowerLastChangeMs;
  doc["fw_version"] = sys.fwVersion;
  doc["fw_version_full"] = VERSION_FULL;
  doc["build_timestamp"] = BUILD_TIMESTAMP;
  doc["git_commit"] = GIT_COMMIT;
  doc["git_status"] = GIT_STATUS;
  doc["mcu_variant"] = mcuVariantString();
  doc["lib_ee871_version"] = libEe871Version();
  doc["lib_bme280_version"] = libBme280Version();
  doc["lib_sht3x_version"] = libSht3xVersion();
  doc["lib_rv3032_version"] = libRv3032Version();
  doc["lib_ssd1315_version"] = libSsd1315Version();
  doc["lib_asyncsd_version"] = libAsyncSdVersion();
  doc["lib_systemchrono_version"] = libSystemChronoVersion();
  doc["lib_statusled_version"] = libStatusLedVersion();
  doc["lib_arduinojson_version"] = libArduinoJsonVersion();
  doc["lib_espasyncwebserver_version"] = TFLUNACTRL_DEP_ESPASYNCWEBSERVER_VERSION;
  doc["lib_asynctcp_version"] = TFLUNACTRL_DEP_ASYNCTCP_VERSION;

  doc["heap_free_bytes"] = sys.heapFreeBytes;
  doc["heap_min_free_bytes"] = sys.heapMinFreeBytes;
  doc["heap_total_bytes"] = sys.heapTotalBytes;
  doc["heap_max_alloc_bytes"] = sys.heapMaxAllocBytes;
  doc["psram_available"] = sys.psramAvailable;
  doc["psram_total_bytes"] = sys.psramTotalBytes;
  doc["psram_free_bytes"] = sys.psramFreeBytes;
  doc["psram_min_free_bytes"] = sys.psramMinFreeBytes;
  doc["psram_max_alloc_bytes"] = sys.psramMaxAllocBytes;
  doc["main_task_stack_free_bytes"] = sys.mainTaskStackFreeBytes;
  doc["i2c_task_stack_free_bytes"] = sys.i2cTaskStackFreeBytes;

  if (sample) {
    setU16OrNull(doc, "distance_cm", sample->distanceCm, sample->validFrame);
    setU16OrNull(doc, "strength", sample->strength, sample->validFrame);
    setFloatOrNull(doc, "temperature_c", sample->lidarTempC, sample->validFrame);
    doc["valid_frame"] = sample->validFrame;
    doc["signal_ok"] = sample->signalOk;
    setFloatOrNull(doc, "co2_ppm", sample->co2ppm, (sample->validMask & VALID_CO2) != 0U);
    setFloatOrNull(doc, "temp_c", sample->tempC, (sample->validMask & VALID_TEMP) != 0U);
    setFloatOrNull(doc, "rh_pct", sample->rhPct, (sample->validMask & VALID_RH) != 0U);
    setFloatOrNull(doc, "pressure_hpa", sample->pressureHpa, (sample->validMask & VALID_PRESSURE) != 0U);
    doc["valid_mask"] = sample->validMask;
    if (sample->tsLocal[0] != '\0') {
      doc["rtc_local"] = sample->tsLocal;
      doc["timestamp"] = sample->tsLocal;
    } else {
      doc["rtc_local"] = nullptr;
      doc["timestamp"] = "uptime";
    }
    doc["uptime_ms"] = sample->uptimeMs;
    doc["sample_index"] = sample->sampleIndex;
  } else {
    doc["distance_cm"] = nullptr;
    doc["strength"] = nullptr;
    doc["temperature_c"] = nullptr;
    doc["valid_frame"] = false;
    doc["signal_ok"] = false;
    doc["timestamp"] = nullptr;
  }
}

void populateLiveStatusJson(JsonDocument& doc, const SystemStatus& sys, const Sample* sample) {
  doc["type"] = "live";
  doc["health"] = healthToString(sys.health);
  doc["uptime_ms"] = sys.uptimeMs;
  doc["sample_count"] = sys.sampleCount;
  doc["last_sample_ms"] = sys.lastSampleMs;
  doc["time_source"] = sys.timeSource;
  doc["frame_age_ms"] = sys.lidarFrameAgeMs;
  doc["valid_sample_count"] = sys.lidarStats.validSamples;
  doc["invalid_sample_count"] = sys.lidarStats.invalidSamples;
  doc["checksum_errors"] = sys.lidarChecksumErrors;
  setFloatOrNull(doc, "min_distance_cm", sys.lidarStats.minDistanceCm, sys.lidarStats.hasDistanceStats);
  setFloatOrNull(doc, "max_distance_cm", sys.lidarStats.maxDistanceCm, sys.lidarStats.hasDistanceStats);
  setFloatOrNull(doc, "mean_distance_cm", sys.lidarStats.meanDistanceCm, sys.lidarStats.hasDistanceStats);
  setFloatOrNull(doc, "stddev_distance_cm", sys.lidarStats.stddevDistanceCm, sys.lidarStats.hasDistanceStats);
  setFloatOrNull(doc, "distance_range_cm", sys.lidarStats.rangeDistanceCm, sys.lidarStats.hasDistanceStats);
  doc["cmd_queue_depth"] = static_cast<uint32_t>(sys.commandQueueDepth);

  doc["ap_running"] = sys.wifiApRunning;
  doc["wifi_rssi_dbm"] = sys.wifiRssiDbm;
  doc["wifi_signal_pct"] = rssiToPercent(sys.wifiRssiDbm);
  doc["wifi_channel"] = sys.wifiChannel;
  doc["stations"] = sys.wifiStationCount;
  doc["web_clients"] = static_cast<uint32_t>(sys.webClientCount);

  doc["sd_mounted"] = sys.sdMounted;
  doc["sd_info_valid"] = sys.sdInfoValid;
  doc["sd_usage_valid"] = sys.sdUsageValid;
  doc["sd_fs_capacity_bytes"] = sys.sdFsCapacityBytes;
  doc["sd_fs_used_bytes"] = sys.sdFsUsedBytes;
  doc["sd_fs_free_bytes"] = sys.sdFsFreeBytes;
  doc["sd_info_age_ms"] = sys.sdInfoAgeMs;
  doc["sd_fs_type"] = sdFsTypeToString(sys.sdFsType);
  doc["sd_card_type"] = sdCardTypeToString(sys.sdCardType);
  doc["sd_usage_pct"] = sdUsagePercent(sys);

  doc["log_daily_ok"] = sys.logDailyOk;
  doc["log_all_ok"] = sys.logAllOk;
  doc["logging_ok"] = sys.sdMounted && (sys.logDailyOk || sys.logAllOk);
  doc["log_file"] = sys.logCurrentSampleFile;
  doc["log_lines_written"] = sys.logSampleWrittenTotal;
  doc["log_dropped_lines"] = sys.logDroppedCount;
  doc["log_session_active"] = sys.logSessionActive;
  doc["log_dropped_count"] = sys.logDroppedCount;
  doc["log_queue_depth"] = static_cast<uint32_t>(sys.logQueueDepth);
  doc["log_queue_capacity"] = static_cast<uint32_t>(sys.logQueueCapacity);
  doc["log_event_dropped_count"] = sys.logEventDroppedCount;
  doc["log_event_queue_depth"] = static_cast<uint32_t>(sys.logEventQueueDepth);
  doc["log_event_queue_capacity"] = static_cast<uint32_t>(sys.logEventQueueCapacity);
  doc["log_last_write_age_ms"] = sys.logLastWriteAgeMs;
  doc["log_last_tick_elapsed_ms"] = sys.logLastTickElapsedMs;
  doc["log_budget_exceeded_count"] = sys.logBudgetExceededCount;
  doc["log_last_error_age_ms"] = sys.logLastErrorAgeMs;
  doc["log_last_error_msg"] = sys.logLastErrorMsg;
  doc["log_current_sample_file_part"] = sys.logCurrentSampleFilePart;
  doc["log_current_event_file_part"] = sys.logCurrentEventFilePart;
  doc["log_sample_current_data_line"] = sys.logSampleCurrentDataLine;
  doc["log_event_current_data_line"] = sys.logEventCurrentDataLine;
  doc["log_sample_written_total"] = sys.logSampleWrittenTotal;
  doc["log_event_written_total"] = sys.logEventWrittenTotal;
  doc["log_sample_write_success_count"] = sys.logSampleWriteSuccessCount;
  doc["log_sample_write_failure_count"] = sys.logSampleWriteFailureCount;
  doc["log_event_write_success_count"] = sys.logEventWriteSuccessCount;
  doc["log_event_write_failure_count"] = sys.logEventWriteFailureCount;
  doc["log_sample_rotate_count"] = sys.logSampleRotateCount;
  doc["log_event_rotate_count"] = sys.logEventRotateCount;

  doc["i2c_rtc_consecutive_failures"] = sys.i2cRtcConsecutiveFailures;
  doc["i2c_env_consecutive_failures"] = sys.i2cEnvConsecutiveFailures;

  doc["endstop_upper_pin"] = sys.endstopUpperPin;
  doc["endstop_upper_configured"] = sys.endstopUpperConfigured;
  doc["endstop_upper_active_low"] = sys.endstopUpperActiveLow;
  doc["endstop_upper_raw_high"] = sys.endstopUpperRawHigh;
  doc["endstop_upper_triggered"] = sys.endstopUpperTriggered;
  doc["endstop_upper_last_change_ms"] = sys.endstopUpperLastChangeMs;
  doc["endstop_lower_pin"] = sys.endstopLowerPin;
  doc["endstop_lower_configured"] = sys.endstopLowerConfigured;
  doc["endstop_lower_active_low"] = sys.endstopLowerActiveLow;
  doc["endstop_lower_raw_high"] = sys.endstopLowerRawHigh;
  doc["endstop_lower_triggered"] = sys.endstopLowerTriggered;
  doc["endstop_lower_last_change_ms"] = sys.endstopLowerLastChangeMs;

  if (sample) {
    setU16OrNull(doc, "distance_cm", sample->distanceCm, sample->validFrame);
    setU16OrNull(doc, "strength", sample->strength, sample->validFrame);
    setFloatOrNull(doc, "temperature_c", sample->lidarTempC, sample->validFrame);
    doc["valid_frame"] = sample->validFrame;
    doc["signal_ok"] = sample->signalOk;
    setFloatOrNull(doc, "co2_ppm", sample->co2ppm, (sample->validMask & VALID_CO2) != 0U);
    setFloatOrNull(doc, "temp_c", sample->tempC, (sample->validMask & VALID_TEMP) != 0U);
    setFloatOrNull(doc, "rh_pct", sample->rhPct, (sample->validMask & VALID_RH) != 0U);
    setFloatOrNull(doc, "pressure_hpa", sample->pressureHpa, (sample->validMask & VALID_PRESSURE) != 0U);
    doc["valid_mask"] = sample->validMask;
    if (sample->tsLocal[0] != '\0') {
      doc["rtc_local"] = sample->tsLocal;
      doc["timestamp"] = sample->tsLocal;
    } else {
      doc["rtc_local"] = nullptr;
      doc["timestamp"] = "uptime";
    }
    doc["sample_index"] = sample->sampleIndex;
  } else {
    doc["distance_cm"] = nullptr;
    doc["strength"] = nullptr;
    doc["temperature_c"] = nullptr;
    doc["valid_frame"] = false;
    doc["signal_ok"] = false;
    doc["co2_ppm"] = nullptr;
    doc["temp_c"] = nullptr;
    doc["rh_pct"] = nullptr;
    doc["pressure_hpa"] = nullptr;
    doc["valid_mask"] = 0;
    doc["rtc_local"] = nullptr;
    doc["timestamp"] = nullptr;
  }
}

size_t serializeStatusJsonBounded(const SystemStatus& sys, const Sample* sample, char* out, size_t outLen) {
  if (!out || outLen == 0) {
    return 0;
  }

  StaticJsonDocument<HardwareSettings::WEB_STATUS_JSON_DOC_BYTES> doc;
  populateStatusJson(doc, sys, sample);

  const size_t needed = measureJson(doc) + 1;  // +1 for null terminator
  if (needed > outLen) {
    out[0] = '\0';
    return 0;
  }

  return serializeJson(doc, out, outLen);
}

size_t serializeLiveStatusJsonBounded(const SystemStatus& sys,
                                      const Sample* sample,
                                      char* out,
                                      size_t outLen) {
  if (!out || outLen == 0) {
    return 0;
  }

  StaticJsonDocument<HardwareSettings::WEB_LIVE_STATUS_JSON_DOC_BYTES> doc;
  populateLiveStatusJson(doc, sys, sample);

  const size_t needed = measureJson(doc) + 1;
  if (needed > outLen) {
    out[0] = '\0';
    return 0;
  }

  return serializeJson(doc, out, outLen);
}

void populateDeviceStatusJson(JsonDocument& doc, const DeviceStatus& status, uint32_t id) {
  doc["id"] = id;
  doc["name"] = status.name;
  doc["health"] = healthToString(status.health);
  doc["last_ok_ms"] = status.lastOkMs;
  doc["last_error_ms"] = status.lastErrorMs;
  doc["last_activity_ms"] = status.lastActivityMs;
  doc["last_status_code"] = static_cast<uint32_t>(status.lastStatus.code);
  doc["last_status_detail"] = status.lastStatus.detail;
  doc["last_status_msg"] = status.lastStatus.msg;
  doc["error_count"] = status.errorCount;
  doc["optional"] = status.optional;
}

void populateGraphSampleJson(JsonDocument& doc, const Sample& sample) {
  doc["ts"] = sample.tsUnix;
  if (sample.tsLocal[0] != '\0') {
    doc["ts_local"] = sample.tsLocal;
  } else {
    doc["ts_local"] = nullptr;
  }

  setFloatOrNull(doc, "co2_ppm", sample.co2ppm, (sample.validMask & VALID_CO2) != 0U);
  setFloatOrNull(doc, "temp_c", sample.tempC, (sample.validMask & VALID_TEMP) != 0U);
  setFloatOrNull(doc, "rh_pct", sample.rhPct, (sample.validMask & VALID_RH) != 0U);
  setFloatOrNull(doc, "pressure_hpa", sample.pressureHpa, (sample.validMask & VALID_PRESSURE) != 0U);
  setU16OrNull(doc, "distance_cm", sample.distanceCm, sample.validFrame);
  setU16OrNull(doc, "strength", sample.strength, sample.validFrame);
  setFloatOrNull(doc, "temperature_c", sample.lidarTempC, sample.validFrame);
  doc["valid_frame"] = sample.validFrame;
  doc["signal_ok"] = sample.signalOk;
  doc["uptime_ms"] = sample.uptimeMs;
  doc["sample_index"] = sample.sampleIndex;
  doc["valid_mask"] = sample.validMask;
}

}  // namespace TFLunaControl

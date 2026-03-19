#include "TFLunaControl/RuntimeSettings.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

namespace TFLunaControl {

static bool isAsciiAlphaNum(char c) {
  return (c >= '0' && c <= '9') ||
         (c >= 'A' && c <= 'Z') ||
         (c >= 'a' && c <= 'z');
}

static bool isValidLogSessionName(const char* name, size_t len) {
  if (name == nullptr || len < RuntimeSettings::MIN_LOG_SESSION_NAME_LEN ||
      len > RuntimeSettings::MAX_LOG_SESSION_NAME_LEN) {
    return false;
  }
  if (!isAsciiAlphaNum(name[0])) {
    return false;
  }
  if (!isAsciiAlphaNum(name[len - 1U])) {
    return false;
  }
  for (size_t i = 0; i < len; ++i) {
    const char c = name[i];
    if (isAsciiAlphaNum(c) || c == '_' || c == '-') {
      continue;
    }
    return false;
  }
  return true;
}

Status RuntimeSettings::validate() const {
  if (sampleIntervalMs < MIN_SAMPLE_INTERVAL_MS || sampleIntervalMs > MAX_SAMPLE_INTERVAL_MS) {
    return Status(Err::INVALID_CONFIG, 0, "sampleIntervalMs out of range");
  }
  if (logFlushMs < MIN_LOG_FLUSH_MS || logFlushMs > MAX_LOG_FLUSH_MS) {
    return Status(Err::INVALID_CONFIG, 0, "logFlushMs out of range");
  }
  if (logIoBudgetMs < MIN_LOG_IO_BUDGET_MS || logIoBudgetMs > MAX_LOG_IO_BUDGET_MS) {
    return Status(Err::INVALID_CONFIG, 0, "logIoBudgetMs out of range");
  }
  if (logMountRetryMs < MIN_LOG_MOUNT_RETRY_MS || logMountRetryMs > MAX_LOG_MOUNT_RETRY_MS) {
    return Status(Err::INVALID_CONFIG, 0, "logMountRetryMs out of range");
  }
  if (logWriteRetryBackoffMs < MIN_LOG_WRITE_RETRY_BACKOFF_MS ||
      logWriteRetryBackoffMs > MAX_LOG_WRITE_RETRY_BACKOFF_MS) {
    return Status(Err::INVALID_CONFIG, 0, "logWriteRetryBackoffMs out of range");
  }
  if (logMaxWriteRetries < MIN_LOG_MAX_WRITE_RETRIES || logMaxWriteRetries > MAX_LOG_MAX_WRITE_RETRIES) {
    return Status(Err::INVALID_CONFIG, 0, "logMaxWriteRetries out of range");
  }
  if (logEventsMaxBytes < MIN_LOG_EVENTS_MAX_BYTES || logEventsMaxBytes > MAX_LOG_EVENTS_MAX_BYTES) {
    return Status(Err::INVALID_CONFIG, 0, "logEventsMaxBytes out of range");
  }
  if (lidarServiceMs < MIN_LIDAR_SERVICE_MS || lidarServiceMs > MAX_LIDAR_SERVICE_MS) {
    return Status(Err::INVALID_CONFIG, 0, "lidarServiceMs out of range");
  }
  if (lidarMinStrength < MIN_LIDAR_SIGNAL_STRENGTH || lidarMinStrength > MAX_LIDAR_SIGNAL_STRENGTH) {
    return Status(Err::INVALID_CONFIG, 0, "lidarMinStrength out of range");
  }
  if (lidarMaxDistanceCm < MIN_LIDAR_MAX_DISTANCE_CM || lidarMaxDistanceCm > MAX_LIDAR_MAX_DISTANCE_CM) {
    return Status(Err::INVALID_CONFIG, 0, "lidarMaxDistanceCm out of range");
  }
  if (lidarFrameStaleMs < MIN_LIDAR_FRAME_STALE_MS || lidarFrameStaleMs > MAX_LIDAR_FRAME_STALE_MS) {
    return Status(Err::INVALID_CONFIG, 0, "lidarFrameStaleMs out of range");
  }
  if (serialPrintIntervalMs < MIN_SERIAL_PRINT_INTERVAL_MS ||
      serialPrintIntervalMs > MAX_SERIAL_PRINT_INTERVAL_MS) {
    return Status(Err::INVALID_CONFIG, 0, "serialPrintIntervalMs out of range");
  }
  if (cliVerbosity < MIN_CLI_VERBOSITY || cliVerbosity > MAX_CLI_VERBOSITY) {
    return Status(Err::INVALID_CONFIG, 0, "cliVerbosity out of range");
  }
  if (logAllEnabled &&
      (logAllMaxBytes < MIN_LOG_ALL_MAX_BYTES || logAllMaxBytes > MAX_LOG_ALL_MAX_BYTES)) {
    return Status(Err::INVALID_CONFIG, 0, "logAllMaxBytes out of range");
  }
  const size_t logSessionNameLen = strnlen(logSessionName, sizeof(logSessionName));
  if (logSessionNameLen >= sizeof(logSessionName)) {
    return Status(Err::INVALID_CONFIG, 0, "logSessionName not null-terminated");
  }
  if (!isValidLogSessionName(logSessionName, logSessionNameLen)) {
    return Status(Err::INVALID_CONFIG, 0, "logSessionName must match [A-Za-z0-9][A-Za-z0-9_-]*[A-Za-z0-9]");
  }
  if (apAutoOffMs < MIN_AP_AUTO_OFF_MS || apAutoOffMs > MAX_AP_AUTO_OFF_MS) {
    return Status(Err::INVALID_CONFIG, 0, "apAutoOffMs out of range");
  }
  if (!isfinite(co2OnPpm) || !isfinite(co2OffPpm)) {
    return Status(Err::INVALID_CONFIG, 0, "co2 thresholds must be finite");
  }
  if (co2OnPpm < MIN_CO2_PPM || co2OnPpm > MAX_CO2_PPM ||
      co2OffPpm < MIN_CO2_PPM || co2OffPpm > MAX_CO2_PPM) {
    return Status(Err::INVALID_CONFIG, 0, "co2 thresholds out of range");
  }
  if (co2OnPpm <= co2OffPpm) {
    return Status(Err::INVALID_CONFIG, 0, "co2OnPpm must be > co2OffPpm");
  }
  if (!isfinite(tempOnC) || !isfinite(tempOffC)) {
    return Status(Err::INVALID_CONFIG, 0, "temp thresholds must be finite");
  }
  if (tempOnC < MIN_TEMP_C || tempOnC > MAX_TEMP_C ||
      tempOffC < MIN_TEMP_C || tempOffC > MAX_TEMP_C) {
    return Status(Err::INVALID_CONFIG, 0, "temp thresholds out of range");
  }
  if (tempOnC <= tempOffC) {
    return Status(Err::INVALID_CONFIG, 0, "tempOnC must be > tempOffC");
  }
  if (!isfinite(rhOnPct) || !isfinite(rhOffPct)) {
    return Status(Err::INVALID_CONFIG, 0, "rh thresholds must be finite");
  }
  if (rhOnPct < MIN_RH_PCT || rhOnPct > MAX_RH_PCT ||
      rhOffPct < MIN_RH_PCT || rhOffPct > MAX_RH_PCT) {
    return Status(Err::INVALID_CONFIG, 0, "rh thresholds out of range");
  }
  if (rhOnPct <= rhOffPct) {
    return Status(Err::INVALID_CONFIG, 0, "rhOnPct must be > rhOffPct");
  }
  if (outputSource > static_cast<uint8_t>(2U)) {
    return Status(Err::INVALID_CONFIG, 0, "outputSource must be 0..2");
  }
  const auto validOutputChannel = [](uint8_t channel) -> bool {
    if (channel == OUTPUT_CHANNEL_DISABLED) {
      return true;
    }
    return channel <= MAX_OUTPUT_CHANNEL_INDEX;
  };
  if (!validOutputChannel(outputValveChannel)) {
    return Status(Err::INVALID_CONFIG, 0, "outputValveChannel out of range");
  }
  if (!validOutputChannel(outputFanChannel)) {
    return Status(Err::INVALID_CONFIG, 0, "outputFanChannel out of range");
  }
  if (outputValveChannel != OUTPUT_CHANNEL_DISABLED &&
      outputFanChannel != OUTPUT_CHANNEL_DISABLED &&
      outputValveChannel == outputFanChannel) {
    return Status(Err::INVALID_CONFIG, 0, "output valve/fan channels must differ");
  }
  if (outputFanPwmPercent > MAX_OUTPUT_FAN_PWM_PERCENT) {
    return Status(Err::INVALID_CONFIG, 0, "outputFanPwmPercent out of range");
  }
  if (outputFanPeriodMs != OUTPUT_FAN_PERIOD_DISABLED_MS &&
      (outputFanPeriodMs < MIN_OUTPUT_FAN_PERIOD_MS ||
       outputFanPeriodMs > MAX_OUTPUT_FAN_PERIOD_MS)) {
    return Status(Err::INVALID_CONFIG, 0, "outputFanPeriodMs out of range");
  }
  if (outputFanOnMs < MIN_OUTPUT_FAN_ON_MS || outputFanOnMs > MAX_OUTPUT_FAN_ON_MS) {
    return Status(Err::INVALID_CONFIG, 0, "outputFanOnMs out of range");
  }
  if (outputFanPeriodMs != OUTPUT_FAN_PERIOD_DISABLED_MS &&
      outputFanOnMs > outputFanPeriodMs) {
    return Status(Err::INVALID_CONFIG, 0, "outputFanOnMs > outputFanPeriodMs");
  }
  if (minOnMs < MIN_DWELL_MS || minOnMs > MAX_DWELL_MS ||
      minOffMs < MIN_DWELL_MS || minOffMs > MAX_DWELL_MS) {
    return Status(Err::INVALID_CONFIG, 0, "minOnMs/minOffMs out of range");
  }
  if (commandDrainPerTick < MIN_COMMANDS_PER_TICK || commandDrainPerTick > MAX_COMMANDS_PER_TICK) {
    return Status(Err::INVALID_CONFIG, 0, "commandDrainPerTick out of range");
  }
  if (commandQueueDegradedWindowMs < MIN_COMMAND_QUEUE_DEGRADED_WINDOW_MS ||
      commandQueueDegradedWindowMs > MAX_COMMAND_QUEUE_DEGRADED_WINDOW_MS) {
    return Status(Err::INVALID_CONFIG, 0, "commandQueueDegradedWindowMs out of range");
  }
  if (commandQueueDegradedDepthThreshold < MIN_COMMAND_QUEUE_DEGRADED_DEPTH ||
      commandQueueDegradedDepthThreshold > MAX_COMMAND_QUEUE_DEGRADED_DEPTH) {
    return Status(Err::INVALID_CONFIG, 0, "commandQueueDegradedDepthThreshold out of range");
  }
  if (outputDataStaleMinMs < MIN_OUTPUT_DATA_STALE_MIN_MS ||
      outputDataStaleMinMs > MAX_OUTPUT_DATA_STALE_MIN_MS) {
    return Status(Err::INVALID_CONFIG, 0, "outputDataStaleMinMs out of range");
  }
  if (mainTickSlowThresholdUs < MIN_MAIN_TICK_SLOW_THRESHOLD_US ||
      mainTickSlowThresholdUs > MAX_MAIN_TICK_SLOW_THRESHOLD_US) {
    return Status(Err::INVALID_CONFIG, 0, "mainTickSlowThresholdUs out of range");
  }
  if (webOverrunThresholdUs < MIN_WEB_OVERRUN_THRESHOLD_US ||
      webOverrunThresholdUs > MAX_WEB_OVERRUN_THRESHOLD_US) {
    return Status(Err::INVALID_CONFIG, 0, "webOverrunThresholdUs out of range");
  }
  if (webOverrunBurstThreshold < MIN_WEB_OVERRUN_BURST_THRESHOLD ||
      webOverrunBurstThreshold > MAX_WEB_OVERRUN_BURST_THRESHOLD) {
    return Status(Err::INVALID_CONFIG, 0, "webOverrunBurstThreshold out of range");
  }
  if (webOverrunThrottleMs < MIN_WEB_OVERRUN_THROTTLE_MS ||
      webOverrunThrottleMs > MAX_WEB_OVERRUN_THROTTLE_MS) {
    return Status(Err::INVALID_CONFIG, 0, "webOverrunThrottleMs out of range");
  }
  if (ledHealthInitMs > MAX_LED_HEALTH_INIT_MS) {
    return Status(Err::INVALID_CONFIG, 0, "ledHealthInitMs out of range");
  }
  if (ledHealthDebounceMs > MAX_LED_HEALTH_DEBOUNCE_MS) {
    return Status(Err::INVALID_CONFIG, 0, "ledHealthDebounceMs out of range");
  }
  if (apStartRetryBackoffMs < MIN_AP_START_RETRY_BACKOFF_MS ||
      apStartRetryBackoffMs > MAX_AP_START_RETRY_BACKOFF_MS) {
    return Status(Err::INVALID_CONFIG, 0, "apStartRetryBackoffMs out of range");
  }
  if (i2cFreqHz != 100000UL && i2cFreqHz != 400000UL) {
    return Status(Err::INVALID_CONFIG, 0, "i2cFreqHz must be 100k or 400k");
  }
  if (i2cOpTimeoutMs < MIN_I2C_TIMEOUT_MS || i2cOpTimeoutMs > MAX_I2C_TIMEOUT_MS) {
    return Status(Err::INVALID_CONFIG, 0, "i2cOpTimeoutMs out of range");
  }
  if (i2cStuckDebounceMs < MIN_I2C_STUCK_DEBOUNCE_MS ||
      i2cStuckDebounceMs > MAX_I2C_STUCK_DEBOUNCE_MS) {
    return Status(Err::INVALID_CONFIG, 0, "i2cStuckDebounceMs out of range");
  }
  if (i2cMaxConsecutiveFailures < MIN_I2C_CONSECUTIVE_FAILURES ||
      i2cMaxConsecutiveFailures > MAX_I2C_CONSECUTIVE_FAILURES) {
    return Status(Err::INVALID_CONFIG, 0, "i2cMaxConsecutiveFailures out of range");
  }
  if (i2cRecoveryBackoffMs < MIN_I2C_RECOVERY_BACKOFF_MS ||
      i2cRecoveryBackoffMs > MAX_I2C_RECOVERY_BACKOFF_MS) {
    return Status(Err::INVALID_CONFIG, 0, "i2cRecoveryBackoffMs out of range");
  }
  if (i2cRecoveryBackoffMaxMs < MIN_I2C_RECOVERY_BACKOFF_MAX_MS ||
      i2cRecoveryBackoffMaxMs > MAX_I2C_RECOVERY_BACKOFF_MAX_MS) {
    return Status(Err::INVALID_CONFIG, 0, "i2cRecoveryBackoffMaxMs out of range");
  }
  if (i2cRecoveryBackoffMaxMs < i2cRecoveryBackoffMs) {
    return Status(Err::INVALID_CONFIG, 0, "i2cRecoveryBackoffMaxMs < i2cRecoveryBackoffMs");
  }
  if (i2cRequestsPerTick < MIN_I2C_REQUESTS_PER_TICK ||
      i2cRequestsPerTick > MAX_I2C_REQUESTS_PER_TICK) {
    return Status(Err::INVALID_CONFIG, 0, "i2cRequestsPerTick out of range");
  }
  if (i2cSlowOpThresholdUs < MIN_I2C_SLOW_OP_THRESHOLD_US ||
      i2cSlowOpThresholdUs > MAX_I2C_SLOW_OP_THRESHOLD_US) {
    return Status(Err::INVALID_CONFIG, 0, "i2cSlowOpThresholdUs out of range");
  }
  if (i2cSlowOpDegradeCount < MIN_I2C_SLOW_OP_DEGRADE_COUNT ||
      i2cSlowOpDegradeCount > MAX_I2C_SLOW_OP_DEGRADE_COUNT) {
    return Status(Err::INVALID_CONFIG, 0, "i2cSlowOpDegradeCount out of range");
  }
  if (i2cTaskHeartbeatTimeoutMs < MIN_I2C_HEARTBEAT_TIMEOUT_MS ||
      i2cTaskHeartbeatTimeoutMs > MAX_I2C_HEARTBEAT_TIMEOUT_MS) {
    return Status(Err::INVALID_CONFIG, 0, "i2cTaskHeartbeatTimeoutMs out of range");
  }
  if (i2cEnvPollMs < MIN_I2C_POLL_MS || i2cEnvPollMs > MAX_I2C_POLL_MS) {
    return Status(Err::INVALID_CONFIG, 0, "i2cEnvPollMs out of range");
  }
  if (i2cRtcPollMs < MIN_I2C_POLL_MS || i2cRtcPollMs > MAX_I2C_POLL_MS) {
    return Status(Err::INVALID_CONFIG, 0, "i2cRtcPollMs out of range");
  }
  if (i2cDisplayPollMs < MIN_I2C_POLL_MS || i2cDisplayPollMs > MAX_I2C_POLL_MS) {
    return Status(Err::INVALID_CONFIG, 0, "i2cDisplayPollMs out of range");
  }
  if (i2cEnvConversionWaitMs < MIN_I2C_ENV_CONVERSION_WAIT_MS ||
      i2cEnvConversionWaitMs > MAX_I2C_ENV_CONVERSION_WAIT_MS) {
    return Status(Err::INVALID_CONFIG, 0, "i2cEnvConversionWaitMs out of range");
  }
  if (i2cEnvBmeMode > MAX_I2C_ENV_BME_MODE ||
      (i2cEnvBmeMode != 0U && i2cEnvBmeMode != 1U && i2cEnvBmeMode != 3U)) {
    return Status(Err::INVALID_CONFIG, 0, "i2cEnvBmeMode invalid");
  }
  if (i2cEnvBmeOsrsT > MAX_I2C_ENV_BME_OVERSAMPLING) {
    return Status(Err::INVALID_CONFIG, 0, "i2cEnvBmeOsrsT out of range");
  }
  if (i2cEnvBmeOsrsP > MAX_I2C_ENV_BME_OVERSAMPLING) {
    return Status(Err::INVALID_CONFIG, 0, "i2cEnvBmeOsrsP out of range");
  }
  if (i2cEnvBmeOsrsH > MAX_I2C_ENV_BME_OVERSAMPLING) {
    return Status(Err::INVALID_CONFIG, 0, "i2cEnvBmeOsrsH out of range");
  }
  if (i2cEnvBmeFilter > MAX_I2C_ENV_BME_FILTER) {
    return Status(Err::INVALID_CONFIG, 0, "i2cEnvBmeFilter out of range");
  }
  if (i2cEnvBmeStandby > MAX_I2C_ENV_BME_STANDBY) {
    return Status(Err::INVALID_CONFIG, 0, "i2cEnvBmeStandby out of range");
  }
  if (i2cEnvShtMode > MAX_I2C_ENV_SHT_MODE) {
    return Status(Err::INVALID_CONFIG, 0, "i2cEnvShtMode out of range");
  }
  if (i2cEnvShtRepeatability > MAX_I2C_ENV_SHT_REPEATABILITY) {
    return Status(Err::INVALID_CONFIG, 0, "i2cEnvShtRepeatability out of range");
  }
  if (i2cEnvShtPeriodicRate > MAX_I2C_ENV_SHT_PERIODIC_RATE) {
    return Status(Err::INVALID_CONFIG, 0, "i2cEnvShtPeriodicRate out of range");
  }
  if (i2cEnvShtClockStretching > MAX_I2C_ENV_SHT_CLOCK_STRETCHING) {
    return Status(Err::INVALID_CONFIG, 0, "i2cEnvShtClockStretching out of range");
  }
  if (i2cEnvShtCommandDelayMs < MIN_I2C_ENV_SHT_COMMAND_DELAY_MS ||
      i2cEnvShtCommandDelayMs > MAX_I2C_ENV_SHT_COMMAND_DELAY_MS) {
    return Status(Err::INVALID_CONFIG, 0, "i2cEnvShtCommandDelayMs out of range");
  }
  if (i2cEnvShtNotReadyTimeoutMs > MAX_I2C_ENV_SHT_NOT_READY_TIMEOUT_MS) {
    return Status(Err::INVALID_CONFIG, 0, "i2cEnvShtNotReadyTimeoutMs out of range");
  }
  if (i2cEnvShtPeriodicFetchMarginMs > MAX_I2C_ENV_SHT_PERIODIC_FETCH_MARGIN_MS) {
    return Status(Err::INVALID_CONFIG, 0, "i2cEnvShtPeriodicFetchMarginMs out of range");
  }
  if (i2cRecoverTimeoutMs < MIN_I2C_RECOVER_TIMEOUT_MS ||
      i2cRecoverTimeoutMs > MAX_I2C_RECOVER_TIMEOUT_MS) {
    return Status(Err::INVALID_CONFIG, 0, "i2cRecoverTimeoutMs out of range");
  }
  if (i2cMaxResultsPerTick < MIN_I2C_MAX_RESULTS_PER_TICK ||
      i2cMaxResultsPerTick > MAX_I2C_MAX_RESULTS_PER_TICK) {
    return Status(Err::INVALID_CONFIG, 0, "i2cMaxResultsPerTick out of range");
  }
  if (i2cTaskWaitMs < MIN_I2C_TASK_WAIT_MS || i2cTaskWaitMs > MAX_I2C_TASK_WAIT_MS) {
    return Status(Err::INVALID_CONFIG, 0, "i2cTaskWaitMs out of range");
  }
  if (i2cHealthStaleTaskMultiplier < MIN_I2C_HEALTH_STALE_TASK_MULTIPLIER ||
      i2cHealthStaleTaskMultiplier > MAX_I2C_HEALTH_STALE_TASK_MULTIPLIER) {
    return Status(Err::INVALID_CONFIG, 0, "i2cHealthStaleTaskMultiplier out of range");
  }
  if (i2cSlowWindowMs < MIN_I2C_SLOW_WINDOW_MS || i2cSlowWindowMs > MAX_I2C_SLOW_WINDOW_MS) {
    return Status(Err::INVALID_CONFIG, 0, "i2cSlowWindowMs out of range");
  }
  if (i2cHealthRecentWindowMs < MIN_I2C_HEALTH_RECENT_WINDOW_MS ||
      i2cHealthRecentWindowMs > MAX_I2C_HEALTH_RECENT_WINDOW_MS) {
    return Status(Err::INVALID_CONFIG, 0, "i2cHealthRecentWindowMs out of range");
  }
  if (i2cEnvAddress < MIN_I2C_ADDR || i2cEnvAddress > MAX_I2C_ADDR) {
    return Status(Err::INVALID_CONFIG, 0, "i2cEnvAddress out of range");
  }
  if (i2cRtcAddress < MIN_I2C_ADDR || i2cRtcAddress > MAX_I2C_ADDR) {
    return Status(Err::INVALID_CONFIG, 0, "i2cRtcAddress out of range");
  }
  if (i2cRtcBackupMode > MAX_I2C_RTC_BACKUP_MODE) {
    return Status(Err::INVALID_CONFIG, 0, "i2cRtcBackupMode out of range");
  }
  if (i2cRtcEepromTimeoutMs < MIN_I2C_RTC_EEPROM_TIMEOUT_MS ||
      i2cRtcEepromTimeoutMs > MAX_I2C_RTC_EEPROM_TIMEOUT_MS) {
    return Status(Err::INVALID_CONFIG, 0, "i2cRtcEepromTimeoutMs out of range");
  }
  if (i2cRtcOfflineThreshold < MIN_I2C_RTC_OFFLINE_THRESHOLD ||
      i2cRtcOfflineThreshold > MAX_I2C_RTC_OFFLINE_THRESHOLD) {
    return Status(Err::INVALID_CONFIG, 0, "i2cRtcOfflineThreshold out of range");
  }
  if (i2cDisplayAddress < MIN_I2C_ADDR || i2cDisplayAddress > MAX_I2C_ADDR) {
    return Status(Err::INVALID_CONFIG, 0, "i2cDisplayAddress out of range");
  }
  if (e2Address > MAX_E2_ADDR) {
    return Status(Err::INVALID_CONFIG, 0, "e2Address out of range");
  }
  if (e2ClockLowUs < MIN_E2_CLOCK_US || e2ClockLowUs > MAX_E2_CLOCK_US) {
    return Status(Err::INVALID_CONFIG, 0, "e2ClockLowUs out of range");
  }
  if (e2ClockHighUs < MIN_E2_CLOCK_US || e2ClockHighUs > MAX_E2_CLOCK_US) {
    return Status(Err::INVALID_CONFIG, 0, "e2ClockHighUs out of range");
  }
  if (e2StartHoldUs < MIN_E2_HOLD_US || e2StartHoldUs > MAX_E2_HOLD_US) {
    return Status(Err::INVALID_CONFIG, 0, "e2StartHoldUs out of range");
  }
  if (e2StopHoldUs < MIN_E2_HOLD_US || e2StopHoldUs > MAX_E2_HOLD_US) {
    return Status(Err::INVALID_CONFIG, 0, "e2StopHoldUs out of range");
  }
  if (e2BitTimeoutUs < MIN_E2_BIT_TIMEOUT_US || e2BitTimeoutUs > MAX_E2_BIT_TIMEOUT_US) {
    return Status(Err::INVALID_CONFIG, 0, "e2BitTimeoutUs out of range");
  }
  if (e2ByteTimeoutUs < MIN_E2_BYTE_TIMEOUT_US || e2ByteTimeoutUs > MAX_E2_BYTE_TIMEOUT_US) {
    return Status(Err::INVALID_CONFIG, 0, "e2ByteTimeoutUs out of range");
  }
  if (e2WriteDelayMs < MIN_E2_WRITE_DELAY_MS || e2WriteDelayMs > MAX_E2_WRITE_DELAY_MS) {
    return Status(Err::INVALID_CONFIG, 0, "e2WriteDelayMs out of range");
  }
  if (e2IntervalWriteDelayMs < MIN_E2_INTERVAL_WRITE_DELAY_MS ||
      e2IntervalWriteDelayMs > MAX_E2_INTERVAL_WRITE_DELAY_MS) {
    return Status(Err::INVALID_CONFIG, 0, "e2IntervalWriteDelayMs out of range");
  }
  if (e2ByteTimeoutUs < e2BitTimeoutUs) {
    return Status(Err::INVALID_CONFIG, 0, "e2ByteTimeoutUs < e2BitTimeoutUs");
  }
  if (e2OfflineThreshold < MIN_E2_OFFLINE_THRESHOLD ||
      e2OfflineThreshold > MAX_E2_OFFLINE_THRESHOLD) {
    return Status(Err::INVALID_CONFIG, 0, "e2OfflineThreshold out of range");
  }
  if (e2RecoveryBackoffMs < MIN_E2_RECOVERY_BACKOFF_MS ||
      e2RecoveryBackoffMs > MAX_E2_RECOVERY_BACKOFF_MS) {
    return Status(Err::INVALID_CONFIG, 0, "e2RecoveryBackoffMs out of range");
  }
  if (e2RecoveryBackoffMaxMs < MIN_E2_RECOVERY_BACKOFF_MAX_MS ||
      e2RecoveryBackoffMaxMs > MAX_E2_RECOVERY_BACKOFF_MAX_MS) {
    return Status(Err::INVALID_CONFIG, 0, "e2RecoveryBackoffMaxMs out of range");
  }
  if (e2RecoveryBackoffMaxMs < e2RecoveryBackoffMs) {
    return Status(Err::INVALID_CONFIG, 0, "e2RecoveryBackoffMaxMs < e2RecoveryBackoffMs");
  }
  if (e2ConfigIntervalDs != 0U &&
      (e2ConfigIntervalDs < MIN_E2_CONFIG_INTERVAL_DS || e2ConfigIntervalDs > MAX_E2_CONFIG_INTERVAL_DS)) {
    return Status(Err::INVALID_CONFIG, 0, "e2ConfigIntervalDs out of range");
  }
  if (e2ConfigCo2IntervalFactor != E2_CONFIG_INTERVAL_FACTOR_DISABLED &&
      e2ConfigCo2IntervalFactor < MIN_E2_CONFIG_INTERVAL_FACTOR) {
    return Status(Err::INVALID_CONFIG, 0, "e2ConfigCo2IntervalFactor out of range");
  }
  if (e2ConfigFilter != E2_CONFIG_FILTER_DISABLED &&
      e2ConfigFilter > MAX_E2_CONFIG_FILTER) {
    return Status(Err::INVALID_CONFIG, 0, "e2ConfigFilter out of range");
  }
  if (e2ConfigOperatingMode != E2_CONFIG_OPERATING_MODE_DISABLED &&
      e2ConfigOperatingMode > MAX_E2_CONFIG_OPERATING_MODE) {
    return Status(Err::INVALID_CONFIG, 0, "e2ConfigOperatingMode out of range");
  }
  if (e2ConfigOffsetPpm != E2_CONFIG_OFFSET_PPM_DISABLED &&
      (e2ConfigOffsetPpm < MIN_E2_CONFIG_OFFSET_PPM ||
       e2ConfigOffsetPpm > MAX_E2_CONFIG_OFFSET_PPM)) {
    return Status(Err::INVALID_CONFIG, 0, "e2ConfigOffsetPpm out of range");
  }
  if (e2ConfigGain != E2_CONFIG_GAIN_DISABLED &&
      (e2ConfigGain < MIN_E2_CONFIG_GAIN || e2ConfigGain > MAX_E2_CONFIG_GAIN)) {
    return Status(Err::INVALID_CONFIG, 0, "e2ConfigGain out of range");
  }
  const size_t ssidLen = strnlen(apSsid, sizeof(apSsid));
  if (ssidLen == 0 || ssidLen >= sizeof(apSsid)) {
    return Status(Err::INVALID_CONFIG, 0, "apSsid length invalid");
  }
  const size_t passLen = strnlen(apPass, sizeof(apPass));
  if (passLen > 0 && passLen < 8) {
    return Status(Err::INVALID_CONFIG, 0, "apPass must be >= 8 or empty");
  }
  if (webMaxSettingsBodyBytes < MIN_WEB_MAX_SETTINGS_BODY_BYTES ||
      webMaxSettingsBodyBytes > MAX_WEB_MAX_SETTINGS_BODY_BYTES) {
    return Status(Err::INVALID_CONFIG, 0, "webMaxSettingsBodyBytes out of range");
  }
  if (webMaxRtcBodyBytes < MIN_WEB_MAX_RTC_BODY_BYTES ||
      webMaxRtcBodyBytes > MAX_WEB_MAX_RTC_BODY_BYTES) {
    return Status(Err::INVALID_CONFIG, 0, "webMaxRtcBodyBytes out of range");
  }
  return Ok();
}

void RuntimeSettings::restoreDefaults() {
  *this = RuntimeSettings();
}

}  // namespace TFLunaControl

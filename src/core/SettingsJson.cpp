#include "core/ApiJson.h"

#include <string.h>

namespace TFLunaControl {

namespace {

const char* cliVerbosityToStr(uint8_t level) {
  switch (level) {
    case 0:
      return "off";
    case 1:
      return "normal";
    case 2:
      return "verbose";
    default:
      return "unknown";
  }
}

}  // namespace

void populateSettingsJson(JsonDocument& doc, const RuntimeSettings& s) {
  const bool hasPassword = strnlen(s.apPass, sizeof(s.apPass)) > 0;

  doc["sample_interval_ms"] = s.sampleIntervalMs;
  doc["log_daily_enabled"] = s.logDailyEnabled;
  doc["log_all_enabled"] = s.logAllEnabled;
  doc["log_all_max_bytes"] = s.logAllMaxBytes;
  doc["log_flush_ms"] = s.logFlushMs;
  doc["log_io_budget_ms"] = s.logIoBudgetMs;
  doc["log_mount_retry_ms"] = s.logMountRetryMs;
  doc["log_write_retry_backoff_ms"] = s.logWriteRetryBackoffMs;
  doc["log_max_write_retries"] = s.logMaxWriteRetries;
  doc["log_session_name"] = s.logSessionName;
  doc["log_events_max_bytes"] = s.logEventsMaxBytes;
  doc["lidar_service_ms"] = s.lidarServiceMs;
  doc["lidar_min_strength"] = s.lidarMinStrength;
  doc["lidar_max_distance_cm"] = s.lidarMaxDistanceCm;
  doc["lidar_frame_stale_ms"] = s.lidarFrameStaleMs;
  doc["serial_print_interval_ms"] = s.serialPrintIntervalMs;
  doc["cli_verbosity"] = s.cliVerbosity;
  doc["cli_verbosity_name"] = cliVerbosityToStr(s.cliVerbosity);

  doc["i2c_freq_hz"] = s.i2cFreqHz;
  doc["i2c_op_timeout_ms"] = s.i2cOpTimeoutMs;
  doc["i2c_stuck_debounce_ms"] = s.i2cStuckDebounceMs;
  doc["i2c_max_consecutive_failures"] = s.i2cMaxConsecutiveFailures;
  doc["i2c_recovery_backoff_ms"] = s.i2cRecoveryBackoffMs;
  doc["i2c_recovery_backoff_max_ms"] = s.i2cRecoveryBackoffMaxMs;
  doc["i2c_requests_per_tick"] = s.i2cRequestsPerTick;
  doc["i2c_slow_op_threshold_us"] = s.i2cSlowOpThresholdUs;
  doc["i2c_slow_op_degrade_count"] = s.i2cSlowOpDegradeCount;
  doc["i2c_task_heartbeat_timeout_ms"] = s.i2cTaskHeartbeatTimeoutMs;
  doc["i2c_env_poll_ms"] = s.i2cEnvPollMs;
  doc["i2c_rtc_poll_ms"] = s.i2cRtcPollMs;
  doc["i2c_display_poll_ms"] = s.i2cDisplayPollMs;
  doc["i2c_env_conversion_wait_ms"] = s.i2cEnvConversionWaitMs;
  doc["i2c_env_bme_mode"] = s.i2cEnvBmeMode;
  doc["i2c_env_bme_osrs_t"] = s.i2cEnvBmeOsrsT;
  doc["i2c_env_bme_osrs_p"] = s.i2cEnvBmeOsrsP;
  doc["i2c_env_bme_osrs_h"] = s.i2cEnvBmeOsrsH;
  doc["i2c_env_bme_filter"] = s.i2cEnvBmeFilter;
  doc["i2c_env_bme_standby"] = s.i2cEnvBmeStandby;
  doc["i2c_env_sht_mode"] = s.i2cEnvShtMode;
  doc["i2c_env_sht_repeatability"] = s.i2cEnvShtRepeatability;
  doc["i2c_env_sht_periodic_rate"] = s.i2cEnvShtPeriodicRate;
  doc["i2c_env_sht_clock_stretching"] = s.i2cEnvShtClockStretching;
  doc["i2c_env_sht_low_vdd"] = s.i2cEnvShtLowVdd;
  doc["i2c_env_sht_command_delay_ms"] = s.i2cEnvShtCommandDelayMs;
  doc["i2c_env_sht_not_ready_timeout_ms"] = s.i2cEnvShtNotReadyTimeoutMs;
  doc["i2c_env_sht_periodic_fetch_margin_ms"] = s.i2cEnvShtPeriodicFetchMarginMs;
  doc["i2c_env_sht_allow_general_call_reset"] = s.i2cEnvShtAllowGeneralCallReset;
  doc["i2c_env_sht_recover_use_bus_reset"] = s.i2cEnvShtRecoverUseBusReset;
  doc["i2c_env_sht_recover_use_soft_reset"] = s.i2cEnvShtRecoverUseSoftReset;
  doc["i2c_env_sht_recover_use_hard_reset"] = s.i2cEnvShtRecoverUseHardReset;
  doc["i2c_recover_timeout_ms"] = s.i2cRecoverTimeoutMs;
  doc["i2c_max_results_per_tick"] = s.i2cMaxResultsPerTick;
  doc["i2c_task_wait_ms"] = s.i2cTaskWaitMs;
  doc["i2c_health_stale_task_multiplier"] = s.i2cHealthStaleTaskMultiplier;
  doc["i2c_slow_window_ms"] = s.i2cSlowWindowMs;
  doc["i2c_health_recent_window_ms"] = s.i2cHealthRecentWindowMs;
  doc["i2c_env_address"] = s.i2cEnvAddress;
  doc["i2c_rtc_address"] = s.i2cRtcAddress;
  doc["i2c_rtc_backup_mode"] = s.i2cRtcBackupMode;
  doc["i2c_rtc_enable_eeprom_writes"] = s.i2cRtcEnableEepromWrites;
  doc["i2c_rtc_eeprom_timeout_ms"] = s.i2cRtcEepromTimeoutMs;
  doc["i2c_rtc_offline_threshold"] = s.i2cRtcOfflineThreshold;
  doc["i2c_display_address"] = s.i2cDisplayAddress;
  doc["e2_address"] = s.e2Address;
  doc["e2_bit_timeout_us"] = s.e2BitTimeoutUs;
  doc["e2_byte_timeout_us"] = s.e2ByteTimeoutUs;
  doc["e2_clock_low_us"] = s.e2ClockLowUs;
  doc["e2_clock_high_us"] = s.e2ClockHighUs;
  doc["e2_start_hold_us"] = s.e2StartHoldUs;
  doc["e2_stop_hold_us"] = s.e2StopHoldUs;
  doc["e2_write_delay_ms"] = s.e2WriteDelayMs;
  doc["e2_interval_write_delay_ms"] = s.e2IntervalWriteDelayMs;
  doc["e2_offline_threshold"] = s.e2OfflineThreshold;
  doc["e2_recovery_backoff_ms"] = s.e2RecoveryBackoffMs;
  doc["e2_recovery_backoff_max_ms"] = s.e2RecoveryBackoffMaxMs;
  doc["e2_config_interval_ds"] = s.e2ConfigIntervalDs;
  doc["e2_config_co2_interval_factor"] = s.e2ConfigCo2IntervalFactor;
  doc["e2_config_filter"] = s.e2ConfigFilter;
  doc["e2_config_operating_mode"] = s.e2ConfigOperatingMode;
  doc["e2_config_offset_ppm"] = s.e2ConfigOffsetPpm;
  doc["e2_config_gain"] = s.e2ConfigGain;

  doc["wifi_enabled"] = s.wifiEnabled;
  doc["ap_ssid"] = s.apSsid;
  doc["ap_pass_set"] = hasPassword;
  doc["ap_pass_masked"] = hasPassword ? "********" : "";
  doc["ap_pass_update_mode"] = "write_only";

  doc["ap_auto_off_ms"] = s.apAutoOffMs;
  doc["command_drain_per_tick"] = s.commandDrainPerTick;
  doc["command_queue_degraded_window_ms"] = s.commandQueueDegradedWindowMs;
  doc["command_queue_degraded_depth_threshold"] = s.commandQueueDegradedDepthThreshold;
  doc["main_tick_slow_threshold_us"] = s.mainTickSlowThresholdUs;
  doc["web_overrun_threshold_us"] = s.webOverrunThresholdUs;
  doc["web_overrun_burst_threshold"] = s.webOverrunBurstThreshold;
  doc["web_overrun_throttle_ms"] = s.webOverrunThrottleMs;
  doc["led_health_init_ms"] = s.ledHealthInitMs;
  doc["led_health_debounce_ms"] = s.ledHealthDebounceMs;
  doc["ap_start_retry_backoff_ms"] = s.apStartRetryBackoffMs;
  doc["web_max_settings_body_bytes"] = s.webMaxSettingsBodyBytes;
  doc["web_max_rtc_body_bytes"] = s.webMaxRtcBodyBytes;
}

}  // namespace TFLunaControl

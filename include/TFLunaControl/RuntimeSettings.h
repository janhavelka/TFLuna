/**
 * @file RuntimeSettings.h
 * @brief Runtime settings for TFLunaControl.
 */

#pragma once

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include "TFLunaControl/Status.h"

namespace TFLunaControl {

/**
 * @brief User-configurable runtime settings (optionally persisted in NVS).
 *
 * Runtime settings are separate from boot-time hardware settings in HardwareSettings.h.
 * Validation/default implementation is in src/settings/RuntimeSettings.cpp.
 * Persistence backend is in src/settings/SettingsStore.h/.cpp.
 */
struct RuntimeSettings {
  static constexpr uint32_t MIN_SAMPLE_INTERVAL_MS = 10;
  // PeriodicTimer uses signed wrap-safe comparisons; keep interval within INT32 range.
  static constexpr uint32_t MAX_SAMPLE_INTERVAL_MS = static_cast<uint32_t>(INT32_MAX);

  static constexpr uint32_t MIN_LOG_ALL_MAX_BYTES = 1;
  static constexpr uint32_t MAX_LOG_ALL_MAX_BYTES = 4000000000UL;

  static constexpr uint32_t MIN_LOG_FLUSH_MS = 100;
  static constexpr uint32_t MAX_LOG_FLUSH_MS = 60000;

  static constexpr uint32_t MIN_LOG_IO_BUDGET_MS = 1;
  static constexpr uint32_t MAX_LOG_IO_BUDGET_MS = 100;

  static constexpr uint32_t MIN_LOG_MOUNT_RETRY_MS = 100;
  static constexpr uint32_t MAX_LOG_MOUNT_RETRY_MS = 60000;

  static constexpr uint32_t MIN_LOG_WRITE_RETRY_BACKOFF_MS = 10;
  static constexpr uint32_t MAX_LOG_WRITE_RETRY_BACKOFF_MS = 60000;

  static constexpr uint8_t MIN_LOG_MAX_WRITE_RETRIES = 1;
  static constexpr uint8_t MAX_LOG_MAX_WRITE_RETRIES = 10;

  static constexpr size_t LOG_SESSION_NAME_BYTES = 25;
  static constexpr uint8_t MIN_LOG_SESSION_NAME_LEN = 1;
  static constexpr uint8_t MAX_LOG_SESSION_NAME_LEN = 24;

  static constexpr uint32_t MIN_LOG_EVENTS_MAX_BYTES = 1024;
  static constexpr uint32_t MAX_LOG_EVENTS_MAX_BYTES = 52428800UL;

  static constexpr uint32_t MIN_AP_AUTO_OFF_MS = 1000;
  static constexpr uint32_t MAX_AP_AUTO_OFF_MS = 86400000UL;

  static constexpr float MIN_CO2_PPM = 0.0f;
  static constexpr float MAX_CO2_PPM = 10000.0f;

  static constexpr uint32_t MIN_DWELL_MS = 1;
  static constexpr uint32_t MAX_DWELL_MS = 86400000UL;

  static constexpr uint8_t MIN_COMMANDS_PER_TICK = 1;
  static constexpr uint8_t MAX_COMMANDS_PER_TICK = 8;

  static constexpr uint32_t MIN_COMMAND_QUEUE_DEGRADED_WINDOW_MS = 1000;
  static constexpr uint32_t MAX_COMMAND_QUEUE_DEGRADED_WINDOW_MS = 600000;

  static constexpr uint8_t MIN_COMMAND_QUEUE_DEGRADED_DEPTH = 1;
  static constexpr uint8_t MAX_COMMAND_QUEUE_DEGRADED_DEPTH = 8;

  static constexpr uint32_t MIN_OUTPUT_DATA_STALE_MIN_MS = 1000;
  static constexpr uint32_t MAX_OUTPUT_DATA_STALE_MIN_MS = 86400000UL;
  static constexpr uint8_t OUTPUT_CHANNEL_DISABLED = 0xFFU;
  static constexpr uint8_t MIN_OUTPUT_CHANNEL_INDEX = 0U;
  static constexpr uint8_t MAX_OUTPUT_CHANNEL_INDEX = 3U;
  static constexpr uint8_t MIN_OUTPUT_FAN_PWM_PERCENT = 0U;
  static constexpr uint8_t MAX_OUTPUT_FAN_PWM_PERCENT = 100U;
  static constexpr uint8_t OUTPUT_FAN_PWM_EFFECTIVE_MIN_PERCENT = 30U;
  static constexpr uint32_t OUTPUT_FAN_PERIOD_DISABLED_MS = 0U;
  static constexpr uint32_t MIN_OUTPUT_FAN_PERIOD_MS = 100U;
  static constexpr uint32_t MAX_OUTPUT_FAN_PERIOD_MS = 86400000UL;
  static constexpr uint32_t MIN_OUTPUT_FAN_ON_MS = 0U;
  static constexpr uint32_t MAX_OUTPUT_FAN_ON_MS = 86400000UL;

  static constexpr uint32_t MIN_MAIN_TICK_SLOW_THRESHOLD_US = 100;
  static constexpr uint32_t MAX_MAIN_TICK_SLOW_THRESHOLD_US = 500000;

  static constexpr uint32_t MIN_WEB_OVERRUN_THRESHOLD_US = 100;
  static constexpr uint32_t MAX_WEB_OVERRUN_THRESHOLD_US = 500000;

  static constexpr uint8_t MIN_WEB_OVERRUN_BURST_THRESHOLD = 1;
  static constexpr uint8_t MAX_WEB_OVERRUN_BURST_THRESHOLD = 20;

  static constexpr uint32_t MIN_WEB_OVERRUN_THROTTLE_MS = 100;
  static constexpr uint32_t MAX_WEB_OVERRUN_THROTTLE_MS = 60000;

  static constexpr uint32_t MIN_LED_HEALTH_INIT_MS = 0;
  static constexpr uint32_t MAX_LED_HEALTH_INIT_MS = 60000;

  static constexpr uint32_t MIN_LED_HEALTH_DEBOUNCE_MS = 0;
  static constexpr uint32_t MAX_LED_HEALTH_DEBOUNCE_MS = 60000;

  static constexpr uint32_t MIN_AP_START_RETRY_BACKOFF_MS = 100;
  static constexpr uint32_t MAX_AP_START_RETRY_BACKOFF_MS = 60000;

  static constexpr uint32_t MIN_I2C_FREQ_HZ = 100000;
  static constexpr uint32_t MAX_I2C_FREQ_HZ = 400000;

  static constexpr uint32_t MIN_I2C_TIMEOUT_MS = 5;
  static constexpr uint32_t MAX_I2C_TIMEOUT_MS = 30;

  static constexpr uint8_t MIN_I2C_STUCK_DEBOUNCE_MS = 2;
  static constexpr uint8_t MAX_I2C_STUCK_DEBOUNCE_MS = 20;

  static constexpr uint8_t MIN_I2C_CONSECUTIVE_FAILURES = 1;
  static constexpr uint8_t MAX_I2C_CONSECUTIVE_FAILURES = 20;

  static constexpr uint32_t MIN_I2C_RECOVERY_BACKOFF_MS = 100;
  static constexpr uint32_t MAX_I2C_RECOVERY_BACKOFF_MS = 10000;

  static constexpr uint32_t MIN_I2C_RECOVERY_BACKOFF_MAX_MS = 1000;
  static constexpr uint32_t MAX_I2C_RECOVERY_BACKOFF_MAX_MS = 60000;

  static constexpr uint8_t MIN_I2C_REQUESTS_PER_TICK = 1;
  static constexpr uint8_t MAX_I2C_REQUESTS_PER_TICK = 8;

  static constexpr uint32_t MIN_I2C_SLOW_OP_THRESHOLD_US = 1000;
  static constexpr uint32_t MAX_I2C_SLOW_OP_THRESHOLD_US = 500000;

  static constexpr uint8_t MIN_I2C_SLOW_OP_DEGRADE_COUNT = 1;
  static constexpr uint8_t MAX_I2C_SLOW_OP_DEGRADE_COUNT = 100;

  static constexpr uint32_t MIN_I2C_HEARTBEAT_TIMEOUT_MS = 100;
  static constexpr uint32_t MAX_I2C_HEARTBEAT_TIMEOUT_MS = 10000;

  static constexpr uint32_t MIN_I2C_POLL_MS = 50;
  static constexpr uint32_t MAX_I2C_POLL_MS = 60000;

  static constexpr uint32_t MIN_I2C_ENV_CONVERSION_WAIT_MS = 1;
  static constexpr uint32_t MAX_I2C_ENV_CONVERSION_WAIT_MS = 1000;

  static constexpr uint8_t MIN_I2C_ENV_BME_MODE = 0;
  static constexpr uint8_t MAX_I2C_ENV_BME_MODE = 3;

  static constexpr uint8_t MIN_I2C_ENV_BME_OVERSAMPLING = 0;
  static constexpr uint8_t MAX_I2C_ENV_BME_OVERSAMPLING = 5;

  static constexpr uint8_t MIN_I2C_ENV_BME_FILTER = 0;
  static constexpr uint8_t MAX_I2C_ENV_BME_FILTER = 4;

  static constexpr uint8_t MIN_I2C_ENV_BME_STANDBY = 0;
  static constexpr uint8_t MAX_I2C_ENV_BME_STANDBY = 7;

  static constexpr uint8_t MIN_I2C_ENV_SHT_MODE = 0;
  static constexpr uint8_t MAX_I2C_ENV_SHT_MODE = 2;

  static constexpr uint8_t MIN_I2C_ENV_SHT_REPEATABILITY = 0;
  static constexpr uint8_t MAX_I2C_ENV_SHT_REPEATABILITY = 2;

  static constexpr uint8_t MIN_I2C_ENV_SHT_PERIODIC_RATE = 0;
  static constexpr uint8_t MAX_I2C_ENV_SHT_PERIODIC_RATE = 4;

  static constexpr uint8_t MIN_I2C_ENV_SHT_CLOCK_STRETCHING = 0;
  static constexpr uint8_t MAX_I2C_ENV_SHT_CLOCK_STRETCHING = 1;

  static constexpr uint16_t MIN_I2C_ENV_SHT_COMMAND_DELAY_MS = 1;
  static constexpr uint16_t MAX_I2C_ENV_SHT_COMMAND_DELAY_MS = 1000;

  static constexpr uint32_t MIN_I2C_ENV_SHT_NOT_READY_TIMEOUT_MS = 0;
  static constexpr uint32_t MAX_I2C_ENV_SHT_NOT_READY_TIMEOUT_MS = 600000;

  static constexpr uint32_t MIN_I2C_ENV_SHT_PERIODIC_FETCH_MARGIN_MS = 0;
  static constexpr uint32_t MAX_I2C_ENV_SHT_PERIODIC_FETCH_MARGIN_MS = 60000;

  static constexpr uint32_t MIN_I2C_RECOVER_TIMEOUT_MS = 10;
  static constexpr uint32_t MAX_I2C_RECOVER_TIMEOUT_MS = 5000;

  static constexpr uint8_t MIN_I2C_MAX_RESULTS_PER_TICK = 1;
  static constexpr uint8_t MAX_I2C_MAX_RESULTS_PER_TICK = 16;

  static constexpr uint32_t MIN_I2C_TASK_WAIT_MS = 1;
  static constexpr uint32_t MAX_I2C_TASK_WAIT_MS = 1000;

  static constexpr uint8_t MIN_I2C_HEALTH_STALE_TASK_MULTIPLIER = 1;
  static constexpr uint8_t MAX_I2C_HEALTH_STALE_TASK_MULTIPLIER = 10;

  static constexpr uint32_t MIN_I2C_SLOW_WINDOW_MS = 1000;
  static constexpr uint32_t MAX_I2C_SLOW_WINDOW_MS = 600000;

  static constexpr uint32_t MIN_I2C_HEALTH_RECENT_WINDOW_MS = 1000;
  static constexpr uint32_t MAX_I2C_HEALTH_RECENT_WINDOW_MS = 600000;

  static constexpr uint8_t MIN_I2C_ADDR = 1;
  static constexpr uint8_t MAX_I2C_ADDR = 0x7F;

  static constexpr uint8_t MIN_E2_ADDR = 0;
  static constexpr uint8_t MAX_E2_ADDR = 7;

  // EE871 driver currently enforces >=100us clock low/high in begin().
  static constexpr uint16_t MIN_E2_CLOCK_US = 100;
  static constexpr uint16_t MAX_E2_CLOCK_US = 5000;

  static constexpr uint16_t MIN_E2_HOLD_US = 4;
  static constexpr uint16_t MAX_E2_HOLD_US = 5000;

  static constexpr uint32_t MIN_E2_BIT_TIMEOUT_US = 100;
  static constexpr uint32_t MAX_E2_BIT_TIMEOUT_US = 25000;

  static constexpr uint32_t MIN_E2_BYTE_TIMEOUT_US = 100;
  static constexpr uint32_t MAX_E2_BYTE_TIMEOUT_US = 50000;

  static constexpr uint32_t MIN_E2_WRITE_DELAY_MS = 10;
  static constexpr uint32_t MAX_E2_WRITE_DELAY_MS = 5000;

  static constexpr uint32_t MIN_E2_INTERVAL_WRITE_DELAY_MS = 10;
  static constexpr uint32_t MAX_E2_INTERVAL_WRITE_DELAY_MS = 10000;

  static constexpr uint8_t MIN_E2_OFFLINE_THRESHOLD = 1;
  static constexpr uint8_t MAX_E2_OFFLINE_THRESHOLD = 20;

  static constexpr uint32_t MIN_E2_RECOVERY_BACKOFF_MS = 100;
  static constexpr uint32_t MAX_E2_RECOVERY_BACKOFF_MS = 10000;

  static constexpr uint32_t MIN_E2_RECOVERY_BACKOFF_MAX_MS = 1000;
  static constexpr uint32_t MAX_E2_RECOVERY_BACKOFF_MAX_MS = 60000;

  static constexpr uint16_t MIN_E2_CONFIG_INTERVAL_DS = 150;
  static constexpr uint16_t MAX_E2_CONFIG_INTERVAL_DS = 36000;

  static constexpr int8_t MIN_E2_CONFIG_INTERVAL_FACTOR = -127;
  static constexpr int8_t MAX_E2_CONFIG_INTERVAL_FACTOR = 127;

  static constexpr uint8_t MIN_E2_CONFIG_FILTER = 0;
  static constexpr uint8_t MAX_E2_CONFIG_FILTER = 0xFE;

  static constexpr uint8_t MIN_E2_CONFIG_OPERATING_MODE = 0;
  static constexpr uint8_t MAX_E2_CONFIG_OPERATING_MODE = 3;

  static constexpr int16_t MIN_E2_CONFIG_OFFSET_PPM = -10000;
  static constexpr int16_t MAX_E2_CONFIG_OFFSET_PPM = 10000;

  static constexpr uint16_t MIN_E2_CONFIG_GAIN = 1;
  static constexpr uint16_t MAX_E2_CONFIG_GAIN = 0xFFFE;

  static constexpr int8_t E2_CONFIG_INTERVAL_FACTOR_DISABLED = static_cast<int8_t>(-128);
  static constexpr uint8_t E2_CONFIG_FILTER_DISABLED = 0xFFU;
  static constexpr uint8_t E2_CONFIG_OPERATING_MODE_DISABLED = 0xFFU;
  static constexpr int16_t E2_CONFIG_OFFSET_PPM_DISABLED = static_cast<int16_t>(-32768);
  static constexpr uint16_t E2_CONFIG_GAIN_DISABLED = 0xFFFFU;

  static constexpr uint8_t MIN_I2C_RTC_BACKUP_MODE = 0;
  static constexpr uint8_t MAX_I2C_RTC_BACKUP_MODE = 2;

  static constexpr uint32_t MIN_I2C_RTC_EEPROM_TIMEOUT_MS = 10;
  static constexpr uint32_t MAX_I2C_RTC_EEPROM_TIMEOUT_MS = 5000;

  static constexpr uint8_t MIN_I2C_RTC_OFFLINE_THRESHOLD = 1;
  static constexpr uint8_t MAX_I2C_RTC_OFFLINE_THRESHOLD = 20;

  static constexpr uint16_t MIN_WEB_MAX_SETTINGS_BODY_BYTES = 128;
  static constexpr uint16_t MAX_WEB_MAX_SETTINGS_BODY_BYTES = 4096;

  static constexpr uint16_t MIN_WEB_MAX_RTC_BODY_BYTES = 64;
  static constexpr uint16_t MAX_WEB_MAX_RTC_BODY_BYTES = 1024;

  static constexpr uint32_t MIN_LIDAR_SERVICE_MS = 1;
  static constexpr uint32_t MAX_LIDAR_SERVICE_MS = 1000;

  static constexpr uint16_t MIN_LIDAR_SIGNAL_STRENGTH = 0;
  static constexpr uint16_t MAX_LIDAR_SIGNAL_STRENGTH = 65535;

  static constexpr uint16_t MIN_LIDAR_MAX_DISTANCE_CM = 1;
  static constexpr uint16_t MAX_LIDAR_MAX_DISTANCE_CM = 1200;

  static constexpr uint32_t MIN_LIDAR_FRAME_STALE_MS = 100;
  static constexpr uint32_t MAX_LIDAR_FRAME_STALE_MS = 600000;

  static constexpr uint32_t MIN_SERIAL_PRINT_INTERVAL_MS = 100;
  static constexpr uint32_t MAX_SERIAL_PRINT_INTERVAL_MS = 600000;
  static constexpr uint8_t MIN_CLI_VERBOSITY = 0;
  static constexpr uint8_t MAX_CLI_VERBOSITY = 2;

  static constexpr float MIN_TEMP_C = -40.0f;
  static constexpr float MAX_TEMP_C = 125.0f;
  static constexpr float MIN_RH_PCT = 0.0f;
  static constexpr float MAX_RH_PCT = 100.0f;

  /// @brief Sample interval in milliseconds.
  uint32_t sampleIntervalMs = 100;

  /// @brief Enable daily CSV logging.
  bool logDailyEnabled = false;

  /// @brief Enable all-time CSV logging.
  bool logAllEnabled = false;

  /// @brief Maximum size for logs/all.csv in bytes.
  uint32_t logAllMaxBytes = 3500000000UL;

  /// @brief Log flush interval in milliseconds.
  uint32_t logFlushMs = 2000;

  /// @brief SD logger per-tick I/O budget in milliseconds.
  uint32_t logIoBudgetMs = 10;

  /// @brief Retry interval for remount attempts while SD is unavailable.
  uint32_t logMountRetryMs = 5000;

  /// @brief Backoff between failed SD write retries.
  uint32_t logWriteRetryBackoffMs = 1000;

  /// @brief Maximum retries before dropping a queued SD write.
  uint8_t logMaxWriteRetries = 3;

  /// @brief Base name used for session folder creation (`<name>_00001`).
  char logSessionName[LOG_SESSION_NAME_BYTES] = "tfluna";

  /// @brief Maximum size for events.csv before rollover.
  uint32_t logEventsMaxBytes = 1048576;

  /// @brief TF-Luna UART service cadence in milliseconds.
  uint32_t lidarServiceMs = 10;

  /// @brief Minimum accepted TF-Luna signal strength.
  uint16_t lidarMinStrength = 100;

  /// @brief Maximum accepted TF-Luna distance in centimeters.
  uint16_t lidarMaxDistanceCm = 800;

  /// @brief Latest-frame stale threshold for diagnostics and display.
  uint32_t lidarFrameStaleMs = 1500;

  /// @brief Serial diagnostic summary interval in milliseconds.
  uint32_t serialPrintIntervalMs = 5000;

  /// @brief CLI detail level (0=compact, 1=normal, 2=verbose).
  uint8_t cliVerbosity = 1;

  /// @brief I2C bus frequency in Hz.
  uint32_t i2cFreqHz = 400000;

  /// @brief I2C per-operation timeout in milliseconds.
  uint32_t i2cOpTimeoutMs = 20;

  /// @brief SDA/SCL low debounce for physical stuck detection (ms).
  uint8_t i2cStuckDebounceMs = 3;

  /// @brief Max consecutive I2C bus failures before forced recovery.
  uint8_t i2cMaxConsecutiveFailures = 3;

  /// @brief Backoff between I2C recoveries in milliseconds.
  uint32_t i2cRecoveryBackoffMs = 1000;

  /// @brief Maximum recovery backoff in milliseconds.
  uint32_t i2cRecoveryBackoffMaxMs = 30000;

  /// @brief Max new I2C requests generated by orchestrator per tick.
  uint8_t i2cRequestsPerTick = 2;

  /// @brief Slow-op threshold (us) used for I2C health degradation.
  uint32_t i2cSlowOpThresholdUs = 50000;

  /// @brief Number of slow operations within a minute before degrading health.
  uint8_t i2cSlowOpDegradeCount = 3;

  /// @brief Heartbeat timeout for I2C task liveness monitoring.
  uint32_t i2cTaskHeartbeatTimeoutMs = 5000;

  /// @brief Poll interval for ENV one-shot start in milliseconds.
  uint32_t i2cEnvPollMs = 5000;

  /// @brief Poll interval for RTC read in milliseconds.
  uint32_t i2cRtcPollMs = 1000;

  /// @brief Poll interval for future I2C display refresh in milliseconds.
  uint32_t i2cDisplayPollMs = 250;

  /// @brief Wait between ENV one-shot trigger and read phases.
  uint32_t i2cEnvConversionWaitMs = 30;

  /// @brief BME280 mode (0=sleep, 1=forced, 3=normal).
  uint8_t i2cEnvBmeMode = 1;

  /// @brief BME280 temperature oversampling (0..5).
  uint8_t i2cEnvBmeOsrsT = 1;

  /// @brief BME280 pressure oversampling (0..5).
  uint8_t i2cEnvBmeOsrsP = 1;

  /// @brief BME280 humidity oversampling (0..5).
  uint8_t i2cEnvBmeOsrsH = 1;

  /// @brief BME280 IIR filter coefficient (0..4).
  uint8_t i2cEnvBmeFilter = 0;

  /// @brief BME280 standby index (0..7) for normal mode cadence.
  uint8_t i2cEnvBmeStandby = 2;

  /// @brief SHT3x mode (0=single-shot, 1=periodic, 2=art).
  uint8_t i2cEnvShtMode = 0;

  /// @brief SHT3x repeatability (0=low,1=medium,2=high).
  uint8_t i2cEnvShtRepeatability = 2;

  /// @brief SHT3x periodic rate index (0..4).
  uint8_t i2cEnvShtPeriodicRate = 1;

  /// @brief SHT3x clock stretching mode (0=disabled,1=enabled).
  uint8_t i2cEnvShtClockStretching = 0;

  /// @brief SHT3x low-VDD timing mode.
  bool i2cEnvShtLowVdd = false;

  /// @brief SHT3x command spacing in milliseconds.
  uint16_t i2cEnvShtCommandDelayMs = 1;

  /// @brief SHT3x periodic not-ready timeout in milliseconds (0=disabled).
  uint32_t i2cEnvShtNotReadyTimeoutMs = 0;

  /// @brief SHT3x periodic fetch margin in milliseconds.
  uint32_t i2cEnvShtPeriodicFetchMarginMs = 0;

  /// @brief Allow SHT3x recover path to use bus-wide general call reset.
  bool i2cEnvShtAllowGeneralCallReset = false;

  /// @brief Use SHT3x bus-reset stage in recover.
  bool i2cEnvShtRecoverUseBusReset = true;

  /// @brief Use SHT3x soft-reset stage in recover.
  bool i2cEnvShtRecoverUseSoftReset = true;

  /// @brief Use SHT3x hard-reset callback stage in recover.
  bool i2cEnvShtRecoverUseHardReset = true;

  /// @brief Timeout used for explicit I2C recover requests.
  uint32_t i2cRecoverTimeoutMs = 250;

  /// @brief Maximum number of I2C results processed per orchestrator tick.
  uint8_t i2cMaxResultsPerTick = 8;

  /// @brief I2C task queue wait timeout (RTOS task loop).
  uint32_t i2cTaskWaitMs = 20;

  /// @brief Multiplier for heartbeat timeout before declaring task fault.
  uint8_t i2cHealthStaleTaskMultiplier = 2;

  /// @brief Rolling slow-op metrics window.
  uint32_t i2cSlowWindowMs = 60000;

  /// @brief Recent-window for queue issue health degradation.
  uint32_t i2cHealthRecentWindowMs = 60000;

  /// @brief ENV sensor I2C address.
  uint8_t i2cEnvAddress = 0x76;

  /// @brief RTC sensor I2C address.
  uint8_t i2cRtcAddress = 0x51;

  /// @brief RTC backup switch mode (0=off,1=level,2=direct).
  uint8_t i2cRtcBackupMode = 1;

  /// @brief Allow RTC to persist settings into EEPROM.
  bool i2cRtcEnableEepromWrites = true;

  /// @brief RTC EEPROM write timeout in milliseconds.
  uint32_t i2cRtcEepromTimeoutMs = 100;

  /// @brief RTC consecutive-failure threshold before offline.
  uint8_t i2cRtcOfflineThreshold = 3;

  /// @brief Future display I2C address.
  uint8_t i2cDisplayAddress = 0x3C;

  /// @brief EE871 E2 bus address (0-7).
  uint8_t e2Address = 0;

  /// @brief EE871 E2 per-bit timeout in microseconds.
  uint32_t e2BitTimeoutUs = 2000;

  /// @brief EE871 E2 per-byte timeout in microseconds.
  uint32_t e2ByteTimeoutUs = 6000;

  /// @brief EE871 E2 clock-low timing in microseconds.
  uint16_t e2ClockLowUs = 100;

  /// @brief EE871 E2 clock-high timing in microseconds.
  uint16_t e2ClockHighUs = 100;

  /// @brief EE871 E2 START hold timing in microseconds.
  uint16_t e2StartHoldUs = 100;

  /// @brief EE871 E2 STOP hold timing in microseconds.
  uint16_t e2StopHoldUs = 100;

  /// @brief EE871 E2 flash-write settle delay in milliseconds.
  uint32_t e2WriteDelayMs = 50;

  /// @brief EE871 E2 interval-write settle delay in milliseconds.
  uint32_t e2IntervalWriteDelayMs = 80;

  /// @brief EE871 offline threshold (consecutive failures).
  uint8_t e2OfflineThreshold = 3;

  /// @brief Backoff between EE871 recovery attempts in milliseconds.
  uint32_t e2RecoveryBackoffMs = 1000;

  /// @brief Maximum EE871 recovery backoff in milliseconds.
  uint32_t e2RecoveryBackoffMaxMs = 30000;

  /// @brief Managed EE871 global interval (0.1s units, 0=do not manage).
  uint16_t e2ConfigIntervalDs = 0;

  /// @brief Managed EE871 CO2 interval factor (-128=do not manage).
  int8_t e2ConfigCo2IntervalFactor = E2_CONFIG_INTERVAL_FACTOR_DISABLED;

  /// @brief Managed EE871 CO2 filter (255=do not manage).
  uint8_t e2ConfigFilter = E2_CONFIG_FILTER_DISABLED;

  /// @brief Managed EE871 operating mode bits (255=do not manage).
  uint8_t e2ConfigOperatingMode = E2_CONFIG_OPERATING_MODE_DISABLED;

  /// @brief Managed EE871 CO2 offset in ppm (-32768=do not manage).
  int16_t e2ConfigOffsetPpm = E2_CONFIG_OFFSET_PPM_DISABLED;

  /// @brief Managed EE871 CO2 gain raw value (65535=do not manage).
  uint16_t e2ConfigGain = E2_CONFIG_GAIN_DISABLED;

  /// @brief Enable SoftAP.
  bool wifiEnabled = false;

  /// @brief SoftAP SSID (null-terminated).
  char apSsid[32] = "TFLuna-XXXX";

  /// @brief SoftAP password (null-terminated).
  char apPass[64] = "tflunactrl";

  /// @brief Auto-off timeout for SoftAP when idle (ms).
  uint32_t apAutoOffMs = 60000;

  /// @brief Enable output control.
  bool outputsEnabled = false;

  /// @brief Output control input source (0=CO2, 1=TEMP, 2=RH).
  uint8_t outputSource = 0;

  /// @brief Channel index used for valve actuation (0..3, 255=disabled).
  uint8_t outputValveChannel = 1;

  /// @brief Semantics helper: true when powered valve state means "closed".
  bool outputValvePoweredClosed = true;

  /// @brief Channel index used for fan actuation (0..3, 255=disabled).
  uint8_t outputFanChannel = 0;

  /// @brief Fan power setting in user percent (0=off, 1..100 mapped to effective PWM).
  uint8_t outputFanPwmPercent = 0;

  /// @brief Fan interval period in milliseconds (0 = continuous, no interval gating).
  uint32_t outputFanPeriodMs = OUTPUT_FAN_PERIOD_DISABLED_MS;

  /// @brief Fan ON window inside interval period in milliseconds.
  /// Clamped to `outputFanPeriodMs` when interval gating is enabled.
  uint32_t outputFanOnMs = 0;

  /// @brief CO2 threshold to turn outputs ON (ppm).
  float co2OnPpm = 1200.0f;

  /// @brief CO2 threshold to turn outputs OFF (ppm).
  float co2OffPpm = 900.0f;

  /// @brief Temperature threshold to turn outputs ON (Â°C).
  float tempOnC = 28.0f;

  /// @brief Temperature threshold to turn outputs OFF (Â°C).
  float tempOffC = 24.0f;
  /// @brief Relative humidity threshold to turn outputs ON (%).
  float rhOnPct = 80.0f;
  /// @brief Relative humidity threshold to turn outputs OFF (%).
  float rhOffPct = 70.0f;

  /// @brief Minimum ON time in milliseconds.
  uint32_t minOnMs = 30000;

  /// @brief Minimum OFF time in milliseconds.
  uint32_t minOffMs = 30000;

  /// @brief Max queued command mutations processed in one firmware tick.
  uint8_t commandDrainPerTick = 4;

  /// @brief Queue health degraded window for overflow/depth checks.
  uint32_t commandQueueDegradedWindowMs = 60000;

  /// @brief Queue depth threshold considered degraded.
  uint8_t commandQueueDegradedDepthThreshold = 6;

  /// @brief Minimum acceptable "fresh" CO2 sample age for output control.
  uint32_t outputDataStaleMinMs = 300000;

  /// @brief Main loop runtime threshold used for slow-tick telemetry.
  uint32_t mainTickSlowThresholdUs = 5000;

  /// @brief Per-phase overrun threshold for web tick (us).
  uint32_t webOverrunThresholdUs = 5000;

  /// @brief Consecutive web overruns before throttle engages.
  uint8_t webOverrunBurstThreshold = 3;

  /// @brief How long web ticks stay throttled after burst (ms).
  uint32_t webOverrunThrottleMs = 5000;

  /// @brief Health LED init grace period after boot.
  uint32_t ledHealthInitMs = 1500;

  /// @brief Debounce interval for health LED severity transitions.
  uint32_t ledHealthDebounceMs = 1500;

  /// @brief Retry backoff for AP start attempts when AP is requested.
  uint32_t apStartRetryBackoffMs = 1000;

  /// @brief Maximum JSON body for `/api/settings` POST.
  uint16_t webMaxSettingsBodyBytes = 512;

  /// @brief Maximum JSON body for `/api/rtc/set` POST.
  uint16_t webMaxRtcBodyBytes = 128;

  /// @brief Validate settings for safe ranges.
  /// @return Ok if valid, INVALID_CONFIG otherwise.
  Status validate() const;

  /// @brief Restore default values.
  void restoreDefaults();
};

}  // namespace TFLunaControl

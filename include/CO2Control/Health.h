/**
 * @file Health.h
 * @brief Health model for CO2Control devices.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "CO2Control/Status.h"
#include "CO2Control/Types.h"

namespace CO2Control {

/// @brief Device health state.
enum class HealthState : uint8_t {
  UNKNOWN = 0,
  OK,
  DEGRADED,
  FAULT
};

/// @brief Compare two health states and return the worse one.
constexpr HealthState worstOf(HealthState a, HealthState b) {
  return (static_cast<uint8_t>(a) >= static_cast<uint8_t>(b)) ? a : b;
}

/**
 * @brief Per-device health and last error details.
 */
struct DeviceStatus {
  DeviceId id = DeviceId::SYSTEM;  ///< Device identifier
  const char* name = "";           ///< Static device name
  HealthState health = HealthState::UNKNOWN;  ///< Current health state
  Status lastStatus = Ok();        ///< Last reported status
  uint32_t lastOkMs = 0;           ///< Timestamp of last OK status (ms)
  uint32_t lastErrorMs = 0;        ///< Timestamp of last error (ms)
  uint32_t lastActivityMs = 0;     ///< Timestamp of last activity (ms)
  uint32_t errorCount = 0;         ///< Total error count
  bool optional = false;           ///< Optional device — excluded from system health aggregation
};

/**
 * @brief System-wide status summary.
 */
struct SystemStatus {
  HealthState health = HealthState::UNKNOWN;  ///< Worst device health
  Status lastStatus = Ok();                   ///< Last system status
  uint32_t uptimeMs = 0;                      ///< Uptime (ms)
  uint32_t tickLastDurationUs = 0;            ///< Last main tick runtime
  uint32_t tickMaxDurationUs = 0;             ///< Max observed main tick runtime
  uint32_t tickMeanDurationUs = 0;            ///< Mean main tick runtime (approx)
  uint32_t tickSlowCount = 0;                 ///< Number of ticks exceeding slow threshold
  uint32_t tickLastSlowMs = 0;                ///< Timestamp of last slow tick
  uint32_t tickPhaseUsCmd = 0;                ///< Command-processing phase runtime inside tick (us)
  uint32_t tickPhaseUsCo2 = 0;                ///< CO2 phase runtime inside tick (us)
  uint32_t tickPhaseUsI2c = 0;                ///< I2C orchestration phase runtime inside tick (us)
  uint32_t tickPhaseUsSd = 0;                 ///< SD phase runtime inside tick (us)
  uint32_t tickPhaseUsIo = 0;                 ///< Output/Web orchestration phase runtime inside tick (us)
  uint32_t tickPhaseUsStatus = 0;             ///< Status phase runtime inside tick (us)
  uint32_t tickPhaseUsLed = 0;                ///< LED phase runtime inside tick (us)
  uint32_t tickMaxAtMs = 0;                   ///< Uptime timestamp when max tick was observed
  uint32_t tickMaxPhaseUsCmd = 0;             ///< Max observed command phase runtime (us)
  uint32_t tickMaxPhaseUsCo2 = 0;             ///< Max observed CO2 phase runtime (us)
  uint32_t tickMaxPhaseUsI2c = 0;             ///< Max observed I2C phase runtime (us)
  uint32_t tickMaxPhaseUsSd = 0;              ///< Max observed SD phase runtime (us)
  uint32_t tickMaxPhaseUsIo = 0;              ///< Max observed output/web phase runtime (us)
  uint32_t tickMaxPhaseUsStatus = 0;          ///< Max observed status phase runtime (us)
  uint32_t tickMaxPhaseUsLed = 0;             ///< Max observed LED phase runtime (us)
  uint32_t tickSlowDomCmdCount = 0;           ///< Slow ticks where command phase was dominant
  uint32_t tickSlowDomCo2Count = 0;           ///< Slow ticks where CO2 phase was dominant
  uint32_t tickSlowDomI2cCount = 0;           ///< Slow ticks where I2C phase was dominant
  uint32_t tickSlowDomSdCount = 0;            ///< Slow ticks where SD phase was dominant
  uint32_t tickSlowDomIoCount = 0;            ///< Slow ticks where output/web phase was dominant
  uint32_t tickSlowDomStatusCount = 0;        ///< Slow ticks where status phase was dominant
  uint32_t tickSlowDomLedCount = 0;           ///< Slow ticks where LED phase was dominant
  uint32_t tickSlowDomOtherCount = 0;         ///< Slow ticks dominated by non-instrumented work
  bool webThrottled = false;                  ///< True when web tick is throttled by overrun guard
  uint32_t webSkipCount = 0;                  ///< Web ticks skipped due to overrun throttle
  uint8_t webOverrunBurst = 0;                ///< Current web overrun burst counter
  uint32_t lastSampleMs = 0;                  ///< Last sample time (ms)
  uint32_t sampleCount = 0;                   ///< Total samples collected
  bool sdMounted = false;                     ///< SD mount state
  bool sdInfoValid = false;                   ///< SD info snapshot validity
  bool sdUsageValid = false;                  ///< SD used/free fields validity
  uint64_t sdFsCapacityBytes = 0;             ///< Filesystem capacity in bytes
  uint64_t sdFsUsedBytes = 0;                 ///< Filesystem used bytes
  uint64_t sdFsFreeBytes = 0;                 ///< Filesystem free bytes
  uint64_t sdCardCapacityBytes = 0;           ///< Raw card capacity in bytes
  uint32_t sdInfoLastUpdateMs = 0;            ///< Last SD info refresh timestamp
  uint32_t sdInfoAgeMs = 0;                   ///< Age since SD info refresh
  uint8_t sdFsType = 0;                       ///< AsyncSD FsType enum value
  uint8_t sdCardType = 0;                     ///< AsyncSD CardType enum value
  bool logDailyOk = false;                    ///< Daily logging status
  bool logAllOk = false;                      ///< All-time logging status
  bool wifiApRunning = false;                 ///< SoftAP running
  int16_t wifiRssiDbm = -127;                 ///< Average station RSSI (dBm), -127 when unavailable
  uint8_t wifiChannel = 0;                    ///< Active SoftAP channel
  uint8_t wifiStationCount = 0;               ///< Connected stations
  size_t webClientCount = 0;                  ///< WebSocket clients
  uint32_t logDroppedCount = 0;               ///< Dropped log records
  size_t logQueueDepth = 0;                   ///< Pending log queue depth
  size_t logQueueCapacity = 0;                ///< Runtime sample queue capacity
  bool logQueueUsingPsram = false;            ///< True when sample queue storage is in PSRAM
  uint32_t logLastWriteMs = 0;                ///< Last successful log write time
  uint32_t logLastWriteAgeMs = 0;             ///< Age since last log write
  uint32_t logLastErrorMs = 0;                ///< Last log error time
  uint32_t logLastErrorAgeMs = 0;             ///< Age since last log error
  const char* logLastErrorMsg = "";           ///< Last log error message
  int32_t logLastErrorDetail = 0;             ///< Last log error detail code
  uint32_t logIoBudgetMs = 0;                 ///< Configured logger I/O budget per tick
  uint32_t logLastTickElapsedMs = 0;          ///< Last logger tick elapsed duration
  uint32_t logBudgetExceededCount = 0;        ///< Logger ticks exceeding budget
  uint32_t logEventDroppedCount = 0;          ///< Dropped event log records
  size_t logEventQueueDepth = 0;              ///< Pending event log queue depth
  size_t logEventQueueCapacity = 0;           ///< Runtime event queue capacity
  bool logEventQueueUsingPsram = false;       ///< True when event queue storage is in PSRAM
  size_t sampleHistoryDepth = 0;              ///< In-memory sample history depth
  size_t sampleHistoryCapacity = 0;           ///< In-memory sample history capacity
  bool sampleHistoryUsingPsram = false;       ///< True when sample history ring storage is in PSRAM
  size_t eventHistoryDepth = 0;               ///< In-memory event history depth
  size_t eventHistoryCapacity = 0;            ///< In-memory event history capacity
  bool eventHistoryUsingPsram = false;        ///< True when event history ring storage is in PSRAM
  bool webScratchUsingPsram = false;          ///< True when web graph/event scratch storage is in PSRAM
  uint32_t webGraphScratchCapacity = 0;       ///< Runtime graph scratch capacity (samples)
  uint32_t webEventScratchCapacity = 0;       ///< Runtime event scratch capacity (events)
  bool logSessionActive = false;              ///< True when session-scoped file set is active
  const char* logSessionDir = "";             ///< Active session directory path
  const char* logCurrentSampleFile = "";      ///< Active rotating sample file path
  const char* logCurrentEventFile = "";       ///< Active rotating event file path
  uint16_t logCurrentSampleFilePart = 0;      ///< Active sample file part index
  uint16_t logCurrentEventFilePart = 0;       ///< Active event file part index
  uint32_t logSampleCurrentDataLine = 0;      ///< Current sample data line in active sample file
  uint32_t logEventCurrentDataLine = 0;       ///< Current event data line in active event file
  uint32_t logSampleWrittenTotal = 0;         ///< Total sample rows written to rotating sample files
  uint32_t logEventWrittenTotal = 0;          ///< Total event rows written to rotating event files
  uint32_t logSampleWriteSuccessCount = 0;    ///< Successful sample file write operations
  uint32_t logSampleWriteFailureCount = 0;    ///< Failed sample file write operations
  uint32_t logEventWriteSuccessCount = 0;     ///< Successful event file write operations
  uint32_t logEventWriteFailureCount = 0;     ///< Failed event file write operations
  uint32_t logSampleRotateCount = 0;          ///< Sample file rotations due to size limit
  uint32_t logEventRotateCount = 0;           ///< Event file rotations due to size limit
  uint32_t i2cErrorCount = 0;                 ///< I2C bus total error count
  uint32_t i2cConsecutiveErrors = 0;          ///< I2C bus consecutive error count
  uint32_t i2cRecoveryCount = 0;              ///< I2C bus recovery attempts
  uint32_t i2cLastErrorMs = 0;                ///< Last I2C bus error timestamp
  uint32_t i2cLastRecoveryMs = 0;             ///< Last I2C bus recovery timestamp
  uint32_t i2cStuckSdaCount = 0;              ///< Detected SDA-stuck events
  uint32_t i2cStuckBusFastFailCount = 0;      ///< Fast-fail preflight rejections due to stuck bus
  uint32_t i2cRequestOverflowCount = 0;       ///< I2C request queue overflows
  uint32_t i2cResultDroppedCount = 0;         ///< I2C result drops due to full queue
  uint32_t i2cStaleResultCount = 0;           ///< I2C late/stale results ignored by orchestrator
  uint32_t i2cSlowOpCount = 0;                ///< Total I2C operations above slow-op threshold
  uint32_t i2cRecentSlowOpCount = 0;          ///< Slow-op count in rolling configured window
  size_t i2cRequestQueueDepth = 0;            ///< Pending I2C request queue depth
  size_t i2cResultQueueDepth = 0;             ///< Pending I2C result queue depth
  uint32_t i2cMaxDurationUs = 0;              ///< Max observed I2C transaction duration
  uint32_t i2cRollingMaxDurationUs = 0;       ///< Rolling max duration in current configured window
  uint32_t i2cMeanDurationUs = 0;             ///< Mean I2C transaction duration (approx)
  uint32_t i2cTaskAliveMs = 0;                ///< I2C task heartbeat timestamp
  uint32_t i2cTaskAliveAgeMs = 0;             ///< Age since last I2C task heartbeat
  uint32_t i2cPowerCycleAttempts = 0;         ///< Escalated power-cycle recovery attempts
  uint32_t i2cLastPowerCycleMs = 0;           ///< Last power-cycle attempt timestamp
  bool i2cPowerCycleConfigured = false;       ///< True when board-level power-cycle hook is configured
  uint16_t i2cPowerCycleLastCode = 0;         ///< Last power-cycle Status code value
  int32_t i2cPowerCycleLastDetail = 0;        ///< Last power-cycle Status detail value
  const char* i2cPowerCycleLastMsg = "";      ///< Last power-cycle Status message
  uint8_t i2cLastRecoveryStage = 0;           ///< Last recovery stage enum value
  const char* i2cBackendName = "";            ///< Active backend name
  bool i2cDeterministicTimeout = false;       ///< True when backend enforces deterministic timeout
  uint32_t i2cRtcConsecutiveFailures = 0;     ///< RTC orchestrator consecutive failures
  uint32_t i2cEnvConsecutiveFailures = 0;     ///< ENV orchestrator consecutive failures
  size_t commandQueueDepth = 0;               ///< Pending command queue depth
  size_t commandQueueCapacity = 0;            ///< Command queue capacity
  uint32_t commandQueueOverflowCount = 0;     ///< Command queue overflow count
  uint32_t commandQueueLastOverflowMs = 0;    ///< Last command queue overflow time

  // --- Output state ---
  uint8_t outputPresentMask = 0;              ///< Bitmask of physically configured output channels
  uint8_t outputChannelMask = 0;              ///< Bitmask of active output channels
  uint8_t outputOverrideMode = 0;             ///< OutputOverrideMode enum value (0=AUTO,1=OFF,2=ON)
  bool outputsEnabled = false;                ///< True when outputs are enabled in settings
  bool outputLogicState = false;              ///< Current hysteresis logic output state
  uint8_t outputValveChannel = 0xFFU;         ///< Configured valve channel index (255=disabled)
  bool outputValvePoweredCloses = true;       ///< True when powered valve state semantically means "closed"
  bool outputValveState = false;              ///< Current valve actuation state (powered/unpowered)
  uint8_t outputFanChannel = 0xFFU;           ///< Configured fan channel index (255=disabled)
  bool outputFanState = false;                ///< Current fan actuation state (on/off)
  uint8_t outputFanPwmPercent = 0;            ///< Current effective fan PWM percent (0..100)
  uint8_t outputTestActiveMask = 0;           ///< Bitmask of channels with active test override
  uint8_t outputTestStateMask = 0;            ///< Bitmask of active test overrides requesting ON state
  uint32_t outputLastChangeMs = 0;            ///< Timestamp of last output state change
  const char* fwVersion = "";                  ///< Firmware version string

  // --- System resource metrics ---
  uint32_t heapFreeBytes = 0;                 ///< Current free heap bytes
  uint32_t heapMinFreeBytes = 0;              ///< Minimum free heap since boot (watermark)
  uint32_t heapTotalBytes = 0;                ///< Total heap size
  uint32_t heapMaxAllocBytes = 0;             ///< Largest contiguous free block
  bool psramAvailable = false;                ///< True when runtime PSRAM is detected
  uint32_t psramTotalBytes = 0;               ///< Total PSRAM bytes reported by runtime
  uint32_t psramFreeBytes = 0;                ///< Current free PSRAM bytes
  uint32_t psramMinFreeBytes = 0;             ///< Minimum free PSRAM bytes since boot
  uint32_t psramMaxAllocBytes = 0;            ///< Largest contiguous free PSRAM block
  uint32_t mainTaskStackFreeBytes = 0;        ///< Main task stack high-water mark (bytes remaining)
  uint32_t i2cTaskStackFreeBytes = 0;         ///< I2C task stack high-water mark (bytes remaining)
};

}  // namespace CO2Control

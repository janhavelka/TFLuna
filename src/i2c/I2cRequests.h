#pragma once

#include <stddef.h>
#include <stdint.h>

#include "CO2Control/HardwareSettings.h"
#include "CO2Control/Health.h"
#include "CO2Control/RuntimeSettings.h"
#include "CO2Control/Status.h"
#include "CO2Control/Types.h"

namespace CO2Control {

enum class I2cOpType : uint8_t {
  NONE = 0,
  PROBE,
  WRITE,
  READ,
  WRITE_READ,
  RTC_SET_TIME,
  ENV_TRIGGER_ONESHOT,
  ENV_READ_ONESHOT,
  DISPLAY_REFRESH,
  RECOVER,
  SET_FREQ
};

static constexpr uint32_t kRtcEepromMinTimeoutMs = 50U;

inline bool rtcBackupConfigRequiresPersistence(const RuntimeSettings& settings) {
  return settings.i2cRtcBackupMode != 0U;
}

inline bool rtcEepromWritesEnabled(const RuntimeSettings& settings) {
  return settings.i2cRtcEnableEepromWrites || rtcBackupConfigRequiresPersistence(settings);
}

inline uint32_t rtcI2cTimeoutMs(const RuntimeSettings& settings) {
  if (!rtcEepromWritesEnabled(settings) ||
      settings.i2cOpTimeoutMs >= kRtcEepromMinTimeoutMs) {
    return settings.i2cOpTimeoutMs;
  }
  return kRtcEepromMinTimeoutMs;
}

struct I2cRequest {
  I2cOpType op = I2cOpType::NONE;
  DeviceId deviceId = DeviceId::I2C_BUS;
  uint8_t address = 0;
  uint8_t tx[HardwareSettings::I2C_PAYLOAD_BYTES] = {0};
  uint8_t txLen = 0;
  uint8_t rxLen = 0;
  uint32_t timeoutMs = 20;
  uint32_t createdMs = 0;
  uint32_t deadlineMs = 0;
  uint32_t token = 0;
};

struct I2cResult {
  Status status = Ok();
  I2cOpType op = I2cOpType::NONE;
  DeviceId deviceId = DeviceId::I2C_BUS;
  uint8_t address = 0;
  uint8_t data[HardwareSettings::I2C_PAYLOAD_BYTES] = {0};
  uint8_t dataLen = 0;
  uint32_t durationUs = 0;
  uint32_t token = 0;
  uint32_t completedMs = 0;
  uint32_t requestDeadlineMs = 0;
  bool late = false;
};

enum class I2cRecoveryStage : uint8_t {
  NONE = 0,
  RESET = 1,
  SCL_PULSE = 2,
  POWER_CYCLE = 3
};

struct I2cBusMetrics {
  uint32_t errorCount = 0;
  uint32_t consecutiveErrors = 0;
  uint32_t recoveryCount = 0;
  uint32_t lastErrorMs = 0;
  uint32_t lastRecoveryMs = 0;
  uint32_t stuckSdaCount = 0;
  uint32_t stuckBusFastFailCount = 0;
  uint32_t requestOverflowCount = 0;
  uint32_t resultDroppedCount = 0;
  uint32_t staleResultCount = 0;
  uint32_t slowOpCount = 0;
  uint32_t recentSlowOpCount = 0;
  uint32_t rollingMaxDurationUs = 0;
  uint32_t slowWindowStartMs = 0;
  uint32_t taskAliveMs = 0;
  uint32_t powerCycleAttempts = 0;
  uint32_t lastPowerCycleMs = 0;
  bool powerCycleConfigured = false;
  Status lastPowerCycleStatus = Status(Err::NOT_INITIALIZED, 0, "not configured");
  I2cRecoveryStage lastRecoveryStage = I2cRecoveryStage::NONE;
  size_t requestQueueDepth = 0;
  size_t resultQueueDepth = 0;
  uint32_t maxDurationUs = 0;
  uint64_t totalDurationUs = 0;
  uint64_t transactionCount = 0;
  const char* backendName = "";
  bool deterministicTimeout = false;
  uint32_t taskStackFreeBytes = 0;
  Status lastError = Ok();
};

class II2cRequestPort {
 public:
  virtual ~II2cRequestPort() = default;
  virtual Status begin(const HardwareSettings& config, const RuntimeSettings& settings) = 0;
  virtual void end() = 0;
  virtual Status enqueue(const I2cRequest& request, uint32_t nowMs) = 0;
  virtual bool dequeueResult(I2cResult& out) = 0;
  virtual void tick(uint32_t nowMs) = 0;
  virtual void applySettings(const RuntimeSettings& settings, uint32_t nowMs) = 0;
  virtual I2cBusMetrics getMetrics() const = 0;
  virtual HealthState health() const = 0;
};

}  // namespace CO2Control

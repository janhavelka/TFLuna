/**
 * @file I2cRaw.h
 * @brief I2C raw-operation and RTC diagnostic snapshot types.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "CO2Control/HardwareSettings.h"
#include "CO2Control/Status.h"

namespace CO2Control {

/// @brief Raw I2C operation type for CLI/task-owned bus operations.
enum class I2cRawOp : uint8_t {
  NONE = 0,
  WRITE,
  READ,
  WRITE_READ,
  PROBE
};

/// @brief Snapshot of last queued/in-flight/completed raw I2C operation.
struct I2cRawSnapshot {
  static constexpr size_t MAX_BYTES = HardwareSettings::I2C_PAYLOAD_BYTES;

  bool queued = false;
  bool active = false;
  bool complete = false;
  I2cRawOp op = I2cRawOp::NONE;
  uint8_t address = 0;
  uint8_t txLen = 0;
  uint8_t tx[MAX_BYTES] = {0};
  uint8_t rxRequested = 0;
  uint8_t rxLen = 0;
  uint8_t rx[MAX_BYTES] = {0};
  uint32_t queuedMs = 0;
  uint32_t updatedMs = 0;
  Status lastStatus = Status(Err::NOT_INITIALIZED, 0, "raw idle");
};

/// @brief Cached RTC diagnostics captured by the task-owned I2C backend.
struct RtcDebugSnapshot {
  bool supported = false;
  bool enabled = false;
  bool initialized = false;
  uint8_t address = 0;
  uint32_t requestedI2cTimeoutMs = 0;
  uint32_t effectiveI2cTimeoutMs = 0;
  uint8_t requestedBackupMode = 0;
  bool requestedEepromWrites = false;
  bool effectiveEepromWrites = false;
  uint32_t eepromTimeoutMs = 0;
  uint8_t offlineThreshold = 0;
  uint8_t driverState = 0;
  bool eepromBusy = false;
  Status eepromStatus = Status(Err::NOT_INITIALIZED, 0, "RTC EEPROM idle");
  uint32_t eepromWriteCount = 0;
  uint32_t eepromWriteFailures = 0;
  uint8_t eepromQueueDepth = 0;
  bool hasStatusReg = false;
  uint8_t rawStatusReg = 0;
  Status statusRegStatus =
      Status(Err::NOT_INITIALIZED, 0, "RTC status register not read");
  bool hasTempLsb = false;
  uint8_t rawTempLsb = 0;
  Status tempLsbStatus =
      Status(Err::NOT_INITIALIZED, 0, "RTC temp-lsb register not read");
  bool hasPmuReg = false;
  uint8_t rawPmuReg = 0;
  Status pmuStatus =
      Status(Err::NOT_INITIALIZED, 0, "RTC PMU register not read");
  bool hasEffectiveBackupMode = false;
  uint8_t effectiveBackupMode = 0;
  bool powerOnReset = false;
  bool voltageLow = false;
  bool backupSwitched = false;
  bool timeInvalid = false;
  uint32_t updatedMs = 0;
};

}  // namespace CO2Control

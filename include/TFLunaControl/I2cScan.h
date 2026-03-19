/**
 * @file I2cScan.h
 * @brief I2C bus scan snapshot types.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "TFLunaControl/Status.h"

namespace TFLunaControl {

/// @brief Snapshot of asynchronous I2C bus scan state.
struct I2cScanSnapshot {
  static constexpr size_t MAX_FOUND = 32;

  bool active = false;
  bool complete = false;
  uint8_t nextAddress = 0x03;
  uint8_t foundCount = 0;
  uint8_t foundAddresses[MAX_FOUND] = {0};
  uint32_t startedMs = 0;
  uint32_t updatedMs = 0;
  uint32_t probesTotal = 0;
  uint32_t probesTimeout = 0;
  uint32_t probesError = 0;
  uint32_t probesNack = 0;
  Status lastStatus = Status(Err::NOT_INITIALIZED, 0, "scan idle");
};

}  // namespace TFLunaControl

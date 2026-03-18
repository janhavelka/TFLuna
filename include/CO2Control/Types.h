/**
 * @file Types.h
 * @brief Core types for CO2Control.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace CO2Control {

/// @brief Device identifiers used for health/status reporting.
enum class DeviceId : uint8_t {
  SYSTEM = 0,
  I2C_BUS,
  SD,
  ENV,
  RTC,
  CO2,
  OUTPUTS,
  WIFI,
  WEB,
  LEDS,
  BUTTON,
  RS485,
  COUNT
};

/// @brief Number of devices tracked.
static constexpr size_t DEVICE_COUNT = static_cast<size_t>(DeviceId::COUNT);

/// @brief Sample validity mask bits.
enum SampleValidMask : uint8_t {
  VALID_CO2 = 1 << 0,
  VALID_TEMP = 1 << 1,
  VALID_RH = 1 << 2,
  VALID_PRESSURE = 1 << 3
};

/// @brief Output control override mode.
enum class OutputOverrideMode : uint8_t {
  AUTO = 0,
  FORCE_OFF,
  FORCE_ON
};

/// @brief Output control input source.
enum class OutputSource : uint8_t {
  CO2  = 0,  ///< Control based on CO2 ppm readings.
  TEMP = 1,  ///< Control based on temperature readings.
  RH   = 2   ///< Control based on relative humidity readings.
};

/**
 * @brief Timestamp from RTC.
 */
struct RtcTime {
  uint16_t year = 0;
  uint8_t month = 0;
  uint8_t day = 0;
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t second = 0;
  bool valid = false;
};

/**
 * @brief Sensor sample with timestamps.
 */
struct Sample {
  uint32_t tsUnix = 0;    ///< Unix timestamp (seconds) or 0 if invalid
  char tsLocal[20] = {0}; ///< Local time "YYYY-MM-DD HH:MM:SS" or empty
  float co2ppm = 0.0f;
  float tempC = 0.0f;
  float rhPct = 0.0f;
  float pressureHpa = 0.0f;
  uint8_t validMask = 0;  ///< OR of SampleValidMask
};

/**
 * @brief Event record for logging important state changes.
 */
struct Event {
  uint32_t tsUnix = 0;
  char tsLocal[20] = {0};
  uint16_t code = 0;
  char msg[64] = {0};
};

}  // namespace CO2Control

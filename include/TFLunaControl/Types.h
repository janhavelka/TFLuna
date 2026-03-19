/**
 * @file Types.h
 * @brief Core types for TFLunaControl.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace TFLunaControl {

/// @brief Device identifiers used for health/status reporting.
enum class DeviceId : uint8_t {
  SYSTEM = 0,
  I2C_BUS,
  SD,
  ENV,
  RTC,
  LIDAR,
  CO2 = LIDAR,
  WIFI,
  WEB,
  LEDS,
  BUTTON,
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

struct LidarMeasurement {
  uint16_t distanceCm = 0;
  uint16_t strength = 0;
  float temperatureC = 0.0f;
  bool validFrame = false;
  bool signalOk = false;
  uint32_t capturedMs = 0;
};

struct LidarStatsSnapshot {
  uint64_t totalFrames = 0;
  uint64_t validSamples = 0;
  uint64_t invalidSamples = 0;
  uint64_t weakSamples = 0;
  bool hasDistanceStats = false;
  float minDistanceCm = 0.0f;
  float maxDistanceCm = 0.0f;
  float meanDistanceCm = 0.0f;
  float stddevDistanceCm = 0.0f;
  float rangeDistanceCm = 0.0f;
  float meanStrength = 0.0f;
  float stddevStrength = 0.0f;
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
  uint32_t uptimeMs = 0;  ///< Monotonic uptime at sample capture
  uint32_t sampleIndex = 0;
  uint16_t distanceCm = 0;
  uint16_t strength = 0;
  float lidarTempC = 0.0f;
  bool validFrame = false;
  bool signalOk = false;
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

}  // namespace TFLunaControl

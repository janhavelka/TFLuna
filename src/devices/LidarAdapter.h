#pragma once

#include "TFLunaControl/HardwareSettings.h"
#include "TFLunaControl/Health.h"
#include "TFLunaControl/RuntimeSettings.h"
#include "TFLunaControl/Status.h"
#include "TFLunaControl/Types.h"
#include "core/LidarStats.h"
#include "devices/TfLunaFrameParser.h"

#ifdef ARDUINO
#include <HardwareSerial.h>
#endif

namespace TFLunaControl {

/**
 * @brief TF-Luna UART adapter for the measurement channel.
 *
 * Some compatibility-oriented status fields still retain legacy naming so the
 * restored CLI/web shell keeps working, but the transport and measurements are
 * TF-Luna LiDAR only.
 */
class LidarAdapter {
 public:
  LidarAdapter();

  void applySettings(const RuntimeSettings& settings, uint32_t nowMs);
  Status begin(const HardwareSettings& config);
  void tick(uint32_t nowMs);
  Status forceRecover(uint32_t nowMs);
  Status readOnce(Sample& sample, uint32_t nowMs);
  Status probeOnce(Sample& sample, uint32_t nowMs);

  bool latestMeasurement(LidarMeasurement& out) const;
  LidarStatsSnapshot statsSnapshot() const;
  uint32_t framesParsed() const;
  uint32_t checksumErrors() const;
  uint32_t syncLossCount() const;

  HealthState health() const { return _health; }
  Status lastStatus() const { return _lastStatus; }

 private:
  Status reinitializeUart(uint32_t nowMs, bool explicitRecover);
  Status evaluateHealth(uint32_t nowMs);

  bool _configured = false;
  HealthState _health = HealthState::UNKNOWN;
  Status _lastStatus = Ok();
  int _rxPin = -1;
  int _txPin = -1;
  uint32_t _baudRate = 115200U;
  uint32_t _serviceIntervalMs = 10U;
  uint32_t _frameStaleMs = 1500U;
  uint32_t _nextServiceMs = 0U;
  bool _haveLatest = false;
  LidarMeasurement _latest{};
  LidarStats _stats{};
  TfLunaFrameParser _parser{};

#if defined(ARDUINO)
  HardwareSerial _serial;
#endif
};

}  // namespace TFLunaControl

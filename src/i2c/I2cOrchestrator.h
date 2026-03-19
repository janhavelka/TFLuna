#pragma once

#include "TFLunaControl/AppSettings.h"
#include "TFLunaControl/I2cRaw.h"
#include "TFLunaControl/I2cScan.h"
#include "i2c/I2cRequests.h"

namespace TFLunaControl {

class I2cOrchestrator {
 public:
  Status begin(const HardwareSettings& config,
               const AppSettings& appSettings,
               const RuntimeSettings& settings,
               II2cRequestPort* port);
  void end();
  void applySettings(const RuntimeSettings& settings);
  void tick(uint32_t nowMs);

  Status queueRtcSet(const RtcTime& time, uint32_t nowMs);
  Status queueBusRecover(uint32_t nowMs);
  Status queueBusScan(uint32_t nowMs);
  Status queueRawRequest(const I2cRequest& request, uint32_t nowMs);
  void setDisplayCo2Snapshot(float ppm, bool valid, uint32_t sampleMs);
  void setDisplayOutputSnapshot(uint8_t channelMask,
                                OutputOverrideMode mode,
                                bool outputsEnabled);
  void setDisplaySystemSnapshot(bool loggingEnabled,
                                bool sdMounted,
                                bool loggingHealthy,
                                uint32_t samplesWritten,
                                HealthState systemHealth);
  I2cScanSnapshot scanSnapshot() const { return _scan; }
  I2cRawSnapshot rawSnapshot() const { return _raw; }
  Status getRtcTime(uint32_t nowMs, RtcTime& out) const;
  Status fillEnvSample(Sample& sample, uint32_t nowMs) const;

  HealthState busHealth() const;
  I2cBusMetrics busMetrics() const;
  Status busStatus() const;

  HealthState rtcHealth() const { return _rtcHealth; }
  HealthState envHealth() const { return _envHealth; }
  Status rtcStatus() const { return _rtcStatus; }
  Status envStatus() const { return _envStatus; }
  uint32_t rtcConsecutiveFailures() const { return _rtcConsecutiveFailures; }
  uint32_t envConsecutiveFailures() const { return _envConsecutiveFailures; }

 private:
  enum class EnvPhase : uint8_t {
    IDLE = 0,
    WAIT_CONVERSION,
    WAIT_READ_RESULT
  };

  void handleExpiredInFlight(uint32_t nowMs);
  bool isExpired(uint32_t nowMs, uint32_t deadlineMs) const;
  uint32_t computeDeadlineMs(uint32_t nowMs, uint32_t timeoutMs) const;
  bool enqueueRequest(I2cRequest& request, uint32_t nowMs);
  void processResult(const I2cResult& result, uint32_t nowMs);
  void updateRtcSuccess(const RtcTime& time, uint32_t nowMs);
  void updateRtcFailure(const Status& status, uint32_t nowMs);
  void updateEnvFailure(const Status& status, uint32_t nowMs);
  void scheduleRequests(uint32_t nowMs);

  II2cRequestPort* _port = nullptr;
  HardwareSettings _config{};
  AppSettings _appSettings{};
  RuntimeSettings _settings{};
  bool _enabled = false;

  uint32_t _nextToken = 1;
  uint32_t _requestOverflowCount = 0;
  uint32_t _lastOverflowMs = 0;
  uint32_t _staleResultCount = 0;
  bool _busRecoverPending = false;
  uint32_t _busRecoverActiveToken = 0;
  uint32_t _busRecoverActiveDeadlineMs = 0;
  Status _busRecoverStatus = Status(Err::NOT_INITIALIZED, 0, "I2C recover idle");

  // RTC state
  bool _rtcHasTime = false;
  RtcTime _rtcTime{};
  uint32_t _rtcLastSuccessMs = 0;
  uint32_t _rtcNextPollMs = 0;
  uint32_t _rtcActiveReadToken = 0;
  uint32_t _rtcActiveReadDeadlineMs = 0;
  uint32_t _rtcActiveSetToken = 0;
  uint32_t _rtcActiveSetDeadlineMs = 0;
  bool _rtcSetPending = false;
  RtcTime _rtcSetPendingValue{};
  uint32_t _rtcConsecutiveFailures = 0;
  uint32_t _rtcConsecutiveSuccesses = 0;
  HealthState _rtcHealth = HealthState::UNKNOWN;
  Status _rtcStatus = Status(Err::NOT_INITIALIZED, 0, "RTC not initialized");

  // ENV state
  EnvPhase _envPhase = EnvPhase::IDLE;
  uint32_t _envPhaseDeadlineMs = 0;
  uint32_t _envNextPollMs = 0;
  uint32_t _envActiveTriggerToken = 0;
  uint32_t _envActiveTriggerDeadlineMs = 0;
  uint32_t _envActiveReadToken = 0;
  uint32_t _envActiveReadDeadlineMs = 0;
  bool _envHasData = false;
  uint32_t _envLastSuccessMs = 0;
  float _envTempC = 0.0f;
  float _envRhPct = 0.0f;
  float _envPressureHpa = 0.0f;
  uint32_t _envConsecutiveFailures = 0;
  uint32_t _envConsecutiveSuccesses = 0;
  HealthState _envHealth = HealthState::UNKNOWN;
  Status _envStatus = Status(Err::NOT_INITIALIZED, 0, "ENV not initialized");

  // Future display channel
  bool _displayEnabled = false;
  uint32_t _displayNextPollMs = 0;
  bool _displayCo2Valid = false;
  float _displayCo2Ppm = 0.0f;
  uint32_t _displayCo2SampleMs = 0;
  uint8_t _displayOutputMask = 0;
  OutputOverrideMode _displayOutputMode = OutputOverrideMode::AUTO;
  bool _displayOutputsEnabled = false;
  bool _displayLoggingEnabled = false;
  bool _displaySdMounted = false;
  bool _displayLoggingHealthy = false;
  uint32_t _displayLogSamplesWritten = 0;
  HealthState _displaySystemHealth = HealthState::UNKNOWN;

  // Bus scan state
  bool _scanPending = false;
  uint8_t _scanAddress = 0x03;
  uint32_t _scanActiveToken = 0;
  uint32_t _scanActiveDeadlineMs = 0;
  uint32_t _scanNextProbeMs = 0;
  I2cScanSnapshot _scan{};

  // Raw command state
  bool _rawPending = false;
  I2cRequest _rawRequest{};
  uint32_t _rawActiveToken = 0;
  uint32_t _rawActiveDeadlineMs = 0;
  I2cRawSnapshot _raw{};
};

}  // namespace TFLunaControl

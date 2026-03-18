#pragma once

#include "CO2Control/HardwareSettings.h"
#include "CO2Control/Health.h"
#include "CO2Control/RuntimeSettings.h"
#include "CO2Control/Status.h"
#include "CO2Control/Types.h"

#if defined(ARDUINO) && __has_include(<EE871/EE871.h>)
#define CO2CONTROL_HAS_EE871_LIB 1
#include <EE871/EE871.h>
#if __has_include(<EE871/Version.h>)
#include <EE871/Version.h>
static_assert(EE871::VERSION_CODE >= 300, "CO2Control requires EE871-E2 >= v0.3.0");
#endif
#else
#define CO2CONTROL_HAS_EE871_LIB 0
#endif

#if CO2CONTROL_HAS_EE871_LIB
namespace EE871Api = EE871;
#endif

namespace CO2Control {

/**
 * @brief CO2 sensor adapter (EE871 on E2 bus).
 */
class Co2Adapter {
 public:
  void applySettings(const RuntimeSettings& settings, uint32_t nowMs);
  Status begin(const HardwareSettings& config);
  Status forceRecover(uint32_t nowMs);
  Status readOnce(Sample& sample, uint32_t nowMs);
  HealthState health() const { return _health; }
  Status lastStatus() const { return _lastStatus; }

 private:
#if CO2CONTROL_HAS_EE871_LIB
  static void e2SetScl(bool level, void* user);
  static void e2SetSda(bool level, void* user);
  static bool e2ReadScl(void* user);
  static bool e2ReadSda(void* user);
  static void e2DelayUs(uint32_t us, void* user);
  static Status mapDriverStatus(const EE871Api::Status& status);
  static HealthState mapDriverHealth(EE871Api::DriverState state);
  static bool shouldRetry(const EE871Api::Status& status);
  Status applyManagedDeviceSettings();
  EE871Api::Config makeDriverConfig() const;
  Status attemptBeginOrRecover(uint32_t nowMs);
  void resetRetryState();
  void scheduleNextRetry(uint32_t nowMs);
  bool retryDue(uint32_t nowMs) const;
  void setLine(int pin, bool level);
  bool readLine(int pin) const;
#endif

  bool _configured = false;
  HealthState _health = HealthState::UNKNOWN;
  Status _lastStatus = Ok();

#if CO2CONTROL_HAS_EE871_LIB
  int _sclPin = -1;
  int _sdaPin = -1;
  int _enPin = -1;
  bool _driverInitialized = false;
  bool _recoveryPending = false;
  uint32_t _nextRetryMs = 0;
  uint32_t _retryDelayMs = 0;
  uint8_t _e2Address = 0;
  uint16_t _e2ClockLowUs = 100;
  uint16_t _e2ClockHighUs = 100;
  uint16_t _e2StartHoldUs = 100;
  uint16_t _e2StopHoldUs = 100;
  uint32_t _e2BitTimeoutUs = 2000;
  uint32_t _e2ByteTimeoutUs = 6000;
  uint32_t _e2WriteDelayMs = 150;
  uint32_t _e2IntervalWriteDelayMs = 300;
  uint8_t _e2OfflineThreshold = 3;
  uint32_t _e2RecoveryBackoffMs = 1000;
  uint32_t _e2RecoveryBackoffMaxMs = 30000;
  uint16_t _e2ConfigIntervalDs = 0;
  int8_t _e2ConfigCo2IntervalFactor = RuntimeSettings::E2_CONFIG_INTERVAL_FACTOR_DISABLED;
  uint8_t _e2ConfigFilter = RuntimeSettings::E2_CONFIG_FILTER_DISABLED;
  uint8_t _e2ConfigOperatingMode = RuntimeSettings::E2_CONFIG_OPERATING_MODE_DISABLED;
  int16_t _e2ConfigOffsetPpm = RuntimeSettings::E2_CONFIG_OFFSET_PPM_DISABLED;
  uint16_t _e2ConfigGain = RuntimeSettings::E2_CONFIG_GAIN_DISABLED;
  bool _managedSettingsDirty = false;
  uint8_t _managedSettingsStage = 0;  ///< 0=idle, 1-6=per-param stage
  EE871Api::EE871 _driver{};
#endif
};

}  // namespace CO2Control


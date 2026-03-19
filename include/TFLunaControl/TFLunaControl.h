/**
 * @file TFLunaControl.h
 * @brief Main application class for TFLunaControl firmware.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "TFLunaControl/AppSettings.h"
#include "TFLunaControl/HardwareSettings.h"
#include "TFLunaControl/I2cRaw.h"
#include "TFLunaControl/Health.h"
#include "TFLunaControl/I2cScan.h"
#include "TFLunaControl/RuntimeSettings.h"
#include "TFLunaControl/Status.h"
#include "TFLunaControl/Types.h"

namespace TFLunaControl {

/**
 * @brief Main firmware application class.
 *
 * Implements deterministic begin/tick/end lifecycle with non-blocking behavior.
 * @note Not thread-safe. Call from a single thread (Arduino loop()).
 */
class TFLunaControl {
 public:
  /// @brief Initialize with configuration.
  /// @param config Hardware configuration.
  /// @note Uses default AppSettings().
  /// @return Ok on success, INVALID_CONFIG on error.
  Status begin(const HardwareSettings& config);
  /// @brief Initialize with explicit hardware and app settings.
  /// @param config Hardware configuration.
  /// @param appSettings Boot-time software/service settings.
  Status begin(const HardwareSettings& config, const AppSettings& appSettings);

  /// @brief Stop and release resources.
  void end();

  /// @brief Cooperative update. Call every loop iteration.
  /// @param now_ms Current time in milliseconds.
  void tick(uint32_t now_ms);

  /// @brief Execute deferred blocking operations (NVS save, WiFi start/stop).
  /// Call from main loop after tick(), outside tick timing measurement.
  /// These operations are split from tick() so they do not inflate tick metrics.
  void processDeferred();

  /// @brief Get active configuration.
  const HardwareSettings& getConfig() const { return _config; }

  /// @brief Get active boot-time software/service settings.
  const AppSettings& getAppSettings() const { return _appSettings; }

  /// @brief Get active settings.
  RuntimeSettings getSettings() const;

  /// @brief Get current system status.
  /// @return Copy of current system status.
  SystemStatus getSystemStatus() const;

  /// @brief Try to get status and latest sample snapshot without blocking.
  /// @param outStatus Snapshot output.
  /// @param outLatest Latest sample output (valid only when outHasLatest is true).
  /// @param outHasLatest True when outLatest contains a sample.
  /// @return true if lock acquired and snapshot copied, false if busy/not initialized.
  bool tryGetStatusSnapshot(SystemStatus& outStatus, Sample& outLatest, bool& outHasLatest) const;

  /// @brief Get status for a specific device.
  const DeviceStatus& getDeviceStatus(DeviceId id) const;

  /// @brief Try to get settings snapshot without blocking.
  /// @param out Snapshot output.
  /// @return true when snapshot copied, false if busy/not initialized.
  bool tryGetSettingsSnapshot(RuntimeSettings& out) const;

  /// @brief Try to copy device statuses without blocking.
  /// @param out Destination array.
  /// @param max Maximum number of status entries to copy.
  /// @param outCount Number of entries copied.
  /// @return true when snapshot copied, false if busy/not initialized.
  bool tryCopyDeviceStatuses(DeviceStatus* out, size_t max, size_t& outCount) const;

  /// @brief Copy latest sample.
  /// @param out Destination sample.
  /// @return true if latest sample exists.
  bool getLatestSample(Sample& out) const;

  /// @brief Copy sample history into caller buffer.
  /// @param out Destination array.
  /// @param max Maximum number of samples to copy.
  /// @param oldestFirst Order of samples.
  /// @return Number of samples copied.
  size_t copySamples(Sample* out, size_t max, bool oldestFirst) const;

  /// @brief Try to copy sample history without blocking.
  /// @param out Destination array.
  /// @param max Maximum number of samples to copy.
  /// @param oldestFirst Order of samples.
  /// @param outCount Number of copied samples.
  /// @return true if lock acquired and copy completed, false if busy/not initialized.
  bool tryCopySamples(Sample* out, size_t max, bool oldestFirst, size_t& outCount) const;

  /// @brief Try to copy event history without blocking.
  /// @param out Destination array.
  /// @param max Maximum number of events to copy.
  /// @param oldestFirst Order of copied events.
  /// @param outCount Number of copied events.
  /// @return true if lock acquired and copy completed, false if busy/not initialized.
  bool tryCopyEvents(Event* out, size_t max, bool oldestFirst, size_t& outCount) const;

  /// @brief Update settings and optionally persist.
  /// @param settings New settings.
  /// @param persist Persist to NVS if enabled.
  /// @param changeHint Optional short hint describing changed keys for event log.
  /// @return Ok on success, INVALID_CONFIG if validation fails.
  Status updateSettings(const RuntimeSettings& settings, bool persist, const char* changeHint = nullptr);

  /// @brief Factory reset settings to defaults.
  /// @param persist Persist to NVS if enabled.
  /// @return Ok on success.
  Status factoryResetSettings(bool persist);

  /// @brief Set RTC time.
  /// @param time New time.
  /// @return Ok on success.
  Status setRtcTime(const RtcTime& time);

  /// @brief Attempt SD remount.
  /// @return Ok on success.
  Status remountSd();

  /// @brief Enqueue settings update from non-loop context.
  /// @param settings New settings payload.
  /// @param persist Persist to NVS if enabled.
  /// @param changeHint Optional short hint describing changed keys for event log.
  /// @return Ok if queued, RESOURCE_BUSY if queue full.
  Status enqueueApplySettings(const RuntimeSettings& settings,
                              bool persist,
                              const char* changeHint = nullptr);

  /// @brief Enqueue runtime SoftAP enable/disable request (button-like behavior).
  /// @param enabled True to allow/start AP, false to disable/stop AP.
  /// @return Ok if queued, RESOURCE_BUSY if queue full.
  Status enqueueSetWifiApEnabled(bool enabled);

  /// @brief Enqueue RTC set command from non-loop context.
  /// @param time New RTC time payload.
  /// @return Ok if queued, RESOURCE_BUSY if queue full.
  Status enqueueSetRtcTime(const RtcTime& time);

  /// @brief Enqueue SD remount command from non-loop context.
  /// @return Ok if queued, RESOURCE_BUSY if queue full.
  Status enqueueRemountSd();

  /// @brief Enqueue output override mode change.
  /// @param mode Output override mode.
  /// @return Ok if queued, RESOURCE_BUSY if queue full.
  Status enqueueSetOutputOverride(OutputOverrideMode mode);

  /// @brief Enqueue per-channel output test override update.
  /// @param index Channel index (0..3).
  /// @param enabled True to force channel state, false to return channel to auto logic.
  /// @param state Forced state when enabled=true.
  /// @return Ok if queued, RESOURCE_BUSY if queue full, INVALID_CONFIG on bad index.
  Status enqueueSetOutputChannelTest(size_t index, bool enabled, bool state);

  /// @brief Enqueue explicit I2C bus recovery request.
  /// @return Ok if queued, RESOURCE_BUSY if queue full.
  Status enqueueRecoverI2cBus();

  /// @brief Enqueue explicit TF-Luna recovery request.
  /// @return Ok if queued, RESOURCE_BUSY if queue full.
  Status enqueueRecoverLidarSensor();

  /// @brief Enqueue explicit TF-Luna probe/read-now request.
  /// @return Ok if queued, RESOURCE_BUSY if queue full.
  Status enqueueProbeLidarSensor();

  /// @brief Enqueue non-disruptive SD probe (mount-state check).
  /// @return Ok if queued, RESOURCE_BUSY if queue full.
  Status enqueueProbeSdCard();

  /// @brief Enqueue asynchronous I2C bus scan request.
  /// @return Ok if queued, RESOURCE_BUSY if queue full.
  Status enqueueScanI2cBus();

  /// @brief Try to get I2C scan snapshot without blocking.
  /// @param out Snapshot output.
  /// @return true when snapshot copied, false if busy/not initialized.
  bool tryGetI2cScanSnapshot(I2cScanSnapshot& out) const;

  /// @brief Enqueue raw I2C write operation.
  /// @param address 7-bit device address.
  /// @param tx Payload bytes.
  /// @param txLen Number of payload bytes.
  /// @return Ok if queued, RESOURCE_BUSY if queue full.
  Status enqueueI2cRawWrite(uint8_t address, const uint8_t* tx, uint8_t txLen);

  /// @brief Enqueue raw I2C read operation.
  /// @param address 7-bit device address.
  /// @param rxLen Number of bytes to read.
  /// @return Ok if queued, RESOURCE_BUSY if queue full.
  Status enqueueI2cRawRead(uint8_t address, uint8_t rxLen);

  /// @brief Enqueue raw I2C write-read operation.
  /// @param address 7-bit device address.
  /// @param tx Write payload bytes.
  /// @param txLen Number of bytes to write.
  /// @param rxLen Number of bytes to read.
  /// @return Ok if queued, RESOURCE_BUSY if queue full.
  Status enqueueI2cRawWriteRead(uint8_t address,
                                const uint8_t* tx,
                                uint8_t txLen,
                                uint8_t rxLen);

  /// @brief Enqueue raw I2C probe operation for a single address.
  /// @param address 7-bit device address.
  /// @return Ok if queued, RESOURCE_BUSY if queue full.
  Status enqueueI2cProbeAddress(uint8_t address);

  /// @brief Try to get raw I2C operation snapshot without blocking.
  /// @param out Snapshot output.
  /// @return true when snapshot copied, false if busy/not initialized.
  bool tryGetI2cRawSnapshot(I2cRawSnapshot& out) const;

  /// @brief Try to get cached RTC diagnostic snapshot without blocking.
  /// @param out Snapshot output.
  /// @return true when snapshot copied, false if busy/not initialized.
  bool tryGetRtcDebugSnapshot(RtcDebugSnapshot& out) const;

  /// @brief Get current output override mode.
  OutputOverrideMode getOutputOverrideMode() const;

  /// @brief Try to get output channel state (0..3).
  /// @param index Channel index.
  /// @param outState Output state.
  /// @return true when state copied, false on invalid index or lock busy.
  bool tryGetOutputChannelState(size_t index, bool& outState) const;

 private:
  void pushEvent(uint32_t nowMs, uint16_t code, const char* msg);

  HardwareSettings _config{};
  AppSettings _appSettings{};
  RuntimeSettings _settings{};
  bool _initialized = false;

  DeviceStatus _deviceStatus[DEVICE_COUNT]{};
  SystemStatus _systemStatus{};

  struct Impl;
  Impl* _impl = nullptr;
};

}  // namespace TFLunaControl

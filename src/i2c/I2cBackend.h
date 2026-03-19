#pragma once

#include <stddef.h>
#include <stdint.h>

#include "TFLunaControl/Status.h"

namespace TFLunaControl {

struct I2cBackendConfig {
  int sdaPin = -1;
  int sclPin = -1;
  uint32_t freqHz = 400000;
  uint32_t timeoutMs = 20;
};

struct I2cTransfer {
  uint8_t address = 0;
  const uint8_t* txData = nullptr;
  size_t txLen = 0;
  uint8_t* rxData = nullptr;
  size_t rxLen = 0;
  uint32_t timeoutMs = 20;
  bool sendStop = true;
};

class II2cBackend {
 public:
  virtual ~II2cBackend() = default;
  virtual const char* name() const = 0;
  virtual bool supportsDeterministicTimeout() const = 0;
  virtual bool isAvailable() const = 0;
  virtual Status begin(const I2cBackendConfig& config) = 0;
  virtual void end() = 0;
  virtual Status applyConfig(const I2cBackendConfig& config) = 0;
  virtual Status reset(const I2cBackendConfig& config) = 0;
  virtual Status transfer(const I2cTransfer& transfer, uint32_t& durationUs) = 0;
};

class IdfI2cBackend : public II2cBackend {
 public:
  const char* name() const override;
  bool supportsDeterministicTimeout() const override;
  bool isAvailable() const override;
  Status begin(const I2cBackendConfig& config) override;
  void end() override;
  Status applyConfig(const I2cBackendConfig& config) override;
  Status reset(const I2cBackendConfig& config) override;
  Status transfer(const I2cTransfer& transfer, uint32_t& durationUs) override;

 private:
  /// Maximum cached device handles (env, RTC, display, spare).
  static constexpr size_t MAX_CACHED_DEVICES = 6;

  struct DeviceEntry {
    uint8_t address = 0;
    void* handle = nullptr;  ///< Opaque: i2c_master_dev_handle_t on target.
  };

  I2cBackendConfig _config{};
  void* _busHandle = nullptr;  ///< Opaque: i2c_master_bus_handle_t on target.
  DeviceEntry _devices[MAX_CACHED_DEVICES]{};
  size_t _deviceCount = 0;
  bool _started = false;

  /// @brief Get or create a device handle for the given address.
  void* getOrCreateDevice(uint8_t address);

  /// @brief Remove all cached device handles.
  void removeAllDevices();
};

}  // namespace TFLunaControl

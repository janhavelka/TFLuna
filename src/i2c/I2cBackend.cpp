#include "i2c/I2cBackend.h"

#include "core/SystemClock.h"

#ifdef ARDUINO
#include <Arduino.h>
#include <driver/i2c_master.h>
#include <esp_log.h>
#endif

namespace CO2Control {

const char* IdfI2cBackend::name() const {
  return "idf5";
}

bool IdfI2cBackend::supportsDeterministicTimeout() const {
  return true;
}

bool IdfI2cBackend::isAvailable() const {
#ifdef ARDUINO
  return true;
#else
  return false;
#endif
}

void IdfI2cBackend::removeAllDevices() {
#ifdef ARDUINO
  for (size_t i = 0; i < _deviceCount; ++i) {
    if (_devices[i].handle != nullptr) {
      i2c_master_bus_rm_device(static_cast<i2c_master_dev_handle_t>(_devices[i].handle));
      _devices[i].handle = nullptr;
    }
    _devices[i].address = 0;
  }
#endif
  _deviceCount = 0;
}

void* IdfI2cBackend::getOrCreateDevice(uint8_t address) {
#ifdef ARDUINO
  // Look up existing handle.
  for (size_t i = 0; i < _deviceCount; ++i) {
    if (_devices[i].address == address && _devices[i].handle != nullptr) {
      return _devices[i].handle;
    }
  }

  // Evict oldest entry if cache is full.
  if (_deviceCount >= MAX_CACHED_DEVICES) {
    if (_devices[0].handle != nullptr) {
      i2c_master_bus_rm_device(static_cast<i2c_master_dev_handle_t>(_devices[0].handle));
    }
    for (size_t i = 1; i < _deviceCount; ++i) {
      _devices[i - 1] = _devices[i];
    }
    --_deviceCount;
  }

  i2c_device_config_t devCfg{};
  devCfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  devCfg.device_address = address;
  devCfg.scl_speed_hz = _config.freqHz;
  // Convert timeoutMs to microseconds for scl_wait_us.
  devCfg.scl_wait_us = _config.timeoutMs * 1000U;

  i2c_master_dev_handle_t devHandle = nullptr;
  const esp_err_t rc = i2c_master_bus_add_device(
      static_cast<i2c_master_bus_handle_t>(_busHandle), &devCfg, &devHandle);
  if (rc != ESP_OK || devHandle == nullptr) {
    return nullptr;
  }

  _devices[_deviceCount].address = address;
  _devices[_deviceCount].handle = devHandle;
  ++_deviceCount;
  return devHandle;
#else
  (void)address;
  return nullptr;
#endif
}

Status IdfI2cBackend::begin(const I2cBackendConfig& config) {
  _config = config;
#ifdef ARDUINO
  if (config.sdaPin < 0 || config.sclPin < 0) {
    return Status(Err::INVALID_CONFIG, 0, "IDF backend pins invalid");
  }
  if (_started) {
    end();
  }

  i2c_master_bus_config_t busCfg{};
  busCfg.i2c_port = I2C_NUM_0;
  busCfg.sda_io_num = static_cast<gpio_num_t>(config.sdaPin);
  busCfg.scl_io_num = static_cast<gpio_num_t>(config.sclPin);
  busCfg.clk_source = I2C_CLK_SRC_DEFAULT;
  busCfg.glitch_ignore_cnt = 7;
  busCfg.intr_priority = 0;
  busCfg.trans_queue_depth = 0;
  busCfg.flags.enable_internal_pullup = true;

  i2c_master_bus_handle_t busHandle = nullptr;
  esp_err_t rc = i2c_new_master_bus(&busCfg, &busHandle);
  if (rc != ESP_OK || busHandle == nullptr) {
    return Status(Err::HARDWARE_FAULT, rc, "IDF i2c_new_master_bus failed");
  }

  // Suppress IDF I2C driver logs for expected NACKs (missing devices,
  // bus probes). All errors are handled via return codes in our I2C task.
  esp_log_level_set("i2c.master", ESP_LOG_NONE);

  _busHandle = busHandle;
  _deviceCount = 0;
  _started = true;
  return Ok();
#else
  (void)config;
  return Status(Err::NOT_INITIALIZED, 0, "IDF backend unavailable");
#endif
}

void IdfI2cBackend::end() {
#ifdef ARDUINO
  if (_started) {
    removeAllDevices();
    if (_busHandle != nullptr) {
      i2c_del_master_bus(static_cast<i2c_master_bus_handle_t>(_busHandle));
      _busHandle = nullptr;
    }
  }
#endif
  _started = false;
}

Status IdfI2cBackend::applyConfig(const I2cBackendConfig& config) {
  _config = config;
  if (!_started) {
    return Status(Err::NOT_INITIALIZED, 0, "IDF backend not started");
  }
  // New API doesn't support in-place frequency change; reset the bus.
  return reset(config);
}

Status IdfI2cBackend::reset(const I2cBackendConfig& config) {
  end();
  return begin(config);
}

Status IdfI2cBackend::transfer(const I2cTransfer& transfer, uint32_t& durationUs) {
  durationUs = 0;
  if (!_started) {
    return Status(Err::NOT_INITIALIZED, 0, "IDF backend not started");
  }
  if (transfer.address == 0 || transfer.address > 0x7F) {
    return Status(Err::INVALID_CONFIG, 0, "IDF address invalid");
  }
  if ((transfer.txLen > 0 && transfer.txData == nullptr) ||
      (transfer.rxLen > 0 && transfer.rxData == nullptr)) {
    return Status(Err::INVALID_CONFIG, 0, "IDF transfer buffers invalid");
  }

#ifdef ARDUINO
  void* devRaw = getOrCreateDevice(transfer.address);
  if (devRaw == nullptr) {
    return Status(Err::HARDWARE_FAULT, 0, "IDF device handle alloc failed");
  }
  i2c_master_dev_handle_t dev = static_cast<i2c_master_dev_handle_t>(devRaw);

  const uint32_t startedUs = SystemClock::nowUs();
  const int timeoutMs = static_cast<int>(transfer.timeoutMs);
  esp_err_t rc = ESP_OK;

  if (transfer.txLen == 0 && transfer.rxLen == 0) {
    // Probe: write zero bytes to check ACK.
    rc = i2c_master_probe(
        static_cast<i2c_master_bus_handle_t>(_busHandle),
        transfer.address, timeoutMs);
  } else if (transfer.txLen > 0 && transfer.rxLen > 0) {
    // Write-then-read (repeated start).
    rc = i2c_master_transmit_receive(dev,
                                     transfer.txData, transfer.txLen,
                                     transfer.rxData, transfer.rxLen,
                                     timeoutMs);
  } else if (transfer.txLen > 0) {
    // Write only.
    rc = i2c_master_transmit(dev,
                             transfer.txData, transfer.txLen,
                             timeoutMs);
  } else {
    // Read only.
    rc = i2c_master_receive(dev,
                            transfer.rxData, transfer.rxLen,
                            timeoutMs);
  }

  durationUs = SystemClock::nowUs() - startedUs;

  if (rc == ESP_OK) {
    return Ok();
  }
  if (rc == ESP_ERR_TIMEOUT) {
    return Status(Err::TIMEOUT, rc, "IDF i2c timeout");
  }
  return Status(Err::COMM_FAILURE, rc, "IDF i2c transfer failed");
#else
  (void)transfer;
  durationUs = 100;
  return Status(Err::NOT_INITIALIZED, 0, "IDF backend unavailable");
#endif
}

}  // namespace CO2Control

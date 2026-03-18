#pragma once

#include "CO2Control/I2cRaw.h"
#include "core/CommandQueue.h"
#include "i2c/I2cBackend.h"
#include "i2c/I2cGpioProbe.h"
#include "i2c/I2cRequests.h"
#include "i2c/RecoveryPolicy.h"

#ifndef CO2CONTROL_ENABLE_DISPLAY
#define CO2CONTROL_ENABLE_DISPLAY 0
#endif

#if defined(ARDUINO) && __has_include(<BME280/BME280.h>)
#define CO2CONTROL_HAS_BME280_LIB 1
#include <BME280/BME280.h>
#else
#define CO2CONTROL_HAS_BME280_LIB 0
#endif

#if defined(ARDUINO) && __has_include(<SHT3x/SHT3x.h>)
#define CO2CONTROL_HAS_SHT3X_LIB 1
#include <SHT3x/SHT3x.h>
#else
#define CO2CONTROL_HAS_SHT3X_LIB 0
#endif

#if defined(ARDUINO) && __has_include(<RV3032/RV3032.h>)
#define CO2CONTROL_HAS_RV3032_LIB 1
#include <RV3032/RV3032.h>
#else
#define CO2CONTROL_HAS_RV3032_LIB 0
#endif

#if CO2CONTROL_ENABLE_DISPLAY && defined(ARDUINO) && __has_include(<SSD1315.h>)
#define CO2CONTROL_HAS_SSD1315_LIB 1
#include <SSD1315.h>
#else
#define CO2CONTROL_HAS_SSD1315_LIB 0
#endif

#if CO2CONTROL_HAS_BME280_LIB
static_assert(BME280::VERSION_CODE >= 10201, "CO2Control requires BME280 >= v1.2.1");
#endif

#if CO2CONTROL_HAS_SHT3X_LIB
static_assert(SHT3x::VERSION_CODE >= 10400, "CO2Control requires SHT3x >= v1.4.0");
#endif

#if CO2CONTROL_HAS_RV3032_LIB
#if __has_include(<RV3032/Version.h>)
#include <RV3032/Version.h>
static_assert(RV3032::VERSION_CODE >= 10300, "CO2Control requires RV3032-C7 >= v1.3.0");
#endif
#endif

#if CO2CONTROL_HAS_SSD1315_LIB
#if __has_include(<ssd1315/Version.h>)
#include <ssd1315/Version.h>
static_assert(SSD1315::VERSION_CODE >= 10100, "CO2Control requires SSD1315 >= v1.1.0");
#endif
#endif

#if CO2CONTROL_HAS_SSD1315_LIB
namespace SSD1315Api = SSD1315;
#endif

#ifdef ARDUINO
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#endif

namespace CO2Control {

class I2cTask : public II2cRequestPort {
 public:
  static constexpr size_t REQUEST_QUEUE_CAPACITY = HardwareSettings::I2C_REQUEST_QUEUE_CAPACITY;
  static constexpr size_t RESULT_QUEUE_CAPACITY = HardwareSettings::I2C_RESULT_QUEUE_CAPACITY;

  Status begin(const HardwareSettings& config, const RuntimeSettings& settings) override;
  void end() override;

  Status enqueue(const I2cRequest& request, uint32_t nowMs) override;
  bool dequeueResult(I2cResult& out) override;

  void tick(uint32_t nowMs) override;
  void applySettings(const RuntimeSettings& settings, uint32_t nowMs) override;

  I2cBusMetrics getMetrics() const override;
  HealthState health() const override;
  RtcDebugSnapshot getRtcDebugSnapshot() const;

  /// @brief Inject backend for deterministic native tests.
  /// @note Must be called before begin().
  void setBackendForTest(II2cBackend* backend) { _backendOverride = backend; }

  /// @brief Inject power-cycle hook for deterministic native tests.
  /// @note Must be called before begin().
  void setPowerCycleHookForTest(I2cPowerCycleHook hook, void* context) {
    _powerHookOverride = hook;
    _powerHookContextOverride = context;
  }

  /// @brief Force stuck-bus preflight result for native tests.
  void setForceBusStuckForTest(bool stuck) { _forceBusStuck = stuck; }

 private:
  I2cResult processRequest(const I2cRequest& request, uint32_t nowMs);
  bool queueResult(const I2cResult& result, uint32_t nowMs);
  Status recoverBus(uint32_t nowMs);
  bool isBusStuck(uint32_t nowMs) const;
  void updateMetricsSuccess(uint32_t durationUs, uint32_t nowMs);
  void updateMetricsError(const Status& status, uint32_t nowMs);
  void markRequestOverflow(uint32_t nowMs);
  void markResultDrop(uint32_t nowMs);
  void updateTaskHeartbeat(uint32_t nowMs);
  bool isRecent(uint32_t nowMs, uint32_t timestampMs, uint32_t windowMs) const;
  RtcDebugSnapshot makeRtcDebugBase(uint32_t nowMs) const;
  void storeRtcDebugSnapshot(const RtcDebugSnapshot& snapshot);
  void resetRtcDebugSnapshot(uint32_t nowMs);
  void refreshRtcDebugSnapshot(uint32_t nowMs);
  II2cBackend* selectBackend();

#if CO2CONTROL_HAS_BME280_LIB
  static BME280::Status bmeI2cWriteThunk(uint8_t addr,
                                         const uint8_t* data,
                                         size_t len,
                                         uint32_t timeoutMs,
                                         void* user);
  static BME280::Status bmeI2cWriteReadThunk(uint8_t addr,
                                             const uint8_t* txData,
                                             size_t txLen,
                                             uint8_t* rxData,
                                             size_t rxLen,
                                             uint32_t timeoutMs,
                                             void* user);
  BME280::Status bmeI2cWrite(uint8_t addr,
                             const uint8_t* data,
                             size_t len,
                             uint32_t timeoutMs);
  BME280::Status bmeI2cWriteRead(uint8_t addr,
                                 const uint8_t* txData,
                                 size_t txLen,
                                 uint8_t* rxData,
                                 size_t rxLen,
                                 uint32_t timeoutMs);
  Status mapBmeStatus(const BME280::Status& status) const;
  Status ensureBmeReady(uint8_t address, uint32_t nowMs);
  Status handleBmeTrigger(const I2cRequest& request, I2cResult& result, uint32_t nowMs);
  Status handleBmeRead(const I2cRequest& request, I2cResult& result, uint32_t nowMs);
#endif

#if CO2CONTROL_HAS_SHT3X_LIB
  static SHT3x::Status shtI2cWriteThunk(uint8_t addr,
                                        const uint8_t* data,
                                        size_t len,
                                        uint32_t timeoutMs,
                                        void* user);
  static SHT3x::Status shtI2cWriteReadThunk(uint8_t addr,
                                            const uint8_t* txData,
                                            size_t txLen,
                                            uint8_t* rxData,
                                            size_t rxLen,
                                            uint32_t timeoutMs,
                                            void* user);
  SHT3x::Status shtI2cWrite(uint8_t addr,
                            const uint8_t* data,
                            size_t len,
                            uint32_t timeoutMs);
  SHT3x::Status shtI2cWriteRead(uint8_t addr,
                                const uint8_t* txData,
                                size_t txLen,
                                uint8_t* rxData,
                                size_t rxLen,
                                uint32_t timeoutMs);
  Status mapShtStatus(const SHT3x::Status& status) const;
  Status ensureShtReady(uint8_t address, uint32_t nowMs);
  Status handleShtTrigger(const I2cRequest& request, I2cResult& result, uint32_t nowMs);
  Status handleShtRead(const I2cRequest& request, I2cResult& result, uint32_t nowMs);
#endif

#if CO2CONTROL_HAS_RV3032_LIB
  static RV3032::Status rtcI2cWriteThunk(uint8_t addr,
                                         const uint8_t* data,
                                         size_t len,
                                         uint32_t timeoutMs,
                                         void* user);
  static RV3032::Status rtcI2cWriteReadThunk(uint8_t addr,
                                             const uint8_t* txData,
                                             size_t txLen,
                                             uint8_t* rxData,
                                             size_t rxLen,
                                             uint32_t timeoutMs,
                                             void* user);
  RV3032::Status rtcI2cWrite(uint8_t addr,
                             const uint8_t* data,
                             size_t len,
                             uint32_t timeoutMs);
  RV3032::Status rtcI2cWriteRead(uint8_t addr,
                                 const uint8_t* txData,
                                 size_t txLen,
                                 uint8_t* rxData,
                                 size_t rxLen,
                                 uint32_t timeoutMs);
  Status mapRtcStatus(const RV3032::Status& status) const;
  Status ensureRtcReady(uint8_t address, uint32_t nowMs);
  Status handleRtcRead(const I2cRequest& request, I2cResult& result, uint32_t nowMs);
  Status handleRtcSet(const I2cRequest& request, I2cResult& result, uint32_t nowMs);
#endif

#if CO2CONTROL_HAS_SSD1315_LIB
  static SSD1315Api::Status displayI2cWriteThunk(uint8_t addr,
                                              const uint8_t* data,
                                              size_t len,
                                              uint32_t timeoutMs,
                                              void* user);
  SSD1315Api::Status displayI2cWrite(uint8_t addr,
                                  const uint8_t* data,
                                  size_t len,
                                  uint32_t timeoutMs);
  Status mapDisplayStatus(const SSD1315Api::Status& status) const;
  Status ensureDisplayReady(uint8_t address, uint32_t nowMs);
  Status handleDisplayRefresh(const I2cRequest& request, I2cResult& result, uint32_t nowMs);
  void renderDisplayFrame(uint32_t nowMs);
#endif

#ifdef ARDUINO
  static void taskEntry(void* arg);
  void taskLoop();
  mutable SemaphoreHandle_t _metricsMutex = nullptr;
  SemaphoreHandle_t _completionSem = nullptr;
  QueueHandle_t _requestQueue = nullptr;
  QueueHandle_t _resultQueue = nullptr;
  TaskHandle_t _taskHandle = nullptr;
#else
  CommandQueue<I2cRequest, REQUEST_QUEUE_CAPACITY> _requestQueueNative;
  CommandQueue<I2cResult, RESULT_QUEUE_CAPACITY> _resultQueueNative;
#endif

  mutable I2cBusMetrics _metrics = {};
  HardwareSettings _config{};
  RuntimeSettings _settings{};
  bool _running = false;
  bool _enabled = false;
  uint32_t _nextToken = 1;
  RecoveryPolicy _recoveryPolicy{};
  I2cGpioProbe _gpioProbe{};
  I2cPowerCycleHook _powerHook = nullptr;
  void* _powerHookContext = nullptr;
  I2cPowerCycleHook _powerHookOverride = nullptr;
  void* _powerHookContextOverride = nullptr;
  I2cBackendConfig _backendConfig{};
  II2cBackend* _backend = nullptr;
  II2cBackend* _backendOverride = nullptr;
  IdfI2cBackend _idfBackend{};
  bool _forceBusStuck = false;
  RtcDebugSnapshot _rtcDebug{};

#if CO2CONTROL_HAS_BME280_LIB
  BME280::BME280 _envBme{};
  bool _envBmeInitialized = false;
  uint8_t _envBmeAddress = 0;
  uint32_t _envBmeTimeoutMs = 0;
#endif

#if CO2CONTROL_HAS_SHT3X_LIB
  SHT3x::SHT3x _envSht{};
  bool _envShtInitialized = false;
  uint8_t _envShtAddress = 0;
  uint32_t _envShtTimeoutMs = 0;
#endif

#if CO2CONTROL_HAS_RV3032_LIB
  RV3032::RV3032 _rtcRv3032{};
  bool _rtcRv3032Initialized = false;
  uint8_t _rtcRv3032Address = 0;
  uint32_t _rtcRv3032TimeoutMs = 0;
#endif

#if CO2CONTROL_HAS_SSD1315_LIB
  SSD1315Api::SSD1315 _display{};
  bool _displayInitialized = false;
  uint8_t _displayAddress = 0;
  uint32_t _displayTimeoutMs = 0;
  uint8_t _displayOfflineThreshold = 0;
  uint32_t _displayNextRecoverMs = 0;

  Status _displayRtcStatus = Status(Err::NOT_INITIALIZED, 0, "RTC waiting first sample");
  Status _displayEnvStatus = Status(Err::NOT_INITIALIZED, 0, "ENV waiting first sample");
  bool _displayRtcValid = false;
  RtcTime _displayRtc{};
  uint32_t _displayRtcSampleMs = 0;
  bool _displayEnvValid = false;
  float _displayEnvTempC = 0.0f;
  float _displayEnvRhPct = 0.0f;
  float _displayEnvPressureHpa = 0.0f;
  bool _displayEnvHasPressure = false;
  uint32_t _displayEnvSampleMs = 0;
  bool _displayCo2Valid = false;
  float _displayCo2Ppm = 0.0f;
  uint32_t _displayCo2SampleMs = 0;
  uint8_t _displayOutputMask = 0;
  OutputOverrideMode _displayOutputMode = OutputOverrideMode::AUTO;
  bool _displayOutputsEnabled = false;
  bool _displayLogEnabled = false;
  bool _displayLogMounted = false;
  bool _displayLogHealthy = false;
  uint32_t _displayLogSamplesWritten = 0;
  HealthState _displaySystemHealth = HealthState::UNKNOWN;
#endif

  uint32_t _lastRequestOverflowMs = 0;
  uint32_t _lastResultDropMs = 0;
  uint32_t _lastStaleResultMs = 0;
  uint32_t _slowWindowStartMs = 0;
  uint32_t _slowWindowCount = 0;

};

}  // namespace CO2Control

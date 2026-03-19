/**
 * @file HardwareSettings.h
 * @brief Hardware settings for TFLunaControl.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "TFLunaControl/Status.h"

namespace TFLunaControl {

/// @brief Optional board-level callback used to power-cycle I2C peripherals.
/// @param nowMs Current monotonic time in milliseconds.
/// @param userContext Opaque pointer provided by application.
/// @return Ok on successful power-cycle, NOT_INITIALIZED when unsupported, or other error.
using I2cPowerCycleHook = Status (*)(uint32_t nowMs, void* userContext);

/**
 * @brief Boot-time hardware configuration for TFLunaControl initialization.
 *
 * All pin values default to -1 (disabled). Define only the pins your
 * application uses. Board-specific values belong in application code.
 * This hardware configuration is distinct from runtime settings in RuntimeSettings.h.
 * Application-level board defaults are typically loaded from src/config/AppConfig.cpp.
 */
struct HardwareSettings {
  /// @brief Number of output channels exposed by OutputController.
  static constexpr size_t OUTPUT_CHANNEL_COUNT = 4;

  /// @brief Build-time sample history capacity (ring buffer slots).
  static constexpr size_t SAMPLE_HISTORY_CAPACITY = 150;
  /// @brief PSRAM target sample history capacity (ring buffer slots).
  static constexpr size_t SAMPLE_HISTORY_CAPACITY_PSRAM = 4096;

  /// @brief Build-time event history capacity (ring buffer slots).
  static constexpr size_t EVENT_HISTORY_CAPACITY = 64;
  /// @brief PSRAM target event history capacity (ring buffer slots).
  static constexpr size_t EVENT_HISTORY_CAPACITY_PSRAM = 1024;

  /// @brief Build-time app command queue capacity.
  static constexpr size_t COMMAND_QUEUE_CAPACITY = 8;

  /// @brief Build-time I2C request queue capacity.
  static constexpr size_t I2C_REQUEST_QUEUE_CAPACITY = 16;

  /// @brief Build-time I2C result queue capacity.
  static constexpr size_t I2C_RESULT_QUEUE_CAPACITY = 16;

  /// @brief Build-time SD sample log queue capacity.
  static constexpr size_t SD_LOG_QUEUE_CAPACITY = 512;
  /// @brief PSRAM target SD sample log queue capacity.
  /// @note Prioritize sample backlog depth over event backlog depth.
  static constexpr size_t SD_LOG_QUEUE_CAPACITY_PSRAM = 8192;

  /// @brief Build-time SD event log queue capacity.
  static constexpr size_t SD_EVENT_QUEUE_CAPACITY = 64;
  /// @brief PSRAM target SD event log queue capacity.
  /// @note Events are sparse transition/error records, so keep this moderate.
  static constexpr size_t SD_EVENT_QUEUE_CAPACITY_PSRAM = 512;

  /// @brief Build-time serial CLI maximum line length.
  static constexpr size_t CLI_LINE_BYTES = 192;

  /// @brief Build-time serial CLI maximum parsed tokens.
  static constexpr size_t CLI_MAX_TOKENS = 24;

  /// @brief Build-time serial CLI maximum samples printed per command.
  static constexpr size_t CLI_MAX_SAMPLE_PRINT = 16;

  /// @brief Build-time serial CLI maximum events printed per command.
  static constexpr size_t CLI_MAX_EVENT_PRINT = 16;

  /// @brief Build-time serial CLI RX byte budget per poll.
  static constexpr size_t CLI_RX_BUDGET_PER_POLL = 96;

  /// @brief Build-time web graph endpoint maximum returned samples.
  static constexpr size_t WEB_MAX_GRAPH_SAMPLES = SAMPLE_HISTORY_CAPACITY;
  /// @brief PSRAM target web graph endpoint maximum returned samples.
  static constexpr size_t WEB_MAX_GRAPH_SAMPLES_PSRAM = 1000;

  /// @brief Build-time web events endpoint maximum returned events.
  static constexpr size_t WEB_MAX_EVENT_COUNT = EVENT_HISTORY_CAPACITY;
  /// @brief PSRAM target web events endpoint maximum returned events.
  static constexpr size_t WEB_MAX_EVENT_COUNT_PSRAM = 256;

  /// @brief Build-time web graph scratch RAM hard cap.
  static constexpr size_t WEB_MAX_GRAPH_SCRATCH_BYTES = 16U * 1024U;

  /// @brief Build-time web events scratch RAM hard cap.
  static constexpr size_t WEB_MAX_EVENT_SCRATCH_BYTES = 8U * 1024U;

  /// @brief Build-time total web scratch RAM hard cap.
  static constexpr size_t WEB_MAX_TOTAL_SCRATCH_BYTES = 24U * 1024U;

  /// @brief Build-time max I2C payload bytes for request/result buffers.
  static constexpr size_t I2C_PAYLOAD_BYTES = 16;

  /// @brief Build-time SD sample CSV line buffer bytes.
  static constexpr size_t SD_SAMPLE_LINE_BYTES = 128;

  /// @brief Build-time SD event CSV line buffer bytes.
  static constexpr size_t SD_EVENT_LINE_BYTES = 192;

  /// @brief Build-time SD path scratch bytes.
  static constexpr size_t SD_PATH_BYTES = 64;

  /// @brief Build-time JSON doc size for `/api/status` and WS status payload prep.
  static constexpr size_t WEB_STATUS_JSON_DOC_BYTES = 6144;

  /// @brief Build-time JSON doc size for compact live WS status payloads.
  static constexpr size_t WEB_LIVE_STATUS_JSON_DOC_BYTES = 3072;

  /// @brief Build-time JSON doc size for `/api/settings` payloads.
  static constexpr size_t WEB_SETTINGS_JSON_DOC_BYTES = 4608;

  /// @brief Build-time JSON doc size for `/api/devices` item objects.
  static constexpr size_t WEB_DEVICE_ITEM_JSON_DOC_BYTES = 384;

  /// @brief Build-time JSON doc size for `/api/graph` item objects.
  static constexpr size_t WEB_GRAPH_ITEM_JSON_DOC_BYTES = 256;

  /// @brief Build-time JSON doc size for `/api/events` item objects.
  static constexpr size_t WEB_EVENT_ITEM_JSON_DOC_BYTES = 224;

  /// @brief Build-time JSON doc size for `/api/rtc/set` request parse.
  static constexpr size_t WEB_RTC_JSON_DOC_BYTES = 256;

  /// @brief Build-time WebSocket status payload buffer bytes.
  static constexpr size_t WEB_STATUS_WS_BUFFER_BYTES = 6144;

  /// @brief Build-time compact live WebSocket payload buffer bytes.
  static constexpr size_t WEB_LIVE_STATUS_WS_BUFFER_BYTES = 3072;

  /// @brief I2C SDA pin. Set to -1 to disable I2C.
  /// @note Application-provided. Library does not define pin defaults.
  int i2cSda = -1;

  /// @brief I2C SCL pin. Set to -1 to disable I2C.
  /// @note Application-provided. Library does not define pin defaults.
  int i2cScl = -1;

  /// @brief FreeRTOS stack size (bytes) for dedicated I2C task.
  /// @note IDF 5.x has deeper internal call stacks; 5120 prevents overflow
  ///       while keeping the bump conservative (was 4096).
  uint16_t i2cTaskStack = 5120;

  /// @brief FreeRTOS priority for dedicated I2C task.
  uint8_t i2cTaskPriority = 2;

  /// @brief Optional board-level hook for I2C peripheral power-cycle escalation.
  /// @note Leave null for default no-op behavior.
  I2cPowerCycleHook i2cPowerCycleHook = nullptr;

  /// @brief Opaque context passed to i2cPowerCycleHook.
  void* i2cPowerCycleContext = nullptr;

  /// @brief Number of SCL pulses for stuck-bus recovery (9-16 recommended).
  uint8_t i2cRecoveryPulses = 9;

  /// @brief SCL pulse HIGH duration for stuck-bus recovery.
  uint32_t i2cRecoveryPulseHighUs = 5;

  /// @brief SCL pulse LOW duration for stuck-bus recovery.
  uint32_t i2cRecoveryPulseLowUs = 5;

  /// @brief SPI SCK pin for SD. Set to -1 to disable SD.
  /// @note Application-provided. Library does not define pin defaults.
  int spiSck = -1;

  /// @brief SPI MISO pin for SD. Set to -1 to disable SD.
  /// @note Application-provided. Library does not define pin defaults.
  int spiMiso = -1;

  /// @brief SPI MOSI pin for SD. Set to -1 to disable SD.
  /// @note Application-provided. Library does not define pin defaults.
  int spiMosi = -1;

  /// @brief SD card chip-select pin. Set to -1 to disable SD.
  /// @note Application-provided. Library does not define pin defaults.
  int sdCs = -1;

  /// @brief SD card detect pin. Set to -1 if not used.
  /// @note Application-provided. Library does not define pin defaults.
  int sdCdPin = -1;

  /// @brief SD card detect pin active-low flag.
  bool sdCdActiveLow = true;

  /// @brief EE871 E2 TX pin. Set to -1 to disable E2 bus.
  int e2Tx = -1;

  /// @brief EE871 E2 RX pin. Set to -1 to disable E2 bus.
  int e2Rx = -1;

  /// @brief Optional EE871 enable/power pin. Set to -1 if not used.
  int e2En = -1;

  /// @brief TF-Luna UART RX pin on the ESP32. Connect TF-Luna TX here.
  int lidarRx = -1;

  /// @brief TF-Luna UART TX pin on the ESP32. Connect TF-Luna RX here.
  int lidarTx = -1;

  /// @brief Hardware UART index used for TF-Luna on ESP32 targets.
  uint8_t lidarUartIndex = 1;

  /// @brief OLED horizontal orientation flag. True mirrors the display on X.
  bool displayFlipX = false;

  /// @brief OLED vertical orientation flag. True mirrors the display on Y.
  bool displayFlipY = false;

  /// @brief Button GPIO pin. Set to -1 to disable button.
  int buttonPin = -1;

  /// @brief Button active-low flag.
  bool buttonActiveLow = true;

  /// @brief Button debounce time in milliseconds.
  uint32_t buttonDebounceMs = 30;

  /// @brief Long press threshold in milliseconds.
  uint32_t buttonLongPressMs = 2000;

  /// @brief Multi-press window in milliseconds.
  uint32_t buttonMultiPressWindowMs = 8000;

  /// @brief Multi-press count to trigger reset.
  uint8_t buttonMultiPressCount = 5;

  /// @brief WS2812 data pin. Set to -1 to disable LEDs.
  int ledPin = -1;

  /// @brief Total WS2812 LED count on the strip.
  uint16_t ledCount = 2;

  /// @brief LED index for WiFi/Web status.
  uint8_t wifiLedIndex = 0;

  /// @brief LED index for system health.
  uint8_t healthLedIndex = 1;

  /// @brief LED brightness (0-255).
  uint8_t ledBrightness = 20;

  /// @brief LED transition smoothing step in milliseconds.
  uint16_t ledSmoothStepMs = 20;

  /// @brief MOSFET output 1 pin. Set to -1 to disable.
  int mosfet1Pin = -1;

  /// @brief MOSFET output 2 pin. Set to -1 to disable.
  int mosfet2Pin = -1;

  /// @brief Relay output 1 pin. Set to -1 to disable.
  int relay1Pin = -1;

  /// @brief Relay output 2 pin. Set to -1 to disable.
  int relay2Pin = -1;

  /// @brief MOSFET output 1 active-high flag.
  bool mosfet1ActiveHigh = true;

  /// @brief MOSFET output 2 active-high flag.
  bool mosfet2ActiveHigh = true;

  /// @brief Relay output 1 active-high flag.
  bool relay1ActiveHigh = true;

  /// @brief Relay output 2 active-high flag.
  bool relay2ActiveHigh = true;

};

}  // namespace TFLunaControl

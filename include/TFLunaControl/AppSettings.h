/**
 * @file AppSettings.h
 * @brief Boot-time software/service settings for TFLunaControl.
 */

#pragma once

#include <stdint.h>

#ifndef TFLUNACTRL_ENABLE_DISPLAY
#define TFLUNACTRL_ENABLE_DISPLAY 0
#endif

namespace TFLunaControl {

/**
 * @brief Boot-time software/service configuration.
 *
 * This struct intentionally excludes board pin mapping and electrical settings.
 * Hardware-specific values belong in HardwareSettings.
 */
struct AppSettings {
  /// @brief Enable serial transport used by firmware CLI/status output.
  bool serialEnabled = true;

  /// @brief Serial baud rate used in firmware `setup()`.
  uint32_t serialBaudRate = 115200;

  /// @brief Enable NVS settings storage.
  bool enableNvs = false;

  /// @brief Enable SD card logging.
  bool enableSd = false;

  /// @brief Enable SoftAP web server.
  bool enableWeb = true;

  /// @brief Enable OLED display pipeline (I2C display refresh requests).
  bool enableDisplay = (TFLUNACTRL_ENABLE_DISPLAY != 0);

  /// @brief SoftAP web server TCP port.
  uint16_t webPort = 80;

  /// @brief WebSocket broadcast interval in milliseconds.
  uint32_t webBroadcastMs = 1000;

  /// @brief UI WebSocket reconnect delay (served by `/` HTML page).
  uint32_t webUiWsReconnectMs = 2000;

  /// @brief UI graph refresh interval (served by `/` HTML page).
  uint32_t webUiGraphRefreshMs = 5000;

  /// @brief UI events refresh interval (served by `/` HTML page).
  uint32_t webUiEventsRefreshMs = 10000;

  /// @brief UI event fetch count for `/api/events`.
  uint16_t webUiEventFetchCount = 12;

  /// @brief Max wait for state mutex lock in cooperative tick context.
  uint32_t stateMutexTimeoutMs = 10;

  /// @brief AsyncSD worker priority.
  /// Priority 0 prevents FreeRTOS round-robin time-slicing from
  /// preempting the main tick on single-core targets (ESP32-S2),
  /// which otherwise inflates measured tick duration.  The worker
  /// runs during the main-loop delay(1) yield slot instead.
  uint8_t sdWorkerPriority = 0;

  /// @brief AsyncSD worker stack size.
  /// Increased from 4096 to reduce stack pressure in deep SdFat error paths.
  uint16_t sdWorkerStackBytes = 6144;

  /// @brief AsyncSD worker idle delay.
  uint16_t sdWorkerIdleMs = 2;

  /// @brief AsyncSD request queue depth.
  uint8_t sdRequestQueueDepth = 12;

  /// @brief AsyncSD result queue depth.
  uint8_t sdResultQueueDepth = 12;

  /// @brief AsyncSD open file cap.
  uint8_t sdMaxOpenFiles = 3;

  /// @brief AsyncSD max path length.
  uint16_t sdMaxPathLength = 96;

  /// @brief AsyncSD max copy-write bytes per request.
  uint16_t sdMaxCopyWriteBytes = 256;

  /// @brief AsyncSD copy-write slot count.
  uint8_t sdCopyWriteSlots = 2;

  /// @brief AsyncSD lock timeout.
  uint16_t sdLockTimeoutMs = 20;

  /// @brief AsyncSD mount timeout.
  /// Increased to 10 s for FAT32 cards that initialise slowly via SPI.
  uint16_t sdMountTimeoutMs = 10000;

  /// @brief AsyncSD generic-operation timeout (Info, Stat, Mkdir, â€¦).
  /// Must be long enough that an enqueued Info request is not expired
  /// while waiting behind a file-write job on slow cards.
  uint16_t sdOpTimeoutMs = 10000;

  /// @brief AsyncSD low-level I/O timeout.
  uint16_t sdIoTimeoutMs = 5000;

  /// @brief AsyncSD chunk size for copy I/O.
  uint16_t sdIoChunkBytes = 256;

  /// @brief SPI clock frequency for SD card (Hz).
  /// 20 MHz keeps comfortable headroom for sustained high-rate CSV logging
  /// on this board without pushing all cards to the 25 MHz edge.
  uint32_t sdSpiFrequencyHz = 20000000;

  /// @brief AsyncSD worker stall watchdog timeout (ms). 0 = disabled.
  /// Disabled by default because the FAT32 free-cluster scan in
  /// SdFat's freeClusterCount() can block the worker for many seconds
  /// on large cards and there is no way to interrupt it.
  uint32_t sdWorkerStallMs = 0;

  /// @brief AsyncSD shutdown timeout while waiting for worker stop (ms).
  /// Longer timeout helps avoid teardown races under heavy SD fault conditions.
  uint32_t sdShutdownTimeoutMs = 5000;
};

}  // namespace TFLunaControl

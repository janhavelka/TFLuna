#pragma once

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include "TFLunaControl/HardwareSettings.h"
#include "TFLunaControl/RuntimeSettings.h"
#include "TFLunaControl/Status.h"
#include "TFLunaControl/Types.h"

namespace TFLunaControl {

class TFLunaControl;

/**
 * @brief SoftAP web server with REST endpoints and WebSocket updates.
 */
class WebServer {
 public:
  static constexpr size_t MAX_GRAPH_SAMPLES = HardwareSettings::WEB_MAX_GRAPH_SAMPLES;
  static constexpr size_t MAX_EVENT_COUNT = HardwareSettings::WEB_MAX_EVENT_COUNT;
  static constexpr size_t MAX_GRAPH_SAMPLES_PSRAM = HardwareSettings::WEB_MAX_GRAPH_SAMPLES_PSRAM;
  static constexpr size_t MAX_EVENT_COUNT_PSRAM = HardwareSettings::WEB_MAX_EVENT_COUNT_PSRAM;
  static constexpr size_t GRAPH_SCRATCH_BYTES = sizeof(Sample) * MAX_GRAPH_SAMPLES;
  static constexpr size_t EVENT_SCRATCH_BYTES = sizeof(Event) * MAX_EVENT_COUNT;
  static constexpr size_t MAX_GRAPH_SCRATCH_BYTES = HardwareSettings::WEB_MAX_GRAPH_SCRATCH_BYTES;
  static constexpr size_t MAX_EVENT_SCRATCH_BYTES = HardwareSettings::WEB_MAX_EVENT_SCRATCH_BYTES;
  static constexpr size_t MAX_TOTAL_SCRATCH_BYTES = HardwareSettings::WEB_MAX_TOTAL_SCRATCH_BYTES;

  static_assert(GRAPH_SCRATCH_BYTES <= MAX_GRAPH_SCRATCH_BYTES,
                "Graph scratch RAM exceeded safety budget");
  static_assert(EVENT_SCRATCH_BYTES <= MAX_EVENT_SCRATCH_BYTES,
                "Event scratch RAM exceeded safety budget");
  static_assert(GRAPH_SCRATCH_BYTES + EVENT_SCRATCH_BYTES <= MAX_TOTAL_SCRATCH_BYTES,
                "Combined scratch RAM exceeded safety budget");

  /// @brief Clamp requested graph sample count to safe bounds.
  static size_t clampGraphCount(size_t requested, size_t capacity = MAX_GRAPH_SAMPLES);

  /// @brief Clamp requested event count to safe bounds.
  static size_t clampEventCount(size_t requested, size_t capacity = MAX_EVENT_COUNT);

  Status begin(TFLunaControl* app);
  void end();

  Status startAp(const RuntimeSettings& settings);
  void stopAp();

  void tick(uint32_t nowMs);

  /// @brief Send pending WS broadcast.  Call from processDeferred() so
  ///        the blocking TCP send does not inflate tick timing.
  void broadcastDeferred(uint32_t nowMs);

  void setBroadcastInterval(uint32_t intervalMs) {
    if (intervalMs == 0U) {
      _broadcastIntervalMs = 500U;
      return;
    }
    if (intervalMs > static_cast<uint32_t>(INT32_MAX)) {
      _broadcastIntervalMs = static_cast<uint32_t>(INT32_MAX);
      return;
    }
    _broadcastIntervalMs = intervalMs;
  }
  void setPort(uint16_t port) { _port = (port == 0U) ? 80U : port; }
  void setPsramAvailable(bool available) { _psramAvailable = available; }
  void setUiRefreshTiming(uint32_t wsReconnectMs, uint32_t graphRefreshMs, uint32_t eventsRefreshMs);
  void setUiEventFetchCount(uint16_t eventCount);

  bool isApRunning() const { return _apRunning; }
  uint8_t stationCount() const { return _cachedStationCount; }
  int16_t averageStationRssiDbm() const { return _cachedRssiDbm; }
  uint8_t apChannel() const { return _cachedChannel; }
  size_t webClientCount() const;
  size_t graphScratchCapacity() const;
  size_t eventScratchCapacity() const;
  bool webScratchUsingPsram() const;
  bool hasRecentUiActivity(uint32_t nowMs, uint32_t windowMs) const;

  /// @brief Refresh cached WiFi stats from ESP-IDF APIs.
  /// Call from tick or broadcastDeferred at a low rate (e.g. 1 Hz).
  void refreshWifiStatsIfDue(uint32_t nowMs);

 private:
  struct WebServerImpl;
  Status setupHandlers();
  void noteUiActivity();
  void refreshWifiStatsNow();

  TFLunaControl* _app = nullptr;
  WebServerImpl* _impl = nullptr;
  bool _apRunning = false;
  uint32_t _lastBroadcastMs = 0;
  uint32_t _broadcastIntervalMs = 1000;
  uint16_t _port = 80;
  uint32_t _uiWsReconnectMs = 2000;
  uint32_t _uiGraphRefreshMs = 5000;
  uint32_t _uiEventsRefreshMs = 10000;
  uint16_t _uiEventFetchCount = 12;
  bool _mdnsRunning = false;
  uint32_t _mdnsNextRetryMs = 0;
  bool _serverStarted = false;
  uint32_t _apRestartGuardUntilMs = 0;
  uint32_t _lastWsCleanupMs = 0;
  uint32_t _lastUiActivityMs = 0;
  uint32_t _lastWifiStatsMs = 0;
  uint8_t _cachedStationCount = 0;
  int16_t _cachedRssiDbm = -127;
  uint8_t _cachedChannel = 0;
  bool _psramAvailable = false;
};

}  // namespace TFLunaControl

#include "web/WebServer.h"

#include "TFLunaControl/TFLunaControl.h"

namespace TFLunaControl {

size_t WebServer::clampGraphCount(size_t requested, size_t capacity) {
  if (capacity == 0U) {
    return 0U;
  }
  if (requested == 0 || requested > capacity) {
    return capacity;
  }
  return requested;
}

size_t WebServer::clampEventCount(size_t requested, size_t capacity) {
  if (capacity == 0U) {
    return 0U;
  }
  if (requested == 0 || requested > capacity) {
    return capacity;
  }
  return requested;
}

void WebServer::setUiRefreshTiming(uint32_t wsReconnectMs,
                                   uint32_t graphRefreshMs,
                                   uint32_t eventsRefreshMs) {
  _uiWsReconnectMs = (wsReconnectMs == 0U) ? 2000U : wsReconnectMs;
  _uiGraphRefreshMs = (graphRefreshMs == 0U) ? 5000U : graphRefreshMs;
  _uiEventsRefreshMs = (eventsRefreshMs == 0U) ? 10000U : eventsRefreshMs;
}

void WebServer::setUiEventFetchCount(uint16_t eventCount) {
  if (eventCount == 0U) {
    _uiEventFetchCount = 1U;
    return;
  }
  if (_impl == nullptr) {
    _uiEventFetchCount = eventCount;
    return;
  }
  size_t cap = eventScratchCapacity();
  if (cap == 0U) {
    cap = MAX_EVENT_COUNT;
  }
  const size_t clamped = clampEventCount(static_cast<size_t>(eventCount), cap);
  _uiEventFetchCount = static_cast<uint16_t>(clamped);
}

}  // namespace TFLunaControl

#if defined(ARDUINO) && TFLUNACTRL_ENABLE_WEB
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#if __has_include(<ESPmDNS.h>)
#define TFLUNACTRL_HAS_MDNS 1
#include <ESPmDNS.h>
#else
#define TFLUNACTRL_HAS_MDNS 0
#endif
#if __has_include(<esp_wifi.h>)
#define TFLUNACTRL_HAS_ESP_WIFI_API 1
#include <esp_wifi.h>
#else
#define TFLUNACTRL_HAS_ESP_WIFI_API 0
#endif
#include <freertos/semphr.h>
#include <memory>
#include <new>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "core/ApiJson.h"
#include "core/PsramSupport.h"
#include "web/WebLockOrder.h"
#include "web/WebPages.h"

namespace TFLunaControl {

namespace {

// Local AP hostname exposed as http://ustredna.local when mDNS is available.
static constexpr const char* kApHostname = "ustredna";
// Guard AP restarts after stop to avoid WiFi stop/start races.
// Empirically, S3 + lwIP can still race on shorter windows under heavy churn.
static constexpr uint32_t kApRestartGuardMs = 5000U;
static constexpr uint32_t kWsCleanupIntervalMs = 2000U;
static constexpr size_t kDeviceActionBodyMaxBytes = 256U;

AsyncWebSocketSharedBuffer makeWsSharedBuffer(const char* payload, size_t len) {
  if (payload == nullptr || len == 0U) {
    return AsyncWebSocketSharedBuffer{};
  }

  // Use nothrow new so OOM returns nullptr instead of crashing via std::bad_alloc.
  auto* vec = new (std::nothrow) std::vector<uint8_t>();
  if (vec == nullptr) {
    return AsyncWebSocketSharedBuffer{};
  }
  try {
    vec->resize(len);
  } catch (...) {
    delete vec;
    return AsyncWebSocketSharedBuffer{};
  }
  memcpy(vec->data(), payload, len);
  // Wrap in shared_ptr with custom deleter (avoids make_shared's throwing allocator).
  return AsyncWebSocketSharedBuffer(vec);
}

void addNoStoreHeaders(AsyncWebServerResponse* response) {
  if (response == nullptr) {
    return;
  }
  response->addHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  response->addHeader("Pragma", "no-cache");
  response->addHeader("Connection", "close");
}

bool appendRequestBodyChunk(AsyncWebServerRequest* request,
                            uint8_t* data,
                            size_t len,
                            size_t index,
                            size_t total,
                            size_t hardLimit) {
  if (request == nullptr || data == nullptr) {
    return false;
  }
  if (total == 0U || total > hardLimit) {
    request->abort();
    return false;
  }
  if (index == 0U) {
    if (request->_tempObject != nullptr) {
      free(request->_tempObject);
      request->_tempObject = nullptr;
    }
    request->_tempObject = calloc(total + 1U, sizeof(char));
    if (request->_tempObject == nullptr) {
      request->abort();
      return false;
    }
  }
  if (request->_tempObject == nullptr ||
      index > total ||
      len > (total - index)) {
    request->abort();
    return false;
  }
  char* buffer = static_cast<char*>(request->_tempObject);
  memcpy(buffer + index, data, len);
  buffer[total] = '\0';
  return true;
}

String extractRequestBody(AsyncWebServerRequest* request) {
  if (request == nullptr) {
    return String();
  }

  String body;
  const AsyncWebParameter* plain = request->getParam("plain", true);
  if (plain != nullptr) {
    body = plain->value();
  }
  if (body.isEmpty()) {
    const String argPlain = request->arg("plain");
    if (!argPlain.isEmpty()) {
      body = argPlain;
    }
  }
  if (body.isEmpty() && request->_tempObject != nullptr) {
    const char* raw = static_cast<const char*>(request->_tempObject);
    if (raw != nullptr && raw[0] != '\0') {
      body = raw;
    }
  }
  return body;
}

const char* rawOpToStr(I2cRawOp op) {
  switch (op) {
    case I2cRawOp::WRITE:
      return "write";
    case I2cRawOp::READ:
      return "read";
    case I2cRawOp::WRITE_READ:
      return "write_read";
    case I2cRawOp::PROBE:
      return "probe";
    case I2cRawOp::NONE:
    default:
      return "none";
  }
}

}  // namespace

struct WebServer::WebServerImpl : public IWebTryLock {
  enum class JsonStreamKind : uint8_t {
    NONE = 0,
    GRAPH,
    EVENTS
  };

  enum class JsonStreamPhase : uint8_t {
    PREFIX = 0,
    ITEM,
    COMMA,
    SUFFIX,
    DONE
  };

  struct JsonStreamState {
    static constexpr size_t ITEM_BUFFER_BYTES = 512U;
    bool active = false;
    JsonStreamKind kind = JsonStreamKind::NONE;
    JsonStreamPhase phase = JsonStreamPhase::DONE;
    size_t totalItems = 0U;
    size_t itemIndex = 0U;
    size_t tokenOffset = 0U;
    size_t itemLen = 0U;
    char itemBuffer[ITEM_BUFFER_BYTES] = {0};
  };

  static constexpr size_t MAX_TRACKED_WS_CLIENTS = static_cast<size_t>(DEFAULT_MAX_WS_CLIENTS);
  static constexpr uint8_t MAX_STALLED_WS_FRAMES = 8U;

  struct WsClientState {
    uint32_t id = 0U;
    uint8_t stalledFrames = 0U;
  };

  AsyncWebServer server;
  AsyncWebSocket ws;
  SemaphoreHandle_t scratchMutex = nullptr;
  Sample* graphScratch = nullptr;
  Event* eventScratch = nullptr;
  size_t graphScratchCapacity = 0U;
  size_t eventScratchCapacity = 0U;
  bool scratchUsingPsram = false;
  JsonStreamState jsonStream{};
  WsClientState wsClients[MAX_TRACKED_WS_CLIENTS] = {};
  // Guard against concurrent index page builds (~112 KB heap copy each).
  volatile bool indexPageBusy = false;

  explicit WebServerImpl(uint16_t port) : server(port), ws("/ws") {
    scratchMutex = xSemaphoreCreateMutex();
  }

  void clearWsClients() {
    for (size_t i = 0; i < MAX_TRACKED_WS_CLIENTS; ++i) {
      wsClients[i] = WsClientState{};
    }
  }

  bool noteWsClientConnected(uint32_t id) {
    if (id == 0U) {
      return false;
    }
    for (size_t i = 0; i < MAX_TRACKED_WS_CLIENTS; ++i) {
      if (wsClients[i].id == id) {
        wsClients[i].stalledFrames = 0U;
        return true;
      }
    }
    for (size_t i = 0; i < MAX_TRACKED_WS_CLIENTS; ++i) {
      if (wsClients[i].id == 0U) {
        wsClients[i].id = id;
        wsClients[i].stalledFrames = 0U;
        return true;
      }
    }
    return false;
  }

  void noteWsClientDisconnected(uint32_t id) {
    if (id == 0U) {
      return;
    }
    for (size_t i = 0; i < MAX_TRACKED_WS_CLIENTS; ++i) {
      if (wsClients[i].id == id) {
        wsClients[i] = WsClientState{};
        return;
      }
    }
  }

  void pruneWsClients() {
    for (size_t i = 0; i < MAX_TRACKED_WS_CLIENTS; ++i) {
      if (wsClients[i].id != 0U && !ws.hasClient(wsClients[i].id)) {
        wsClients[i] = WsClientState{};
      }
    }
  }

  void releaseScratch() {
    if (graphScratch != nullptr) {
      PsramSupport::freeMemory(graphScratch);
      graphScratch = nullptr;
    }
    if (eventScratch != nullptr) {
      PsramSupport::freeMemory(eventScratch);
      eventScratch = nullptr;
    }
    graphScratchCapacity = 0U;
    eventScratchCapacity = 0U;
    scratchUsingPsram = false;
  }

  bool allocateScratch(bool preferPsram) {
    releaseScratch();

    struct ScratchTier {
      size_t graphCapacity;
      size_t eventCapacity;
      bool usePsram;
    };

    ScratchTier tiers[2] = {};
    size_t tierCount = 0U;
    if (preferPsram) {
      tiers[tierCount++] = {MAX_GRAPH_SAMPLES_PSRAM, MAX_EVENT_COUNT_PSRAM, true};
    }
    tiers[tierCount++] = {MAX_GRAPH_SAMPLES, MAX_EVENT_COUNT, false};

    for (size_t i = 0U; i < tierCount; ++i) {
      const ScratchTier tier = tiers[i];
      if (tier.graphCapacity == 0U || tier.eventCapacity == 0U) {
        continue;
      }
      if (tier.graphCapacity > (SIZE_MAX / sizeof(Sample)) ||
          tier.eventCapacity > (SIZE_MAX / sizeof(Event))) {
        continue;
      }
      const size_t graphBytes = tier.graphCapacity * sizeof(Sample);
      const size_t eventBytes = tier.eventCapacity * sizeof(Event);
      void* graphMem = tier.usePsram ? PsramSupport::allocPsram(graphBytes)
                                     : PsramSupport::allocInternal(graphBytes);
      if (graphMem == nullptr) {
        continue;
      }
      void* eventMem = tier.usePsram ? PsramSupport::allocPsram(eventBytes)
                                     : PsramSupport::allocInternal(eventBytes);
      if (eventMem == nullptr) {
        PsramSupport::freeMemory(graphMem);
        continue;
      }

      memset(graphMem, 0, graphBytes);
      memset(eventMem, 0, eventBytes);
      graphScratch = static_cast<Sample*>(graphMem);
      eventScratch = static_cast<Event*>(eventMem);
      graphScratchCapacity = tier.graphCapacity;
      eventScratchCapacity = tier.eventCapacity;
      scratchUsingPsram = tier.usePsram;
      return true;
    }
    return false;
  }

  ~WebServerImpl() {
    releaseJsonStream();
    releaseScratch();
    clearWsClients();
    if (scratchMutex != nullptr) {
      vSemaphoreDelete(scratchMutex);
      scratchMutex = nullptr;
    }
  }

  bool scratchReady() const {
    return scratchMutex != nullptr &&
           graphScratch != nullptr &&
           eventScratch != nullptr &&
           graphScratchCapacity > 0U &&
           eventScratchCapacity > 0U;
  }

  bool tryLock() override {
    if (scratchMutex == nullptr) {
      return false;
    }
    return xSemaphoreTake(scratchMutex, 0) == pdTRUE;
  }

  void unlock() override {
    if (scratchMutex != nullptr) {
      xSemaphoreGive(scratchMutex);
    }
  }

  void resetJsonStream() {
    jsonStream.active = false;
    jsonStream.kind = JsonStreamKind::NONE;
    jsonStream.phase = JsonStreamPhase::DONE;
    jsonStream.totalItems = 0U;
    jsonStream.itemIndex = 0U;
    jsonStream.tokenOffset = 0U;
    jsonStream.itemLen = 0U;
    jsonStream.itemBuffer[0] = '\0';
  }

  bool beginJsonStream(JsonStreamKind kind, size_t totalItems) {
    if (kind == JsonStreamKind::NONE) {
      return false;
    }
    if (jsonStream.active) {
      return false;
    }
    if (!scratchReady()) {
      return false;
    }
    if ((kind == JsonStreamKind::GRAPH && totalItems > graphScratchCapacity) ||
        (kind == JsonStreamKind::EVENTS && totalItems > eventScratchCapacity)) {
      return false;
    }
    jsonStream.active = true;
    jsonStream.kind = kind;
    jsonStream.phase = JsonStreamPhase::PREFIX;
    jsonStream.totalItems = totalItems;
    jsonStream.itemIndex = 0U;
    jsonStream.tokenOffset = 0U;
    jsonStream.itemLen = 0U;
    jsonStream.itemBuffer[0] = '\0';
    return true;
  }

  void releaseJsonStream() {
    if (!jsonStream.active) {
      return;
    }
    resetJsonStream();
    unlock();
  }

  bool prepareCurrentJsonItem() {
    if (!jsonStream.active || jsonStream.phase != JsonStreamPhase::ITEM) {
      return false;
    }
    if (jsonStream.itemIndex >= jsonStream.totalItems) {
      return false;
    }
    size_t written = 0U;
    if (jsonStream.kind == JsonStreamKind::GRAPH) {
      if (graphScratch == nullptr || jsonStream.itemIndex >= graphScratchCapacity) {
        return false;
      }
      StaticJsonDocument<HardwareSettings::WEB_GRAPH_ITEM_JSON_DOC_BYTES> itemDoc;
      populateGraphSampleJson(itemDoc, graphScratch[jsonStream.itemIndex]);
      const size_t needed = measureJson(itemDoc);
      if (needed == 0U || needed >= JsonStreamState::ITEM_BUFFER_BYTES) {
        return false;
      }
      written = serializeJson(itemDoc, jsonStream.itemBuffer, JsonStreamState::ITEM_BUFFER_BYTES);
    } else if (jsonStream.kind == JsonStreamKind::EVENTS) {
      if (eventScratch == nullptr || jsonStream.itemIndex >= eventScratchCapacity) {
        return false;
      }
      StaticJsonDocument<HardwareSettings::WEB_EVENT_ITEM_JSON_DOC_BYTES> itemDoc;
      itemDoc["ts"] = eventScratch[jsonStream.itemIndex].tsUnix;
      if (eventScratch[jsonStream.itemIndex].tsLocal[0] != '\0') {
        itemDoc["ts_local"] = eventScratch[jsonStream.itemIndex].tsLocal;
      } else {
        itemDoc["ts_local"] = nullptr;
      }
      itemDoc["code"] = eventScratch[jsonStream.itemIndex].code;
      itemDoc["msg"] = eventScratch[jsonStream.itemIndex].msg;
      const size_t needed = measureJson(itemDoc);
      if (needed == 0U || needed >= JsonStreamState::ITEM_BUFFER_BYTES) {
        return false;
      }
      written = serializeJson(itemDoc, jsonStream.itemBuffer, JsonStreamState::ITEM_BUFFER_BYTES);
    } else {
      return false;
    }

    if (written == 0U || written >= JsonStreamState::ITEM_BUFFER_BYTES) {
      return false;
    }
    jsonStream.itemLen = written;
    jsonStream.tokenOffset = 0U;
    return true;
  }

  static size_t appendToken(uint8_t* dst,
                            size_t dstLen,
                            size_t dstOffset,
                            const char* src,
                            size_t srcLen,
                            size_t& srcOffset) {
    if (dst == nullptr || src == nullptr || dstOffset >= dstLen || srcOffset >= srcLen) {
      return 0U;
    }
    const size_t room = dstLen - dstOffset;
    const size_t remaining = srcLen - srcOffset;
    const size_t n = (remaining < room) ? remaining : room;
    memcpy(dst + dstOffset, src + srcOffset, n);
    srcOffset += n;
    return n;
  }

  size_t fillJsonStream(uint8_t* dst, size_t dstLen, size_t index) {
    (void)index;
    if (dst == nullptr || dstLen == 0U) {
      return 0U;
    }
    if (!jsonStream.active) {
      return 0U;
    }

    static constexpr const char* kGraphPrefix = "{\"samples\":[";
    static constexpr const char* kEventsPrefix = "{\"events\":[";
    static constexpr const char* kComma = ",";
    static constexpr const char* kSuffix = "]}";

    size_t written = 0U;
    while (written < dstLen) {
      if (jsonStream.phase == JsonStreamPhase::PREFIX) {
        const char* prefix =
            (jsonStream.kind == JsonStreamKind::EVENTS) ? kEventsPrefix : kGraphPrefix;
        const size_t prefixLen = strlen(prefix);
        written += appendToken(dst, dstLen, written, prefix, prefixLen, jsonStream.tokenOffset);
        if (jsonStream.tokenOffset >= prefixLen) {
          jsonStream.tokenOffset = 0U;
          jsonStream.phase =
              (jsonStream.totalItems > 0U) ? JsonStreamPhase::ITEM : JsonStreamPhase::SUFFIX;
        } else {
          break;
        }
        continue;
      }

      if (jsonStream.phase == JsonStreamPhase::ITEM) {
        if (jsonStream.itemLen == 0U && !prepareCurrentJsonItem()) {
          // Truncate cleanly on unexpected serialization failure.
          jsonStream.totalItems = jsonStream.itemIndex;
          jsonStream.phase = JsonStreamPhase::SUFFIX;
          jsonStream.tokenOffset = 0U;
          continue;
        }
        written += appendToken(dst,
                               dstLen,
                               written,
                               jsonStream.itemBuffer,
                               jsonStream.itemLen,
                               jsonStream.tokenOffset);
        if (jsonStream.tokenOffset >= jsonStream.itemLen) {
          jsonStream.tokenOffset = 0U;
          jsonStream.itemLen = 0U;
          jsonStream.itemIndex++;
          jsonStream.phase = (jsonStream.itemIndex < jsonStream.totalItems)
                                 ? JsonStreamPhase::COMMA
                                 : JsonStreamPhase::SUFFIX;
        } else {
          break;
        }
        continue;
      }

      if (jsonStream.phase == JsonStreamPhase::COMMA) {
        written += appendToken(dst, dstLen, written, kComma, 1U, jsonStream.tokenOffset);
        if (jsonStream.tokenOffset >= 1U) {
          jsonStream.tokenOffset = 0U;
          jsonStream.phase = JsonStreamPhase::ITEM;
        } else {
          break;
        }
        continue;
      }

      if (jsonStream.phase == JsonStreamPhase::SUFFIX) {
        const size_t suffixLen = 2U;
        written += appendToken(dst, dstLen, written, kSuffix, suffixLen, jsonStream.tokenOffset);
        if (jsonStream.tokenOffset >= suffixLen) {
          jsonStream.tokenOffset = 0U;
          jsonStream.phase = JsonStreamPhase::DONE;
        } else {
          break;
        }
        continue;
      }

      if (jsonStream.phase == JsonStreamPhase::DONE) {
        releaseJsonStream();
        break;
      }
    }
    return written;
  }
};

Status WebServer::setupHandlers() {
  if (_impl == nullptr || _app == nullptr) {
    return Status(Err::NOT_INITIALIZED, 0, "web impl not ready");
  }

  _impl->ws.onEvent([this](AsyncWebSocket* server,
                           AsyncWebSocketClient* client,
                           AwsEventType type,
                           void* arg,
                           uint8_t* data,
                           size_t len) {
    (void)server;
    (void)client;
    (void)arg;
    (void)data;
    (void)len;
    if (type == WS_EVT_CONNECT) {
      if (_impl != nullptr && client != nullptr) {
        client->setCloseClientOnQueueFull(false);
        client->keepAlivePeriod(15);
        if (!_impl->noteWsClientConnected(client->id())) {
          client->close(1013, "server busy");
          return;
        }
      }
      noteUiActivity();
      return;
    }
    if (type == WS_EVT_DISCONNECT) {
      if (_impl != nullptr && client != nullptr) {
        _impl->noteWsClientDisconnected(client->id());
      }
      return;
    }
    if (type == WS_EVT_DATA || type == WS_EVT_PONG) {
      noteUiActivity();
    }
  });
  _impl->server.addHandler(&_impl->ws);

  _impl->server.on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
    AsyncWebServerResponse* response =
        request->beginResponse(200, "text/html; charset=utf-8", kIndexHtml);
    if (response == nullptr) {
      request->send(503, "text/plain", "Page alloc failed");
      return;
    }
    addNoStoreHeaders(response);
    request->send(response);
  });

  _impl->server.on("/api/ui/heartbeat", HTTP_POST, [this](AsyncWebServerRequest* request) {
    noteUiActivity();
    request->send(204);
  });

  _impl->server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (_app == nullptr) {
      request->send(503, "text/plain", "App not ready");
      return;
    }
    SystemStatus status{};
    Sample latest{};
    bool hasLatest = false;
    if (!_app->tryGetStatusSnapshot(status, latest, hasLatest)) {
      request->send(503, "text/plain", "State busy");
      return;
    }
    AsyncResponseStream* response = request->beginResponseStream("application/json");
    if (response == nullptr) {
      request->send(503, "text/plain", "Response alloc failed");
      return;
    }
    StaticJsonDocument<HardwareSettings::WEB_STATUS_JSON_DOC_BYTES> doc;
    populateStatusJson(doc, status, hasLatest ? &latest : nullptr);
    serializeJson(doc, *response);
    addNoStoreHeaders(response);
    request->send(response);
  });

  _impl->server.on("/api/devices", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (_app == nullptr) {
      request->send(503, "text/plain", "App not ready");
      return;
    }
    DeviceStatus statuses[DEVICE_COUNT] = {};
    size_t count = 0;
    if (!_app->tryCopyDeviceStatuses(statuses, DEVICE_COUNT, count)) {
      request->send(503, "text/plain", "State busy");
      return;
    }
    AsyncResponseStream* response = request->beginResponseStream("application/json");
    if (response == nullptr) {
      request->send(503, "text/plain", "Response alloc failed");
      return;
    }
    response->print("{\"devices\":[");
    bool first = true;
    for (size_t i = 0; i < count; ++i) {
      const DeviceStatus& st = statuses[i];
      StaticJsonDocument<HardwareSettings::WEB_DEVICE_ITEM_JSON_DOC_BYTES> itemDoc;
      // Use the DeviceId enum value as stable identifier.
      populateDeviceStatusJson(itemDoc, st, static_cast<uint32_t>(st.id));
      if (!first) {
        response->print(",");
      }
      serializeJson(itemDoc, *response);
      first = false;
    }
    response->print("]}");
    addNoStoreHeaders(response);
    request->send(response);
  });

  _impl->server.on("/api/i2c/raw", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (_app == nullptr) {
      request->send(503, "text/plain", "App not ready");
      return;
    }
    I2cRawSnapshot raw{};
    if (!_app->tryGetI2cRawSnapshot(raw)) {
      request->send(503, "text/plain", "State busy");
      return;
    }
    StaticJsonDocument<512> doc;
    doc["queued"] = raw.queued;
    doc["active"] = raw.active;
    doc["complete"] = raw.complete;
    doc["op"] = rawOpToStr(raw.op);
    doc["address"] = raw.address;
    doc["updated_ms"] = raw.updatedMs;
    doc["queued_ms"] = raw.queuedMs;
    doc["last_status_code"] = static_cast<uint16_t>(raw.lastStatus.code);
    doc["last_status_detail"] = raw.lastStatus.detail;
    doc["last_status_msg"] = raw.lastStatus.msg;
    doc["rx_len"] = raw.rxLen;
    AsyncResponseStream* response = request->beginResponseStream("application/json");
    if (response == nullptr) {
      request->send(503, "text/plain", "Response alloc failed");
      return;
    }
    serializeJson(doc, *response);
    addNoStoreHeaders(response);
    request->send(response);
  });

  _impl->server.on("/api/i2c/scan", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (_app == nullptr) {
      request->send(503, "text/plain", "App not ready");
      return;
    }
    I2cScanSnapshot scan{};
    if (!_app->tryGetI2cScanSnapshot(scan)) {
      request->send(503, "text/plain", "State busy");
      return;
    }
    StaticJsonDocument<768> doc;
    doc["active"] = scan.active;
    doc["complete"] = scan.complete;
    doc["next_address"] = scan.nextAddress;
    doc["started_ms"] = scan.startedMs;
    doc["updated_ms"] = scan.updatedMs;
    doc["probes_total"] = scan.probesTotal;
    doc["probes_timeout"] = scan.probesTimeout;
    doc["probes_error"] = scan.probesError;
    doc["probes_nack"] = scan.probesNack;
    doc["last_status_code"] = static_cast<uint16_t>(scan.lastStatus.code);
    doc["last_status_detail"] = scan.lastStatus.detail;
    doc["last_status_msg"] = scan.lastStatus.msg;
    JsonArray found = doc.createNestedArray("found");
    const uint8_t count = (scan.foundCount > I2cScanSnapshot::MAX_FOUND)
                              ? static_cast<uint8_t>(I2cScanSnapshot::MAX_FOUND)
                              : scan.foundCount;
    for (uint8_t i = 0; i < count; ++i) {
      found.add(scan.foundAddresses[i]);
    }
    AsyncResponseStream* response = request->beginResponseStream("application/json");
    if (response == nullptr) {
      request->send(503, "text/plain", "Response alloc failed");
      return;
    }
    serializeJson(doc, *response);
    addNoStoreHeaders(response);
    request->send(response);
  });

  _impl->server.on("/api/device/recover", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (_app == nullptr) {
      request->send(503, "text/plain", "App not ready");
      return;
    }
    const String body = extractRequestBody(request);
    if (body.isEmpty()) {
      request->send(400, "text/plain", "Missing body");
      return;
    }
    StaticJsonDocument<256> doc;
    const DeserializationError err = deserializeJson(doc, body);
    if (err) {
      request->send(400, "text/plain", "Invalid JSON");
      return;
    }
    const char* dev = doc["device"].as<const char*>();
    if (dev == nullptr || dev[0] == '\0') {
      request->send(400, "text/plain", "Missing device");
      return;
    }

    Status st = Status(Err::INVALID_CONFIG, 0, "unsupported device");
    if (strcmp(dev, "i2c_bus") == 0 || strcmp(dev, "env") == 0 || strcmp(dev, "rtc") == 0) {
      st = _app->enqueueRecoverI2cBus();
    } else if (strcmp(dev, "lidar") == 0 || strcmp(dev, "tfluna") == 0 ||
               strcmp(dev, "co2") == 0) {
      st = _app->enqueueRecoverLidarSensor();
    } else if (strcmp(dev, "sd") == 0) {
      st = _app->enqueueRemountSd();
    }
    if (!st.ok()) {
      if (st.code == Err::RESOURCE_BUSY) {
        request->send(503, "text/plain", st.msg);
      } else {
        request->send(400, "text/plain", st.msg);
      }
      return;
    }

    StaticJsonDocument<192> res;
    res["queued"] = true;
    res["device"] = dev;
    res["action"] = "recover";
    AsyncResponseStream* response = request->beginResponseStream("application/json");
    if (response == nullptr) {
      request->send(503, "text/plain", "Response alloc failed");
      return;
    }
    serializeJson(res, *response);
    addNoStoreHeaders(response);
    request->send(response);
  }, nullptr, [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
    (void)appendRequestBodyChunk(request, data, len, index, total, kDeviceActionBodyMaxBytes);
  });

  _impl->server.on("/api/device/probe", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (_app == nullptr) {
      request->send(503, "text/plain", "App not ready");
      return;
    }
    const String body = extractRequestBody(request);
    if (body.isEmpty()) {
      request->send(400, "text/plain", "Missing body");
      return;
    }
    StaticJsonDocument<256> doc;
    const DeserializationError err = deserializeJson(doc, body);
    if (err) {
      request->send(400, "text/plain", "Invalid JSON");
      return;
    }
    const char* dev = doc["device"].as<const char*>();
    if (dev == nullptr || dev[0] == '\0') {
      request->send(400, "text/plain", "Missing device");
      return;
    }

    StaticJsonDocument<256> res;
    res["queued"] = false;
    res["device"] = dev;
    res["action"] = "probe";
    res["queued_ms"] = millis();

    if (strcmp(dev, "i2c_bus") == 0) {
      const Status st = _app->enqueueScanI2cBus();
      if (!st.ok()) {
        if (st.code == Err::RESOURCE_BUSY) {
          request->send(503, "text/plain", st.msg);
        } else {
          request->send(400, "text/plain", st.msg);
        }
        return;
      }
      res["queued"] = true;
      res["mode"] = "scan";
      AsyncResponseStream* response = request->beginResponseStream("application/json");
      if (response == nullptr) {
        request->send(503, "text/plain", "Response alloc failed");
        return;
      }
      serializeJson(res, *response);
      addNoStoreHeaders(response);
      request->send(response);
      return;
    }

    if (strcmp(dev, "lidar") == 0 || strcmp(dev, "tfluna") == 0 ||
        strcmp(dev, "co2") == 0) {
      const Status st = _app->enqueueProbeLidarSensor();
      if (!st.ok()) {
        if (st.code == Err::RESOURCE_BUSY) {
          request->send(503, "text/plain", st.msg);
        } else {
          request->send(400, "text/plain", st.msg);
        }
        return;
      }
      res["queued"] = true;
      res["mode"] = "device_probe";
      AsyncResponseStream* response = request->beginResponseStream("application/json");
      if (response == nullptr) {
        request->send(503, "text/plain", "Response alloc failed");
        return;
      }
      serializeJson(res, *response);
      addNoStoreHeaders(response);
      request->send(response);
      return;
    }

    if (strcmp(dev, "sd") == 0) {
      const Status st = _app->enqueueProbeSdCard();
      if (!st.ok()) {
        if (st.code == Err::RESOURCE_BUSY) {
          request->send(503, "text/plain", st.msg);
        } else {
          request->send(400, "text/plain", st.msg);
        }
        return;
      }
      res["queued"] = true;
      res["mode"] = "device_probe";
      AsyncResponseStream* response = request->beginResponseStream("application/json");
      if (response == nullptr) {
        request->send(503, "text/plain", "Response alloc failed");
        return;
      }
      serializeJson(res, *response);
      addNoStoreHeaders(response);
      request->send(response);
      return;
    }

    RuntimeSettings settings{};
    if (!_app->tryGetSettingsSnapshot(settings)) {
      request->send(503, "text/plain", "State busy");
      return;
    }

    uint8_t address = 0;
    if (strcmp(dev, "env") == 0) {
      address = settings.i2cEnvAddress;
    } else if (strcmp(dev, "rtc") == 0) {
      address = settings.i2cRtcAddress;
    } else {
      request->send(400, "text/plain", "unsupported device");
      return;
    }

    const Status st = _app->enqueueI2cProbeAddress(address);
    if (!st.ok()) {
      if (st.code == Err::RESOURCE_BUSY) {
        request->send(503, "text/plain", st.msg);
      } else {
        request->send(400, "text/plain", st.msg);
      }
      return;
    }

    res["queued"] = true;
    res["mode"] = "probe";
    res["address"] = address;
    AsyncResponseStream* response = request->beginResponseStream("application/json");
    if (response == nullptr) {
      request->send(503, "text/plain", "Response alloc failed");
      return;
    }
    serializeJson(res, *response);
    addNoStoreHeaders(response);
    request->send(response);
  }, nullptr, [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
    (void)appendRequestBodyChunk(request, data, len, index, total, kDeviceActionBodyMaxBytes);
  });

  _impl->server.on("/api/device/reset_stats", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (_app == nullptr) {
      request->send(503, "text/plain", "App not ready");
      return;
    }
    const String body = extractRequestBody(request);
    if (body.isEmpty()) {
      request->send(400, "text/plain", "Missing body");
      return;
    }
    StaticJsonDocument<256> doc;
    const DeserializationError err = deserializeJson(doc, body);
    if (err) {
      request->send(400, "text/plain", "Invalid JSON");
      return;
    }
    const char* dev = doc["device"].as<const char*>();
    if (dev == nullptr || dev[0] == '\0') {
      request->send(400, "text/plain", "Missing device");
      return;
    }
    if (strcmp(dev, "lidar") != 0 && strcmp(dev, "tfluna") != 0 &&
        strcmp(dev, "co2") != 0) {
      request->send(400, "text/plain", "unsupported device");
      return;
    }

    const Status st = _app->enqueueResetLidarStats();
    if (!st.ok()) {
      if (st.code == Err::RESOURCE_BUSY) {
        request->send(503, "text/plain", st.msg);
      } else {
        request->send(400, "text/plain", st.msg);
      }
      return;
    }

    StaticJsonDocument<192> res;
    res["queued"] = true;
    res["device"] = dev;
    res["action"] = "reset_stats";
    res["queued_ms"] = millis();
    AsyncResponseStream* response = request->beginResponseStream("application/json");
    if (response == nullptr) {
      request->send(503, "text/plain", "Response alloc failed");
      return;
    }
    serializeJson(res, *response);
    addNoStoreHeaders(response);
    request->send(response);
  }, nullptr, [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
    (void)appendRequestBodyChunk(request, data, len, index, total, kDeviceActionBodyMaxBytes);
  });

  _impl->server.on("/api/output/test", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (_app == nullptr) {
      request->send(503, "text/plain", "App not ready");
      return;
    }
    const String body = extractRequestBody(request);
    if (body.isEmpty()) {
      request->send(400, "text/plain", "Missing body");
      return;
    }
    StaticJsonDocument<256> doc;
    const DeserializationError err = deserializeJson(doc, body);
    if (err) {
      request->send(400, "text/plain", "Invalid JSON");
      return;
    }
    if (!doc.containsKey("channel")) {
      request->send(400, "text/plain", "Missing channel");
      return;
    }
    if (!doc.containsKey("state")) {
      request->send(400, "text/plain", "Missing state");
      return;
    }

    const int32_t channel = doc["channel"].as<int32_t>();
    if (channel < 0 || channel >= static_cast<int32_t>(HardwareSettings::OUTPUT_CHANNEL_COUNT)) {
      request->send(400, "text/plain", "Invalid channel");
      return;
    }
    const bool enabled = doc.containsKey("enabled") ? doc["enabled"].as<bool>() : true;
    const bool state = doc["state"].as<bool>();
    const Status st = _app->enqueueSetOutputChannelTest(static_cast<size_t>(channel), enabled, state);
    if (!st.ok()) {
      if (st.code == Err::RESOURCE_BUSY) {
        request->send(503, "text/plain", st.msg);
      } else {
        request->send(400, "text/plain", st.msg);
      }
      return;
    }

    StaticJsonDocument<192> res;
    res["queued"] = true;
    res["channel"] = static_cast<uint32_t>(channel);
    res["enabled"] = enabled;
    res["state"] = state;
    res["queued_ms"] = millis();
    AsyncResponseStream* response = request->beginResponseStream("application/json");
    if (response == nullptr) {
      request->send(503, "text/plain", "Response alloc failed");
      return;
    }
    serializeJson(res, *response);
    addNoStoreHeaders(response);
    request->send(response);
  }, nullptr, [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
    (void)appendRequestBodyChunk(request, data, len, index, total, kDeviceActionBodyMaxBytes);
  });

  _impl->server.on("/api/settings", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (_app == nullptr) {
      request->send(503, "text/plain", "App not ready");
      return;
    }
    RuntimeSettings s{};
    if (!_app->tryGetSettingsSnapshot(s)) {
      request->send(503, "text/plain", "State busy");
      return;
    }
    StaticJsonDocument<HardwareSettings::WEB_SETTINGS_JSON_DOC_BYTES> doc;
    populateSettingsJson(doc, s);

    AsyncResponseStream* response = request->beginResponseStream("application/json");
    if (response == nullptr) {
      request->send(503, "text/plain", "Response alloc failed");
      return;
    }
    serializeJson(doc, *response);
    addNoStoreHeaders(response);
    request->send(response);
  });

  _impl->server.on("/api/settings/defaults", HTTP_GET, [this](AsyncWebServerRequest* request) {
    (void)this;
    RuntimeSettings defaults{};
    StaticJsonDocument<HardwareSettings::WEB_SETTINGS_JSON_DOC_BYTES> doc;
    populateSettingsJson(doc, defaults);
    AsyncResponseStream* response = request->beginResponseStream("application/json");
    if (response == nullptr) {
      request->send(503, "text/plain", "Response alloc failed");
      return;
    }
    serializeJson(doc, *response);
    addNoStoreHeaders(response);
    request->send(response);
  });

  _impl->server.on("/api/settings/reset", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (_app == nullptr) {
      request->send(503, "text/plain", "App not ready");
      return;
    }

    bool persist = true;
    if (request->contentLength() > 128U) {
      request->send(413, "text/plain", "Payload too large");
      return;
    }
    const String body = extractRequestBody(request);
    if (!body.isEmpty()) {
      StaticJsonDocument<128> doc;
      const DeserializationError err = deserializeJson(doc, body);
      if (err) {
        request->send(400, "text/plain", "Invalid JSON");
        return;
      }
      if (doc.containsKey("persist")) {
        persist = doc["persist"].as<bool>();
      }
    }

    RuntimeSettings defaults{};
    defaults.restoreDefaults();
    const Status st = _app->enqueueApplySettings(defaults, persist, "factory_reset");
    if (!st.ok()) {
      if (st.code == Err::RESOURCE_BUSY) {
        request->send(503, "text/plain", st.msg);
      } else {
        request->send(400, "text/plain", st.msg);
      }
      return;
    }
    request->send(202, "text/plain", "QUEUED");
  }, nullptr, [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
    (void)appendRequestBodyChunk(request, data, len, index, total, 128U);
  });

  _impl->server.on("/api/settings", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (_app == nullptr) {
      request->send(503, "text/plain", "App not ready");
      return;
    }
    RuntimeSettings updated{};
    if (!_app->tryGetSettingsSnapshot(updated)) {
      request->send(503, "text/plain", "State busy");
      return;
    }
    const RuntimeSettings snapshot = updated;
    const size_t settingsBodyLimit = updated.webMaxSettingsBodyBytes;
    const size_t contentLength = request->contentLength();
    if (contentLength > settingsBodyLimit) {
      request->send(413, "text/plain", "Payload too large");
      return;
    }

    const String body = extractRequestBody(request);
    if (body.isEmpty()) {
      request->send(400, "text/plain", "Missing body");
      return;
    }
    if (body.length() > settingsBodyLimit) {
      request->send(413, "text/plain", "Payload too large");
      return;
    }

    StaticJsonDocument<HardwareSettings::WEB_SETTINGS_JSON_DOC_BYTES> doc;
    const DeserializationError err = deserializeJson(doc, body);
    if (err) {
      request->send(400, "text/plain", "Invalid JSON");
      return;
    }
    bool persist = true;
    if (doc.containsKey("persist")) {
      persist = doc["persist"].as<bool>();
    }

    if (doc.containsKey("sample_interval_ms")) {
      updated.sampleIntervalMs = doc["sample_interval_ms"].as<uint32_t>();
    } else if (doc.containsKey("sample_interval_sec")) {
      const uint32_t seconds = doc["sample_interval_sec"].as<uint32_t>();
      if (seconds > (UINT32_MAX / 1000U)) {
        updated.sampleIntervalMs = UINT32_MAX;
      } else {
        updated.sampleIntervalMs = seconds * 1000U;
      }
    }
    if (doc.containsKey("log_daily_enabled")) {
      updated.logDailyEnabled = doc["log_daily_enabled"].as<bool>();
    }
    if (doc.containsKey("log_all_enabled")) {
      updated.logAllEnabled = doc["log_all_enabled"].as<bool>();
    }
    // Reject logging enable when SD card is missing or FAT32.
    const bool loggingWasOff = !snapshot.logDailyEnabled && !snapshot.logAllEnabled;
    const bool loggingNowOn = updated.logDailyEnabled || updated.logAllEnabled;
    if (loggingWasOff && loggingNowOn) {
      SystemStatus sdCheck{};
      Sample dummy{};
      bool hasDummy = false;
      if (!_app->tryGetStatusSnapshot(sdCheck, dummy, hasDummy)) {
        request->send(503, "text/plain", "State busy");
        return;
      }
      if (!sdCheck.sdMounted) {
        request->send(409, "text/plain", "No SD card mounted");
        return;
      }
      if (!sdCheck.sdInfoValid) {
        request->send(409, "text/plain", "SD info not ready");
        return;
      }
      const uint8_t ft = sdCheck.sdFsType;
      if (ft >= 1 && ft <= 3) {
        request->send(409, "text/plain", "FAT32 not supported, format exFAT");
        return;
      }
    }
    if (doc.containsKey("log_all_max_bytes")) {
      updated.logAllMaxBytes = doc["log_all_max_bytes"].as<uint32_t>();
    }
    if (doc.containsKey("log_flush_ms")) {
      updated.logFlushMs = doc["log_flush_ms"].as<uint32_t>();
    }
    if (doc.containsKey("log_io_budget_ms")) {
      updated.logIoBudgetMs = doc["log_io_budget_ms"].as<uint32_t>();
    }
    if (doc.containsKey("log_mount_retry_ms")) {
      updated.logMountRetryMs = doc["log_mount_retry_ms"].as<uint32_t>();
    }
    if (doc.containsKey("log_write_retry_backoff_ms")) {
      updated.logWriteRetryBackoffMs = doc["log_write_retry_backoff_ms"].as<uint32_t>();
    }
    if (doc.containsKey("log_max_write_retries")) {
      updated.logMaxWriteRetries = doc["log_max_write_retries"].as<uint8_t>();
    }
    if (doc.containsKey("log_session_name")) {
      const char* sessionName = doc["log_session_name"].as<const char*>();
      strncpy(updated.logSessionName, sessionName ? sessionName : "", sizeof(updated.logSessionName) - 1);
      updated.logSessionName[sizeof(updated.logSessionName) - 1] = '\0';
    }
    if (doc.containsKey("log_events_max_bytes")) {
      updated.logEventsMaxBytes = doc["log_events_max_bytes"].as<uint32_t>();
    }
    if (doc.containsKey("lidar_service_ms")) {
      updated.lidarServiceMs = doc["lidar_service_ms"].as<uint32_t>();
    }
    if (doc.containsKey("lidar_min_strength")) {
      updated.lidarMinStrength = doc["lidar_min_strength"].as<uint16_t>();
    }
    if (doc.containsKey("lidar_max_distance_cm")) {
      updated.lidarMaxDistanceCm = doc["lidar_max_distance_cm"].as<uint16_t>();
    }
    if (doc.containsKey("lidar_frame_stale_ms")) {
      updated.lidarFrameStaleMs = doc["lidar_frame_stale_ms"].as<uint32_t>();
    }
    if (doc.containsKey("serial_print_interval_ms")) {
      updated.serialPrintIntervalMs = doc["serial_print_interval_ms"].as<uint32_t>();
    }
    if (doc.containsKey("cli_verbosity")) {
      updated.cliVerbosity = doc["cli_verbosity"].as<uint8_t>();
    }
    if (doc.containsKey("i2c_freq_hz")) {
      updated.i2cFreqHz = doc["i2c_freq_hz"].as<uint32_t>();
    }
    if (doc.containsKey("i2c_op_timeout_ms")) {
      updated.i2cOpTimeoutMs = doc["i2c_op_timeout_ms"].as<uint32_t>();
    }
    if (doc.containsKey("i2c_stuck_debounce_ms")) {
      updated.i2cStuckDebounceMs = doc["i2c_stuck_debounce_ms"].as<uint8_t>();
    }
    if (doc.containsKey("i2c_max_consecutive_failures")) {
      updated.i2cMaxConsecutiveFailures = doc["i2c_max_consecutive_failures"].as<uint8_t>();
    }
    if (doc.containsKey("i2c_recovery_backoff_ms")) {
      updated.i2cRecoveryBackoffMs = doc["i2c_recovery_backoff_ms"].as<uint32_t>();
    }
    if (doc.containsKey("i2c_recovery_backoff_max_ms")) {
      updated.i2cRecoveryBackoffMaxMs = doc["i2c_recovery_backoff_max_ms"].as<uint32_t>();
    }
    if (doc.containsKey("i2c_requests_per_tick")) {
      updated.i2cRequestsPerTick = doc["i2c_requests_per_tick"].as<uint8_t>();
    }
    if (doc.containsKey("i2c_slow_op_threshold_us")) {
      updated.i2cSlowOpThresholdUs = doc["i2c_slow_op_threshold_us"].as<uint32_t>();
    }
    if (doc.containsKey("i2c_slow_op_degrade_count")) {
      updated.i2cSlowOpDegradeCount = doc["i2c_slow_op_degrade_count"].as<uint8_t>();
    }
    if (doc.containsKey("i2c_task_heartbeat_timeout_ms")) {
      updated.i2cTaskHeartbeatTimeoutMs = doc["i2c_task_heartbeat_timeout_ms"].as<uint32_t>();
    }
    if (doc.containsKey("i2c_env_poll_ms")) {
      updated.i2cEnvPollMs = doc["i2c_env_poll_ms"].as<uint32_t>();
    }
    if (doc.containsKey("i2c_rtc_poll_ms")) {
      updated.i2cRtcPollMs = doc["i2c_rtc_poll_ms"].as<uint32_t>();
    }
    if (doc.containsKey("i2c_display_poll_ms")) {
      updated.i2cDisplayPollMs = doc["i2c_display_poll_ms"].as<uint32_t>();
    }
    if (doc.containsKey("i2c_env_bme_mode")) {
      updated.i2cEnvBmeMode = doc["i2c_env_bme_mode"].as<uint8_t>();
    }
    if (doc.containsKey("i2c_env_bme_osrs_t")) {
      updated.i2cEnvBmeOsrsT = doc["i2c_env_bme_osrs_t"].as<uint8_t>();
    }
    if (doc.containsKey("i2c_env_bme_osrs_p")) {
      updated.i2cEnvBmeOsrsP = doc["i2c_env_bme_osrs_p"].as<uint8_t>();
    }
    if (doc.containsKey("i2c_env_bme_osrs_h")) {
      updated.i2cEnvBmeOsrsH = doc["i2c_env_bme_osrs_h"].as<uint8_t>();
    }
    if (doc.containsKey("i2c_env_bme_filter")) {
      updated.i2cEnvBmeFilter = doc["i2c_env_bme_filter"].as<uint8_t>();
    }
    if (doc.containsKey("i2c_env_bme_standby")) {
      updated.i2cEnvBmeStandby = doc["i2c_env_bme_standby"].as<uint8_t>();
    }
    if (doc.containsKey("i2c_env_sht_mode")) {
      updated.i2cEnvShtMode = doc["i2c_env_sht_mode"].as<uint8_t>();
    }
    if (doc.containsKey("i2c_env_sht_repeatability")) {
      updated.i2cEnvShtRepeatability = doc["i2c_env_sht_repeatability"].as<uint8_t>();
    }
    if (doc.containsKey("i2c_env_sht_periodic_rate")) {
      updated.i2cEnvShtPeriodicRate = doc["i2c_env_sht_periodic_rate"].as<uint8_t>();
    }
    if (doc.containsKey("i2c_env_sht_clock_stretching")) {
      updated.i2cEnvShtClockStretching = doc["i2c_env_sht_clock_stretching"].as<uint8_t>();
    }
    if (doc.containsKey("i2c_env_sht_low_vdd")) {
      updated.i2cEnvShtLowVdd = doc["i2c_env_sht_low_vdd"].as<bool>();
    }
    if (doc.containsKey("i2c_env_sht_command_delay_ms")) {
      updated.i2cEnvShtCommandDelayMs = doc["i2c_env_sht_command_delay_ms"].as<uint16_t>();
    }
    if (doc.containsKey("i2c_env_sht_not_ready_timeout_ms")) {
      updated.i2cEnvShtNotReadyTimeoutMs = doc["i2c_env_sht_not_ready_timeout_ms"].as<uint32_t>();
    }
    if (doc.containsKey("i2c_env_sht_periodic_fetch_margin_ms")) {
      updated.i2cEnvShtPeriodicFetchMarginMs =
          doc["i2c_env_sht_periodic_fetch_margin_ms"].as<uint32_t>();
    }
    if (doc.containsKey("i2c_env_sht_allow_general_call_reset")) {
      updated.i2cEnvShtAllowGeneralCallReset =
          doc["i2c_env_sht_allow_general_call_reset"].as<bool>();
    }
    if (doc.containsKey("i2c_env_sht_recover_use_bus_reset")) {
      updated.i2cEnvShtRecoverUseBusReset =
          doc["i2c_env_sht_recover_use_bus_reset"].as<bool>();
    }
    if (doc.containsKey("i2c_env_sht_recover_use_soft_reset")) {
      updated.i2cEnvShtRecoverUseSoftReset =
          doc["i2c_env_sht_recover_use_soft_reset"].as<bool>();
    }
    if (doc.containsKey("i2c_env_sht_recover_use_hard_reset")) {
      updated.i2cEnvShtRecoverUseHardReset =
          doc["i2c_env_sht_recover_use_hard_reset"].as<bool>();
    }
    if (doc.containsKey("i2c_recover_timeout_ms")) {
      updated.i2cRecoverTimeoutMs = doc["i2c_recover_timeout_ms"].as<uint32_t>();
    }
    if (doc.containsKey("i2c_max_results_per_tick")) {
      updated.i2cMaxResultsPerTick = doc["i2c_max_results_per_tick"].as<uint8_t>();
    }
    if (doc.containsKey("i2c_task_wait_ms")) {
      updated.i2cTaskWaitMs = doc["i2c_task_wait_ms"].as<uint32_t>();
    }
    if (doc.containsKey("i2c_health_stale_task_multiplier")) {
      updated.i2cHealthStaleTaskMultiplier = doc["i2c_health_stale_task_multiplier"].as<uint8_t>();
    }
    if (doc.containsKey("i2c_slow_window_ms")) {
      updated.i2cSlowWindowMs = doc["i2c_slow_window_ms"].as<uint32_t>();
    }
    if (doc.containsKey("i2c_health_recent_window_ms")) {
      updated.i2cHealthRecentWindowMs = doc["i2c_health_recent_window_ms"].as<uint32_t>();
    }
    if (doc.containsKey("i2c_env_address")) {
      updated.i2cEnvAddress = doc["i2c_env_address"].as<uint8_t>();
    }
    if (doc.containsKey("i2c_rtc_backup_mode")) {
      updated.i2cRtcBackupMode = doc["i2c_rtc_backup_mode"].as<uint8_t>();
    }
    if (doc.containsKey("i2c_rtc_enable_eeprom_writes")) {
      updated.i2cRtcEnableEepromWrites = doc["i2c_rtc_enable_eeprom_writes"].as<bool>();
    }
    if (doc.containsKey("i2c_rtc_eeprom_timeout_ms")) {
      updated.i2cRtcEepromTimeoutMs = doc["i2c_rtc_eeprom_timeout_ms"].as<uint32_t>();
    }
    if (doc.containsKey("i2c_rtc_offline_threshold")) {
      updated.i2cRtcOfflineThreshold = doc["i2c_rtc_offline_threshold"].as<uint8_t>();
    }
    if (doc.containsKey("i2c_display_address")) {
      updated.i2cDisplayAddress = doc["i2c_display_address"].as<uint8_t>();
    }
    if (doc.containsKey("e2_address")) {
      updated.e2Address = doc["e2_address"].as<uint8_t>();
    }
    if (doc.containsKey("e2_bit_timeout_us")) {
      updated.e2BitTimeoutUs = doc["e2_bit_timeout_us"].as<uint32_t>();
    }
    if (doc.containsKey("e2_byte_timeout_us")) {
      updated.e2ByteTimeoutUs = doc["e2_byte_timeout_us"].as<uint32_t>();
    }
    if (doc.containsKey("e2_clock_low_us")) {
      updated.e2ClockLowUs = doc["e2_clock_low_us"].as<uint16_t>();
    }
    if (doc.containsKey("e2_clock_high_us")) {
      updated.e2ClockHighUs = doc["e2_clock_high_us"].as<uint16_t>();
    }
    if (doc.containsKey("e2_start_hold_us")) {
      updated.e2StartHoldUs = doc["e2_start_hold_us"].as<uint16_t>();
    }
    if (doc.containsKey("e2_stop_hold_us")) {
      updated.e2StopHoldUs = doc["e2_stop_hold_us"].as<uint16_t>();
    }
    if (doc.containsKey("e2_write_delay_ms")) {
      updated.e2WriteDelayMs = doc["e2_write_delay_ms"].as<uint32_t>();
    }
    if (doc.containsKey("e2_interval_write_delay_ms")) {
      updated.e2IntervalWriteDelayMs = doc["e2_interval_write_delay_ms"].as<uint32_t>();
    }
    if (doc.containsKey("e2_offline_threshold")) {
      updated.e2OfflineThreshold = doc["e2_offline_threshold"].as<uint8_t>();
    }
    if (doc.containsKey("e2_recovery_backoff_ms")) {
      updated.e2RecoveryBackoffMs = doc["e2_recovery_backoff_ms"].as<uint32_t>();
    }
    if (doc.containsKey("e2_recovery_backoff_max_ms")) {
      updated.e2RecoveryBackoffMaxMs = doc["e2_recovery_backoff_max_ms"].as<uint32_t>();
    }
    if (doc.containsKey("e2_config_interval_ds")) {
      updated.e2ConfigIntervalDs = doc["e2_config_interval_ds"].as<uint16_t>();
    }
    if (doc.containsKey("e2_config_co2_interval_factor")) {
      updated.e2ConfigCo2IntervalFactor =
          static_cast<int8_t>(doc["e2_config_co2_interval_factor"].as<int>());
    }
    if (doc.containsKey("e2_config_filter")) {
      updated.e2ConfigFilter = doc["e2_config_filter"].as<uint8_t>();
    }
    if (doc.containsKey("e2_config_operating_mode")) {
      updated.e2ConfigOperatingMode = doc["e2_config_operating_mode"].as<uint8_t>();
    }
    if (doc.containsKey("e2_config_offset_ppm")) {
      updated.e2ConfigOffsetPpm =
          static_cast<int16_t>(doc["e2_config_offset_ppm"].as<int>());
    }
    if (doc.containsKey("e2_config_gain")) {
      updated.e2ConfigGain = doc["e2_config_gain"].as<uint16_t>();
    }
    if (doc.containsKey("wifi_enabled")) {
      updated.wifiEnabled = doc["wifi_enabled"].as<bool>();
    }
    if (doc.containsKey("ap_ssid")) {
      const char* ssid = doc["ap_ssid"].as<const char*>();
      strncpy(updated.apSsid, ssid ? ssid : "", sizeof(updated.apSsid) - 1);
      updated.apSsid[sizeof(updated.apSsid) - 1] = '\0';
    }
    const bool apPassUpdate = doc["ap_pass_update"].as<bool>();
    if (apPassUpdate && !doc.containsKey("ap_pass")) {
      request->send(400, "text/plain", "Missing ap_pass for update");
      return;
    }
    if (apPassUpdate) {
      const char* pass = doc["ap_pass"].as<const char*>();
      strncpy(updated.apPass, pass ? pass : "", sizeof(updated.apPass) - 1);
      updated.apPass[sizeof(updated.apPass) - 1] = '\0';
    }
    if (doc.containsKey("co2_on_ppm")) {
      updated.co2OnPpm = doc["co2_on_ppm"].as<float>();
    }
    if (doc.containsKey("co2_off_ppm")) {
      updated.co2OffPpm = doc["co2_off_ppm"].as<float>();
    }
    if (doc.containsKey("temp_on_c")) {
      updated.tempOnC = doc["temp_on_c"].as<float>();
    }
    if (doc.containsKey("temp_off_c")) {
      updated.tempOffC = doc["temp_off_c"].as<float>();
    }
    if (doc.containsKey("rh_on_pct")) {
      updated.rhOnPct = doc["rh_on_pct"].as<float>();
    }
    if (doc.containsKey("rh_off_pct")) {
      updated.rhOffPct = doc["rh_off_pct"].as<float>();
    }
    if (doc.containsKey("output_source")) {
      updated.outputSource = doc["output_source"].as<uint8_t>();
    }
    const bool updatesOutputValveChannel = doc.containsKey("output_valve_channel");
    const bool updatesOutputFanChannel = doc.containsKey("output_fan_channel");
    if (updatesOutputValveChannel) {
      updated.outputValveChannel = doc["output_valve_channel"].as<uint8_t>();
    }
    if (doc.containsKey("output_valve_powered_closes")) {
      updated.outputValvePoweredClosed = doc["output_valve_powered_closes"].as<bool>();
    }
    if (updatesOutputFanChannel) {
      updated.outputFanChannel = doc["output_fan_channel"].as<uint8_t>();
    }
    if (doc.containsKey("output_fan_pwm_percent")) {
      updated.outputFanPwmPercent = doc["output_fan_pwm_percent"].as<uint8_t>();
    }
    if (doc.containsKey("output_fan_period_ms")) {
      updated.outputFanPeriodMs = doc["output_fan_period_ms"].as<uint32_t>();
    }
    if (doc.containsKey("output_fan_on_ms")) {
      updated.outputFanOnMs = doc["output_fan_on_ms"].as<uint32_t>();
    }
    if (doc.containsKey("min_on_ms")) {
      updated.minOnMs = doc["min_on_ms"].as<uint32_t>();
    }
    if (doc.containsKey("min_off_ms")) {
      updated.minOffMs = doc["min_off_ms"].as<uint32_t>();
    }
    if (doc.containsKey("command_drain_per_tick")) {
      updated.commandDrainPerTick = doc["command_drain_per_tick"].as<uint8_t>();
    }
    if (doc.containsKey("command_queue_degraded_window_ms")) {
      updated.commandQueueDegradedWindowMs = doc["command_queue_degraded_window_ms"].as<uint32_t>();
    }
    if (doc.containsKey("command_queue_degraded_depth_threshold")) {
      updated.commandQueueDegradedDepthThreshold =
          doc["command_queue_degraded_depth_threshold"].as<uint8_t>();
    }
    if (doc.containsKey("output_data_stale_min_ms")) {
      updated.outputDataStaleMinMs = doc["output_data_stale_min_ms"].as<uint32_t>();
    }
    if (doc.containsKey("main_tick_slow_threshold_us")) {
      updated.mainTickSlowThresholdUs = doc["main_tick_slow_threshold_us"].as<uint32_t>();
    }
    if (doc.containsKey("web_overrun_threshold_us")) {
      updated.webOverrunThresholdUs = doc["web_overrun_threshold_us"].as<uint32_t>();
    }
    if (doc.containsKey("web_overrun_burst_threshold")) {
      updated.webOverrunBurstThreshold = doc["web_overrun_burst_threshold"].as<uint8_t>();
    }
    if (doc.containsKey("web_overrun_throttle_ms")) {
      updated.webOverrunThrottleMs = doc["web_overrun_throttle_ms"].as<uint32_t>();
    }
    if (doc.containsKey("led_health_init_ms")) {
      updated.ledHealthInitMs = doc["led_health_init_ms"].as<uint32_t>();
    }
    if (doc.containsKey("led_health_debounce_ms")) {
      updated.ledHealthDebounceMs = doc["led_health_debounce_ms"].as<uint32_t>();
    }
    if (doc.containsKey("ap_start_retry_backoff_ms")) {
      updated.apStartRetryBackoffMs = doc["ap_start_retry_backoff_ms"].as<uint32_t>();
    }
    if (doc.containsKey("web_max_settings_body_bytes")) {
      updated.webMaxSettingsBodyBytes = doc["web_max_settings_body_bytes"].as<uint16_t>();
    }
    if (doc.containsKey("web_max_rtc_body_bytes")) {
      updated.webMaxRtcBodyBytes = doc["web_max_rtc_body_bytes"].as<uint16_t>();
    }
    if (doc.containsKey("outputs_enabled")) {
      updated.outputsEnabled = doc["outputs_enabled"].as<bool>();
    }
    if (doc.containsKey("ap_auto_off_ms")) {
      updated.apAutoOffMs = doc["ap_auto_off_ms"].as<uint32_t>();
    }

    const auto validOutputChannel = [](uint8_t channel) -> bool {
      return channel == RuntimeSettings::OUTPUT_CHANNEL_DISABLED ||
             channel <= RuntimeSettings::MAX_OUTPUT_CHANNEL_INDEX;
    };
    if (!validOutputChannel(updated.outputValveChannel)) {
      request->send(400, "text/plain", "outputValveChannel out of range");
      return;
    }
    if (!validOutputChannel(updated.outputFanChannel)) {
      request->send(400, "text/plain", "outputFanChannel out of range");
      return;
    }
    bool autoDisabledLegacyFanChannelConflict = false;
    if (updated.outputValveChannel != RuntimeSettings::OUTPUT_CHANNEL_DISABLED &&
        updated.outputFanChannel != RuntimeSettings::OUTPUT_CHANNEL_DISABLED &&
        updated.outputValveChannel == updated.outputFanChannel) {
      // Backward compatibility: older persisted settings could legally hold
      // identical channels. Keep strict validation for explicit channel edits,
      // but auto-heal legacy conflicts for unrelated updates by disabling
      // the fan channel. The auto-heal is reported in the change hint
      // ("output_fan_channel") so the event log records the side-effect.
      if (updatesOutputValveChannel || updatesOutputFanChannel) {
        request->send(400, "text/plain", "output valve/fan channels must differ");
        return;
      }
      updated.outputFanChannel = RuntimeSettings::OUTPUT_CHANNEL_DISABLED;
      autoDisabledLegacyFanChannelConflict = true;
    }

    String changeHint;
    bool hasAnyHintKey = false;
    bool apPassHintAdded = false;
    auto appendHintKey = [&](const char* key) {
      if (key == nullptr || key[0] == '\0') {
        return;
      }
      if (!hasAnyHintKey) {
        changeHint = key;
        hasAnyHintKey = true;
        return;
      }
      constexpr size_t kMaxHintChars = 58U;
      if ((changeHint.length() + 2U + strlen(key)) > kMaxHintChars) {
        if (!changeHint.endsWith("...")) {
          changeHint += "...";
        }
        return;
      }
      changeHint += ", ";
      changeHint += key;
    };

    for (JsonPair kv : doc.as<JsonObject>()) {
      const char* key = kv.key().c_str();
      if (key == nullptr || key[0] == '\0') {
        continue;
      }
      if (strcmp(key, "ap_pass") == 0) {
        if (apPassUpdate && !apPassHintAdded) {
          appendHintKey("ap_pass");
          apPassHintAdded = true;
        }
        continue;
      }
      if (strcmp(key, "ap_pass_update") == 0) {
        if (apPassUpdate && !apPassHintAdded) {
          appendHintKey("ap_pass");
          apPassHintAdded = true;
        }
        continue;
      }
      if (strcmp(key, "persist") == 0) {
        continue;
      }
      appendHintKey(key);
    }
    if (autoDisabledLegacyFanChannelConflict) {
      appendHintKey("output_fan_channel");
    }

    const Status st = _app->enqueueApplySettings(updated,
                                                 persist,
                                                 hasAnyHintKey ? changeHint.c_str() : nullptr);
    if (!st.ok()) {
      if (st.code == Err::RESOURCE_BUSY) {
        request->send(503, "text/plain", st.msg);
      } else {
        request->send(400, "text/plain", st.msg);
      }
      return;
    }
    request->send(202, "text/plain", "QUEUED");
  }, nullptr, [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
    (void)appendRequestBodyChunk(request,
                                 data,
                                 len,
                                 index,
                                 total,
                                 RuntimeSettings::MAX_WEB_MAX_SETTINGS_BODY_BYTES);
  });

  _impl->server.on("/api/graph", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (_app == nullptr) {
      request->send(503, "text/plain", "App not ready");
      return;
    }
    size_t count = _impl->graphScratchCapacity;
    if (request->hasParam("count")) {
      const long requested = request->getParam("count")->value().toInt();
      const size_t parsed = (requested <= 0) ? 0U : static_cast<size_t>(requested);
      count = clampGraphCount(parsed, _impl->graphScratchCapacity);
    }

    size_t got = 0;
    // Preserve the same lock ordering contract as OrderedWebReadGuard:
    // scratch lock first, then snapshot copy from app state.
    if (!_impl->tryLock()) {
      request->send(503, "text/plain", "State busy");
      return;
    }
    if (!_app->tryCopySamples(_impl->graphScratch, count, true, got)) {
      _impl->unlock();
      request->send(503, "text/plain", "State busy");
      return;
    }
    if (!_impl->beginJsonStream(WebServerImpl::JsonStreamKind::GRAPH, got)) {
      _impl->unlock();
      request->send(503, "text/plain", "State busy");
      return;
    }

    WebServerImpl* const implPtr = _impl;
    request->onDisconnect([this, implPtr]() {
      if (_impl == implPtr && _impl != nullptr) {
        _impl->releaseJsonStream();
      }
    });

    AsyncWebServerResponse* response = request->beginChunkedResponse(
        "application/json",
        [this, implPtr](uint8_t* buffer, size_t maxLen, size_t index) -> size_t {
          if (_impl != implPtr || implPtr == nullptr) {
            return 0U;
          }
          return implPtr->fillJsonStream(buffer, maxLen, index);
        });
    if (response == nullptr) {
      _impl->releaseJsonStream();
      request->send(503, "text/plain", "Response alloc failed");
      return;
    }
    addNoStoreHeaders(response);
    request->send(response);
  });

  _impl->server.on("/api/events", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (_app == nullptr) {
      request->send(503, "text/plain", "App not ready");
      return;
    }
    size_t count = _impl->eventScratchCapacity;
    if (request->hasParam("count")) {
      const long requested = request->getParam("count")->value().toInt();
      const size_t parsed = (requested <= 0) ? 0U : static_cast<size_t>(requested);
      count = clampEventCount(parsed, _impl->eventScratchCapacity);
    }

    size_t got = 0;
    if (!_impl->tryLock()) {
      request->send(503, "text/plain", "State busy");
      return;
    }
    if (!_app->tryCopyEvents(_impl->eventScratch, count, false, got)) {
      _impl->unlock();
      request->send(503, "text/plain", "State busy");
      return;
    }
    if (!_impl->beginJsonStream(WebServerImpl::JsonStreamKind::EVENTS, got)) {
      _impl->unlock();
      request->send(503, "text/plain", "State busy");
      return;
    }

    WebServerImpl* const implPtr = _impl;
    request->onDisconnect([this, implPtr]() {
      if (_impl == implPtr && _impl != nullptr) {
        _impl->releaseJsonStream();
      }
    });

    AsyncWebServerResponse* response = request->beginChunkedResponse(
        "application/json",
        [this, implPtr](uint8_t* buffer, size_t maxLen, size_t index) -> size_t {
          if (_impl != implPtr || implPtr == nullptr) {
            return 0U;
          }
          return implPtr->fillJsonStream(buffer, maxLen, index);
        });
    if (response == nullptr) {
      _impl->releaseJsonStream();
      request->send(503, "text/plain", "Response alloc failed");
      return;
    }
    addNoStoreHeaders(response);
    request->send(response);
  });

  _impl->server.on("/api/sd/remount", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (_app == nullptr) {
      request->send(503, "text/plain", "App not ready");
      return;
    }
    const Status st = _app->enqueueRemountSd();
    if (!st.ok()) {
      if (st.code == Err::RESOURCE_BUSY) {
        request->send(503, "text/plain", st.msg);
      } else {
        request->send(400, "text/plain", st.msg);
      }
      return;
    }
    request->send(202, "text/plain", "QUEUED");
  });

  _impl->server.on("/api/rtc/set", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (_app == nullptr) {
      request->send(503, "text/plain", "App not ready");
      return;
    }
    RuntimeSettings settings{};
    if (!_app->tryGetSettingsSnapshot(settings)) {
      request->send(503, "text/plain", "State busy");
      return;
    }
    const size_t rtcBodyLimit = settings.webMaxRtcBodyBytes;
    const size_t contentLength = request->contentLength();
    if (contentLength > rtcBodyLimit) {
      request->send(413, "text/plain", "Payload too large");
      return;
    }

    const String body = extractRequestBody(request);
    if (body.isEmpty()) {
      request->send(400, "text/plain", "Missing body");
      return;
    }
    if (body.length() > rtcBodyLimit) {
      request->send(413, "text/plain", "Payload too large");
      return;
    }

    StaticJsonDocument<HardwareSettings::WEB_RTC_JSON_DOC_BYTES> doc;
    const DeserializationError err = deserializeJson(doc, body);
    if (err) {
      request->send(400, "text/plain", "Invalid JSON");
      return;
    }

    RtcTime t;
    t.year = doc["year"].as<uint16_t>();
    t.month = doc["month"].as<uint8_t>();
    t.day = doc["day"].as<uint8_t>();
    t.hour = doc["hour"].as<uint8_t>();
    t.minute = doc["minute"].as<uint8_t>();
    t.second = doc["second"].as<uint8_t>();
    t.valid = true;

    const Status st = _app->enqueueSetRtcTime(t);
    if (!st.ok()) {
      if (st.code == Err::RESOURCE_BUSY) {
        request->send(503, "text/plain", st.msg);
      } else {
        request->send(400, "text/plain", st.msg);
      }
      return;
    }

    request->send(202, "text/plain", "QUEUED");
  }, nullptr, [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
    (void)appendRequestBodyChunk(request,
                                 data,
                                 len,
                                 index,
                                 total,
                                 RuntimeSettings::MAX_WEB_MAX_RTC_BODY_BYTES);
  });

  _impl->server.onNotFound([](AsyncWebServerRequest* request) {
    request->send(404, "text/plain", "Not found");
  });

  return Ok();
}

Status WebServer::begin(TFLunaControl* app) {
  if (app == nullptr) {
    return Status(Err::INVALID_CONFIG, 0, "app null");
  }

  // Re-init-safe lifecycle: tear down previous server instance first.
  end();
  _app = app;
  _apRunning = false;
  _lastBroadcastMs = 0;
  _mdnsRunning = false;
  _mdnsNextRetryMs = 0;
  _serverStarted = false;
  _apRestartGuardUntilMs = 0;
  _lastWsCleanupMs = 0;
  _lastUiActivityMs = 0;

  _impl = new (std::nothrow) WebServerImpl(_port);
  if (_impl == nullptr) {
    _app = nullptr;
    return Status(Err::OUT_OF_MEMORY, 0, "web impl alloc failed");
  }
  if (!_impl->allocateScratch(_psramAvailable)) {
    delete _impl;
    _impl = nullptr;
    _app = nullptr;
    return Status(Err::OUT_OF_MEMORY, 0, "web scratch alloc failed");
  }
  if (!_impl->scratchReady()) {
    delete _impl;
    _impl = nullptr;
    _app = nullptr;
    return Status(Err::OUT_OF_MEMORY, 0, "web scratch not ready");
  }
  _uiEventFetchCount = static_cast<uint16_t>(
      clampEventCount(static_cast<size_t>(_uiEventFetchCount), _impl->eventScratchCapacity));

  const Status handlerStatus = setupHandlers();
  if (!handlerStatus.ok()) {
    delete _impl;
    _impl = nullptr;
    _app = nullptr;
    return handlerStatus;
  }

  return Ok();
}

void WebServer::end() {
  stopAp();
  if (_impl != nullptr) {
    _impl->releaseJsonStream();
    if (_serverStarted) {
      _impl->server.end();
      _serverStarted = false;
    }
    _impl->ws.closeAll();
    _impl->ws.cleanupClients(static_cast<uint16_t>(WebServerImpl::MAX_TRACKED_WS_CLIENTS));
    _impl->clearWsClients();
    delete _impl;
    _impl = nullptr;
  }
  _app = nullptr;
  _apRunning = false;
  _lastBroadcastMs = 0;
  _mdnsRunning = false;
  _mdnsNextRetryMs = 0;
  _serverStarted = false;
  _apRestartGuardUntilMs = 0;
  _lastWsCleanupMs = 0;
  _lastUiActivityMs = 0;
}

Status WebServer::startAp(const RuntimeSettings& settings) {
  if (_impl == nullptr || _app == nullptr) {
    return Status(Err::NOT_INITIALIZED, 0, "web not initialized");
  }
  if (_apRunning) {
    return Ok();
  }

  const uint32_t nowMs = millis();
  if (_apRestartGuardUntilMs != 0U &&
      static_cast<int32_t>(nowMs - _apRestartGuardUntilMs) < 0) {
    return Status(Err::RESOURCE_BUSY, 0, "WiFi stop in progress");
  }

  const wifi_mode_t mode = WiFi.getMode();
  const bool apModeReady = (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA);
  if (!apModeReady) {
    if (!WiFi.mode(WIFI_AP)) {
      _apRestartGuardUntilMs = nowMs + kApRestartGuardMs;
      return Status(Err::COMM_FAILURE, 0, "WiFi AP mode failed");
    }
  }
  (void)WiFi.softAPsetHostname(kApHostname);

  const char* pass = settings.apPass;
  if (pass[0] == '\0') {
    if (!WiFi.softAP(settings.apSsid)) {
      // Avoid full WiFi-off teardown from loopTask context; keep radio stack up.
      WiFi.softAPdisconnect(false);
      _apRestartGuardUntilMs = millis() + kApRestartGuardMs;
      return Status(Err::COMM_FAILURE, 0, "SoftAP start failed");
    }
  } else {
    if (!WiFi.softAP(settings.apSsid, pass)) {
      // Avoid full WiFi-off teardown from loopTask context; keep radio stack up.
      WiFi.softAPdisconnect(false);
      _apRestartGuardUntilMs = millis() + kApRestartGuardMs;
      return Status(Err::COMM_FAILURE, 0, "SoftAP start failed");
    }
  }
  // Reduce the AP's station inactivity timeout from the default 300 s to
  // 120 s so that softAPgetStationNum() drops to 0 within a reasonable
  // window after a phone disconnects without sending a clean deauth.
  // 120 s is well above typical WiFi power-save null-frame intervals
  // (30-60 s) so sleeping/locked phones stay associated.
  // Note: esp_wifi_set_inactive_time() may be removed in future IDF releases.
#if TFLUNACTRL_HAS_ESP_WIFI_API && __has_include(<esp_wifi.h>)
  {
    #if !defined(ESP_IDF_VERSION_MAJOR) || ESP_IDF_VERSION_MAJOR < 6
    (void)esp_wifi_set_inactive_time(WIFI_IF_AP, 120);
    #endif
  }
#endif
#if TFLUNACTRL_HAS_MDNS
  _mdnsRunning = false;
  _mdnsNextRetryMs = 0;
  if (MDNS.begin(kApHostname)) {
    MDNS.addService("http", "tcp", _port);
    _mdnsRunning = true;
  }
#else
  _mdnsRunning = false;
  _mdnsNextRetryMs = 0;
#endif
  if (!_serverStarted) {
    _impl->server.begin();
    _serverStarted = true;
  }
  _impl->clearWsClients();
  _lastWsCleanupMs = nowMs;
  _apRestartGuardUntilMs = 0;
  _lastUiActivityMs = 0;
  _apRunning = true;
  return Ok();
}

void WebServer::stopAp() {
  if (!_apRunning) {
    return;
  }
  if (_impl != nullptr) {
    _impl->ws.closeAll();
    _impl->clearWsClients();
  }
#if TFLUNACTRL_HAS_MDNS
  MDNS.end();
#endif
  _mdnsRunning = false;
  _mdnsNextRetryMs = 0;
  // Keep WiFi in AP-capable mode and only stop SoftAP itself.
  // This avoids frequent mode teardown/recreate races on ESP32-S3.
  WiFi.softAPdisconnect(false);
  _apRunning = false;
  _apRestartGuardUntilMs = millis() + kApRestartGuardMs;
  _lastWsCleanupMs = 0;
  _lastUiActivityMs = 0;
}

void WebServer::tick(uint32_t nowMs) {
  if (!_apRunning || _impl == nullptr || _app == nullptr) {
    return;
  }
  // All blocking cross-task IPC operations (WiFi stats, mDNS, WS
  // broadcast) moved to broadcastDeferred() so they don't inflate
  // cooperative tick timing.
  (void)nowMs;
}

void WebServer::broadcastDeferred(uint32_t nowMs) {
  if (!_apRunning || _impl == nullptr || _app == nullptr) {
    return;
  }

  // WiFi stat cache refresh â€” calls esp_wifi_ap_get_sta_list(),
  // esp_wifi_get_config(), WiFi.softAPgetStationNum() which all use
  // cross-task IPC and can block 50-200 ms when the lwIP task is busy.
  // Runs outside tick timing at 1 Hz.
  refreshWifiStatsIfDue(nowMs);

#if TFLUNACTRL_HAS_MDNS
  // MDNS.begin() performs multicast socket setup via tcpip_api_call()
  // and can stall for hundreds of milliseconds to seconds.  Retries
  // every 2 s until successful â€” must not run inside cooperative tick.
  if (!_mdnsRunning &&
      (_mdnsNextRetryMs == 0 || static_cast<int32_t>(nowMs - _mdnsNextRetryMs) >= 0)) {
    if (MDNS.begin(kApHostname)) {
      MDNS.addService("http", "tcp", _port);
      _mdnsRunning = true;
      _mdnsNextRetryMs = 0;
    } else {
      _mdnsNextRetryMs = nowMs + 2000U;
    }
  }
#endif

  // WS client cleanup â€” may trigger TCP FIN via tcpip_api_call()
  // for dead clients, so it runs here outside tick timing.
  if (_impl->ws.count() > 0U &&
      (_lastWsCleanupMs == 0U ||
       static_cast<int32_t>(nowMs - _lastWsCleanupMs) >= static_cast<int32_t>(kWsCleanupIntervalMs))) {
    _impl->ws.cleanupClients(static_cast<uint16_t>(WebServerImpl::MAX_TRACKED_WS_CLIENTS));
    _impl->pruneWsClients();
    _lastWsCleanupMs = nowMs;
  }

  if (static_cast<int32_t>(nowMs - _lastBroadcastMs) < 0) {
    return;
  }
  _lastBroadcastMs = nowMs + _broadcastIntervalMs;

  // Skip this frame when there are no clients.
  if (_impl->ws.count() == 0U) {
    return;
  }
  _impl->pruneWsClients();

  SystemStatus status{};
  Sample latest{};
  bool hasLatest = false;
  if (!_app->tryGetStatusSnapshot(status, latest, hasLatest)) {
    return;
  }

  char buf[HardwareSettings::WEB_LIVE_STATUS_WS_BUFFER_BYTES];
  const size_t n = serializeLiveStatusJsonBounded(status, hasLatest ? &latest : nullptr, buf, sizeof(buf));
  const char* payload = buf;
  size_t payloadLen = n;
  if (payloadLen == 0U) {
    static const char kFallback[] =
        "{\"type\":\"live\",\"health\":\"DEGRADED\",\"error\":\"live payload too large\"}";
    payload = kFallback;
    payloadLen = sizeof(kFallback) - 1U;
  }
  const AsyncWebSocketSharedBuffer sharedPayload = makeWsSharedBuffer(payload, payloadLen);
  if (!sharedPayload) {
    return;
  }

  for (size_t i = 0; i < WebServerImpl::MAX_TRACKED_WS_CLIENTS; ++i) {
    WebServerImpl::WsClientState& slot = _impl->wsClients[i];
    if (slot.id == 0U) {
      continue;
    }
    if (!_impl->ws.hasClient(slot.id)) {
      slot = WebServerImpl::WsClientState{};
      continue;
    }
    if (!_impl->ws.availableForWrite(slot.id)) {
      if (slot.stalledFrames < UINT8_MAX) {
        ++slot.stalledFrames;
      }
      if (slot.stalledFrames >= WebServerImpl::MAX_STALLED_WS_FRAMES) {
        _impl->ws.close(slot.id, 1013, "slow client");
        slot = WebServerImpl::WsClientState{};
      }
      continue;
    }
    if (_impl->ws.text(slot.id, sharedPayload)) {
      slot.stalledFrames = 0U;
    } else if (slot.stalledFrames < UINT8_MAX) {
      ++slot.stalledFrames;
      if (slot.stalledFrames >= WebServerImpl::MAX_STALLED_WS_FRAMES) {
        _impl->ws.close(slot.id, 1013, "stalled client");
        slot = WebServerImpl::WsClientState{};
      }
    }
  }
}

// stationCount(), averageStationRssiDbm(), apChannel() are now inline
// in the header, returning cached values refreshed by refreshWifiStatsIfDue().

void WebServer::refreshWifiStatsIfDue(uint32_t nowMs) {
  static constexpr uint32_t kWifiStatsRefreshMs = 1000U;
  if (_lastWifiStatsMs != 0U &&
      static_cast<int32_t>(nowMs - _lastWifiStatsMs) < static_cast<int32_t>(kWifiStatsRefreshMs)) {
    return;
  }
  _lastWifiStatsMs = nowMs;
  refreshWifiStatsNow();
}

void WebServer::refreshWifiStatsNow() {
  if (!_apRunning) {
    _cachedStationCount = 0;
    _cachedRssiDbm = -127;
    _cachedChannel = 0;
    return;
  }
  _cachedStationCount = WiFi.softAPgetStationNum();
#if TFLUNACTRL_HAS_ESP_WIFI_API
  {
    wifi_sta_list_t staList{};
    if (esp_wifi_ap_get_sta_list(&staList) == ESP_OK && staList.num > 0) {
      int32_t sum = 0;
      for (int i = 0; i < staList.num; ++i) {
        sum += static_cast<int32_t>(staList.sta[i].rssi);
      }
      _cachedRssiDbm = static_cast<int16_t>(sum / staList.num);
    } else {
      _cachedRssiDbm = -127;
    }
  }
  {
    wifi_config_t cfg{};
    if (esp_wifi_get_config(WIFI_IF_AP, &cfg) == ESP_OK) {
      _cachedChannel = static_cast<uint8_t>(cfg.ap.channel);
    } else {
      _cachedChannel = 0;
    }
  }
#else
  _cachedRssiDbm = -127;
  _cachedChannel = 0;
#endif
}

size_t WebServer::webClientCount() const {
  if (_impl == nullptr) {
    return 0;
  }
  return _impl->ws.count();
}

size_t WebServer::graphScratchCapacity() const {
  return (_impl != nullptr) ? _impl->graphScratchCapacity : 0U;
}

size_t WebServer::eventScratchCapacity() const {
  return (_impl != nullptr) ? _impl->eventScratchCapacity : 0U;
}

bool WebServer::webScratchUsingPsram() const {
  return (_impl != nullptr) ? _impl->scratchUsingPsram : false;
}

void WebServer::noteUiActivity() {
  _lastUiActivityMs = millis();
}

bool WebServer::hasRecentUiActivity(uint32_t nowMs, uint32_t windowMs) const {
  if (!_apRunning) {
    return false;
  }
  const uint32_t last = _lastUiActivityMs;
  if (last == 0U) {
    return false;
  }
  const uint32_t window = (windowMs == 0U) ? 10000U : windowMs;
  return static_cast<int32_t>(nowMs - last) <= static_cast<int32_t>(window);
}

}  // namespace TFLunaControl

#else

namespace TFLunaControl {

Status WebServer::setupHandlers() {
  return Ok();
}

Status WebServer::begin(TFLunaControl* app) {
  _app = app;
  _mdnsRunning = false;
  _mdnsNextRetryMs = 0;
  _lastUiActivityMs = 0;
  return Ok();
}

void WebServer::end() {
  _app = nullptr;
  _apRunning = false;
  _lastBroadcastMs = 0;
  _mdnsRunning = false;
  _mdnsNextRetryMs = 0;
  _lastUiActivityMs = 0;
}

Status WebServer::startAp(const RuntimeSettings& settings) {
  (void)settings;
  _apRunning = false;
  return Status(Err::NOT_INITIALIZED, 0, "Web disabled");
}

void WebServer::stopAp() {
  _apRunning = false;
}

void WebServer::tick(uint32_t nowMs) {
  (void)nowMs;
}

void WebServer::broadcastDeferred(uint32_t nowMs) {
  (void)nowMs;
}

void WebServer::refreshWifiStatsIfDue(uint32_t nowMs) {
  (void)nowMs;
}

void WebServer::refreshWifiStatsNow() {
}

size_t WebServer::webClientCount() const {
  return 0;
}

size_t WebServer::graphScratchCapacity() const {
  return 0U;
}

size_t WebServer::eventScratchCapacity() const {
  return 0U;
}

bool WebServer::webScratchUsingPsram() const {
  return false;
}

void WebServer::noteUiActivity() {
}

bool WebServer::hasRecentUiActivity(uint32_t nowMs, uint32_t windowMs) const {
  (void)nowMs;
  (void)windowMs;
  return false;
}

}  // namespace TFLunaControl

#endif

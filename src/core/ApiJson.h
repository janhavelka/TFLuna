#pragma once

#include <stddef.h>

#include <ArduinoJson.h>

#include "TFLunaControl/Health.h"
#include "TFLunaControl/RuntimeSettings.h"
#include "TFLunaControl/Types.h"

namespace TFLunaControl {

/// @brief Convert health enum to API string.
const char* healthToString(HealthState health);

/// @brief Populate status JSON document.
void populateStatusJson(JsonDocument& doc, const SystemStatus& sys, const Sample* sample);

/// @brief Populate compact live status JSON for WebSocket transport.
void populateLiveStatusJson(JsonDocument& doc, const SystemStatus& sys, const Sample* sample);

/// @brief Serialize status JSON into a bounded buffer.
/// @return Number of bytes written (excluding null terminator), or 0 when buffer is too small.
size_t serializeStatusJsonBounded(const SystemStatus& sys, const Sample* sample, char* out, size_t outLen);

/// @brief Serialize compact live status JSON into a bounded buffer.
/// @return Number of bytes written (excluding null terminator), or 0 when buffer is too small.
size_t serializeLiveStatusJsonBounded(const SystemStatus& sys, const Sample* sample, char* out, size_t outLen);

/// @brief Populate settings response JSON with write-only password semantics.
void populateSettingsJson(JsonDocument& doc, const RuntimeSettings& settings);

/// @brief Populate one device record JSON for `/api/devices`.
void populateDeviceStatusJson(JsonDocument& doc, const DeviceStatus& status, uint32_t id);

/// @brief Populate one sample record JSON for `/api/graph`.
/// @note Non-finite float values are emitted as null.
void populateGraphSampleJson(JsonDocument& doc, const Sample& sample);

}  // namespace TFLunaControl

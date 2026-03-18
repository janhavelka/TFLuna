#pragma once

#include <stddef.h>

#include <ArduinoJson.h>

#include "CO2Control/Health.h"
#include "CO2Control/RuntimeSettings.h"
#include "CO2Control/Types.h"

namespace CO2Control {

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

}  // namespace CO2Control

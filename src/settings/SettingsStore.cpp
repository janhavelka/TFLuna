#include "settings/SettingsStore.h"

#include <stddef.h>
#include <string.h>

#ifdef ARDUINO
#include <Preferences.h>
#endif

namespace TFLunaControl {

static constexpr uint32_t SETTINGS_MAGIC = 0x54464C43;  // "TFLC"
static constexpr uint16_t SETTINGS_VERSION = 16;

struct SettingsBlob {
  uint32_t magic = SETTINGS_MAGIC;
  uint16_t version = SETTINGS_VERSION;
  uint16_t reserved = 0;
  RuntimeSettings settings{};
  uint32_t checksum = 0;
};

static uint32_t calcChecksum(const uint8_t* data, size_t len) {
  uint32_t sum = 0;
  for (size_t i = 0; i < len; ++i) {
    sum = (sum * 33) ^ data[i];
  }
  return sum;
}

Status SettingsStore::begin(bool enable) {
  _enabled = enable;
  if (!_enabled) {
    return Ok();
  }
#ifdef ARDUINO
  Preferences prefs;
  if (!prefs.begin("tflunactrl", false)) {
    return Status(Err::COMM_FAILURE, 0, "NVS begin failed");
  }
  prefs.end();
  return Ok();
#else
  return Status(Err::NOT_INITIALIZED, 0, "NVS not available");
#endif
}

Status SettingsStore::load(RuntimeSettings& settings) {
  if (!_enabled) {
    return Status(Err::NOT_INITIALIZED, 0, "NVS disabled");
  }
#ifdef ARDUINO
  Preferences prefs;
  if (!prefs.begin("tflunactrl", true)) {
    return Status(Err::COMM_FAILURE, 0, "NVS begin failed");
  }

  SettingsBlob blob;
  const size_t got = prefs.getBytes("settings", &blob, sizeof(blob));
  prefs.end();

  if (got != sizeof(blob)) {
    return Status(Err::NOT_INITIALIZED, 0, "settings not found");
  }
  if (blob.magic != SETTINGS_MAGIC || blob.version != SETTINGS_VERSION) {
    return Status(Err::DATA_CORRUPT, 0, "settings version mismatch");
  }

  const uint32_t checksum = blob.checksum;
  blob.checksum = 0;
  const uint32_t calc = calcChecksum(reinterpret_cast<const uint8_t*>(&blob), sizeof(blob));
  if (checksum != calc) {
    return Status(Err::DATA_CORRUPT, 0, "settings checksum invalid");
  }

  settings = blob.settings;
  return Ok();
#else
  (void)settings;
  return Status(Err::NOT_INITIALIZED, 0, "NVS not available");
#endif
}

Status SettingsStore::save(const RuntimeSettings& settings) {
  if (!_enabled) {
    return Status(Err::NOT_INITIALIZED, 0, "NVS disabled");
  }
#ifdef ARDUINO
  Preferences prefs;
  if (!prefs.begin("tflunactrl", false)) {
    return Status(Err::COMM_FAILURE, 0, "NVS begin failed");
  }

  SettingsBlob blob;
  blob.settings = settings;
  blob.checksum = 0;
  blob.checksum = calcChecksum(reinterpret_cast<const uint8_t*>(&blob), sizeof(blob));

  const size_t wrote = prefs.putBytes("settings", &blob, sizeof(blob));
  prefs.end();
  if (wrote != sizeof(blob)) {
    return Status(Err::COMM_FAILURE, 0, "NVS write failed");
  }
  return Ok();
#else
  (void)settings;
  return Status(Err::NOT_INITIALIZED, 0, "NVS not available");
#endif
}

Status SettingsStore::factoryReset(RuntimeSettings& settings) {
  settings.restoreDefaults();
  if (!_enabled) {
    return Ok();
  }
  return save(settings);
}

}  // namespace TFLunaControl

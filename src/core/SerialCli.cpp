#include "core/SerialCli.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "TFLunaControl/Version.h"
#include "core/TimeUtil.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

#if __has_include(<ArduinoJson.h>)
#include <ArduinoJson.h>
#endif

#if __has_include("EE871/Version.h")
#include "EE871/Version.h"
#endif

#if __has_include("BME280/Version.h")
#include "BME280/Version.h"
#endif

#if __has_include("SHT3x/Version.h")
#include "SHT3x/Version.h"
#endif

#if __has_include("RV3032/Version.h")
#include "RV3032/Version.h"
#endif

#if __has_include("ssd1315/Version.h")
#include "ssd1315/Version.h"
#endif

#if __has_include("AsyncSD/Version.h")
#include "AsyncSD/Version.h"
#endif

#if __has_include("SystemChrono/Version.h")
#include "SystemChrono/Version.h"
#endif

#if __has_include("StatusLed/Version.h")
#include "StatusLed/Version.h"
#endif

#ifndef TFLUNACTRL_DEP_EE871_VERSION
#define TFLUNACTRL_DEP_EE871_VERSION "v0.3.0"
#endif

#ifndef TFLUNACTRL_DEP_BME280_VERSION
#define TFLUNACTRL_DEP_BME280_VERSION "v1.2.1"
#endif

#ifndef TFLUNACTRL_DEP_SHT3X_VERSION
#define TFLUNACTRL_DEP_SHT3X_VERSION "v1.4.0"
#endif

#ifndef TFLUNACTRL_DEP_RV3032_VERSION
#define TFLUNACTRL_DEP_RV3032_VERSION "v1.3.0"
#endif

#ifndef TFLUNACTRL_DEP_SSD1315_VERSION
#define TFLUNACTRL_DEP_SSD1315_VERSION "v1.1.0"
#endif

#ifndef TFLUNACTRL_DEP_ASYNCSD_VERSION
#define TFLUNACTRL_DEP_ASYNCSD_VERSION "v1.3.0"
#endif

#ifndef TFLUNACTRL_DEP_SYSTEMCHRONO_VERSION
#define TFLUNACTRL_DEP_SYSTEMCHRONO_VERSION "v1.2.0"
#endif

#ifndef TFLUNACTRL_DEP_STATUSLED_VERSION
#define TFLUNACTRL_DEP_STATUSLED_VERSION "v1.3.0"
#endif

#ifndef TFLUNACTRL_DEP_ARDUINOJSON_VERSION
#define TFLUNACTRL_DEP_ARDUINOJSON_VERSION "^6.21.3"
#endif

#ifndef TFLUNACTRL_DEP_ESPASYNCWEBSERVER_VERSION
#define TFLUNACTRL_DEP_ESPASYNCWEBSERVER_VERSION "v3.9.6"
#endif

#ifndef TFLUNACTRL_DEP_ASYNCTCP_VERSION
#define TFLUNACTRL_DEP_ASYNCTCP_VERSION "v3.4.10"
#endif

namespace TFLunaControl {

#ifndef ARDUINO

SerialCli::SerialCli(TFLunaControl& app) : _app(app) {}

Status SerialCli::begin() { return Ok(); }

void SerialCli::tick(uint32_t nowMs) {
  (void)nowMs;
}

void SerialCli::end() {}

#else

namespace {

static constexpr size_t CLI_MAX_TOKENS = HardwareSettings::CLI_MAX_TOKENS;
static constexpr size_t CLI_MAX_SAMPLE_PRINT = HardwareSettings::CLI_MAX_SAMPLE_PRINT;
static constexpr size_t CLI_MAX_EVENT_PRINT = HardwareSettings::CLI_MAX_EVENT_PRINT;
static constexpr size_t CLI_RX_BUDGET_PER_POLL = HardwareSettings::CLI_RX_BUDGET_PER_POLL;

static constexpr const char* CLI_ANSI_RESET = "\033[0m";
static constexpr const char* CLI_ANSI_BOLD = "\033[1m";
static constexpr const char* CLI_ANSI_INFO = "\033[36m";
static constexpr const char* CLI_ANSI_OK = "\033[32m";
static constexpr const char* CLI_ANSI_WARN = "\033[33m";
static constexpr const char* CLI_ANSI_ERR = "\033[31m";

const char* healthToStrColored(HealthState health) {
  switch (health) {
    case HealthState::OK:
      return "\033[32mOK\033[0m";
    case HealthState::DEGRADED:
      return "\033[33mDEGRADED\033[0m";
    case HealthState::FAULT:
      return "\033[31mFAULT\033[0m";
    case HealthState::UNKNOWN:
    default:
      return "\033[36mUNKNOWN\033[0m";
  }
}

const char* errToStr(Err err) {
  switch (err) {
    case Err::OK:
      return "OK";
    case Err::INVALID_CONFIG:
      return "INVALID_CONFIG";
    case Err::TIMEOUT:
      return "TIMEOUT";
    case Err::BUS_STUCK:
      return "BUS_STUCK";
    case Err::RESOURCE_BUSY:
      return "RESOURCE_BUSY";
    case Err::COMM_FAILURE:
      return "COMM_FAILURE";
    case Err::NOT_INITIALIZED:
      return "NOT_INITIALIZED";
    case Err::OUT_OF_MEMORY:
      return "OUT_OF_MEMORY";
    case Err::HARDWARE_FAULT:
      return "HARDWARE_FAULT";
    case Err::EXTERNAL_LIB_ERROR:
      return "EXTERNAL_LIB_ERROR";
    case Err::DATA_CORRUPT:
      return "DATA_CORRUPT";
    case Err::INTERNAL_ERROR:
    default:
      return "INTERNAL_ERROR";
  }
}

const char* errToStrColored(Err err) {
  switch (err) {
    case Err::OK:
      return "\033[32mOK\033[0m";
    case Err::INVALID_CONFIG:
      return "\033[31mINVALID_CONFIG\033[0m";
    case Err::RESOURCE_BUSY:
      return "\033[33mRESOURCE_BUSY\033[0m";
    case Err::TIMEOUT:
      return "\033[33mTIMEOUT\033[0m";
    case Err::BUS_STUCK:
      return "\033[31mBUS_STUCK\033[0m";
    case Err::COMM_FAILURE:
      return "\033[31mCOMM_FAILURE\033[0m";
    case Err::NOT_INITIALIZED:
      return "\033[31mNOT_INITIALIZED\033[0m";
    case Err::OUT_OF_MEMORY:
      return "\033[31mOUT_OF_MEMORY\033[0m";
    case Err::HARDWARE_FAULT:
      return "\033[31mHARDWARE_FAULT\033[0m";
    case Err::EXTERNAL_LIB_ERROR:
      return "\033[31mEXTERNAL_LIB_ERROR\033[0m";
    case Err::DATA_CORRUPT:
      return "\033[31mDATA_CORRUPT\033[0m";
    case Err::INTERNAL_ERROR:
      return "\033[31mINTERNAL_ERROR\033[0m";
    default:
      return "\033[31mINTERNAL_ERROR\033[0m";
  }
}

/// Colorized yes/no: trueâ†’green "yes", falseâ†’red "no"
const char* yesNo(bool v) {
  return v ? "\033[32myes\033[0m" : "\033[31mno\033[0m";
}
/// Colorized OK/NOT OK: trueâ†’green "yes", falseâ†’red "no"
const char* okNo(bool v) {
  return v ? "\033[32myes\033[0m" : "\033[31mno\033[0m";
}
/// Colorized valid/invalid
const char* validInvalid(bool v) {
  return v ? "\033[32mvalid\033[0m" : "\033[31minvalid\033[0m";
}
/// Colorize a counter: 0â†’plain, >0â†’red
void printCounterVal(const char* label, uint32_t value, const char* unit = "") {
  const char* color = (value > 0U) ? CLI_ANSI_ERR : CLI_ANSI_INFO;
  if (unit[0] == '\0') {
    Serial.printf("  %s%-28s%s %s%lu%s\n", CLI_ANSI_INFO, label, CLI_ANSI_RESET,
                  (value > 0U) ? color : "", static_cast<unsigned long>(value),
                  (value > 0U) ? CLI_ANSI_RESET : "");
  } else {
    Serial.printf("  %s%-28s%s %s%lu%s %s\n", CLI_ANSI_INFO, label, CLI_ANSI_RESET,
                  (value > 0U) ? color : "", static_cast<unsigned long>(value),
                  (value > 0U) ? CLI_ANSI_RESET : "", unit);
  }
}

void formatUptimeHuman(uint32_t uptimeMs, char* out, size_t len) {
  if (out == nullptr || len == 0U) {
    return;
  }
  const uint32_t totalSec = uptimeMs / 1000U;
  const uint32_t days = totalSec / 86400U;
  const uint32_t daySec = totalSec % 86400U;
  const uint32_t hours = daySec / 3600U;
  const uint32_t hourSec = daySec % 3600U;
  const uint32_t minutes = hourSec / 60U;
  const uint32_t seconds = hourSec % 60U;
  if (days > 0U) {
    snprintf(out, len, "%lud %02lu:%02lu:%02lu",
             static_cast<unsigned long>(days),
             static_cast<unsigned long>(hours),
             static_cast<unsigned long>(minutes),
             static_cast<unsigned long>(seconds));
  } else {
    snprintf(out, len, "%02lu:%02lu:%02lu",
             static_cast<unsigned long>(hours),
             static_cast<unsigned long>(minutes),
             static_cast<unsigned long>(seconds));
  }
}

void printHelpSection(const char* name) {
  Serial.printf("%s%s[%s]%s\n", CLI_ANSI_BOLD, CLI_ANSI_INFO, name, CLI_ANSI_RESET);
}

void printHelpLine(const char* command, const char* description) {
  Serial.printf("  %s%-28s%s - %s\n", CLI_ANSI_INFO, command, CLI_ANSI_RESET, description);
}

void printTopicHeader(const char* title) {
  Serial.printf("%s%s%s\n", CLI_ANSI_BOLD, title, CLI_ANSI_RESET);
  Serial.println("------------------------------");
}

void printTopicGroup(const char* label) {
  Serial.printf("%s%s%s\n", CLI_ANSI_WARN, label, CLI_ANSI_RESET);
}

void printTopicMeta(const char* label) {
  Serial.printf("%s%s%s\n", CLI_ANSI_INFO, label, CLI_ANSI_RESET);
}

void printHint(const char* text) {
  Serial.printf("%shint:%s %s\n", CLI_ANSI_INFO, CLI_ANSI_RESET, text);
}

void printOkf(const char* fmt, ...) {
  char line[192];
  va_list args;
  va_start(args, fmt);
  vsnprintf(line, sizeof(line), fmt, args);
  va_end(args);
  Serial.printf("%sOK%s %s\n", CLI_ANSI_OK, CLI_ANSI_RESET, line);
}

const char* i2cRawOpToStr(I2cRawOp op) {
  switch (op) {
    case I2cRawOp::WRITE:
      return "WRITE";
    case I2cRawOp::READ:
      return "READ";
    case I2cRawOp::WRITE_READ:
      return "WRITE_READ";
    case I2cRawOp::PROBE:
      return "PROBE";
    case I2cRawOp::NONE:
    default:
      return "NONE";
  }
}

const char* rtcDriverStateToStr(uint8_t state) {
  switch (state) {
    case 0:
      return "UNINIT";
    case 1:
      return "READY";
    case 2:
      return "DEGRADED";
    case 3:
      return "OFFLINE";
    default:
      return "UNKNOWN";
  }
}

const char* rtcBackupModeToStr(uint8_t mode) {
  switch (mode) {
    case 0:
      return "off";
    case 1:
      return "level";
    case 2:
      return "direct";
    default:
      return "unknown";
  }
}

const char* sdFsTypeToStr(uint8_t code) {
  switch (code) {
    case 1:
      return "fat12";
    case 2:
      return "fat16";
    case 3:
      return "fat32";
    case 4:
      return "exfat";
    default:
      return "unknown";
  }
}

const char* sdCardTypeToStr(uint8_t code) {
  switch (code) {
    case 1:
      return "sd1";
    case 2:
      return "sd2";
    case 3:
      return "sdhc";
    default:
      return "unknown";
  }
}

const char* cliVerbosityToStr(uint8_t level) {
  switch (level) {
    case 0:
      return "off";
    case 1:
      return "normal";
    case 2:
      return "verbose";
    default:
      return "unknown";
  }
}

bool parseU8Token(const char* token, uint8_t& out);

bool parseCliVerbosityToken(const char* token, uint8_t& out) {
  if (token == nullptr) {
    return false;
  }
  if (strcmp(token, "off") == 0 || strcmp(token, "compact") == 0) {
    out = 0U;
    return true;
  }
  if (strcmp(token, "normal") == 0) {
    out = 1U;
    return true;
  }
  if (strcmp(token, "verbose") == 0) {
    out = 2U;
    return true;
  }
  if (!parseU8Token(token, out)) {
    return false;
  }
  return out >= RuntimeSettings::MIN_CLI_VERBOSITY &&
         out <= RuntimeSettings::MAX_CLI_VERBOSITY;
}

void printVersionInfo() {
  Serial.printf("%sVersion%s\n", CLI_ANSI_BOLD, CLI_ANSI_RESET);
  Serial.println("------------------------------");
  Serial.printf("  tflunactrl: %s\n", ::TFLunaControl::VERSION);
  Serial.printf("  tflunactrl_full: %s\n", ::TFLunaControl::VERSION_FULL);
  Serial.printf("  build_meta: %s\n", ::TFLunaControl::BUILD_TIMESTAMP);
  Serial.printf("  git: %s (%s)\n", ::TFLunaControl::GIT_COMMIT, ::TFLunaControl::GIT_STATUS);
  Serial.printf("  firmware_build: %s %s\n", __DATE__, __TIME__);

  [[maybe_unused]] auto printMissingVersionHeader = [](const char* libKey,
                                                       const char* pinnedVersion) {
    Serial.printf("  %s%s: missing Version.h%s\n", CLI_ANSI_WARN, libKey, CLI_ANSI_RESET);
    Serial.printf("    %sinfo:%s using pinned dependency %s%s%s\n", CLI_ANSI_INFO, CLI_ANSI_RESET,
                  CLI_ANSI_BOLD, pinnedVersion, CLI_ANSI_RESET);
  };

#if __has_include("EE871/Version.h")
  Serial.printf("  lib_ee871: %s [header]\n", EE871::VERSION);
#else
  printMissingVersionHeader("lib_ee871", TFLUNACTRL_DEP_EE871_VERSION);
#endif
#if __has_include("BME280/Version.h")
  Serial.printf("  lib_bme280: %s [header]\n", BME280::VERSION);
#else
  printMissingVersionHeader("lib_bme280", TFLUNACTRL_DEP_BME280_VERSION);
#endif
#if __has_include("SHT3x/Version.h")
  Serial.printf("  lib_sht3x: %s [header]\n", SHT3x::VERSION);
#else
  printMissingVersionHeader("lib_sht3x", TFLUNACTRL_DEP_SHT3X_VERSION);
#endif
#if __has_include("RV3032/Version.h")
  Serial.printf("  lib_rv3032: %s [header]\n", RV3032::VERSION);
#else
  printMissingVersionHeader("lib_rv3032", TFLUNACTRL_DEP_RV3032_VERSION);
#endif
#if __has_include("ssd1315/Version.h")
  Serial.printf("  lib_ssd1315: %s [header]\n", SSD1315::VERSION);
#else
  printMissingVersionHeader("lib_ssd1315", TFLUNACTRL_DEP_SSD1315_VERSION);
#endif
#if __has_include("AsyncSD/Version.h")
  Serial.printf("  lib_asyncsd: %s [header]\n", AsyncSD::VERSION);
#else
  printMissingVersionHeader("lib_asyncsd", TFLUNACTRL_DEP_ASYNCSD_VERSION);
#endif
#if __has_include("SystemChrono/Version.h")
  Serial.printf("  lib_systemchrono: %s [header]\n", SystemChrono::VERSION);
#else
  printMissingVersionHeader("lib_systemchrono", TFLUNACTRL_DEP_SYSTEMCHRONO_VERSION);
#endif
#if __has_include("StatusLed/Version.h")
  Serial.printf("  lib_statusled: %s [header]\n", StatusLed::VERSION);
#else
  printMissingVersionHeader("lib_statusled", TFLUNACTRL_DEP_STATUSLED_VERSION);
#endif
#if defined(ARDUINOJSON_VERSION_MAJOR) && defined(ARDUINOJSON_VERSION_MINOR) && \
    defined(ARDUINOJSON_VERSION_REVISION)
  Serial.printf("  lib_arduinojson: %d.%d.%d [header]\n", ARDUINOJSON_VERSION_MAJOR,
                ARDUINOJSON_VERSION_MINOR, ARDUINOJSON_VERSION_REVISION);
#else
  printMissingVersionHeader("lib_arduinojson", TFLUNACTRL_DEP_ARDUINOJSON_VERSION);
#endif
  Serial.printf("  lib_espasyncwebserver: %s [pin]\n",
                TFLUNACTRL_DEP_ESPASYNCWEBSERVER_VERSION);
  Serial.printf("  lib_asynctcp: %s [pin]\n", TFLUNACTRL_DEP_ASYNCTCP_VERSION);
}

void sprintBytesHuman(char* buf, size_t bufLen, uint64_t bytes) {
  static const char* kUnits[] = {"B", "KB", "MB", "GB", "TB"};
  double value = static_cast<double>(bytes);
  size_t unit = 0;
  while (value >= 1024.0 && unit < (sizeof(kUnits) / sizeof(kUnits[0])) - 1U) {
    value /= 1024.0;
    unit++;
  }
  snprintf(buf, bufLen, "%.2f %s", value, kUnits[unit]);
}

bool parseBoolToken(const char* token, bool& out) {
  if (token == nullptr) {
    return false;
  }
  if (strcmp(token, "1") == 0 || strcmp(token, "true") == 0 ||
      strcmp(token, "on") == 0 || strcmp(token, "yes") == 0) {
    out = true;
    return true;
  }
  if (strcmp(token, "0") == 0 || strcmp(token, "false") == 0 ||
      strcmp(token, "off") == 0 || strcmp(token, "no") == 0) {
    out = false;
    return true;
  }
  return false;
}

bool parseU32Token(const char* token, uint32_t& out) {
  if (token == nullptr || token[0] == '\0' || token[0] == '-') {
    return false;
  }
  char* end = nullptr;
  const unsigned long value = strtoul(token, &end, 0);
  if (end == token || *end != '\0') {
    return false;
  }
  out = static_cast<uint32_t>(value);
  return true;
}

bool parseU16Token(const char* token, uint16_t& out) {
  uint32_t value = 0;
  if (!parseU32Token(token, value) || value > 0xFFFFUL) {
    return false;
  }
  out = static_cast<uint16_t>(value);
  return true;
}

bool parseU8Token(const char* token, uint8_t& out) {
  uint32_t value = 0;
  if (!parseU32Token(token, value) || value > 0xFFUL) {
    return false;
  }
  out = static_cast<uint8_t>(value);
  return true;
}

bool parseI32Token(const char* token, int32_t& out) {
  if (token == nullptr || token[0] == '\0') {
    return false;
  }
  char* end = nullptr;
  const long value = strtol(token, &end, 0);
  if (end == token || *end != '\0') {
    return false;
  }
  out = static_cast<int32_t>(value);
  return true;
}

bool parseI16Token(const char* token, int16_t& out) {
  int32_t value = 0;
  if (!parseI32Token(token, value) || value < -32768L || value > 32767L) {
    return false;
  }
  out = static_cast<int16_t>(value);
  return true;
}

bool parseI8Token(const char* token, int8_t& out) {
  int32_t value = 0;
  if (!parseI32Token(token, value) || value < -128L || value > 127L) {
    return false;
  }
  out = static_cast<int8_t>(value);
  return true;
}

bool isApPassSettingKey(const char* key) {
  static const char kApPass[] = {'a', 'p', '_', 'p', 'a', 's', 's', '\0'};
  return key != nullptr && (strcmp(key, kApPass) == 0 || strcmp(key, "ap_secret") == 0);
}

bool clampPrintCount(const char* token, size_t maxAllowed, size_t& outCount) {
  if (token == nullptr) {
    outCount = maxAllowed;
    return true;
  }
  uint32_t parsed = 0;
  if (!parseU32Token(token, parsed) || parsed == 0) {
    return false;
  }
  size_t count = static_cast<size_t>(parsed);
  if (count > maxAllowed) {
    count = maxAllowed;
  }
  outCount = count;
  return true;
}

bool isSettingGroup(const char* group) {
  if (group == nullptr) {
    return false;
  }
  return strcmp(group, "system") == 0 ||
         strcmp(group, "log") == 0 ||
         strcmp(group, "sd") == 0 ||
         strcmp(group, "lidar") == 0 ||
         strcmp(group, "tfluna") == 0 ||
         strcmp(group, "i2c") == 0 ||
         strcmp(group, "env") == 0 ||
         strcmp(group, "rtc") == 0 ||
         strcmp(group, "display") == 0 ||
         strcmp(group, "co2") == 0 ||
         strcmp(group, "e2") == 0 ||
         strcmp(group, "wifi") == 0 ||
         strcmp(group, "leds") == 0 ||
         strcmp(group, "web") == 0;
}

bool safePrefixedKey(char* out, size_t outSize, const char* prefix, const char* key) {
  if (out == nullptr || outSize == 0 || prefix == nullptr || key == nullptr) {
    return false;
  }
  const int n = snprintf(out, outSize, "%s%s", prefix, key);
  return n > 0 && static_cast<size_t>(n) < outSize;
}

bool resolveGroupedSettingKey(const char* group,
                              const char* key,
                              char* keyBuffer,
                              size_t keyBufferSize,
                              const char*& outKey,
                              const char*& errorMsg) {
  if (group == nullptr || key == nullptr) {
    errorMsg = "group/key missing";
    return false;
  }

  if (strcmp(group, "log") == 0 || strcmp(group, "sd") == 0) {
    if (!safePrefixedKey(keyBuffer, keyBufferSize, "log_", key)) {
      errorMsg = "key too long";
      return false;
    }
    outKey = keyBuffer;
    return true;
  }
  if (strcmp(group, "i2c") == 0) {
    if (!safePrefixedKey(keyBuffer, keyBufferSize, "i2c_", key)) {
      errorMsg = "key too long";
      return false;
    }
    outKey = keyBuffer;
    return true;
  }
  if (strcmp(group, "lidar") == 0 || strcmp(group, "tfluna") == 0) {
    if (strcmp(key, "service_ms") == 0) {
      outKey = "lidar_service_ms";
      return true;
    }
    if (strcmp(key, "min_strength") == 0) {
      outKey = "lidar_min_strength";
      return true;
    }
    if (strcmp(key, "max_distance_cm") == 0) {
      outKey = "lidar_max_distance_cm";
      return true;
    }
    if (strcmp(key, "frame_stale_ms") == 0 || strcmp(key, "stale_ms") == 0) {
      outKey = "lidar_frame_stale_ms";
      return true;
    }
    if (strcmp(key, "serial_print_interval_ms") == 0 || strcmp(key, "serial_ms") == 0) {
      outKey = "serial_print_interval_ms";
      return true;
    }
    errorMsg = "unknown lidar key";
    return false;
  }
  if (strcmp(group, "e2") == 0 || strcmp(group, "co2") == 0) {
    if (!safePrefixedKey(keyBuffer, keyBufferSize, "e2_", key)) {
      errorMsg = "key too long";
      return false;
    }
    outKey = keyBuffer;
    return true;
  }

  if (strcmp(group, "env") == 0) {
    if (strcmp(key, "address") == 0) {
      outKey = "i2c_env_address";
      return true;
    }
    if (strcmp(key, "poll_ms") == 0) {
      outKey = "i2c_env_poll_ms";
      return true;
    }
    if (strcmp(key, "conversion_wait_ms") == 0) {
      outKey = "i2c_env_conversion_wait_ms";
      return true;
    }
    if (strcmp(key, "bme_mode") == 0) {
      outKey = "i2c_env_bme_mode";
      return true;
    }
    if (strcmp(key, "bme_osrs_t") == 0) {
      outKey = "i2c_env_bme_osrs_t";
      return true;
    }
    if (strcmp(key, "bme_osrs_p") == 0) {
      outKey = "i2c_env_bme_osrs_p";
      return true;
    }
    if (strcmp(key, "bme_osrs_h") == 0) {
      outKey = "i2c_env_bme_osrs_h";
      return true;
    }
    if (strcmp(key, "bme_filter") == 0) {
      outKey = "i2c_env_bme_filter";
      return true;
    }
    if (strcmp(key, "bme_standby") == 0) {
      outKey = "i2c_env_bme_standby";
      return true;
    }
    if (strcmp(key, "sht_mode") == 0) {
      outKey = "i2c_env_sht_mode";
      return true;
    }
    if (strcmp(key, "sht_repeatability") == 0) {
      outKey = "i2c_env_sht_repeatability";
      return true;
    }
    if (strcmp(key, "sht_periodic_rate") == 0 || strcmp(key, "sht_rate") == 0) {
      outKey = "i2c_env_sht_periodic_rate";
      return true;
    }
    if (strcmp(key, "sht_clock_stretching") == 0 || strcmp(key, "sht_stretch") == 0) {
      outKey = "i2c_env_sht_clock_stretching";
      return true;
    }
    if (strcmp(key, "sht_low_vdd") == 0) {
      outKey = "i2c_env_sht_low_vdd";
      return true;
    }
    if (strcmp(key, "sht_command_delay_ms") == 0 || strcmp(key, "sht_command_delay") == 0) {
      outKey = "i2c_env_sht_command_delay_ms";
      return true;
    }
    if (strcmp(key, "sht_not_ready_timeout_ms") == 0 || strcmp(key, "sht_not_ready_timeout") == 0) {
      outKey = "i2c_env_sht_not_ready_timeout_ms";
      return true;
    }
    if (strcmp(key, "sht_periodic_fetch_margin_ms") == 0 || strcmp(key, "sht_fetch_margin") == 0) {
      outKey = "i2c_env_sht_periodic_fetch_margin_ms";
      return true;
    }
    if (strcmp(key, "sht_allow_general_call_reset") == 0 || strcmp(key, "sht_allow_gc_reset") == 0) {
      outKey = "i2c_env_sht_allow_general_call_reset";
      return true;
    }
    if (strcmp(key, "sht_recover_use_bus_reset") == 0 || strcmp(key, "sht_recover_bus_reset") == 0) {
      outKey = "i2c_env_sht_recover_use_bus_reset";
      return true;
    }
    if (strcmp(key, "sht_recover_use_soft_reset") == 0 || strcmp(key, "sht_recover_soft_reset") == 0) {
      outKey = "i2c_env_sht_recover_use_soft_reset";
      return true;
    }
    if (strcmp(key, "sht_recover_use_hard_reset") == 0 || strcmp(key, "sht_recover_hard_reset") == 0) {
      outKey = "i2c_env_sht_recover_use_hard_reset";
      return true;
    }
    errorMsg = "unknown env key";
    return false;
  }

  if (strcmp(group, "rtc") == 0) {
    if (strcmp(key, "address") == 0) {
      errorMsg = "rtc address is fixed at 0x51";
      return false;
    }
    if (strcmp(key, "poll_ms") == 0) {
      outKey = "i2c_rtc_poll_ms";
      return true;
    }
    if (strcmp(key, "backup_mode") == 0) {
      outKey = "i2c_rtc_backup_mode";
      return true;
    }
    if (strcmp(key, "enable_eeprom_writes") == 0 || strcmp(key, "eeprom_writes") == 0) {
      outKey = "i2c_rtc_enable_eeprom_writes";
      return true;
    }
    if (strcmp(key, "eeprom_timeout_ms") == 0 || strcmp(key, "eeprom_timeout") == 0) {
      outKey = "i2c_rtc_eeprom_timeout_ms";
      return true;
    }
    if (strcmp(key, "offline_threshold") == 0) {
      outKey = "i2c_rtc_offline_threshold";
      return true;
    }
    errorMsg = "unknown rtc key";
    return false;
  }

  if (strcmp(group, "display") == 0) {
    if (strcmp(key, "address") == 0) {
      outKey = "i2c_display_address";
      return true;
    }
    if (strcmp(key, "poll_ms") == 0) {
      outKey = "i2c_display_poll_ms";
      return true;
    }
    errorMsg = "unknown display key";
    return false;
  }

  if (strcmp(group, "wifi") == 0) {
    if (strcmp(key, "enabled") == 0) {
      outKey = "wifi_enabled";
      return true;
    }
    if (strcmp(key, "ssid") == 0) {
      outKey = "ap_ssid";
      return true;
    }
    if (strcmp(key, "secret") == 0 || strcmp(key, "pass") == 0 || strcmp(key, "password") == 0) {
      outKey = "ap_secret";
      return true;
    }
    if (strcmp(key, "auto_off_ms") == 0) {
      outKey = "ap_auto_off_ms";
      return true;
    }
    errorMsg = "unknown wifi key";
    return false;
  }

  if (strcmp(group, "system") == 0) {
    if (strcmp(key, "verbosity") == 0 || strcmp(key, "cli_verbosity") == 0) {
      outKey = "cli_verbosity";
      return true;
    }
    outKey = key;
    return true;
  }

  if (strcmp(group, "leds") == 0) {
    if (strcmp(key, "health_init") == 0 || strcmp(key, "health_init_ms") == 0) {
      outKey = "led_health_init_ms";
      return true;
    }
    if (strcmp(key, "health_debounce") == 0 || strcmp(key, "health_debounce_ms") == 0) {
      outKey = "led_health_debounce_ms";
      return true;
    }
    errorMsg = "unknown leds key";
    return false;
  }

  if (strcmp(group, "web") == 0) {
    if (!safePrefixedKey(keyBuffer, keyBufferSize, "web_", key)) {
      errorMsg = "key too long";
      return false;
    }
    outKey = keyBuffer;
    return true;
  }

  errorMsg = "unknown group";
  return false;
}

const char* normalizeDeviceName(const char* name) {
  if (name == nullptr) {
    return nullptr;
  }
  if (strcmp(name, "i2c") == 0) {
    return "i2c_bus";
  }
  if (strcmp(name, "e2") == 0 || strcmp(name, "co2") == 0 ||
      strcmp(name, "lidar") == 0 || strcmp(name, "tfluna") == 0) {
    return "lidar";
  }
  return name;
}

bool deviceNameEquals(const char* lhs, const char* rhs) {
  if (lhs == nullptr || rhs == nullptr) {
    return false;
  }
  return strcmp(normalizeDeviceName(lhs), normalizeDeviceName(rhs)) == 0;
}

const char* settingsSectionForDevice(const char* deviceName) {
  if (deviceName == nullptr) {
    return nullptr;
  }

  const char* normalized = normalizeDeviceName(deviceName);
  if (normalized == nullptr) {
    return nullptr;
  }

  if (strcmp(normalized, "i2c_bus") == 0) {
    return "i2c";
  }
  if (strcmp(normalized, "env") == 0) {
    return "env";
  }
  if (strcmp(normalized, "rtc") == 0) {
    return "rtc";
  }
  if (strcmp(normalized, "lidar") == 0) {
    return "lidar";
  }
  if (strcmp(normalized, "sd") == 0) {
    return "sd";
  }
  if (strcmp(normalized, "wifi") == 0) {
    return "wifi";
  }
  if (strcmp(normalized, "web") == 0) {
    return "web";
  }
  if (strcmp(normalized, "leds") == 0) {
    return "system";
  }
  if (strcmp(normalized, "system") == 0) {
    return "system";
  }
  return nullptr;
}

bool isBme280Address(uint8_t address) {
  return address == 0x76U || address == 0x77U;
}

bool isSht3xAddress(uint8_t address) {
  return address == 0x44U || address == 0x45U;
}

const char* envModelHint(uint8_t address) {
  if (isBme280Address(address)) {
    return "BME280";
  }
  if (isSht3xAddress(address)) {
    return "SHT3x";
  }
  return "unknown";
}

bool parseI2cAddressToken(const char* token, uint8_t& out) {
  if (!parseU8Token(token, out)) {
    return false;
  }
  return out >= 0x01U && out <= 0x7FU;
}

bool applySettingByKey(RuntimeSettings& settings,
                       const char* key,
                       const char* value,
                       const char*& errorMsg) {
  if (key == nullptr || value == nullptr) {
    errorMsg = "key/value missing";
    return false;
  }

  if (strcmp(key, "sample_interval_ms") == 0) {
    return parseU32Token(value, settings.sampleIntervalMs) ? true : (errorMsg = "invalid u32", false);
  }
  if (strcmp(key, "sample_interval_sec") == 0) {
    uint32_t seconds = 0;
    if (!parseU32Token(value, seconds)) {
      errorMsg = "invalid u32";
      return false;
    }
    if (seconds > (UINT32_MAX / 1000U)) {
      errorMsg = "sample interval too large";
      return false;
    }
    settings.sampleIntervalMs = seconds * 1000U;
    return true;
  }
  if (strcmp(key, "log_daily_enabled") == 0) {
    return parseBoolToken(value, settings.logDailyEnabled) ? true : (errorMsg = "invalid bool", false);
  }
  if (strcmp(key, "log_all_enabled") == 0) {
    return parseBoolToken(value, settings.logAllEnabled) ? true : (errorMsg = "invalid bool", false);
  }
  if (strcmp(key, "log_all_max_bytes") == 0) {
    return parseU32Token(value, settings.logAllMaxBytes) ? true : (errorMsg = "invalid u32", false);
  }
  if (strcmp(key, "log_flush_ms") == 0) {
    return parseU32Token(value, settings.logFlushMs) ? true : (errorMsg = "invalid u32", false);
  }
  if (strcmp(key, "log_io_budget_ms") == 0) {
    return parseU32Token(value, settings.logIoBudgetMs) ? true : (errorMsg = "invalid u32", false);
  }
  if (strcmp(key, "log_mount_retry_ms") == 0) {
    return parseU32Token(value, settings.logMountRetryMs) ? true : (errorMsg = "invalid u32", false);
  }
  if (strcmp(key, "log_write_retry_backoff_ms") == 0) {
    return parseU32Token(value, settings.logWriteRetryBackoffMs) ? true : (errorMsg = "invalid u32", false);
  }
  if (strcmp(key, "log_max_write_retries") == 0) {
    return parseU8Token(value, settings.logMaxWriteRetries) ? true : (errorMsg = "invalid u8", false);
  }
  if (strcmp(key, "log_session_name") == 0) {
    strncpy(settings.logSessionName, value, sizeof(settings.logSessionName) - 1);
    settings.logSessionName[sizeof(settings.logSessionName) - 1] = '\0';
    return true;
  }
  if (strcmp(key, "log_events_max_bytes") == 0) {
    return parseU32Token(value, settings.logEventsMaxBytes) ? true : (errorMsg = "invalid u32", false);
  }
  if (strcmp(key, "lidar_service_ms") == 0) {
    return parseU32Token(value, settings.lidarServiceMs) ? true : (errorMsg = "invalid u32", false);
  }
  if (strcmp(key, "lidar_min_strength") == 0) {
    return parseU16Token(value, settings.lidarMinStrength) ? true : (errorMsg = "invalid u16", false);
  }
  if (strcmp(key, "lidar_max_distance_cm") == 0) {
    return parseU16Token(value, settings.lidarMaxDistanceCm) ? true : (errorMsg = "invalid u16", false);
  }
  if (strcmp(key, "lidar_frame_stale_ms") == 0) {
    return parseU32Token(value, settings.lidarFrameStaleMs) ? true : (errorMsg = "invalid u32", false);
  }
  if (strcmp(key, "serial_print_interval_ms") == 0) {
    return parseU32Token(value, settings.serialPrintIntervalMs) ? true : (errorMsg = "invalid u32", false);
  }
  if (strcmp(key, "cli_verbosity") == 0) {
    return parseCliVerbosityToken(value, settings.cliVerbosity)
               ? true
               : (errorMsg = "invalid verbosity (off|normal|verbose or 0..2)", false);
  }
  if (strcmp(key, "i2c_freq_hz") == 0) {
    return parseU32Token(value, settings.i2cFreqHz) ? true : (errorMsg = "invalid u32", false);
  }
  if (strcmp(key, "i2c_op_timeout_ms") == 0) {
    return parseU32Token(value, settings.i2cOpTimeoutMs) ? true : (errorMsg = "invalid u32", false);
  }
  if (strcmp(key, "i2c_stuck_debounce_ms") == 0) {
    return parseU8Token(value, settings.i2cStuckDebounceMs) ? true : (errorMsg = "invalid u8", false);
  }
  if (strcmp(key, "i2c_max_consecutive_failures") == 0) {
    return parseU8Token(value, settings.i2cMaxConsecutiveFailures) ? true : (errorMsg = "invalid u8", false);
  }
  if (strcmp(key, "i2c_recovery_backoff_ms") == 0) {
    return parseU32Token(value, settings.i2cRecoveryBackoffMs) ? true : (errorMsg = "invalid u32", false);
  }
  if (strcmp(key, "i2c_recovery_backoff_max_ms") == 0) {
    return parseU32Token(value, settings.i2cRecoveryBackoffMaxMs) ? true : (errorMsg = "invalid u32", false);
  }
  if (strcmp(key, "i2c_requests_per_tick") == 0) {
    return parseU8Token(value, settings.i2cRequestsPerTick) ? true : (errorMsg = "invalid u8", false);
  }
  if (strcmp(key, "i2c_slow_op_threshold_us") == 0) {
    return parseU32Token(value, settings.i2cSlowOpThresholdUs) ? true : (errorMsg = "invalid u32", false);
  }
  if (strcmp(key, "i2c_slow_op_degrade_count") == 0) {
    return parseU8Token(value, settings.i2cSlowOpDegradeCount) ? true : (errorMsg = "invalid u8", false);
  }
  if (strcmp(key, "i2c_task_heartbeat_timeout_ms") == 0) {
    return parseU32Token(value, settings.i2cTaskHeartbeatTimeoutMs) ? true : (errorMsg = "invalid u32", false);
  }
  if (strcmp(key, "i2c_env_poll_ms") == 0) {
    return parseU32Token(value, settings.i2cEnvPollMs) ? true : (errorMsg = "invalid u32", false);
  }
  if (strcmp(key, "i2c_rtc_poll_ms") == 0) {
    return parseU32Token(value, settings.i2cRtcPollMs) ? true : (errorMsg = "invalid u32", false);
  }
  if (strcmp(key, "i2c_display_poll_ms") == 0) {
    return parseU32Token(value, settings.i2cDisplayPollMs) ? true : (errorMsg = "invalid u32", false);
  }
  if (strcmp(key, "i2c_env_conversion_wait_ms") == 0) {
    return parseU32Token(value, settings.i2cEnvConversionWaitMs) ? true : (errorMsg = "invalid u32", false);
  }
  if (strcmp(key, "i2c_env_bme_mode") == 0) {
    return parseU8Token(value, settings.i2cEnvBmeMode) ? true : (errorMsg = "invalid u8", false);
  }
  if (strcmp(key, "i2c_env_bme_osrs_t") == 0) {
    return parseU8Token(value, settings.i2cEnvBmeOsrsT) ? true : (errorMsg = "invalid u8", false);
  }
  if (strcmp(key, "i2c_env_bme_osrs_p") == 0) {
    return parseU8Token(value, settings.i2cEnvBmeOsrsP) ? true : (errorMsg = "invalid u8", false);
  }
  if (strcmp(key, "i2c_env_bme_osrs_h") == 0) {
    return parseU8Token(value, settings.i2cEnvBmeOsrsH) ? true : (errorMsg = "invalid u8", false);
  }
  if (strcmp(key, "i2c_env_bme_filter") == 0) {
    return parseU8Token(value, settings.i2cEnvBmeFilter) ? true : (errorMsg = "invalid u8", false);
  }
  if (strcmp(key, "i2c_env_bme_standby") == 0) {
    return parseU8Token(value, settings.i2cEnvBmeStandby) ? true : (errorMsg = "invalid u8", false);
  }
  if (strcmp(key, "i2c_env_sht_mode") == 0) {
    return parseU8Token(value, settings.i2cEnvShtMode) ? true : (errorMsg = "invalid u8", false);
  }
  if (strcmp(key, "i2c_env_sht_repeatability") == 0) {
    return parseU8Token(value, settings.i2cEnvShtRepeatability) ? true : (errorMsg = "invalid u8", false);
  }
  if (strcmp(key, "i2c_env_sht_periodic_rate") == 0) {
    return parseU8Token(value, settings.i2cEnvShtPeriodicRate) ? true : (errorMsg = "invalid u8", false);
  }
  if (strcmp(key, "i2c_env_sht_clock_stretching") == 0) {
    return parseU8Token(value, settings.i2cEnvShtClockStretching) ? true : (errorMsg = "invalid u8", false);
  }
  if (strcmp(key, "i2c_env_sht_low_vdd") == 0) {
    return parseBoolToken(value, settings.i2cEnvShtLowVdd) ? true : (errorMsg = "invalid bool", false);
  }
  if (strcmp(key, "i2c_env_sht_command_delay_ms") == 0) {
    return parseU16Token(value, settings.i2cEnvShtCommandDelayMs) ? true : (errorMsg = "invalid u16", false);
  }
  if (strcmp(key, "i2c_env_sht_not_ready_timeout_ms") == 0) {
    return parseU32Token(value, settings.i2cEnvShtNotReadyTimeoutMs) ? true : (errorMsg = "invalid u32", false);
  }
  if (strcmp(key, "i2c_env_sht_periodic_fetch_margin_ms") == 0) {
    return parseU32Token(value, settings.i2cEnvShtPeriodicFetchMarginMs) ? true : (errorMsg = "invalid u32", false);
  }
  if (strcmp(key, "i2c_env_sht_allow_general_call_reset") == 0) {
    return parseBoolToken(value, settings.i2cEnvShtAllowGeneralCallReset) ? true : (errorMsg = "invalid bool", false);
  }
  if (strcmp(key, "i2c_env_sht_recover_use_bus_reset") == 0) {
    return parseBoolToken(value, settings.i2cEnvShtRecoverUseBusReset) ? true : (errorMsg = "invalid bool", false);
  }
  if (strcmp(key, "i2c_env_sht_recover_use_soft_reset") == 0) {
    return parseBoolToken(value, settings.i2cEnvShtRecoverUseSoftReset) ? true : (errorMsg = "invalid bool", false);
  }
  if (strcmp(key, "i2c_env_sht_recover_use_hard_reset") == 0) {
    return parseBoolToken(value, settings.i2cEnvShtRecoverUseHardReset) ? true : (errorMsg = "invalid bool", false);
  }
  if (strcmp(key, "i2c_recover_timeout_ms") == 0) {
    return parseU32Token(value, settings.i2cRecoverTimeoutMs) ? true : (errorMsg = "invalid u32", false);
  }
  if (strcmp(key, "i2c_max_results_per_tick") == 0) {
    return parseU8Token(value, settings.i2cMaxResultsPerTick) ? true : (errorMsg = "invalid u8", false);
  }
  if (strcmp(key, "i2c_task_wait_ms") == 0) {
    return parseU32Token(value, settings.i2cTaskWaitMs) ? true : (errorMsg = "invalid u32", false);
  }
  if (strcmp(key, "i2c_health_stale_task_multiplier") == 0) {
    return parseU8Token(value, settings.i2cHealthStaleTaskMultiplier) ? true : (errorMsg = "invalid u8", false);
  }
  if (strcmp(key, "i2c_slow_window_ms") == 0) {
    return parseU32Token(value, settings.i2cSlowWindowMs) ? true : (errorMsg = "invalid u32", false);
  }
  if (strcmp(key, "i2c_health_recent_window_ms") == 0) {
    return parseU32Token(value, settings.i2cHealthRecentWindowMs) ? true : (errorMsg = "invalid u32", false);
  }
  if (strcmp(key, "i2c_env_address") == 0) {
    return parseU8Token(value, settings.i2cEnvAddress) ? true : (errorMsg = "invalid u8", false);
  }
  if (strcmp(key, "i2c_rtc_address") == 0) {
    errorMsg = "i2c_rtc_address is fixed at 0x51";
    return false;
  }
  if (strcmp(key, "i2c_rtc_backup_mode") == 0) {
    return parseU8Token(value, settings.i2cRtcBackupMode) ? true : (errorMsg = "invalid u8", false);
  }
  if (strcmp(key, "i2c_rtc_enable_eeprom_writes") == 0) {
    return parseBoolToken(value, settings.i2cRtcEnableEepromWrites) ? true : (errorMsg = "invalid bool", false);
  }
  if (strcmp(key, "i2c_rtc_eeprom_timeout_ms") == 0) {
    return parseU32Token(value, settings.i2cRtcEepromTimeoutMs) ? true : (errorMsg = "invalid u32", false);
  }
  if (strcmp(key, "i2c_rtc_offline_threshold") == 0) {
    return parseU8Token(value, settings.i2cRtcOfflineThreshold) ? true : (errorMsg = "invalid u8", false);
  }
  if (strcmp(key, "i2c_display_address") == 0) {
    return parseU8Token(value, settings.i2cDisplayAddress) ? true : (errorMsg = "invalid u8", false);
  }
  if (strcmp(key, "e2_address") == 0) {
    return parseU8Token(value, settings.e2Address) ? true : (errorMsg = "invalid u8", false);
  }
  if (strcmp(key, "e2_bit_timeout_us") == 0) {
    return parseU32Token(value, settings.e2BitTimeoutUs) ? true : (errorMsg = "invalid u32", false);
  }
  if (strcmp(key, "e2_byte_timeout_us") == 0) {
    return parseU32Token(value, settings.e2ByteTimeoutUs) ? true : (errorMsg = "invalid u32", false);
  }
  if (strcmp(key, "e2_clock_low_us") == 0) {
    return parseU16Token(value, settings.e2ClockLowUs) ? true : (errorMsg = "invalid u16", false);
  }
  if (strcmp(key, "e2_clock_high_us") == 0) {
    return parseU16Token(value, settings.e2ClockHighUs) ? true : (errorMsg = "invalid u16", false);
  }
  if (strcmp(key, "e2_start_hold_us") == 0) {
    return parseU16Token(value, settings.e2StartHoldUs) ? true : (errorMsg = "invalid u16", false);
  }
  if (strcmp(key, "e2_stop_hold_us") == 0) {
    return parseU16Token(value, settings.e2StopHoldUs) ? true : (errorMsg = "invalid u16", false);
  }
  if (strcmp(key, "e2_write_delay_ms") == 0) {
    return parseU32Token(value, settings.e2WriteDelayMs) ? true : (errorMsg = "invalid u32", false);
  }
  if (strcmp(key, "e2_interval_write_delay_ms") == 0) {
    return parseU32Token(value, settings.e2IntervalWriteDelayMs) ? true : (errorMsg = "invalid u32", false);
  }
  if (strcmp(key, "e2_offline_threshold") == 0) {
    return parseU8Token(value, settings.e2OfflineThreshold) ? true : (errorMsg = "invalid u8", false);
  }
  if (strcmp(key, "e2_recovery_backoff_ms") == 0) {
    return parseU32Token(value, settings.e2RecoveryBackoffMs) ? true : (errorMsg = "invalid u32", false);
  }
  if (strcmp(key, "e2_recovery_backoff_max_ms") == 0) {
    return parseU32Token(value, settings.e2RecoveryBackoffMaxMs) ? true : (errorMsg = "invalid u32", false);
  }
  if (strcmp(key, "e2_config_interval_ds") == 0) {
    return parseU16Token(value, settings.e2ConfigIntervalDs) ? true : (errorMsg = "invalid u16", false);
  }
  if (strcmp(key, "e2_config_co2_interval_factor") == 0) {
    return parseI8Token(value, settings.e2ConfigCo2IntervalFactor) ? true : (errorMsg = "invalid i8", false);
  }
  if (strcmp(key, "e2_config_filter") == 0) {
    return parseU8Token(value, settings.e2ConfigFilter) ? true : (errorMsg = "invalid u8", false);
  }
  if (strcmp(key, "e2_config_operating_mode") == 0) {
    return parseU8Token(value, settings.e2ConfigOperatingMode) ? true : (errorMsg = "invalid u8", false);
  }
  if (strcmp(key, "e2_config_offset_ppm") == 0) {
    return parseI16Token(value, settings.e2ConfigOffsetPpm) ? true : (errorMsg = "invalid i16", false);
  }
  if (strcmp(key, "e2_config_gain") == 0) {
    return parseU16Token(value, settings.e2ConfigGain) ? true : (errorMsg = "invalid u16", false);
  }
  if (strcmp(key, "wifi_enabled") == 0) {
    return parseBoolToken(value, settings.wifiEnabled) ? true : (errorMsg = "invalid bool", false);
  }
  if (strcmp(key, "ap_ssid") == 0) {
    strncpy(settings.apSsid, value, sizeof(settings.apSsid) - 1);
    settings.apSsid[sizeof(settings.apSsid) - 1] = '\0';
    return true;
  }
  if (isApPassSettingKey(key)) {
    strncpy(settings.apPass, value, sizeof(settings.apPass) - 1);
    settings.apPass[sizeof(settings.apPass) - 1] = '\0';
    return true;
  }
  if (strcmp(key, "ap_auto_off_ms") == 0) {
    return parseU32Token(value, settings.apAutoOffMs) ? true : (errorMsg = "invalid u32", false);
  }
  if (strcmp(key, "command_drain_per_tick") == 0) {
    return parseU8Token(value, settings.commandDrainPerTick) ? true : (errorMsg = "invalid u8", false);
  }
  if (strcmp(key, "command_queue_degraded_window_ms") == 0) {
    return parseU32Token(value, settings.commandQueueDegradedWindowMs) ? true : (errorMsg = "invalid u32", false);
  }
  if (strcmp(key, "command_queue_degraded_depth_threshold") == 0) {
    return parseU8Token(value, settings.commandQueueDegradedDepthThreshold) ? true : (errorMsg = "invalid u8", false);
  }
  if (strcmp(key, "main_tick_slow_threshold_us") == 0) {
    return parseU32Token(value, settings.mainTickSlowThresholdUs) ? true : (errorMsg = "invalid u32", false);
  }
  if (strcmp(key, "led_health_init_ms") == 0) {
    return parseU32Token(value, settings.ledHealthInitMs) ? true : (errorMsg = "invalid u32", false);
  }
  if (strcmp(key, "led_health_debounce_ms") == 0) {
    return parseU32Token(value, settings.ledHealthDebounceMs) ? true : (errorMsg = "invalid u32", false);
  }
  if (strcmp(key, "ap_start_retry_backoff_ms") == 0) {
    return parseU32Token(value, settings.apStartRetryBackoffMs) ? true : (errorMsg = "invalid u32", false);
  }
  if (strcmp(key, "web_max_settings_body_bytes") == 0) {
    return parseU16Token(value, settings.webMaxSettingsBodyBytes) ? true : (errorMsg = "invalid u16", false);
  }
  if (strcmp(key, "web_max_rtc_body_bytes") == 0) {
    return parseU16Token(value, settings.webMaxRtcBodyBytes) ? true : (errorMsg = "invalid u16", false);
  }

  errorMsg = "unknown key";
  return false;
}

}  // namespace

SerialCli::SerialCli(TFLunaControl& app) : _app(app) {}

Status SerialCli::begin() {
  Serial.println();
  Serial.printf("%sTFLunaControl Serial CLI%s\n", CLI_ANSI_BOLD, CLI_ANSI_RESET);
  Serial.println("Type 'help' or '?' for commands.");
  Serial.print("> ");
  return Ok();
}

void SerialCli::tick(uint32_t nowMs) {
  size_t rxBudget = CLI_RX_BUDGET_PER_POLL;
  while (rxBudget > 0 && Serial.available() > 0) {
    rxBudget--;
    const char ch = static_cast<char>(Serial.read());
    if (ch == '\r') {
      continue;
    }
    if (ch == '\n') {
      _line[_lineLen] = '\0';
      if (_lineLen > 0) {
        executeLine(_line, nowMs);
      }
      _lineLen = 0;
      Serial.print("> ");
      return;
    }
    if (ch == '\b' || ch == 0x7F) {
      if (_lineLen > 0) {
        _lineLen--;
      }
      continue;
    }
  if (_lineLen + 1 < LINE_BYTES) {
      _line[_lineLen++] = ch;
    } else {
      _lineLen = 0;
      Serial.printf("%sERR%s line too long\n", CLI_ANSI_ERR, CLI_ANSI_RESET);
      Serial.print("> ");
      return;
    }
  }
}

void SerialCli::printHelp(const char* topic) {
  const char* resolved = topic;
  if (resolved == nullptr || resolved[0] == '\0' || strcmp(resolved, "all") == 0 ||
      strcmp(resolved, "topics") == 0) {
    Serial.printf("%sTFLunaControl CLI%s\n", CLI_ANSI_BOLD, CLI_ANSI_RESET);
    Serial.println("------------------------------");
    printHelpSection("Common");
    printHelpLine("help / ?", "Show this help");
    printHelpLine("help <topic>", "Deep topic help (subcommands, returns, flows)");
    printHelpLine("help flow", "Recommended operator command workflows");
    printHelpLine("version", "Firmware and dependency versions");
    printHelpLine("status", "System health summary");
    printHelpLine("devices", "Health summary for all devices");
    printHelpLine("read [all|lidar|env|rtc]", "Latest cached measurements");
    printHelpLine("ls [path]", "Show runtime-tracked SD log paths");
    printHelpLine("settings show all", "Runtime settings snapshot");
    printHelpLine("diag all", "Guided diagnostics");
    Serial.println();
    printHelpSection("Domains");
    printHelpLine("env", "Environment sensor control");
    printHelpLine("rtc", "RTC control");
    printHelpLine("lidar / tfluna / co2", "TF-Luna UART diagnostics and settings");
    printHelpLine("i2c", "I2C bus diagnostics and settings");
    printHelpLine("display", "SSD1315 display control");
    printHelpLine("sd", "SD logger control");
    printHelpLine("wifi", "SoftAP control");
    printHelpLine("web", "Web server limits");
    printHelpLine("system", "System timing/control settings");
    printHelpLine("leds", "Status LED behavior");
    printHelpLine("button", "Button diagnostics");
    Serial.println();
    printHelpSection("Aliases");
    printHelpLine("doctor", "Alias of diag");
    printHelpLine("ver", "Alias of version");
    printHelpLine("guide <topic>", "Alias of help <topic>");
    printHelpLine("i2c_scan [status]", "Alias of i2c scan");
    printHelpLine("i2c_recover", "Alias of i2c recover");
    printHelpLine("ls [path]", "Alias of sd ls [path]");
    Serial.printf("\n%sHint:%s use %shelp <topic>%s for detailed command contracts.\n",
                  CLI_ANSI_INFO,
                  CLI_ANSI_RESET,
                  CLI_ANSI_INFO,
                  CLI_ANSI_RESET);
    return;
  }

  if (strcmp(resolved, "syntax") == 0) {
    printTopicHeader("CLI Syntax");
    printTopicGroup("General forms");
    Serial.println("  command subcommand ...");
    Serial.println("  set <group> <key> <value> [persist]");
    Serial.println("  set <full_key> <value> [persist]");
    Serial.println();
    printTopicGroup("Value formats");
    Serial.println("  bool: 1|0|true|false|on|off|yes|no");
    Serial.println("  int: decimal or 0x-prefixed");
    Serial.println("  i2c address: 0x01..0x7F");
    Serial.println();
    printTopicGroup("Execution model");
    Serial.println("  Status/read commands: immediate snapshot.");
    Serial.println("  Actions/settings: enqueue command, then apply in main loop.");
    Serial.println("  Use status/read after queueing to confirm results.");
    return;
  }

  if (strcmp(resolved, "flow") == 0) {
    printTopicHeader("Operator Flows");
    Serial.println("1) Bring-up (minimal)");
    Serial.println("  status");
    Serial.println("  devices");
    Serial.println("  read all");
    Serial.println();
    Serial.println("2) Sensor check (targeted)");
    Serial.println("  env status / env read");
    Serial.println("  lidar status / lidar read");
    Serial.println("  rtc status / rtc read");
    Serial.println();
    Serial.println("3) Bus / recovery path");
    Serial.println("  i2c status");
    Serial.println("  i2c scan");
    Serial.println("  i2c recover");
    Serial.println("  device <env|rtc> probe");
    Serial.println();
    Serial.println("4) Persisted config change");
    Serial.println("  <domain> <setting> <value> 1");
    Serial.println("  settings show <domain>");
    Serial.println("  status");
    Serial.println();
    Serial.println("5) Diagnostics");
    Serial.println("  diag all");
    Serial.println("  diag i2c / diag env / diag rtc");
    return;
  }

  if (strcmp(resolved, "aliases") == 0) {
    printTopicHeader("CLI Aliases");
    printTopicGroup("Top-level aliases");
    Serial.println("  doctor            -> diag");
    Serial.println("  tfluna ...        -> lidar ...");
    Serial.println("  e2 ...            -> lidar ...");
    Serial.println("  co2 ...           -> lidar ...");
    Serial.println("  guide <topic>     -> help <topic>");
    Serial.println("  i2c_scan [status] -> i2c scan / i2c scan status");
    Serial.println("  i2c_recover       -> i2c recover");
    Serial.println("  ls [path]         -> sd ls [path]");
    Serial.println();
    printTopicGroup("Device wrapper");
    Serial.println("  device <name> status|settings|read|diag|probe|recover");
    Serial.println("  device normalizes names: i2c->i2c_bus, e2/co2/tfluna->lidar");
    return;
  }

  if (strcmp(resolved, "status") == 0) {
    printTopicHeader("Status");
    Serial.println("  status");
    Serial.println("  Prints one-shot system summary (health, tick timing, sd, i2c, wifi).");
    printTopicMeta("Returns");
    Serial.println("  health, uptime, sample counters, tick metrics, SD and I2C summary.");
    printTopicMeta("Use when");
    Serial.println("  First command after boot, and after any recover/config change.");
    return;
  }

  if (strcmp(resolved, "devices") == 0) {
    printTopicHeader("Devices");
    Serial.println("  devices");
    Serial.println("  device <name> status");
    Serial.println("  Shows health/error summary from device status cache.");
    printTopicMeta("Returns");
    Serial.println("  Per-device state, online/offline signals, and last error details.");
    printTopicMeta("Use when");
    Serial.println("  Quick fault localization without reading raw measurements.");
    return;
  }

  if (strcmp(resolved, "read") == 0) {
    printTopicHeader("Read");
    Serial.println("  read [all|lidar|env|rtc]");
    Serial.println("  device <lidar|tfluna|co2|e2|env|rtc> read");
    Serial.println("  Reads latest cached values (does not force hardware transaction).");
    printTopicMeta("Returns");
    Serial.println("  Last sampled measurements and timestamps from runtime cache.");
    printTopicMeta("Use when");
    Serial.println("  Verifying the live data path without forcing sensor traffic.");
    return;
  }

  if (strcmp(resolved, "sample") == 0) {
    printTopicHeader("Sample");
    Serial.println("  sample [count]");
    Serial.printf("  count is clamped to <= %lu\n", static_cast<unsigned long>(CLI_MAX_SAMPLE_PRINT));
    return;
  }

  if (strcmp(resolved, "events") == 0) {
    printTopicHeader("Events");
    Serial.println("  events [count]");
    Serial.printf("  count is clamped to <= %lu\n", static_cast<unsigned long>(CLI_MAX_EVENT_PRINT));
    return;
  }

  if (strcmp(resolved, "factory_reset") == 0) {
    printTopicHeader("Factory Reset");
    Serial.println("  factory_reset [persist]");
    Serial.println("  Restores runtime defaults, queues APPLY_SETTINGS.");
    Serial.println("  persist=1 also writes defaults to NVS.");
    return;
  }

  if (strcmp(resolved, "device") == 0) {
    printTopicHeader("Device Wrapper");
    Serial.println("  device list");
    Serial.println("  device <name> status");
    Serial.println("  device <name> settings");
    Serial.println("  device <lidar|tfluna|co2|e2|env|rtc> read");
    Serial.println("  device <lidar|i2c|rtc|env> diag");
    Serial.println("  device <lidar|i2c|env|rtc|display> probe");
    Serial.println("  device <lidar|i2c|env|rtc> recover");
    Serial.println("  device sd remount");
    Serial.println("  device sd ls [path]");
    Serial.println("  device wifi <on|off> [persist]");
    printTopicMeta("Returns");
    Serial.println("  Delegates to domain command and prints same output contract.");
    printTopicMeta("Possibilities");
    Serial.println("  Unified automation interface: same verb set across device domains.");
    return;
  }

  if (strcmp(resolved, "rtc") == 0) {
    printTopicHeader("Real-Time Clock");
    printTopicGroup("Snapshot");
    Serial.println("  rtc status");
    Serial.println("  rtc read");
    Serial.println("  rtc diag");
    Serial.println("  rtc settings");
    Serial.println("  rtc poll");
    Serial.println("  rtc backup");
    Serial.println("  rtc eeprom_writes");
    Serial.println("  rtc eeprom_timeout");
    Serial.println("  rtc offline");
    Serial.println();
    printTopicGroup("Queue actions");
    Serial.println("  rtc probe");
    Serial.println("  rtc recover");
    Serial.println("  rtc set <YYYY MM DD hh mm ss>");
    Serial.println("  rtc set_unix <epoch_seconds>");
    Serial.println();
    printTopicGroup("Queue settings ([persist])");
    Serial.println("  rtc poll <ms> [persist]");
    Serial.println("  rtc backup <0|1|2> [persist]");
    Serial.println("  rtc eeprom_writes <on|off> [p]");
    Serial.println("  rtc eeprom_timeout <ms> [p]");
    Serial.println("  rtc offline <n> [persist]");
    Serial.println();
    printTopicMeta("Returns");
    Serial.println("  status/settings include state, online, health counters, and timing.");
    printTopicMeta("Possibilities");
    Serial.println("  Runtime clock sync, unix set, and persistence policy tuning.");
    return;
  }

  if (strcmp(resolved, "env") == 0) {
    printTopicHeader("Environment Sensor");
    printTopicGroup("Snapshot");
    Serial.println("  env status");
    Serial.println("  env read");
    Serial.println("  env settings");
    Serial.println("  env model");
    Serial.println("  env profile");
    Serial.println("  env poll");
    Serial.println("  env conversion_wait");
    Serial.println("  env address");
    Serial.println("  env bme");
    Serial.println("  env sht");
    Serial.println();
    printTopicGroup("Queue actions");
    Serial.println("  env probe");
    Serial.println();
    printTopicGroup("Queue settings ([persist])");
    Serial.println("  env model <bme280|sht3x> [persist]");
    Serial.println("  env profile <safe|balanced|fast> [p]");
    Serial.println("  env poll <ms> [p]       Poll interval");
    Serial.println("  env address <hex> [p]   I2C address");
    Serial.println("  env conversion_wait <ms> [p]");
    Serial.println();
    printTopicGroup("BME280-specific");
    Serial.println("  env bme mode <0|1|3> [p]");
    Serial.println("  env bme osrs <t> <p> <h> [p]  Oversampling (0-5)");
    Serial.println("  env bme filter <0..4> [p]");
    Serial.println("  env bme standby <0..7> [p]");
    Serial.println();
    printTopicGroup("SHT3x-specific");
    Serial.println("  env sht mode <0|1|2> [p]");
    Serial.println("  env sht repeatability <0..2> [p]");
    Serial.println("  env sht rate <0..4> [p]");
    Serial.println("  env sht stretch <0|1> [p]");
    Serial.println("  env sht low_vdd <on|off> [p]");
    Serial.println("  env sht command_delay <ms> [p]");
    Serial.println("  env sht not_ready_timeout <ms> [p]");
    Serial.println("  env sht fetch_margin <ms> [p]");
    Serial.println("  env sht allow_gc_reset <on|off> [p]");
    Serial.println("  env sht recover_bus_reset <on|off> [p]");
    Serial.println("  env sht recover_soft_reset <on|off> [p]");
    Serial.println("  env sht recover_hard_reset <on|off> [p]");
    Serial.println();
    printTopicGroup("Presets (same as model)");
    Serial.println("  preset env bme280 [p]   Apply BME280 defaults");
    Serial.println("  preset env sht3x  [p]   Apply SHT3x defaults");
    Serial.println();
    printTopicMeta("Returns");
    Serial.println("  read/status expose model, quality, and latest temp/humidity values.");
    printTopicMeta("Possibilities");
    Serial.println("  Switch BME/SHT model, tune profile, and deep sensor-specific settings.");
    return;
  }

  if (strcmp(resolved, "display") == 0) {
    printTopicHeader("I2C Display");
    printTopicGroup("Snapshot");
    Serial.println("  display status");
    Serial.println("  display settings");
    Serial.println("  display profile");
    Serial.println("  display poll");
    Serial.println("  display address");
    Serial.println();
    printTopicGroup("Queue actions");
    Serial.println("  display probe");
    Serial.println("  display recover");
    Serial.println();
    printTopicGroup("Queue settings ([persist])");
    Serial.println("  display profile <slow|normal|fast> [persist]");
    Serial.println("  display poll <ms> [persist]");
    Serial.println("  display address <hex> [persist]");
    Serial.println();
    printTopicMeta("Returns");
    Serial.println("  status/settings include health state, poll cadence, and profile.");
    printTopicMeta("Possibilities");
    Serial.println("  Fast probe/recover and profile tuning for UI responsiveness.");
    return;
  }

  if (strcmp(resolved, "i2c") == 0) {
    printTopicHeader("I2C Bus");
    printTopicGroup("Snapshot");
    Serial.println("  i2c status");
    Serial.println("  i2c settings");
    Serial.println("  i2c diag");
    Serial.println("  i2c scan status");
    Serial.println("  i2c probe status");
    Serial.println();
    printTopicGroup("Queue actions");
    Serial.println("  i2c recover");
    Serial.println("  i2c scan");
    Serial.println("  i2c probe <hex>");
    Serial.println();
    printTopicGroup("Queue settings ([persist])");
    Serial.println("  i2c freq <hz> [persist]");
    Serial.println("  i2c timeout <ms> [persist]");
    Serial.println("  i2c stuck_debounce <ms> [p]");
    Serial.println("  i2c max_failures <n> [p]");
    Serial.println("  i2c backoff <base> <max> [p]  Recovery backoff");
    Serial.println();
    printTopicGroup("Queue / task");
    Serial.println("  i2c requests <n> [p]    Request queue depth");
    Serial.println("  i2c results <n> [p]     Result queue depth");
    Serial.println("  i2c task_wait <ms> [p]  Task poll interval");
    Serial.println("  i2c heartbeat <ms> [p]");
    Serial.println("  i2c recover_timeout <ms> [p]");
    Serial.println();
    printTopicGroup("Diagnostics");
    Serial.println("  i2c slow_threshold <us> [p]");
    Serial.println("  i2c slow_degrade <n> [p]");
    Serial.println("  i2c slow_window <ms> [p]");
    Serial.println("  i2c health_window <ms> [p]");
    Serial.println("  i2c stale_multiplier <n> [p]");
    Serial.println();
    Serial.println("  settings show i2c       View current config");
    Serial.println("  preset i2c safe [p]     Apply safe defaults");
    Serial.println("  i2c_scan [status]       Alias of i2c scan");
    Serial.println("  i2c_recover             Alias of i2c recover");
    Serial.println();
    printTopicMeta("Returns");
    Serial.println("  status/diag expose queue health, timing, errors, and bus condition.");
    printTopicMeta("Possibilities");
    Serial.println("  Online address discovery, recovery, and transport stability tuning.");
    return;
  }

  if (strcmp(resolved, "lidar") == 0 || strcmp(resolved, "tfluna") == 0 ||
      strcmp(resolved, "co2") == 0 || strcmp(resolved, "e2") == 0) {
    printTopicHeader("TF-Luna UART");
    printTopicGroup("Snapshot");
    Serial.println("  lidar status");
    Serial.println("  lidar read");
    Serial.println("  lidar settings");
    Serial.println("  lidar pins");
    Serial.println("  lidar service");
    Serial.println("  lidar min_strength");
    Serial.println("  lidar max_distance");
    Serial.println("  lidar stale");
    Serial.println("  lidar serial");
    Serial.println();
    printTopicGroup("Queue actions");
    Serial.println("  lidar recover");
    Serial.println("  lidar probe");
    Serial.println();
    printTopicGroup("Runtime settings ([p] = persist:0|1)");
    Serial.println("  lidar service <ms> [p]        UART service cadence");
    Serial.println("  lidar min_strength <n> [p]    Minimum accepted strength");
    Serial.println("  lidar max_distance <cm> [p]   Maximum accepted distance");
    Serial.println("  lidar stale <ms> [p]          Stale-frame threshold");
    Serial.println("  lidar serial <ms> [p]         Periodic serial summary");
    Serial.println();
    printTopicGroup("Pin mapping");
    Serial.println("  TF-Luna TX -> ESP32 RX GPIO15");
    Serial.println("  TF-Luna RX -> ESP32 TX GPIO14");
    Serial.println("  aliases: tfluna/co2/e2 -> lidar");
    Serial.println();
    printTopicMeta("Returns");
    Serial.println("  read/status expose distance, strength, temperature, and UART health counters.");
    printTopicMeta("Possibilities");
    Serial.println("  Verify wiring, run one-shot probe, and tune live validity thresholds.");
    return;
  }

  if (strcmp(resolved, "sd") == 0 || strcmp(resolved, "log") == 0) {
    printTopicHeader("SD Card / Logger");
    printTopicGroup("Status");
    Serial.println("  sd status               Health and queue depth");
    Serial.println("  sd info                 Card type and usage");
    Serial.println("  sd ls [path]            Runtime-tracked SD paths");
    Serial.println("  sd settings             All SD settings");
    Serial.println();
    printTopicGroup("Actions");
    Serial.println("  sd remount              Remount SD card");
    Serial.println();
    printTopicGroup("Logging settings ([p] = persist:0|1)");
    Serial.println("  sd daily <on|off> [p]   Daily CSV log");
    Serial.println("  sd all <on|off> [p]     Single-file log");
    Serial.println("  sd all_max <bytes> [p]  Max single-file size");
    Serial.println("  sd flush <ms> [p]       Flush interval");
    Serial.println("  sd io_budget <ms> [p]   Per-tick I/O budget");
    Serial.println("  sd events_max <bytes> [p]  Events file max");
    Serial.println();
    printTopicGroup("Reliability");
    Serial.println("  sd mount_retry <ms> [p]");
    Serial.println("  sd write_retry <ms> [p]");
    Serial.println("  sd max_retries <n> [p]");
    Serial.println("  alias topic: help log");
    Serial.println();
    printTopicMeta("Returns");
    Serial.println("  status/info expose mount state, queues, retries, and capacity usage.");
    printTopicMeta("Possibilities");
    Serial.println("  Balance durability vs write load using flush/budget/retry controls.");
    return;
  }

  if (strcmp(resolved, "wifi") == 0) {
    printTopicHeader("WiFi Access Point");
    printTopicGroup("Status");
    Serial.println("  wifi status             AP state and clients");
    Serial.println("  wifi settings           All WiFi settings");
    Serial.println();
    printTopicGroup("Control ([p] = persist:0|1)");
    Serial.println("  wifi on [p]             Start AP");
    Serial.println("  wifi off [p]            Stop AP");
    Serial.println("  wifi ssid [<name> [p]]  Set/view SSID");
    Serial.println("  wifi secret [<pw> [p]]  Set/view password");
    Serial.println("  wifi auto_off [<ms> [p]] Auto-off timer (0=never)");
    Serial.println("  note: 'wifi secret' prints plaintext on serial console");
    Serial.println();
    printTopicMeta("Returns");
    Serial.println("  status reports AP mode, client count, and active security settings.");
    printTopicMeta("Possibilities");
    Serial.println("  Operator-safe commissioning flow with controlled AP uptime.");
    return;
  }

  if (strcmp(resolved, "web") == 0) {
    printTopicHeader("Web Server");
    printTopicGroup("Status");
    Serial.println("  web status              Server state");
    Serial.println("  web settings            All web settings");
    Serial.println();
    printTopicGroup("Settings ([p] = persist:0|1)");
    Serial.println("  web max_settings_body <bytes> [p]");
    Serial.println("  web max_rtc_body <bytes> [p]");
    Serial.println();
    printTopicMeta("Returns");
    Serial.println("  status/settings report server state and request-body limits.");
    printTopicMeta("Possibilities");
    Serial.println("  Harden request handling under constrained memory conditions.");
    return;
  }

  if (strcmp(resolved, "system") == 0) {
    printTopicHeader("System / Timing");
    printTopicGroup("Status");
    Serial.println("  version                 Firmware and linked library versions");
    Serial.println("  system status           Tick stats and uptime");
    Serial.println("  system settings         All system settings");
    Serial.println();
    printTopicGroup("Settings ([p] = persist:0|1)");
    Serial.println("  system sample_interval <sec> [p]");
    Serial.println("  system verbosity <off|normal|verbose> [p]");
    Serial.println("  system command_drain <n> [p]");
    Serial.println("  system command_window <ms> [p]");
    Serial.println("  system command_depth <n> [p]");
    Serial.println("  system tick_slow <us> [p]  Slow-tick threshold");
    Serial.println("  system ap_retry <ms> [p]   AP restart delay");
    Serial.println("  factory_reset [persist]    Restore runtime defaults");
    Serial.println();
    printTopicMeta("Returns");
    Serial.println("  status/settings expose command queue pressure and main-loop timing.");
    printTopicMeta("Possibilities");
    Serial.println("  Tune responsiveness, command throughput, and watchdog-safe behavior.");
    return;
  }

  if (strcmp(resolved, "leds") == 0) {
    printTopicHeader("Status LEDs (WS2812)");
    Serial.println("  leds status             Health and config");
    Serial.println("  leds settings           All LED settings");
    Serial.println("  leds health_init <ms> [p]      Init blink time");
    Serial.println("  leds health_debounce <ms> [p]  Debounce time");
    Serial.println();
    Serial.println("  [p] = persist:0|1");
    printTopicMeta("Returns");
    Serial.println("  status/settings expose backend state and health indication timing.");
    printTopicMeta("Possibilities");
    Serial.println("  Visual health signaling tuned for field readability.");
    return;
  }

  if (strcmp(resolved, "button") == 0) {
    printTopicHeader("Button");
    Serial.println("  button status           Press state");
    Serial.println("  button settings         Button settings");
    printTopicMeta("Returns");
    Serial.println("  Debounced state and configuration used by local interaction logic.");
    return;
  }

  if (strcmp(resolved, "settings") == 0 || strcmp(resolved, "set") == 0) {
    printTopicHeader("Settings");
    printTopicGroup("View");
    Serial.println("  settings show [section]   all|log|sd|lidar|i2c|env|rtc|");
    Serial.println("                            display|wifi|");
    Serial.println("                            system|leds|web");
    Serial.println("  set list [group]          List keys in group");
    Serial.println();
    printTopicGroup("Modify");
    Serial.println("  set <group> <key> <val> [p]   By group + key");
    Serial.println("  set <full_key> <val> [p]      By full key name");
    Serial.println();
    printTopicGroup("Examples");
    Serial.println("  set env address 0x76 1");
    Serial.println("  set lidar min_strength 80");
    Serial.println("  set rtc poll_ms 2000");
    Serial.println("  set i2c op_timeout_ms 30");
    Serial.println();
    Serial.println("  factory_reset [persist]   Restore defaults");
    printTopicMeta("Returns");
    Serial.println("  Validation result + queued apply indication; use settings show to verify.");
    return;
  }

  if (strcmp(resolved, "diag") == 0 || strcmp(resolved, "doctor") == 0) {
    printTopicHeader("Diagnostics");
    Serial.println("  diag all                Run all checks");
    Serial.println("  diag lidar              TF-Luna UART health");
    Serial.println("  diag i2c                I2C bus health");
    Serial.println("  diag rtc                RTC health");
    Serial.println("  diag env                ENV sensor health");
    Serial.println();
    Serial.println("  Suggests next-step commands based on current state.");
    Serial.println("  alias: doctor");
    printTopicMeta("Returns");
    Serial.println("  Pass/fail hints + targeted follow-up commands per subsystem.");
    return;
  }

  if (strcmp(resolved, "preset") == 0) {
    printTopicHeader("Presets");
    Serial.println("  preset env bme280 [p]   BME280 defaults");
    Serial.println("  preset env sht3x  [p]   SHT3x defaults");
    Serial.println("  preset rtc rv3032 [p]   RV-3032 defaults");
    Serial.println("  preset i2c safe   [p]   Conservative I2C");
    Serial.println();
    Serial.println("  [p] = persist:0|1  (presets modify runtime only)");
    printTopicMeta("Returns");
    Serial.println("  Queued config updates for the selected domain defaults.");
    return;
  }

  if (strcmp(resolved, "config") == 0) {
    printTopicHeader("Build Config");
    Serial.println("  config show [hardware|app|all]");
    Serial.println("  Prints compile-time hardware/app constants.");
    return;
  }

  Serial.printf("%sERR%s unknown help topic.\n", CLI_ANSI_ERR, CLI_ANSI_RESET);
  Serial.printf("%sHint:%s help topics\n", CLI_ANSI_INFO, CLI_ANSI_RESET);
}

void SerialCli::printStatus() {
  const uint8_t verbosity = currentVerbosity();
  const bool minimalOutput = verbosity == 0U;
  const bool verbose = verbosity >= 2U;
  SystemStatus sys{};
  Sample latest{};
  bool hasLatest = false;
  if (!_app.tryGetStatusSnapshot(sys, latest, hasLatest)) {
    Serial.println("ERR state busy");
    return;
  }

  char buf[32];
  Serial.printf("%s[System]%s\n", CLI_ANSI_WARN, CLI_ANSI_RESET);
  Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Health", CLI_ANSI_RESET, healthToStrColored(sys.health));
  Serial.printf("  %s%-28s%s %lu ms\n", CLI_ANSI_INFO, "Uptime", CLI_ANSI_RESET, static_cast<unsigned long>(sys.uptimeMs));
  formatUptimeHuman(sys.uptimeMs, buf, sizeof(buf));
  Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Uptime (human)", CLI_ANSI_RESET, buf);
  Serial.printf("  %s%-28s%s %s\n",
                CLI_ANSI_INFO,
                "CLI verbosity",
                CLI_ANSI_RESET,
                cliVerbosityToStr(verbosity));
  Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Time source", CLI_ANSI_RESET, sys.timeSource);
  Serial.printf("  %s%-28s%s %lu\n", CLI_ANSI_INFO, "Samples", CLI_ANSI_RESET, static_cast<unsigned long>(sys.sampleCount));
  Serial.printf("  %s%-28s%s %lu ms\n", CLI_ANSI_INFO, "Last sample", CLI_ANSI_RESET, static_cast<unsigned long>(sys.lastSampleMs));
  if (!minimalOutput) {
    Serial.println();
    Serial.printf("%s[Tick Timing]%s\n", CLI_ANSI_WARN, CLI_ANSI_RESET);
    Serial.printf("  %s%-28s%s %lu us\n", CLI_ANSI_INFO, "Last", CLI_ANSI_RESET, static_cast<unsigned long>(sys.tickLastDurationUs));
    Serial.printf("  %s%-28s%s %lu us\n", CLI_ANSI_INFO, "Mean", CLI_ANSI_RESET, static_cast<unsigned long>(sys.tickMeanDurationUs));
    Serial.printf("  %s%-28s%s %lu us\n", CLI_ANSI_INFO, "Max", CLI_ANSI_RESET, static_cast<unsigned long>(sys.tickMaxDurationUs));
    printCounterVal("Slow count", sys.tickSlowCount);
  }
  Serial.println();
  Serial.printf("%s[SD Card]%s\n", CLI_ANSI_WARN, CLI_ANSI_RESET);
  Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Mounted", CLI_ANSI_RESET, yesNo(sys.sdMounted));
  Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Daily OK", CLI_ANSI_RESET, okNo(sys.logDailyOk));
  Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "All OK", CLI_ANSI_RESET, okNo(sys.logAllOk));
  Serial.printf("  %s%-28s%s %lu / %lu\n",
                CLI_ANSI_INFO,
                "Queue depth",
                CLI_ANSI_RESET,
                static_cast<unsigned long>(sys.logQueueDepth),
                static_cast<unsigned long>(sys.logQueueCapacity));
  if (!minimalOutput) {
    Serial.printf("  %s%-28s%s %lu / %lu\n",
                  CLI_ANSI_INFO,
                  "Event queue depth",
                  CLI_ANSI_RESET,
                  static_cast<unsigned long>(sys.logEventQueueDepth),
                  static_cast<unsigned long>(sys.logEventQueueCapacity));
    Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Queue storage (sample)", CLI_ANSI_RESET,
                  sys.logQueueUsingPsram ? "PSRAM" : "Internal");
    Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Queue storage (event)", CLI_ANSI_RESET,
                  sys.logEventQueueUsingPsram ? "PSRAM" : "Internal");
  }
  printCounterVal("Dropped", sys.logDroppedCount);
  if (!minimalOutput) {
    Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Last error", CLI_ANSI_RESET, sys.logLastErrorMsg);
  }
  if (!minimalOutput && sys.sdInfoValid) {
    Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Filesystem", CLI_ANSI_RESET, sdFsTypeToStr(sys.sdFsType));
    Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Card type", CLI_ANSI_RESET, sdCardTypeToStr(sys.sdCardType));
    sprintBytesHuman(buf, sizeof(buf), sys.sdFsCapacityBytes);
    Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Capacity", CLI_ANSI_RESET, buf);
    sprintBytesHuman(buf, sizeof(buf), sys.sdFsUsedBytes);
    Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Used", CLI_ANSI_RESET, buf);
    sprintBytesHuman(buf, sizeof(buf), sys.sdFsFreeBytes);
    Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Free", CLI_ANSI_RESET, buf);
    if (sys.sdUsageValid && sys.sdFsCapacityBytes > 0U) {
      const uint64_t pct = (sys.sdFsUsedBytes * 100ULL) / sys.sdFsCapacityBytes;
      const uint32_t usagePct = static_cast<uint32_t>((pct > 100ULL) ? 100ULL : pct);
      Serial.printf("  %s%-28s%s %lu%%\n", CLI_ANSI_INFO, "Usage", CLI_ANSI_RESET, static_cast<unsigned long>(usagePct));
    }
  }
  Serial.println();
  Serial.printf("%s[I2C Bus]%s\n", CLI_ANSI_WARN, CLI_ANSI_RESET);
  printCounterVal("Errors", sys.i2cErrorCount);
  printCounterVal("Consecutive errors", sys.i2cConsecutiveErrors);
  printCounterVal("Recoveries", sys.i2cRecoveryCount);
  Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Backend", CLI_ANSI_RESET, sys.i2cBackendName);
  Serial.printf("  %s%-28s%s %lu\n", CLI_ANSI_INFO, "Request queue", CLI_ANSI_RESET, static_cast<unsigned long>(sys.i2cRequestQueueDepth));
  Serial.printf("  %s%-28s%s %lu\n", CLI_ANSI_INFO, "Result queue", CLI_ANSI_RESET, static_cast<unsigned long>(sys.i2cResultQueueDepth));
  if (!minimalOutput) {
    printCounterVal("Stuck SDA", sys.i2cStuckSdaCount);
    printCounterVal("Fast fail", sys.i2cStuckBusFastFailCount);
    printCounterVal("Request overflow", sys.i2cRequestOverflowCount);
    printCounterVal("Result dropped", sys.i2cResultDroppedCount);
    printCounterVal("Stale results", sys.i2cStaleResultCount);
  }
  if (verbose) {
    Serial.println();
    Serial.printf("%s[I2C Performance]%s\n", CLI_ANSI_WARN, CLI_ANSI_RESET);
    printCounterVal("Slow ops", sys.i2cSlowOpCount);
    printCounterVal("Recent slow ops", sys.i2cRecentSlowOpCount);
    Serial.printf("  %s%-28s%s %lu us\n", CLI_ANSI_INFO, "Max duration", CLI_ANSI_RESET, static_cast<unsigned long>(sys.i2cMaxDurationUs));
    Serial.printf("  %s%-28s%s %lu us\n", CLI_ANSI_INFO, "Rolling max duration", CLI_ANSI_RESET, static_cast<unsigned long>(sys.i2cRollingMaxDurationUs));
    Serial.printf("  %s%-28s%s %lu ms\n", CLI_ANSI_INFO, "Task alive age", CLI_ANSI_RESET, static_cast<unsigned long>(sys.i2cTaskAliveAgeMs));
    Serial.printf("  %s%-28s%s %u\n", CLI_ANSI_INFO, "Recovery stage", CLI_ANSI_RESET, static_cast<unsigned int>(sys.i2cLastRecoveryStage));
    Serial.println();
    Serial.printf("%s[I2C Power Cycle]%s\n", CLI_ANSI_WARN, CLI_ANSI_RESET);
    Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Configured", CLI_ANSI_RESET, yesNo(sys.i2cPowerCycleConfigured));
    printCounterVal("Attempts", sys.i2cPowerCycleAttempts);
    Serial.printf("  %s%-28s%s %lu ms\n", CLI_ANSI_INFO, "Last cycle", CLI_ANSI_RESET, static_cast<unsigned long>(sys.i2cLastPowerCycleMs));
    Serial.printf("  %s%-28s%s %u\n", CLI_ANSI_INFO, "Last code", CLI_ANSI_RESET, static_cast<unsigned int>(sys.i2cPowerCycleLastCode));
    Serial.printf("  %s%-28s%s %ld\n", CLI_ANSI_INFO, "Last detail", CLI_ANSI_RESET, static_cast<long>(sys.i2cPowerCycleLastDetail));
  }
  Serial.println();
  Serial.printf("%s[WiFi]%s\n", CLI_ANSI_WARN, CLI_ANSI_RESET);
  Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "AP running", CLI_ANSI_RESET, yesNo(sys.wifiApRunning));
  Serial.printf("  %s%-28s%s %u\n", CLI_ANSI_INFO, "Stations", CLI_ANSI_RESET, static_cast<unsigned int>(sys.wifiStationCount));
  Serial.printf("  %s%-28s%s %lu\n", CLI_ANSI_INFO, "Web clients", CLI_ANSI_RESET, static_cast<unsigned long>(sys.webClientCount));
  Serial.printf("  %s%-28s%s %lu\n", CLI_ANSI_INFO, "Command queue", CLI_ANSI_RESET, static_cast<unsigned long>(sys.commandQueueDepth));
  if (!minimalOutput) {
    printCounterVal("Command overflow", sys.commandQueueOverflowCount);
  }
  if (verbose) {
    Serial.println();
    Serial.printf("%s[History + Web Buffers]%s\n", CLI_ANSI_WARN, CLI_ANSI_RESET);
    Serial.printf("  %s%-28s%s %lu / %lu (%s)\n",
                  CLI_ANSI_INFO,
                  "Samples ring",
                  CLI_ANSI_RESET,
                  static_cast<unsigned long>(sys.sampleHistoryDepth),
                  static_cast<unsigned long>(sys.sampleHistoryCapacity),
                  sys.sampleHistoryUsingPsram ? "PSRAM" : "Internal");
    Serial.printf("  %s%-28s%s %lu / %lu (%s)\n",
                  CLI_ANSI_INFO,
                  "Events ring",
                  CLI_ANSI_RESET,
                  static_cast<unsigned long>(sys.eventHistoryDepth),
                  static_cast<unsigned long>(sys.eventHistoryCapacity),
                  sys.eventHistoryUsingPsram ? "PSRAM" : "Internal");
    Serial.printf("  %s%-28s%s %lu samples\n",
                  CLI_ANSI_INFO,
                  "Web graph scratch",
                  CLI_ANSI_RESET,
                  static_cast<unsigned long>(sys.webGraphScratchCapacity));
    Serial.printf("  %s%-28s%s %lu events\n",
                  CLI_ANSI_INFO,
                  "Web event scratch",
                  CLI_ANSI_RESET,
                  static_cast<unsigned long>(sys.webEventScratchCapacity));
    Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Web scratch storage", CLI_ANSI_RESET,
                  sys.webScratchUsingPsram ? "PSRAM" : "Internal");
    Serial.println();
    Serial.printf("%s[Memory]%s\n", CLI_ANSI_WARN, CLI_ANSI_RESET);
    sprintBytesHuman(buf, sizeof(buf), sys.heapFreeBytes);
    Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Heap free", CLI_ANSI_RESET, buf);
    sprintBytesHuman(buf, sizeof(buf), sys.heapMinFreeBytes);
    Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Heap min free", CLI_ANSI_RESET, buf);
    sprintBytesHuman(buf, sizeof(buf), sys.heapMaxAllocBytes);
    Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Heap max alloc", CLI_ANSI_RESET, buf);
    Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "PSRAM available", CLI_ANSI_RESET, yesNo(sys.psramAvailable));
    if (sys.psramAvailable) {
      sprintBytesHuman(buf, sizeof(buf), sys.psramTotalBytes);
      Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "PSRAM total", CLI_ANSI_RESET, buf);
      sprintBytesHuman(buf, sizeof(buf), sys.psramFreeBytes);
      Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "PSRAM free", CLI_ANSI_RESET, buf);
      sprintBytesHuman(buf, sizeof(buf), sys.psramMinFreeBytes);
      Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "PSRAM min free", CLI_ANSI_RESET, buf);
      sprintBytesHuman(buf, sizeof(buf), sys.psramMaxAllocBytes);
      Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "PSRAM max alloc", CLI_ANSI_RESET, buf);
    }
  }
  Serial.println();
  if (hasLatest) {
    Serial.printf("%s[Latest Sample]%s\n", CLI_ANSI_WARN, CLI_ANSI_RESET);
    Serial.printf("  %s%-28s%s %lu\n", CLI_ANSI_INFO, "Timestamp (unix)", CLI_ANSI_RESET, static_cast<unsigned long>(latest.tsUnix));
    Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Local time", CLI_ANSI_RESET, latest.tsLocal);
    Serial.printf("  %s%-28s%s %u cm\n", CLI_ANSI_INFO, "Distance", CLI_ANSI_RESET, static_cast<unsigned int>(latest.distanceCm));
    Serial.printf("  %s%-28s%s %u\n", CLI_ANSI_INFO, "Strength", CLI_ANSI_RESET, static_cast<unsigned int>(latest.strength));
    Serial.printf("  %s%-28s%s %.2f C\n", CLI_ANSI_INFO, "LiDAR temp", CLI_ANSI_RESET, static_cast<double>(latest.lidarTempC));
    Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Valid frame", CLI_ANSI_RESET, yesNo(latest.validFrame));
    Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Signal OK", CLI_ANSI_RESET, yesNo(latest.signalOk));
    if (!minimalOutput) {
      Serial.printf("  %s%-28s%s %.2f C\n", CLI_ANSI_INFO, "Temperature", CLI_ANSI_RESET, static_cast<double>(latest.tempC));
      Serial.printf("  %s%-28s%s %.2f %%\n", CLI_ANSI_INFO, "Humidity", CLI_ANSI_RESET, static_cast<double>(latest.rhPct));
      Serial.printf("  %s%-28s%s %.2f hPa\n", CLI_ANSI_INFO, "Pressure", CLI_ANSI_RESET, static_cast<double>(latest.pressureHpa));
    }
    if (verbose) {
      Serial.printf("  %s%-28s%s 0x%02X\n", CLI_ANSI_INFO, "Valid mask", CLI_ANSI_RESET, static_cast<unsigned int>(latest.validMask));
    }
  } else {
    Serial.printf("%s[Latest Sample]%s\n", CLI_ANSI_WARN, CLI_ANSI_RESET);
    Serial.printf("  %s%-28s%s none\n", CLI_ANSI_INFO, "Data", CLI_ANSI_RESET);
  }
}

uint8_t SerialCli::currentVerbosity() const {
  RuntimeSettings settings{};
  if (!_app.tryGetSettingsSnapshot(settings)) {
    return 1U;
  }
  if (settings.cliVerbosity < RuntimeSettings::MIN_CLI_VERBOSITY ||
      settings.cliVerbosity > RuntimeSettings::MAX_CLI_VERBOSITY) {
    return 1U;
  }
  return settings.cliVerbosity;
}

bool SerialCli::loadDeviceStatuses(size_t& outCount) {
  outCount = 0;
  return _app.tryCopyDeviceStatuses(_deviceScratch, DEVICE_COUNT, outCount);
}

const DeviceStatus* SerialCli::findDeviceInScratch(const char* name, size_t count) const {
  if (name == nullptr || name[0] == '\0') {
    return nullptr;
  }
  for (size_t i = 0; i < count; ++i) {
    if (deviceNameEquals(_deviceScratch[i].name, name)) {
      return &_deviceScratch[i];
    }
  }
  return nullptr;
}

void SerialCli::printDevices() {
  const uint8_t verbosity = currentVerbosity();
  const bool verbose = verbosity >= 2U;
  size_t count = 0;
  if (!loadDeviceStatuses(count)) {
    Serial.println("ERR state busy");
    return;
  }
  for (size_t i = 0; i < count; ++i) {
    const DeviceStatus& st = _deviceScratch[i];
    if (verbose) {
      Serial.printf("[%lu] %-8s health=%-8s code=%s detail=%ld last_ok=%lu last_err=%lu last_act=%lu msg='%s'\n",
                    static_cast<unsigned long>(i),
                    st.name,
                    healthToStrColored(st.health),
                    errToStrColored(st.lastStatus.code),
                    static_cast<long>(st.lastStatus.detail),
                    static_cast<unsigned long>(st.lastOkMs),
                    static_cast<unsigned long>(st.lastErrorMs),
                    static_cast<unsigned long>(st.lastActivityMs),
                    st.lastStatus.msg);
    } else {
      Serial.printf("[%lu] %-8s health=%-8s code=%s msg='%s'\n",
                    static_cast<unsigned long>(i),
                    st.name,
                    healthToStrColored(st.health),
                    errToStrColored(st.lastStatus.code),
                    st.lastStatus.msg);
    }
  }
}

void SerialCli::printDevice(const char* name) {
  if (name == nullptr || name[0] == '\0') {
    Serial.println("ERR device name missing");
    return;
  }

  const uint8_t verbosity = currentVerbosity();
  const bool minimalOutput = verbosity == 0U;

  size_t count = 0;
  if (!loadDeviceStatuses(count)) {
    Serial.println("ERR state busy");
    return;
  }

  const DeviceStatus* found = findDeviceInScratch(name, count);
  if (found == nullptr) {
    Serial.printf("ERR unknown device: %s\n", name);
    return;
  }

  auto printSection = [](const char* title) {
    Serial.printf("%s[%s]%s\n", CLI_ANSI_WARN, title, CLI_ANSI_RESET);
  };
  auto printVal = [](const char* label, const char* value) {
    Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, label, CLI_ANSI_RESET, value);
  };
  auto printBoolVal = [&](const char* label, bool value) {
    printVal(label, yesNo(value));
  };
  auto printCounterU32 = [](const char* label, uint32_t value) {
    printCounterVal(label, value);
  };
  auto printU32Val = [](const char* label, uint32_t value, const char* unit = "") {
    if (unit[0] == '\0') {
      Serial.printf("  %s%-28s%s %lu\n", CLI_ANSI_INFO, label, CLI_ANSI_RESET,
                    static_cast<unsigned long>(value));
    } else {
      Serial.printf("  %s%-28s%s %lu %s\n", CLI_ANSI_INFO, label, CLI_ANSI_RESET,
                    static_cast<unsigned long>(value), unit);
    }
  };
  auto printI32Val = [](const char* label, int32_t value, const char* unit = "") {
    if (unit[0] == '\0') {
      Serial.printf("  %s%-28s%s %ld\n", CLI_ANSI_INFO, label, CLI_ANSI_RESET,
                    static_cast<long>(value));
    } else {
      Serial.printf("  %s%-28s%s %ld %s\n", CLI_ANSI_INFO, label, CLI_ANSI_RESET,
                    static_cast<long>(value), unit);
    }
  };
  auto printFloatVal = [](const char* label, float value, const char* unit = "") {
    if (unit[0] == '\0') {
      Serial.printf("  %s%-28s%s %.2f\n", CLI_ANSI_INFO, label, CLI_ANSI_RESET,
                    static_cast<double>(value));
    } else {
      Serial.printf("  %s%-28s%s %.2f %s\n", CLI_ANSI_INFO, label, CLI_ANSI_RESET,
                    static_cast<double>(value), unit);
    }
  };
  auto printHex8Val = [](const char* label, uint8_t value) {
    Serial.printf("  %s%-28s%s 0x%02X\n", CLI_ANSI_INFO, label, CLI_ANSI_RESET,
                  static_cast<unsigned int>(value));
  };
  auto printBytesVal = [](const char* label, uint64_t bytes) {
    char buf[32];
    sprintBytesHuman(buf, sizeof(buf), bytes);
    Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, label, CLI_ANSI_RESET, buf);
  };

  printSection(found->name);
  printVal("Health", healthToStrColored(found->health));
  printVal("Status code", errToStrColored(found->lastStatus.code));
  printI32Val("Detail", static_cast<int32_t>(found->lastStatus.detail));
  printVal("Message", found->lastStatus.msg);
  printU32Val("Last OK", found->lastOkMs, "ms");
  printU32Val("Last error", found->lastErrorMs, "ms");
  printU32Val("Last activity", found->lastActivityMs, "ms");

  SystemStatus sys{};
  Sample latest{};
  bool hasLatest = false;
  const bool haveSys = _app.tryGetStatusSnapshot(sys, latest, hasLatest);
  RuntimeSettings settings{};
  const bool haveSettings = _app.tryGetSettingsSnapshot(settings);
  const HardwareSettings& hw = _app.getConfig();

  if (strcmp(found->name, "i2c_bus") == 0 && haveSys) {
    Serial.println();
    printSection("I2C Bus Details");
    printVal("Backend", sys.i2cBackendName);
    printBoolVal("Deterministic timeout", sys.i2cDeterministicTimeout);
    printCounterU32("Total errors", sys.i2cErrorCount);
    printCounterU32("Consecutive errors", sys.i2cConsecutiveErrors);
    printCounterU32("Recoveries", sys.i2cRecoveryCount);
    printCounterU32("Stuck SDA count", sys.i2cStuckSdaCount);
    printCounterU32("Fast-fail count", sys.i2cStuckBusFastFailCount);
    printCounterU32("Stale results", sys.i2cStaleResultCount);
  }

  if (strcmp(found->name, "rtc") == 0) {
    if (!minimalOutput && haveSettings) {
      Serial.println();
      printSection("RTC Configuration");
      printHex8Val("I2C address", settings.i2cRtcAddress);
      printU32Val("Poll interval", settings.i2cRtcPollMs, "ms");
      printU32Val("Backup mode", settings.i2cRtcBackupMode);
      printBoolVal("EEPROM writes", settings.i2cRtcEnableEepromWrites);
      printU32Val("EEPROM timeout", settings.i2cRtcEepromTimeoutMs, "ms");
      printU32Val("Offline threshold", settings.i2cRtcOfflineThreshold);
    }
    if (hasLatest) {
      const bool validTs = (latest.tsUnix != 0U) && (latest.tsLocal[0] != '\0');
      Serial.println();
      printSection("RTC Latest");
      printBoolVal("Valid", validTs);
      printU32Val("Unix timestamp", latest.tsUnix);
      printVal("Local time", latest.tsLocal);
    }
  }

  if (strcmp(found->name, "env") == 0) {
    if (!minimalOutput && haveSettings) {
      Serial.println();
      printSection("ENV Configuration");
      printHex8Val("I2C address", settings.i2cEnvAddress);
      printVal("Model", envModelHint(settings.i2cEnvAddress));
      printU32Val("Poll interval", settings.i2cEnvPollMs, "ms");
      printU32Val("Conversion wait", settings.i2cEnvConversionWaitMs, "ms");
      Serial.println();
      printSection("BME280");
      printU32Val("Mode", settings.i2cEnvBmeMode);
      printU32Val("Oversampling T", settings.i2cEnvBmeOsrsT);
      printU32Val("Oversampling P", settings.i2cEnvBmeOsrsP);
      printU32Val("Oversampling H", settings.i2cEnvBmeOsrsH);
      printU32Val("Filter", settings.i2cEnvBmeFilter);
      printU32Val("Standby", settings.i2cEnvBmeStandby);
      Serial.println();
      printSection("SHT3x");
      printU32Val("Mode", settings.i2cEnvShtMode);
      printU32Val("Repeatability", settings.i2cEnvShtRepeatability);
      printU32Val("Periodic rate", settings.i2cEnvShtPeriodicRate);
      printBoolVal("Clock stretching", settings.i2cEnvShtClockStretching != 0U);
      printBoolVal("Low VDD", settings.i2cEnvShtLowVdd);
    }
    if (hasLatest) {
      const bool valid = (latest.validMask & (VALID_TEMP | VALID_RH)) != 0U;
      Serial.println();
      printSection("ENV Latest");
      printBoolVal("Valid", valid);
      printFloatVal("Temperature", latest.tempC, "C");
      printFloatVal("Humidity", latest.rhPct, "%");
      printFloatVal("Pressure", latest.pressureHpa, "hPa");
    }
  }

  if (strcmp(found->name, "sd") == 0 && haveSys) {
    Serial.println();
    printSection("SD Logger State");
    printBoolVal("Mounted", sys.sdMounted);
    printBoolVal("Daily log OK", sys.logDailyOk);
    printBoolVal("Single-file log OK", sys.logAllOk);
    printU32Val("Write queue depth", sys.logQueueDepth);
    printU32Val("Event queue depth", sys.logEventQueueDepth);
    printCounterU32("Dropped writes", sys.logDroppedCount);
    printCounterU32("Dropped events", sys.logEventDroppedCount);
    printU32Val("Last write age", sys.logLastWriteAgeMs, "ms");
    if (!minimalOutput && sys.sdInfoValid) {
      uint32_t usagePct = 0U;
      if (sys.sdUsageValid && sys.sdFsCapacityBytes > 0U) {
        const uint64_t pct = (sys.sdFsUsedBytes * 100ULL) / sys.sdFsCapacityBytes;
        usagePct = static_cast<uint32_t>((pct > 100ULL) ? 100ULL : pct);
      }
      Serial.println();
      printSection("SD Card Info");
      printVal("Filesystem", sdFsTypeToStr(sys.sdFsType));
      printVal("Card type", sdCardTypeToStr(sys.sdCardType));
      printBytesVal("Capacity", sys.sdFsCapacityBytes);
      printBytesVal("Used", sys.sdFsUsedBytes);
      printBytesVal("Free", sys.sdFsFreeBytes);
      char pctBuf[16];
      snprintf(pctBuf, sizeof(pctBuf), "%lu%%", static_cast<unsigned long>(usagePct));
      printVal("Usage", pctBuf);
      printU32Val("Info age", sys.sdInfoAgeMs, "ms");
    } else {
      printVal("Card info", "unavailable");
    }
  }

  if (strcmp(found->name, "lidar") == 0) {
    if (haveSettings) {
      Serial.println();
      printSection("TF-Luna Configuration");
      printU32Val("UART service", settings.lidarServiceMs, "ms");
      printU32Val("Minimum strength", settings.lidarMinStrength);
      printU32Val("Maximum distance", settings.lidarMaxDistanceCm, "cm");
      printU32Val("Frame stale", settings.lidarFrameStaleMs, "ms");
      printU32Val("Serial summary", settings.serialPrintIntervalMs, "ms");
      {
        char verbosityBuf[24];
        snprintf(verbosityBuf,
                 sizeof(verbosityBuf),
                 "%s",
                 cliVerbosityToStr(settings.cliVerbosity));
        printVal("CLI verbosity", verbosityBuf);
      }
      Serial.println();
      printSection("TF-Luna Wiring");
      printI32Val("ESP32 RX pin", hw.lidarRx);
      printI32Val("ESP32 TX pin", hw.lidarTx);
      printU32Val("UART index", hw.lidarUartIndex);
      printVal("Mapping", "sensor TX -> ESP RX, sensor RX -> ESP TX");
      if (!minimalOutput && haveSys) {
        Serial.println();
        printSection("TF-Luna Stats");
        printU32Val("Frames parsed", sys.lidarFramesParsed);
        printU32Val("Checksum errors", sys.lidarChecksumErrors);
        printU32Val("Sync loss", sys.lidarSyncLossCount);
        printU32Val("Frame age", sys.lidarFrameAgeMs, "ms");
      }
    }
    if (hasLatest) {
      const bool valid = latest.validFrame;
      Serial.println();
      printSection("TF-Luna Latest");
      printBoolVal("Valid frame", valid);
      printBoolVal("Signal OK", latest.signalOk);
      printU32Val("Distance", latest.distanceCm, "cm");
      printU32Val("Strength", latest.strength);
      printFloatVal("Temperature", latest.lidarTempC, "C");
      if (haveSys && sys.lidarStats.hasDistanceStats) {
        printFloatVal("Minimum", sys.lidarStats.minDistanceCm, "cm");
        printFloatVal("Maximum", sys.lidarStats.maxDistanceCm, "cm");
        printFloatVal("Mean", sys.lidarStats.meanDistanceCm, "cm");
        printFloatVal("Stddev", sys.lidarStats.stddevDistanceCm, "cm");
      }
    }
  }

  if (strcmp(found->name, "wifi") == 0 && haveSys) {
    Serial.println();
    printSection("WiFi State");
    printBoolVal("AP running", sys.wifiApRunning);
    printU32Val("Connected stations", sys.wifiStationCount);
    printU32Val("Web clients", sys.webClientCount);
    if (!minimalOutput && haveSettings) {
      Serial.println();
      printSection("WiFi Configuration");
      printBoolVal("Enabled", settings.wifiEnabled);
      printVal("SSID", settings.apSsid);
      const size_t apPassLen = strnlen(settings.apPass, sizeof(settings.apPass));
      printBoolVal("Secret set", apPassLen > 0U);
      printU32Val("Secret length", static_cast<uint32_t>(apPassLen));
      printU32Val("Auto-off timeout", settings.apAutoOffMs, "ms");
    }
  }

  if (strcmp(found->name, "web") == 0 && haveSys) {
    Serial.println();
    printSection("Web Server State");
    printU32Val("Active clients", sys.webClientCount);
    printU32Val("Command queue depth", sys.commandQueueDepth);
    if (!minimalOutput) {
      printCounterU32("Command overflows", sys.commandQueueOverflowCount);
      printU32Val("Last overflow", sys.commandQueueLastOverflowMs, "ms");
    }
    if (!minimalOutput && haveSettings) {
      Serial.println();
      printSection("Web Configuration");
      printU32Val("Max settings body", settings.webMaxSettingsBodyBytes, "B");
      printU32Val("Max RTC body", settings.webMaxRtcBodyBytes, "B");
    }
  }

  if (strcmp(found->name, "leds") == 0) {
    Serial.println();
    printSection("LED Hardware");
    printI32Val("Pin", hw.ledPin);
    printU32Val("Count", hw.ledCount);
    printU32Val("WiFi LED index", hw.wifiLedIndex);
    printU32Val("Health LED index", hw.healthLedIndex);
    printU32Val("Brightness", hw.ledBrightness);
    printU32Val("Smooth step", hw.ledSmoothStepMs, "ms");
    if (!minimalOutput && haveSettings) {
      Serial.println();
      printSection("LED Configuration");
      printU32Val("Health init grace", settings.ledHealthInitMs, "ms");
      printU32Val("Health debounce", settings.ledHealthDebounceMs, "ms");
    }
  }

  if (strcmp(found->name, "button") == 0) {
    Serial.println();
    printSection("Button Hardware");
    printI32Val("Pin", hw.buttonPin);
    printBoolVal("Active low", hw.buttonActiveLow);
    printU32Val("Debounce", hw.buttonDebounceMs, "ms");
    printU32Val("Long press", hw.buttonLongPressMs, "ms");
    printU32Val("Multi-press window", hw.buttonMultiPressWindowMs, "ms");
    printU32Val("Multi-press count", hw.buttonMultiPressCount);
  }

}

void SerialCli::printSamples(size_t count) {
  Sample samples[CLI_MAX_SAMPLE_PRINT] = {};
  size_t got = 0;
  if (!_app.tryCopySamples(samples, count, false, got)) {
    Serial.println("ERR state busy");
    return;
  }
  Serial.printf("%s[Samples]%s  count=%lu\n", CLI_ANSI_WARN, CLI_ANSI_RESET, static_cast<unsigned long>(got));
  for (size_t i = 0; i < got; ++i) {
    const Sample& s = samples[i];
    Serial.printf("  %s%-4lu%s ts=%lu local='%s' dist=%ucm strength=%u temp_lidar=%.2f frame=%u signal=%u env_t=%.2f rh=%.2f p=%.2f mask=0x%02X\n",
                  CLI_ANSI_INFO, static_cast<unsigned long>(i), CLI_ANSI_RESET,
                  static_cast<unsigned long>(s.tsUnix),
                  s.tsLocal,
                  static_cast<unsigned int>(s.distanceCm),
                  static_cast<unsigned int>(s.strength),
                  static_cast<double>(s.lidarTempC),
                  s.validFrame ? 1U : 0U,
                  s.signalOk ? 1U : 0U,
                  static_cast<double>(s.tempC),
                  static_cast<double>(s.rhPct),
                  static_cast<double>(s.pressureHpa),
                  static_cast<unsigned int>(s.validMask));
  }
}

void SerialCli::printEvents(size_t count) {
  Event events[CLI_MAX_EVENT_PRINT] = {};
  size_t got = 0;
  if (!_app.tryCopyEvents(events, count, false, got)) {
    Serial.println("ERR state busy");
    return;
  }
  Serial.printf("%s[Events]%s  count=%lu\n", CLI_ANSI_WARN, CLI_ANSI_RESET, static_cast<unsigned long>(got));
  for (size_t i = 0; i < got; ++i) {
    const Event& e = events[i];
    Serial.printf("  %s%-4lu%s ts=%lu  local='%s'  code=%u  msg='%s'\n",
                  CLI_ANSI_INFO, static_cast<unsigned long>(i), CLI_ANSI_RESET,
                  static_cast<unsigned long>(e.tsUnix),
                  e.tsLocal,
                  static_cast<unsigned int>(e.code),
                  e.msg);
  }
}

void SerialCli::printRead(const char* which) {
  const uint8_t verbosity = currentVerbosity();
  const bool minimalOutput = verbosity == 0U;
  const bool verbose = verbosity >= 2U;
  Sample latest{};
  if (!_app.getLatestSample(latest)) {
    Serial.println("ERR no sample");
    return;
  }
  const char* normalized = normalizeDeviceName(which);
  if (normalized == nullptr || strcmp(normalized, "all") == 0) {
    Serial.printf("%s[Latest Reading]%s\n", CLI_ANSI_WARN, CLI_ANSI_RESET);
    Serial.printf("  %s%-28s%s %lu\n", CLI_ANSI_INFO, "Timestamp (unix)", CLI_ANSI_RESET, static_cast<unsigned long>(latest.tsUnix));
    Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Local time", CLI_ANSI_RESET, latest.tsLocal);
    Serial.printf("  %s%-28s%s %u cm\n", CLI_ANSI_INFO, "Distance", CLI_ANSI_RESET, static_cast<unsigned int>(latest.distanceCm));
    Serial.printf("  %s%-28s%s %u\n", CLI_ANSI_INFO, "Strength", CLI_ANSI_RESET, static_cast<unsigned int>(latest.strength));
    Serial.printf("  %s%-28s%s %.2f C\n", CLI_ANSI_INFO, "LiDAR temp", CLI_ANSI_RESET, static_cast<double>(latest.lidarTempC));
    Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Valid frame", CLI_ANSI_RESET, yesNo(latest.validFrame));
    Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Signal OK", CLI_ANSI_RESET, yesNo(latest.signalOk));
    if (!minimalOutput) {
      Serial.printf("  %s%-28s%s %.2f C\n", CLI_ANSI_INFO, "Temperature", CLI_ANSI_RESET, static_cast<double>(latest.tempC));
      Serial.printf("  %s%-28s%s %.2f %%\n", CLI_ANSI_INFO, "Humidity", CLI_ANSI_RESET, static_cast<double>(latest.rhPct));
      Serial.printf("  %s%-28s%s %.2f hPa\n", CLI_ANSI_INFO, "Pressure", CLI_ANSI_RESET, static_cast<double>(latest.pressureHpa));
    }
    if (verbose) {
      Serial.printf("  %s%-28s%s 0x%02X\n", CLI_ANSI_INFO, "Valid mask", CLI_ANSI_RESET, static_cast<unsigned int>(latest.validMask));
    }
    return;
  }
  if (strcmp(normalized, "lidar") == 0) {
    Serial.printf("%s[TF-Luna Reading]%s\n", CLI_ANSI_WARN, CLI_ANSI_RESET);
    Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Valid frame", CLI_ANSI_RESET, yesNo(latest.validFrame));
    Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Signal OK", CLI_ANSI_RESET, yesNo(latest.signalOk));
    Serial.printf("  %s%-28s%s %u cm\n", CLI_ANSI_INFO, "Distance", CLI_ANSI_RESET, static_cast<unsigned int>(latest.distanceCm));
    Serial.printf("  %s%-28s%s %u\n", CLI_ANSI_INFO, "Strength", CLI_ANSI_RESET, static_cast<unsigned int>(latest.strength));
    Serial.printf("  %s%-28s%s %.2f C\n", CLI_ANSI_INFO, "Temperature", CLI_ANSI_RESET, static_cast<double>(latest.lidarTempC));
    return;
  }
  if (strcmp(normalized, "env") == 0) {
    const bool tempValid = (latest.validMask & VALID_TEMP) != 0U;
    const bool rhValid = (latest.validMask & VALID_RH) != 0U;
    const bool pValid = (latest.validMask & VALID_PRESSURE) != 0U;
    Serial.printf("%s[ENV Reading]%s\n", CLI_ANSI_WARN, CLI_ANSI_RESET);
    Serial.printf("  %s%-28s%s %s  %.2f C\n", CLI_ANSI_INFO, "Temperature", CLI_ANSI_RESET, validInvalid(tempValid), static_cast<double>(latest.tempC));
    Serial.printf("  %s%-28s%s %s  %.2f %%\n", CLI_ANSI_INFO, "Humidity", CLI_ANSI_RESET, validInvalid(rhValid), static_cast<double>(latest.rhPct));
    Serial.printf("  %s%-28s%s %s  %.2f hPa\n", CLI_ANSI_INFO, "Pressure", CLI_ANSI_RESET, validInvalid(pValid), static_cast<double>(latest.pressureHpa));
    return;
  }
  if (strcmp(normalized, "rtc") == 0) {
    Serial.printf("%s[RTC Reading]%s\n", CLI_ANSI_WARN, CLI_ANSI_RESET);
    Serial.printf("  %s%-28s%s %lu\n", CLI_ANSI_INFO, "Timestamp (unix)", CLI_ANSI_RESET, static_cast<unsigned long>(latest.tsUnix));
    Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Local time", CLI_ANSI_RESET, latest.tsLocal);
    return;
  }
  Serial.println("ERR read target must be all|lidar|env|rtc");
}

void SerialCli::printSettings(const char* section) {
  const char* resolved = section;
  if (resolved == nullptr || resolved[0] == '\0') {
    resolved = "all";
  }
  if (strcmp(resolved, "all") != 0 && !isSettingGroup(resolved)) {
    Serial.printf("%sERR%s section must be all|log|sd|lidar|tfluna|i2c|env|rtc|display|wifi|system|leds|web\n",
                  CLI_ANSI_ERR,
                  CLI_ANSI_RESET);
    printHint("help settings");
    return;
  }

  RuntimeSettings s{};
  if (!_app.tryGetSettingsSnapshot(s)) {
    Serial.printf("%sERR%s state busy\n", CLI_ANSI_ERR, CLI_ANSI_RESET);
    return;
  }

  const bool showAll = strcmp(resolved, "all") == 0;
  const bool showSystem = showAll || strcmp(resolved, "system") == 0 || strcmp(resolved, "leds") == 0;
  const bool showLog = showAll || strcmp(resolved, "log") == 0 || strcmp(resolved, "sd") == 0;
  const bool showLidar = showAll || strcmp(resolved, "lidar") == 0 || strcmp(resolved, "tfluna") == 0 ||
                         strcmp(resolved, "co2") == 0 || strcmp(resolved, "e2") == 0;
  const bool showI2c = showAll || strcmp(resolved, "i2c") == 0;
  const bool showEnv = showI2c || strcmp(resolved, "env") == 0;
  const bool showRtc = showI2c || strcmp(resolved, "rtc") == 0;
  const bool showDisplay = showI2c || strcmp(resolved, "display") == 0;
  const bool showWifi = showAll || strcmp(resolved, "wifi") == 0;
  const bool showWeb = showAll || strcmp(resolved, "web") == 0;

  const size_t apPassLen = strnlen(s.apPass, sizeof(s.apPass));
  auto bmeModeToStr = [](uint8_t mode) -> const char* {
    switch (mode) {
      case 0:
        return "sleep";
      case 1:
        return "forced";
      case 3:
        return "normal";
      default:
        return "unknown";
    }
  };

  auto printValue = [](const char* name, const char* value) {
    Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, name, CLI_ANSI_RESET, value);
  };
  auto printBool = [&](const char* name, bool value) {
    printValue(name, value ? "on" : "off");
  };
  auto printU32 = [&](const char* name, uint32_t value, const char* unit = "") {
    if (unit[0] == '\0') {
      Serial.printf("  %s%-28s%s %lu\n",
                    CLI_ANSI_INFO,
                    name,
                    CLI_ANSI_RESET,
                    static_cast<unsigned long>(value));
    } else {
      Serial.printf("  %s%-28s%s %lu %s\n",
                    CLI_ANSI_INFO,
                    name,
                    CLI_ANSI_RESET,
                    static_cast<unsigned long>(value),
                    unit);
    }
  };
  auto printHex8 = [&](const char* name, uint8_t value) {
    Serial.printf("  %s%-28s%s 0x%02X\n",
                  CLI_ANSI_INFO,
                  name,
                  CLI_ANSI_RESET,
                  static_cast<unsigned int>(value));
  };

  bool firstSection = true;
  auto beginSection = [&](const char* name) {
    if (!firstSection) {
      Serial.println();
    }
    firstSection = false;
    Serial.printf("%s[%s]%s\n", CLI_ANSI_WARN, name, CLI_ANSI_RESET);
  };

  Serial.printf("%sSettings%s topic=%s%s%s\n",
                CLI_ANSI_BOLD,
                CLI_ANSI_RESET,
                CLI_ANSI_INFO,
                resolved,
                CLI_ANSI_RESET);

  if (showSystem) {
    beginSection("System");
    printU32("Sample interval", s.sampleIntervalMs, "ms");
    Serial.printf("  %s%-28s%s %s\n",
                  CLI_ANSI_INFO,
                  "CLI verbosity",
                  CLI_ANSI_RESET,
                  cliVerbosityToStr(s.cliVerbosity));
    printU32("Command drain per tick", s.commandDrainPerTick);
    printU32("Queue degraded window", s.commandQueueDegradedWindowMs, "ms");
    printU32("Queue depth threshold", s.commandQueueDegradedDepthThreshold);
    printU32("Slow tick threshold", s.mainTickSlowThresholdUs, "us");
    printU32("LED health init", s.ledHealthInitMs, "ms");
    printU32("LED health debounce", s.ledHealthDebounceMs, "ms");
    printU32("AP retry backoff", s.apStartRetryBackoffMs, "ms");
  }

  if (showLog) {
    beginSection("SD Logger");
    printBool("Daily log", s.logDailyEnabled);
    printBool("Single-file log", s.logAllEnabled);
    printU32("Single-file max size", s.logAllMaxBytes, "B");
    printU32("Flush interval", s.logFlushMs, "ms");
    printU32("I/O budget", s.logIoBudgetMs, "ms");
    printU32("Mount retry backoff", s.logMountRetryMs, "ms");
    printU32("Write retry backoff", s.logWriteRetryBackoffMs, "ms");
    printU32("Max write retries", s.logMaxWriteRetries);
    Serial.printf("  %s%-28s%s %s\n",
                  CLI_ANSI_INFO,
                  "Session name",
                  CLI_ANSI_RESET,
                  s.logSessionName);
    printU32("Events max size", s.logEventsMaxBytes, "B");
  }

  if (showI2c) {
    beginSection("I2C Bus");
    printU32("Bus frequency", s.i2cFreqHz, "Hz");
    printU32("Operation timeout", s.i2cOpTimeoutMs, "ms");
    printU32("Stuck debounce", s.i2cStuckDebounceMs, "ms");
    printU32("Max consecutive failures", s.i2cMaxConsecutiveFailures);
    printU32("Recovery backoff base", s.i2cRecoveryBackoffMs, "ms");
    printU32("Recovery backoff max", s.i2cRecoveryBackoffMaxMs, "ms");
    printU32("Requests per tick", s.i2cRequestsPerTick);
    printU32("Results per tick", s.i2cMaxResultsPerTick);
    printU32("Task wait", s.i2cTaskWaitMs, "ms");
    printU32("Slow op threshold", s.i2cSlowOpThresholdUs, "us");
    printU32("Slow op degrade count", s.i2cSlowOpDegradeCount);
    printU32("Slow window", s.i2cSlowWindowMs, "ms");
    printU32("Task heartbeat timeout", s.i2cTaskHeartbeatTimeoutMs, "ms");
    printU32("Health stale multiplier", s.i2cHealthStaleTaskMultiplier);
    printU32("Health recent window", s.i2cHealthRecentWindowMs, "ms");
    printU32("Recover timeout", s.i2cRecoverTimeoutMs, "ms");
  }

  if (showEnv) {
    beginSection("Environment");
    printU32("Poll interval", s.i2cEnvPollMs, "ms");
    printU32("Conversion wait", s.i2cEnvConversionWaitMs, "ms");
    printHex8("I2C address", s.i2cEnvAddress);

    Serial.printf("  %s%-28s%s %s (%u)\n",
                  CLI_ANSI_INFO,
                  "BME mode",
                  CLI_ANSI_RESET,
                  bmeModeToStr(s.i2cEnvBmeMode),
                  static_cast<unsigned int>(s.i2cEnvBmeMode));
    printU32("BME oversampling T", s.i2cEnvBmeOsrsT);
    printU32("BME oversampling P", s.i2cEnvBmeOsrsP);
    printU32("BME oversampling H", s.i2cEnvBmeOsrsH);
    printU32("BME filter", s.i2cEnvBmeFilter);
    printU32("BME standby", s.i2cEnvBmeStandby);

    printU32("SHT mode", s.i2cEnvShtMode);
    printU32("SHT repeatability", s.i2cEnvShtRepeatability);
    printU32("SHT periodic rate", s.i2cEnvShtPeriodicRate);
    printBool("SHT clock stretching", s.i2cEnvShtClockStretching != 0U);
    printBool("SHT low VDD mode", s.i2cEnvShtLowVdd);
    printU32("SHT command delay", s.i2cEnvShtCommandDelayMs, "ms");
    printU32("SHT not-ready timeout", s.i2cEnvShtNotReadyTimeoutMs, "ms");
    printU32("SHT fetch margin", s.i2cEnvShtPeriodicFetchMarginMs, "ms");
    printBool("SHT allow GC reset", s.i2cEnvShtAllowGeneralCallReset);
    printBool("SHT recover via bus reset", s.i2cEnvShtRecoverUseBusReset);
    printBool("SHT recover via soft reset", s.i2cEnvShtRecoverUseSoftReset);
    printBool("SHT recover via hard reset", s.i2cEnvShtRecoverUseHardReset);
  }

  if (showRtc) {
    beginSection("RTC");
    printU32("Poll interval", s.i2cRtcPollMs, "ms");
    printHex8("I2C address", s.i2cRtcAddress);
    printU32("Backup mode", s.i2cRtcBackupMode);
    printBool("EEPROM writes", s.i2cRtcEnableEepromWrites);
    printU32("EEPROM timeout", s.i2cRtcEepromTimeoutMs, "ms");
    printU32("Offline threshold", s.i2cRtcOfflineThreshold);
  }

  if (showDisplay) {
    beginSection("Display");
    printU32("Poll interval", s.i2cDisplayPollMs, "ms");
    printHex8("I2C address", s.i2cDisplayAddress);
  }

  if (showLidar) {
    beginSection("TF-Luna");
    printU32("UART service", s.lidarServiceMs, "ms");
    printU32("Minimum strength", s.lidarMinStrength);
    printU32("Maximum distance", s.lidarMaxDistanceCm, "cm");
    printU32("Frame stale threshold", s.lidarFrameStaleMs, "ms");
    printU32("Serial summary interval", s.serialPrintIntervalMs, "ms");
  }

  if (showWifi) {
    beginSection("WiFi");
    printBool("Enabled", s.wifiEnabled);
    printValue("AP SSID", s.apSsid);
    printBool("AP secret set", apPassLen > 0U);
    printU32("AP secret length", static_cast<uint32_t>(apPassLen), "chars");
    printU32("AP auto-off", s.apAutoOffMs, "ms");
  }

  if (showWeb) {
    beginSection("Web");
    printU32("Max /api/settings body", s.webMaxSettingsBodyBytes, "B");
    printU32("Max /api/rtc/set body", s.webMaxRtcBodyBytes, "B");
  }
}

void SerialCli::printBootConfig(const char* which) {
  const char* resolved = which;
  if (resolved == nullptr || resolved[0] == '\0') {
    resolved = "all";
  }

  const bool showAll = strcmp(resolved, "all") == 0;
  const bool showHardware = showAll || strcmp(resolved, "hardware") == 0;
  const bool showApp = showAll || strcmp(resolved, "app") == 0;
  if (!showHardware && !showApp) {
    Serial.println("ERR usage: config show <hardware|app|all>");
    printHint("help config");
    return;
  }

  const HardwareSettings& hw = _app.getConfig();
  const AppSettings& app = _app.getAppSettings();

  if (showHardware) {
    Serial.println("hardware:");
    Serial.printf("i2c_sda=%d i2c_scl=%d i2c_task_stack=%u i2c_task_priority=%u\n",
                  hw.i2cSda,
                  hw.i2cScl,
                  static_cast<unsigned int>(hw.i2cTaskStack),
                  static_cast<unsigned int>(hw.i2cTaskPriority));
    Serial.printf("i2c_recovery_pulses=%u i2c_recovery_high_us=%lu i2c_recovery_low_us=%lu i2c_power_cycle_hook=%u\n",
                  static_cast<unsigned int>(hw.i2cRecoveryPulses),
                  static_cast<unsigned long>(hw.i2cRecoveryPulseHighUs),
                  static_cast<unsigned long>(hw.i2cRecoveryPulseLowUs),
                  hw.i2cPowerCycleHook != nullptr ? 1U : 0U);
    Serial.printf("spi_sck=%d spi_miso=%d spi_mosi=%d sd_cs=%d sd_cd=%d sd_cd_active_low=%u\n",
                  hw.spiSck,
                  hw.spiMiso,
                  hw.spiMosi,
                  hw.sdCs,
                  hw.sdCdPin,
                  hw.sdCdActiveLow ? 1U : 0U);
    Serial.printf("e2_tx=%d e2_rx=%d e2_en=%d\n", hw.e2Tx, hw.e2Rx, hw.e2En);
    Serial.printf("lidar_rx=%d lidar_tx=%d lidar_uart=%u (sensor_tx->esp_rx sensor_rx->esp_tx)\n",
                  hw.lidarRx,
                  hw.lidarTx,
                  static_cast<unsigned int>(hw.lidarUartIndex));
    Serial.printf("button_pin=%d button_active_low=%u debounce_ms=%lu long_press_ms=%lu multi_window_ms=%lu multi_count=%u\n",
                  hw.buttonPin,
                  hw.buttonActiveLow ? 1U : 0U,
                  static_cast<unsigned long>(hw.buttonDebounceMs),
                  static_cast<unsigned long>(hw.buttonLongPressMs),
                  static_cast<unsigned long>(hw.buttonMultiPressWindowMs),
                  static_cast<unsigned int>(hw.buttonMultiPressCount));
    Serial.printf("led_pin=%d led_count=%u wifi_led_index=%u health_led_index=%u led_brightness=%u led_smooth_step_ms=%u\n",
                  hw.ledPin,
                  static_cast<unsigned int>(hw.ledCount),
                  static_cast<unsigned int>(hw.wifiLedIndex),
                  static_cast<unsigned int>(hw.healthLedIndex),
                  static_cast<unsigned int>(hw.ledBrightness),
                  static_cast<unsigned int>(hw.ledSmoothStepMs));
  }

  if (showApp) {
    Serial.println("app:");
    Serial.printf("serial_enabled=%u serial_baud_rate=%lu enable_nvs=%u enable_sd=%u enable_web=%u enable_display=%u\n",
                  app.serialEnabled ? 1U : 0U,
                  static_cast<unsigned long>(app.serialBaudRate),
                  app.enableNvs ? 1U : 0U,
                  app.enableSd ? 1U : 0U,
                  app.enableWeb ? 1U : 0U,
                  app.enableDisplay ? 1U : 0U);
    Serial.printf("web_port=%u web_broadcast_ms=%lu web_ui_ws_reconnect_ms=%lu web_ui_graph_refresh_ms=%lu web_ui_events_refresh_ms=%lu web_ui_event_fetch_count=%u\n",
                  static_cast<unsigned int>(app.webPort),
                  static_cast<unsigned long>(app.webBroadcastMs),
                  static_cast<unsigned long>(app.webUiWsReconnectMs),
                  static_cast<unsigned long>(app.webUiGraphRefreshMs),
                  static_cast<unsigned long>(app.webUiEventsRefreshMs),
                  static_cast<unsigned int>(app.webUiEventFetchCount));
    Serial.printf("state_mutex_timeout_ms=%lu\n",
                  static_cast<unsigned long>(app.stateMutexTimeoutMs));
    Serial.printf("sd_worker_priority=%u sd_worker_stack_bytes=%u sd_worker_idle_ms=%u\n",
                  static_cast<unsigned int>(app.sdWorkerPriority),
                  static_cast<unsigned int>(app.sdWorkerStackBytes),
                  static_cast<unsigned int>(app.sdWorkerIdleMs));
    Serial.printf("sd_request_queue_depth=%u sd_result_queue_depth=%u sd_max_open_files=%u sd_max_path_length=%u\n",
                  static_cast<unsigned int>(app.sdRequestQueueDepth),
                  static_cast<unsigned int>(app.sdResultQueueDepth),
                  static_cast<unsigned int>(app.sdMaxOpenFiles),
                  static_cast<unsigned int>(app.sdMaxPathLength));
    Serial.printf("sd_max_copy_write_bytes=%u sd_copy_write_slots=%u sd_lock_timeout_ms=%u sd_mount_timeout_ms=%u sd_op_timeout_ms=%u sd_io_timeout_ms=%u sd_io_chunk_bytes=%u\n",
                  static_cast<unsigned int>(app.sdMaxCopyWriteBytes),
                  static_cast<unsigned int>(app.sdCopyWriteSlots),
                  static_cast<unsigned int>(app.sdLockTimeoutMs),
                  static_cast<unsigned int>(app.sdMountTimeoutMs),
                  static_cast<unsigned int>(app.sdOpTimeoutMs),
                  static_cast<unsigned int>(app.sdIoTimeoutMs),
                  static_cast<unsigned int>(app.sdIoChunkBytes));
  }
}

void SerialCli::printSettableKeys(const char* group) {
  const char* resolved = group;
  if (resolved == nullptr || resolved[0] == '\0') {
    resolved = "all";
  }
  if (strcmp(resolved, "all") != 0 && !isSettingGroup(resolved)) {
    Serial.println("ERR group must be all|system|leds|log|sd|lidar|tfluna|i2c|env|rtc|display|wifi|web");
    return;
  }

  const bool showAll = strcmp(resolved, "all") == 0;
  const bool showSystem = showAll || strcmp(resolved, "system") == 0 || strcmp(resolved, "leds") == 0;
  const bool showLog = showAll || strcmp(resolved, "log") == 0 || strcmp(resolved, "sd") == 0;
  const bool showLidar = showAll || strcmp(resolved, "lidar") == 0 || strcmp(resolved, "tfluna") == 0 ||
                         strcmp(resolved, "co2") == 0 || strcmp(resolved, "e2") == 0;
  const bool showI2c = showAll || strcmp(resolved, "i2c") == 0;
  const bool showEnv = showAll || strcmp(resolved, "env") == 0;
  const bool showRtc = showAll || strcmp(resolved, "rtc") == 0;
  const bool showDisplay = showAll || strcmp(resolved, "display") == 0;
  const bool showWifi = showAll || strcmp(resolved, "wifi") == 0;
  const bool showWeb = showAll || strcmp(resolved, "web") == 0;

  Serial.printf("set list group=%s\n", resolved);
  if (showSystem) {
    Serial.println("system: sample_interval_ms cli_verbosity command_drain_per_tick command_queue_degraded_window_ms command_queue_degraded_depth_threshold main_tick_slow_threshold_us led_health_init_ms led_health_debounce_ms ap_start_retry_backoff_ms");
  }
  if (showLog) {
    Serial.println("log/sd: daily_enabled all_enabled all_max_bytes flush_ms io_budget_ms mount_retry_ms write_retry_backoff_ms max_write_retries session_name events_max_bytes");
  }
  if (showLidar) {
    Serial.println("lidar/tfluna: service_ms min_strength max_distance_cm frame_stale_ms serial_print_interval_ms");
  }
  if (showI2c) {
    Serial.println("i2c: freq_hz op_timeout_ms stuck_debounce_ms max_consecutive_failures recovery_backoff_ms recovery_backoff_max_ms requests_per_tick slow_op_threshold_us slow_op_degrade_count task_heartbeat_timeout_ms recover_timeout_ms max_results_per_tick task_wait_ms health_stale_task_multiplier slow_window_ms health_recent_window_ms");
  }
  if (showEnv) {
    Serial.println("env: poll_ms conversion_wait_ms address bme_mode bme_osrs_t bme_osrs_p bme_osrs_h bme_filter bme_standby sht_mode sht_repeatability sht_periodic_rate sht_clock_stretching sht_low_vdd sht_command_delay_ms sht_not_ready_timeout_ms sht_periodic_fetch_margin_ms sht_allow_general_call_reset sht_recover_use_bus_reset sht_recover_use_soft_reset sht_recover_use_hard_reset");
  }
  if (showRtc) {
    Serial.println("rtc: poll_ms backup_mode enable_eeprom_writes eeprom_timeout_ms offline_threshold");
  }
  if (showDisplay) {
    Serial.println("display: poll_ms address");
  }
  if (showWifi) {
    Serial.println("wifi: enabled ssid secret auto_off_ms");
  }
  if (showWeb) {
    Serial.println("web: max_settings_body_bytes max_rtc_body_bytes");
  }
  Serial.println("usage: set <group> <key> <value> [persist:0|1]");
  Serial.println("direct mode: set <full_key> <value> [persist:0|1]");
}

void SerialCli::printSdInfo() {
  SystemStatus sys{};
  Sample latest{};
  bool hasLatest = false;
  if (!_app.tryGetStatusSnapshot(sys, latest, hasLatest)) {
    Serial.println("ERR state busy");
    return;
  }
  (void)latest;
  (void)hasLatest;

  Serial.printf("%s[SD Card Info]%s\n", CLI_ANSI_WARN, CLI_ANSI_RESET);
  Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Mounted", CLI_ANSI_RESET, yesNo(sys.sdMounted));
  Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Info valid", CLI_ANSI_RESET, yesNo(sys.sdInfoValid));
  Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Usage valid", CLI_ANSI_RESET, yesNo(sys.sdUsageValid));
  Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Filesystem", CLI_ANSI_RESET, sdFsTypeToStr(sys.sdFsType));
  Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Card type", CLI_ANSI_RESET, sdCardTypeToStr(sys.sdCardType));
  char buf[32];
  sprintBytesHuman(buf, sizeof(buf), sys.sdFsCapacityBytes);
  Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Capacity", CLI_ANSI_RESET, buf);
  sprintBytesHuman(buf, sizeof(buf), sys.sdFsUsedBytes);
  Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Used", CLI_ANSI_RESET, buf);
  sprintBytesHuman(buf, sizeof(buf), sys.sdFsFreeBytes);
  Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Free", CLI_ANSI_RESET, buf);
  sprintBytesHuman(buf, sizeof(buf), sys.sdCardCapacityBytes);
  Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Card capacity", CLI_ANSI_RESET, buf);
  if (sys.sdUsageValid && sys.sdFsCapacityBytes > 0U) {
    const uint64_t pct = (sys.sdFsUsedBytes * 100ULL) / sys.sdFsCapacityBytes;
    const uint32_t usagePct = static_cast<uint32_t>((pct > 100ULL) ? 100ULL : pct);
    Serial.printf("  %s%-28s%s %lu%%\n", CLI_ANSI_INFO, "Usage", CLI_ANSI_RESET, static_cast<unsigned long>(usagePct));
  }
  Serial.printf("  %s%-28s%s %lu ms\n", CLI_ANSI_INFO, "Info age", CLI_ANSI_RESET, static_cast<unsigned long>(sys.sdInfoAgeMs));
}

void SerialCli::printSdList(const char* path) {
  SystemStatus sys{};
  Sample latest{};
  bool hasLatest = false;
  if (!_app.tryGetStatusSnapshot(sys, latest, hasLatest)) {
    Serial.println("ERR state busy");
    return;
  }
  (void)latest;
  (void)hasLatest;

  const char* requested = (path != nullptr && path[0] != '\0') ? path : "/logs";
  char query[HardwareSettings::SD_PATH_BYTES] = {0};
  if (requested[0] == '/') {
    strncpy(query, requested, sizeof(query) - 1);
  } else {
    snprintf(query, sizeof(query), "/%s", requested);
  }
  size_t queryLen = strnlen(query, sizeof(query));
  while (queryLen > 1U && query[queryLen - 1U] == '/') {
    query[queryLen - 1U] = '\0';
    --queryLen;
  }

  auto pathStartsWith = [](const char* value, const char* prefix) -> bool {
    if (value == nullptr || prefix == nullptr) {
      return false;
    }
    const size_t pLen = strlen(prefix);
    if (pLen == 0U) {
      return false;
    }
    if (strncmp(value, prefix, pLen) != 0) {
      return false;
    }
    return value[pLen] == '\0' || value[pLen] == '/';
  };

  auto pathInQuery = [&](const char* value) -> bool {
    if (value == nullptr || value[0] == '\0') {
      return false;
    }
    if (strcmp(query, "/") == 0) {
      return true;
    }
    return pathStartsWith(value, query);
  };

  struct KnownPath {
    const char* kind;
    const char* path;
  };

  const KnownPath known[] = {
      {"dir ", "/logs"},
      {"dir ", "/logs/daily"},
      {"dir ", "/logs/runs"},
      {"dir ", (sys.logSessionDir != nullptr) ? sys.logSessionDir : ""},
      {"file", (sys.logCurrentSampleFile != nullptr) ? sys.logCurrentSampleFile : ""},
      {"file", (sys.logCurrentEventFile != nullptr) ? sys.logCurrentEventFile : ""},
  };

  Serial.printf("%s[SD ls]%s\n", CLI_ANSI_WARN, CLI_ANSI_RESET);
  Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Query", CLI_ANSI_RESET, query);
  Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Mounted", CLI_ANSI_RESET, yesNo(sys.sdMounted));
  if (!sys.sdMounted) {
    Serial.printf("  %snote:%s SD not mounted; showing cached runtime paths\n",
                  CLI_ANSI_INFO,
                  CLI_ANSI_RESET);
  }

  size_t shown = 0U;
  for (size_t i = 0; i < (sizeof(known) / sizeof(known[0])); ++i) {
    const char* entry = known[i].path;
    if (entry == nullptr || entry[0] == '\0') {
      continue;
    }
    if (!pathInQuery(entry)) {
      continue;
    }
    bool duplicate = false;
    for (size_t j = 0; j < i; ++j) {
      const char* prev = known[j].path;
      if (prev != nullptr && strcmp(prev, entry) == 0) {
        duplicate = true;
        break;
      }
    }
    if (duplicate) {
      continue;
    }
    Serial.printf("  %s%s%s %s\n", CLI_ANSI_INFO, known[i].kind, CLI_ANSI_RESET, entry);
    ++shown;
  }

  if (shown == 0U) {
    Serial.println("  (no runtime-tracked entries)");
  }

  Serial.printf("  %snote:%s ls shows runtime-tracked paths, not a full card scan\n",
                CLI_ANSI_INFO,
                CLI_ANSI_RESET);
}

void SerialCli::printRtcDiagnostics() {
  RtcDebugSnapshot snap{};
  if (!_app.tryGetRtcDebugSnapshot(snap)) {
    Serial.println("ERR state busy");
    return;
  }

  printTopicHeader("RTC Diagnostics");
  Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Library support", CLI_ANSI_RESET,
                yesNo(snap.supported));
  Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "RTC enabled", CLI_ANSI_RESET,
                yesNo(snap.enabled));
  Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Initialized", CLI_ANSI_RESET,
                yesNo(snap.initialized));
  Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Driver state", CLI_ANSI_RESET,
                rtcDriverStateToStr(snap.driverState));
  Serial.printf("  %s%-28s%s %lu ms\n", CLI_ANSI_INFO, "Snapshot at", CLI_ANSI_RESET,
                static_cast<unsigned long>(snap.updatedMs));
  Serial.println();

  Serial.printf("%s[Config]%s\n", CLI_ANSI_WARN, CLI_ANSI_RESET);
  Serial.printf("  %s%-28s%s 0x%02X\n", CLI_ANSI_INFO, "Address", CLI_ANSI_RESET,
                static_cast<unsigned int>(snap.address));
  Serial.printf("  %s%-28s%s %lu ms\n", CLI_ANSI_INFO, "Requested timeout", CLI_ANSI_RESET,
                static_cast<unsigned long>(snap.requestedI2cTimeoutMs));
  Serial.printf("  %s%-28s%s %lu ms\n", CLI_ANSI_INFO, "Effective timeout", CLI_ANSI_RESET,
                static_cast<unsigned long>(snap.effectiveI2cTimeoutMs));
  Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Requested backup", CLI_ANSI_RESET,
                rtcBackupModeToStr(snap.requestedBackupMode));
  Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Effective backup", CLI_ANSI_RESET,
                snap.hasEffectiveBackupMode ? rtcBackupModeToStr(snap.effectiveBackupMode)
                                            : "unknown");
  Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "EEPROM writes req", CLI_ANSI_RESET,
                snap.requestedEepromWrites ? "on" : "off");
  Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "EEPROM writes eff", CLI_ANSI_RESET,
                snap.effectiveEepromWrites ? "on" : "off");
  Serial.printf("  %s%-28s%s %lu ms\n", CLI_ANSI_INFO, "EEPROM timeout", CLI_ANSI_RESET,
                static_cast<unsigned long>(snap.eepromTimeoutMs));
  Serial.printf("  %s%-28s%s %u\n", CLI_ANSI_INFO, "Offline threshold", CLI_ANSI_RESET,
                static_cast<unsigned int>(snap.offlineThreshold));
  Serial.println();

  Serial.printf("%s[Validity]%s\n", CLI_ANSI_WARN, CLI_ANSI_RESET);
  Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "PORF", CLI_ANSI_RESET,
                snap.powerOnReset ? "set" : "clear");
  Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "VLF", CLI_ANSI_RESET,
                snap.voltageLow ? "set" : "clear");
  Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "BSF", CLI_ANSI_RESET,
                snap.backupSwitched ? "set" : "clear");
  Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Time invalid", CLI_ANSI_RESET,
                snap.timeInvalid ? "yes" : "no");
  Serial.println();

  Serial.printf("%s[EEPROM]%s\n", CLI_ANSI_WARN, CLI_ANSI_RESET);
  Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Busy", CLI_ANSI_RESET,
                yesNo(snap.eepromBusy));
  Serial.printf("  %s%-28s%s code=%s detail=%ld msg='%s'\n",
                CLI_ANSI_INFO,
                "Status",
                CLI_ANSI_RESET,
                errToStrColored(snap.eepromStatus.code),
                static_cast<long>(snap.eepromStatus.detail),
                snap.eepromStatus.msg);
  Serial.printf("  %s%-28s%s %lu\n", CLI_ANSI_INFO, "Writes ok", CLI_ANSI_RESET,
                static_cast<unsigned long>(snap.eepromWriteCount));
  Serial.printf("  %s%-28s%s %lu\n", CLI_ANSI_INFO, "Writes failed", CLI_ANSI_RESET,
                static_cast<unsigned long>(snap.eepromWriteFailures));
  Serial.printf("  %s%-28s%s %u\n", CLI_ANSI_INFO, "Queue depth", CLI_ANSI_RESET,
                static_cast<unsigned int>(snap.eepromQueueDepth));
  Serial.println();

  Serial.printf("%s[Registers]%s\n", CLI_ANSI_WARN, CLI_ANSI_RESET);
  if (snap.hasStatusReg) {
    Serial.printf("  %s%-28s%s 0x%02X\n", CLI_ANSI_INFO, "STATUS raw", CLI_ANSI_RESET,
                  static_cast<unsigned int>(snap.rawStatusReg));
  } else {
    Serial.printf("  %s%-28s%s n/a\n", CLI_ANSI_INFO, "STATUS raw", CLI_ANSI_RESET);
  }
  Serial.printf("  %s%-28s%s code=%s detail=%ld msg='%s'\n",
                CLI_ANSI_INFO,
                "STATUS read",
                CLI_ANSI_RESET,
                errToStrColored(snap.statusRegStatus.code),
                static_cast<long>(snap.statusRegStatus.detail),
                snap.statusRegStatus.msg);
  if (snap.hasTempLsb) {
    Serial.printf("  %s%-28s%s 0x%02X\n", CLI_ANSI_INFO, "TEMP_LSB raw", CLI_ANSI_RESET,
                  static_cast<unsigned int>(snap.rawTempLsb));
  } else {
    Serial.printf("  %s%-28s%s n/a\n", CLI_ANSI_INFO, "TEMP_LSB raw", CLI_ANSI_RESET);
  }
  Serial.printf("  %s%-28s%s code=%s detail=%ld msg='%s'\n",
                CLI_ANSI_INFO,
                "TEMP_LSB read",
                CLI_ANSI_RESET,
                errToStrColored(snap.tempLsbStatus.code),
                static_cast<long>(snap.tempLsbStatus.detail),
                snap.tempLsbStatus.msg);
  if (snap.hasPmuReg) {
    Serial.printf("  %s%-28s%s 0x%02X\n", CLI_ANSI_INFO, "PMU raw", CLI_ANSI_RESET,
                  static_cast<unsigned int>(snap.rawPmuReg));
  } else {
    Serial.printf("  %s%-28s%s n/a\n", CLI_ANSI_INFO, "PMU raw", CLI_ANSI_RESET);
  }
  Serial.printf("  %s%-28s%s code=%s detail=%ld msg='%s'\n",
                CLI_ANSI_INFO,
                "PMU read",
                CLI_ANSI_RESET,
                errToStrColored(snap.pmuStatus.code),
                static_cast<long>(snap.pmuStatus.detail),
                snap.pmuStatus.msg);
}

void SerialCli::printDiagnostics(const char* scope) {
  const char* resolved = scope;
  if (resolved == nullptr || resolved[0] == '\0') {
    resolved = "all";
  }

  const bool showAll = strcmp(resolved, "all") == 0;
  const bool showLidar = showAll || strcmp(resolved, "lidar") == 0 || strcmp(resolved, "tfluna") == 0 ||
                         strcmp(resolved, "co2") == 0 || strcmp(resolved, "e2") == 0;
  const bool showI2c = showAll || strcmp(resolved, "i2c") == 0;
  const bool showRtc = showAll || strcmp(resolved, "rtc") == 0;
  const bool showEnv = showAll || strcmp(resolved, "env") == 0;
  if (!showLidar && !showI2c && !showRtc && !showEnv) {
    Serial.println("ERR usage: diag [all|lidar|i2c|rtc|env]");
    printHint("help diag");
    return;
  }

  size_t count = 0;
  if (!loadDeviceStatuses(count)) {
    Serial.println("ERR state busy");
    return;
  }

  RuntimeSettings settings{};
  const bool haveSettings = _app.tryGetSettingsSnapshot(settings);
  const HardwareSettings& hw = _app.getConfig();

  SystemStatus sys{};
  Sample latest{};
  bool hasLatest = false;
  const bool haveSys = _app.tryGetStatusSnapshot(sys, latest, hasLatest);
  (void)hasLatest;
  I2cScanSnapshot scan{};
  const bool haveScan = _app.tryGetI2cScanSnapshot(scan);

  if (showLidar) {
    const DeviceStatus* lidar = findDeviceInScratch("lidar", count);
    if (lidar != nullptr) {
      Serial.printf("diag lidar: health=%s code=%s detail=%ld msg='%s'\n",
                    healthToStrColored(lidar->health),
                    errToStrColored(lidar->lastStatus.code),
                    static_cast<long>(lidar->lastStatus.detail),
                    lidar->lastStatus.msg);
    }
    Serial.printf("diag lidar hw: rx=%d tx=%d uart=%u mapping='sensor TX -> ESP RX, sensor RX -> ESP TX'\n",
                  hw.lidarRx,
                  hw.lidarTx,
                  static_cast<unsigned int>(hw.lidarUartIndex));
    if (haveSettings) {
      Serial.printf("diag lidar cfg: service_ms=%lu min_strength=%u max_distance_cm=%u stale_ms=%lu serial_ms=%lu\n",
                    static_cast<unsigned long>(settings.lidarServiceMs),
                    static_cast<unsigned int>(settings.lidarMinStrength),
                    static_cast<unsigned int>(settings.lidarMaxDistanceCm),
                    static_cast<unsigned long>(settings.lidarFrameStaleMs),
                    static_cast<unsigned long>(settings.serialPrintIntervalMs));
    }
    if (haveSys) {
      Serial.printf("diag lidar stream: frames=%lu checksum=%lu sync=%lu age_ms=%lu valid=%lu invalid=%lu weak=%lu\n",
                    static_cast<unsigned long>(sys.lidarFramesParsed),
                    static_cast<unsigned long>(sys.lidarChecksumErrors),
                    static_cast<unsigned long>(sys.lidarSyncLossCount),
                    static_cast<unsigned long>(sys.lidarFrameAgeMs),
                    static_cast<unsigned long>(sys.lidarStats.validSamples),
                    static_cast<unsigned long>(sys.lidarStats.invalidSamples),
                    static_cast<unsigned long>(sys.lidarStats.weakSamples));
      Serial.printf("diag lidar last: dist=%u strength=%u temp=%.2f frame=%u signal=%u time_src=%s\n",
                    static_cast<unsigned int>(latest.distanceCm),
                    static_cast<unsigned int>(latest.strength),
                    static_cast<double>(latest.lidarTempC),
                    latest.validFrame ? 1U : 0U,
                    latest.signalOk ? 1U : 0U,
                    sys.timeSource);
    }
    if (lidar != nullptr && lidar->health != HealthState::OK) {
      Serial.println("diag lidar next:");
      Serial.println("  lidar pins");
      Serial.println("  lidar probe");
      Serial.println("  lidar recover");
      Serial.println("  settings show lidar");
    }
  }

  if (showI2c) {
    const DeviceStatus* i2c = findDeviceInScratch("i2c_bus", count);
    if (i2c != nullptr) {
      Serial.printf("diag i2c: health=%s code=%s detail=%ld msg='%s'\n",
                    healthToStrColored(i2c->health),
                    errToStrColored(i2c->lastStatus.code),
                    static_cast<long>(i2c->lastStatus.detail),
                    i2c->lastStatus.msg);
    }
    Serial.printf("diag i2c hw: sda=%d scl=%d backend_pref=%s\n",
                  hw.i2cSda,
                  hw.i2cScl,
                  "idf");
    if (haveSettings) {
      Serial.printf("diag i2c cfg: freq=%lu timeout_ms=%lu rtc_addr=0x%02X env_addr=0x%02X env_model=%s\n",
                    static_cast<unsigned long>(settings.i2cFreqHz),
                    static_cast<unsigned long>(settings.i2cOpTimeoutMs),
                    static_cast<unsigned int>(settings.i2cRtcAddress),
                    static_cast<unsigned int>(settings.i2cEnvAddress),
                    envModelHint(settings.i2cEnvAddress));
    }
    if (haveSys) {
      Serial.printf("diag i2c bus: err=%lu consec=%lu recov=%lu stuck=%lu fast_fail=%lu stale=%lu\n",
                    static_cast<unsigned long>(sys.i2cErrorCount),
                    static_cast<unsigned long>(sys.i2cConsecutiveErrors),
                    static_cast<unsigned long>(sys.i2cRecoveryCount),
                    static_cast<unsigned long>(sys.i2cStuckSdaCount),
                    static_cast<unsigned long>(sys.i2cStuckBusFastFailCount),
                    static_cast<unsigned long>(sys.i2cStaleResultCount));
      Serial.printf("diag i2c perf: req_q=%lu res_q=%lu max_us=%lu mean_us=%lu backend=%s\n",
                    static_cast<unsigned long>(sys.i2cRequestQueueDepth),
                    static_cast<unsigned long>(sys.i2cResultQueueDepth),
                    static_cast<unsigned long>(sys.i2cMaxDurationUs),
                    static_cast<unsigned long>(sys.i2cMeanDurationUs),
                    sys.i2cBackendName);
    }
    if (haveScan && (scan.active || scan.complete || scan.probesTotal > 0U)) {
      Serial.printf("diag i2c scan: active=%u complete=%u next=0x%02X probes=%lu found=%u nack=%lu\n",
                    scan.active ? 1U : 0U,
                    scan.complete ? 1U : 0U,
                    static_cast<unsigned int>(scan.nextAddress),
                    static_cast<unsigned long>(scan.probesTotal),
                    static_cast<unsigned int>(scan.foundCount),
                    static_cast<unsigned long>(scan.probesNack));
    }

    if (i2c != nullptr && i2c->health != HealthState::OK) {
      Serial.println("diag i2c next:");
      Serial.println("  i2c recover");
      Serial.println("  i2c scan");
      Serial.println("  config show hardware");
      Serial.println("  settings show i2c");
      Serial.println("  preset i2c safe");
    }
  }

  if (showRtc) {
    const DeviceStatus* rtc = findDeviceInScratch("rtc", count);
    if (rtc != nullptr) {
      Serial.printf("diag rtc: health=%s code=%s detail=%ld msg='%s'\n",
                    healthToStrColored(rtc->health),
                    errToStrColored(rtc->lastStatus.code),
                    static_cast<long>(rtc->lastStatus.detail),
                    rtc->lastStatus.msg);
    }
    if (haveSettings) {
      Serial.printf("diag rtc cfg: addr=0x%02X poll_ms=%lu backup=%u eeprom=%u eeprom_timeout_ms=%lu offline=%u\n",
                    static_cast<unsigned int>(settings.i2cRtcAddress),
                    static_cast<unsigned long>(settings.i2cRtcPollMs),
                    static_cast<unsigned int>(settings.i2cRtcBackupMode),
                    settings.i2cRtcEnableEepromWrites ? 1U : 0U,
                    static_cast<unsigned long>(settings.i2cRtcEepromTimeoutMs),
                    static_cast<unsigned int>(settings.i2cRtcOfflineThreshold));
    }
    printRtcDiagnostics();

    if (rtc != nullptr) {
      const bool needsSet =
          rtc->lastStatus.code == Err::NOT_INITIALIZED &&
          strstr(rtc->lastStatus.msg, "time not set") != nullptr;
      if (needsSet) {
        Serial.println("diag rtc next:");
        Serial.println("  rtc set 2026 01 01 00 00 00");
        Serial.println("  rtc read");
      } else if (rtc->health != HealthState::OK) {
        Serial.println("diag rtc next:");
        Serial.println("  rtc recover");
        Serial.println("  rtc status");
        Serial.println("  settings show rtc");
      }
    }
  }

  if (showEnv) {
    const DeviceStatus* env = findDeviceInScratch("env", count);
    if (env != nullptr) {
      Serial.printf("diag env: health=%s code=%s detail=%ld msg='%s'\n",
                    healthToStrColored(env->health),
                    errToStrColored(env->lastStatus.code),
                    static_cast<long>(env->lastStatus.detail),
                    env->lastStatus.msg);
    }
    if (haveSettings) {
      Serial.printf("diag env cfg: addr=0x%02X model=%s poll_ms=%lu conv_wait_ms=%lu bme_mode=%u sht_mode=%u\n",
                    static_cast<unsigned int>(settings.i2cEnvAddress),
                    envModelHint(settings.i2cEnvAddress),
                    static_cast<unsigned long>(settings.i2cEnvPollMs),
                    static_cast<unsigned long>(settings.i2cEnvConversionWaitMs),
                    static_cast<unsigned int>(settings.i2cEnvBmeMode),
                    static_cast<unsigned int>(settings.i2cEnvShtMode));
    }
    if (env != nullptr && env->health != HealthState::OK) {
      Serial.println("diag env next:");
      Serial.println("  env read");
      Serial.println("  settings show env");
      Serial.println("  env address 0x76");
      Serial.println("  env conversion_wait 30");
      Serial.println("  i2c recover");
    }
  }
}

void SerialCli::printI2cScan() {
  I2cScanSnapshot scan{};
  if (!_app.tryGetI2cScanSnapshot(scan)) {
    Serial.println("ERR state busy");
    return;
  }

  Err summaryCode = scan.lastStatus.code;
  int32_t summaryDetail = scan.lastStatus.detail;
  const char* summaryMsg = scan.lastStatus.msg;
  if (scan.complete && scan.probesError == 0U && scan.probesTimeout == 0U) {
    summaryCode = Err::OK;
    summaryDetail = 0;
    summaryMsg = "I2C scan complete";
  }

  Serial.printf("%s[I2C Scan]%s\n", CLI_ANSI_WARN, CLI_ANSI_RESET);
  Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Active", CLI_ANSI_RESET, yesNo(scan.active));
  Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Complete", CLI_ANSI_RESET, yesNo(scan.complete));
  Serial.printf("  %s%-28s%s 0x%02X\n", CLI_ANSI_INFO, "Next address", CLI_ANSI_RESET, static_cast<unsigned int>(scan.nextAddress));
  Serial.printf("  %s%-28s%s %lu\n", CLI_ANSI_INFO, "Probes total", CLI_ANSI_RESET, static_cast<unsigned long>(scan.probesTotal));
  printCounterVal("Probes timeout", scan.probesTimeout);
  printCounterVal("Probes error", scan.probesError);
  Serial.printf("  %s%-28s%s %lu\n", CLI_ANSI_INFO, "Probes NACK", CLI_ANSI_RESET, static_cast<unsigned long>(scan.probesNack));
  Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Status code", CLI_ANSI_RESET, errToStrColored(summaryCode));
  Serial.printf("  %s%-28s%s %ld\n", CLI_ANSI_INFO, "Status detail", CLI_ANSI_RESET, static_cast<long>(summaryDetail));
  Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Status message", CLI_ANSI_RESET, summaryMsg);
  Serial.println();
  Serial.printf("%s[Found Devices]%s  count=%u\n", CLI_ANSI_WARN, CLI_ANSI_RESET, static_cast<unsigned int>(scan.foundCount));
  if (scan.foundCount == 0) {
    Serial.println("  none");
    return;
  }
  for (uint8_t i = 0; i < scan.foundCount; ++i) {
    Serial.printf("  0x%02X\n", static_cast<unsigned int>(scan.foundAddresses[i]));
  }
}

void SerialCli::printI2cProbe() {
  I2cRawSnapshot raw{};
  if (!_app.tryGetI2cRawSnapshot(raw)) {
    Serial.println("ERR state busy");
    return;
  }

  Serial.printf("%s[I2C Probe/Raw]%s\n", CLI_ANSI_WARN, CLI_ANSI_RESET);
  Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Queued", CLI_ANSI_RESET, yesNo(raw.queued));
  Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Active", CLI_ANSI_RESET, yesNo(raw.active));
  Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Complete", CLI_ANSI_RESET, yesNo(raw.complete));
  Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Operation", CLI_ANSI_RESET, i2cRawOpToStr(raw.op));
  Serial.printf("  %s%-28s%s 0x%02X\n", CLI_ANSI_INFO, "Address", CLI_ANSI_RESET, static_cast<unsigned int>(raw.address));
  Serial.printf("  %s%-28s%s %lu ms\n", CLI_ANSI_INFO, "Updated", CLI_ANSI_RESET, static_cast<unsigned long>(raw.updatedMs));
  Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Status code", CLI_ANSI_RESET, errToStrColored(raw.lastStatus.code));
  Serial.printf("  %s%-28s%s %ld\n", CLI_ANSI_INFO, "Status detail", CLI_ANSI_RESET, static_cast<long>(raw.lastStatus.detail));
  Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Status message", CLI_ANSI_RESET, raw.lastStatus.msg);
}

void SerialCli::applyPreset(const char* domain, const char* preset, bool persist) {
  if (domain == nullptr || preset == nullptr) {
    Serial.println("ERR usage: preset <env|i2c|rtc> <profile> [persist]");
    printHint("help preset");
    return;
  }

  RuntimeSettings settings{};
  if (!_app.tryGetSettingsSnapshot(settings)) {
    Serial.println("ERR state busy");
    return;
  }

  if (strcmp(domain, "env") == 0) {
    if (strcmp(preset, "bme280") == 0 || strcmp(preset, "bme") == 0) {
      settings.i2cEnvAddress = 0x76;
      settings.i2cEnvConversionWaitMs = 30;
      settings.i2cEnvBmeMode = 1;
      settings.i2cEnvBmeOsrsT = 1;
      settings.i2cEnvBmeOsrsP = 1;
      settings.i2cEnvBmeOsrsH = 1;
      settings.i2cEnvBmeFilter = 0;
      settings.i2cEnvBmeStandby = 2;
      if (settings.i2cEnvPollMs < 1000UL) {
        settings.i2cEnvPollMs = 1000UL;
      }
    } else if (strcmp(preset, "sht3x") == 0 || strcmp(preset, "sht") == 0) {
      settings.i2cEnvAddress = 0x44;
      settings.i2cEnvConversionWaitMs = 20;
      settings.i2cEnvShtMode = 0;
      settings.i2cEnvShtRepeatability = 2;
      settings.i2cEnvShtPeriodicRate = 1;
      settings.i2cEnvShtClockStretching = 0;
      settings.i2cEnvShtLowVdd = false;
      if (settings.i2cEnvPollMs < 1000UL) {
        settings.i2cEnvPollMs = 1000UL;
      }
    } else {
      Serial.println("ERR env preset must be bme280|sht3x");
      return;
    }
  } else if (strcmp(domain, "rtc") == 0) {
    if (strcmp(preset, "rv3032") != 0) {
      Serial.println("ERR rtc preset must be rv3032");
      return;
    }
    settings.i2cRtcAddress = 0x51;
    settings.i2cRtcPollMs = 1000;
    settings.i2cRtcBackupMode = 1;
    settings.i2cRtcEnableEepromWrites = true;
  } else if (strcmp(domain, "i2c") == 0) {
    if (strcmp(preset, "safe") != 0) {
      Serial.println("ERR i2c preset must be safe");
      return;
    }
    settings.i2cFreqHz = 100000;
    settings.i2cOpTimeoutMs = 30;
    settings.i2cRecoverTimeoutMs = 500;
    settings.i2cRequestsPerTick = 2;
    settings.i2cMaxResultsPerTick = 8;
    settings.i2cTaskWaitMs = 20;
    if (settings.i2cEnvConversionWaitMs < 20UL) {
      settings.i2cEnvConversionWaitMs = 20UL;
    }
  } else {
    Serial.println("ERR usage: preset <env|i2c|rtc> <profile> [persist]");
    printHint("help preset");
    return;
  }

  const Status validation = settings.validate();
  if (!validation.ok()) {
    Serial.printf("ERR preset invalid: %s (%s)\n", errToStr(validation.code), validation.msg);
    return;
  }

  queueSettingsUpdate(settings, persist);
}

void SerialCli::queueSettingsUpdate(const RuntimeSettings& s, bool persist) {
  const Status st = _app.enqueueApplySettings(s, persist);
  if (!st.ok()) {
    Serial.printf("ERR settings queue failed: %s (%s)\n", errToStr(st.code), st.msg);
    return;
  }
  printOkf("settings queued persist=%u", persist ? 1U : 0U);
}

void SerialCli::executeLine(char* line, uint32_t nowMs) {
  (void)nowMs;
  char* tokens[CLI_MAX_TOKENS] = {};
  size_t argc = 0;

  char* p = line;
  while (*p != '\0') {
    while (*p == ' ' || *p == '\t') {
      ++p;
    }
    if (*p == '\0') {
      break;
    }
    if (argc >= CLI_MAX_TOKENS) {
      Serial.println("ERR too many tokens");
      return;
    }
    tokens[argc++] = p;
    while (*p != '\0' && *p != ' ' && *p != '\t') {
      ++p;
    }
    if (*p == '\0') {
      break;
    }
    *p = '\0';
    ++p;
  }

  if (argc == 0) {
    return;
  }

  auto queueSdRemount = [&]() {
    const Status st = _app.enqueueRemountSd();
    if (!st.ok()) {
      Serial.printf("ERR sd remount queue failed: %s (%s)\n", errToStr(st.code), st.msg);
      return;
    }
    printOkf("sd remount queued");
  };

  auto queueI2cRecover = [&]() {
    const Status st = _app.enqueueRecoverI2cBus();
    if (!st.ok()) {
      Serial.printf("ERR i2c recover queue failed: %s (%s)\n", errToStr(st.code), st.msg);
      return;
    }
    printOkf("i2c recover queued");
  };

  auto queueWifiSet = [&](bool enable, bool persist) {
    RuntimeSettings settings{};
    if (!_app.tryGetSettingsSnapshot(settings)) {
      Serial.println("ERR state busy");
      return;
    }
    const bool settingsChanged = (settings.wifiEnabled != enable);
    if (settingsChanged || persist) {
      settings.wifiEnabled = enable;
      queueSettingsUpdate(settings, persist);
    }
    if (!settingsChanged) {
      const Status st = _app.enqueueSetWifiApEnabled(enable);
      if (!st.ok()) {
        Serial.printf("ERR wifi queue failed: %s (%s)\n", errToStr(st.code), st.msg);
        return;
      }
      if (!persist) {
        printOkf("wifi %s queued", enable ? "on" : "off");
      }
    }
  };

  auto printUsageWithHint = [&](const char* usage, const char* helpTopic) {
    Serial.printf("%sERR%s usage: %s\n", CLI_ANSI_ERR, CLI_ANSI_RESET, usage);
    if (helpTopic != nullptr && helpTopic[0] != '\0') {
      Serial.printf("%shint:%s help %s%s%s\n",
                    CLI_ANSI_INFO,
                    CLI_ANSI_RESET,
                    CLI_ANSI_INFO,
                    helpTopic,
                    CLI_ANSI_RESET);
    }
  };

  auto queueGroupedSetting = [&](const char* group, const char* key, const char* value, bool persist) -> bool {
    char keyBuffer[48] = {};
    const char* resolvedKey = nullptr;
    const char* errorMsg = "";
    if (!resolveGroupedSettingKey(group, key, keyBuffer, sizeof(keyBuffer), resolvedKey, errorMsg)) {
      Serial.printf("ERR set key mapping failed: %s\n", errorMsg);
      return false;
    }

    RuntimeSettings settings{};
    if (!_app.tryGetSettingsSnapshot(settings)) {
      Serial.println("ERR state busy");
      return false;
    }

    if (!applySettingByKey(settings, resolvedKey, value, errorMsg)) {
      Serial.printf("ERR set failed: %s\n", errorMsg);
      return false;
    }

    const Status validation = settings.validate();
    if (!validation.ok()) {
      Serial.printf("ERR settings invalid: %s (%s)\n", errToStr(validation.code), validation.msg);
      return false;
    }

    queueSettingsUpdate(settings, persist);
    return true;
  };

  auto loadSettingsSnapshot = [&](RuntimeSettings& out) -> bool {
    if (!_app.tryGetSettingsSnapshot(out)) {
      Serial.println("ERR state busy");
      return false;
    }
    return true;
  };

  auto queueI2cProbeAddress = [&](uint8_t address) {
    const Status st = _app.enqueueI2cProbeAddress(address);
    if (!st.ok()) {
      Serial.printf("ERR i2c probe queue failed: %s (%s)\n", errToStr(st.code), st.msg);
      return;
    }
    printOkf("i2c probe queued addr=0x%02X", static_cast<unsigned int>(address));
    printHint("use 'i2c probe status' after 1-2 seconds");
  };

  auto printEnvBmeSummary = [&](const RuntimeSettings& s) {
    Serial.printf("%s[ENV BME280 Config]%s\n", CLI_ANSI_WARN, CLI_ANSI_RESET);
    Serial.printf("  %s%-28s%s %u\n", CLI_ANSI_INFO, "Mode", CLI_ANSI_RESET, static_cast<unsigned int>(s.i2cEnvBmeMode));
    Serial.printf("  %s%-28s%s %u\n", CLI_ANSI_INFO, "Oversampling temp", CLI_ANSI_RESET, static_cast<unsigned int>(s.i2cEnvBmeOsrsT));
    Serial.printf("  %s%-28s%s %u\n", CLI_ANSI_INFO, "Oversampling pressure", CLI_ANSI_RESET, static_cast<unsigned int>(s.i2cEnvBmeOsrsP));
    Serial.printf("  %s%-28s%s %u\n", CLI_ANSI_INFO, "Oversampling humidity", CLI_ANSI_RESET, static_cast<unsigned int>(s.i2cEnvBmeOsrsH));
    Serial.printf("  %s%-28s%s %u\n", CLI_ANSI_INFO, "Filter", CLI_ANSI_RESET, static_cast<unsigned int>(s.i2cEnvBmeFilter));
    Serial.printf("  %s%-28s%s %u\n", CLI_ANSI_INFO, "Standby", CLI_ANSI_RESET, static_cast<unsigned int>(s.i2cEnvBmeStandby));
  };

  auto printEnvShtSummary = [&](const RuntimeSettings& s) {
    Serial.printf("%s[ENV SHT3x Config]%s\n", CLI_ANSI_WARN, CLI_ANSI_RESET);
    Serial.printf("  %s%-28s%s %u\n", CLI_ANSI_INFO, "Mode", CLI_ANSI_RESET, static_cast<unsigned int>(s.i2cEnvShtMode));
    Serial.printf("  %s%-28s%s %u\n", CLI_ANSI_INFO, "Repeatability", CLI_ANSI_RESET, static_cast<unsigned int>(s.i2cEnvShtRepeatability));
    Serial.printf("  %s%-28s%s %u\n", CLI_ANSI_INFO, "Periodic rate", CLI_ANSI_RESET, static_cast<unsigned int>(s.i2cEnvShtPeriodicRate));
    Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Clock stretching", CLI_ANSI_RESET, s.i2cEnvShtClockStretching ? "on" : "off");
    Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Low VDD", CLI_ANSI_RESET, s.i2cEnvShtLowVdd ? "on" : "off");
    Serial.println();
    Serial.printf("%s[ENV SHT3x Recovery]%s\n", CLI_ANSI_WARN, CLI_ANSI_RESET);
    Serial.printf("  %s%-28s%s %u ms\n", CLI_ANSI_INFO, "Command delay", CLI_ANSI_RESET, static_cast<unsigned int>(s.i2cEnvShtCommandDelayMs));
    Serial.printf("  %s%-28s%s %lu ms\n", CLI_ANSI_INFO, "Not-ready timeout", CLI_ANSI_RESET, static_cast<unsigned long>(s.i2cEnvShtNotReadyTimeoutMs));
    Serial.printf("  %s%-28s%s %lu ms\n", CLI_ANSI_INFO, "Fetch margin", CLI_ANSI_RESET, static_cast<unsigned long>(s.i2cEnvShtPeriodicFetchMarginMs));
    Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Allow GC reset", CLI_ANSI_RESET, s.i2cEnvShtAllowGeneralCallReset ? "on" : "off");
    Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Bus reset", CLI_ANSI_RESET, s.i2cEnvShtRecoverUseBusReset ? "on" : "off");
    Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Soft reset", CLI_ANSI_RESET, s.i2cEnvShtRecoverUseSoftReset ? "on" : "off");
    Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Hard reset", CLI_ANSI_RESET, s.i2cEnvShtRecoverUseHardReset ? "on" : "off");
  };

  auto printRtcSummary = [&](const RuntimeSettings& s) {
    Serial.printf("%s[RTC Config]%s\n", CLI_ANSI_WARN, CLI_ANSI_RESET);
    Serial.printf("  %s%-28s%s %lu ms\n", CLI_ANSI_INFO, "Poll interval", CLI_ANSI_RESET, static_cast<unsigned long>(s.i2cRtcPollMs));
    Serial.printf("  %s%-28s%s 0x%02X\n", CLI_ANSI_INFO, "Address", CLI_ANSI_RESET, static_cast<unsigned int>(s.i2cRtcAddress));
    Serial.printf("  %s%-28s%s %u\n", CLI_ANSI_INFO, "Backup mode", CLI_ANSI_RESET, static_cast<unsigned int>(s.i2cRtcBackupMode));
    Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "EEPROM writes", CLI_ANSI_RESET, s.i2cRtcEnableEepromWrites ? "on" : "off");
    Serial.printf("  %s%-28s%s %lu ms\n", CLI_ANSI_INFO, "EEPROM timeout", CLI_ANSI_RESET, static_cast<unsigned long>(s.i2cRtcEepromTimeoutMs));
    Serial.printf("  %s%-28s%s %u\n", CLI_ANSI_INFO, "Offline threshold", CLI_ANSI_RESET, static_cast<unsigned int>(s.i2cRtcOfflineThreshold));
  };

  auto printLidarSummary = [&](const RuntimeSettings& s) {
    const HardwareSettings& hw = _app.getConfig();
    Serial.printf("%s[TF-Luna Config]%s\n", CLI_ANSI_WARN, CLI_ANSI_RESET);
    Serial.printf("  %s%-28s%s %lu ms\n", CLI_ANSI_INFO, "UART service", CLI_ANSI_RESET, static_cast<unsigned long>(s.lidarServiceMs));
    Serial.printf("  %s%-28s%s %u\n", CLI_ANSI_INFO, "Min strength", CLI_ANSI_RESET, static_cast<unsigned int>(s.lidarMinStrength));
    Serial.printf("  %s%-28s%s %u cm\n", CLI_ANSI_INFO, "Max distance", CLI_ANSI_RESET, static_cast<unsigned int>(s.lidarMaxDistanceCm));
    Serial.printf("  %s%-28s%s %lu ms\n", CLI_ANSI_INFO, "Frame stale", CLI_ANSI_RESET, static_cast<unsigned long>(s.lidarFrameStaleMs));
    Serial.printf("  %s%-28s%s %lu ms\n", CLI_ANSI_INFO, "Serial summary", CLI_ANSI_RESET, static_cast<unsigned long>(s.serialPrintIntervalMs));
    Serial.println();
    Serial.printf("%s[TF-Luna Wiring]%s\n", CLI_ANSI_WARN, CLI_ANSI_RESET);
    Serial.printf("  %s%-28s%s GPIO%d\n", CLI_ANSI_INFO, "ESP32 RX", CLI_ANSI_RESET, hw.lidarRx);
    Serial.printf("  %s%-28s%s GPIO%d\n", CLI_ANSI_INFO, "ESP32 TX", CLI_ANSI_RESET, hw.lidarTx);
    Serial.printf("  %s%-28s%s UART%u\n", CLI_ANSI_INFO, "UART index", CLI_ANSI_RESET, static_cast<unsigned int>(hw.lidarUartIndex));
    Serial.printf("  %s%-28s%s sensor TX -> ESP RX, sensor RX -> ESP TX\n", CLI_ANSI_INFO, "Mapping", CLI_ANSI_RESET);
  };

  // Core discovery and monitoring commands.
  if (strcmp(tokens[0], "help") == 0 || strcmp(tokens[0], "?") == 0) {
    if (argc == 1) {
      printHelp();
      return;
    }
    if (argc == 2) {
      printHelp(tokens[1]);
      return;
    }
    printUsageWithHint("help [topic]", "topics");
    return;
  }

  if (strcmp(tokens[0], "version") == 0 || strcmp(tokens[0], "ver") == 0) {
    if (argc != 1) {
      printUsageWithHint("version", "system");
      return;
    }
    printVersionInfo();
    return;
  }

  if (strcmp(tokens[0], "status") == 0) {
    if (argc != 1) {
      printUsageWithHint("status", "status");
      return;
    }
    printStatus();
    return;
  }

  if (strcmp(tokens[0], "devices") == 0) {
    if (argc != 1) {
      printUsageWithHint("devices", "devices");
      return;
    }
    printDevices();
    return;
  }

  if (strcmp(tokens[0], "diag") == 0 || strcmp(tokens[0], "doctor") == 0) {
    if (argc == 1) {
      printDiagnostics("all");
      return;
    }
    if (argc == 2) {
      printDiagnostics(tokens[1]);
      return;
    }
    printUsageWithHint("diag [all|lidar|i2c|rtc|env]", "diag");
    return;
  }

  if (strcmp(tokens[0], "preset") == 0) {
    if (argc < 3 || argc > 4) {
      printUsageWithHint("preset <env|i2c|rtc> <profile> [persist]", "preset");
      return;
    }

    bool persist = false;
    if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
      Serial.println("ERR persist must be bool");
      return;
    }
    applyPreset(tokens[1], tokens[2], persist);
    return;
  }

  if (strcmp(tokens[0], "read") == 0) {
    if (argc == 1) {
      printRead("all");
      return;
    }
    if (argc == 2) {
      printRead(tokens[1]);
      return;
    }
    printUsageWithHint("read [all|lidar|env|rtc]", "read");
    return;
  }

  if (strcmp(tokens[0], "ls") == 0) {
    if (argc == 1) {
      printSdList(nullptr);
      return;
    }
    if (argc == 2) {
      printSdList(tokens[1]);
      return;
    }
    printUsageWithHint("ls [path]", "sd");
    return;
  }

  if (strcmp(tokens[0], "sample") == 0) {
    size_t count = CLI_MAX_SAMPLE_PRINT;
    if (argc == 2 && !clampPrintCount(tokens[1], CLI_MAX_SAMPLE_PRINT, count)) {
      printUsageWithHint("sample [count]", "sample");
      Serial.printf("note: count must be 1..%lu\n", static_cast<unsigned long>(CLI_MAX_SAMPLE_PRINT));
      return;
    }
    if (argc > 2) {
      printUsageWithHint("sample [count]", "sample");
      Serial.printf("note: count must be 1..%lu\n", static_cast<unsigned long>(CLI_MAX_SAMPLE_PRINT));
      return;
    }
    printSamples(count);
    return;
  }

  if (strcmp(tokens[0], "events") == 0) {
    size_t count = CLI_MAX_EVENT_PRINT;
    if (argc == 2 && !clampPrintCount(tokens[1], CLI_MAX_EVENT_PRINT, count)) {
      printUsageWithHint("events [count]", "events");
      Serial.printf("note: count must be 1..%lu\n", static_cast<unsigned long>(CLI_MAX_EVENT_PRINT));
      return;
    }
    if (argc > 2) {
      printUsageWithHint("events [count]", "events");
      Serial.printf("note: count must be 1..%lu\n", static_cast<unsigned long>(CLI_MAX_EVENT_PRINT));
      return;
    }
    printEvents(count);
    return;
  }

  // Generic settings/config access commands.
  if (strcmp(tokens[0], "settings") == 0) {
    if (argc == 1) {
      printSettings("all");
      return;
    }
    if (strcmp(tokens[1], "show") != 0) {
      printUsageWithHint("settings show [section]", "settings");
      return;
    }
    if (argc == 2) {
      printSettings("all");
      return;
    }
    if (argc == 3) {
      printSettings(tokens[2]);
      return;
    }
    printUsageWithHint("settings show [section]", "settings");
    return;
  }

  if (strcmp(tokens[0], "config") == 0) {
    if (argc == 1) {
      printBootConfig("all");
      return;
    }
    if (strcmp(tokens[1], "show") != 0) {
      printUsageWithHint("config show [hardware|app|all]", "config");
      return;
    }
    if (argc == 2) {
      printBootConfig("all");
      return;
    }
    if (argc == 3) {
      printBootConfig(tokens[2]);
      return;
    }
    printUsageWithHint("config show [hardware|app|all]", "config");
    return;
  }

  if (strcmp(tokens[0], "device") == 0) {
    if (argc == 2 && strcmp(tokens[1], "list") == 0) {
      printDevices();
      return;
    }
    if (argc == 3 && strcmp(tokens[2], "status") == 0) {
      printDevice(tokens[1]);
      return;
    }
    if (argc == 3 && strcmp(tokens[2], "settings") == 0) {
      const char* normalized = normalizeDeviceName(tokens[1]);
      const char* section = settingsSectionForDevice(tokens[1]);
      if (section != nullptr) {
        printSettings(section);
        if (normalized != nullptr && strcmp(normalized, "leds") == 0) {
          printBootConfig("hardware");
        }
        return;
      }
      if (normalized != nullptr && strcmp(normalized, "button") == 0) {
        printBootConfig("hardware");
        return;
      }
      Serial.printf("ERR no settings section for device: %s\n", tokens[1]);
      return;
    }
    if (argc == 3 && strcmp(tokens[2], "read") == 0) {
      const char* normalized = normalizeDeviceName(tokens[1]);
      if (strcmp(normalized, "lidar") == 0 || strcmp(normalized, "env") == 0 || strcmp(normalized, "rtc") == 0) {
        printRead(normalized);
        return;
      }
      printUsageWithHint("device <env|rtc|lidar|tfluna|co2|e2> read", "device");
      return;
    }
    if (argc == 3 && strcmp(tokens[2], "diag") == 0) {
      const char* normalized = normalizeDeviceName(tokens[1]);
      if (strcmp(normalized, "lidar") == 0) {
        printDevice("lidar");
        printRead("lidar");
        return;
      }
      if (strcmp(normalized, "i2c_bus") == 0) {
        printDiagnostics("i2c");
        return;
      }
      if (strcmp(normalized, "rtc") == 0) {
        printDiagnostics("rtc");
        return;
      }
      if (strcmp(normalized, "env") == 0) {
        printDiagnostics("env");
        return;
      }
      printUsageWithHint("device <lidar|i2c|rtc|env> diag", "device");
      return;
    }
    if (argc == 3 && strcmp(tokens[2], "probe") == 0) {
      const char* normalized = normalizeDeviceName(tokens[1]);
      if (strcmp(normalized, "lidar") == 0) {
        const Status st = _app.enqueueProbeLidarSensor();
        if (!st.ok()) {
          Serial.printf("ERR lidar probe queue failed: %s (%s)\n", errToStr(st.code), st.msg);
          return;
        }
        printOkf("lidar probe queued");
        return;
      }
      if (strcmp(normalized, "i2c_bus") == 0) {
        const Status st = _app.enqueueScanI2cBus();
        if (!st.ok()) {
          Serial.printf("ERR i2c scan queue failed: %s (%s)\n", errToStr(st.code), st.msg);
          return;
        }
        printOkf("i2c scan queued");
        printHint("use 'i2c scan status' after 1-2 seconds");
        return;
      }
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      if (strcmp(normalized, "env") == 0) {
        queueI2cProbeAddress(settings.i2cEnvAddress);
        return;
      }
      if (strcmp(normalized, "rtc") == 0) {
        queueI2cProbeAddress(settings.i2cRtcAddress);
        return;
      }
      if (strcmp(normalized, "display") == 0) {
        queueI2cProbeAddress(settings.i2cDisplayAddress);
        return;
      }
      printUsageWithHint("device <lidar|i2c|env|rtc|display> probe", "device");
      return;
    }
    if (argc == 3 && deviceNameEquals(tokens[1], "lidar") && strcmp(tokens[2], "recover") == 0) {
      const Status st = _app.enqueueRecoverLidarSensor();
      if (!st.ok()) {
        Serial.printf("ERR lidar recover queue failed: %s (%s)\n", errToStr(st.code), st.msg);
        return;
      }
      printOkf("lidar recover queued");
      return;
    }
    if (argc == 3 && deviceNameEquals(tokens[1], "i2c_bus") && strcmp(tokens[2], "recover") == 0) {
      queueI2cRecover();
      return;
    }
    if (argc == 3 &&
        (deviceNameEquals(tokens[1], "i2c_bus") || deviceNameEquals(tokens[1], "rtc") ||
         deviceNameEquals(tokens[1], "env")) &&
        strcmp(tokens[2], "recover") == 0) {
      queueI2cRecover();
      return;
    }
    if (argc == 3 && deviceNameEquals(tokens[1], "sd") && strcmp(tokens[2], "remount") == 0) {
      queueSdRemount();
      return;
    }
    if (argc == 3 && deviceNameEquals(tokens[1], "sd") && strcmp(tokens[2], "ls") == 0) {
      printSdList(nullptr);
      return;
    }
    if (argc == 4 && deviceNameEquals(tokens[1], "sd") && strcmp(tokens[2], "ls") == 0) {
      printSdList(tokens[3]);
      return;
    }
    if ((argc == 3 || argc == 4) && deviceNameEquals(tokens[1], "wifi")) {
      bool enable = false;
      if (strcmp(tokens[2], "on") == 0) {
        enable = true;
      } else if (strcmp(tokens[2], "off") == 0) {
        enable = false;
      } else {
        printUsageWithHint("device wifi <on|off> [persist]", "device");
        return;
      }
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      queueWifiSet(enable, persist);
      return;
    }
    printUsageWithHint("device <name> <status|settings|read|diag|probe|recover|...>", "device");
    return;
  }

  if (strcmp(tokens[0], "set") == 0) {
    if (argc == 2 && strcmp(tokens[1], "list") == 0) {
      printSettableKeys(nullptr);
      return;
    }
    if (argc == 3 && strcmp(tokens[1], "list") == 0) {
      printSettableKeys(tokens[2]);
      return;
    }

    const bool grouped = (argc >= 4 && isSettingGroup(tokens[1]));
    const char* key = nullptr;
    const char* value = nullptr;
    const char* persistToken = nullptr;
    char keyBuffer[48] = {};
    const char* errorMsg = "";

    if (grouped) {
      if (argc < 4 || argc > 5) {
        printUsageWithHint("set <group> <key> <value> [persist]", "settings");
        return;
      }
      if (!resolveGroupedSettingKey(tokens[1], tokens[2], keyBuffer, sizeof(keyBuffer), key, errorMsg)) {
        Serial.printf("ERR set key mapping failed: %s\n", errorMsg);
        return;
      }
      value = tokens[3];
      if (argc == 5) {
        persistToken = tokens[4];
      }
    } else {
      if (argc < 3 || argc > 4) {
        printUsageWithHint("set <key> <value> [persist]", "settings");
        return;
      }
      key = tokens[1];
      value = tokens[2];
      if (argc == 4) {
        persistToken = tokens[3];
      }
    }

    bool persist = false;
    if (persistToken != nullptr && !parseBoolToken(persistToken, persist)) {
      Serial.println("ERR persist must be bool");
      return;
    }

    RuntimeSettings settings{};
    if (!_app.tryGetSettingsSnapshot(settings)) {
      Serial.println("ERR state busy");
      return;
    }

    if (!applySettingByKey(settings, key, value, errorMsg)) {
      Serial.printf("ERR set failed: %s\n", errorMsg);
      return;
    }

    const Status validation = settings.validate();
    if (!validation.ok()) {
      Serial.printf("ERR settings invalid: %s (%s)\n", errToStr(validation.code), validation.msg);
      return;
    }

    queueSettingsUpdate(settings, persist);
    return;
  }

  if (strcmp(tokens[0], "factory_reset") == 0) {
    if (argc > 2) {
      printUsageWithHint("factory_reset [persist]", "factory_reset");
      return;
    }

    bool persist = false;
    if (argc == 2 && !parseBoolToken(tokens[1], persist)) {
      Serial.println("ERR persist must be bool");
      return;
    }

    RuntimeSettings defaults{};
    defaults.restoreDefaults();
    queueSettingsUpdate(defaults, persist);
    return;
  }

  // Device-specific command families.
  if (strcmp(tokens[0], "env") == 0) {
    if (argc == 1) {
      printSettings("env");
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "status") == 0) {
      printDevice("env");
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "read") == 0) {
      printRead("env");
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "settings") == 0) {
      printSettings("env");
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "model") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Model", CLI_ANSI_RESET, envModelHint(settings.i2cEnvAddress));
      Serial.printf("  %s%-28s%s 0x%02X\n", CLI_ANSI_INFO, "Address", CLI_ANSI_RESET, static_cast<unsigned int>(settings.i2cEnvAddress));
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "profile") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %lu ms\n", CLI_ANSI_INFO, "Poll interval", CLI_ANSI_RESET, static_cast<unsigned long>(settings.i2cEnvPollMs));
      Serial.printf("  %s%-28s%s %lu ms\n", CLI_ANSI_INFO, "Conversion wait", CLI_ANSI_RESET, static_cast<unsigned long>(settings.i2cEnvConversionWaitMs));
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "poll") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %lu ms\n", CLI_ANSI_INFO, "Poll interval", CLI_ANSI_RESET, static_cast<unsigned long>(settings.i2cEnvPollMs));
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "conversion_wait") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %lu ms\n", CLI_ANSI_INFO, "Conversion wait", CLI_ANSI_RESET, static_cast<unsigned long>(settings.i2cEnvConversionWaitMs));
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "address") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s 0x%02X\n", CLI_ANSI_INFO, "Address", CLI_ANSI_RESET, static_cast<unsigned int>(settings.i2cEnvAddress));
      Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Model", CLI_ANSI_RESET, envModelHint(settings.i2cEnvAddress));
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "probe") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      queueI2cProbeAddress(settings.i2cEnvAddress);
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "bme") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      printEnvBmeSummary(settings);
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "sht") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      printEnvShtSummary(settings);
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "model") == 0) {
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      applyPreset("env", tokens[2], persist);
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "profile") == 0) {
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      RuntimeSettings settings{};
      if (!_app.tryGetSettingsSnapshot(settings)) {
        Serial.println("ERR state busy");
        return;
      }
      const bool currentLooksSht = isSht3xAddress(settings.i2cEnvAddress);
      if (strcmp(tokens[2], "safe") == 0) {
        settings.i2cEnvConversionWaitMs = currentLooksSht ? 25U : 40U;
        settings.i2cEnvPollMs = 10000U;
      } else if (strcmp(tokens[2], "balanced") == 0 || strcmp(tokens[2], "normal") == 0) {
        settings.i2cEnvConversionWaitMs = currentLooksSht ? 20U : 30U;
        settings.i2cEnvPollMs = 5000U;
      } else if (strcmp(tokens[2], "fast") == 0) {
        settings.i2cEnvConversionWaitMs = currentLooksSht ? 10U : 20U;
        settings.i2cEnvPollMs = 1000U;
      } else {
        Serial.println("ERR env profile must be safe|balanced|fast");
        return;
      }
      const Status validation = settings.validate();
      if (!validation.ok()) {
        Serial.printf("ERR settings invalid: %s (%s)\n", errToStr(validation.code), validation.msg);
        return;
      }
      queueSettingsUpdate(settings, persist);
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "poll") == 0) {
      uint32_t value = 0;
      if (!parseU32Token(tokens[2], value)) {
        Serial.println("ERR poll must be u32 ms");
        return;
      }
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      RuntimeSettings settings{};
      if (!_app.tryGetSettingsSnapshot(settings)) {
        Serial.println("ERR state busy");
        return;
      }
      settings.i2cEnvPollMs = value;
      const Status validation = settings.validate();
      if (!validation.ok()) {
        Serial.printf("ERR settings invalid: %s (%s)\n", errToStr(validation.code), validation.msg);
        return;
      }
      queueSettingsUpdate(settings, persist);
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "conversion_wait") == 0) {
      uint32_t value = 0;
      if (!parseU32Token(tokens[2], value)) {
        Serial.println("ERR conversion_wait must be u32 ms");
        return;
      }
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      RuntimeSettings settings{};
      if (!_app.tryGetSettingsSnapshot(settings)) {
        Serial.println("ERR state busy");
        return;
      }
      settings.i2cEnvConversionWaitMs = value;
      const Status validation = settings.validate();
      if (!validation.ok()) {
        Serial.printf("ERR settings invalid: %s (%s)\n", errToStr(validation.code), validation.msg);
        return;
      }
      queueSettingsUpdate(settings, persist);
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "address") == 0) {
      uint8_t address = 0;
      if (!parseI2cAddressToken(tokens[2], address)) {
        Serial.println("ERR address must be 0x01..0x7F");
        return;
      }
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      RuntimeSettings settings{};
      if (!_app.tryGetSettingsSnapshot(settings)) {
        Serial.println("ERR state busy");
        return;
      }
      settings.i2cEnvAddress = address;
      const Status validation = settings.validate();
      if (!validation.ok()) {
        Serial.printf("ERR settings invalid: %s (%s)\n", errToStr(validation.code), validation.msg);
        return;
      }
      queueSettingsUpdate(settings, persist);
      return;
    }
    if ((argc == 4 || argc == 5) && strcmp(tokens[1], "bme") == 0 && strcmp(tokens[2], "mode") == 0) {
      bool persist = false;
      if (argc == 5 && !parseBoolToken(tokens[4], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("env", "bme_mode", tokens[3], persist);
      return;
    }
    if (argc == 3 && strcmp(tokens[1], "bme") == 0 && strcmp(tokens[2], "mode") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %u\n", CLI_ANSI_INFO, "BME mode", CLI_ANSI_RESET, static_cast<unsigned int>(settings.i2cEnvBmeMode));
      return;
    }
    if ((argc == 6 || argc == 7) && strcmp(tokens[1], "bme") == 0 && strcmp(tokens[2], "osrs") == 0) {
      bool persist = false;
      if (argc == 7 && !parseBoolToken(tokens[6], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      RuntimeSettings settings{};
      if (!_app.tryGetSettingsSnapshot(settings)) {
        Serial.println("ERR state busy");
        return;
      }
      const char* errorMsg = "";
      if (!applySettingByKey(settings, "i2c_env_bme_osrs_t", tokens[3], errorMsg) ||
          !applySettingByKey(settings, "i2c_env_bme_osrs_p", tokens[4], errorMsg) ||
          !applySettingByKey(settings, "i2c_env_bme_osrs_h", tokens[5], errorMsg)) {
        Serial.printf("ERR set failed: %s\n", errorMsg);
        return;
      }
      const Status validation = settings.validate();
      if (!validation.ok()) {
        Serial.printf("ERR settings invalid: %s (%s)\n", errToStr(validation.code), validation.msg);
        return;
      }
      queueSettingsUpdate(settings, persist);
      return;
    }
    if (argc == 3 && strcmp(tokens[1], "bme") == 0 && strcmp(tokens[2], "osrs") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %u\n", CLI_ANSI_INFO, "Oversampling temp", CLI_ANSI_RESET, static_cast<unsigned int>(settings.i2cEnvBmeOsrsT));
      Serial.printf("  %s%-28s%s %u\n", CLI_ANSI_INFO, "Oversampling pressure", CLI_ANSI_RESET, static_cast<unsigned int>(settings.i2cEnvBmeOsrsP));
      Serial.printf("  %s%-28s%s %u\n", CLI_ANSI_INFO, "Oversampling humidity", CLI_ANSI_RESET, static_cast<unsigned int>(settings.i2cEnvBmeOsrsH));
      return;
    }
    if ((argc == 4 || argc == 5) && strcmp(tokens[1], "bme") == 0 && strcmp(tokens[2], "filter") == 0) {
      bool persist = false;
      if (argc == 5 && !parseBoolToken(tokens[4], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("env", "bme_filter", tokens[3], persist);
      return;
    }
    if (argc == 3 && strcmp(tokens[1], "bme") == 0 && strcmp(tokens[2], "filter") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %u\n", CLI_ANSI_INFO, "BME filter", CLI_ANSI_RESET, static_cast<unsigned int>(settings.i2cEnvBmeFilter));
      return;
    }
    if ((argc == 4 || argc == 5) && strcmp(tokens[1], "bme") == 0 && strcmp(tokens[2], "standby") == 0) {
      bool persist = false;
      if (argc == 5 && !parseBoolToken(tokens[4], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("env", "bme_standby", tokens[3], persist);
      return;
    }
    if (argc == 3 && strcmp(tokens[1], "bme") == 0 && strcmp(tokens[2], "standby") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %u\n", CLI_ANSI_INFO, "BME standby", CLI_ANSI_RESET, static_cast<unsigned int>(settings.i2cEnvBmeStandby));
      return;
    }
    if ((argc == 4 || argc == 5) && strcmp(tokens[1], "sht") == 0 && strcmp(tokens[2], "mode") == 0) {
      bool persist = false;
      if (argc == 5 && !parseBoolToken(tokens[4], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("env", "sht_mode", tokens[3], persist);
      return;
    }
    if (argc == 3 && strcmp(tokens[1], "sht") == 0 && strcmp(tokens[2], "mode") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %u\n", CLI_ANSI_INFO, "SHT mode", CLI_ANSI_RESET, static_cast<unsigned int>(settings.i2cEnvShtMode));
      return;
    }
    if ((argc == 4 || argc == 5) && strcmp(tokens[1], "sht") == 0 && strcmp(tokens[2], "repeatability") == 0) {
      bool persist = false;
      if (argc == 5 && !parseBoolToken(tokens[4], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("env", "sht_repeatability", tokens[3], persist);
      return;
    }
    if (argc == 3 && strcmp(tokens[1], "sht") == 0 && strcmp(tokens[2], "repeatability") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %u\n", CLI_ANSI_INFO, "SHT repeatability", CLI_ANSI_RESET, static_cast<unsigned int>(settings.i2cEnvShtRepeatability));
      return;
    }
    if ((argc == 4 || argc == 5) && strcmp(tokens[1], "sht") == 0 &&
        (strcmp(tokens[2], "rate") == 0 || strcmp(tokens[2], "periodic_rate") == 0)) {
      bool persist = false;
      if (argc == 5 && !parseBoolToken(tokens[4], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("env", "sht_periodic_rate", tokens[3], persist);
      return;
    }
    if (argc == 3 && strcmp(tokens[1], "sht") == 0 &&
        (strcmp(tokens[2], "rate") == 0 || strcmp(tokens[2], "periodic_rate") == 0)) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %u\n", CLI_ANSI_INFO, "SHT periodic rate", CLI_ANSI_RESET, static_cast<unsigned int>(settings.i2cEnvShtPeriodicRate));
      return;
    }
    if ((argc == 4 || argc == 5) && strcmp(tokens[1], "sht") == 0 &&
        (strcmp(tokens[2], "stretch") == 0 || strcmp(tokens[2], "clock_stretching") == 0)) {
      bool persist = false;
      if (argc == 5 && !parseBoolToken(tokens[4], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("env", "sht_clock_stretching", tokens[3], persist);
      return;
    }
    if (argc == 3 && strcmp(tokens[1], "sht") == 0 &&
        (strcmp(tokens[2], "stretch") == 0 || strcmp(tokens[2], "clock_stretching") == 0)) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "SHT clock stretching", CLI_ANSI_RESET, settings.i2cEnvShtClockStretching ? "on" : "off");
      return;
    }
    if ((argc == 4 || argc == 5) && strcmp(tokens[1], "sht") == 0 && strcmp(tokens[2], "low_vdd") == 0) {
      bool persist = false;
      if (argc == 5 && !parseBoolToken(tokens[4], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("env", "sht_low_vdd", tokens[3], persist);
      return;
    }
    if (argc == 3 && strcmp(tokens[1], "sht") == 0 && strcmp(tokens[2], "low_vdd") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "SHT low VDD", CLI_ANSI_RESET, settings.i2cEnvShtLowVdd ? "on" : "off");
      return;
    }
    if ((argc == 4 || argc == 5) && strcmp(tokens[1], "sht") == 0 &&
        (strcmp(tokens[2], "command_delay") == 0 || strcmp(tokens[2], "command_delay_ms") == 0)) {
      bool persist = false;
      if (argc == 5 && !parseBoolToken(tokens[4], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("env", "sht_command_delay_ms", tokens[3], persist);
      return;
    }
    if (argc == 3 && strcmp(tokens[1], "sht") == 0 &&
        (strcmp(tokens[2], "command_delay") == 0 || strcmp(tokens[2], "command_delay_ms") == 0)) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %u ms\n", CLI_ANSI_INFO, "SHT command delay", CLI_ANSI_RESET, static_cast<unsigned int>(settings.i2cEnvShtCommandDelayMs));
      return;
    }
    if ((argc == 4 || argc == 5) && strcmp(tokens[1], "sht") == 0 &&
        (strcmp(tokens[2], "not_ready_timeout") == 0 || strcmp(tokens[2], "not_ready_timeout_ms") == 0)) {
      bool persist = false;
      if (argc == 5 && !parseBoolToken(tokens[4], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("env", "sht_not_ready_timeout_ms", tokens[3], persist);
      return;
    }
    if (argc == 3 && strcmp(tokens[1], "sht") == 0 &&
        (strcmp(tokens[2], "not_ready_timeout") == 0 || strcmp(tokens[2], "not_ready_timeout_ms") == 0)) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %lu ms\n", CLI_ANSI_INFO, "SHT not-ready timeout", CLI_ANSI_RESET,
                    static_cast<unsigned long>(settings.i2cEnvShtNotReadyTimeoutMs));
      return;
    }
    if ((argc == 4 || argc == 5) && strcmp(tokens[1], "sht") == 0 &&
        (strcmp(tokens[2], "fetch_margin") == 0 || strcmp(tokens[2], "periodic_fetch_margin_ms") == 0)) {
      bool persist = false;
      if (argc == 5 && !parseBoolToken(tokens[4], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("env", "sht_periodic_fetch_margin_ms", tokens[3], persist);
      return;
    }
    if (argc == 3 && strcmp(tokens[1], "sht") == 0 &&
        (strcmp(tokens[2], "fetch_margin") == 0 || strcmp(tokens[2], "periodic_fetch_margin_ms") == 0)) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %lu ms\n", CLI_ANSI_INFO, "SHT fetch margin", CLI_ANSI_RESET,
                    static_cast<unsigned long>(settings.i2cEnvShtPeriodicFetchMarginMs));
      return;
    }
    if ((argc == 4 || argc == 5) && strcmp(tokens[1], "sht") == 0 &&
        (strcmp(tokens[2], "allow_gc_reset") == 0 || strcmp(tokens[2], "allow_general_call_reset") == 0)) {
      bool persist = false;
      if (argc == 5 && !parseBoolToken(tokens[4], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("env", "sht_allow_general_call_reset", tokens[3], persist);
      return;
    }
    if (argc == 3 && strcmp(tokens[1], "sht") == 0 &&
        (strcmp(tokens[2], "allow_gc_reset") == 0 || strcmp(tokens[2], "allow_general_call_reset") == 0)) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "SHT allow GC reset", CLI_ANSI_RESET,
                    settings.i2cEnvShtAllowGeneralCallReset ? "on" : "off");
      return;
    }
    if ((argc == 4 || argc == 5) && strcmp(tokens[1], "sht") == 0 && strcmp(tokens[2], "recover_bus_reset") == 0) {
      bool persist = false;
      if (argc == 5 && !parseBoolToken(tokens[4], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("env", "sht_recover_use_bus_reset", tokens[3], persist);
      return;
    }
    if (argc == 3 && strcmp(tokens[1], "sht") == 0 && strcmp(tokens[2], "recover_bus_reset") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "SHT bus reset", CLI_ANSI_RESET,
                    settings.i2cEnvShtRecoverUseBusReset ? "on" : "off");
      return;
    }
    if ((argc == 4 || argc == 5) && strcmp(tokens[1], "sht") == 0 && strcmp(tokens[2], "recover_soft_reset") == 0) {
      bool persist = false;
      if (argc == 5 && !parseBoolToken(tokens[4], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("env", "sht_recover_use_soft_reset", tokens[3], persist);
      return;
    }
    if (argc == 3 && strcmp(tokens[1], "sht") == 0 && strcmp(tokens[2], "recover_soft_reset") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "SHT soft reset", CLI_ANSI_RESET,
                    settings.i2cEnvShtRecoverUseSoftReset ? "on" : "off");
      return;
    }
    if ((argc == 4 || argc == 5) && strcmp(tokens[1], "sht") == 0 && strcmp(tokens[2], "recover_hard_reset") == 0) {
      bool persist = false;
      if (argc == 5 && !parseBoolToken(tokens[4], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("env", "sht_recover_use_hard_reset", tokens[3], persist);
      return;
    }
    if (argc == 3 && strcmp(tokens[1], "sht") == 0 && strcmp(tokens[2], "recover_hard_reset") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "SHT hard reset", CLI_ANSI_RESET,
                    settings.i2cEnvShtRecoverUseHardReset ? "on" : "off");
      return;
    }
    printUsageWithHint("env <status|read|settings|model|profile|poll|conversion_wait|address|probe|bme|sht|...>", "env");
    return;
  }

  if (strcmp(tokens[0], "rtc") == 0) {
    if (argc == 1) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      printRtcSummary(settings);
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "status") == 0) {
      printDevice("rtc");
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "read") == 0) {
      printRead("rtc");
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "diag") == 0) {
      printRtcDiagnostics();
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "settings") == 0) {
      printSettings("rtc");
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "probe") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      queueI2cProbeAddress(settings.i2cRtcAddress);
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "recover") == 0) {
      queueI2cRecover();
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "poll") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %lu ms\n", CLI_ANSI_INFO, "Poll interval", CLI_ANSI_RESET, static_cast<unsigned long>(settings.i2cRtcPollMs));
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "backup") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %u\n", CLI_ANSI_INFO, "Backup mode", CLI_ANSI_RESET, static_cast<unsigned int>(settings.i2cRtcBackupMode));
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "eeprom_writes") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "EEPROM writes", CLI_ANSI_RESET, settings.i2cRtcEnableEepromWrites ? "on" : "off");
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "eeprom_timeout") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %lu ms\n", CLI_ANSI_INFO, "EEPROM timeout", CLI_ANSI_RESET, static_cast<unsigned long>(settings.i2cRtcEepromTimeoutMs));
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "offline") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %u\n", CLI_ANSI_INFO, "Offline threshold", CLI_ANSI_RESET, static_cast<unsigned int>(settings.i2cRtcOfflineThreshold));
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "poll") == 0) {
      uint32_t value = 0;
      if (!parseU32Token(tokens[2], value)) {
        Serial.println("ERR poll must be u32 ms");
        return;
      }
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      RuntimeSettings settings{};
      if (!_app.tryGetSettingsSnapshot(settings)) {
        Serial.println("ERR state busy");
        return;
      }
      settings.i2cRtcPollMs = value;
      const Status validation = settings.validate();
      if (!validation.ok()) {
        Serial.printf("ERR settings invalid: %s (%s)\n", errToStr(validation.code), validation.msg);
        return;
      }
      queueSettingsUpdate(settings, persist);
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "backup") == 0) {
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("rtc", "backup_mode", tokens[2], persist);
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "eeprom_writes") == 0) {
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("rtc", "eeprom_writes", tokens[2], persist);
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "eeprom_timeout") == 0) {
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("rtc", "eeprom_timeout_ms", tokens[2], persist);
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "offline") == 0) {
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("rtc", "offline_threshold", tokens[2], persist);
      return;
    }
    if (argc == 3 && strcmp(tokens[1], "set_unix") == 0) {
      uint32_t unixSeconds = 0;
      if (!parseU32Token(tokens[2], unixSeconds)) {
        Serial.println("ERR epoch must be u32 seconds");
        return;
      }
      RtcTime time{};
      fromUnixSeconds(unixSeconds, time);
      if (!time.valid) {
        Serial.println("ERR epoch conversion failed");
        return;
      }
      const Status st = _app.enqueueSetRtcTime(time);
      if (!st.ok()) {
        Serial.printf("ERR rtc set queue failed: %s (%s)\n", errToStr(st.code), st.msg);
        return;
      }
      printOkf("rtc set queued");
      return;
    }
    if (argc != 8 || strcmp(tokens[1], "set") != 0) {
      printUsageWithHint("rtc <status|read|diag|settings|probe|recover|poll|backup|eeprom_writes|eeprom_timeout|offline|set|set_unix>", "rtc");
      return;
    }

    uint16_t year = 0;
    uint8_t month = 0;
    uint8_t day = 0;
    uint8_t hour = 0;
    uint8_t minute = 0;
    uint8_t second = 0;
    if (!parseU16Token(tokens[2], year) ||
        !parseU8Token(tokens[3], month) ||
        !parseU8Token(tokens[4], day) ||
        !parseU8Token(tokens[5], hour) ||
        !parseU8Token(tokens[6], minute) ||
        !parseU8Token(tokens[7], second)) {
      Serial.println("ERR rtc numeric parse failed");
      return;
    }

    if (year < 2000U || year > 2099U || month < 1U || month > 12U || day < 1U || day > 31U ||
        hour > 23U || minute > 59U || second > 59U) {
      Serial.println("ERR rtc values out of range");
      return;
    }

    RtcTime time{};
    time.year = year;
    time.month = month;
    time.day = day;
    time.hour = hour;
    time.minute = minute;
    time.second = second;
    time.valid = true;

    const Status st = _app.enqueueSetRtcTime(time);
    if (!st.ok()) {
      Serial.printf("ERR rtc set queue failed: %s (%s)\n", errToStr(st.code), st.msg);
      return;
    }
    printOkf("rtc set queued");
    return;
  }

  if (strcmp(tokens[0], "lidar") == 0 || strcmp(tokens[0], "tfluna") == 0 ||
      strcmp(tokens[0], "co2") == 0 || strcmp(tokens[0], "e2") == 0) {
    if (argc == 1) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      printLidarSummary(settings);
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "status") == 0) {
      printDevice("lidar");
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "read") == 0) {
      printRead("lidar");
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "settings") == 0) {
      printSettings("lidar");
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "pins") == 0) {
      const HardwareSettings& hw = _app.getConfig();
      Serial.printf("  %s%-28s%s GPIO%d\n", CLI_ANSI_INFO, "ESP32 RX", CLI_ANSI_RESET, hw.lidarRx);
      Serial.printf("  %s%-28s%s GPIO%d\n", CLI_ANSI_INFO, "ESP32 TX", CLI_ANSI_RESET, hw.lidarTx);
      Serial.printf("  %s%-28s%s UART%u\n", CLI_ANSI_INFO, "UART index", CLI_ANSI_RESET, static_cast<unsigned int>(hw.lidarUartIndex));
      Serial.printf("  %s%-28s%s sensor TX -> ESP RX, sensor RX -> ESP TX\n", CLI_ANSI_INFO, "Mapping", CLI_ANSI_RESET);
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "recover") == 0) {
      const Status st = _app.enqueueRecoverLidarSensor();
      if (!st.ok()) {
        Serial.printf("ERR lidar recover queue failed: %s (%s)\n", errToStr(st.code), st.msg);
        return;
      }
      printOkf("lidar recover queued");
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "probe") == 0) {
      const Status st = _app.enqueueProbeLidarSensor();
      if (!st.ok()) {
        Serial.printf("ERR lidar probe queue failed: %s (%s)\n", errToStr(st.code), st.msg);
        return;
      }
      printOkf("lidar probe queued");
      printHint("this uses the GitHub TFMini-Plus library path once, for diagnostics");
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "service") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %lu ms\n", CLI_ANSI_INFO, "UART service", CLI_ANSI_RESET, static_cast<unsigned long>(settings.lidarServiceMs));
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "min_strength") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %u\n", CLI_ANSI_INFO, "Minimum strength", CLI_ANSI_RESET, static_cast<unsigned int>(settings.lidarMinStrength));
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "max_distance") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %u cm\n", CLI_ANSI_INFO, "Maximum distance", CLI_ANSI_RESET, static_cast<unsigned int>(settings.lidarMaxDistanceCm));
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "stale") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %lu ms\n", CLI_ANSI_INFO, "Frame stale", CLI_ANSI_RESET, static_cast<unsigned long>(settings.lidarFrameStaleMs));
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "serial") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %lu ms\n", CLI_ANSI_INFO, "Serial summary", CLI_ANSI_RESET, static_cast<unsigned long>(settings.serialPrintIntervalMs));
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "service") == 0) {
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("lidar", "service_ms", tokens[2], persist);
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "min_strength") == 0) {
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("lidar", "min_strength", tokens[2], persist);
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "max_distance") == 0) {
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("lidar", "max_distance_cm", tokens[2], persist);
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "stale") == 0) {
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("lidar", "frame_stale_ms", tokens[2], persist);
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "serial") == 0) {
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("lidar", "serial_print_interval_ms", tokens[2], persist);
      return;
    }
    printUsageWithHint("lidar <status|read|settings|pins|recover|probe|service|min_strength|max_distance|stale|serial>", "lidar");
    return;
  }

  if (strcmp(tokens[0], "sd") == 0) {
    if (argc == 2 && strcmp(tokens[1], "status") == 0) {
      printDevice("sd");
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "info") == 0) {
      printSdInfo();
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "ls") == 0) {
      printSdList(nullptr);
      return;
    }
    if (argc == 3 && strcmp(tokens[1], "ls") == 0) {
      printSdList(tokens[2]);
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "settings") == 0) {
      printSettings("sd");
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "remount") == 0) {
      queueSdRemount();
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "daily") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("sd daily=%s\n", settings.logDailyEnabled ? "on" : "off");
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "all") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("sd all=%s\n", settings.logAllEnabled ? "on" : "off");
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "all_max") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("sd all_max=%lu B\n", static_cast<unsigned long>(settings.logAllMaxBytes));
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "flush") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("sd flush=%lu ms\n", static_cast<unsigned long>(settings.logFlushMs));
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "io_budget") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("sd io_budget=%lu ms\n", static_cast<unsigned long>(settings.logIoBudgetMs));
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "mount_retry") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("sd mount_retry=%lu ms\n", static_cast<unsigned long>(settings.logMountRetryMs));
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "write_retry") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("sd write_retry=%lu ms\n", static_cast<unsigned long>(settings.logWriteRetryBackoffMs));
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "max_retries") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("sd max_retries=%u\n", static_cast<unsigned int>(settings.logMaxWriteRetries));
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "events_max") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("sd events_max=%lu B\n", static_cast<unsigned long>(settings.logEventsMaxBytes));
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "daily") == 0) {
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("sd", "daily_enabled", tokens[2], persist);
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "all") == 0) {
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("sd", "all_enabled", tokens[2], persist);
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "all_max") == 0) {
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("sd", "all_max_bytes", tokens[2], persist);
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "flush") == 0) {
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("sd", "flush_ms", tokens[2], persist);
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "io_budget") == 0) {
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("sd", "io_budget_ms", tokens[2], persist);
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "mount_retry") == 0) {
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("sd", "mount_retry_ms", tokens[2], persist);
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "write_retry") == 0) {
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("sd", "write_retry_backoff_ms", tokens[2], persist);
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "max_retries") == 0) {
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("sd", "max_write_retries", tokens[2], persist);
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "events_max") == 0) {
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("sd", "events_max_bytes", tokens[2], persist);
      return;
    }
    printUsageWithHint("sd <status|info|ls|settings|remount|daily|all|all_max|flush|io_budget|mount_retry|write_retry|max_retries|events_max>", "sd");
    return;
  }

  if (strcmp(tokens[0], "i2c") == 0) {
    if (argc == 2 && strcmp(tokens[1], "recover") == 0) {
      queueI2cRecover();
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "scan") == 0) {
      const Status st = _app.enqueueScanI2cBus();
      if (!st.ok()) {
        Serial.printf("ERR i2c scan queue failed: %s (%s)\n", errToStr(st.code), st.msg);
        printI2cScan();
        return;
      }
      printOkf("i2c scan queued");
      printHint("use 'i2c scan status' after 1-2 seconds");
      return;
    }
    if (argc == 3 && strcmp(tokens[1], "probe") == 0 && strcmp(tokens[2], "status") == 0) {
      printI2cProbe();
      return;
    }
    if (argc == 3 && strcmp(tokens[1], "probe") == 0) {
      uint8_t address = 0;
      if (!parseI2cAddressToken(tokens[2], address)) {
        Serial.println("ERR address must be 0x01..0x7F");
        return;
      }
      queueI2cProbeAddress(address);
      return;
    }
    if (argc == 3 && strcmp(tokens[1], "scan") == 0 && strcmp(tokens[2], "status") == 0) {
      printI2cScan();
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "freq") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %lu Hz\n", CLI_ANSI_INFO, "Frequency", CLI_ANSI_RESET, static_cast<unsigned long>(settings.i2cFreqHz));
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "timeout") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %lu ms\n", CLI_ANSI_INFO, "Timeout", CLI_ANSI_RESET, static_cast<unsigned long>(settings.i2cOpTimeoutMs));
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "stuck_debounce") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %u ms\n", CLI_ANSI_INFO, "Stuck debounce", CLI_ANSI_RESET, static_cast<unsigned int>(settings.i2cStuckDebounceMs));
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "max_failures") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %u\n", CLI_ANSI_INFO, "Max failures", CLI_ANSI_RESET, static_cast<unsigned int>(settings.i2cMaxConsecutiveFailures));
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "backoff") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %lu ms\n", CLI_ANSI_INFO, "Backoff", CLI_ANSI_RESET, static_cast<unsigned long>(settings.i2cRecoveryBackoffMs));
      Serial.printf("  %s%-28s%s %lu ms\n", CLI_ANSI_INFO, "Backoff max", CLI_ANSI_RESET, static_cast<unsigned long>(settings.i2cRecoveryBackoffMaxMs));
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "requests") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %u\n", CLI_ANSI_INFO, "Requests per tick", CLI_ANSI_RESET, static_cast<unsigned int>(settings.i2cRequestsPerTick));
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "results") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %u\n", CLI_ANSI_INFO, "Results per tick", CLI_ANSI_RESET, static_cast<unsigned int>(settings.i2cMaxResultsPerTick));
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "task_wait") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %lu ms\n", CLI_ANSI_INFO, "Task wait", CLI_ANSI_RESET, static_cast<unsigned long>(settings.i2cTaskWaitMs));
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "heartbeat") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %lu ms\n", CLI_ANSI_INFO, "Heartbeat timeout", CLI_ANSI_RESET, static_cast<unsigned long>(settings.i2cTaskHeartbeatTimeoutMs));
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "recover_timeout") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %lu ms\n", CLI_ANSI_INFO, "Recover timeout", CLI_ANSI_RESET, static_cast<unsigned long>(settings.i2cRecoverTimeoutMs));
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "slow_threshold") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %lu us\n", CLI_ANSI_INFO, "Slow threshold", CLI_ANSI_RESET, static_cast<unsigned long>(settings.i2cSlowOpThresholdUs));
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "slow_degrade") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %u\n", CLI_ANSI_INFO, "Slow degrade count", CLI_ANSI_RESET, static_cast<unsigned int>(settings.i2cSlowOpDegradeCount));
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "slow_window") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %lu ms\n", CLI_ANSI_INFO, "Slow window", CLI_ANSI_RESET, static_cast<unsigned long>(settings.i2cSlowWindowMs));
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "health_window") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %lu ms\n", CLI_ANSI_INFO, "Health window", CLI_ANSI_RESET, static_cast<unsigned long>(settings.i2cHealthRecentWindowMs));
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "stale_multiplier") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %u\n", CLI_ANSI_INFO, "Stale multiplier", CLI_ANSI_RESET, static_cast<unsigned int>(settings.i2cHealthStaleTaskMultiplier));
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "freq") == 0) {
      uint32_t value = 0;
      if (!parseU32Token(tokens[2], value)) {
        Serial.println("ERR freq must be u32 Hz");
        return;
      }
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      RuntimeSettings settings{};
      if (!_app.tryGetSettingsSnapshot(settings)) {
        Serial.println("ERR state busy");
        return;
      }
      settings.i2cFreqHz = value;
      const Status validation = settings.validate();
      if (!validation.ok()) {
        Serial.printf("ERR settings invalid: %s (%s)\n", errToStr(validation.code), validation.msg);
        return;
      }
      queueSettingsUpdate(settings, persist);
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "timeout") == 0) {
      uint32_t value = 0;
      if (!parseU32Token(tokens[2], value)) {
        Serial.println("ERR timeout must be u32 ms");
        return;
      }
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      RuntimeSettings settings{};
      if (!_app.tryGetSettingsSnapshot(settings)) {
        Serial.println("ERR state busy");
        return;
      }
      settings.i2cOpTimeoutMs = value;
      const Status validation = settings.validate();
      if (!validation.ok()) {
        Serial.printf("ERR settings invalid: %s (%s)\n", errToStr(validation.code), validation.msg);
        return;
      }
      queueSettingsUpdate(settings, persist);
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "stuck_debounce") == 0) {
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("i2c", "stuck_debounce_ms", tokens[2], persist);
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "max_failures") == 0) {
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("i2c", "max_consecutive_failures", tokens[2], persist);
      return;
    }
    if ((argc == 4 || argc == 5) && strcmp(tokens[1], "backoff") == 0) {
      bool persist = false;
      if (argc == 5 && !parseBoolToken(tokens[4], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      RuntimeSettings settings{};
      if (!_app.tryGetSettingsSnapshot(settings)) {
        Serial.println("ERR state busy");
        return;
      }
      const char* errorMsg = "";
      if (!applySettingByKey(settings, "i2c_recovery_backoff_ms", tokens[2], errorMsg) ||
          !applySettingByKey(settings, "i2c_recovery_backoff_max_ms", tokens[3], errorMsg)) {
        Serial.printf("ERR set failed: %s\n", errorMsg);
        return;
      }
      const Status validation = settings.validate();
      if (!validation.ok()) {
        Serial.printf("ERR settings invalid: %s (%s)\n", errToStr(validation.code), validation.msg);
        return;
      }
      queueSettingsUpdate(settings, persist);
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "requests") == 0) {
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("i2c", "requests_per_tick", tokens[2], persist);
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "results") == 0) {
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("i2c", "max_results_per_tick", tokens[2], persist);
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "task_wait") == 0) {
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("i2c", "task_wait_ms", tokens[2], persist);
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "heartbeat") == 0) {
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("i2c", "task_heartbeat_timeout_ms", tokens[2], persist);
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "recover_timeout") == 0) {
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("i2c", "recover_timeout_ms", tokens[2], persist);
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "slow_threshold") == 0) {
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("i2c", "slow_op_threshold_us", tokens[2], persist);
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "slow_degrade") == 0) {
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("i2c", "slow_op_degrade_count", tokens[2], persist);
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "slow_window") == 0) {
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("i2c", "slow_window_ms", tokens[2], persist);
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "health_window") == 0) {
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("i2c", "health_recent_window_ms", tokens[2], persist);
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "stale_multiplier") == 0) {
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("i2c", "health_stale_task_multiplier", tokens[2], persist);
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "status") == 0) {
      printDevice("i2c_bus");
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "settings") == 0) {
      printSettings("i2c");
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "diag") == 0) {
      printDiagnostics("i2c");
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "safe") == 0) {
      applyPreset("i2c", "safe", false);
      return;
    }
    if (argc == 3 && strcmp(tokens[1], "safe") == 0) {
      bool persist = false;
      if (!parseBoolToken(tokens[2], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      applyPreset("i2c", "safe", persist);
      return;
    }
    if (argc == 1) {
      printDiagnostics("i2c");
      return;
    }
    printUsageWithHint("i2c <status|settings|diag|recover|scan|probe|freq|timeout|...>", "i2c");
    return;
  }

  if (strcmp(tokens[0], "display") == 0) {
    if (argc == 1 || (argc == 2 && strcmp(tokens[1], "status") == 0)) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      const AppSettings& app = _app.getAppSettings();
      const HardwareSettings& hw = _app.getConfig();
      Serial.printf("%s[Display]%s\n", CLI_ANSI_WARN, CLI_ANSI_RESET);
      Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Enabled", CLI_ANSI_RESET, yesNo(app.enableDisplay));
      Serial.printf("  %s%-28s%s 0x%02X\n", CLI_ANSI_INFO, "I2C address", CLI_ANSI_RESET, static_cast<unsigned int>(settings.i2cDisplayAddress));
      Serial.printf("  %s%-28s%s %lu ms\n", CLI_ANSI_INFO, "Poll interval", CLI_ANSI_RESET, static_cast<unsigned long>(settings.i2cDisplayPollMs));
      Serial.printf("  %s%-28s%s %d\n", CLI_ANSI_INFO, "HW SDA pin", CLI_ANSI_RESET, hw.i2cSda);
      Serial.printf("  %s%-28s%s %d\n", CLI_ANSI_INFO, "HW SCL pin", CLI_ANSI_RESET, hw.i2cScl);
      if (!app.enableDisplay) {
        Serial.println("display note: pipeline disabled by app setting (enable_display=0)");
      }
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "settings") == 0) {
      printSettings("display");
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "profile") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      const char* profile = "normal";
      if (settings.i2cDisplayPollMs <= 500U) {
        profile = "fast";
      } else if (settings.i2cDisplayPollMs >= 3000U) {
        profile = "slow";
      }
      Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Profile", CLI_ANSI_RESET, profile);
      Serial.printf("  %s%-28s%s %lu ms\n", CLI_ANSI_INFO, "Poll interval", CLI_ANSI_RESET, static_cast<unsigned long>(settings.i2cDisplayPollMs));
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "poll") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %lu ms\n", CLI_ANSI_INFO, "Poll interval", CLI_ANSI_RESET, static_cast<unsigned long>(settings.i2cDisplayPollMs));
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "address") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s 0x%02X\n", CLI_ANSI_INFO, "Address", CLI_ANSI_RESET, static_cast<unsigned int>(settings.i2cDisplayAddress));
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "probe") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      queueI2cProbeAddress(settings.i2cDisplayAddress);
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "recover") == 0) {
      queueI2cRecover();
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "profile") == 0) {
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      RuntimeSettings settings{};
      if (!_app.tryGetSettingsSnapshot(settings)) {
        Serial.println("ERR state busy");
        return;
      }
      if (strcmp(tokens[2], "slow") == 0) {
        settings.i2cDisplayPollMs = 5000U;
      } else if (strcmp(tokens[2], "normal") == 0 || strcmp(tokens[2], "balanced") == 0) {
        settings.i2cDisplayPollMs = 1000U;
      } else if (strcmp(tokens[2], "fast") == 0) {
        settings.i2cDisplayPollMs = 100U;
      } else {
        Serial.println("ERR display profile must be slow|normal|fast");
        return;
      }
      const Status validation = settings.validate();
      if (!validation.ok()) {
        Serial.printf("ERR settings invalid: %s (%s)\n", errToStr(validation.code), validation.msg);
        return;
      }
      queueSettingsUpdate(settings, persist);
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "poll") == 0) {
      uint32_t value = 0;
      if (!parseU32Token(tokens[2], value)) {
        Serial.println("ERR poll must be u32 ms");
        return;
      }
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      RuntimeSettings settings{};
      if (!_app.tryGetSettingsSnapshot(settings)) {
        Serial.println("ERR state busy");
        return;
      }
      settings.i2cDisplayPollMs = value;
      const Status validation = settings.validate();
      if (!validation.ok()) {
        Serial.printf("ERR settings invalid: %s (%s)\n", errToStr(validation.code), validation.msg);
        return;
      }
      queueSettingsUpdate(settings, persist);
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "address") == 0) {
      uint8_t address = 0;
      if (!parseI2cAddressToken(tokens[2], address)) {
        Serial.println("ERR address must be 0x01..0x7F");
        return;
      }
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      RuntimeSettings settings{};
      if (!_app.tryGetSettingsSnapshot(settings)) {
        Serial.println("ERR state busy");
        return;
      }
      settings.i2cDisplayAddress = address;
      const Status validation = settings.validate();
      if (!validation.ok()) {
        Serial.printf("ERR settings invalid: %s (%s)\n", errToStr(validation.code), validation.msg);
        return;
      }
      queueSettingsUpdate(settings, persist);
      return;
    }
    printUsageWithHint("display <status|settings|probe|recover|profile|poll|address>", "display");
    return;
  }

  // Backward-compatible aliases.
  if (strcmp(tokens[0], "guide") == 0) {
    if (argc == 2) {
      printHelp(tokens[1]);
      return;
    }
    printUsageWithHint("guide <topic>", "topics");
    return;
  }

  if (strcmp(tokens[0], "i2c_recover") == 0) {
    if (argc != 1) {
      printUsageWithHint("i2c_recover", "i2c");
      return;
    }
    queueI2cRecover();
    return;
  }

  if (strcmp(tokens[0], "i2c_scan") == 0) {
    if (argc == 1) {
      const Status st = _app.enqueueScanI2cBus();
      if (!st.ok()) {
        Serial.printf("ERR i2c scan queue failed: %s (%s)\n", errToStr(st.code), st.msg);
        printI2cScan();
        return;
      }
      printOkf("i2c scan queued");
      printHint("use 'i2c scan status' after 1-2 seconds");
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "status") == 0) {
      printI2cScan();
      return;
    }
    printUsageWithHint("i2c_scan [status]", "i2c");
    return;
  }

  if (strcmp(tokens[0], "wifi") == 0) {
    if (argc == 1) {
      printDevice("wifi");
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "status") == 0) {
      printDevice("wifi");
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "settings") == 0) {
      printSettings("wifi");
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "ssid") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "SSID", CLI_ANSI_RESET, settings.apSsid);
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "secret") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      const size_t apPassLen = strnlen(settings.apPass, sizeof(settings.apPass));
      Serial.printf("  %s%-28s%s %s\n", CLI_ANSI_INFO, "Secret", CLI_ANSI_RESET, settings.apPass);
      Serial.printf("  %s%-28s%s %lu\n", CLI_ANSI_INFO, "Secret length", CLI_ANSI_RESET, static_cast<unsigned long>(apPassLen));
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "auto_off") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %lu ms\n", CLI_ANSI_INFO, "Auto off", CLI_ANSI_RESET, static_cast<unsigned long>(settings.apAutoOffMs));
      return;
    }
    if (strcmp(tokens[1], "on") == 0 || strcmp(tokens[1], "off") == 0) {
      if (argc != 2 && argc != 3) {
        printUsageWithHint("wifi <on|off> [persist]", "wifi");
        return;
      }
      bool persist = false;
      if (argc == 3 && !parseBoolToken(tokens[2], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      queueWifiSet(strcmp(tokens[1], "on") == 0, persist);
      return;
    }
    if (strcmp(tokens[1], "ssid") == 0 || strcmp(tokens[1], "secret") == 0 ||
        strcmp(tokens[1], "auto_off") == 0) {
      if (argc != 3 && argc != 4) {
        printUsageWithHint("wifi <ssid|secret|auto_off> [value] [persist]", "wifi");
        return;
      }
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      if (strcmp(tokens[1], "ssid") == 0) {
        (void)queueGroupedSetting("wifi", "ssid", tokens[2], persist);
      } else if (strcmp(tokens[1], "secret") == 0) {
        (void)queueGroupedSetting("wifi", "secret", tokens[2], persist);
      } else {
        (void)queueGroupedSetting("wifi", "auto_off_ms", tokens[2], persist);
      }
      return;
    }
    printUsageWithHint("wifi <status|settings|on|off|ssid|secret|auto_off>", "wifi");
    return;
  }

  if (strcmp(tokens[0], "web") == 0) {
    if (argc == 2 && strcmp(tokens[1], "status") == 0) {
      printDevice("web");
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "settings") == 0) {
      printSettings("web");
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "max_settings_body") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("web max_settings_body=%u B\n", static_cast<unsigned int>(settings.webMaxSettingsBodyBytes));
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "max_rtc_body") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("web max_rtc_body=%u B\n", static_cast<unsigned int>(settings.webMaxRtcBodyBytes));
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "max_settings_body") == 0) {
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("web", "max_settings_body_bytes", tokens[2], persist);
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "max_rtc_body") == 0) {
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("web", "max_rtc_body_bytes", tokens[2], persist);
      return;
    }
    printUsageWithHint("web <status|settings|max_settings_body|max_rtc_body>", "web");
    return;
  }

  if (strcmp(tokens[0], "system") == 0) {
    if (argc == 2 && strcmp(tokens[1], "status") == 0) {
      printDevice("system");
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "settings") == 0) {
      printSettings("system");
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "sample_interval") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("system sample_interval=%lu ms\n", static_cast<unsigned long>(settings.sampleIntervalMs));
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "verbosity") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("system verbosity=%s\n", cliVerbosityToStr(settings.cliVerbosity));
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "command_drain") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("system command_drain=%u\n", static_cast<unsigned int>(settings.commandDrainPerTick));
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "command_window") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("system command_window=%lu ms\n", static_cast<unsigned long>(settings.commandQueueDegradedWindowMs));
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "command_depth") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("system command_depth=%u\n", static_cast<unsigned int>(settings.commandQueueDegradedDepthThreshold));
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "tick_slow") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("system tick_slow=%lu us\n", static_cast<unsigned long>(settings.mainTickSlowThresholdUs));
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "ap_retry") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("system ap_retry=%lu ms\n", static_cast<unsigned long>(settings.apStartRetryBackoffMs));
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "sample_interval") == 0) {
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      RuntimeSettings settings{};
      if (!_app.tryGetSettingsSnapshot(settings)) {
        Serial.println("ERR state busy");
        return;
      }
      settings.sampleIntervalMs = 0;
      if (!parseU32Token(tokens[2], settings.sampleIntervalMs)) {
        Serial.println("ERR sample interval must be u32 ms");
        return;
      }
      const Status validation = settings.validate();
      if (!validation.ok()) {
        Serial.printf("ERR settings invalid: %s (%s)\n", errToStr(validation.code), validation.msg);
        return;
      }
      queueSettingsUpdate(settings, persist);
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "verbosity") == 0) {
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("system", "verbosity", tokens[2], persist);
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "command_drain") == 0) {
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("system", "command_drain_per_tick", tokens[2], persist);
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "command_window") == 0) {
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("system", "command_queue_degraded_window_ms", tokens[2], persist);
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "command_depth") == 0) {
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("system", "command_queue_degraded_depth_threshold", tokens[2], persist);
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "tick_slow") == 0) {
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("system", "main_tick_slow_threshold_us", tokens[2], persist);
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "ap_retry") == 0) {
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("system", "ap_start_retry_backoff_ms", tokens[2], persist);
      return;
    }
    printUsageWithHint("system <status|settings|sample_interval|verbosity|command_drain|command_window|command_depth|tick_slow|ap_retry>", "system");
    return;
  }

  if (strcmp(tokens[0], "leds") == 0) {
    if (argc == 2 && strcmp(tokens[1], "status") == 0) {
      printDevice("leds");
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "settings") == 0) {
      printSettings("system");
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "health_init") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %lu ms\n", CLI_ANSI_INFO, "Health init", CLI_ANSI_RESET, static_cast<unsigned long>(settings.ledHealthInitMs));
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "health_debounce") == 0) {
      RuntimeSettings settings{};
      if (!loadSettingsSnapshot(settings)) {
        return;
      }
      Serial.printf("  %s%-28s%s %lu ms\n", CLI_ANSI_INFO, "Health debounce", CLI_ANSI_RESET, static_cast<unsigned long>(settings.ledHealthDebounceMs));
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "health_init") == 0) {
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("system", "led_health_init_ms", tokens[2], persist);
      return;
    }
    if ((argc == 3 || argc == 4) && strcmp(tokens[1], "health_debounce") == 0) {
      bool persist = false;
      if (argc == 4 && !parseBoolToken(tokens[3], persist)) {
        Serial.println("ERR persist must be bool");
        return;
      }
      (void)queueGroupedSetting("system", "led_health_debounce_ms", tokens[2], persist);
      return;
    }
    printUsageWithHint("leds <status|settings|health_init|health_debounce>", "leds");
    return;
  }

  if (strcmp(tokens[0], "button") == 0) {
    if (argc == 2 && strcmp(tokens[1], "status") == 0) {
      printDevice("button");
      return;
    }
    if (argc == 2 && strcmp(tokens[1], "settings") == 0) {
      printBootConfig("hardware");
      return;
    }
    printUsageWithHint("button <status|settings>", "button");
    return;
  }

  Serial.printf("%sERR%s unknown command\n", CLI_ANSI_ERR, CLI_ANSI_RESET);
  Serial.printf("%sHint:%s help topics\n", CLI_ANSI_INFO, CLI_ANSI_RESET);
}

void SerialCli::end() {
  _lineLen = 0;
}

#endif

}  // namespace TFLunaControl

#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <fstream>
#include <sstream>
#include <string>

#include <ArduinoJson.h>
#include <unity.h>

#include "config/AppConfig.h"
#include "core/CommandQueue.h"
#include "core/QueueHealth.h"
#include "core/RingBuffer.h"
#include "core/Scheduler.h"
#include "core/ApiJson.h"
#include "core/LidarStats.h"
#include "TFLunaControl/RuntimeSettings.h"
#include "TFLunaControl/Types.h"
#include "devices/StatusLedAdapter.h"
#include "i2c/I2cBackend.h"
#include "i2c/I2cOrchestrator.h"
#include "i2c/I2cTask.h"
#include "i2c/RecoveryPolicy.h"
#include "logging/SdLogger.h"
#include "web/WebLockOrder.h"
#include "web/WebServer.h"
#include "TFLunaControl/TFLunaControl.h"

using namespace TFLunaControl;

void setUp() {}
void tearDown() {}

static bool loadTextFile(const char* relativePath, std::string& out) {
  std::ifstream in(relativePath, std::ios::in | std::ios::binary);
  if (!in.good()) {
    return false;
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  out = ss.str();
  return true;
}

void test_ring_buffer_order() {
  RingBuffer<int, 3> rb;
  rb.push(1);
  rb.push(2);
  rb.push(3);
  TEST_ASSERT_EQUAL(3, rb.size());

  int out[3] = {0};
  const size_t n = rb.copy(out, 3, true);
  TEST_ASSERT_EQUAL(3, n);
  TEST_ASSERT_EQUAL(1, out[0]);
  TEST_ASSERT_EQUAL(2, out[1]);
  TEST_ASSERT_EQUAL(3, out[2]);

  rb.push(4);  // overwrite oldest
  TEST_ASSERT_EQUAL(3, rb.size());
  const size_t n2 = rb.copy(out, 3, true);
  TEST_ASSERT_EQUAL(3, n2);
  TEST_ASSERT_EQUAL(2, out[0]);
  TEST_ASSERT_EQUAL(3, out[1]);
  TEST_ASSERT_EQUAL(4, out[2]);
}

void test_ring_buffer_oldest_first_returns_latest_window() {
  RingBuffer<int, 5> rb;
  rb.push(1);
  rb.push(2);
  rb.push(3);
  rb.push(4);
  rb.push(5);

  int out[3] = {0};
  const size_t n = rb.copy(out, 3, true);
  TEST_ASSERT_EQUAL(3, n);
  TEST_ASSERT_EQUAL(3, out[0]);
  TEST_ASSERT_EQUAL(4, out[1]);
  TEST_ASSERT_EQUAL(5, out[2]);

  rb.push(6);
  const size_t n2 = rb.copy(out, 3, true);
  TEST_ASSERT_EQUAL(3, n2);
  TEST_ASSERT_EQUAL(4, out[0]);
  TEST_ASSERT_EQUAL(5, out[1]);
  TEST_ASSERT_EQUAL(6, out[2]);
}

void test_periodic_timer() {
  PeriodicTimer t;
  t.setInterval(1000);
  TEST_ASSERT_FALSE(t.isDue(0));
  TEST_ASSERT_FALSE(t.isDue(500));
  TEST_ASSERT_TRUE(t.isDue(1000));
  TEST_ASSERT_FALSE(t.isDue(1500));
  TEST_ASSERT_TRUE(t.isDue(2000));
}

void test_settings_validation() {
  RuntimeSettings s;
  s.sampleIntervalMs = 0;
  TEST_ASSERT_FALSE(s.validate().ok());

  s.restoreDefaults();
  s.apAutoOffMs = 0;
  TEST_ASSERT_FALSE(s.validate().ok());

  s.restoreDefaults();
  s.i2cFreqHz = 200000;
  TEST_ASSERT_FALSE(s.validate().ok());

  s.restoreDefaults();
  TEST_ASSERT_TRUE(s.validate().ok());
}

void test_default_rtc_address_is_rv3032() {
  RuntimeSettings s;
  s.restoreDefaults();
  TEST_ASSERT_EQUAL_UINT8(0x51, s.i2cRtcAddress);
  TEST_ASSERT_EQUAL_UINT8(1U, s.i2cRtcBackupMode);
  TEST_ASSERT_TRUE(s.i2cRtcEnableEepromWrites);
}

void test_board_defaults_prepare_endstop_inputs() {
  const HardwareSettings hw = loadHardwareSettings();
  TEST_ASSERT_EQUAL_INT(5, hw.endstopUpperPin);
  TEST_ASSERT_EQUAL_INT(6, hw.endstopLowerPin);
  TEST_ASSERT_TRUE(hw.endstopUpperActiveLow);
  TEST_ASSERT_TRUE(hw.endstopLowerActiveLow);
}

void test_settings_validation_extremes() {
  RuntimeSettings s;

  s.restoreDefaults();
  s.sampleIntervalMs = RuntimeSettings::MAX_SAMPLE_INTERVAL_MS + 1U;
  TEST_ASSERT_FALSE(s.validate().ok());

  s.restoreDefaults();
  s.logAllEnabled = true;
  s.logAllMaxBytes = 0;
  TEST_ASSERT_FALSE(s.validate().ok());

  s.restoreDefaults();
  s.logFlushMs = RuntimeSettings::MIN_LOG_FLUSH_MS - 1U;
  TEST_ASSERT_FALSE(s.validate().ok());

  s.restoreDefaults();
  s.logIoBudgetMs = RuntimeSettings::MAX_LOG_IO_BUDGET_MS + 1U;
  TEST_ASSERT_FALSE(s.validate().ok());

  s.restoreDefaults();
  s.logMaxWriteRetries = RuntimeSettings::MAX_LOG_MAX_WRITE_RETRIES + 1U;
  TEST_ASSERT_FALSE(s.validate().ok());

  s.restoreDefaults();
  s.logSessionName[0] = '\0';
  TEST_ASSERT_FALSE(s.validate().ok());

  s.restoreDefaults();
  strncpy(s.logSessionName, "bad/name", sizeof(s.logSessionName) - 1U);
  s.logSessionName[sizeof(s.logSessionName) - 1U] = '\0';
  TEST_ASSERT_FALSE(s.validate().ok());

  s.restoreDefaults();
  strncpy(s.logSessionName, "-bad", sizeof(s.logSessionName) - 1U);
  s.logSessionName[sizeof(s.logSessionName) - 1U] = '\0';
  TEST_ASSERT_FALSE(s.validate().ok());

  s.restoreDefaults();
  strncpy(s.logSessionName, "bad_trailing_", sizeof(s.logSessionName) - 1U);
  s.logSessionName[sizeof(s.logSessionName) - 1U] = '\0';
  TEST_ASSERT_FALSE(s.validate().ok());

  s.restoreDefaults();
  memset(s.logSessionName, 'a', sizeof(s.logSessionName));
  TEST_ASSERT_FALSE(s.validate().ok());

  s.restoreDefaults();
  s.apAutoOffMs = 0;
  TEST_ASSERT_FALSE(s.validate().ok());

  s.restoreDefaults();
  s.i2cFreqHz = 200000;
  TEST_ASSERT_FALSE(s.validate().ok());

  s.restoreDefaults();
  s.i2cStuckDebounceMs = RuntimeSettings::MAX_I2C_STUCK_DEBOUNCE_MS + 1U;
  TEST_ASSERT_FALSE(s.validate().ok());

  s.restoreDefaults();
  s.i2cRecoveryBackoffMaxMs = s.i2cRecoveryBackoffMs - 1U;
  TEST_ASSERT_FALSE(s.validate().ok());

  s.restoreDefaults();
  s.i2cSlowOpThresholdUs = RuntimeSettings::MIN_I2C_SLOW_OP_THRESHOLD_US - 1U;
  TEST_ASSERT_FALSE(s.validate().ok());

  s.restoreDefaults();
  s.i2cSlowOpDegradeCount = RuntimeSettings::MAX_I2C_SLOW_OP_DEGRADE_COUNT + 1U;
  TEST_ASSERT_FALSE(s.validate().ok());

  s.restoreDefaults();
  s.i2cEnvConversionWaitMs = RuntimeSettings::MIN_I2C_ENV_CONVERSION_WAIT_MS - 1U;
  TEST_ASSERT_FALSE(s.validate().ok());

  s.restoreDefaults();
  s.i2cTaskHeartbeatTimeoutMs = RuntimeSettings::MIN_I2C_HEARTBEAT_TIMEOUT_MS - 1U;
  TEST_ASSERT_FALSE(s.validate().ok());

  s.restoreDefaults();
  s.i2cEnvAddress = 0;
  TEST_ASSERT_FALSE(s.validate().ok());

  s.restoreDefaults();
  s.e2Address = RuntimeSettings::MAX_E2_ADDR + 1U;
  TEST_ASSERT_FALSE(s.validate().ok());

  s.restoreDefaults();
  s.e2ByteTimeoutUs = s.e2BitTimeoutUs - 1U;
  TEST_ASSERT_FALSE(s.validate().ok());

  s.restoreDefaults();
  s.e2RecoveryBackoffMaxMs = s.e2RecoveryBackoffMs - 1U;
  TEST_ASSERT_FALSE(s.validate().ok());

  s.restoreDefaults();
  s.commandDrainPerTick = RuntimeSettings::MIN_COMMANDS_PER_TICK - 1U;
  TEST_ASSERT_FALSE(s.validate().ok());

  s.restoreDefaults();
  s.webMaxRtcBodyBytes = RuntimeSettings::MIN_WEB_MAX_RTC_BODY_BYTES - 1U;
  TEST_ASSERT_FALSE(s.validate().ok());

  s.restoreDefaults();
  s.webMaxSettingsBodyBytes = RuntimeSettings::MIN_WEB_MAX_SETTINGS_BODY_BYTES - 1U;
  TEST_ASSERT_FALSE(s.validate().ok());
}

void test_sample_interval_upper_bound_matches_timer_window() {
  TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(INT32_MAX),
                           RuntimeSettings::MAX_SAMPLE_INTERVAL_MS);

  RuntimeSettings s;
  s.restoreDefaults();
  s.sampleIntervalMs = RuntimeSettings::MAX_SAMPLE_INTERVAL_MS;
  TEST_ASSERT_TRUE(s.validate().ok());
}

void test_app_settings_validation_guards_present() {
  std::string source;
  TEST_ASSERT_TRUE(loadTextFile("src/TFLunaControl.cpp", source));
  const char* text = source.c_str();
  TEST_ASSERT_NOT_NULL(strstr(text, "validateAppSettings"));
  TEST_ASSERT_NOT_NULL(strstr(text, "webBroadcastMs out of range"));
  TEST_ASSERT_NOT_NULL(strstr(text, "sd queue depth invalid"));
  TEST_ASSERT_NOT_NULL(strstr(text, "appSettingsValidation = validateAppSettings(appSettings)"));
}

void test_command_queue_overflow_behavior() {
  CommandQueue<int, 2> q;
  q.clear();

  TEST_ASSERT_TRUE(q.push(1, 100));
  TEST_ASSERT_TRUE(q.push(2, 110));
  TEST_ASSERT_EQUAL(2, q.depth());

  TEST_ASSERT_FALSE(q.push(3, 120));
  TEST_ASSERT_EQUAL_UINT32(1, q.overflowCount());
  TEST_ASSERT_EQUAL_UINT32(120, q.lastOverflowMs());

  int value = 0;
  TEST_ASSERT_TRUE(q.pop(value));
  TEST_ASSERT_EQUAL(1, value);
  TEST_ASSERT_TRUE(q.pop(value));
  TEST_ASSERT_EQUAL(2, value);
  TEST_ASSERT_FALSE(q.pop(value));
}

void test_status_json_bounded_serialization() {
  SystemStatus sys;
  sys.health = HealthState::DEGRADED;
  sys.uptimeMs = 123456;
  sys.sampleCount = 77;
  sys.lastSampleMs = 123400;
  sys.sdMounted = true;
  sys.logDailyOk = true;
  sys.logAllOk = false;
  sys.wifiApRunning = true;
  sys.wifiStationCount = 3;
  sys.webClientCount = 2;
  sys.logDroppedCount = 9;
  sys.logQueueDepth = 4;
  sys.logLastWriteMs = 120000;
  sys.logLastWriteAgeMs = 3456;
  sys.logLastErrorMs = 121111;
  sys.logLastErrorMsg = "queue full";
  sys.logLastErrorDetail = 42;
  sys.logIoBudgetMs = 5;
  sys.logLastTickElapsedMs = 3;
  sys.logBudgetExceededCount = 2;
  sys.commandQueueDepth = 1;
  sys.commandQueueOverflowCount = 5;
  sys.commandQueueLastOverflowMs = 122222;
  sys.endstopUpperPin = 5;
  sys.endstopUpperConfigured = true;
  sys.endstopUpperActiveLow = true;
  sys.endstopUpperRawHigh = false;
  sys.endstopUpperTriggered = true;
  sys.endstopUpperLastChangeMs = 2222;
  sys.endstopLowerPin = 6;
  sys.endstopLowerConfigured = true;
  sys.endstopLowerActiveLow = true;
  sys.endstopLowerRawHigh = true;
  sys.endstopLowerTriggered = false;
  sys.endstopLowerLastChangeMs = 3333;
  Sample sample{};
  sample.tsUnix = 1710000000UL;
  strncpy(sample.tsLocal, "2026-01-10 12:34:56", sizeof(sample.tsLocal) - 1);
  sample.co2ppm = 1050.0f;
  sample.tempC = 24.5f;
  sample.rhPct = 41.0f;
  sample.pressureHpa = 1009.3f;
  sample.validMask = VALID_CO2 | VALID_TEMP | VALID_RH | VALID_PRESSURE;

  char tooSmall[64];
  TEST_ASSERT_EQUAL(0U, serializeStatusJsonBounded(sys, &sample, tooSmall, sizeof(tooSmall)));

  char jsonBuf[HardwareSettings::WEB_STATUS_WS_BUFFER_BYTES];
  const size_t written = serializeStatusJsonBounded(sys, &sample, jsonBuf, sizeof(jsonBuf));
  TEST_ASSERT_TRUE(written > 0);

  StaticJsonDocument<6144> doc;
  const DeserializationError err = deserializeJson(doc, jsonBuf);
  TEST_ASSERT_FALSE(err);
  TEST_ASSERT_EQUAL_STRING("status", doc["type"].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("DEGRADED", doc["health"].as<const char*>());
  TEST_ASSERT_EQUAL_UINT32(5, doc["cmd_queue_overflow_count"].as<uint32_t>());
  TEST_ASSERT_EQUAL_STRING("queue full", doc["log_last_error_msg"].as<const char*>());
  TEST_ASSERT_EQUAL_UINT32(2, doc["log_budget_exceeded_count"].as<uint32_t>());
  TEST_ASSERT_EQUAL_INT(5, doc["endstop_upper_pin"].as<int>());
  TEST_ASSERT_TRUE(doc["endstop_upper_triggered"].as<bool>());
  TEST_ASSERT_EQUAL_INT(6, doc["endstop_lower_pin"].as<int>());
  TEST_ASSERT_FALSE(doc["endstop_lower_triggered"].as<bool>());
}

void test_command_queue_degraded_window_logic() {
  static constexpr uint32_t kWindowMs = 60000;
  static constexpr size_t kDepthThreshold = 6;

  TEST_ASSERT_FALSE(isCommandQueueDegraded(100000, 1000, 0, kWindowMs, kDepthThreshold));
  TEST_ASSERT_TRUE(isCommandQueueDegraded(100000, 50000, 0, kWindowMs, kDepthThreshold));
  TEST_ASSERT_TRUE(isCommandQueueDegraded(100000, 1000, 6, kWindowMs, kDepthThreshold));
  TEST_ASSERT_FALSE(isCommandQueueDegraded(100000, 1000, 5, kWindowMs, kDepthThreshold));
}

void test_device_json_escaping_and_parseability() {
  DeviceStatus status{};
  status.id = DeviceId::WEB;
  status.name = "web\\node";
  status.health = HealthState::DEGRADED;
  status.lastOkMs = 100;
  status.lastErrorMs = 200;
  status.lastActivityMs = 250;
  status.lastStatus = Status(Err::COMM_FAILURE, 7, "bad \"msg\"\nline");

  StaticJsonDocument<512> itemDoc;
  populateDeviceStatusJson(itemDoc, status, 8);

  char out[512];
  const size_t n = serializeJson(itemDoc, out, sizeof(out));
  TEST_ASSERT_TRUE(n > 0);

  StaticJsonDocument<512> parsed;
  const DeserializationError err = deserializeJson(parsed, out);
  TEST_ASSERT_FALSE(err);
  TEST_ASSERT_EQUAL_STRING("web\\node", parsed["name"].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("bad \"msg\"\nline", parsed["last_status_msg"].as<const char*>());
}

void test_graph_sample_json_nan_outputs_null() {
  Sample sample{};
  sample.tsUnix = 1700000000UL;
  strncpy(sample.tsLocal, "2026-02-07 19:30:00", sizeof(sample.tsLocal) - 1);
  sample.validMask = VALID_CO2 | VALID_TEMP | VALID_RH | VALID_PRESSURE;
  sample.co2ppm = NAN;
  sample.tempC = INFINITY;
  sample.rhPct = 55.0f;
  sample.pressureHpa = NAN;

  StaticJsonDocument<256> itemDoc;
  populateGraphSampleJson(itemDoc, sample);

  char out[256];
  const size_t n = serializeJson(itemDoc, out, sizeof(out));
  TEST_ASSERT_TRUE(n > 0);

  StaticJsonDocument<256> parsed;
  const DeserializationError err = deserializeJson(parsed, out);
  TEST_ASSERT_FALSE(err);
  TEST_ASSERT_TRUE(parsed["co2_ppm"].isNull());
  TEST_ASSERT_TRUE(parsed["temp_c"].isNull());
  TEST_ASSERT_EQUAL_FLOAT(55.0f, parsed["rh_pct"].as<float>());
  TEST_ASSERT_TRUE(parsed["pressure_hpa"].isNull());
}

void test_settings_json_write_only_password() {
  RuntimeSettings settings;
  settings.restoreDefaults();
  strncpy(settings.apPass, "supersecret", sizeof(settings.apPass) - 1);
  settings.apPass[sizeof(settings.apPass) - 1] = '\0';

  StaticJsonDocument<4096> doc;
  populateSettingsJson(doc, settings);

  char out[4096];
  const size_t n = serializeJson(doc, out, sizeof(out));
  TEST_ASSERT_TRUE(n > 0);

  TEST_ASSERT_NULL(strstr(out, "\"ap_pass\":"));
  TEST_ASSERT_NOT_NULL(strstr(out, "\"ap_pass_set\":"));
  TEST_ASSERT_NOT_NULL(strstr(out, "\"ap_pass_masked\":"));
  TEST_ASSERT_NOT_NULL(strstr(out, "\"ap_pass_update_mode\":\"write_only\""));
  TEST_ASSERT_NOT_NULL(strstr(out, "\"cli_verbosity_name\":\"normal\""));
  TEST_ASSERT_NOT_NULL(strstr(out, "\"log_session_name\":\"run\""));
  TEST_ASSERT_NOT_NULL(strstr(out, "\"e2_address\":"));
  TEST_ASSERT_NOT_NULL(strstr(out, "\"e2_recovery_backoff_ms\":"));
}

void test_cli_verbosity_uses_named_levels() {
  std::string cliSource;
  std::string webSource;
  TEST_ASSERT_TRUE(loadTextFile("src/core/SerialCli.cpp", cliSource));
  TEST_ASSERT_TRUE(loadTextFile("src/web/WebServer.cpp", webSource));

  TEST_ASSERT_NOT_NULL(strstr(cliSource.c_str(), "return \"off\";"));
  TEST_ASSERT_NOT_NULL(strstr(cliSource.c_str(), "strcmp(token, \"off\") == 0"));
  TEST_ASSERT_NOT_NULL(strstr(cliSource.c_str(), "system verbosity <off|normal|verbose> [p]"));
  TEST_ASSERT_NOT_NULL(
      strstr(cliSource.c_str(), "invalid verbosity (off|normal|verbose or 0..2)"));
  TEST_ASSERT_NOT_NULL(strstr(cliSource.c_str(), "system verbosity=%s\\n"));
  TEST_ASSERT_NULL(
      strstr(cliSource.c_str(), "system verbosity <0|1|2> [p]  compact|normal|verbose"));
  TEST_ASSERT_NULL(strstr(cliSource.c_str(), "system verbosity=%s (%u)\\n"));
  TEST_ASSERT_NOT_NULL(strstr(webSource.c_str(), "Invalid cli_verbosity"));
  TEST_ASSERT_NOT_NULL(strstr(webSource.c_str(), "strcmp(token, \"off\") == 0"));
}

void test_cli_settings_updates_use_change_hints_and_skip_noops() {
  std::string cliSource;
  TEST_ASSERT_TRUE(loadTextFile("src/core/SerialCli.cpp", cliSource));
  const char* text = cliSource.c_str();

  TEST_ASSERT_NOT_NULL(strstr(text, "const Status st = _app.enqueueApplySettings(s, persist, changeHint);"));
  TEST_ASSERT_NOT_NULL(strstr(text, "printOkf(\"settings unchanged\")"));
  TEST_ASSERT_NOT_NULL(strstr(text, "queueSettingsUpdateIfChanged(before, settings, persist, resolvedKey)"));
  TEST_ASSERT_NOT_NULL(strstr(text, "queueSettingsUpdateIfChanged(before, settings, persist, key)"));
}

void test_led_health_debounce_logic() {
  StatusLedAdapter::HealthDebounceState state{};

  StatusLedAdapter::HealthState out =
      StatusLedAdapter::debounceHealth(StatusLedAdapter::HealthState::OK, 100, 1000, state);
  TEST_ASSERT_EQUAL(StatusLedAdapter::HealthState::OK, out);

  out = StatusLedAdapter::debounceHealth(StatusLedAdapter::HealthState::DEGRADED, 200, 1000, state);
  TEST_ASSERT_EQUAL(StatusLedAdapter::HealthState::OK, out);

  out = StatusLedAdapter::debounceHealth(StatusLedAdapter::HealthState::DEGRADED, 1300, 1000, state);
  TEST_ASSERT_EQUAL(StatusLedAdapter::HealthState::DEGRADED, out);

  out = StatusLedAdapter::debounceHealth(StatusLedAdapter::HealthState::FAULT, 1301, 1000, state);
  TEST_ASSERT_EQUAL(StatusLedAdapter::HealthState::FAULT, out);
}

void test_log_flush_due_logic() {
  TEST_ASSERT_TRUE(SdLogger::shouldAttemptFlush(1000, 0, 2000));
  TEST_ASSERT_FALSE(SdLogger::shouldAttemptFlush(1500, 1000, 1000));
  TEST_ASSERT_TRUE(SdLogger::shouldAttemptFlush(2000, 1000, 1000));
}

void test_logged_sample_csv_omits_redundant_time_fields() {
  std::string source;
  TEST_ASSERT_TRUE(loadTextFile("src/logging/SdLogger.cpp", source));
  const char* text = source.c_str();

  TEST_ASSERT_NOT_NULL(strstr(text, "\"timestamp,uptime_ms,sample_index,distance_cm,strength,temperature_c,valid_frame,signal_ok,env_temp_c,env_rh_pct,env_pressure_hpa\\n\""));
  TEST_ASSERT_NULL(strstr(text, "timestamp,timestamp_ms,time_source"));
  TEST_ASSERT_NULL(strstr(text, "formatSampleTimestampMs("));
  TEST_ASSERT_NOT_NULL(strstr(text, "static constexpr char SESSION_DATA_PREFIX[] = \"data_\";"));
  TEST_ASSERT_NOT_NULL(strstr(text, "static constexpr char SESSION_DATA_PREFIX_COMPAT[] = \"data_v2_\";"));
  TEST_ASSERT_NOT_NULL(strstr(text, "%s/%s_v2.csv"));
}

void test_lidar_stats_reset_clears_running_window() {
  LidarStats stats;
  LidarMeasurement measurement{};
  measurement.validFrame = true;
  measurement.signalOk = true;
  measurement.distanceCm = 120U;
  measurement.strength = 40U;
  stats.recordMeasurement(measurement);

  measurement.distanceCm = 180U;
  measurement.strength = 60U;
  stats.recordMeasurement(measurement);

  LidarStatsSnapshot snapshot = stats.snapshot();
  TEST_ASSERT_TRUE(snapshot.hasDistanceStats);
  TEST_ASSERT_EQUAL_UINT64(2U, snapshot.totalFrames);
  TEST_ASSERT_EQUAL_UINT64(2U, snapshot.validSamples);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 120.0f, snapshot.minDistanceCm);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 180.0f, snapshot.maxDistanceCm);

  stats.reset();
  snapshot = stats.snapshot();
  TEST_ASSERT_FALSE(snapshot.hasDistanceStats);
  TEST_ASSERT_EQUAL_UINT64(0U, snapshot.totalFrames);
  TEST_ASSERT_EQUAL_UINT64(0U, snapshot.validSamples);
  TEST_ASSERT_EQUAL_UINT64(0U, snapshot.invalidSamples);
  TEST_ASSERT_EQUAL_UINT64(0U, snapshot.weakSamples);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, snapshot.minDistanceCm);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, snapshot.maxDistanceCm);
}

void test_web_server_lifecycle_reinit_safe() {
  WebServer web;
  TFLunaControl::TFLunaControl app;
  TEST_ASSERT_TRUE(web.begin(&app).ok());
  TEST_ASSERT_TRUE(web.begin(&app).ok());
  web.end();
  web.end();
}

void test_web_request_count_clamps() {
  TEST_ASSERT_EQUAL_UINT32(WebServer::MAX_GRAPH_SAMPLES, WebServer::clampGraphCount(0));
  TEST_ASSERT_EQUAL_UINT32(WebServer::MAX_GRAPH_SAMPLES,
                           WebServer::clampGraphCount(WebServer::MAX_GRAPH_SAMPLES + 1));
  TEST_ASSERT_EQUAL_UINT32(42, WebServer::clampGraphCount(42));
  TEST_ASSERT_EQUAL_UINT32(77, WebServer::clampGraphCount(88, 77));
  TEST_ASSERT_EQUAL_UINT32(0, WebServer::clampGraphCount(10, 0));

  TEST_ASSERT_EQUAL_UINT32(WebServer::MAX_EVENT_COUNT, WebServer::clampEventCount(0));
  TEST_ASSERT_EQUAL_UINT32(WebServer::MAX_EVENT_COUNT,
                           WebServer::clampEventCount(WebServer::MAX_EVENT_COUNT + 1));
  TEST_ASSERT_EQUAL_UINT32(12, WebServer::clampEventCount(12));
  TEST_ASSERT_EQUAL_UINT32(25, WebServer::clampEventCount(100, 25));
  TEST_ASSERT_EQUAL_UINT32(0, WebServer::clampEventCount(1, 0));
}

void test_web_graph_events_no_heap_alloc_path() {
  std::string source;
  TEST_ASSERT_TRUE(loadTextFile("src/web/WebServer.cpp", source));
  const char* text = source.c_str();

  TEST_ASSERT_NULL(strstr(text, "new (std::nothrow) Sample"));
  TEST_ASSERT_NULL(strstr(text, "new (std::nothrow) Event"));
  TEST_ASSERT_NOT_NULL(strstr(text, "allocateScratch"));
  TEST_ASSERT_NOT_NULL(strstr(text, "releaseScratch"));
  TEST_ASSERT_NOT_NULL(strstr(text, "graphScratchCapacity"));
  TEST_ASSERT_NOT_NULL(strstr(text, "eventScratchCapacity"));
  TEST_ASSERT_NOT_NULL(strstr(text, "OrderedWebReadGuard"));
}

void test_web_lidar_stats_reset_controls_present() {
  std::string webSource;
  std::string pageSource;
  TEST_ASSERT_TRUE(loadTextFile("src/web/WebServer.cpp", webSource));
  TEST_ASSERT_TRUE(loadTextFile("src/web/WebPages.h", pageSource));

  TEST_ASSERT_NOT_NULL(strstr(webSource.c_str(), "/api/device/reset_stats"));
  TEST_ASSERT_NOT_NULL(strstr(webSource.c_str(), "enqueueResetLidarStats"));
  TEST_ASSERT_NOT_NULL(strstr(pageSource.c_str(), "Reset Stats"));
  TEST_ASSERT_NOT_NULL(strstr(pageSource.c_str(), "reset_stats"));
}

void test_faster_distance_refresh_defaults() {
  AppSettings app;
  TEST_ASSERT_EQUAL_UINT32(500U, app.webBroadcastMs);
  TEST_ASSERT_EQUAL_UINT32(2000U, app.webUiGraphRefreshMs);

  RuntimeSettings settings;
  settings.restoreDefaults();
  TEST_ASSERT_FALSE(settings.logDailyEnabled);
  TEST_ASSERT_FALSE(settings.logAllEnabled);
  TEST_ASSERT_EQUAL_UINT32(100U, settings.i2cDisplayPollMs);
}

void test_web_live_device_tab_rerenders_distance_stats() {
  std::string pageSource;
  TEST_ASSERT_TRUE(loadTextFile("src/web/WebPages.h", pageSource));
  const char* text = pageSource.c_str();

  TEST_ASSERT_NOT_NULL(strstr(text, "const UI_GRAPH_REFRESH_MS=2000;"));
  TEST_ASSERT_NOT_NULL(strstr(text, "const STATUS_POLL_MS=1000;"));
  TEST_ASSERT_NOT_NULL(strstr(text, "if(activeTab==='devices'){devs()}"));
}

void test_web_i2c_settings_are_cli_only() {
  std::string pageSource;
  TEST_ASSERT_TRUE(loadTextFile("src/web/WebPages.h", pageSource));
  const char* text = pageSource.c_str();

  TEST_ASSERT_NOT_NULL(strstr(text, "I2C tuning is CLI-only."));
  TEST_ASSERT_NOT_NULL(strstr(text, "const DM={i2c_bus:[],sd:[],env:[],rtc:[],lidar:["));
  TEST_ASSERT_NOT_NULL(strstr(text, "else if(n==='settings'){Promise.all([getSet(),stOnce()])}"));
  TEST_ASSERT_NOT_NULL(strstr(text, "if(activeTab==='settings'){await getSet();return}"));
  TEST_ASSERT_NULL(strstr(text, "env-model"));
  TEST_ASSERT_NULL(strstr(text, "queueRtcBackupSetup"));
  TEST_ASSERT_NULL(strstr(text, "rtc-setup-backup"));
}

void test_web_endstops_tab_replaces_outputs_surface() {
  std::string pageSource;
  TEST_ASSERT_TRUE(loadTextFile("src/web/WebPages.h", pageSource));
  const char* text = pageSource.c_str();

  TEST_ASSERT_NOT_NULL(strstr(text, "b-endstops"));
  TEST_ASSERT_NOT_NULL(strstr(text, "id=\"endstops\""));
  TEST_ASSERT_NOT_NULL(strstr(text, "GPIO 5 is prepared for the upper limit, GPIO 6 for the lower limit."));
  TEST_ASSERT_NOT_NULL(strstr(text, "renderEndstopsQuick"));
  TEST_ASSERT_NOT_NULL(strstr(text, "renderEndstopsTab"));
  TEST_ASSERT_NULL(strstr(text, "id=\"b-outputs\""));
  TEST_ASSERT_NULL(strstr(text, "<div id=\"outputs\" class=\"tab\">"));
  TEST_ASSERT_NULL(strstr(text, "outputs:'Outputs'"));
  TEST_ASSERT_NULL(strstr(text, "renderOutputsTab"));
  TEST_ASSERT_NULL(strstr(text, "queueOutputBlink"));
  TEST_ASSERT_NULL(strstr(text, "postOutputTestWithRetry"));
  TEST_ASSERT_NULL(strstr(text, "OUTPUT_TEST_RUNNING"));
  TEST_ASSERT_NULL(strstr(text, "out-fan-range"));
}

void test_web_logging_tuning_is_cli_only() {
  std::string pageSource;
  TEST_ASSERT_TRUE(loadTextFile("src/web/WebPages.h", pageSource));
  const char* text = pageSource.c_str();

  TEST_ASSERT_NOT_NULL(strstr(text, "const LOGGING_BASIC_KEYS=['sample_interval_ms','log_daily_enabled','log_all_enabled','log_flush_ms','log_session_name','log_all_max_bytes','log_events_max_bytes'];"));
  TEST_ASSERT_NULL(strstr(text, "'log_io_budget_ms','log_mount_retry_ms','log_write_retry_backoff_ms','log_max_write_retries'"));
}

void test_serial_summary_runs_before_deferred_work_and_keeps_cadence() {
  std::string mainSource;
  TEST_ASSERT_TRUE(loadTextFile("src/main.cpp", mainSource));
  const char* text = mainSource.c_str();

  const size_t summaryPos = mainSource.find("printSerialSummary(nowMs);");
  const size_t deferredPos = mainSource.find("g_app.processDeferred();");
  TEST_ASSERT_TRUE(summaryPos != std::string::npos);
  TEST_ASSERT_TRUE(deferredPos != std::string::npos);
  TEST_ASSERT_TRUE(summaryPos < deferredPos);

  TEST_ASSERT_NOT_NULL(strstr(text, "if (g_nextSerialSummaryMs == 0U) {"));
  TEST_ASSERT_NOT_NULL(strstr(text, "do {"));
  TEST_ASSERT_NOT_NULL(strstr(text, "g_nextSerialSummaryMs += intervalMs;"));
  TEST_ASSERT_NOT_NULL(strstr(text, "} while (static_cast<int32_t>(nowMs - g_nextSerialSummaryMs) >= 0);"));
}

void test_no_unbounded_state_mutex_waits() {
  std::string source;
  TEST_ASSERT_TRUE(loadTextFile("src/TFLunaControl.cpp", source));
  const char* text = source.c_str();

  TEST_ASSERT_NULL(strstr(text, "portMAX_DELAY"));
  TEST_ASSERT_NOT_NULL(strstr(text, "stateMutexTimeoutMs"));
}

void test_nothrow_allocation_paths_for_runtime_objects() {
  std::string appSource;
  std::string ledSource;
  TEST_ASSERT_TRUE(loadTextFile("src/TFLunaControl.cpp", appSource));
  TEST_ASSERT_TRUE(loadTextFile("src/devices/StatusLedAdapter.cpp", ledSource));

  TEST_ASSERT_NOT_NULL(strstr(appSource.c_str(), "new (std::nothrow) Impl()"));
  TEST_ASSERT_NOT_NULL(
      strstr(ledSource.c_str(), "new (std::nothrow) ::StatusLed::StatusLed()"));
}

void test_i2c_token_wrap_guards_present() {
  std::string taskSource;
  std::string orchestratorSource;
  TEST_ASSERT_TRUE(loadTextFile("src/i2c/I2cTask.cpp", taskSource));
  TEST_ASSERT_TRUE(loadTextFile("src/i2c/I2cOrchestrator.cpp", orchestratorSource));

  TEST_ASSERT_NOT_NULL(strstr(taskSource.c_str(), "nextNonZeroToken"));
  TEST_ASSERT_NOT_NULL(strstr(orchestratorSource.c_str(), "nextNonZeroToken"));
}

class FakeTryLock final : public IWebTryLock {
 public:
  bool tryLock() override {
    if (!allowLock) {
      return false;
    }
    lockCount++;
    held = true;
    return true;
  }

  void unlock() override {
    if (held) {
      unlockCount++;
      held = false;
    }
  }

  bool allowLock = true;
  bool held = false;
  uint32_t lockCount = 0;
  uint32_t unlockCount = 0;
};

void test_web_lock_guard_busy_path() {
  FakeTryLock lock;
  lock.allowLock = false;
  bool snapshotCalled = false;

  OrderedWebReadGuard guard(lock);
  const bool ok = guard.tryAcquireScratchThenSnapshot([&]() {
    snapshotCalled = true;
    return true;
  });

  TEST_ASSERT_FALSE(ok);
  TEST_ASSERT_FALSE(snapshotCalled);
  TEST_ASSERT_FALSE(lock.held);
  TEST_ASSERT_EQUAL_UINT32(0, lock.unlockCount);
}

void test_web_lock_guard_snapshot_fail_releases_lock() {
  FakeTryLock lock;
  bool snapshotCalled = false;

  {
    OrderedWebReadGuard guard(lock);
    const bool ok = guard.tryAcquireScratchThenSnapshot([&]() {
      snapshotCalled = true;
      return false;
    });
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_FALSE(guard.isHeld());
  }

  TEST_ASSERT_TRUE(snapshotCalled);
  TEST_ASSERT_FALSE(lock.held);
  TEST_ASSERT_EQUAL_UINT32(1, lock.lockCount);
  TEST_ASSERT_EQUAL_UINT32(1, lock.unlockCount);
}

void test_web_scratch_ram_guardrails() {
  const size_t graphBytes = WebServer::GRAPH_SCRATCH_BYTES;
  const size_t eventBytes = WebServer::EVENT_SCRATCH_BYTES;
  const size_t totalBytes = graphBytes + eventBytes;

  char msg[128];
  snprintf(msg,
           sizeof(msg),
           "scratch graph=%u event=%u total=%u",
           static_cast<unsigned>(graphBytes),
           static_cast<unsigned>(eventBytes),
           static_cast<unsigned>(totalBytes));
  TEST_MESSAGE(msg);

  TEST_ASSERT_TRUE(graphBytes <= WebServer::MAX_GRAPH_SCRATCH_BYTES);
  TEST_ASSERT_TRUE(eventBytes <= WebServer::MAX_EVENT_SCRATCH_BYTES);
  TEST_ASSERT_TRUE(totalBytes <= WebServer::MAX_TOTAL_SCRATCH_BYTES);
}

class FakeI2cPort : public II2cRequestPort {
 public:
  Status begin(const HardwareSettings& config, const RuntimeSettings& settings) override {
    (void)config;
    (void)settings;
    _started = true;
    _requestCount = 0;
    _resultHead = 0;
    _resultCount = 0;
    _metrics = I2cBusMetrics{};
    return Ok();
  }

  void end() override {
    _started = false;
  }

  Status enqueue(const I2cRequest& request, uint32_t nowMs) override {
    (void)nowMs;
    if (!_started) {
      return Status(Err::NOT_INITIALIZED, 0, "fake i2c not started");
    }
    if (_requestCount >= kRequestCapacity) {
      _metrics.requestOverflowCount++;
      return Status(Err::RESOURCE_BUSY, 0, "fake request queue full");
    }
    _requests[_requestCount++] = request;
    _metrics.requestQueueDepth = _requestCount;
    return Ok();
  }

  bool dequeueResult(I2cResult& out) override {
    if (_resultCount == 0) {
      return false;
    }
    out = _results[_resultHead];
    _resultHead = (_resultHead + 1) % kResultCapacity;
    _resultCount--;
    _metrics.resultQueueDepth = _resultCount;
    return true;
  }

  void tick(uint32_t nowMs) override { (void)nowMs; }
  void applySettings(const RuntimeSettings& settings, uint32_t nowMs) override {
    (void)settings;
    (void)nowMs;
  }

  I2cBusMetrics getMetrics() const override { return _metrics; }
  HealthState health() const override { return HealthState::OK; }

  bool pushResult(const I2cResult& result) {
    if (_resultCount >= kResultCapacity) {
      _metrics.resultDroppedCount++;
      return false;
    }
    const size_t idx = (_resultHead + _resultCount) % kResultCapacity;
    _results[idx] = result;
    _resultCount++;
    _metrics.resultQueueDepth = _resultCount;
    return true;
  }

  size_t requestCount() const { return _requestCount; }
  const I2cRequest& requestAt(size_t index) const { return _requests[index]; }

 private:
  static constexpr size_t kRequestCapacity = 256;
  static constexpr size_t kResultCapacity = 64;

  bool _started = false;
  I2cBusMetrics _metrics{};
  I2cRequest _requests[kRequestCapacity]{};
  size_t _requestCount = 0;
  I2cResult _results[kResultCapacity]{};
  size_t _resultHead = 0;
  size_t _resultCount = 0;
};

void test_display_refresh_feature_gate_disabled_by_default() {
  FakeI2cPort port;
  HardwareSettings cfg;
  cfg.i2cSda = 8;
  cfg.i2cScl = 9;
  AppSettings app;
  app.enableDisplay = true;  // Runtime intent ON, but build-time gate is OFF in tests.

  RuntimeSettings s;
  s.restoreDefaults();
  s.i2cRequestsPerTick = 4;
  s.i2cEnvPollMs = 60000;
  s.i2cRtcPollMs = 60000;
  s.i2cDisplayPollMs = 1;

  TEST_ASSERT_TRUE(port.begin(cfg, s).ok());
  I2cOrchestrator orch;
  TEST_ASSERT_TRUE(orch.begin(cfg, app, s, &port).ok());

  orch.tick(0);
  orch.tick(10);

  for (size_t i = 0; i < port.requestCount(); ++i) {
    TEST_ASSERT_NOT_EQUAL(I2cOpType::DISPLAY_REFRESH, port.requestAt(i).op);
  }
}

class MockI2cBackend : public II2cBackend {
 public:
  const char* name() const override { return _backendName; }
  bool supportsDeterministicTimeout() const override { return _deterministicTimeout; }
  bool isAvailable() const override { return true; }

  Status begin(const I2cBackendConfig& config) override {
    _config = config;
    _started = true;
    return _beginStatus;
  }

  void end() override { _started = false; }

  Status applyConfig(const I2cBackendConfig& config) override {
    _config = config;
    return _applyStatus;
  }

  Status reset(const I2cBackendConfig& config) override {
    _config = config;
    _resetCount++;
    return _resetStatus;
  }

  Status transfer(const I2cTransfer& transfer, uint32_t& durationUs) override {
    _lastTransfer = transfer;
    _transferCount++;
    durationUs = _durationUs;
    if (transfer.rxData != nullptr && transfer.rxLen > 0) {
      for (size_t i = 0; i < transfer.rxLen; ++i) {
        transfer.rxData[i] = static_cast<uint8_t>(i & 0xFFU);
      }
    }
    return _transferStatus;
  }

  void setBackendName(const char* name) { _backendName = name; }
  void setDeterministicTimeout(bool deterministic) { _deterministicTimeout = deterministic; }
  void setBeginStatus(const Status& status) { _beginStatus = status; }
  void setApplyStatus(const Status& status) { _applyStatus = status; }
  void setResetStatus(const Status& status) { _resetStatus = status; }
  void setTransferStatus(const Status& status) { _transferStatus = status; }
  void setDurationUs(uint32_t durationUs) { _durationUs = durationUs; }

  size_t transferCount() const { return _transferCount; }
  size_t resetCount() const { return _resetCount; }
  I2cTransfer lastTransfer() const { return _lastTransfer; }

 private:
  const char* _backendName = "mock";
  bool _deterministicTimeout = true;
  bool _started = false;
  I2cBackendConfig _config{};
  Status _beginStatus = Ok();
  Status _applyStatus = Ok();
  Status _resetStatus = Ok();
  Status _transferStatus = Ok();
  uint32_t _durationUs = 1000;
  size_t _transferCount = 0;
  size_t _resetCount = 0;
  I2cTransfer _lastTransfer{};
};

struct PowerHookState {
  uint32_t calls = 0;
  uint32_t lastMs = 0;
  Status nextStatus = Ok();
};

static Status countingPowerHook(uint32_t nowMs, void* context) {
  PowerHookState* state = static_cast<PowerHookState*>(context);
  if (state == nullptr) {
    return Status(Err::INVALID_CONFIG, 0, "null power hook context");
  }
  state->calls++;
  state->lastMs = nowMs;
  return state->nextStatus;
}

void test_i2c_request_queue_bounded() {
  MockI2cBackend backend;
  backend.setBackendName("idf_mock");
  backend.setDeterministicTimeout(true);

  I2cTask task;
  task.setBackendForTest(&backend);
  HardwareSettings cfg;
  cfg.i2cSda = 8;
  cfg.i2cScl = 9;

  RuntimeSettings s;
  s.restoreDefaults();
  TEST_ASSERT_TRUE(task.begin(cfg, s).ok());

  I2cRequest req{};
  req.op = I2cOpType::PROBE;
  req.deviceId = DeviceId::I2C_BUS;
  req.address = 0x44;
  req.timeoutMs = 10;

  for (size_t i = 0; i < I2cTask::REQUEST_QUEUE_CAPACITY; ++i) {
    TEST_ASSERT_TRUE(task.enqueue(req, 100 + static_cast<uint32_t>(i)).ok());
  }
  TEST_ASSERT_FALSE(task.enqueue(req, 999).ok());

  const I2cBusMetrics metrics = task.getMetrics();
  TEST_ASSERT_EQUAL_UINT32(1, metrics.requestOverflowCount);
  task.end();
}

void test_i2c_orchestrator_prioritization() {
  FakeI2cPort port;
  HardwareSettings cfg;
  cfg.i2cSda = 8;
  cfg.i2cScl = 9;

  RuntimeSettings s;
  s.restoreDefaults();
  s.i2cRequestsPerTick = 1;
  s.i2cEnvPollMs = 1000;
  s.i2cRtcPollMs = 1000;
  s.i2cDisplayPollMs = 1000;

  TEST_ASSERT_TRUE(port.begin(cfg, s).ok());
  I2cOrchestrator orch;
  TEST_ASSERT_TRUE(orch.begin(cfg, AppSettings(), s, &port).ok());

  orch.tick(1000);
  orch.tick(1001);
  orch.tick(1002);

  TEST_ASSERT_TRUE(port.requestCount() >= 2);
  TEST_ASSERT_NOT_EQUAL(I2cOpType::DISPLAY_REFRESH, port.requestAt(0).op);
  TEST_ASSERT_NOT_EQUAL(I2cOpType::DISPLAY_REFRESH, port.requestAt(1).op);
}

void test_env_oneshot_nonblocking() {
  FakeI2cPort port;
  HardwareSettings cfg;
  cfg.i2cSda = 8;
  cfg.i2cScl = 9;

  RuntimeSettings s;
  s.restoreDefaults();
  s.i2cRequestsPerTick = 2;
  s.i2cEnvPollMs = 10;
  s.i2cEnvConversionWaitMs = 20;
  s.i2cRtcPollMs = 60000;
  s.i2cDisplayPollMs = 60000;

  TEST_ASSERT_TRUE(port.begin(cfg, s).ok());
  I2cOrchestrator orch;
  TEST_ASSERT_TRUE(orch.begin(cfg, AppSettings(), s, &port).ok());

  orch.tick(1000);
  size_t initialCount = port.requestCount();
  TEST_ASSERT_TRUE(initialCount >= 1);

  bool sawEnvTrigger = false;
  uint32_t envTriggerToken = 0;
  for (size_t i = 0; i < port.requestCount(); ++i) {
    if (port.requestAt(i).op == I2cOpType::ENV_TRIGGER_ONESHOT) {
      sawEnvTrigger = true;
      envTriggerToken = port.requestAt(i).token;
      break;
    }
  }
  TEST_ASSERT_TRUE(sawEnvTrigger);
  TEST_ASSERT_NOT_EQUAL(0U, envTriggerToken);

  I2cResult triggerOk{};
  triggerOk.op = I2cOpType::ENV_TRIGGER_ONESHOT;
  triggerOk.deviceId = DeviceId::ENV;
  triggerOk.status = Ok();
  triggerOk.token = envTriggerToken;
  triggerOk.address = 0x44;
  TEST_ASSERT_TRUE(port.pushResult(triggerOk));

  orch.tick(1005);
  size_t countAfterShortWait = port.requestCount();
  orch.tick(1025);
  size_t countAfterReady = port.requestCount();
  TEST_ASSERT_TRUE(countAfterReady >= countAfterShortWait);

  uint32_t envReadToken = 0;
  for (size_t i = 0; i < port.requestCount(); ++i) {
    if (port.requestAt(i).op == I2cOpType::ENV_READ_ONESHOT) {
      envReadToken = port.requestAt(i).token;
      break;
    }
  }
  TEST_ASSERT_NOT_EQUAL(0U, envReadToken);

  I2cResult ok{};
  ok.op = I2cOpType::ENV_READ_ONESHOT;
  ok.deviceId = DeviceId::ENV;
  ok.status = Ok();
  ok.token = envReadToken;
  ok.address = 0x44;
  ok.dataLen = 6;
  ok.data[0] = 0x66;
  ok.data[1] = 0x66;
  ok.data[2] = 0x00;
  ok.data[3] = 0x88;
  ok.data[4] = 0x88;
  ok.data[5] = 0x00;
  TEST_ASSERT_TRUE(port.pushResult(ok));

  orch.tick(1030);
  Sample sample{};
  TEST_ASSERT_TRUE(orch.fillEnvSample(sample, 1030).ok());
  TEST_ASSERT_TRUE((sample.validMask & VALID_TEMP) != 0);
  TEST_ASSERT_TRUE((sample.validMask & VALID_RH) != 0);
}

void test_env_bme280_packed_result_parsing() {
  FakeI2cPort port;
  HardwareSettings cfg;
  cfg.i2cSda = 8;
  cfg.i2cScl = 9;

  RuntimeSettings s;
  s.restoreDefaults();
  s.i2cEnvAddress = 0x76;
  s.i2cRequestsPerTick = 2;
  s.i2cEnvPollMs = 10;
  s.i2cEnvConversionWaitMs = 20;
  s.i2cRtcPollMs = 60000;
  s.i2cDisplayPollMs = 60000;

  TEST_ASSERT_TRUE(port.begin(cfg, s).ok());
  I2cOrchestrator orch;
  TEST_ASSERT_TRUE(orch.begin(cfg, AppSettings(), s, &port).ok());

  orch.tick(1000);
  uint32_t envTriggerToken = 0;
  for (size_t i = 0; i < port.requestCount(); ++i) {
    if (port.requestAt(i).op == I2cOpType::ENV_TRIGGER_ONESHOT) {
      envTriggerToken = port.requestAt(i).token;
      break;
    }
  }
  TEST_ASSERT_NOT_EQUAL(0U, envTriggerToken);

  I2cResult triggerOk{};
  triggerOk.op = I2cOpType::ENV_TRIGGER_ONESHOT;
  triggerOk.deviceId = DeviceId::ENV;
  triggerOk.status = Ok();
  triggerOk.token = envTriggerToken;
  triggerOk.address = 0x76;
  TEST_ASSERT_TRUE(port.pushResult(triggerOk));

  orch.tick(1005);
  orch.tick(1025);

  uint32_t envReadToken = 0;
  for (size_t i = 0; i < port.requestCount(); ++i) {
    if (port.requestAt(i).op == I2cOpType::ENV_READ_ONESHOT) {
      envReadToken = port.requestAt(i).token;
      break;
    }
  }
  TEST_ASSERT_NOT_EQUAL(0U, envReadToken);

  I2cResult readOk{};
  readOk.op = I2cOpType::ENV_READ_ONESHOT;
  readOk.deviceId = DeviceId::ENV;
  readOk.status = Ok();
  readOk.token = envReadToken;
  readOk.address = 0x76;
  readOk.dataLen = 6;
  readOk.data[0] = 0x09;  // temp 23.45 C -> 2345
  readOk.data[1] = 0x29;
  readOk.data[2] = 0x11;  // RH 45.67 % -> 4567
  readOk.data[3] = 0xD7;
  readOk.data[4] = 0x27;  // pressure 1013.2 hPa -> 10132
  readOk.data[5] = 0x94;
  TEST_ASSERT_TRUE(port.pushResult(readOk));

  orch.tick(1030);
  Sample sample{};
  TEST_ASSERT_TRUE(orch.fillEnvSample(sample, 1030).ok());
  TEST_ASSERT_TRUE((sample.validMask & VALID_TEMP) != 0);
  TEST_ASSERT_TRUE((sample.validMask & VALID_RH) != 0);
  TEST_ASSERT_TRUE((sample.validMask & VALID_PRESSURE) != 0);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 23.45f, sample.tempC);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 45.67f, sample.rhPct);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1013.2f, sample.pressureHpa);
}

void test_i2c_scan_completion_ignores_expected_nack_status() {
  FakeI2cPort port;
  HardwareSettings cfg;
  cfg.i2cSda = 8;
  cfg.i2cScl = 9;

  RuntimeSettings s;
  s.restoreDefaults();
  s.i2cRequestsPerTick = 4;
  s.i2cOpTimeoutMs = 20;
  s.i2cEnvPollMs = 60000;
  s.i2cRtcPollMs = 60000;
  s.i2cDisplayPollMs = 60000;

  TEST_ASSERT_TRUE(port.begin(cfg, s).ok());
  I2cOrchestrator orch;
  TEST_ASSERT_TRUE(orch.begin(cfg, AppSettings(), s, &port).ok());
  TEST_ASSERT_TRUE(orch.queueBusScan(0).ok());

  size_t handledRequests = 0;
  bool finished = false;
  for (uint32_t nowMs = 0; nowMs < 5000U; nowMs += 5U) {
    orch.tick(nowMs);

    while (handledRequests < port.requestCount()) {
      const I2cRequest& req = port.requestAt(handledRequests++);
      if (req.op != I2cOpType::PROBE) {
        continue;
      }

      I2cResult result{};
      result.op = req.op;
      result.deviceId = req.deviceId;
      result.token = req.token;
      result.address = req.address;
      result.requestDeadlineMs = req.deadlineMs;
      result.completedMs = nowMs;
      if (req.address == 0x51U || req.address == 0x76U || req.address == 0x3CU) {
        result.status = Ok();
      } else {
        result.status = Status(Err::COMM_FAILURE, 2, "addr nack");
      }
      TEST_ASSERT_TRUE(port.pushResult(result));
    }

    const I2cScanSnapshot scan = orch.scanSnapshot();
    if (scan.complete) {
      finished = true;
      TEST_ASSERT_FALSE(scan.active);
      TEST_ASSERT_EQUAL(Err::OK, scan.lastStatus.code);
      TEST_ASSERT_TRUE(scan.probesNack > 0U);
      TEST_ASSERT_TRUE(scan.foundCount >= 1U);
      break;
    }
  }

  TEST_ASSERT_TRUE(finished);
}

void test_recovery_backoff_policy() {
  RecoveryPolicy policy;
  policy.configure(3, 1000, 8000);

  policy.onFailure();
  policy.onFailure();
  TEST_ASSERT_FALSE(policy.shouldRecover(100));

  policy.onFailure();
  TEST_ASSERT_TRUE(policy.shouldRecover(100));
  policy.onRecovery(100);

  policy.onFailure();
  policy.onFailure();
  policy.onFailure();
  TEST_ASSERT_FALSE(policy.shouldRecover(500));   // less than current backoff (2000)
  TEST_ASSERT_TRUE(policy.shouldRecover(2200));   // past doubled backoff
}

void test_i2c_deadline_expiry_ignores_stale_result() {
  FakeI2cPort port;
  HardwareSettings cfg;
  cfg.i2cSda = 8;
  cfg.i2cScl = 9;

  RuntimeSettings s;
  s.restoreDefaults();
  s.i2cRequestsPerTick = 1;
  s.i2cEnvPollMs = 10;
  s.i2cRtcPollMs = 60000;
  s.i2cDisplayPollMs = 60000;
  s.i2cOpTimeoutMs = 10;

  TEST_ASSERT_TRUE(port.begin(cfg, s).ok());
  I2cOrchestrator orch;
  TEST_ASSERT_TRUE(orch.begin(cfg, AppSettings(), s, &port).ok());

  orch.tick(1000);
  TEST_ASSERT_TRUE(port.requestCount() >= 1);

  const I2cRequest& first = port.requestAt(0);
  const uint32_t requestToken = first.token;
  const uint32_t requestDeadlineMs = first.deadlineMs;
  TEST_ASSERT_NOT_EQUAL(0U, requestToken);
  TEST_ASSERT_NOT_EQUAL(0U, requestDeadlineMs);

  // Move beyond request deadline so orchestrator times out this in-flight operation.
  orch.tick(requestDeadlineMs + 1U);

  I2cResult stale{};
  stale.op = first.op;
  stale.deviceId = first.deviceId;
  stale.token = requestToken;
  stale.status = Ok();
  stale.requestDeadlineMs = requestDeadlineMs;
  stale.completedMs = requestDeadlineMs + 100U;
  stale.late = true;
  TEST_ASSERT_TRUE(port.pushResult(stale));

  orch.tick(requestDeadlineMs + 101U);
  Sample sample{};
  TEST_ASSERT_FALSE(orch.fillEnvSample(sample, requestDeadlineMs + 101U).ok());

  const I2cBusMetrics metrics = orch.busMetrics();
  TEST_ASSERT_TRUE(metrics.staleResultCount >= 1U);
}

void test_rtc_queue_uses_eeprom_safe_timeout_when_backup_persistence_enabled() {
  FakeI2cPort port;
  HardwareSettings cfg;
  cfg.i2cSda = 8;
  cfg.i2cScl = 9;
  AppSettings app;

  RuntimeSettings s;
  s.restoreDefaults();
  s.i2cRequestsPerTick = 1;
  s.i2cEnvPollMs = 60000;
  s.i2cRtcPollMs = 60000;
  s.i2cDisplayPollMs = 60000;
  s.i2cOpTimeoutMs = 20;
  s.i2cRtcBackupMode = 1;
  s.i2cRtcEnableEepromWrites = false;

  TEST_ASSERT_TRUE(port.begin(cfg, s).ok());
  I2cOrchestrator orch;
  TEST_ASSERT_TRUE(orch.begin(cfg, app, s, &port).ok());

  RtcTime time{};
  time.year = 2026;
  time.month = 3;
  time.day = 18;
  time.hour = 12;
  time.minute = 0;
  time.second = 0;
  time.valid = true;
  TEST_ASSERT_TRUE(orch.queueRtcSet(time, 0).ok());

  orch.tick(1000);
  TEST_ASSERT_EQUAL_UINT32(1U, static_cast<uint32_t>(port.requestCount()));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(I2cOpType::RTC_SET_TIME),
                          static_cast<uint8_t>(port.requestAt(0).op));
  TEST_ASSERT_EQUAL_UINT32(kRtcEepromMinTimeoutMs, port.requestAt(0).timeoutMs);
}

void test_rtc_queue_uses_plain_timeout_when_backup_persistence_disabled() {
  FakeI2cPort port;
  HardwareSettings cfg;
  cfg.i2cSda = 8;
  cfg.i2cScl = 9;
  AppSettings app;

  RuntimeSettings s;
  s.restoreDefaults();
  s.i2cRequestsPerTick = 1;
  s.i2cEnvPollMs = 60000;
  s.i2cRtcPollMs = 60000;
  s.i2cDisplayPollMs = 60000;
  s.i2cOpTimeoutMs = 20;
  s.i2cRtcBackupMode = 0;
  s.i2cRtcEnableEepromWrites = false;

  TEST_ASSERT_TRUE(port.begin(cfg, s).ok());
  I2cOrchestrator orch;
  TEST_ASSERT_TRUE(orch.begin(cfg, app, s, &port).ok());

  RtcTime time{};
  time.year = 2026;
  time.month = 3;
  time.day = 18;
  time.hour = 12;
  time.minute = 0;
  time.second = 0;
  time.valid = true;
  TEST_ASSERT_TRUE(orch.queueRtcSet(time, 0).ok());

  orch.tick(1000);
  TEST_ASSERT_EQUAL_UINT32(1U, static_cast<uint32_t>(port.requestCount()));
  TEST_ASSERT_EQUAL_UINT32(20U, port.requestAt(0).timeoutMs);
}

void test_i2c_preflight_fast_fail_on_stuck_bus() {
  HardwareSettings cfg;
  cfg.i2cSda = 8;
  cfg.i2cScl = 9;

  RuntimeSettings s;
  s.restoreDefaults();

  MockI2cBackend backend;
  backend.setBackendName("idf_mock");
  backend.setDeterministicTimeout(true);

  I2cTask task;
  task.setBackendForTest(&backend);
  TEST_ASSERT_TRUE(task.begin(cfg, s).ok());
  task.setForceBusStuckForTest(true);

  I2cRequest req{};
  req.op = I2cOpType::WRITE;
  req.deviceId = DeviceId::RTC;
  req.address = 0x52;
  req.txLen = 1;
  req.tx[0] = 0x00;
  req.timeoutMs = 10;
  TEST_ASSERT_TRUE(task.enqueue(req, 100).ok());

  task.tick(100);

  I2cResult out{};
  TEST_ASSERT_TRUE(task.dequeueResult(out));
  TEST_ASSERT_EQUAL(Err::BUS_STUCK, out.status.code);
  TEST_ASSERT_EQUAL(0U, backend.transferCount());

  const I2cBusMetrics metrics = task.getMetrics();
  TEST_ASSERT_EQUAL_UINT32(1, metrics.stuckBusFastFailCount);
  task.end();
}

void test_i2c_backend_timeout_respected() {
  HardwareSettings cfg;
  cfg.i2cSda = 8;
  cfg.i2cScl = 9;

  RuntimeSettings s;
  s.restoreDefaults();
  s.i2cOpTimeoutMs = 20;

  MockI2cBackend backend;
  backend.setBackendName("idf_mock");
  backend.setDeterministicTimeout(true);
  backend.setTransferStatus(Status(Err::TIMEOUT, 0, "mock timeout"));
  backend.setDurationUs(25000);

  I2cTask task;
  task.setBackendForTest(&backend);
  TEST_ASSERT_TRUE(task.begin(cfg, s).ok());

  I2cRequest req{};
  req.op = I2cOpType::READ;
  req.deviceId = DeviceId::RTC;
  req.address = 0x52;
  req.rxLen = 4;
  req.timeoutMs = 20;
  TEST_ASSERT_TRUE(task.enqueue(req, 100).ok());

  task.tick(100);

  I2cResult out{};
  TEST_ASSERT_TRUE(task.dequeueResult(out));
  TEST_ASSERT_EQUAL(Err::TIMEOUT, out.status.code);
  TEST_ASSERT_EQUAL_UINT32(25000, out.durationUs);

  const I2cBusMetrics metrics = task.getMetrics();
  TEST_ASSERT_EQUAL_STRING("idf_mock", metrics.backendName);
  TEST_ASSERT_TRUE(metrics.deterministicTimeout);
  task.end();
}

void test_power_cycle_hook_invocation_is_backoff_gated() {
  HardwareSettings cfg;
  cfg.i2cSda = 8;
  cfg.i2cScl = 9;
  PowerHookState hookState{};
  cfg.i2cPowerCycleHook = countingPowerHook;
  cfg.i2cPowerCycleContext = &hookState;

  RuntimeSettings s;
  s.restoreDefaults();
  s.i2cMaxConsecutiveFailures = 1;
  s.i2cRecoveryBackoffMs = 1000;
  s.i2cRecoveryBackoffMaxMs = 4000;

  MockI2cBackend backend;
  backend.setBackendName("idf_mock");
  backend.setDeterministicTimeout(true);

  I2cTask task;
  task.setBackendForTest(&backend);
  TEST_ASSERT_TRUE(task.begin(cfg, s).ok());
  task.setForceBusStuckForTest(true);

  I2cRequest req{};
  req.op = I2cOpType::WRITE;
  req.deviceId = DeviceId::ENV;
  req.address = 0x44;
  req.txLen = 1;
  req.tx[0] = 0x24;

  TEST_ASSERT_TRUE(task.enqueue(req, 0).ok());
  task.tick(0);
  I2cResult out{};
  TEST_ASSERT_TRUE(task.dequeueResult(out));

  TEST_ASSERT_TRUE(task.enqueue(req, 100).ok());
  task.tick(100);
  TEST_ASSERT_TRUE(task.dequeueResult(out));

  TEST_ASSERT_TRUE(task.enqueue(req, 2201).ok());
  task.tick(2201);
  TEST_ASSERT_TRUE(task.dequeueResult(out));

  TEST_ASSERT_EQUAL_UINT32(2, hookState.calls);
  TEST_ASSERT_TRUE(backend.resetCount() >= 2);
  const I2cBusMetrics metrics = task.getMetrics();
  TEST_ASSERT_TRUE(metrics.powerCycleConfigured);
  TEST_ASSERT_EQUAL_UINT32(2, metrics.powerCycleAttempts);
  TEST_ASSERT_EQUAL(Err::OK, metrics.lastPowerCycleStatus.code);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(I2cRecoveryStage::POWER_CYCLE),
                          static_cast<uint8_t>(metrics.lastRecoveryStage));
  task.end();
}

void test_power_cycle_hook_default_noop_has_zero_side_effects() {
  HardwareSettings cfg;
  cfg.i2cSda = 8;
  cfg.i2cScl = 9;
  cfg.i2cPowerCycleHook = nullptr;
  cfg.i2cPowerCycleContext = nullptr;

  RuntimeSettings s;
  s.restoreDefaults();
  s.i2cMaxConsecutiveFailures = 1;
  s.i2cRecoveryBackoffMs = 1000;
  s.i2cRecoveryBackoffMaxMs = 4000;

  MockI2cBackend backend;
  backend.setBackendName("idf_mock");
  backend.setDeterministicTimeout(true);

  I2cTask task;
  task.setBackendForTest(&backend);
  TEST_ASSERT_TRUE(task.begin(cfg, s).ok());
  task.setForceBusStuckForTest(true);

  I2cRequest req{};
  req.op = I2cOpType::WRITE;
  req.deviceId = DeviceId::ENV;
  req.address = 0x44;
  req.txLen = 1;
  req.tx[0] = 0x24;

  TEST_ASSERT_TRUE(task.enqueue(req, 0).ok());
  task.tick(0);
  I2cResult out{};
  TEST_ASSERT_TRUE(task.dequeueResult(out));
  TEST_ASSERT_EQUAL(Err::BUS_STUCK, out.status.code);

  const I2cBusMetrics metrics = task.getMetrics();
  TEST_ASSERT_FALSE(metrics.powerCycleConfigured);
  TEST_ASSERT_EQUAL_UINT32(0, metrics.powerCycleAttempts);
  TEST_ASSERT_EQUAL(Err::NOT_INITIALIZED, metrics.lastPowerCycleStatus.code);
  task.end();
}

void test_power_cycle_hook_failure_telemetry() {
  HardwareSettings cfg;
  cfg.i2cSda = 8;
  cfg.i2cScl = 9;
  PowerHookState hookState{};
  hookState.nextStatus = Status(Err::HARDWARE_FAULT, 42, "hook failed");
  cfg.i2cPowerCycleHook = countingPowerHook;
  cfg.i2cPowerCycleContext = &hookState;

  RuntimeSettings s;
  s.restoreDefaults();
  s.i2cMaxConsecutiveFailures = 1;
  s.i2cRecoveryBackoffMs = 1000;
  s.i2cRecoveryBackoffMaxMs = 4000;

  MockI2cBackend backend;
  backend.setBackendName("idf_mock");
  backend.setDeterministicTimeout(true);

  I2cTask task;
  task.setBackendForTest(&backend);
  TEST_ASSERT_TRUE(task.begin(cfg, s).ok());
  task.setForceBusStuckForTest(true);

  I2cRequest req{};
  req.op = I2cOpType::WRITE;
  req.deviceId = DeviceId::ENV;
  req.address = 0x44;
  req.txLen = 1;
  req.tx[0] = 0x24;

  TEST_ASSERT_TRUE(task.enqueue(req, 0).ok());
  task.tick(0);
  I2cResult out{};
  TEST_ASSERT_TRUE(task.dequeueResult(out));
  TEST_ASSERT_EQUAL(Err::BUS_STUCK, out.status.code);

  const I2cBusMetrics metrics = task.getMetrics();
  TEST_ASSERT_TRUE(metrics.powerCycleConfigured);
  TEST_ASSERT_EQUAL_UINT32(1, hookState.calls);
  TEST_ASSERT_EQUAL_UINT32(1, metrics.powerCycleAttempts);
  TEST_ASSERT_EQUAL(Err::HARDWARE_FAULT, metrics.lastPowerCycleStatus.code);
  TEST_ASSERT_EQUAL_INT32(42, metrics.lastPowerCycleStatus.detail);
  task.end();
}

void test_slow_op_metrics_and_health_escalation() {
  HardwareSettings cfg;
  cfg.i2cSda = 8;
  cfg.i2cScl = 9;

  RuntimeSettings s;
  s.restoreDefaults();
  s.i2cSlowOpThresholdUs = 50000;
  s.i2cSlowOpDegradeCount = 2;

  MockI2cBackend backend;
  backend.setBackendName("idf_mock");
  backend.setDeterministicTimeout(true);
  backend.setTransferStatus(Ok());
  backend.setDurationUs(60000);

  I2cTask task;
  task.setBackendForTest(&backend);
  TEST_ASSERT_TRUE(task.begin(cfg, s).ok());

  I2cRequest req{};
  req.op = I2cOpType::WRITE_READ;
  req.deviceId = DeviceId::RTC;
  req.address = 0x52;
  req.txLen = 1;
  req.tx[0] = 0x00;
  req.rxLen = 4;

  TEST_ASSERT_TRUE(task.enqueue(req, 0).ok());
  task.tick(0);
  I2cResult out{};
  TEST_ASSERT_TRUE(task.dequeueResult(out));

  TEST_ASSERT_TRUE(task.enqueue(req, 10).ok());
  task.tick(10);
  TEST_ASSERT_TRUE(task.dequeueResult(out));

  const I2cBusMetrics metrics = task.getMetrics();
  TEST_ASSERT_TRUE(metrics.slowOpCount >= 2);
  TEST_ASSERT_TRUE(metrics.recentSlowOpCount >= 2);
  TEST_ASSERT_EQUAL(HealthState::DEGRADED, task.health());
  task.end();
}

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_ring_buffer_order);
  RUN_TEST(test_ring_buffer_oldest_first_returns_latest_window);
  RUN_TEST(test_periodic_timer);
  RUN_TEST(test_settings_validation);
  RUN_TEST(test_default_rtc_address_is_rv3032);
  RUN_TEST(test_board_defaults_prepare_endstop_inputs);
  RUN_TEST(test_settings_validation_extremes);
  RUN_TEST(test_sample_interval_upper_bound_matches_timer_window);
  RUN_TEST(test_app_settings_validation_guards_present);
  RUN_TEST(test_command_queue_overflow_behavior);
  RUN_TEST(test_status_json_bounded_serialization);
  RUN_TEST(test_command_queue_degraded_window_logic);
  RUN_TEST(test_device_json_escaping_and_parseability);
  RUN_TEST(test_graph_sample_json_nan_outputs_null);
  RUN_TEST(test_settings_json_write_only_password);
  RUN_TEST(test_cli_verbosity_uses_named_levels);
  RUN_TEST(test_cli_settings_updates_use_change_hints_and_skip_noops);
  RUN_TEST(test_led_health_debounce_logic);
  RUN_TEST(test_log_flush_due_logic);
  RUN_TEST(test_logged_sample_csv_omits_redundant_time_fields);
  RUN_TEST(test_lidar_stats_reset_clears_running_window);
  RUN_TEST(test_web_server_lifecycle_reinit_safe);
  RUN_TEST(test_web_request_count_clamps);
  RUN_TEST(test_web_graph_events_no_heap_alloc_path);
  RUN_TEST(test_web_lidar_stats_reset_controls_present);
  RUN_TEST(test_faster_distance_refresh_defaults);
  RUN_TEST(test_web_live_device_tab_rerenders_distance_stats);
  RUN_TEST(test_web_i2c_settings_are_cli_only);
  RUN_TEST(test_web_endstops_tab_replaces_outputs_surface);
  RUN_TEST(test_web_logging_tuning_is_cli_only);
  RUN_TEST(test_serial_summary_runs_before_deferred_work_and_keeps_cadence);
  RUN_TEST(test_no_unbounded_state_mutex_waits);
  RUN_TEST(test_nothrow_allocation_paths_for_runtime_objects);
  RUN_TEST(test_i2c_token_wrap_guards_present);
  RUN_TEST(test_web_lock_guard_busy_path);
  RUN_TEST(test_web_lock_guard_snapshot_fail_releases_lock);
  RUN_TEST(test_web_scratch_ram_guardrails);
  RUN_TEST(test_display_refresh_feature_gate_disabled_by_default);
  RUN_TEST(test_i2c_request_queue_bounded);
  RUN_TEST(test_i2c_orchestrator_prioritization);
  RUN_TEST(test_env_oneshot_nonblocking);
  RUN_TEST(test_env_bme280_packed_result_parsing);
  RUN_TEST(test_i2c_scan_completion_ignores_expected_nack_status);
  RUN_TEST(test_recovery_backoff_policy);
  RUN_TEST(test_i2c_deadline_expiry_ignores_stale_result);
  RUN_TEST(test_rtc_queue_uses_eeprom_safe_timeout_when_backup_persistence_enabled);
  RUN_TEST(test_rtc_queue_uses_plain_timeout_when_backup_persistence_disabled);
  RUN_TEST(test_i2c_preflight_fast_fail_on_stuck_bus);
  RUN_TEST(test_i2c_backend_timeout_respected);
  RUN_TEST(test_power_cycle_hook_invocation_is_backoff_gated);
  RUN_TEST(test_power_cycle_hook_default_noop_has_zero_side_effects);
  RUN_TEST(test_power_cycle_hook_failure_telemetry);
  RUN_TEST(test_slow_op_metrics_and_health_escalation);
  return UNITY_END();
}

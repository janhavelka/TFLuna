/**
 * @file main.cpp
 * @brief TFLunaControl firmware entrypoint.
 */

#include <Arduino.h>

#include <stdint.h>

#if __has_include(<esp_task_wdt.h>)
#include <esp_task_wdt.h>
#endif

#include "TFLunaControl/TFLunaControl.h"
#include "config/AppConfig.h"
#include "core/SerialCli.h"
#include "core/SystemClock.h"

static TFLunaControl::TFLunaControl g_app;
static TFLunaControl::SerialCli g_cli(g_app);
static bool g_serialCliEnabled = false;
static uint32_t g_nextSerialSummaryMs = 0;

namespace {

#ifndef TFLUNACTRL_QUICK_TEST_PROFILE
#define TFLUNACTRL_QUICK_TEST_PROFILE 1
#endif

#ifndef TFLUNACTRL_STRESS_MODE
#define TFLUNACTRL_STRESS_MODE 0
#endif

void applyQuickProfileHardware(TFLunaControl::HardwareSettings& hw,
                               const TFLunaControl::StartupProfileSettings& profile) {
  hw.i2cSda = profile.quickProfileI2cSdaPin;
  hw.i2cScl = profile.quickProfileI2cSclPin;
  hw.lidarRx = profile.quickProfileLidarRxPin;
  hw.lidarTx = profile.quickProfileLidarTxPin;
}

TFLunaControl::Status applyQuickProfileSettings(TFLunaControl::TFLunaControl& app,
                                             const TFLunaControl::StartupProfileSettings& profile) {
  TFLunaControl::RuntimeSettings settings = app.getSettings();
  settings.sampleIntervalMs = profile.quickProfileSampleIntervalMs;
  settings.wifiEnabled = true;
  settings.i2cEnvAddress = profile.quickProfileEnvAddress;
  return app.updateSettings(settings, false);
}

void tickStressMode(TFLunaControl::TFLunaControl& app,
                    const TFLunaControl::StartupProfileSettings& profile,
                    uint32_t nowMs) {
  static uint32_t nextRtcMs = 0;
  static uint32_t nextSettingsMs = 0;
  static uint32_t nextRemountMs = 0;
  static bool flip = false;

  if (!profile.stressModeEnabled) {
    return;
  }

  if (static_cast<int32_t>(nowMs - nextRtcMs) >= 0) {
    nextRtcMs = nowMs + profile.stressRtcIntervalMs;
    TFLunaControl::RtcTime t{};
    t.year = profile.stressRtcBaseYear;
    t.month = profile.stressRtcBaseMonth;
    t.day = profile.stressRtcBaseDay;
    t.hour = static_cast<uint8_t>((nowMs / 3600000UL) % 24UL);
    t.minute = static_cast<uint8_t>((nowMs / 60000UL) % 60UL);
    t.second = static_cast<uint8_t>((nowMs / 1000UL) % 60UL);
    t.valid = true;
    (void)app.enqueueSetRtcTime(t);
  }

  if (static_cast<int32_t>(nowMs - nextSettingsMs) >= 0) {
    nextSettingsMs = nowMs + profile.stressSettingsIntervalMs;
    TFLunaControl::RuntimeSettings s = app.getSettings();
    flip = !flip;
    s.sampleIntervalMs = flip ? profile.stressSampleIntervalMsA : profile.stressSampleIntervalMsB;
    (void)app.enqueueApplySettings(s, false);
  }

  if (static_cast<int32_t>(nowMs - nextRemountMs) >= 0) {
    nextRemountMs = nowMs + profile.stressRemountIntervalMs;
    (void)app.enqueueRemountSd();
  }
}

TFLunaControl::HardwareSettings g_hw{};
TFLunaControl::AppSettings g_appSettings{};
TFLunaControl::StartupProfileSettings g_startupProfile{};

void printSerialSummary(uint32_t nowMs) {
  TFLunaControl::RuntimeSettings settings{};
  if (!g_app.tryGetSettingsSnapshot(settings)) {
    return;
  }

  const uint32_t intervalMs =
      (settings.serialPrintIntervalMs == 0U) ? 5000U : settings.serialPrintIntervalMs;
  if (g_nextSerialSummaryMs != 0U &&
      static_cast<int32_t>(nowMs - g_nextSerialSummaryMs) < 0) {
    return;
  }
  g_nextSerialSummaryMs = nowMs + intervalMs;

  TFLunaControl::SystemStatus sys{};
  TFLunaControl::Sample latest{};
  bool hasLatest = false;
  if (!g_app.tryGetStatusSnapshot(sys, latest, hasLatest)) {
    Serial.println("[diag] state busy");
    return;
  }

  const char* timestamp =
      (hasLatest && latest.tsLocal[0] != '\0') ? latest.tsLocal : "uptime";
  if (hasLatest) {
    Serial.printf("[lidar] src=%s ts=%s sample=%lu dist=%ucm strength=%u temp=%.1fC frame=%u signal=%u age=%lums frames=%lu checksum=%lu sync=%lu log=%u file=%s\n",
                  sys.timeSource,
                  timestamp,
                  static_cast<unsigned long>(latest.sampleIndex),
                  static_cast<unsigned int>(latest.distanceCm),
                  static_cast<unsigned int>(latest.strength),
                  static_cast<double>(latest.lidarTempC),
                  latest.validFrame ? 1U : 0U,
                  latest.signalOk ? 1U : 0U,
                  static_cast<unsigned long>(sys.lidarFrameAgeMs),
                  static_cast<unsigned long>(sys.lidarFramesParsed),
                  static_cast<unsigned long>(sys.lidarChecksumErrors),
                  static_cast<unsigned long>(sys.lidarSyncLossCount),
                  (sys.sdMounted && (sys.logAllOk || sys.logDailyOk)) ? 1U : 0U,
                  sys.logCurrentSampleFile);
    return;
  }

  Serial.printf("[lidar] src=%s no-sample frames=%lu checksum=%lu sync=%lu age=%lums log=%u\n",
                sys.timeSource,
                static_cast<unsigned long>(sys.lidarFramesParsed),
                static_cast<unsigned long>(sys.lidarChecksumErrors),
                static_cast<unsigned long>(sys.lidarSyncLossCount),
                static_cast<unsigned long>(sys.lidarFrameAgeMs),
                (sys.sdMounted && (sys.logAllOk || sys.logDailyOk)) ? 1U : 0U);
}

}  // namespace

void setup() {
  g_hw = TFLunaControl::loadHardwareSettings();
  g_appSettings = TFLunaControl::loadAppSettings(g_hw);
  g_startupProfile = TFLunaControl::loadStartupProfileSettings();
  g_startupProfile.quickProfileEnabled = (TFLUNACTRL_QUICK_TEST_PROFILE != 0);
  g_startupProfile.stressModeEnabled = (TFLUNACTRL_STRESS_MODE != 0);

  if (g_startupProfile.quickProfileEnabled) {
    applyQuickProfileHardware(g_hw, g_startupProfile);
  }
  g_appSettings.enableSd = (g_hw.sdCs >= 0);

  // IDF 5.x task WDT safety net.  pioarduino initialises the TWDT at boot
  // (CONFIG_ESP_TASK_WDT_INIT=y, CONFIG_ESP_TASK_WDT_PANIC=y) with zero
  // subscribers.  AsyncSD's enableCore0WDT() subscribes the IDLE task during
  // periodic SD-info free-cluster scans - but IDLE has no hook to feed the
  // WDT, which would trigger a panic after the 5 s default timeout.
  // Reconfigure to non-panic mode with a generous timeout so accidental
  // IDLE subscriptions never crash the board.
#if __has_include(<esp_task_wdt.h>)
  {
    esp_task_wdt_config_t wdt_cfg = {
        .timeout_ms = 30000,
        .idle_core_mask = 0,
        .trigger_panic = false,
    };
    (void)esp_task_wdt_reconfigure(&wdt_cfg);
  }
#endif

  g_serialCliEnabled = (g_appSettings.serialEnabled && g_appSettings.serialBaudRate > 0U);
  if (g_serialCliEnabled) {
    Serial.begin(g_appSettings.serialBaudRate);
    Serial.printf("[boot] TF-Luna UART mapping: sensor TX -> ESP32 RX GPIO%d, sensor RX -> ESP32 TX GPIO%d, UART%u @ 115200\n",
                  g_hw.lidarRx,
                  g_hw.lidarTx,
                  static_cast<unsigned int>(g_hw.lidarUartIndex));
  }

  const TFLunaControl::Status status = g_app.begin(g_hw, g_appSettings);
  if (!status.ok()) {
    if (g_serialCliEnabled) {
      Serial.printf("[E] TFLunaControl begin failed: %u (%s)\n",
                    static_cast<unsigned int>(status.code),
                    status.msg);
    }
    return;
  }

  if (g_startupProfile.quickProfileEnabled) {
    const TFLunaControl::Status settingsStatus = applyQuickProfileSettings(g_app, g_startupProfile);
    if (!settingsStatus.ok() && g_serialCliEnabled) {
      Serial.printf("[E] Quick profile settings failed: %u (%s)\n",
                    static_cast<unsigned int>(settingsStatus.code),
                    settingsStatus.msg);
    }
  }

  if (g_serialCliEnabled) {
    g_cli.begin();
  }
}

void loop() {
  const uint32_t nowMs = TFLunaControl::SystemClock::nowMs();
  g_app.tick(nowMs);
  g_app.processDeferred();
  tickStressMode(g_app, g_startupProfile, nowMs);
  if (g_serialCliEnabled) {
    g_cli.tick(nowMs);
    printSerialSummary(nowMs);
  }
  // Yield each iteration so IDLE tasks can run on dual-core targets.
  delay(1);
}

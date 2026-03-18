/**
 * @file main.cpp
 * @brief CO2Control firmware entrypoint.
 */

#include <Arduino.h>

#include <stdint.h>

#if __has_include(<esp_task_wdt.h>)
#include <esp_task_wdt.h>
#endif

#include "CO2Control/CO2Control.h"
#include "config/AppConfig.h"
#include "core/SerialCli.h"
#include "core/SystemClock.h"

static CO2Control::CO2Control g_app;
static CO2Control::SerialCli g_cli(g_app);
static bool g_serialCliEnabled = false;

namespace {

#ifndef CO2CONTROL_QUICK_TEST_PROFILE
#define CO2CONTROL_QUICK_TEST_PROFILE 1
#endif

#ifndef CO2CONTROL_STRESS_MODE
#define CO2CONTROL_STRESS_MODE 0
#endif

void driveOutputPinLow(int pin) {
  if (pin < 0) {
    return;
  }
  pinMode(static_cast<uint8_t>(pin), OUTPUT);
  digitalWrite(static_cast<uint8_t>(pin), LOW);
}

void primeOutputsLow(const CO2Control::HardwareSettings& hw) {
  // Ensure all configured output channels are electrically LOW as early as possible.
  driveOutputPinLow(hw.mosfet1Pin);
  driveOutputPinLow(hw.mosfet2Pin);
  driveOutputPinLow(hw.relay1Pin);
  driveOutputPinLow(hw.relay2Pin);
}

void applyQuickProfileHardware(CO2Control::HardwareSettings& hw,
                               const CO2Control::StartupProfileSettings& profile) {
  hw.i2cSda = profile.quickProfileI2cSdaPin;
  hw.i2cScl = profile.quickProfileI2cSclPin;
  hw.e2Tx = profile.quickProfileE2TxPin;
  hw.e2Rx = profile.quickProfileE2RxPin;
  hw.e2En = profile.quickProfileE2EnPin;
  if (profile.quickProfileMosfet1Pin >= 0) {
    hw.mosfet1Pin = profile.quickProfileMosfet1Pin;
  }
}

CO2Control::Status applyQuickProfileSettings(CO2Control::CO2Control& app,
                                             const CO2Control::StartupProfileSettings& profile) {
  CO2Control::RuntimeSettings settings = app.getSettings();
  settings.sampleIntervalSec = profile.quickProfileSampleIntervalSec;
  settings.wifiEnabled = true;
  settings.i2cEnvAddress = profile.quickProfileEnvAddress;
  return app.updateSettings(settings, false);
}

void tickStressMode(CO2Control::CO2Control& app,
                    const CO2Control::StartupProfileSettings& profile,
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
    CO2Control::RtcTime t{};
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
    CO2Control::RuntimeSettings s = app.getSettings();
    flip = !flip;
    s.outputsEnabled = flip;
    s.sampleIntervalSec = flip ? profile.stressSampleIntervalA : profile.stressSampleIntervalB;
    (void)app.enqueueApplySettings(s, false);
  }

  if (static_cast<int32_t>(nowMs - nextRemountMs) >= 0) {
    nextRemountMs = nowMs + profile.stressRemountIntervalMs;
    (void)app.enqueueRemountSd();
  }
}

CO2Control::HardwareSettings g_hw{};
CO2Control::AppSettings g_appSettings{};
CO2Control::StartupProfileSettings g_startupProfile{};

}  // namespace

void setup() {
  g_hw = CO2Control::loadHardwareSettings();
  g_appSettings = CO2Control::loadAppSettings(g_hw);
  g_startupProfile = CO2Control::loadStartupProfileSettings();
  g_startupProfile.quickProfileEnabled = (CO2CONTROL_QUICK_TEST_PROFILE != 0);
  g_startupProfile.stressModeEnabled = (CO2CONTROL_STRESS_MODE != 0);

  if (g_startupProfile.quickProfileEnabled) {
    applyQuickProfileHardware(g_hw, g_startupProfile);
  }
  primeOutputsLow(g_hw);
  g_appSettings.enableSd = (g_hw.sdCs >= 0);

  // IDF 5.x task WDT safety net.  pioarduino initialises the TWDT at boot
  // (CONFIG_ESP_TASK_WDT_INIT=y, CONFIG_ESP_TASK_WDT_PANIC=y) with zero
  // subscribers.  AsyncSD's enableCore0WDT() subscribes the IDLE task during
  // periodic SD-info free-cluster scans — but IDLE has no hook to feed the
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
  }

  const CO2Control::Status status = g_app.begin(g_hw, g_appSettings);
  if (!status.ok()) {
    if (g_serialCliEnabled) {
      Serial.printf("[E] CO2Control begin failed: %u (%s)\n",
                    static_cast<unsigned int>(status.code),
                    status.msg);
    }
    return;
  }

  if (g_startupProfile.quickProfileEnabled) {
    const CO2Control::Status settingsStatus = applyQuickProfileSettings(g_app, g_startupProfile);
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
  const uint32_t nowMs = CO2Control::SystemClock::nowMs();
  g_app.tick(nowMs);
  g_app.processDeferred();
  tickStressMode(g_app, g_startupProfile, nowMs);
  if (g_serialCliEnabled) {
    g_cli.tick(nowMs);
  }
  // Yield each iteration so IDLE tasks can run on dual-core targets.
  delay(1);
}

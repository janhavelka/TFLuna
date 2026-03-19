#pragma once

#include "TFLunaControl/AppSettings.h"
#include "TFLunaControl/HardwareSettings.h"

namespace TFLunaControl {

struct StartupProfileSettings {
  bool quickProfileEnabled = false;
  uint32_t quickProfileSampleIntervalMs = 100;
  uint8_t quickProfileEnvAddress = 0x76;
  int quickProfileI2cSdaPin = 8;
  int quickProfileI2cSclPin = 9;
  int quickProfileLidarRxPin = 15;
  int quickProfileLidarTxPin = 14;

  bool stressModeEnabled = false;
  uint32_t stressRtcIntervalMs = 250;
  uint32_t stressSettingsIntervalMs = 1000;
  uint32_t stressRemountIntervalMs = 3000;
  uint32_t stressSampleIntervalMsA = 600;
  uint32_t stressSampleIntervalMsB = 601;
  uint16_t stressRtcBaseYear = 2026;
  uint8_t stressRtcBaseMonth = 1;
  uint8_t stressRtcBaseDay = 1;
};

HardwareSettings loadHardwareSettings();
AppSettings loadAppSettings(const HardwareSettings& hardware);
StartupProfileSettings loadStartupProfileSettings();

}  // namespace TFLunaControl

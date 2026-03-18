#pragma once

#include "CO2Control/AppSettings.h"
#include "CO2Control/HardwareSettings.h"

namespace CO2Control {

struct StartupProfileSettings {
  bool quickProfileEnabled = true;
  uint32_t quickProfileSampleIntervalSec = 5;
  uint8_t quickProfileEnvAddress = 0x76;
  int quickProfileI2cSdaPin = 8;
  int quickProfileI2cSclPin = 9;
  int quickProfileE2TxPin = 17;
  int quickProfileE2RxPin = 18;
  int quickProfileE2EnPin = -1;
  int quickProfileMosfet1Pin = -1;

  bool stressModeEnabled = false;
  uint32_t stressRtcIntervalMs = 250;
  uint32_t stressSettingsIntervalMs = 1000;
  uint32_t stressRemountIntervalMs = 3000;
  uint32_t stressSampleIntervalA = 600;
  uint32_t stressSampleIntervalB = 601;
  uint16_t stressRtcBaseYear = 2026;
  uint8_t stressRtcBaseMonth = 1;
  uint8_t stressRtcBaseDay = 1;
};

HardwareSettings loadHardwareSettings();
AppSettings loadAppSettings(const HardwareSettings& hardware);
StartupProfileSettings loadStartupProfileSettings();

}  // namespace CO2Control

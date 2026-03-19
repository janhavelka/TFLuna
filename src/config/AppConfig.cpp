#include "config/AppConfig.h"

namespace TFLunaControl {

HardwareSettings loadHardwareSettings() {
  HardwareSettings settings{};

  // Preserved board wiring already encoded in the repo:
  // I2C bus (RTC + SSD1315 OLED): SDA=IO8, SCL=IO9
  // SPI bus (SD card): MOSI=IO11, SCK=IO12, MISO=IO13, CS=IO10
  //
  // TF-Luna UART mapping is new for this measurement firmware and is kept
  // explicit here instead of being inferred elsewhere:
  //   TF-Luna TX -> ESP32-S3 RX on GPIO15
  //   TF-Luna RX -> ESP32-S3 TX on GPIO14
  settings.i2cSda = 8;
  settings.i2cScl = 9;
  settings.spiMosi = 11;
  settings.spiSck = 12;
  settings.spiMiso = 13;
  settings.sdCs = 10;
  settings.lidarRx = 15;
  settings.lidarTx = 14;
  settings.lidarUartIndex = 1;
  settings.displayFlipX = true;
  settings.displayFlipY = true;
  settings.endstopUpperPin = 5;
  settings.endstopLowerPin = 6;

#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(ARDUINO_ESP32S3_DEV)
  // ESP32-S3-only nets present on this board revision.
  settings.buttonPin = 38;  // PAIR
  settings.ledPin = 47;     // STAT_LED
#endif

  return settings;
}

AppSettings loadAppSettings(const HardwareSettings& hardware) {
  AppSettings settings{};
  settings.enableSd = (hardware.sdCs >= 0);
  settings.enableDisplay = true;
  return settings;
}

StartupProfileSettings loadStartupProfileSettings() {
  return StartupProfileSettings{};
}

}  // namespace TFLunaControl

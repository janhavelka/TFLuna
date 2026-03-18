#include "config/AppConfig.h"

namespace CO2Control {

HardwareSettings loadHardwareSettings() {
  HardwareSettings settings{};

  // Provisional custom-board mapping (from provided ESP32-S3 schematic):
  // I2C: SDA=IO8, SCL=IO9
  // EE871 E2: TX=IO17, RX=IO18, no EN line
  // RS485: TX=IO17, RX=IO18, DE=IO21 (currently stubbed in firmware)
  // SPI bus: MOSI=IO11, SCK=IO12, MISO=IO13, CS=IO10
  settings.i2cSda = 8;
  settings.i2cScl = 9;
  settings.e2Tx = 17;
  settings.e2Rx = 18;
  settings.e2En = -1;
  settings.rs485Tx = 17;
  settings.rs485Rx = 18;
  settings.rs485De = 21;
  settings.spiMosi = 11;
  settings.spiSck = 12;
  settings.spiMiso = 13;
  settings.sdCs = 10;

  settings.mosfet1Pin = 35;
  // mosfet1ActiveHigh defaults to true: output LOW (off) at boot,
  // driven HIGH when the hysteresis threshold is reached.

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
  return settings;
}

StartupProfileSettings loadStartupProfileSettings() {
  return StartupProfileSettings{};
}

}  // namespace CO2Control

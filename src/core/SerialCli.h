#pragma once

#include <stddef.h>
#include <stdint.h>

#include "TFLunaControl/TFLunaControl.h"
#include "TFLunaControl/Status.h"

namespace TFLunaControl {

class SerialCli {
 public:
  explicit SerialCli(TFLunaControl& app);

  Status begin();
  void tick(uint32_t nowMs);
  void end();

 private:
  void printHelp(const char* topic = nullptr);
  void printStatus();
  void printDevices();
  void printDevice(const char* name);
  void printSamples(size_t count);
  void printEvents(size_t count);
  void printRead(const char* which);
  void printSettings(const char* section);
  void printBootConfig(const char* which);
  void printSettableKeys(const char* group);
  void printOutputState();
  void printSdInfo();
  void printSdList(const char* path);
  void printDiagnostics(const char* scope);
  void printRtcDiagnostics();
  void printI2cScan();
  void printI2cProbe();
  void applyPreset(const char* domain, const char* preset, bool persist);
  bool loadDeviceStatuses(size_t& outCount);
  const DeviceStatus* findDeviceInScratch(const char* name, size_t count) const;
  uint8_t currentVerbosity() const;
  void queueSettingsUpdate(const RuntimeSettings& settings, bool persist);
  void executeLine(char* line, uint32_t nowMs);

  static constexpr size_t LINE_BYTES = HardwareSettings::CLI_LINE_BYTES;

  TFLunaControl& _app;
  char _line[LINE_BYTES] = {};
  size_t _lineLen = 0;
  DeviceStatus _deviceScratch[DEVICE_COUNT] = {};
};

}  // namespace TFLunaControl

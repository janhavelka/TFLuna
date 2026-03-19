#pragma once

#include <stdint.h>

#include "TFLunaControl/HardwareSettings.h"
#include "TFLunaControl/Status.h"

namespace TFLunaControl {

class EndstopAdapter {
 public:
  struct Snapshot {
    int upperPin = -1;
    bool upperConfigured = false;
    bool upperActiveLow = true;
    bool upperRawHigh = false;
    bool upperTriggered = false;
    uint32_t upperLastChangeMs = 0;

    int lowerPin = -1;
    bool lowerConfigured = false;
    bool lowerActiveLow = true;
    bool lowerRawHigh = false;
    bool lowerTriggered = false;
    uint32_t lowerLastChangeMs = 0;
  };

  Status begin(const HardwareSettings& config);
  void tick(uint32_t nowMs);
  void end();

  const Snapshot& snapshot() const { return _snapshot; }
  Status lastStatus() const { return _lastStatus; }

 private:
  static bool computeTriggered(bool rawHigh, bool activeLow);
  void sampleChannel(bool upper, uint32_t nowMs);

  Snapshot _snapshot{};
  Status _lastStatus = Status(Err::NOT_INITIALIZED, 0, "endstops disabled");
  bool _initialized = false;
};

}  // namespace TFLunaControl

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "TFLunaControl/Types.h"

namespace TFLunaControl {

class TfLunaFrameParser {
 public:
  void reset();
  void configure(uint16_t minStrength, uint16_t maxDistanceCm);
  bool pushByte(uint8_t value, uint32_t nowMs, LidarMeasurement& outMeasurement);

  uint32_t framesParsed() const { return _framesParsed; }
  uint32_t checksumErrors() const { return _checksumErrors; }
  uint32_t syncLossCount() const { return _syncLossCount; }

 private:
  enum class ParseState : uint8_t {
    WAIT_HEADER_1 = 0,
    WAIT_HEADER_2,
    READ_PAYLOAD
  };

  bool finalizeFrame(uint32_t nowMs, LidarMeasurement& outMeasurement);
  void resetToHeaderSearch(uint8_t lastByte);

  ParseState _state = ParseState::WAIT_HEADER_1;
  uint8_t _frame[9] = {0};
  uint8_t _index = 0;
  uint16_t _minStrength = 100;
  uint16_t _maxDistanceCm = 800;
  uint32_t _framesParsed = 0;
  uint32_t _checksumErrors = 0;
  uint32_t _syncLossCount = 0;
};

}  // namespace TFLunaControl

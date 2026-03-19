#include "devices/TfLunaFrameParser.h"

namespace TFLunaControl {

namespace {

bool checksumMatches(const uint8_t* frame) {
  uint16_t sum = 0U;
  for (size_t i = 0; i < 8U; ++i) {
    sum = static_cast<uint16_t>(sum + frame[i]);
  }
  return static_cast<uint8_t>(sum & 0xFFU) == frame[8];
}

}  // namespace

void TfLunaFrameParser::reset() {
  _state = ParseState::WAIT_HEADER_1;
  _index = 0U;
  _framesParsed = 0U;
  _checksumErrors = 0U;
  _syncLossCount = 0U;
}

void TfLunaFrameParser::configure(uint16_t minStrength, uint16_t maxDistanceCm) {
  _minStrength = minStrength;
  _maxDistanceCm = maxDistanceCm;
}

bool TfLunaFrameParser::pushByte(uint8_t value, uint32_t nowMs, LidarMeasurement& outMeasurement) {
  switch (_state) {
    case ParseState::WAIT_HEADER_1:
      if (value == 0x59U) {
        _frame[0] = value;
        _index = 1U;
        _state = ParseState::WAIT_HEADER_2;
      }
      return false;

    case ParseState::WAIT_HEADER_2:
      if (value == 0x59U) {
        _frame[1] = value;
        _index = 2U;
        _state = ParseState::READ_PAYLOAD;
      } else {
        ++_syncLossCount;
        resetToHeaderSearch(value);
      }
      return false;

    case ParseState::READ_PAYLOAD:
      _frame[_index++] = value;
      if (_index < sizeof(_frame)) {
        return false;
      }
      {
        const bool gotFrame = finalizeFrame(nowMs, outMeasurement);
        resetToHeaderSearch(value);
        return gotFrame;
      }
  }
  return false;
}

bool TfLunaFrameParser::finalizeFrame(uint32_t nowMs, LidarMeasurement& outMeasurement) {
  if (!checksumMatches(_frame)) {
    ++_checksumErrors;
    return false;
  }

  const uint16_t distanceCm =
      static_cast<uint16_t>(_frame[2] | (static_cast<uint16_t>(_frame[3]) << 8U));
  const uint16_t strength =
      static_cast<uint16_t>(_frame[4] | (static_cast<uint16_t>(_frame[5]) << 8U));
  const uint16_t rawTemperature =
      static_cast<uint16_t>(_frame[6] | (static_cast<uint16_t>(_frame[7]) << 8U));

  outMeasurement.distanceCm = distanceCm;
  outMeasurement.strength = strength;
  outMeasurement.temperatureC = (static_cast<float>(rawTemperature) / 8.0f) - 256.0f;
  outMeasurement.validFrame = true;
  outMeasurement.signalOk =
      (distanceCm > 0U) && (distanceCm <= _maxDistanceCm) && (strength >= _minStrength);
  outMeasurement.capturedMs = nowMs;

  ++_framesParsed;
  return true;
}

void TfLunaFrameParser::resetToHeaderSearch(uint8_t lastByte) {
  if (lastByte == 0x59U) {
    _frame[0] = 0x59U;
    _index = 1U;
    _state = ParseState::WAIT_HEADER_2;
    return;
  }

  _index = 0U;
  _state = ParseState::WAIT_HEADER_1;
}

}  // namespace TFLunaControl

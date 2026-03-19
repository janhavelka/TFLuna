#include "control/OutputLogic.h"

namespace TFLunaControl {

bool OutputLogic::update(float value, bool valid, uint32_t nowMs) {
  bool desired = _state;

  if (!valid) {
    desired = false;
  } else if (_state) {
    if (value <= _rule.offThreshold) {
      desired = false;
    }
  } else {
    if (value >= _rule.onThreshold) {
      desired = true;
    }
  }

  if (desired != _state) {
    const uint32_t elapsed = nowMs - _lastChangeMs;
    if (_state) {
      if (elapsed < _rule.minOnMs) {
        return _state;
      }
    } else {
      if (elapsed < _rule.minOffMs) {
        return _state;
      }
    }
    _state = desired;
    _lastChangeMs = nowMs;
  }

  return _state;
}

void OutputLogic::reset(bool state, uint32_t nowMs) {
  _state = state;
  if (state) {
    _lastChangeMs = nowMs - _rule.minOnMs;
  } else {
    _lastChangeMs = nowMs - _rule.minOffMs;
  }
}

}  // namespace TFLunaControl

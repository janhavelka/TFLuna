#pragma once

#include <stdint.h>

namespace CO2Control {

class RecoveryPolicy {
 public:
  void configure(uint8_t threshold, uint32_t backoffMs, uint32_t maxBackoffMs) {
    _threshold = threshold;
    _baseBackoffMs = backoffMs;
    _maxBackoffMs = maxBackoffMs;
    if (_maxBackoffMs < _baseBackoffMs) {
      _maxBackoffMs = _baseBackoffMs;
    }
  }

  void onSuccess() {
    _consecutiveFailures = 0;
    _hasRecovered = false;
    _currentBackoffMs = _baseBackoffMs;
  }

  void onFailure() {
    if (_consecutiveFailures < 0xFFFFFFFFUL) {
      _consecutiveFailures++;
    }
  }

  bool shouldRecover(uint32_t nowMs) const {
    if (_consecutiveFailures < _threshold) {
      return false;
    }
    if (!_hasRecovered) {
      return true;
    }
    return (nowMs - _lastRecoveryMs) >= currentBackoffMs();
  }

  void onRecovery(uint32_t nowMs) {
    _lastRecoveryMs = nowMs;
    _hasRecovered = true;
    if (_currentBackoffMs < _baseBackoffMs) {
      _currentBackoffMs = _baseBackoffMs;
    } else if (_currentBackoffMs < _maxBackoffMs) {
      const uint32_t doubled = _currentBackoffMs * 2UL;
      _currentBackoffMs = (doubled > _maxBackoffMs) ? _maxBackoffMs : doubled;
    }
  }

  uint32_t consecutiveFailures() const { return _consecutiveFailures; }
  uint32_t lastRecoveryMs() const { return _lastRecoveryMs; }
  uint32_t currentBackoffMs() const {
    if (_currentBackoffMs < _baseBackoffMs) {
      return _baseBackoffMs;
    }
    return _currentBackoffMs;
  }

 private:
  uint32_t _consecutiveFailures = 0;
  uint32_t _lastRecoveryMs = 0;
  bool _hasRecovered = false;
  uint8_t _threshold = 3;
  uint32_t _baseBackoffMs = 1000;
  uint32_t _maxBackoffMs = 30000;
  uint32_t _currentBackoffMs = 1000;
};

}  // namespace CO2Control

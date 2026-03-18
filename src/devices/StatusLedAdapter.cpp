#include "devices/StatusLedAdapter.h"

#ifdef ARDUINO
#include <new>
#endif

namespace CO2Control {

StatusLedAdapter::HealthState StatusLedAdapter::debounceHealth(HealthState target,
                                                               uint32_t nowMs,
                                                               uint32_t debounceMs,
                                                               HealthDebounceState& state) {
  if (!state.initialized) {
    state.initialized = true;
    state.stable = target;
    state.pending = target;
    state.pendingSinceMs = nowMs;
    return state.stable;
  }

  // Fault is always urgent and should be shown immediately.
  if (target == HealthState::FAULT) {
    state.stable = HealthState::FAULT;
    state.pending = HealthState::FAULT;
    state.pendingSinceMs = nowMs;
    return state.stable;
  }

  if (target == state.stable) {
    state.pending = target;
    state.pendingSinceMs = nowMs;
    return state.stable;
  }

  if (target != state.pending) {
    state.pending = target;
    state.pendingSinceMs = nowMs;
    return state.stable;
  }

  if ((nowMs - state.pendingSinceMs) >= debounceMs) {
    state.stable = target;
  }

  return state.stable;
}

Status StatusLedAdapter::begin(const HardwareSettings& config) {
  _pin = config.ledPin;
  _count = config.ledCount;
  _wifiIndex = config.wifiLedIndex;
  _healthIndex = config.healthLedIndex;
  _brightness = config.ledBrightness;
  _tickCadenceMs = (config.ledSmoothStepMs == 0U) ? 1U : config.ledSmoothStepMs;
  _nextTickDueMs = 0U;
  _dirty = true;
  _tickKick = true;

#ifdef ARDUINO
  if (_pin < 0 || _count == 0) {
    return Ok();
  }
  if (_count > ::StatusLed::StatusLed::kMaxLedCount) {
    return Status(Err::INVALID_CONFIG,
                  static_cast<int32_t>(_count),
                  "LED count exceeds StatusLED limit");
  }
  _strip = new (std::nothrow) ::StatusLed::StatusLed();
  if (!_strip) {
    return Status(Err::OUT_OF_MEMORY, 0, "LED alloc failed");
  }

  ::StatusLed::Config ledCfg{};
  ledCfg.dataPin = _pin;
  ledCfg.ledCount = static_cast<uint8_t>(_count);
  ledCfg.colorOrder = ::StatusLed::ColorOrder::GRB;
  ledCfg.rmtChannel = 0;
  ledCfg.globalBrightness = _brightness;
  ledCfg.smoothStepMs = config.ledSmoothStepMs;

  Status st = mapBackendStatus(_strip->begin(ledCfg));
  if (!st.ok()) {
    delete _strip;
    _strip = nullptr;
    return st;
  }

  st = applyWifiState();
  if (!st.ok()) {
    return st;
  }
  st = applyHealthState();
  if (!st.ok()) {
    return st;
  }
  _dirty = false;
#else
  (void)config;
#endif
  return Ok();
}

void StatusLedAdapter::end() {
#ifdef ARDUINO
  if (_strip) {
    _strip->clear();
    _strip->end();
    delete _strip;
    _strip = nullptr;
  }
#endif
  _nextTickDueMs = 0U;
  _tickKick = false;
}

void StatusLedAdapter::setWifiState(WifiState state) {
  if (_wifiState != state) {
    _wifiState = state;
    _dirty = true;
    _tickKick = true;
  }
}

void StatusLedAdapter::setHealthState(HealthState state) {
  if (_healthState != state) {
    _healthState = state;
    _dirty = true;
    _tickKick = true;
  }
}

void StatusLedAdapter::tick(uint32_t nowMs) {
#ifdef ARDUINO
  if (!_strip) {
    return;
  }

  const bool cadenceDue = (_nextTickDueMs == 0U) ||
                          (static_cast<int32_t>(nowMs - _nextTickDueMs) >= 0);
  if (!_tickKick && !_dirty && !cadenceDue) {
    return;
  }

  if (_dirty) {
    Status st = applyWifiState();
    if (!st.ok()) {
      return;
    }
    st = applyHealthState();
    if (!st.ok()) {
      return;
    }
    _dirty = false;
  }
  _strip->tick(nowMs);
  _tickKick = false;
  _nextTickDueMs = nowMs + _tickCadenceMs;
#else
  (void)nowMs;
#endif
}

void StatusLedAdapter::flashSuccess(uint32_t durationMs) {
#ifdef ARDUINO
  if (!_strip || _count == 0) {
    return;
  }
  for (uint8_t i = 0; i < _count; ++i) {
    _strip->setTemporaryPreset(i, ::StatusLed::StatusPreset::Success, durationMs);
  }
  _tickKick = true;
#else
  (void)durationMs;
#endif
}

void StatusLedAdapter::forceRefresh() {
#ifdef ARDUINO
  if (_strip) {
    _strip->forceRefresh();
    _tickKick = true;
    _nextTickDueMs = 0U;
  }
#endif
}

#ifdef ARDUINO
Status StatusLedAdapter::mapBackendStatus(const ::StatusLed::Status& status) {
  switch (status.code) {
    case ::StatusLed::Err::OK:
      return Ok();
    case ::StatusLed::Err::INVALID_CONFIG:
      return Status(Err::INVALID_CONFIG, status.detail, "StatusLED invalid config");
    case ::StatusLed::Err::TIMEOUT:
      return Status(Err::TIMEOUT, status.detail, "StatusLED timeout");
    case ::StatusLed::Err::RESOURCE_BUSY:
      return Status(Err::RESOURCE_BUSY, status.detail, "StatusLED busy");
    case ::StatusLed::Err::NOT_INITIALIZED:
      return Status(Err::NOT_INITIALIZED, status.detail, "StatusLED not initialized");
    case ::StatusLed::Err::OUT_OF_MEMORY:
      return Status(Err::OUT_OF_MEMORY, status.detail, "StatusLED out of memory");
    case ::StatusLed::Err::HARDWARE_FAULT:
      return Status(Err::HARDWARE_FAULT, status.detail, "StatusLED hardware fault");
    case ::StatusLed::Err::EXTERNAL_LIB_ERROR:
      return Status(Err::EXTERNAL_LIB_ERROR, status.detail, "StatusLED external error");
    case ::StatusLed::Err::UNSUPPORTED:
      return Status(Err::INVALID_CONFIG, status.detail, "StatusLED unsupported");
    case ::StatusLed::Err::INTERNAL_ERROR:
    default:
      return Status(Err::INTERNAL_ERROR, status.detail, "StatusLED internal error");
  }
}

Status StatusLedAdapter::applyWifiState() {
  if (_strip == nullptr) {
    return Status(Err::NOT_INITIALIZED, 0, "StatusLED not initialized");
  }
  if (_wifiIndex >= _count) {
    return Ok();
  }
  const uint8_t index = _wifiIndex;
  ::StatusLed::Status st = ::StatusLed::Ok();

  if (_wifiState == WifiState::AP_OFF) {
    st = _strip->setColor(index, ::StatusLed::RgbColor(50, 0, 0));
    if (!st.ok()) {
      return mapBackendStatus(st);
    }
    return mapBackendStatus(_strip->setMode(index, ::StatusLed::Mode::Solid));
  }
  if (_wifiState == WifiState::WAITING) {
    return mapBackendStatus(_strip->setPreset(index, ::StatusLed::StatusPreset::Connecting));
  }
  if (_wifiState == WifiState::CONNECTED) {
    st = _strip->setColor(index, ::StatusLed::RgbColor(0, 0, 50));
    if (!st.ok()) {
      return mapBackendStatus(st);
    }
    return mapBackendStatus(_strip->setMode(index, ::StatusLed::Mode::Solid));
  }

  st = _strip->setColor(index, ::StatusLed::RgbColor(0, 50, 0));
  if (!st.ok()) {
    return mapBackendStatus(st);
  }
  return mapBackendStatus(_strip->setMode(index, ::StatusLed::Mode::Solid));
}

Status StatusLedAdapter::applyHealthState() {
  if (_strip == nullptr) {
    return Status(Err::NOT_INITIALIZED, 0, "StatusLED not initialized");
  }
  if (_healthIndex >= _count) {
    return Ok();
  }
  const uint8_t index = _healthIndex;
  ::StatusLed::Status st = ::StatusLed::Ok();

  if (_healthState == HealthState::INIT) {
    st = _strip->setColor(index, ::StatusLed::RgbColor(0, 50, 0));
    if (!st.ok()) {
      return mapBackendStatus(st);
    }
    return mapBackendStatus(_strip->setMode(index, ::StatusLed::Mode::BlinkSlow));
  }
  if (_healthState == HealthState::OK) {
    st = _strip->setColor(index, ::StatusLed::RgbColor(0, 50, 0));
    if (!st.ok()) {
      return mapBackendStatus(st);
    }
    return mapBackendStatus(_strip->setMode(index, ::StatusLed::Mode::Solid));
  }
  if (_healthState == HealthState::DEGRADED) {
    st = _strip->setColor(index, ::StatusLed::RgbColor(40, 20, 0));
    if (!st.ok()) {
      return mapBackendStatus(st);
    }
    return mapBackendStatus(_strip->setMode(index, ::StatusLed::Mode::Solid));
  }

  st = _strip->setColor(index, ::StatusLed::RgbColor(50, 0, 0));
  if (!st.ok()) {
    return mapBackendStatus(st);
  }
  st = _strip->setSecondaryColor(index, ::StatusLed::RgbColor(0, 0, 50));
  if (!st.ok()) {
    return mapBackendStatus(st);
  }
  return mapBackendStatus(_strip->setMode(index, ::StatusLed::Mode::Alternate));
}
#endif

}  // namespace CO2Control

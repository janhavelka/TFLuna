#include "devices/RtcAdapter.h"

#include "i2c/I2cOrchestrator.h"
#include "core/TimeUtil.h"

namespace CO2Control {

Status RtcAdapter::begin(const HardwareSettings& config, I2cOrchestrator* orchestrator) {
  (void)config;
  _orchestrator = orchestrator;
  _hasTime = false;
  _baseUnix = 0;
  _baseMillis = 0;
  _consecutiveFailures = 0;
  return Ok();
}

Status RtcAdapter::setTime(const RtcTime& time, uint64_t nowMs) {
  if (!isValidDateTime(time)) {
    return Status(Err::INVALID_CONFIG, 0, "RTC time invalid");
  }
  _baseUnix = toUnixSeconds(time);
  _baseMillis = nowMs;
  _hasTime = true;

  if (_orchestrator == nullptr) {
    return Status(Err::NOT_INITIALIZED, 0, "RTC I2C orchestrator missing");
  }

  Status hw = _orchestrator->queueRtcSet(time, nowMs);
  if (!hw.ok()) {
    _consecutiveFailures++;
    return hw;
  }
  return Ok();
}

Status RtcAdapter::getTime(uint64_t nowMs, RtcTime& out) const {
  Status hwStatus = Status(Err::NOT_INITIALIZED, 0, "RTC I2C orchestrator missing");
  bool hwAttempted = false;
  if (_orchestrator != nullptr) {
    hwStatus = _orchestrator->getRtcTime(nowMs, out);
    hwAttempted = true;
    if (hwStatus.ok()) {
      _consecutiveFailures = _orchestrator->rtcConsecutiveFailures();
      return Ok();
    }
  }

  if (!_hasTime) {
    out.valid = false;
    _consecutiveFailures++;
    if (hwAttempted) {
      return hwStatus;
    }
    return Status(Err::NOT_INITIALIZED, 0, "RTC time not set");
  }
  const uint64_t elapsed = (nowMs - _baseMillis) / 1000ULL;
  const uint32_t unixSeconds = _baseUnix + static_cast<uint32_t>(elapsed);
  fromUnixSeconds(unixSeconds, out);
  _consecutiveFailures++;
  return Status(Err::COMM_FAILURE, 0, "RTC fallback active");
}

}  // namespace CO2Control

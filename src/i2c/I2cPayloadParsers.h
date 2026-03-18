#pragma once

#include <stdint.h>

#include "CO2Control/Status.h"
#include "CO2Control/Types.h"

namespace CO2Control {

struct EnvDecodedSample {
  float tempC = 0.0f;
  float rhPct = 0.0f;
  float pressureHpa = 0.0f;
};

uint8_t rtcDecToBcd(uint8_t value);
bool isEnvBme280Address(uint8_t address);
Status parseRtcTimePayload(const uint8_t* data, uint8_t dataLen, RtcTime& out);
Status parseEnvSamplePayload(uint8_t address,
                             const uint8_t* data,
                             uint8_t dataLen,
                             EnvDecodedSample& out);

}  // namespace CO2Control

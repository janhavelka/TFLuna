#include "i2c/I2cPayloadParsers.h"

#include "core/TimeUtil.h"

namespace TFLunaControl {

static uint8_t bcdToDec(uint8_t value) {
  return static_cast<uint8_t>(((value >> 4U) * 10U) + (value & 0x0FU));
}

uint8_t rtcDecToBcd(uint8_t value) {
  return static_cast<uint8_t>(((value / 10U) << 4U) | (value % 10U));
}

bool isEnvBme280Address(uint8_t address) {
  return address == 0x76U || address == 0x77U;
}

Status parseRtcTimePayload(const uint8_t* data, uint8_t dataLen, RtcTime& out) {
  if (data == nullptr || dataLen < 7) {
    return Status(Err::DATA_CORRUPT, 0, "RTC read length invalid");
  }

  RtcTime parsed{};
  parsed.second = bcdToDec(static_cast<uint8_t>(data[0] & 0x7F));
  parsed.minute = bcdToDec(static_cast<uint8_t>(data[1] & 0x7F));
  parsed.hour = bcdToDec(static_cast<uint8_t>(data[2] & 0x3F));
  parsed.day = bcdToDec(static_cast<uint8_t>(data[4] & 0x3F));
  parsed.month = bcdToDec(static_cast<uint8_t>(data[5] & 0x1F));
  parsed.year = static_cast<uint16_t>(2000U + bcdToDec(data[6]));
  parsed.valid = true;

  if (!isValidDateTime(parsed)) {
    return Status(Err::DATA_CORRUPT, 0, "RTC payload invalid");
  }

  out = parsed;
  return Ok();
}

Status parseEnvSamplePayload(uint8_t address,
                             const uint8_t* data,
                             uint8_t dataLen,
                             EnvDecodedSample& out) {
  if (data == nullptr || dataLen < 6) {
    return Status(Err::DATA_CORRUPT, 0, "ENV payload too short");
  }

  EnvDecodedSample parsed{};
  if (isEnvBme280Address(address)) {
    const int16_t tempX100 = static_cast<int16_t>((data[0] << 8) | data[1]);
    const uint16_t rhX100 = static_cast<uint16_t>((data[2] << 8) | data[3]);
    const uint16_t pressureX10 = static_cast<uint16_t>((data[4] << 8) | data[5]);
    parsed.tempC = static_cast<float>(tempX100) / 100.0f;
    parsed.rhPct = static_cast<float>(rhX100) / 100.0f;
    parsed.pressureHpa = static_cast<float>(pressureX10) / 10.0f;
  } else {
    const uint16_t tempRaw = static_cast<uint16_t>((data[0] << 8) | data[1]);
    const uint16_t rhRaw = static_cast<uint16_t>((data[3] << 8) | data[4]);
    parsed.tempC = -45.0f + (175.0f * static_cast<float>(tempRaw) / 65535.0f);
    parsed.rhPct = 100.0f * static_cast<float>(rhRaw) / 65535.0f;
    parsed.pressureHpa = 0.0f;
  }

  out = parsed;
  return Ok();
}

}  // namespace TFLunaControl

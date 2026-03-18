#include "core/TimeUtil.h"

#include <stdio.h>

namespace CO2Control {

static bool isLeapYear(uint16_t year) {
  return ((year % 4) == 0) && (((year % 100) != 0) || ((year % 400) == 0));
}

bool isValidDateTime(const RtcTime& t) {
  if (!t.valid) {
    return false;
  }
  if (t.year < 1970 || t.year > 2099) {
    return false;
  }
  if (t.month < 1 || t.month > 12) {
    return false;
  }
  if (t.day < 1 || t.day > 31) {
    return false;
  }
  if (t.hour > 23 || t.minute > 59 || t.second > 59) {
    return false;
  }
  return true;
}

uint32_t toUnixSeconds(const RtcTime& t) {
  if (!isValidDateTime(t)) {
    return 0;
  }

  static const uint16_t daysBeforeMonth[12] = {
      0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

  uint32_t days = 0;
  for (uint16_t y = 1970; y < t.year; ++y) {
    days += isLeapYear(y) ? 366 : 365;
  }

  days += daysBeforeMonth[t.month - 1];
  if (isLeapYear(t.year) && t.month > 2) {
    days += 1;
  }
  days += static_cast<uint32_t>(t.day - 1);

  uint32_t seconds = days * 86400UL;
  seconds += static_cast<uint32_t>(t.hour) * 3600UL;
  seconds += static_cast<uint32_t>(t.minute) * 60UL;
  seconds += static_cast<uint32_t>(t.second);
  return seconds;
}

void formatLocalTime(const RtcTime& t, char* out, size_t len) {
  if (!out || len == 0) {
    return;
  }
  if (!isValidDateTime(t)) {
    out[0] = '\0';
    return;
  }
  // "YYYY-MM-DD HH:MM:SS" = 19 chars + null
  snprintf(out, len, "%04u-%02u-%02u %02u:%02u:%02u",
           static_cast<unsigned int>(t.year),
           static_cast<unsigned int>(t.month),
           static_cast<unsigned int>(t.day),
           static_cast<unsigned int>(t.hour),
           static_cast<unsigned int>(t.minute),
           static_cast<unsigned int>(t.second));
}

void fromUnixSeconds(uint32_t unixSeconds, RtcTime& out) {
  uint32_t seconds = unixSeconds;
  uint32_t days = seconds / 86400UL;
  seconds %= 86400UL;

  out.hour = static_cast<uint8_t>(seconds / 3600UL);
  seconds %= 3600UL;
  out.minute = static_cast<uint8_t>(seconds / 60UL);
  out.second = static_cast<uint8_t>(seconds % 60UL);

  uint16_t year = 1970;
  while (year < 0xFFFFU) {
    const uint16_t daysInYear = isLeapYear(year) ? 366 : 365;
    if (days < daysInYear) {
      break;
    }
    days -= daysInYear;
    year++;
  }

  static const uint8_t daysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  uint8_t month = 0;
  for (month = 0; month < 12; ++month) {
    uint8_t dim = daysInMonth[month];
    if (month == 1 && isLeapYear(year)) {
      dim = 29;
    }
    if (days < dim) {
      break;
    }
    days -= dim;
  }

  out.year = year;
  out.month = static_cast<uint8_t>(month + 1);
  out.day = static_cast<uint8_t>(days + 1);
  out.valid = (year >= 2000 && year <= 2099);
}

}  // namespace CO2Control

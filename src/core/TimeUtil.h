#pragma once

#include <stddef.h>
#include <stdint.h>

#include "TFLunaControl/Types.h"

namespace TFLunaControl {

/// @brief Stateless time conversion helpers used across modules.
/// @note Kept in `core/` because these helpers are generic and do not own runtime settings/state.

/// @brief Check if RTC time is valid.
bool isValidDateTime(const RtcTime& t);

/// @brief Convert RTC time to Unix seconds. Returns 0 if invalid.
uint32_t toUnixSeconds(const RtcTime& t);

/// @brief Format RTC time into "YYYY-MM-DD HH:MM:SS".
/// @param t RTC time
/// @param out Buffer
/// @param len Buffer length (>= 20 recommended)
void formatLocalTime(const RtcTime& t, char* out, size_t len);

/// @brief Convert Unix seconds to RTC time.
/// @param unixSeconds Unix timestamp (seconds)
/// @param out RTC time output
void fromUnixSeconds(uint32_t unixSeconds, RtcTime& out);

}  // namespace TFLunaControl

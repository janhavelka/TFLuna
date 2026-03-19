#pragma once

#include <stddef.h>
#include <stdint.h>

namespace TFLunaControl {

/// @brief Decide if command queue health should be degraded.
/// @note Degraded only when overflow is recent or queue depth remains high.
inline bool isCommandQueueDegraded(uint32_t nowMs,
                                   uint32_t lastOverflowMs,
                                   size_t depth,
                                   uint32_t overflowRecentWindowMs,
                                   size_t depthThreshold) {
  const bool depthHigh = depth >= depthThreshold;
  if (depthHigh) {
    return true;
  }
  if (lastOverflowMs == 0) {
    return false;
  }
  const int32_t delta = static_cast<int32_t>(nowMs - lastOverflowMs);
  return (delta >= 0) && (static_cast<uint32_t>(delta) <= overflowRecentWindowMs);
}

}  // namespace TFLunaControl

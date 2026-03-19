#pragma once

#include "TFLunaControl/RuntimeSettings.h"
#include "TFLunaControl/Status.h"

namespace TFLunaControl {

/**
 * @brief NVS persistence for RuntimeSettings.
 */
class SettingsStore {
 public:
  /// @brief Initialize NVS storage.
  Status begin(bool enable);

  /// @brief Load settings from NVS.
  Status load(RuntimeSettings& settings);

  /// @brief Save settings to NVS.
  Status save(const RuntimeSettings& settings);

  /// @brief Reset settings to defaults and persist.
  Status factoryReset(RuntimeSettings& settings);

  /// @brief Check if NVS is enabled.
  bool isEnabled() const { return _enabled; }

 private:
  bool _enabled = false;
};

}  // namespace TFLunaControl

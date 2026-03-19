#pragma once

#include "TFLunaControl/Types.h"

namespace TFLunaControl {

class LidarStats {
 public:
  void reset();
  void recordMeasurement(const LidarMeasurement& measurement);
  LidarStatsSnapshot snapshot() const;

 private:
  void updateDistance(double value);
  void updateStrength(double value);

  uint64_t _totalFrames = 0;
  uint64_t _validSamples = 0;
  uint64_t _invalidSamples = 0;
  uint64_t _weakSamples = 0;
  bool _hasDistanceStats = false;
  double _minDistance = 0.0;
  double _maxDistance = 0.0;
  double _distanceMean = 0.0;
  double _distanceM2 = 0.0;
  double _strengthMean = 0.0;
  double _strengthM2 = 0.0;
};

}  // namespace TFLunaControl

#include "core/LidarStats.h"

#include <math.h>

namespace TFLunaControl {

void LidarStats::reset() {
  _totalFrames = 0U;
  _validSamples = 0U;
  _invalidSamples = 0U;
  _weakSamples = 0U;
  _hasDistanceStats = false;
  _minDistance = 0.0;
  _maxDistance = 0.0;
  _distanceMean = 0.0;
  _distanceM2 = 0.0;
  _strengthMean = 0.0;
  _strengthM2 = 0.0;
}

void LidarStats::recordMeasurement(const LidarMeasurement& measurement) {
  if (!measurement.validFrame) {
    return;
  }

  ++_totalFrames;
  if (!measurement.signalOk) {
    ++_invalidSamples;
    ++_weakSamples;
    return;
  }

  ++_validSamples;
  updateDistance(static_cast<double>(measurement.distanceCm));
  updateStrength(static_cast<double>(measurement.strength));
}

LidarStatsSnapshot LidarStats::snapshot() const {
  LidarStatsSnapshot out{};
  out.totalFrames = _totalFrames;
  out.validSamples = _validSamples;
  out.invalidSamples = _invalidSamples;
  out.weakSamples = _weakSamples;
  out.hasDistanceStats = _hasDistanceStats;

  if (_hasDistanceStats) {
    out.minDistanceCm = static_cast<float>(_minDistance);
    out.maxDistanceCm = static_cast<float>(_maxDistance);
    out.meanDistanceCm = static_cast<float>(_distanceMean);
    out.rangeDistanceCm = static_cast<float>(_maxDistance - _minDistance);
    out.meanStrength = static_cast<float>(_strengthMean);
    if (_validSamples > 1U) {
      out.stddevDistanceCm =
          static_cast<float>(sqrt(_distanceM2 / static_cast<double>(_validSamples - 1U)));
      out.stddevStrength =
          static_cast<float>(sqrt(_strengthM2 / static_cast<double>(_validSamples - 1U)));
    }
  }

  return out;
}

void LidarStats::updateDistance(double value) {
  if (!_hasDistanceStats) {
    _hasDistanceStats = true;
    _minDistance = value;
    _maxDistance = value;
    _distanceMean = value;
    _distanceM2 = 0.0;
    return;
  }

  if (value < _minDistance) {
    _minDistance = value;
  }
  if (value > _maxDistance) {
    _maxDistance = value;
  }

  const double delta = value - _distanceMean;
  _distanceMean += delta / static_cast<double>(_validSamples);
  const double delta2 = value - _distanceMean;
  _distanceM2 += delta * delta2;
}

void LidarStats::updateStrength(double value) {
  if (_validSamples == 1U) {
    _strengthMean = value;
    _strengthM2 = 0.0;
    return;
  }

  const double delta = value - _strengthMean;
  _strengthMean += delta / static_cast<double>(_validSamples);
  const double delta2 = value - _strengthMean;
  _strengthM2 += delta * delta2;
}

}  // namespace TFLunaControl

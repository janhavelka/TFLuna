/**
 * @file Status.h
 * @brief Error handling types for TFLunaControl.
 *
 * Provides a lightweight, zero-allocation error model. All error messages
 * are static string literals.
 */

#pragma once

#include <stdint.h>

namespace TFLunaControl {

/**
 * @brief Error code enumeration.
 */
enum class Err : uint16_t {
  OK = 0,              ///< Success, no error
  INVALID_CONFIG,      ///< Invalid argument or configuration parameter
  TIMEOUT,             ///< Operation timed out waiting for response
  BUS_STUCK,           ///< Physical bus line stuck (SDA/SCL held low)
  RESOURCE_BUSY,       ///< Resource is busy, cannot acquire lock
  COMM_FAILURE,        ///< Communication or I/O operation failed
  NOT_INITIALIZED,     ///< Not initialized or begin() not called
  OUT_OF_MEMORY,       ///< Memory allocation failed
  HARDWARE_FAULT,      ///< Hardware peripheral returned error
  EXTERNAL_LIB_ERROR,  ///< Error from external library (see detail field)
  DATA_CORRUPT,        ///< Persistent data is invalid or corrupted
  INTERNAL_ERROR       ///< Internal logic error (bug in library code)
};

/**
 * @brief Operation result with error details.
 *
 * Returned by fallible operations. Check with ok() or inspect code/msg.
 *
 * @note The msg field MUST point to a static string literal. Never assign
 *       dynamically allocated strings. This ensures zero heap allocation
 *       in error paths and safe usage across function boundaries.
 */
struct Status {
  Err code = Err::OK;       ///< Error category
  int32_t detail = 0;       ///< Vendor/library-specific error code (optional)
  const char* msg = "";     ///< Human-readable message (STATIC STRING ONLY)

  /// @brief Default constructor - creates OK status.
  constexpr Status() : code(Err::OK), detail(0), msg("") {}

  /// @brief Constructor with all fields.
  constexpr Status(Err c, int32_t d, const char* m) : code(c), detail(d), msg(m) {}

  /// @brief Check if operation succeeded.
  /// @return true if code == Err::OK
  constexpr bool ok() const { return code == Err::OK; }
};

/// @brief Create a success Status.
/// @return Status with Err::OK
constexpr Status Ok() { return Status(Err::OK, 0, ""); }

}  // namespace TFLunaControl

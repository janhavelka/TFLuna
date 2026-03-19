#pragma once

#include <stddef.h>

namespace TFLunaControl {

/**
 * @brief Fixed-size ring buffer.
 */
template <typename T, size_t N>
class RingBuffer {
 public:
  static_assert(N > 0U, "RingBuffer capacity must be > 0");

  /// @brief Clear all entries.
  void clear() {
    _head = 0;
    _count = 0;
  }

  /// @brief Get number of stored elements.
  size_t size() const { return _count; }

  /// @brief Get capacity.
  constexpr size_t capacity() const { return N; }

  /// @brief Push a new element, overwriting oldest if full.
  void push(const T& value) {
    _buffer[_head] = value;
    _head = (_head + 1) % N;
    if (_count < N) {
      _count++;
    }
  }

  /// @brief Get latest element, or nullptr if empty.
  const T* latest() const {
    if (_count == 0) {
      return nullptr;
    }
    size_t idx = (_head + N - 1) % N;
    return &_buffer[idx];
  }

  /// @brief Copy elements into caller buffer.
  /// @param out Destination array
  /// @param max Maximum number of elements to copy
  /// @param oldestFirst If true, copy oldest to newest
  /// @return Number of elements copied
  size_t copy(T* out, size_t max, bool oldestFirst) const {
    if (!out || max == 0 || _count == 0) {
      return 0;
    }
    const size_t n = (_count < max) ? _count : max;
    if (oldestFirst) {
      // Return the newest window limited by `max`, ordered oldest->newest.
      // This keeps fixed-size graph views sliding as new samples arrive.
      size_t start = (_head + N - n) % N;
      for (size_t i = 0; i < n; ++i) {
        out[i] = _buffer[(start + i) % N];
      }
    } else {
      for (size_t i = 0; i < n; ++i) {
        size_t idx = (_head + N - 1 - i) % N;
        out[i] = _buffer[idx];
      }
    }
    return n;
  }

 private:
  T _buffer[N] = {};
  size_t _head = 0;
  size_t _count = 0;
};

}  // namespace TFLunaControl

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace TFLunaControl {

/**
 * @brief Fixed-size FIFO command queue with overflow accounting.
 */
template <typename T, size_t N>
class CommandQueue {
 public:
  static_assert(N > 0U, "CommandQueue capacity must be > 0");

  void clear() {
    _head = 0;
    _tail = 0;
    _count = 0;
    _overflowCount = 0;
    _lastOverflowMs = 0;
  }

  bool push(const T& value, uint32_t nowMs) {
    if (_count >= N) {
      _overflowCount++;
      _lastOverflowMs = nowMs;
      return false;
    }

    _buffer[_head] = value;
    _head = (_head + 1) % N;
    _count++;
    return true;
  }

  bool pop(T& out) {
    if (_count == 0) {
      return false;
    }

    out = _buffer[_tail];
    _tail = (_tail + 1) % N;
    _count--;
    return true;
  }

  size_t depth() const { return _count; }
  constexpr size_t capacity() const { return N; }
  uint32_t overflowCount() const { return _overflowCount; }
  uint32_t lastOverflowMs() const { return _lastOverflowMs; }

 private:
  T _buffer[N] = {};
  size_t _head = 0;
  size_t _tail = 0;
  size_t _count = 0;
  uint32_t _overflowCount = 0;
  uint32_t _lastOverflowMs = 0;
};

}  // namespace TFLunaControl

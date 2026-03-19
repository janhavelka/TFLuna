#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <type_traits>

#include "TFLunaControl/Status.h"
#include "core/PsramSupport.h"

namespace TFLunaControl {

/**
 * @brief Runtime-capacity ring buffer with optional PSRAM backing.
 *
 * Buffer storage is allocated once during begin() and never resized.
 */
template <typename T>
class DynamicRingBuffer {
 public:
  static_assert(std::is_trivially_copyable<T>::value,
                "DynamicRingBuffer requires trivially-copyable element type");

  Status begin(size_t capacity, bool preferPsram) {
    end();
    if (capacity == 0U) {
      return Status(Err::INVALID_CONFIG, 0, "ring capacity zero");
    }
    if (capacity > (SIZE_MAX / sizeof(T))) {
      return Status(Err::INVALID_CONFIG, 0, "ring size overflow");
    }

    const size_t bytes = capacity * sizeof(T);
    void* mem = nullptr;
    bool usingPsram = preferPsram;
    if (preferPsram) {
      mem = PsramSupport::allocPsram(bytes);
    } else {
      mem = PsramSupport::allocInternal(bytes);
    }
    if (mem == nullptr) {
      return Status(Err::OUT_OF_MEMORY, 0, "ring alloc failed");
    }

    memset(mem, 0, bytes);
    _buffer = static_cast<T*>(mem);
    _capacity = capacity;
    _usingPsram = usingPsram;
    _head = 0U;
    _count = 0U;
    return Ok();
  }

  void end() {
    if (_buffer != nullptr) {
      PsramSupport::freeMemory(_buffer);
      _buffer = nullptr;
    }
    _capacity = 0U;
    _usingPsram = false;
    _head = 0U;
    _count = 0U;
  }

  void clear() {
    _head = 0U;
    _count = 0U;
  }

  bool ready() const {
    return _buffer != nullptr && _capacity > 0U;
  }

  size_t size() const { return _count; }

  size_t capacity() const { return _capacity; }

  bool usingPsram() const { return _usingPsram; }

  void push(const T& value) {
    if (!ready()) {
      return;
    }
    _buffer[_head] = value;
    _head = (_head + 1U) % _capacity;
    if (_count < _capacity) {
      _count++;
    }
  }

  const T* latest() const {
    if (!ready() || _count == 0U) {
      return nullptr;
    }
    const size_t idx = (_head + _capacity - 1U) % _capacity;
    return &_buffer[idx];
  }

  size_t copy(T* out, size_t max, bool oldestFirst) const {
    if (!ready() || out == nullptr || max == 0U || _count == 0U) {
      return 0U;
    }
    const size_t n = (_count < max) ? _count : max;
    if (oldestFirst) {
      const size_t start = (_head + _capacity - n) % _capacity;
      for (size_t i = 0U; i < n; ++i) {
        out[i] = _buffer[(start + i) % _capacity];
      }
    } else {
      for (size_t i = 0U; i < n; ++i) {
        const size_t idx = (_head + _capacity - 1U - i) % _capacity;
        out[i] = _buffer[idx];
      }
    }
    return n;
  }

 private:
  T* _buffer = nullptr;
  size_t _capacity = 0U;
  size_t _head = 0U;
  size_t _count = 0U;
  bool _usingPsram = false;
};

}  // namespace TFLunaControl

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef ARDUINO
#include <Arduino.h>
#if __has_include(<esp_heap_caps.h>)
#include <esp_heap_caps.h>
#define TFLUNACTRL_HAS_HEAP_CAPS 1
#else
#define TFLUNACTRL_HAS_HEAP_CAPS 0
#endif
#else
#define TFLUNACTRL_HAS_HEAP_CAPS 0
#endif

namespace TFLunaControl {
namespace PsramSupport {

inline bool isAvailable() {
#ifdef ARDUINO
  return ESP.getPsramSize() > 0U;
#else
  return false;
#endif
}

inline uint32_t totalBytes() {
#ifdef ARDUINO
  return static_cast<uint32_t>(ESP.getPsramSize());
#else
  return 0U;
#endif
}

inline uint32_t freeBytes() {
#ifdef ARDUINO
  return static_cast<uint32_t>(ESP.getFreePsram());
#else
  return 0U;
#endif
}

inline uint32_t minFreeBytes() {
#ifdef ARDUINO
  return static_cast<uint32_t>(ESP.getMinFreePsram());
#else
  return 0U;
#endif
}

inline uint32_t maxAllocBytes() {
#ifdef ARDUINO
  return static_cast<uint32_t>(ESP.getMaxAllocPsram());
#else
  return 0U;
#endif
}

inline void* allocPsram(size_t bytes) {
  if (bytes == 0U) {
    return nullptr;
  }
#if TFLUNACTRL_HAS_HEAP_CAPS
  return heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
  return nullptr;
#endif
}

inline void* allocInternal(size_t bytes) {
  if (bytes == 0U) {
    return nullptr;
  }
#if TFLUNACTRL_HAS_HEAP_CAPS
  return heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#else
  return malloc(bytes);
#endif
}

inline void freeMemory(void* ptr) {
  if (ptr == nullptr) {
    return;
  }
#if TFLUNACTRL_HAS_HEAP_CAPS
  heap_caps_free(ptr);
#else
  free(ptr);
#endif
}

}  // namespace PsramSupport
}  // namespace TFLunaControl

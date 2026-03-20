#include "logging/SdLogger.h"

#include <stdio.h>
#include <string.h>

#include "core/PsramSupport.h"
#include "core/SystemClock.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace TFLunaControl {

static constexpr char LOG_DIR[] = "/logs";
static constexpr char DAILY_DIR[] = "/logs/daily";

/// Mount retry interval when card-detect GPIO reports card present.
static constexpr uint32_t SD_FAST_MOUNT_RETRY_MS = 1000UL;
static constexpr char RUNS_DIR[] = "/logs/runs";
static constexpr char UNKNOWN_DAY[] = "unknown";
static constexpr char SESSION_DATA_PREFIX[] = "data_";
static constexpr char SESSION_DATA_PREFIX_COMPAT[] = "data_v2_";
static constexpr char CSV_HEADER[] =
    "timestamp,uptime_ms,sample_index,distance_cm,strength,temperature_c,valid_frame,signal_ok,env_temp_c,env_rh_pct,env_pressure_hpa\n";
static constexpr char EVENTS_HEADER[] = "ts_unix,ts_local,event_code,event_msg\n";
// --- FAT32-safe info timing ---
// freeClusterCount() scans the entire FAT table and can block the
// AsyncSD worker for 10-30+ seconds on 16-32 GB FAT32 cards.
// The constants below must be generous enough that a late result
// is never orphaned by SdLogger's own timeout.
static constexpr uint32_t SD_INFO_REFRESH_MS = 15000UL;         // 15 s between successful refreshes
static constexpr uint32_t SD_INFO_RETRY_BASE_MS = 30000UL;      // base retry after failure
static constexpr uint32_t SD_INFO_REQUEST_TIMEOUT_MS = 120000UL; // 2 min; must outlast worst-case FAT32 scan
static constexpr uint8_t SD_INFO_TIMEOUT_STREAK_LIMIT = 5U;
static constexpr uint32_t SD_FILE_JOB_TIMEOUT_MS = 10000UL;  // 10 s per file op
static constexpr size_t SD_MAX_RESULTS_PER_TICK = 24U;
static constexpr uint16_t LOG_FILE_PART_MAX = 9999U;
static constexpr uint16_t SESSION_PROBE_MAX = 65535U;
static constexpr uint8_t SESSION_STAGE_IDLE = 0U;
static constexpr uint8_t SESSION_STAGE_WAIT_RUNS_DIR = 1U;
static constexpr uint8_t SESSION_STAGE_WAIT_SESSION_DIR = 2U;

static constexpr uint8_t SESSION_STAGE_WAIT_RESUME_LIST = 3U;
static constexpr uint8_t SESSION_STAGE_WAIT_RESUME_STAT_DATA = 4U;
static constexpr uint8_t SESSION_STAGE_WAIT_RESUME_STAT_EVENTS = 5U;
static constexpr uint8_t SESSION_STAGE_WAIT_FRESH_VERIFY_LIST = 6U;
static constexpr uint8_t SESSION_STAGE_WAIT_MARKER_CREATE_OPEN = 7U;
static constexpr uint8_t SESSION_STAGE_WAIT_MARKER_CREATE_CLOSE = 8U;
static constexpr size_t SD_SAMPLE_QUEUE_PSRAM_TIER0 = HardwareSettings::SD_LOG_QUEUE_CAPACITY_PSRAM;
static constexpr size_t SD_SAMPLE_QUEUE_PSRAM_TIER1 = HardwareSettings::SD_LOG_QUEUE_CAPACITY_PSRAM / 2U;
static constexpr size_t SD_EVENT_QUEUE_PSRAM_TIER0 = HardwareSettings::SD_EVENT_QUEUE_CAPACITY_PSRAM;
static constexpr size_t SD_EVENT_QUEUE_PSRAM_TIER1 = HardwareSettings::SD_EVENT_QUEUE_CAPACITY_PSRAM / 2U;
static constexpr char SESSION_MARKER_PREFIX[] = "session_";
static constexpr char SESSION_MARKER_SUFFIX[] = ".tag";

static uint64_t makeSessionMarkerToken(uint16_t seq, uint32_t nowMs) {
  static uint32_t s_markerSalt = 0x4D595DF4U;
  s_markerSalt = (s_markerSalt * 1664525U) + 1013904223U + nowMs + static_cast<uint32_t>(seq);
  uint64_t token = (static_cast<uint64_t>(nowMs == 0U ? 1U : nowMs) << 32) ^
                   (static_cast<uint64_t>(s_markerSalt) << 1) ^
                   static_cast<uint64_t>(seq == 0U ? 1U : seq);
  if (token == 0U) {
    token = 1U;
  }
  return token;
}

static bool buildSessionMarkerName(uint64_t token, char* out, size_t outLen) {
  if (out == nullptr || outLen == 0U) {
    return false;
  }
  const uint32_t hi = static_cast<uint32_t>(token >> 32);
  const uint32_t lo = static_cast<uint32_t>(token & 0xFFFFFFFFULL);
  const int n = snprintf(out,
                         outLen,
                         "%s%08lX%08lX%s",
                         SESSION_MARKER_PREFIX,
                         static_cast<unsigned long>(hi),
                         static_cast<unsigned long>(lo),
                         SESSION_MARKER_SUFFIX);
  return n > 0 && static_cast<size_t>(n) < outLen;
}

static bool hasSuspiciousCsvLength(uint64_t size, size_t headerLen) {
  return size > 0U && size < headerLen;
}

static bool parsePartFromFileName(const char* name, const char* prefix, uint16_t& outPart) {
  if (name == nullptr || prefix == nullptr) {
    return false;
  }
  const size_t prefixLen = strlen(prefix);
  if (strncmp(name, prefix, prefixLen) != 0) {
    return false;
  }
  const char* digits = name + prefixLen;
  uint16_t part = 0;
  for (size_t i = 0; i < 4; ++i) {
    const char c = digits[i];
    if (c < '0' || c > '9') {
      return false;
    }
    part = static_cast<uint16_t>((part * 10U) + static_cast<uint16_t>(c - '0'));
  }
  if (strcmp(digits + 4, ".csv") != 0) {
    return false;
  }
  if (part == 0U || part > LOG_FILE_PART_MAX) {
    return false;
  }
  outPart = part;
  return true;
}

static bool buildDailySampleFilePath(const char* dayKey, char* outPath, size_t outLen) {
  if (dayKey == nullptr || outPath == nullptr || outLen == 0U) {
    return false;
  }
  const int n = snprintf(outPath, outLen, "%s/%s_v2.csv", DAILY_DIR, dayKey);
  return n > 0 && static_cast<size_t>(n) < outLen;
}

static bool isAsciiAlphaNum(char c) {
  return (c >= '0' && c <= '9') ||
         (c >= 'A' && c <= 'Z') ||
         (c >= 'a' && c <= 'z');
}

static void sanitizeSessionBaseName(const char* requested, char* out, size_t outLen) {
  if (out == nullptr || outLen == 0U) {
    return;
  }
  size_t w = 0U;
  if (requested != nullptr) {
    const size_t inLen = strnlen(requested, RuntimeSettings::LOG_SESSION_NAME_BYTES);
    for (size_t i = 0; i < inLen && (w + 1U) < outLen; ++i) {
      const char c = requested[i];
      if (isAsciiAlphaNum(c)) {
        out[w++] = c;
        continue;
      }
      if ((c == '_' || c == '-') && w > 0U) {
        out[w++] = c;
      }
    }
  }

  while (w > 0U && (out[w - 1U] == '_' || out[w - 1U] == '-')) {
    --w;
  }

  if (w == 0U) {
    strncpy(out, "run", outLen - 1U);
    out[outLen - 1U] = '\0';
    return;
  }
  out[w] = '\0';
}

bool SdLogger::shouldAttemptFlush(uint32_t nowMs, uint32_t lastFlushMs, uint32_t flushIntervalMs) {
  if (flushIntervalMs == 0 || lastFlushMs == 0) {
    return true;
  }
  const int32_t delta = static_cast<int32_t>(nowMs - lastFlushMs);
  return delta >= 0 && static_cast<uint32_t>(delta) >= flushIntervalMs;
}

void SdLogger::resetAllSessionState(bool clearNames) {
  _allSessionReady = false;
  _allSessionProbeCount = 0;
  _allSessionRetryAfterMs = 0;
  _allSessionStage = SESSION_STAGE_IDLE;
#if TFLUNACTRL_HAS_ASYNC_SD
  _sessionRequestId = AsyncSD::INVALID_REQUEST_ID;
  _sessionSetupHandle = AsyncSD::INVALID_FILE_HANDLE;
#endif
  _allFilePart = 1U;
  _eventFilePart = 1U;
  _sampleLinesCurrentFile = 0U;
  _eventLinesCurrentFile = 0U;
  _allCurrentFilePath[0] = '\0';
  _eventCurrentFilePath[0] = '\0';
  if (clearNames) {
    _allSessionDirName[0] = '\0';
    _allSessionDirPath[0] = '\0';
    _allSessionMarkerName[0] = '\0';
  }
  if (!buildSessionEventFilePath(_eventFilePart, _eventCurrentFilePath, sizeof(_eventCurrentFilePath))) {
    _eventCurrentFilePath[0] = '\0';
  }
}

bool SdLogger::buildAllSessionCandidate(uint16_t seq,
                                        char* outPath,
                                        size_t outLen,
                                        char* outName,
                                        size_t outNameLen) {
  if (outPath == nullptr || outLen == 0 || outName == nullptr || outNameLen == 0) {
    return false;
  }
  char sessionBase[RuntimeSettings::LOG_SESSION_NAME_BYTES] = {0};
  sanitizeSessionBaseName(_settings.logSessionName, sessionBase, sizeof(sessionBase));
  const int nName = snprintf(outName, outNameLen, "%s_%05u", sessionBase, static_cast<unsigned int>(seq));
  if (nName <= 0 || static_cast<size_t>(nName) >= outNameLen) {
    return false;
  }
  const int nPath = snprintf(outPath, outLen, "%s/%s", RUNS_DIR, outName);
  if (nPath <= 0 || static_cast<size_t>(nPath) >= outLen) {
    return false;
  }
  return true;
}

bool SdLogger::buildSessionAllFilePath(uint16_t part, char* outPath, size_t outLen) {
  if (outPath == nullptr || outLen == 0 || _allSessionDirPath[0] == '\0') {
    return false;
  }
  const int n = snprintf(outPath,
                         outLen,
                         "%s/%s%04u.csv",
                         _allSessionDirPath,
                         SESSION_DATA_PREFIX,
                         static_cast<unsigned int>(part));
  return n > 0 && static_cast<size_t>(n) < outLen;
}

bool SdLogger::buildSessionEventFilePath(uint16_t part, char* outPath, size_t outLen) {
  if (outPath == nullptr || outLen == 0) {
    return false;
  }
  const char* baseDir = (_allSessionDirPath[0] != '\0') ? _allSessionDirPath : LOG_DIR;
  const int n = snprintf(outPath, outLen, "%s/events_%04u.csv", baseDir, static_cast<unsigned int>(part));
  return n > 0 && static_cast<size_t>(n) < outLen;
}

bool SdLogger::buildSessionMarkerPath(char* outPath, size_t outLen) const {
  if (outPath == nullptr || outLen == 0U ||
      _allSessionDirPath[0] == '\0' || _allSessionMarkerName[0] == '\0') {
    return false;
  }
  const int n = snprintf(outPath, outLen, "%s/%s", _allSessionDirPath, _allSessionMarkerName);
  return n > 0 && static_cast<size_t>(n) < outLen;
}

bool SdLogger::ensureSessionMarkerName(uint32_t nowMs) {
  if (_allSessionMarkerName[0] != '\0') {
    return true;
  }
  const uint64_t token = makeSessionMarkerToken(_allSessionSeq, nowMs);
  return buildSessionMarkerName(token, _allSessionMarkerName, sizeof(_allSessionMarkerName));
}

bool SdLogger::issueOpenForCurrentJob(uint32_t nowMs) {
#if TFLUNACTRL_HAS_ASYNC_SD
  const AsyncSD::RequestId id =
      _sd.requestOpen(_job.path,
                      AsyncSD::OpenMode::Write | AsyncSD::OpenMode::Create | AsyncSD::OpenMode::Append);
  if (id == AsyncSD::INVALID_REQUEST_ID) {
    const AsyncSD::ErrorInfo info = _sd.lastErrorInfo();
    failFileJob(Status(Err::RESOURCE_BUSY, info.detail, "AsyncSD open enqueue failed"), nowMs);
    return false;
  }
  _job.requestId = id;
  _job.op = JobOp::WAIT_OPEN;
  return true;
#else
  (void)nowMs;
  return false;
#endif
}

bool SdLogger::rotateAllFilePart(uint32_t nowMs) {
#if !TFLUNACTRL_HAS_ASYNC_SD
  (void)nowMs;
  return false;
#else
  if (_allFilePart >= LOG_FILE_PART_MAX) {
    _allCapped = true;
    _allOk = false;
    setLastError(Status(Err::RESOURCE_BUSY, 0, "all file part limit reached"), nowMs);
    return false;
  }
  _allFilePart = static_cast<uint16_t>(_allFilePart + 1U);
  if (!buildSessionAllFilePath(_allFilePart, _allCurrentFilePath, sizeof(_allCurrentFilePath))) {
    _allCapped = true;
    _allOk = false;
    setLastError(Status(Err::INVALID_CONFIG, 0, "all file path overflow"), nowMs);
    return false;
  }
  strncpy(_job.path, _allCurrentFilePath, sizeof(_job.path) - 1);
  _job.path[sizeof(_job.path) - 1] = '\0';
  _job.knownSize = 0;
  _job.headerNeeded = (_job.headerLen > 0);
  _allSizeKnown = true;
  _allBytes = 0;
  _sampleRotateCount = _sampleRotateCount + 1U;
  _sampleLinesCurrentFile = 0U;
  return true;
#endif
}

bool SdLogger::rotateEventFilePart(uint32_t nowMs) {
#if !TFLUNACTRL_HAS_ASYNC_SD
  (void)nowMs;
  return false;
#else
  if (_eventFilePart >= LOG_FILE_PART_MAX) {
    setLastError(Status(Err::RESOURCE_BUSY, 0, "event file part limit reached"), nowMs);
    return false;
  }
  _eventFilePart = static_cast<uint16_t>(_eventFilePart + 1U);
  if (!buildSessionEventFilePath(_eventFilePart, _eventCurrentFilePath, sizeof(_eventCurrentFilePath))) {
    setLastError(Status(Err::INVALID_CONFIG, 0, "event file path overflow"), nowMs);
    return false;
  }
  strncpy(_job.path, _eventCurrentFilePath, sizeof(_job.path) - 1);
  _job.path[sizeof(_job.path) - 1] = '\0';
  _job.knownSize = 0;
  _job.headerNeeded = (_job.headerLen > 0);
  _eventRotateCount = _eventRotateCount + 1U;
  _eventLinesCurrentFile = 0U;
  return true;
#endif
}

bool SdLogger::ensureAllSessionReady(uint32_t nowMs) {
  if (!_settings.logAllEnabled) {
    return false;
  }
  if (_allSessionReady) {
    return true;
  }
#if !TFLUNACTRL_HAS_ASYNC_SD
  (void)nowMs;
  return false;
#else
  if (!_sdStarted || !_mounted) {
    return false;
  }
  if (_allSessionStage != SESSION_STAGE_IDLE || _sessionRequestId != AsyncSD::INVALID_REQUEST_ID) {
    return false;
  }
  if (_allSessionRetryAfterMs != 0U &&
      static_cast<int32_t>(nowMs - _allSessionRetryAfterMs) < 0) {
    return false;
  }
  const AsyncSD::RequestId id = _sd.requestMkdir(RUNS_DIR);
  if (id == AsyncSD::INVALID_REQUEST_ID) {
    const AsyncSD::ErrorInfo info = _sd.lastErrorInfo();
    setLastError(Status(Err::RESOURCE_BUSY, info.detail, "AsyncSD mkdir runs enqueue failed"), nowMs);
    _allSessionRetryAfterMs = nowMs + _settings.logWriteRetryBackoffMs;
    return false;
  }
  _sessionRequestId = id;
  _allSessionStage = SESSION_STAGE_WAIT_RUNS_DIR;
  return false;
#endif
}

void SdLogger::setLastError(const Status& error, uint32_t nowMs) {
  if (error.ok()) {
    return;
  }
  _lastError = error;
  _lastErrorMs = nowMs;
}

void SdLogger::invalidateInfoCache() {
  _sdInfoValid = false;
  _sdUsageValid = false;
  _sdFsCapacityBytes = 0;
  _sdFsUsedBytes = 0;
  _sdFsFreeBytes = 0;
  _sdCardCapacityBytes = 0;
  _sdFsTypeCode = 0;
  _sdCardTypeCode = 0;
  _sdInfoLastUpdateMs = 0;
  _lastInfoRequestMs = 0;
  _infoTimeoutStreak = 0;
}

void SdLogger::markUnmounted(uint32_t nowMs, const Status& cause) {
  const bool preserveSessionIdentity =
      _settings.logAllEnabled &&
      _allSessionDirPath[0] != '\0' &&
      _allSessionMarkerName[0] != '\0';
  _mounted = false;
#if TFLUNACTRL_HAS_ASYNC_SD
  _job = FileJob{};
  _mountStage = MountStage::IDLE;
  _mountRequestId = AsyncSD::INVALID_REQUEST_ID;
  _infoRequestId = AsyncSD::INVALID_REQUEST_ID;
  _sessionSetupHandle = AsyncSD::INVALID_FILE_HANDLE;
  // Defer the blocking _sd.end() to processDeferred() so the FreeRTOS
  // worker task has a full loop cycle to stop and release SPI resources
  // before the next _sd.begin() re-initialises the bus.
  if (_sdStarted) {
    _deferredTeardown = true;
  }
  _sdStarted = false;
#endif
  // Preserve the in-flight session identity for hot-reinsert recovery. If the
  // marker cannot be validated on remount, session setup falls back to a fresh
  // directory automatically.
  resetAllSessionState(!preserveSessionIdentity);
  _allSessionNeedsFreshName = _settings.logAllEnabled ? !preserveSessionIdentity : false;
  _allSessionRetryAfterMs = nowMs + _settings.logWriteRetryBackoffMs;
  if (_mountCycleCount < 255U) {
    _mountCycleCount++;
  }
  invalidateInfoCache();
  _lastMountAttemptMs = (nowMs == 0U) ? 1U : nowMs;
  setLastError(cause, nowMs);
}

void SdLogger::popSampleHead() {
  if (_count == 0 || _queue == nullptr || _queueCapacity == 0U) {
    return;
  }
  _queue[_tail].valid = false;
  _tail = (_tail + 1) % _queueCapacity;
  _count--;
}

void SdLogger::popEventHead() {
  if (_eventCount == 0 || _eventQueue == nullptr || _eventQueueCapacity == 0U) {
    return;
  }
  _eventQueue[_eventTail].valid = false;
  _eventTail = (_eventTail + 1) % _eventQueueCapacity;
  _eventCount--;
}

void SdLogger::releaseQueues() {
  if (_queue != nullptr) {
    PsramSupport::freeMemory(_queue);
    _queue = nullptr;
  }
  if (_eventQueue != nullptr) {
    PsramSupport::freeMemory(_eventQueue);
    _eventQueue = nullptr;
  }
  _queueCapacity = 0U;
  _eventQueueCapacity = 0U;
  _queueUsingPsram = false;
  _eventQueueUsingPsram = false;
  _head = 0U;
  _tail = 0U;
  _count = 0U;
  _eventHead = 0U;
  _eventTail = 0U;
  _eventCount = 0U;
}

Status SdLogger::allocateQueues(bool preferPsram) {
  releaseQueues();

  struct QueueTier {
    size_t sampleCapacity;
    size_t eventCapacity;
    bool usePsram;
  };

  QueueTier tiers[3] = {};
  size_t tierCount = 0U;
  if (preferPsram) {
    tiers[tierCount++] = {SD_SAMPLE_QUEUE_PSRAM_TIER0, SD_EVENT_QUEUE_PSRAM_TIER0, true};
    if (SD_SAMPLE_QUEUE_PSRAM_TIER1 >= HardwareSettings::SD_LOG_QUEUE_CAPACITY &&
        SD_EVENT_QUEUE_PSRAM_TIER1 >= HardwareSettings::SD_EVENT_QUEUE_CAPACITY) {
      tiers[tierCount++] = {SD_SAMPLE_QUEUE_PSRAM_TIER1, SD_EVENT_QUEUE_PSRAM_TIER1, true};
    }
  }
  tiers[tierCount++] = {HardwareSettings::SD_LOG_QUEUE_CAPACITY,
                        HardwareSettings::SD_EVENT_QUEUE_CAPACITY,
                        false};

  for (size_t i = 0U; i < tierCount; ++i) {
    const QueueTier tier = tiers[i];
    if (tier.sampleCapacity == 0U || tier.eventCapacity == 0U) {
      continue;
    }
    if (tier.sampleCapacity > (SIZE_MAX / sizeof(LogRecord)) ||
        tier.eventCapacity > (SIZE_MAX / sizeof(EventRecord))) {
      continue;
    }

    const size_t sampleBytes = tier.sampleCapacity * sizeof(LogRecord);
    const size_t eventBytes = tier.eventCapacity * sizeof(EventRecord);
    void* sampleMem = tier.usePsram ? PsramSupport::allocPsram(sampleBytes)
                                    : PsramSupport::allocInternal(sampleBytes);
    if (sampleMem == nullptr) {
      continue;
    }

    void* eventMem = tier.usePsram ? PsramSupport::allocPsram(eventBytes)
                                   : PsramSupport::allocInternal(eventBytes);
    if (eventMem == nullptr) {
      PsramSupport::freeMemory(sampleMem);
      continue;
    }

    memset(sampleMem, 0, sampleBytes);
    memset(eventMem, 0, eventBytes);
    _queue = static_cast<LogRecord*>(sampleMem);
    _eventQueue = static_cast<EventRecord*>(eventMem);
    _queueCapacity = tier.sampleCapacity;
    _eventQueueCapacity = tier.eventCapacity;
    _queueUsingPsram = tier.usePsram;
    _eventQueueUsingPsram = tier.usePsram;
    _head = 0U;
    _tail = 0U;
    _count = 0U;
    _eventHead = 0U;
    _eventTail = 0U;
    _eventCount = 0U;
    return Ok();
  }

  return Status(Err::OUT_OF_MEMORY, 0, "sd log queue alloc failed");
}

Status SdLogger::begin(const HardwareSettings& config,
                       const AppSettings& appSettings,
                       const RuntimeSettings& settings) {
  releaseQueues();
  _config = config;
  _appSettings = appSettings;
  _settings = settings;
  _enabled = appSettings.enableSd;
  if (_enabled) {
    const Status queueAlloc = allocateQueues(PsramSupport::isAvailable());
    if (!queueAlloc.ok()) {
      _enabled = false;
      return queueAlloc;
    }
  }
  _mounted = false;
  _sdInfoValid = false;
  _sdUsageValid = false;
  _dailyOk = _settings.logDailyEnabled;
  _allOk = _settings.logAllEnabled;
  _allCapped = false;
  _allSizeKnown = false;
  _allBytes = 0;
  _lastWriteMs = 0;
  _lastFlushMs = 0;
  _lastMountAttemptMs = 0;
  _mountCycleCount = 0;
  _lastInfoRequestMs = 0;
  _infoTimeoutStreak = 0;
  _sdInfoLastUpdateMs = 0;
  _lastTickElapsedMs = 0;
  _budgetExceededCount = 0;
  _dropped = 0;
  _eventDropped = 0;
  _lastErrorMs = 0;
  _lastError = Ok();
  _sdFsCapacityBytes = 0;
  _sdFsUsedBytes = 0;
  _sdFsFreeBytes = 0;
  _sdCardCapacityBytes = 0;
  _sdFsTypeCode = 0;
  _sdCardTypeCode = 0;
  _allSessionReady = false;
  _allSessionSeq = 1;
  _allSessionProbeCount = 0;
  _allFilePart = 1;
  _eventFilePart = 1;
  _allSessionRetryAfterMs = 0;
  _allSessionNeedsFreshName = _settings.logAllEnabled;
  _allSessionStage = SESSION_STAGE_IDLE;
  _allSessionDirName[0] = '\0';
  _allSessionDirPath[0] = '\0';
  _allSessionMarkerName[0] = '\0';
  _allCurrentFilePath[0] = '\0';
  if (!buildSessionEventFilePath(_eventFilePart, _eventCurrentFilePath, sizeof(_eventCurrentFilePath))) {
    _eventCurrentFilePath[0] = '\0';
  }
  _sampleWriteSuccessCount = 0;
  _sampleWriteFailureCount = 0;
  _eventWriteSuccessCount = 0;
  _eventWriteFailureCount = 0;
  _sampleRotateCount = 0;
  _eventRotateCount = 0;
  _sampleLinesCurrentFile = 0;
  _eventLinesCurrentFile = 0;
  _sampleLinesTotal = 0;
  _eventLinesTotal = 0;
  _head = 0;
  _tail = 0;
  _count = 0;
  _eventHead = 0;
  _eventTail = 0;
  _eventCount = 0;

#if TFLUNACTRL_HAS_ASYNC_SD
  _sdStarted = false;
  _mountStage = MountStage::IDLE;
  _mountRequestId = AsyncSD::INVALID_REQUEST_ID;
  _infoRequestId = AsyncSD::INVALID_REQUEST_ID;
  _sessionRequestId = AsyncSD::INVALID_REQUEST_ID;
  _sessionSetupHandle = AsyncSD::INVALID_FILE_HANDLE;
  _job = FileJob{};
#endif

#ifdef ARDUINO
  if (_config.sdCdPin >= 0) {
    pinMode(static_cast<uint8_t>(_config.sdCdPin), _config.sdCdActiveLow ? INPUT_PULLUP : INPUT);
  }
#endif

  if (!_enabled) {
    return Ok();
  }

  return mount(0);
}

void SdLogger::applySettings(const RuntimeSettings& settings) {
  const bool prevAllEnabled = _settings.logAllEnabled;
  const uint32_t prevAllMaxBytes = _settings.logAllMaxBytes;
  const bool sessionNameChanged =
      strncmp(_settings.logSessionName, settings.logSessionName, sizeof(_settings.logSessionName)) != 0;
  _settings = settings;
  _dailyOk = !_settings.logDailyEnabled ? true : _dailyOk;
  if (!_settings.logAllEnabled) {
    _allOk = true;
    _allCapped = false;
    _allSessionReady = false;
    _allSessionNeedsFreshName = true;
    _allSessionStage = SESSION_STAGE_IDLE;
    _allSessionRetryAfterMs = 0;
#if TFLUNACTRL_HAS_ASYNC_SD
    _sessionRequestId = AsyncSD::INVALID_REQUEST_ID;
#endif
  } else if (_allCapped) {
    if (_settings.logAllMaxBytes > prevAllMaxBytes) {
      _allCapped = false;
      _allOk = true;
      _allSizeKnown = false;
    } else {
      _allOk = false;
    }
  }
  if (!prevAllEnabled && _settings.logAllEnabled) {
    resetAllSessionState(true);
    _allSessionNeedsFreshName = true;
    _allOk = true;
    _allCapped = false;
    _allSizeKnown = false;
    _allBytes = 0;
  }
  if (sessionNameChanged) {
    resetAllSessionState(true);
    _allSessionNeedsFreshName = true;
    _allSessionSeq = 1U;
    _allSessionRetryAfterMs = 0U;
    _allCapped = false;
    _allSizeKnown = false;
    _allBytes = 0U;
    _allOk = true;
  }
}

void SdLogger::end() {
  _enabled = false;
  _mounted = false;
  _mountCycleCount = 0;
  invalidateInfoCache();
  resetAllSessionState(true);
  _allSessionNeedsFreshName = false;
  _sampleWriteSuccessCount = 0;
  _sampleWriteFailureCount = 0;
  _eventWriteSuccessCount = 0;
  _eventWriteFailureCount = 0;
  _sampleRotateCount = 0;
  _eventRotateCount = 0;
  _sampleLinesTotal = 0;
  _eventLinesTotal = 0;
#if TFLUNACTRL_HAS_ASYNC_SD
  if (_sdStarted) {
    _sd.end();
    _sdStarted = false;
  }
  _mountStage = MountStage::IDLE;
  _mountRequestId = AsyncSD::INVALID_REQUEST_ID;
  _infoRequestId = AsyncSD::INVALID_REQUEST_ID;
  _sessionRequestId = AsyncSD::INVALID_REQUEST_ID;
  _sessionSetupHandle = AsyncSD::INVALID_FILE_HANDLE;
  _job = FileJob{};
#endif
  releaseQueues();
}

bool SdLogger::isCardPresent() const {
  if (_config.sdCdPin < 0) {
    return true;
  }
#ifdef ARDUINO
  const int level = digitalRead(static_cast<uint8_t>(_config.sdCdPin));
  return _config.sdCdActiveLow ? (level == LOW) : (level == HIGH);
#else
  return true;
#endif
}

#if TFLUNACTRL_HAS_ASYNC_SD
static const char* asyncSdErrorMessage(AsyncSD::ErrorCode code, const char* fallback) {
  switch (code) {
    case AsyncSD::ErrorCode::CardInitFailed:
      return "SD card init failed";
    case AsyncSD::ErrorCode::MountFailed:
      return "SD mount failed";
    case AsyncSD::ErrorCode::FsUnsupported:
      return "SD filesystem unsupported";
    case AsyncSD::ErrorCode::FsCorrupt:
      return "SD filesystem corrupt";
    case AsyncSD::ErrorCode::WriteProtect:
      return "SD write protected";
    case AsyncSD::ErrorCode::ReadOnly:
      return "SD read-only";
    case AsyncSD::ErrorCode::NoSpaceLeft:
      return "SD full";
    case AsyncSD::ErrorCode::NotReady:
      return "SD not ready";
    case AsyncSD::ErrorCode::BusNotAvailable:
      return "SD bus unavailable";
    case AsyncSD::ErrorCode::IoError:
      return "SD I/O error";
    default:
      return fallback;
  }
}

static const char* asyncSdStateMessage(AsyncSD::SdStatus state) {
  switch (state) {
    case AsyncSD::SdStatus::NoCard:
      return "SD card not present";
    case AsyncSD::SdStatus::Removed:
      return "SD card removed";
    case AsyncSD::SdStatus::Error:
      return "SD status error";
    case AsyncSD::SdStatus::Fault:
      return "SD status fault";
    default:
      return "SD not ready";
  }
}

Status SdLogger::mapAsyncResult(const AsyncSD::RequestResult& result, const char* msg) {
  const char* statusMsg = asyncSdErrorMessage(result.code, msg);
  switch (result.code) {
    case AsyncSD::ErrorCode::Ok:
      return Ok();
    case AsyncSD::ErrorCode::Timeout:
      return Status(Err::TIMEOUT, result.detail, statusMsg);
    case AsyncSD::ErrorCode::Busy:
      return Status(Err::RESOURCE_BUSY, result.detail, statusMsg);
    case AsyncSD::ErrorCode::NoMem:
      return Status(Err::OUT_OF_MEMORY, result.detail, statusMsg);
    case AsyncSD::ErrorCode::InvalidArgument:
      return Status(Err::INVALID_CONFIG, result.detail, statusMsg);
    case AsyncSD::ErrorCode::NotInitialized:
      return Status(Err::NOT_INITIALIZED, result.detail, statusMsg);
    case AsyncSD::ErrorCode::NotFound:
      return Status(Err::INVALID_CONFIG, result.detail, statusMsg);
    case AsyncSD::ErrorCode::AlreadyExists:
      return Status(Err::RESOURCE_BUSY, result.detail, statusMsg);
    case AsyncSD::ErrorCode::TooManyOpenFiles:
    case AsyncSD::ErrorCode::PathTooLong:
    case AsyncSD::ErrorCode::NameTooLong:
    case AsyncSD::ErrorCode::NoSpaceLeft:
    case AsyncSD::ErrorCode::FileTooLarge:
      return Status(Err::RESOURCE_BUSY, result.detail, statusMsg);
    case AsyncSD::ErrorCode::BusNotAvailable:
    case AsyncSD::ErrorCode::CardInitFailed:
    case AsyncSD::ErrorCode::MountFailed:
    case AsyncSD::ErrorCode::FsUnsupported:
    case AsyncSD::ErrorCode::FsCorrupt:
    case AsyncSD::ErrorCode::IoError:
    case AsyncSD::ErrorCode::ReadOnly:
    case AsyncSD::ErrorCode::WriteProtect:
    case AsyncSD::ErrorCode::NotReady:
      return Status(Err::COMM_FAILURE, result.detail, statusMsg);
    case AsyncSD::ErrorCode::Unsupported:
      return Status(Err::NOT_INITIALIZED, result.detail, statusMsg);
    case AsyncSD::ErrorCode::Fault:
    case AsyncSD::ErrorCode::InvalidContext:
    case AsyncSD::ErrorCode::InternalError:
    default:
      return Status(Err::EXTERNAL_LIB_ERROR, result.detail, statusMsg);
  }
}

bool SdLogger::setupAsyncSd(uint32_t nowMs) {
  if (_sdStarted) {
    return true;
  }
  // Defensive reset for re-init paths where previous begin failed mid-way.
  _sd.end();
  if (_config.sdCs < 0) {
    setLastError(Status(Err::INVALID_CONFIG, 0, "sdCs not set"), nowMs);
    return false;
  }

  AsyncSD::SdCardConfig sdCfg{};
  sdCfg.pinCs = _config.sdCs;
  sdCfg.pinMosi = _config.spiMosi;
  sdCfg.pinMiso = _config.spiMiso;
  sdCfg.pinSck = _config.spiSck;
  sdCfg.autoInitSpi = (_config.spiSck >= 0 && _config.spiMiso >= 0 && _config.spiMosi >= 0);
  sdCfg.cdPin = _config.sdCdPin;
  sdCfg.cdActiveLow = _config.sdCdActiveLow;
  sdCfg.cdPullup = _config.sdCdActiveLow;
  sdCfg.autoMount = false;
  sdCfg.useWorkerTask = true;
  sdCfg.workerPriority = _appSettings.sdWorkerPriority;
  sdCfg.workerStackBytes = _appSettings.sdWorkerStackBytes;
  sdCfg.workerIdleMs = _appSettings.sdWorkerIdleMs;
  sdCfg.requestQueueDepth = _appSettings.sdRequestQueueDepth;
  sdCfg.resultQueueDepth = _appSettings.sdResultQueueDepth;
  sdCfg.maxOpenFiles = _appSettings.sdMaxOpenFiles;
  sdCfg.maxPathLength = _appSettings.sdMaxPathLength;
  sdCfg.maxCopyWriteBytes = _appSettings.sdMaxCopyWriteBytes;
  sdCfg.copyWriteSlots = _appSettings.sdCopyWriteSlots;
  sdCfg.lockTimeoutMs = _appSettings.sdLockTimeoutMs;
  sdCfg.mountTimeoutMs = _appSettings.sdMountTimeoutMs;
  sdCfg.opTimeoutMs = _appSettings.sdOpTimeoutMs;
  sdCfg.ioTimeoutMs = _appSettings.sdIoTimeoutMs;
  sdCfg.ioChunkBytes = _appSettings.sdIoChunkBytes;
  sdCfg.spiFrequencyHz = _appSettings.sdSpiFrequencyHz;
  sdCfg.workerStallMs = _appSettings.sdWorkerStallMs;
  sdCfg.shutdownTimeoutMs = _appSettings.sdShutdownTimeoutMs;

  if (!_sd.begin(sdCfg)) {
    const AsyncSD::ErrorInfo info = _sd.lastErrorInfo();
    AsyncSD::RequestResult beginResult{};
    beginResult.code = info.code;
    beginResult.detail = info.detail;
    setLastError(mapAsyncResult(beginResult, "AsyncSD begin failed"), nowMs);
    return false;
  }

  _sdStarted = true;
  return true;
}

bool SdLogger::enqueueMountRequest(uint32_t nowMs) {
  const AsyncSD::RequestId id = _sd.requestMount();
  if (id == AsyncSD::INVALID_REQUEST_ID) {
    const AsyncSD::ErrorInfo info = _sd.lastErrorInfo();
    // Mount enqueue failures can leave AsyncSD in a stale state; force a
    // full teardown before the next attempt so begin()/mount() starts clean.
    if (_sdStarted) {
      _deferredTeardown = true;
      _sdStarted = false;
    }
    setLastError(Status(Err::RESOURCE_BUSY, info.detail, "AsyncSD mount enqueue failed"), nowMs);
    return false;
  }
  _lastMountAttemptMs = (nowMs == 0U) ? 1U : nowMs;
  _mountRequestId = id;
  _mountStage = MountStage::WAIT_MOUNT;
  return true;
}

bool SdLogger::enqueueInfoRequest(uint32_t nowMs) {
  if (!_sdStarted || !_mounted) {
    return false;
  }
  const AsyncSD::RequestId id = _sd.requestInfo();
  if (id == AsyncSD::INVALID_REQUEST_ID) {
    const AsyncSD::ErrorInfo info = _sd.lastErrorInfo();
    setLastError(Status(Err::RESOURCE_BUSY, info.detail, "AsyncSD info enqueue failed"), nowMs);
    _lastInfoRequestMs = (nowMs == 0U) ? 1U : nowMs;
    return false;
  }
  _infoRequestId = id;
  _lastInfoRequestMs = (nowMs == 0U) ? 1U : nowMs;
  return true;
}

bool SdLogger::isAsyncMediaFault(AsyncSD::ErrorCode code) {
  switch (code) {
    case AsyncSD::ErrorCode::BusNotAvailable:
    case AsyncSD::ErrorCode::CardInitFailed:
    case AsyncSD::ErrorCode::MountFailed:
    case AsyncSD::ErrorCode::FsUnsupported:
    case AsyncSD::ErrorCode::FsCorrupt:
    case AsyncSD::ErrorCode::IoError:
    case AsyncSD::ErrorCode::ReadOnly:
    case AsyncSD::ErrorCode::WriteProtect:
    case AsyncSD::ErrorCode::NotReady:
      return true;
    default:
      return false;
  }
}

bool SdLogger::startFileJob(JobType type,
                            size_t queueIndex,
                            uint8_t batchCount,
                            const char* path,
                            const char* header,
                            uint32_t sizeLimit,
                            bool rolloverOnLimit,
                            uint32_t nowMs) {
  if (_job.active ||
      path == nullptr ||
      batchCount == 0U ||
      batchCount > FileJob::MAX_BATCH_LINES) {
    return false;
  }
  if ((type == JobType::EVENT && (_eventQueue == nullptr || _eventQueueCapacity == 0U)) ||
      ((type == JobType::SAMPLE_DAILY || type == JobType::SAMPLE_ALL) &&
       (_queue == nullptr || _queueCapacity == 0U))) {
    return false;
  }
  _job = FileJob{};
  _job.active = true;
  _job.type = type;
  _job.queueIndex = queueIndex;
  _job.batchCount = batchCount;
  _job.startMs = (nowMs == 0U) ? 1U : nowMs;
  _job.header = header;
  _job.headerLen = (header != nullptr) ? strlen(header) : 0U;
  _job.sizeLimit = sizeLimit;
  _job.rolloverOnLimit = rolloverOnLimit;
  strncpy(_job.path, path, sizeof(_job.path) - 1U);
  _job.path[sizeof(_job.path) - 1U] = '\0';

  for (uint8_t i = 0U; i < batchCount; ++i) {
    const char* line = nullptr;
    if (type == JobType::EVENT) {
      if (static_cast<size_t>(i) >= _eventCount) {
        _job = FileJob{};
        return false;
      }
      const size_t idx = (queueIndex + static_cast<size_t>(i)) % _eventQueueCapacity;
      const EventRecord& rec = _eventQueue[idx];
      if (!rec.valid) {
        _job = FileJob{};
        return false;
      }
      line = rec.line;
    } else if (type == JobType::SAMPLE_DAILY || type == JobType::SAMPLE_ALL) {
      if (static_cast<size_t>(i) >= _count) {
        _job = FileJob{};
        return false;
      }
      const size_t idx = (queueIndex + static_cast<size_t>(i)) % _queueCapacity;
      const LogRecord& rec = _queue[idx];
      if (!rec.valid) {
        _job = FileJob{};
        return false;
      }
      line = rec.line;
    } else {
      _job = FileJob{};
      return false;
    }
    const size_t lineLen = strlen(line);
    if (lineLen == 0U || lineLen > UINT16_MAX) {
      _job = FileJob{};
      return false;
    }
    _job.lines[i] = line;
    _job.lineLens[i] = static_cast<uint16_t>(lineLen);
    _job.lineBytesTotal += static_cast<uint32_t>(lineLen);
  }

  const AsyncSD::RequestId id = _sd.requestStat(_job.path);
  if (id == AsyncSD::INVALID_REQUEST_ID) {
    const AsyncSD::ErrorInfo info = _sd.lastErrorInfo();
    setLastError(Status(Err::RESOURCE_BUSY, info.detail, "AsyncSD stat enqueue failed"), nowMs);
    _job = FileJob{};
    return false;
  }
  _job.requestId = id;
  _job.op = JobOp::WAIT_STAT;
  return true;
}

bool SdLogger::enqueueJobWrite(const void* data, size_t len, const char* errMsg, uint32_t nowMs) {
  if (_job.handle == AsyncSD::INVALID_FILE_HANDLE || data == nullptr || len == 0U) {
    failFileJob(Status(Err::INVALID_CONFIG, 0, "invalid SD write request"), nowMs);
    return false;
  }

  AsyncSD::RequestId id = AsyncSD::INVALID_REQUEST_ID;
  const bool useCopyWrite = (_appSettings.sdCopyWriteSlots > 0U) &&
                            (_appSettings.sdMaxCopyWriteBytes > 0U) &&
                            (len <= static_cast<size_t>(_appSettings.sdMaxCopyWriteBytes));
  if (useCopyWrite) {
    id = _sd.requestWriteCopy(_job.handle, AsyncSD::APPEND_OFFSET, data, len);
  } else {
    id = _sd.requestWrite(_job.handle, AsyncSD::APPEND_OFFSET, data, len);
  }

  if (id == AsyncSD::INVALID_REQUEST_ID) {
    const AsyncSD::ErrorInfo info = _sd.lastErrorInfo();
    failFileJob(Status(Err::RESOURCE_BUSY, info.detail, errMsg), nowMs);
    return false;
  }
  _job.requestId = id;
  return true;
}

void SdLogger::popCompletedSampleHeadRecords() {
  while (_count > 0U) {
    const LogRecord& head = _queue[_tail];
    if (!head.valid || (!head.pendingDaily && !head.pendingAll)) {
      popSampleHead();
      continue;
    }
    break;
  }
}

void SdLogger::popInvalidEventHeadRecords() {
  while (_eventCount > 0U) {
    if (_eventQueue[_eventTail].valid) {
      break;
    }
    popEventHead();
  }
}

void SdLogger::handleMountResult(const AsyncSD::RequestResult& result, uint32_t nowMs) {
  // IoError is treated as 'may exist' for mkdir operations because some
  // FAT32 implementations report IoError for pre-existing directories after
  // hot-remove/reinsert cycles. Real media faults are caught by the explicit
  // isAsyncMediaFault() checks on mount and write paths.
  const bool mkdirMayExist = (result.code == AsyncSD::ErrorCode::Ok ||
                              result.code == AsyncSD::ErrorCode::IoError ||
                              result.code == AsyncSD::ErrorCode::AlreadyExists);

  switch (_mountStage) {
    case MountStage::WAIT_MOUNT: {
      if (result.code != AsyncSD::ErrorCode::Ok) {
        _mounted = false;
        _mountStage = MountStage::IDLE;
        if (_sdStarted) {
          _deferredTeardown = true;
          _sdStarted = false;
        }
        setLastError(mapAsyncResult(result, "AsyncSD mount failed"), nowMs);
        return;
      }
      const AsyncSD::RequestId id = _sd.requestMkdir(LOG_DIR);
      if (id == AsyncSD::INVALID_REQUEST_ID) {
        _mounted = false;
        _mountStage = MountStage::IDLE;
        const AsyncSD::ErrorInfo info = _sd.lastErrorInfo();
        setLastError(Status(Err::RESOURCE_BUSY, info.detail, "AsyncSD mkdir logs enqueue failed"), nowMs);
        return;
      }
      _mountRequestId = id;
      _mountStage = MountStage::WAIT_MKDIR_LOGS;
      return;
    }
    case MountStage::WAIT_MKDIR_LOGS: {
      if (!mkdirMayExist) {
        _mounted = false;
        _mountStage = MountStage::IDLE;
        if (_sdStarted) {
          _deferredTeardown = true;
          _sdStarted = false;
        }
        setLastError(mapAsyncResult(result, "AsyncSD mkdir logs failed"), nowMs);
        return;
      }
      const AsyncSD::RequestId id = _sd.requestMkdir(DAILY_DIR);
      if (id == AsyncSD::INVALID_REQUEST_ID) {
        _mounted = false;
        _mountStage = MountStage::IDLE;
        const AsyncSD::ErrorInfo info = _sd.lastErrorInfo();
        setLastError(Status(Err::RESOURCE_BUSY, info.detail, "AsyncSD mkdir daily enqueue failed"), nowMs);
        return;
      }
      _mountRequestId = id;
      _mountStage = MountStage::WAIT_MKDIR_DAILY;
      return;
    }
    case MountStage::WAIT_MKDIR_DAILY: {
      if (!mkdirMayExist) {
        _mounted = false;
        _mountStage = MountStage::IDLE;
        if (_sdStarted) {
          _deferredTeardown = true;
          _sdStarted = false;
        }
        setLastError(mapAsyncResult(result, "AsyncSD mkdir daily failed"), nowMs);
        return;
      }
      // Consider mount ready after directory setup to avoid hard dependency on optional listdir APIs.
      _mounted = true;
      _mountStage = MountStage::READY;
      _dailyOk = _settings.logDailyEnabled;
      _allOk = _settings.logAllEnabled && !_allCapped;
      _allSizeKnown = false;
      _lastError = Ok();
      _lastErrorMs = 0U;
      return;
    }
    case MountStage::IDLE:
    case MountStage::READY:
    default:
      return;
  }
}

void SdLogger::completeFileJobSuccess(uint32_t nowMs) {
  _lastError = Ok();
  _lastErrorMs = 0U;
  _mountCycleCount = 0;
  switch (_job.type) {
    case JobType::SAMPLE_DAILY: {
      if (_count == 0 || _job.queueIndex != _tail) {
        break;
      }
      uint8_t completed = 0U;
      for (uint8_t i = 0U; i < _job.batchCount && static_cast<size_t>(i) < _count; ++i) {
        const size_t idx = (_job.queueIndex + static_cast<size_t>(i)) % _queueCapacity;
        LogRecord& rec = _queue[idx];
        if (!rec.valid) {
          break;
        }
        rec.pendingDaily = false;
        rec.dailyRetries = 0;
        rec.nextAttemptMs = 0;
        completed = static_cast<uint8_t>(completed + 1U);
      }
      _dailyOk = true;
      if (completed > 0U) {
        popCompletedSampleHeadRecords();
        _lastWriteMs = nowMs;
        _lastFlushMs = nowMs;
      }
      break;
    }
    case JobType::SAMPLE_ALL: {
      if (_count == 0 || _job.queueIndex != _tail) {
        break;
      }
      uint8_t completed = 0U;
      for (uint8_t i = 0U; i < _job.batchCount && static_cast<size_t>(i) < _count; ++i) {
        const size_t idx = (_job.queueIndex + static_cast<size_t>(i)) % _queueCapacity;
        LogRecord& rec = _queue[idx];
        if (!rec.valid) {
          break;
        }
        rec.pendingAll = false;
        rec.allRetries = 0;
        rec.nextAttemptMs = 0;
        completed = static_cast<uint8_t>(completed + 1U);
      }
      _allOk = true;
      _allSizeKnown = true;
      _allBytes = _job.knownSize;
      if (completed > 0U) {
        const uint32_t completedCount = static_cast<uint32_t>(completed);
        _sampleWriteSuccessCount = _sampleWriteSuccessCount + completedCount;
        _sampleLinesCurrentFile = _sampleLinesCurrentFile + completedCount;
        _sampleLinesTotal = _sampleLinesTotal + completedCount;
        popCompletedSampleHeadRecords();
        _lastWriteMs = nowMs;
        _lastFlushMs = nowMs;
      }
      break;
    }
    case JobType::EVENT: {
      if (_eventCount == 0 || _job.queueIndex != _eventTail) {
        break;
      }
      uint8_t completed = 0U;
      for (uint8_t i = 0U; i < _job.batchCount && static_cast<size_t>(i) < _eventCount; ++i) {
        const size_t idx = (_job.queueIndex + static_cast<size_t>(i)) % _eventQueueCapacity;
        EventRecord& rec = _eventQueue[idx];
        if (!rec.valid) {
          break;
        }
        rec.valid = false;
        rec.retries = 0;
        rec.nextAttemptMs = 0;
        completed = static_cast<uint8_t>(completed + 1U);
      }
      if (completed > 0U) {
        const uint32_t completedCount = static_cast<uint32_t>(completed);
        popInvalidEventHeadRecords();
        _eventWriteSuccessCount = _eventWriteSuccessCount + completedCount;
        _eventLinesCurrentFile = _eventLinesCurrentFile + completedCount;
        _eventLinesTotal = _eventLinesTotal + completedCount;
        _lastWriteMs = nowMs;
        _lastFlushMs = nowMs;
      }
      break;
    }
    case JobType::NONE:
    default:
      break;
  }

  _job = FileJob{};
}

void SdLogger::applyFileJobFailure(const Status& st, uint32_t nowMs) {
  setLastError(st, nowMs);

  switch (_job.type) {
    case JobType::SAMPLE_DAILY: {
      if (_count == 0 || _job.queueIndex != _tail) {
        break;
      }
      _dailyOk = false;
      bool droppedAny = false;
      for (uint8_t i = 0U; i < _job.batchCount && static_cast<size_t>(i) < _count; ++i) {
        const size_t idx = (_job.queueIndex + static_cast<size_t>(i)) % _queueCapacity;
        LogRecord& rec = _queue[idx];
        if (!rec.valid) {
          break;
        }
        if (rec.dailyRetries < 0xFFU) {
          rec.dailyRetries = static_cast<uint8_t>(rec.dailyRetries + 1U);
        }
        if (rec.dailyRetries >= _settings.logMaxWriteRetries) {
          rec.pendingDaily = false;
          rec.nextAttemptMs = 0U;
          _dropped++;
          droppedAny = true;
        } else {
          rec.nextAttemptMs = nowMs + _settings.logWriteRetryBackoffMs;
        }
      }
      if (droppedAny) {
        setLastError(Status(Err::COMM_FAILURE, 0, "daily write dropped"), nowMs);
      }
      popCompletedSampleHeadRecords();
      break;
    }
    case JobType::SAMPLE_ALL: {
      if (_count == 0 || _job.queueIndex != _tail) {
        break;
      }
      _allOk = false;
      uint8_t affected = 0U;
      bool droppedAny = false;
      for (uint8_t i = 0U; i < _job.batchCount && static_cast<size_t>(i) < _count; ++i) {
        const size_t idx = (_job.queueIndex + static_cast<size_t>(i)) % _queueCapacity;
        LogRecord& rec = _queue[idx];
        if (!rec.valid) {
          break;
        }
        if (rec.allRetries < 0xFFU) {
          rec.allRetries = static_cast<uint8_t>(rec.allRetries + 1U);
        }
        if (rec.allRetries >= _settings.logMaxWriteRetries) {
          rec.pendingAll = false;
          rec.nextAttemptMs = 0U;
          _dropped++;
          droppedAny = true;
        } else {
          rec.nextAttemptMs = nowMs + _settings.logWriteRetryBackoffMs;
        }
        affected = static_cast<uint8_t>(affected + 1U);
      }
      if (affected > 0U) {
        _sampleWriteFailureCount = _sampleWriteFailureCount + static_cast<uint32_t>(affected);
      }
      if (droppedAny) {
        setLastError(Status(Err::COMM_FAILURE, 0, "all.csv write dropped"), nowMs);
      }
      popCompletedSampleHeadRecords();
      break;
    }
    case JobType::EVENT: {
      if (_eventCount == 0 || _job.queueIndex != _eventTail) {
        break;
      }
      uint8_t affected = 0U;
      bool droppedAny = false;
      for (uint8_t i = 0U; i < _job.batchCount && static_cast<size_t>(i) < _eventCount; ++i) {
        const size_t idx = (_job.queueIndex + static_cast<size_t>(i)) % _eventQueueCapacity;
        EventRecord& rec = _eventQueue[idx];
        if (!rec.valid) {
          break;
        }
        if (rec.retries < 0xFFU) {
          rec.retries = static_cast<uint8_t>(rec.retries + 1U);
        }
        if (rec.retries >= _settings.logMaxWriteRetries) {
          rec.valid = false;
          rec.nextAttemptMs = 0U;
          _eventDropped++;
          droppedAny = true;
        } else {
          rec.nextAttemptMs = nowMs + _settings.logWriteRetryBackoffMs;
        }
        affected = static_cast<uint8_t>(affected + 1U);
      }
      if (affected > 0U) {
        _eventWriteFailureCount = _eventWriteFailureCount + static_cast<uint32_t>(affected);
      }
      popInvalidEventHeadRecords();
      if (droppedAny) {
        setLastError(Status(Err::COMM_FAILURE, 0, "events dropped"), nowMs);
      }
      break;
    }
    case JobType::NONE:
    default:
      break;
  }

  if (st.code == Err::COMM_FAILURE) {
    markUnmounted(nowMs, st);
  }

  _job = FileJob{};
}

void SdLogger::failFileJob(const Status& st, uint32_t nowMs) {
  if (!_job.active) {
    return;
  }

  if (_job.handle != AsyncSD::INVALID_FILE_HANDLE && _job.op != JobOp::WAIT_CLOSE) {
    const AsyncSD::RequestId id = _sd.requestClose(_job.handle);
    if (id != AsyncSD::INVALID_REQUEST_ID) {
      _job.failurePending = true;
      _job.failureStatus = st;
      _job.requestId = id;
      _job.op = JobOp::WAIT_CLOSE;
      return;
    }
  }

  applyFileJobFailure(st, nowMs);
}

void SdLogger::handleFileJobResult(const AsyncSD::RequestResult& result, uint32_t nowMs) {
  switch (_job.op) {
    case JobOp::WAIT_STAT: {
      if (result.code == AsyncSD::ErrorCode::Ok) {
        const uint64_t size = result.stat.size;
        _job.knownSize = (size > 0xFFFFFFFFULL) ? 0xFFFFFFFFUL : static_cast<uint32_t>(size);
      } else if (result.code == AsyncSD::ErrorCode::IoError ||
                 result.code == AsyncSD::ErrorCode::NotFound) {
        _job.knownSize = 0;
      } else {
        failFileJob(mapAsyncResult(result, "AsyncSD stat failed"), nowMs);
        return;
      }

      _job.headerNeeded = (_job.knownSize == 0 && _job.headerLen > 0);
      uint64_t expected = static_cast<uint64_t>(_job.knownSize) + _job.lineBytesTotal;
      if (_job.headerNeeded) {
        expected += _job.headerLen;
      }

      if (_job.sizeLimit > 0 && expected > _job.sizeLimit) {
        if (_job.type == JobType::SAMPLE_ALL) {
          if (!rotateAllFilePart(nowMs)) {
            if (_count > 0 && _job.queueIndex == _tail) {
              for (uint8_t i = 0U; i < _job.batchCount && static_cast<size_t>(i) < _count; ++i) {
                const size_t idx = (_job.queueIndex + static_cast<size_t>(i)) % _queueCapacity;
                _queue[idx].pendingAll = false;
              }
              popCompletedSampleHeadRecords();
            }
            _job = FileJob{};
            return;
          }
          (void)issueOpenForCurrentJob(nowMs);
          return;
        }
        if (_job.type == JobType::EVENT) {
          if (!rotateEventFilePart(nowMs)) {
            failFileJob(Status(Err::RESOURCE_BUSY, 0, "event file rotate failed"), nowMs);
            return;
          }
          (void)issueOpenForCurrentJob(nowMs);
          return;
        }
        if (_job.rolloverOnLimit) {
          failFileJob(Status(Err::RESOURCE_BUSY, 0, "rollover unsupported for this job"), nowMs);
          return;
        }
      }

      (void)issueOpenForCurrentJob(nowMs);
      return;
    }

    case JobOp::WAIT_ROLLOVER_REMOVE_PREV: {
      (void)result;
      failFileJob(Status(Err::RESOURCE_BUSY, 0, "rollover stage disabled"), nowMs);
      return;
    }

    case JobOp::WAIT_ROLLOVER_RENAME: {
      (void)result;
      failFileJob(Status(Err::RESOURCE_BUSY, 0, "rollover stage disabled"), nowMs);
      return;
    }

    case JobOp::WAIT_OPEN: {
      if (result.code != AsyncSD::ErrorCode::Ok || result.handle == AsyncSD::INVALID_FILE_HANDLE) {
        failFileJob(mapAsyncResult(result, "AsyncSD open failed"), nowMs);
        return;
      }
      _job.handle = result.handle;
      _job.writeIndex = 0U;
      if (_job.headerNeeded && _job.headerLen > 0) {
        if (!enqueueJobWrite(_job.header, _job.headerLen, "AsyncSD header enqueue failed", nowMs)) {
          return;
        }
        _job.op = JobOp::WAIT_HEADER_WRITE;
        return;
      }

      if (_job.batchCount == 0U || _job.lines[0] == nullptr || _job.lineLens[0] == 0U) {
        failFileJob(Status(Err::INTERNAL_ERROR, 0, "invalid line batch"), nowMs);
        return;
      }
      if (!enqueueJobWrite(_job.lines[0], _job.lineLens[0], "AsyncSD line enqueue failed", nowMs)) {
        return;
      }
      _job.op = JobOp::WAIT_LINE_WRITE;
      return;
    }

    case JobOp::WAIT_HEADER_WRITE: {
      if (result.code != AsyncSD::ErrorCode::Ok || result.bytesProcessed != _job.headerLen) {
        failFileJob(mapAsyncResult(result, "AsyncSD header write failed"), nowMs);
        return;
      }
      _job.knownSize += static_cast<uint32_t>(_job.headerLen);
      _job.writeIndex = 0U;
      if (_job.batchCount == 0U || _job.lines[0] == nullptr || _job.lineLens[0] == 0U) {
        failFileJob(Status(Err::INTERNAL_ERROR, 0, "invalid line batch"), nowMs);
        return;
      }
      if (!enqueueJobWrite(_job.lines[0], _job.lineLens[0], "AsyncSD line enqueue failed", nowMs)) {
        return;
      }
      _job.op = JobOp::WAIT_LINE_WRITE;
      return;
    }

    case JobOp::WAIT_LINE_WRITE: {
      if (_job.writeIndex >= _job.batchCount) {
        failFileJob(Status(Err::INTERNAL_ERROR, 0, "line index overflow"), nowMs);
        return;
      }
      const uint16_t lineLen = _job.lineLens[_job.writeIndex];
      if (result.code != AsyncSD::ErrorCode::Ok || result.bytesProcessed != lineLen) {
        failFileJob(mapAsyncResult(result, "AsyncSD line write failed"), nowMs);
        return;
      }
      _job.knownSize += static_cast<uint32_t>(lineLen);

      _job.writeIndex = static_cast<uint8_t>(_job.writeIndex + 1U);
      if (_job.writeIndex < _job.batchCount) {
        const char* nextLine = _job.lines[_job.writeIndex];
        const uint16_t nextLen = _job.lineLens[_job.writeIndex];
        if (nextLine == nullptr || nextLen == 0U) {
          failFileJob(Status(Err::INTERNAL_ERROR, 0, "invalid line batch"), nowMs);
          return;
        }
        if (!enqueueJobWrite(nextLine, nextLen, "AsyncSD line enqueue failed", nowMs)) {
          return;
        }
        _job.op = JobOp::WAIT_LINE_WRITE;
        return;
      }

      const AsyncSD::RequestId id = _sd.requestClose(_job.handle);
      if (id == AsyncSD::INVALID_REQUEST_ID) {
        const AsyncSD::ErrorInfo info = _sd.lastErrorInfo();
        failFileJob(Status(Err::RESOURCE_BUSY, info.detail, "AsyncSD close enqueue failed"), nowMs);
        return;
      }
      _job.requestId = id;
      _job.op = JobOp::WAIT_CLOSE;
      return;
    }

    case JobOp::WAIT_CLOSE: {
      _job.handle = AsyncSD::INVALID_FILE_HANDLE;
      if (_job.failurePending) {
        const Status st = _job.failureStatus;
        applyFileJobFailure(st, nowMs);
        return;
      }
      if (result.code != AsyncSD::ErrorCode::Ok) {
        applyFileJobFailure(mapAsyncResult(result, "AsyncSD close failed"), nowMs);
        return;
      }
      completeFileJobSuccess(nowMs);
      return;
    }

    case JobOp::NONE:
    default:
      return;
  }
}

void SdLogger::handleAsyncResult(const AsyncSD::RequestResult& result, uint32_t nowMs) {
  if (_mountRequestId != AsyncSD::INVALID_REQUEST_ID && result.id == _mountRequestId) {
    handleMountResult(result, nowMs);
    if (_mountStage == MountStage::READY || _mountStage == MountStage::IDLE) {
      _mountRequestId = AsyncSD::INVALID_REQUEST_ID;
    }
    return;
  }
  if (_infoRequestId != AsyncSD::INVALID_REQUEST_ID && result.id == _infoRequestId) {
    if (result.code == AsyncSD::ErrorCode::Ok) {
      _infoTimeoutStreak = 0;
      _sdInfoValid = true;
      _sdUsageValid = result.fsInfo.usedBytesValid && result.fsInfo.freeBytesValid;
      _sdFsCapacityBytes = result.fsInfo.capacityBytes;
      _sdFsUsedBytes = result.fsInfo.usedBytes;
      _sdFsFreeBytes = result.fsInfo.freeBytes;
      _sdCardCapacityBytes = result.cardInfo.capacityBytes;
      _sdFsTypeCode = static_cast<uint8_t>(result.fsInfo.fsType);
      _sdCardTypeCode = static_cast<uint8_t>(result.cardInfo.type);
      _sdInfoLastUpdateMs = nowMs;
      _lastError = Ok();
      _lastErrorMs = 0U;
    } else {
      _sdInfoValid = false;
      _sdUsageValid = false;
      if (result.code == AsyncSD::ErrorCode::Timeout) {
        if (_infoTimeoutStreak < 0xFFU) {
          _infoTimeoutStreak = static_cast<uint8_t>(_infoTimeoutStreak + 1U);
        }
      } else {
        _infoTimeoutStreak = 0;
      }
      const Status st = mapAsyncResult(result, "AsyncSD info failed");
      setLastError(st, nowMs);
      // Media-level info failures indicate the card is gone or broken.
      if (isAsyncMediaFault(result.code)) {
        markUnmounted(nowMs, st);
      }
    }
    _infoRequestId = AsyncSD::INVALID_REQUEST_ID;
    return;
  }
  if (_sessionRequestId != AsyncSD::INVALID_REQUEST_ID && result.id == _sessionRequestId) {
    auto failSessionSetup = [&](const Status& st, bool deferRetry) {
      setLastError(st, nowMs);
      _allSessionStage = SESSION_STAGE_IDLE;
      _sessionRequestId = AsyncSD::INVALID_REQUEST_ID;
      _sessionSetupHandle = AsyncSD::INVALID_FILE_HANDLE;
      _allSessionRetryAfterMs = deferRetry ? (nowMs + _settings.logWriteRetryBackoffMs) : 0U;
      _allSessionReady = false;
      _allOk = false;
    };

    auto fallbackToFreshSession = [&](bool deferRetry) {
      resetAllSessionState(true);
      _allSessionNeedsFreshName = true;
      _allSessionReady = false;
      _allSessionStage = SESSION_STAGE_IDLE;
      _sessionRequestId = AsyncSD::INVALID_REQUEST_ID;
      _sessionSetupHandle = AsyncSD::INVALID_FILE_HANDLE;
      _allSessionRetryAfterMs = deferRetry ? (nowMs + _settings.logWriteRetryBackoffMs) : 0U;
      _allOk = true;
    };

    auto markSessionReady = [&]() {
      if (_allSessionNeedsFreshName && _allSessionSeq < SESSION_PROBE_MAX) {
        _allSessionSeq = static_cast<uint16_t>(_allSessionSeq + 1U);
      }
      _allSessionNeedsFreshName = false;
      _allSessionProbeCount = 0;
      _allSessionReady = true;
      _allSessionStage = SESSION_STAGE_IDLE;
      _sessionRequestId = AsyncSD::INVALID_REQUEST_ID;
      _sessionSetupHandle = AsyncSD::INVALID_FILE_HANDLE;
      _allSessionRetryAfterMs = 0U;
      _allOk = true;
      _lastError = Ok();
      _lastErrorMs = 0U;
    };

    auto queueMarkerCreate = [&]() -> bool {
      if (!ensureSessionMarkerName(nowMs)) {
        failSessionSetup(Status(Err::INVALID_CONFIG, 0, "session marker format failed"), true);
        return false;
      }
      char markerPath[HardwareSettings::SD_PATH_BYTES] = {0};
      if (!buildSessionMarkerPath(markerPath, sizeof(markerPath))) {
        failSessionSetup(Status(Err::INVALID_CONFIG, 0, "session marker path failed"), true);
        return false;
      }
      const AsyncSD::RequestId openId =
          _sd.requestOpen(markerPath,
                          AsyncSD::OpenMode::Write |
                              AsyncSD::OpenMode::Create |
                              AsyncSD::OpenMode::Truncate);
      if (openId == AsyncSD::INVALID_REQUEST_ID) {
        const AsyncSD::ErrorInfo info = _sd.lastErrorInfo();
        failSessionSetup(Status(Err::RESOURCE_BUSY, info.detail, "AsyncSD open marker enqueue failed"), true);
        return false;
      }
      _sessionRequestId = openId;
      _allSessionStage = SESSION_STAGE_WAIT_MARKER_CREATE_OPEN;
      return true;
    };

    // AsyncSD may report IoError for mkdir on pre-existing directories after
    // hot-remove/reinsert cycles. Treat it as "exists" to keep session setup
    // progressing and let explicit media-fault paths handle real mount issues.
    const bool mkdirOkOrExists = (result.code == AsyncSD::ErrorCode::Ok ||
                                  result.code == AsyncSD::ErrorCode::IoError ||
                                  result.code == AsyncSD::ErrorCode::AlreadyExists);
    if (_allSessionStage == SESSION_STAGE_WAIT_RUNS_DIR) {
      if (!mkdirOkOrExists) {
        failSessionSetup(mapAsyncResult(result, "AsyncSD mkdir runs failed"), true);
        return;
      }
      if (_allSessionNeedsFreshName) {
        if (_allSessionSeq == 0U) {
          _allSessionSeq = 1U;
        }
        char candidatePath[HardwareSettings::SD_PATH_BYTES] = {0};
        char candidateName[sizeof(_allSessionDirName)] = {0};
        if (!buildAllSessionCandidate(_allSessionSeq,
                                      candidatePath,
                                      sizeof(candidatePath),
                                      candidateName,
                                      sizeof(candidateName))) {
          failSessionSetup(Status(Err::INVALID_CONFIG, 0, "session path format failed"), true);
          return;
        }
        strncpy(_allSessionDirName, candidateName, sizeof(_allSessionDirName) - 1);
        _allSessionDirName[sizeof(_allSessionDirName) - 1] = '\0';
        strncpy(_allSessionDirPath, candidatePath, sizeof(_allSessionDirPath) - 1);
        _allSessionDirPath[sizeof(_allSessionDirPath) - 1] = '\0';
      } else if (_allSessionDirPath[0] == '\0') {
        char candidatePath[HardwareSettings::SD_PATH_BYTES] = {0};
        char candidateName[sizeof(_allSessionDirName)] = {0};
        if (!buildAllSessionCandidate(_allSessionSeq,
                                      candidatePath,
                                      sizeof(candidatePath),
                                      candidateName,
                                      sizeof(candidateName))) {
          failSessionSetup(Status(Err::INVALID_CONFIG, 0, "session path format failed"), true);
          return;
        }
        strncpy(_allSessionDirName, candidateName, sizeof(_allSessionDirName) - 1);
        _allSessionDirName[sizeof(_allSessionDirName) - 1] = '\0';
        strncpy(_allSessionDirPath, candidatePath, sizeof(_allSessionDirPath) - 1);
        _allSessionDirPath[sizeof(_allSessionDirPath) - 1] = '\0';
      }

      const AsyncSD::RequestId mkId = _sd.requestMkdir(_allSessionDirPath);
      if (mkId == AsyncSD::INVALID_REQUEST_ID) {
        const AsyncSD::ErrorInfo info = _sd.lastErrorInfo();
        failSessionSetup(Status(Err::RESOURCE_BUSY, info.detail, "AsyncSD mkdir session enqueue failed"), true);
        return;
      }
      _allSessionStage = SESSION_STAGE_WAIT_SESSION_DIR;
      _sessionRequestId = mkId;
      return;
    }

    if (_allSessionStage == SESSION_STAGE_WAIT_SESSION_DIR) {
      if (!mkdirOkOrExists) {
        failSessionSetup(mapAsyncResult(result, "AsyncSD mkdir session failed"), true);
        return;
      }

      if (_allSessionNeedsFreshName &&
          (result.code == AsyncSD::ErrorCode::AlreadyExists || result.code == AsyncSD::ErrorCode::IoError)) {
        if (_allSessionProbeCount < SESSION_PROBE_MAX) {
          _allSessionProbeCount = static_cast<uint16_t>(_allSessionProbeCount + 1U);
        }
        if (_allSessionProbeCount >= SESSION_PROBE_MAX || _allSessionSeq >= SESSION_PROBE_MAX) {
          failSessionSetup(Status(Err::RESOURCE_BUSY, 0, "session sequence exhausted"), true);
          return;
        }
        _allSessionSeq = static_cast<uint16_t>(_allSessionSeq + 1U);
        char candidatePath[HardwareSettings::SD_PATH_BYTES] = {0};
        char candidateName[sizeof(_allSessionDirName)] = {0};
        if (!buildAllSessionCandidate(_allSessionSeq,
                                      candidatePath,
                                      sizeof(candidatePath),
                                      candidateName,
                                      sizeof(candidateName))) {
          failSessionSetup(Status(Err::INVALID_CONFIG, 0, "session path format failed"), true);
          return;
        }
        strncpy(_allSessionDirName, candidateName, sizeof(_allSessionDirName) - 1);
        _allSessionDirName[sizeof(_allSessionDirName) - 1] = '\0';
        strncpy(_allSessionDirPath, candidatePath, sizeof(_allSessionDirPath) - 1);
        _allSessionDirPath[sizeof(_allSessionDirPath) - 1] = '\0';
        const AsyncSD::RequestId mkId = _sd.requestMkdir(_allSessionDirPath);
        if (mkId == AsyncSD::INVALID_REQUEST_ID) {
          const AsyncSD::ErrorInfo info = _sd.lastErrorInfo();
          failSessionSetup(Status(Err::RESOURCE_BUSY, info.detail, "AsyncSD mkdir session enqueue failed"), true);
          return;
        }
        _sessionRequestId = mkId;
        _allSessionStage = SESSION_STAGE_WAIT_SESSION_DIR;
        return;
      }

      if (_allFilePart == 0U) {
        _allFilePart = 1U;
      }
      if (_eventFilePart == 0U) {
        _eventFilePart = 1U;
      }
      if (!buildSessionAllFilePath(_allFilePart, _allCurrentFilePath, sizeof(_allCurrentFilePath))) {
        failSessionSetup(Status(Err::INVALID_CONFIG, 0, "all file path format failed"), true);
        return;
      }
      if (!buildSessionEventFilePath(_eventFilePart, _eventCurrentFilePath, sizeof(_eventCurrentFilePath))) {
        failSessionSetup(Status(Err::INVALID_CONFIG, 0, "event file path format failed"), true);
        return;
      }

      if (!_allSessionNeedsFreshName) {
        const AsyncSD::RequestId listId =
            _sd.requestListDir(_allSessionDirPath, _sessionDirEntries, SESSION_DIR_LIST_MAX);
        if (listId == AsyncSD::INVALID_REQUEST_ID) {
          const AsyncSD::ErrorInfo info = _sd.lastErrorInfo();
          failSessionSetup(Status(Err::RESOURCE_BUSY, info.detail, "AsyncSD listdir enqueue failed"), true);
          return;
        }
        _sessionRequestId = listId;
        _allSessionStage = SESSION_STAGE_WAIT_RESUME_LIST;
        return;
      }

      const AsyncSD::RequestId listId =
          _sd.requestListDir(_allSessionDirPath, _sessionDirEntries, SESSION_DIR_LIST_MAX);
      if (listId == AsyncSD::INVALID_REQUEST_ID) {
        const AsyncSD::ErrorInfo info = _sd.lastErrorInfo();
        failSessionSetup(Status(Err::RESOURCE_BUSY, info.detail, "AsyncSD verify list enqueue failed"), true);
        return;
      }
      _sessionRequestId = listId;
      _allSessionStage = SESSION_STAGE_WAIT_FRESH_VERIFY_LIST;
      return;
    }

    if (_allSessionStage == SESSION_STAGE_WAIT_FRESH_VERIFY_LIST) {
      if (result.code == AsyncSD::ErrorCode::NotFound) {
        const AsyncSD::RequestId mkId = _sd.requestMkdir(_allSessionDirPath);
        if (mkId == AsyncSD::INVALID_REQUEST_ID) {
          const AsyncSD::ErrorInfo info = _sd.lastErrorInfo();
          failSessionSetup(Status(Err::RESOURCE_BUSY, info.detail, "AsyncSD mkdir session enqueue failed"), true);
          return;
        }
        _allSessionStage = SESSION_STAGE_WAIT_SESSION_DIR;
        _sessionRequestId = mkId;
        return;
      }
      if (result.code != AsyncSD::ErrorCode::Ok) {
        setLastError(mapAsyncResult(result, "AsyncSD fresh-session verify failed"), nowMs);
        (void)queueMarkerCreate();
        return;
      }

      uint32_t entryCount = result.bytesProcessed;
      if (entryCount > SESSION_DIR_LIST_MAX) {
        entryCount = SESSION_DIR_LIST_MAX;
      }
      bool hasEntries = false;
      for (uint32_t i = 0; i < entryCount; ++i) {
        const AsyncSD::DirEntry& entry = _sessionDirEntries[i];
        if (entry.name[0] == '\0') {
          continue;
        }
        hasEntries = true;
        break;
      }
      if (!hasEntries) {
        (void)queueMarkerCreate();
        return;
      }

      if (_allSessionProbeCount < SESSION_PROBE_MAX) {
        _allSessionProbeCount = static_cast<uint16_t>(_allSessionProbeCount + 1U);
      }
      if (_allSessionProbeCount >= SESSION_PROBE_MAX || _allSessionSeq >= SESSION_PROBE_MAX) {
        failSessionSetup(Status(Err::RESOURCE_BUSY, 0, "session sequence exhausted"), true);
        return;
      }
      _allSessionSeq = static_cast<uint16_t>(_allSessionSeq + 1U);
      char candidatePath[HardwareSettings::SD_PATH_BYTES] = {0};
      char candidateName[sizeof(_allSessionDirName)] = {0};
      if (!buildAllSessionCandidate(_allSessionSeq,
                                    candidatePath,
                                    sizeof(candidatePath),
                                    candidateName,
                                    sizeof(candidateName))) {
        failSessionSetup(Status(Err::INVALID_CONFIG, 0, "session path format failed"), true);
        return;
      }
      strncpy(_allSessionDirName, candidateName, sizeof(_allSessionDirName) - 1);
      _allSessionDirName[sizeof(_allSessionDirName) - 1] = '\0';
      strncpy(_allSessionDirPath, candidatePath, sizeof(_allSessionDirPath) - 1);
      _allSessionDirPath[sizeof(_allSessionDirPath) - 1] = '\0';
      const AsyncSD::RequestId mkId = _sd.requestMkdir(_allSessionDirPath);
      if (mkId == AsyncSD::INVALID_REQUEST_ID) {
        const AsyncSD::ErrorInfo info = _sd.lastErrorInfo();
        failSessionSetup(Status(Err::RESOURCE_BUSY, info.detail, "AsyncSD mkdir session enqueue failed"), true);
        return;
      }
      _sessionRequestId = mkId;
      _allSessionStage = SESSION_STAGE_WAIT_SESSION_DIR;
      return;
    }

    if (_allSessionStage == SESSION_STAGE_WAIT_RESUME_LIST) {
      if (result.code == AsyncSD::ErrorCode::NotFound) {
        fallbackToFreshSession(false);
        return;
      }
      if (result.code != AsyncSD::ErrorCode::Ok) {
        // Resume list can fail on some media/adapter states after hot reinsert.
        // Fall back to a fresh session bootstrap instead of getting stuck.
        setLastError(mapAsyncResult(result, "AsyncSD listdir session failed"), nowMs);
        fallbackToFreshSession(true);
        return;
      }

      uint16_t maxDataPart = 0U;
      uint16_t maxEventPart = 0U;
      bool markerFound = false;
      uint32_t entryCount = result.bytesProcessed;
      if (entryCount > SESSION_DIR_LIST_MAX) {
        entryCount = SESSION_DIR_LIST_MAX;
      }
      for (uint32_t i = 0; i < entryCount; ++i) {
        const AsyncSD::DirEntry& entry = _sessionDirEntries[i];
        if (_allSessionMarkerName[0] != '\0' &&
            strncmp(entry.name, _allSessionMarkerName, sizeof(entry.name)) == 0) {
          markerFound = !entry.isDir;
          continue;
        }
        if (entry.isDir) {
          continue;
        }
        uint16_t parsedPart = 0U;
        if (parsePartFromFileName(entry.name, SESSION_DATA_PREFIX, parsedPart) ||
            parsePartFromFileName(entry.name, SESSION_DATA_PREFIX_COMPAT, parsedPart)) {
          if (parsedPart > maxDataPart) {
            maxDataPart = parsedPart;
          }
          continue;
        }
        if (parsePartFromFileName(entry.name, "events_", parsedPart)) {
          if (parsedPart > maxEventPart) {
            maxEventPart = parsedPart;
          }
        }
      }

      if (_allSessionMarkerName[0] == '\0' || !markerFound) {
        fallbackToFreshSession(false);
        return;
      }

      const uint16_t previousDataPart = _allFilePart;
      const uint16_t previousEventPart = _eventFilePart;

      if (maxDataPart > 0U) {
        _allFilePart = maxDataPart;
      } else if (_allFilePart == 0U) {
        _allFilePart = 1U;
      }
      if (maxEventPart > 0U) {
        _eventFilePart = maxEventPart;
      } else if (_eventFilePart == 0U) {
        _eventFilePart = 1U;
      }

      if (!buildSessionAllFilePath(_allFilePart, _allCurrentFilePath, sizeof(_allCurrentFilePath))) {
        failSessionSetup(Status(Err::INVALID_CONFIG, 0, "all file path format failed"), true);
        return;
      }
      if (!buildSessionEventFilePath(_eventFilePart, _eventCurrentFilePath, sizeof(_eventCurrentFilePath))) {
        failSessionSetup(Status(Err::INVALID_CONFIG, 0, "event file path format failed"), true);
        return;
      }
      if (_allFilePart != previousDataPart) {
        _sampleLinesCurrentFile = 0U;
      }
      if (_eventFilePart != previousEventPart) {
        _eventLinesCurrentFile = 0U;
      }

      const AsyncSD::RequestId statId = _sd.requestStat(_allCurrentFilePath);
      if (statId == AsyncSD::INVALID_REQUEST_ID) {
        const AsyncSD::ErrorInfo info = _sd.lastErrorInfo();
        failSessionSetup(Status(Err::RESOURCE_BUSY, info.detail, "AsyncSD stat data enqueue failed"), true);
        return;
      }
      _sessionRequestId = statId;
      _allSessionStage = SESSION_STAGE_WAIT_RESUME_STAT_DATA;
      return;
    }

    if (_allSessionStage == SESSION_STAGE_WAIT_RESUME_STAT_DATA) {
      if (result.code == AsyncSD::ErrorCode::Ok) {
        if (result.stat.isDir) {
          fallbackToFreshSession(false);
          return;
        }
        const uint64_t size = result.stat.size;
        if (hasSuspiciousCsvLength(size, strlen(CSV_HEADER))) {
          if (_allFilePart >= LOG_FILE_PART_MAX) {
            failSessionSetup(Status(Err::RESOURCE_BUSY, 0, "session data file invalid"), true);
            return;
          }
          _allFilePart = static_cast<uint16_t>(_allFilePart + 1U);
          if (!buildSessionAllFilePath(_allFilePart, _allCurrentFilePath, sizeof(_allCurrentFilePath))) {
            failSessionSetup(Status(Err::INVALID_CONFIG, 0, "all file path format failed"), true);
            return;
          }
          _allBytes = 0U;
          _allSizeKnown = true;
          _sampleLinesCurrentFile = 0U;
        } else {
          _allBytes = (size > 0xFFFFFFFFULL) ? 0xFFFFFFFFUL : static_cast<uint32_t>(size);
          _allSizeKnown = true;
        }
      } else if (result.code == AsyncSD::ErrorCode::NotFound || result.code == AsyncSD::ErrorCode::IoError) {
        _allBytes = 0U;
        _allSizeKnown = true;
        _sampleLinesCurrentFile = 0U;
      } else {
        failSessionSetup(mapAsyncResult(result, "AsyncSD stat data failed"), true);
        return;
      }

      const AsyncSD::RequestId statId = _sd.requestStat(_eventCurrentFilePath);
      if (statId == AsyncSD::INVALID_REQUEST_ID) {
        const AsyncSD::ErrorInfo info = _sd.lastErrorInfo();
        failSessionSetup(Status(Err::RESOURCE_BUSY, info.detail, "AsyncSD stat events enqueue failed"), true);
        return;
      }
      _sessionRequestId = statId;
      _allSessionStage = SESSION_STAGE_WAIT_RESUME_STAT_EVENTS;
      return;
    }

    if (_allSessionStage == SESSION_STAGE_WAIT_RESUME_STAT_EVENTS) {
      if (result.code == AsyncSD::ErrorCode::Ok) {
        if (result.stat.isDir) {
          fallbackToFreshSession(false);
          return;
        }
        if (hasSuspiciousCsvLength(result.stat.size, strlen(EVENTS_HEADER))) {
          if (_eventFilePart >= LOG_FILE_PART_MAX) {
            failSessionSetup(Status(Err::RESOURCE_BUSY, 0, "session event file invalid"), true);
            return;
          }
          _eventFilePart = static_cast<uint16_t>(_eventFilePart + 1U);
          if (!buildSessionEventFilePath(_eventFilePart, _eventCurrentFilePath, sizeof(_eventCurrentFilePath))) {
            failSessionSetup(Status(Err::INVALID_CONFIG, 0, "event file path format failed"), true);
            return;
          }
          _eventLinesCurrentFile = 0U;
        }
      } else if (result.code == AsyncSD::ErrorCode::NotFound || result.code == AsyncSD::ErrorCode::IoError) {
        _eventLinesCurrentFile = 0U;
      } else {
        failSessionSetup(mapAsyncResult(result, "AsyncSD stat events failed"), true);
        return;
      }
      markSessionReady();
      return;
    }

    if (_allSessionStage == SESSION_STAGE_WAIT_MARKER_CREATE_OPEN) {
      if (result.code != AsyncSD::ErrorCode::Ok || result.handle == AsyncSD::INVALID_FILE_HANDLE) {
        failSessionSetup(mapAsyncResult(result, "AsyncSD open marker failed"), true);
        return;
      }
      _sessionSetupHandle = result.handle;
      const AsyncSD::RequestId closeId = _sd.requestClose(_sessionSetupHandle);
      if (closeId == AsyncSD::INVALID_REQUEST_ID) {
        const AsyncSD::ErrorInfo info = _sd.lastErrorInfo();
        if (_sdStarted) {
          _deferredTeardown = true;
          _sdStarted = false;
        }
        _mounted = false;
        _mountStage = MountStage::IDLE;
        _mountRequestId = AsyncSD::INVALID_REQUEST_ID;
        _infoRequestId = AsyncSD::INVALID_REQUEST_ID;
        failSessionSetup(Status(Err::RESOURCE_BUSY, info.detail, "AsyncSD close marker enqueue failed"), true);
        return;
      }
      _sessionRequestId = closeId;
      _allSessionStage = SESSION_STAGE_WAIT_MARKER_CREATE_CLOSE;
      return;
    }

    if (_allSessionStage == SESSION_STAGE_WAIT_MARKER_CREATE_CLOSE) {
      _sessionSetupHandle = AsyncSD::INVALID_FILE_HANDLE;
      if (result.code != AsyncSD::ErrorCode::Ok) {
        if (_sdStarted) {
          _deferredTeardown = true;
          _sdStarted = false;
        }
        _mounted = false;
        _mountStage = MountStage::IDLE;
        _mountRequestId = AsyncSD::INVALID_REQUEST_ID;
        _infoRequestId = AsyncSD::INVALID_REQUEST_ID;
        failSessionSetup(mapAsyncResult(result, "AsyncSD close marker failed"), true);
        return;
      }
      markSessionReady();
      return;
    }

    _allSessionStage = SESSION_STAGE_IDLE;
    _sessionRequestId = AsyncSD::INVALID_REQUEST_ID;
    _sessionSetupHandle = AsyncSD::INVALID_FILE_HANDLE;
    return;
  }
  if (_job.active && result.id == _job.requestId) {
    handleFileJobResult(result, nowMs);
  }
}
#endif

Status SdLogger::mount(uint32_t nowMs) {
  if (!_enabled) {
    const Status st(Err::NOT_INITIALIZED, 0, "SD disabled");
    setLastError(st, nowMs);
    return st;
  }

  _lastMountAttemptMs = (nowMs == 0U) ? 1U : nowMs;

  if (!isCardPresent()) {
    _mounted = false;
#if TFLUNACTRL_HAS_ASYNC_SD
    _job = FileJob{};
    _mountStage = MountStage::IDLE;
    _mountRequestId = AsyncSD::INVALID_REQUEST_ID;
    _infoRequestId = AsyncSD::INVALID_REQUEST_ID;
    _sessionRequestId = AsyncSD::INVALID_REQUEST_ID;
    if (_sdStarted) {
      _sd.end();
      _sdStarted = false;
    }
#endif
    const Status st(Err::COMM_FAILURE, 0, "SD card not present");
    setLastError(st, nowMs);
    return st;
  }
  if (_config.sdCs < 0) {
    _mounted = false;
    const Status st(Err::INVALID_CONFIG, 0, "sdCs not set");
    setLastError(st, nowMs);
    return st;
  }

#if TFLUNACTRL_HAS_ASYNC_SD
  if (!setupAsyncSd(nowMs)) {
    _mounted = false;
    return _lastError;
  }
  _allSessionReady = false;
  _allSessionStage = SESSION_STAGE_IDLE;
  _mounted = false;
  _mountStage = MountStage::IDLE;
  _mountRequestId = AsyncSD::INVALID_REQUEST_ID;
  _infoRequestId = AsyncSD::INVALID_REQUEST_ID;
  _sessionRequestId = AsyncSD::INVALID_REQUEST_ID;
  _job = FileJob{};
  invalidateInfoCache();
  if (!enqueueMountRequest(nowMs)) {
    return _lastError;
  }
  return Ok();
#elif defined(ARDUINO)
  _mounted = false;
  const Status st(Err::NOT_INITIALIZED, 0, "AsyncSD library unavailable");
  setLastError(st, nowMs);
  return st;
#else
  _mounted = false;
  const Status st(Err::NOT_INITIALIZED, 0, "SD not available");
  setLastError(st, nowMs);
  return st;
#endif
}

Status SdLogger::remount(uint32_t nowMs) {
  const bool preserveSessionIdentity =
      _settings.logAllEnabled &&
      _allSessionDirPath[0] != '\0' &&
      _allSessionMarkerName[0] != '\0';
  _mounted = false;
  _allCapped = false;
  _allSizeKnown = false;
  resetAllSessionState(!preserveSessionIdentity);
  _allSessionNeedsFreshName = _settings.logAllEnabled ? !preserveSessionIdentity : false;
  invalidateInfoCache();
#if TFLUNACTRL_HAS_ASYNC_SD
  _job = FileJob{};
  _mountStage = MountStage::IDLE;
  _mountRequestId = AsyncSD::INVALID_REQUEST_ID;
  _infoRequestId = AsyncSD::INVALID_REQUEST_ID;
  _sessionRequestId = AsyncSD::INVALID_REQUEST_ID;
  _sessionSetupHandle = AsyncSD::INVALID_FILE_HANDLE;
  // Defer remount to processDeferred() and force a clean teardown/restart.
  // State cleanup above is immediate so tick() sees unmounted right away.
  _deferredRemount = true;
  _deferredMount = false;
  _lastMountAttemptMs = 0U;
  return Ok();
#else
  return mount(nowMs);
#endif
}

Status SdLogger::probe(uint32_t nowMs) {
  if (nowMs == 0U) {
    nowMs = SystemClock::nowMs();
  }
  if (!_enabled) {
    return Status(Err::NOT_INITIALIZED, 0, "SD disabled");
  }
  if (!isCardPresent()) {
    const Status st(Err::COMM_FAILURE, 0, "SD card not present");
    markUnmounted(nowMs, st);
    return st;
  }
  if (!_mounted) {
    return _lastError.ok() ? Status(Err::COMM_FAILURE, 0, "SD not mounted") : _lastError;
  }
#if TFLUNACTRL_HAS_ASYNC_SD
  if (!_sdStarted) {
    return Status(Err::NOT_INITIALIZED, 0, "AsyncSD not started");
  }
  const AsyncSD::SdStatus sdState = _sd.status();
  if (sdState == AsyncSD::SdStatus::NoCard ||
      sdState == AsyncSD::SdStatus::Removed ||
      sdState == AsyncSD::SdStatus::Fault) {
    const AsyncSD::ErrorInfo info = _sd.lastErrorInfo();
    AsyncSD::RequestResult stateResult{};
    stateResult.code = info.code;
    stateResult.detail = info.detail;
    Status stateError = mapAsyncResult(stateResult, asyncSdStateMessage(sdState));
    if (stateError.ok()) {
      stateError = Status(Err::COMM_FAILURE, info.detail, asyncSdStateMessage(sdState));
    }
    markUnmounted(nowMs, stateError);
    return stateError;
  }
  if (_mountStage != MountStage::READY) {
    return Status(Err::RESOURCE_BUSY, 0, "SD probe busy");
  }
  if (_infoRequestId == AsyncSD::INVALID_REQUEST_ID) {
    if (!enqueueInfoRequest(nowMs)) {
      return _lastError.ok() ? Status(Err::RESOURCE_BUSY, 0, "SD probe enqueue failed") : _lastError;
    }
  }
  return Ok();
#else
  return Status(Err::NOT_INITIALIZED, 0, "AsyncSD library unavailable");
#endif
}

Status SdLogger::logSample(const Sample& sample, uint32_t nowMs) {
  if (!_enabled) {
    return Status(Err::NOT_INITIALIZED, 0, "SD disabled");
  }
  if (isFatFs()) {
    return Status(Err::INVALID_CONFIG, 0, "FAT32 not supported, format exFAT");
  }
  const bool wantDaily = _settings.logDailyEnabled;
  const bool wantAll = _settings.logAllEnabled && !_allCapped;
  if (!wantDaily && !wantAll) {
    return Ok();
  }
  if (_queue == nullptr || _queueCapacity == 0U) {
    const Status st(Err::OUT_OF_MEMORY, 0, "log queue unavailable");
    setLastError(st, nowMs);
    return st;
  }
  if (_count >= _queueCapacity) {
    _dropped++;
    const Status st(Err::RESOURCE_BUSY, 0, "log queue full");
    setLastError(st, nowMs);
    return st;
  }

  LogRecord& rec = _queue[_head];
  rec.valid = true;
  rec.pendingDaily = wantDaily;
  rec.pendingAll = wantAll;
  rec.dailyRetries = 0;
  rec.allRetries = 0;
  rec.nextAttemptMs = 0;

  char dayKey[11] = {0};
  if (sample.tsLocal[0] != '\0') {
    memcpy(dayKey, sample.tsLocal, 10);
    dayKey[10] = '\0';
  } else {
    strncpy(dayKey, UNKNOWN_DAY, sizeof(dayKey) - 1);
  }
  strncpy(rec.dayKey, dayKey, sizeof(rec.dayKey) - 1);
  rec.dayKey[sizeof(rec.dayKey) - 1] = '\0';

  const int written = snprintf(rec.line,
                               sizeof(rec.line),
                               "%s,%lu,%lu,%u,%u,%.2f,%u,%u,%.2f,%.2f,%.2f\n",
                               (sample.tsLocal[0] != '\0') ? sample.tsLocal : "uptime",
                               static_cast<unsigned long>(sample.uptimeMs),
                               static_cast<unsigned long>(sample.sampleIndex),
                               static_cast<unsigned int>(sample.distanceCm),
                               static_cast<unsigned int>(sample.strength),
                               static_cast<double>(sample.lidarTempC),
                               sample.validFrame ? 1U : 0U,
                               sample.signalOk ? 1U : 0U,
                               static_cast<double>(sample.tempC),
                               static_cast<double>(sample.rhPct),
                               static_cast<double>(sample.pressureHpa));
  if (written <= 0 || written >= static_cast<int>(sizeof(rec.line))) {
    _dropped++;
    rec.valid = false;
    const Status st(Err::INTERNAL_ERROR, 0, "log line format error");
    setLastError(st, nowMs);
    return st;
  }

  _head = (_head + 1) % _queueCapacity;
  _count++;
  return Ok();
}

Status SdLogger::logEvent(const Event& event, uint32_t nowMs) {
  if (!_enabled) {
    return Status(Err::NOT_INITIALIZED, 0, "SD disabled");
  }
  if (isFatFs()) {
    return Status(Err::INVALID_CONFIG, 0, "FAT32 not supported, format exFAT");
  }
  if (_eventQueue == nullptr || _eventQueueCapacity == 0U) {
    const Status st(Err::OUT_OF_MEMORY, 0, "event queue unavailable");
    setLastError(st, nowMs);
    return st;
  }
  if (_eventCount >= _eventQueueCapacity) {
    _eventDropped++;
    const Status st(Err::RESOURCE_BUSY, 0, "event queue full");
    setLastError(st, nowMs);
    return st;
  }

  EventRecord& rec = _eventQueue[_eventHead];
  rec.valid = true;
  rec.retries = 0;
  rec.nextAttemptMs = 0;

  const int written = snprintf(rec.line,
                               sizeof(rec.line),
                               "%lu,%s,%u,%s\n",
                               static_cast<unsigned long>(event.tsUnix),
                               event.tsLocal,
                               static_cast<unsigned int>(event.code),
                               event.msg);
  if (written <= 0 || written >= static_cast<int>(sizeof(rec.line))) {
    _eventDropped++;
    rec.valid = false;
    const Status st(Err::INTERNAL_ERROR, 0, "event line format error");
    setLastError(st, nowMs);
    return st;
  }

  _eventHead = (_eventHead + 1) % _eventQueueCapacity;
  _eventCount++;
  return Ok();
}

void SdLogger::tick(uint32_t nowMs) {
  if (!_enabled) {
    return;
  }

  const uint32_t tickStartMs = SystemClock::nowMs();

  auto elapsedMs = [tickStartMs]() -> uint32_t {
    return SystemClock::nowMs() - tickStartMs;
  };

  auto finalizeTick = [this, &elapsedMs]() {
    _lastTickElapsedMs = elapsedMs();
    if (_lastTickElapsedMs > _settings.logIoBudgetMs) {
      _budgetExceededCount++;
    }
  };

  auto budgetExceeded = [this, &elapsedMs]() -> bool {
    return elapsedMs() >= _settings.logIoBudgetMs;
  };

  if (!_settings.logDailyEnabled) {
    _dailyOk = true;
  }
  if (!_settings.logAllEnabled) {
    _allOk = true;
  } else if (_allCapped) {
    _allOk = false;
  }

  if (_mounted && !isCardPresent()) {
    markUnmounted(nowMs, Status(Err::COMM_FAILURE, 0, "SD card removed"));
  }

#if TFLUNACTRL_HAS_ASYNC_SD
  if (_sdStarted) {
    _sd.poll();
    AsyncSD::RequestResult result{};
    size_t drainedResults = 0U;
    while (_sd.popResult(&result)) {
      handleAsyncResult(result, nowMs);
      ++drainedResults;
      if (drainedResults >= SD_MAX_RESULTS_PER_TICK || budgetExceeded()) {
        break;
      }
    }
    const AsyncSD::SdStatus sdState = _sd.status();
    if (_mounted &&
        (sdState == AsyncSD::SdStatus::NoCard ||
         sdState == AsyncSD::SdStatus::Removed ||
         sdState == AsyncSD::SdStatus::Fault)) {
      const AsyncSD::ErrorInfo info = _sd.lastErrorInfo();
      AsyncSD::RequestResult stateResult{};
      stateResult.code = info.code;
      stateResult.detail = info.detail;
      Status stateError = mapAsyncResult(stateResult, asyncSdStateMessage(sdState));
      if (stateError.ok()) {
        stateError = Status(Err::COMM_FAILURE, info.detail, asyncSdStateMessage(sdState));
      }
      markUnmounted(nowMs, stateError);
    }
    // When not mounted but library reports card fault, force full
    // re-init on the next mount attempt so _sd.end()+_sd.begin()
    // resets the SPI/worker state cleanly.
    if (!_mounted &&
        (sdState == AsyncSD::SdStatus::NoCard ||
         sdState == AsyncSD::SdStatus::Removed ||
         sdState == AsyncSD::SdStatus::Fault)) {
      _deferredTeardown = true;
      _sdStarted = false;
    }
    // --- File job deadline ---
    if (_job.active && _job.startMs != 0U &&
        (nowMs - _job.startMs) >= SD_FILE_JOB_TIMEOUT_MS) {
      failFileJob(Status(Err::TIMEOUT, 0, "SD file job timeout"), nowMs);
    }
    if (_infoRequestId != AsyncSD::INVALID_REQUEST_ID &&
        _lastInfoRequestMs != 0U &&
        static_cast<int32_t>(nowMs - _lastInfoRequestMs) >=
            static_cast<int32_t>(SD_INFO_REQUEST_TIMEOUT_MS)) {
      if (_infoTimeoutStreak < 0xFFU) {
        _infoTimeoutStreak = static_cast<uint8_t>(_infoTimeoutStreak + 1U);
      }
      if (_infoTimeoutStreak > SD_INFO_TIMEOUT_STREAK_LIMIT) {
        _infoTimeoutStreak = SD_INFO_TIMEOUT_STREAK_LIMIT;
      }
      _sdInfoValid = false;
      _sdUsageValid = false;
      const Status timeoutSt(Err::TIMEOUT, static_cast<int32_t>(_infoTimeoutStreak), "AsyncSD info timeout");
      setLastError(timeoutSt, nowMs);
      _infoRequestId = AsyncSD::INVALID_REQUEST_ID;
    }
    if (_mounted && _mountStage == MountStage::READY && _infoRequestId == AsyncSD::INVALID_REQUEST_ID) {
      // Defer info requests while a file job is executing so the
      // potentially long FAT32 free-cluster scan does not block writes.
      if (!_job.active) {
        // Exponential backoff after consecutive info timeouts:
        // base * 2^min(streak,3) -> 30 s / 60 s / 120 s / 240 s max
        const uint32_t backoffMult = 1U << (_infoTimeoutStreak > 3U ? 3U : _infoTimeoutStreak);
        const uint32_t refreshMs = _sdInfoValid
            ? SD_INFO_REFRESH_MS
            : (SD_INFO_RETRY_BASE_MS * backoffMult);
        const bool due = (_lastInfoRequestMs == 0U) ||
                         (static_cast<int32_t>(nowMs - _lastInfoRequestMs) >=
                          static_cast<int32_t>(refreshMs));
        if (due) {
          (void)enqueueInfoRequest(nowMs);
        }
      }
    }
  }
#endif

  if (!_mounted) {
#if TFLUNACTRL_HAS_ASYNC_SD
    if (_mountStage != MountStage::IDLE) {
      const int32_t mountAge = static_cast<int32_t>(nowMs - _lastMountAttemptMs);
      if (_lastMountAttemptMs != 0U && mountAge >= 0 &&
          static_cast<uint32_t>(mountAge) < _settings.logMountRetryMs) {
        finalizeTick();
        return;
      }
      _mountStage = MountStage::IDLE;
      _mountRequestId = AsyncSD::INVALID_REQUEST_ID;
    }
#endif
    // Mount retry policy:
    // - With CD pin + card present: fast retries for quick re-insert recovery.
    // - With CD pin + card absent: configured retry interval.
    // - Without CD pin: fixed configured retry interval for predictable probing.
    const bool cardDetectAvailable = (_config.sdCdPin >= 0);
    const bool cardPresent = isCardPresent();
    const uint32_t backoffShift = (_mountCycleCount > 4U) ? 4U : _mountCycleCount;
    const uint32_t fastMs = SD_FAST_MOUNT_RETRY_MS * (1U << backoffShift);
    uint32_t retryMs = _settings.logMountRetryMs;
    if (cardDetectAvailable && cardPresent) {
      retryMs = (fastMs < _settings.logMountRetryMs) ? fastMs : _settings.logMountRetryMs;
    }
    if (_lastMountAttemptMs == 0 || (nowMs - _lastMountAttemptMs) >= retryMs) {
      _lastMountAttemptMs = nowMs;
#if TFLUNACTRL_HAS_ASYNC_SD
      // Defer the actual mount to processDeferred() so the blocking
      // _sd.end() + _sd.begin() calls do not stall cooperative tick.
      _deferredMount = true;
#else
      mount(nowMs);
#endif
    }
    finalizeTick();
    return;
  }

  if (_settings.logAllEnabled && !_allSessionReady) {
    (void)ensureAllSessionReady(nowMs);
  }

  if (_count == 0 && _eventCount == 0) {
    finalizeTick();
    return;
  }

  const bool flushDue = shouldAttemptFlush(nowMs, _lastFlushMs, _settings.logFlushMs);
  bool flushBypassForBacklog = (_count > 1U) || (_eventCount > 0U);
  if (!flushBypassForBacklog && _count > 0U) {
    const LogRecord& head = _queue[_tail];
    flushBypassForBacklog = head.valid && head.pendingDaily && head.pendingAll;
  }
  if (!flushDue && !flushBypassForBacklog) {
    finalizeTick();
    return;
  }

#if TFLUNACTRL_HAS_ASYNC_SD
  if (budgetExceeded()) {
    finalizeTick();
    return;
  }
  if (_mountStage != MountStage::READY || _job.active) {
    finalizeTick();
    return;
  }

  if (_count > 0) {
    LogRecord& rec = _queue[_tail];
    if (!rec.valid) {
      popSampleHead();
    } else if (rec.nextAttemptMs == 0 || static_cast<int32_t>(nowMs - rec.nextAttemptMs) >= 0) {
      if (rec.pendingDaily) {
        if (!_settings.logDailyEnabled) {
          rec.pendingDaily = false;
          _dailyOk = true;
        } else {
          uint8_t batchCount = 1U;
          while (batchCount < FileJob::MAX_BATCH_LINES &&
                 static_cast<size_t>(batchCount) < _count) {
            const size_t idx = (_tail + static_cast<size_t>(batchCount)) % _queueCapacity;
            const LogRecord& candidate = _queue[idx];
            if (!candidate.valid || !candidate.pendingDaily) {
              break;
            }
            if (strncmp(candidate.dayKey, rec.dayKey, sizeof(rec.dayKey)) != 0) {
              break;
            }
            if (candidate.nextAttemptMs != 0U &&
                static_cast<int32_t>(nowMs - candidate.nextAttemptMs) < 0) {
              break;
            }
            batchCount = static_cast<uint8_t>(batchCount + 1U);
          }
          char dailyPath[HardwareSettings::SD_PATH_BYTES];
          if (!buildDailySampleFilePath(rec.dayKey, dailyPath, sizeof(dailyPath))) {
            setLastError(Status(Err::INVALID_CONFIG, 0, "daily file path overflow"), nowMs);
            _dailyOk = false;
            finalizeTick();
            return;
          }
          (void)startFileJob(JobType::SAMPLE_DAILY,
                             _tail,
                             batchCount,
                             dailyPath,
                             CSV_HEADER,
                             0,
                             false,
                             nowMs);
          finalizeTick();
          return;
        }
      }

      if (rec.pendingAll) {
        if (!_settings.logAllEnabled) {
          rec.pendingAll = false;
          _allOk = true;
        } else if (_allCapped) {
          rec.pendingAll = false;
          _allOk = false;
        } else {
          uint8_t batchCount = 1U;
          while (batchCount < FileJob::MAX_BATCH_LINES &&
                 static_cast<size_t>(batchCount) < _count) {
            const size_t idx = (_tail + static_cast<size_t>(batchCount)) % _queueCapacity;
            const LogRecord& candidate = _queue[idx];
            if (!candidate.valid || !candidate.pendingAll) {
              break;
            }
            if (candidate.nextAttemptMs != 0U &&
                static_cast<int32_t>(nowMs - candidate.nextAttemptMs) < 0) {
              break;
            }
            batchCount = static_cast<uint8_t>(batchCount + 1U);
          }
          if (!ensureAllSessionReady(nowMs)) {
            _allOk = false;
            finalizeTick();
            return;
          }
          (void)startFileJob(JobType::SAMPLE_ALL,
                             _tail,
                             batchCount,
                             _allCurrentFilePath,
                             CSV_HEADER,
                             _settings.logAllMaxBytes,
                             false,
                             nowMs);
          finalizeTick();
          return;
        }
      }

      if (!rec.pendingDaily && !rec.pendingAll) {
        popSampleHead();
      }
    }
  }

  if (_eventCount > 0 && !budgetExceeded()) {
    EventRecord& rec = _eventQueue[_eventTail];
    if (!rec.valid) {
      popEventHead();
    } else if (rec.nextAttemptMs == 0 || static_cast<int32_t>(nowMs - rec.nextAttemptMs) >= 0) {
      if (_eventCurrentFilePath[0] == '\0' &&
          !buildSessionEventFilePath(_eventFilePart, _eventCurrentFilePath, sizeof(_eventCurrentFilePath))) {
        setLastError(Status(Err::INVALID_CONFIG, 0, "event file path format failed"), nowMs);
        finalizeTick();
        return;
      }
      uint8_t batchCount = 1U;
      while (batchCount < FileJob::MAX_BATCH_LINES &&
             static_cast<size_t>(batchCount) < _eventCount) {
        const size_t idx = (_eventTail + static_cast<size_t>(batchCount)) % _eventQueueCapacity;
        const EventRecord& candidate = _eventQueue[idx];
        if (!candidate.valid) {
          break;
        }
        if (candidate.nextAttemptMs != 0U &&
            static_cast<int32_t>(nowMs - candidate.nextAttemptMs) < 0) {
          break;
        }
        batchCount = static_cast<uint8_t>(batchCount + 1U);
      }
      (void)startFileJob(JobType::EVENT,
                         _eventTail,
                         batchCount,
                         _eventCurrentFilePath,
                         EVENTS_HEADER,
                         _settings.logEventsMaxBytes,
                         false,
                         nowMs);
    }
  }
#else
  (void)nowMs;
#endif

  finalizeTick();
}

void SdLogger::processDeferred(uint32_t nowMs) {
#if TFLUNACTRL_HAS_ASYNC_SD
  // Phase 1: teardown - markUnmounted() defers _sd.end() to avoid blocking
  // tick.  Execute it here and yield so the FreeRTOS worker task has a full
  // loop cycle to release SPI resources before _sd.begin() re-inits the bus.
  if (_deferredTeardown) {
    _deferredTeardown = false;
    _sd.end();
    _sdStarted = false;
    // Do not process mount/remount on the same cycle.
    return;
  }

  // Remount supersedes a pending mount - force teardown, then mount fresh.
  if (_deferredRemount) {
    _deferredRemount = false;
    _deferredMount = true;
    _lastMountAttemptMs = 0U;
    if (_sdStarted) {
      _sd.end();
      _sdStarted = false;
      return;
    }
  }

  if (_deferredMount) {
    _deferredMount = false;
    // Always remount from a clean AsyncSD session. If currently started,
    // tear down now and mount on the next deferred cycle.
    if (_sdStarted) {
      _sd.end();
      _sdStarted = false;
      _deferredMount = true;
      return;
    }
    (void)mount(nowMs);
    return;
  }
#else
  (void)nowMs;
#endif
}

}  // namespace TFLunaControl

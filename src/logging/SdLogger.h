#pragma once

#include <stddef.h>
#include <stdint.h>

#include "CO2Control/AppSettings.h"
#include "CO2Control/HardwareSettings.h"
#include "CO2Control/RuntimeSettings.h"
#include "CO2Control/Status.h"
#include "CO2Control/Types.h"

#if defined(ARDUINO) && __has_include(<AsyncSD/AsyncSD.h>)
#include <AsyncSD/AsyncSD.h>
#define CO2CONTROL_HAS_ASYNC_SD 1
#else
#define CO2CONTROL_HAS_ASYNC_SD 0
#endif

namespace CO2Control {

/**
 * @brief SD logging for samples and events.
 */
class SdLogger {
 public:
  Status begin(const HardwareSettings& config,
               const AppSettings& appSettings,
               const RuntimeSettings& settings);
  void applySettings(const RuntimeSettings& settings);
  void end();

  void tick(uint32_t nowMs);

  /// @brief Execute deferred blocking SD operations (mount/remount).
  /// Called from the main loop outside tick timing measurement.
  void processDeferred(uint32_t nowMs);

  Status logSample(const Sample& sample, uint32_t nowMs);
  Status logEvent(const Event& event, uint32_t nowMs);

  bool isMounted() const { return _mounted; }
  bool infoValid() const { return _sdInfoValid; }
  bool usageValid() const { return _sdUsageValid; }
  uint64_t fsCapacityBytes() const { return _sdFsCapacityBytes; }
  uint64_t fsUsedBytes() const { return _sdFsUsedBytes; }
  uint64_t fsFreeBytes() const { return _sdFsFreeBytes; }
  uint64_t cardCapacityBytes() const { return _sdCardCapacityBytes; }
  uint32_t infoLastUpdateMs() const { return _sdInfoLastUpdateMs; }
  uint8_t fsTypeCode() const { return _sdFsTypeCode; }
  uint8_t cardTypeCode() const { return _sdCardTypeCode; }
  /// @brief Returns true when a FAT12/16/32 filesystem is detected (not exFAT).
  bool isFatFs() const { return _sdFsTypeCode >= 1 && _sdFsTypeCode <= 3; }
  bool dailyOk() const { return _dailyOk; }
  bool allOk() const { return _allOk; }
  uint32_t lastWriteMs() const { return _lastWriteMs; }
  uint32_t droppedCount() const { return _dropped; }
  size_t queueDepth() const { return _count; }
  size_t queueCapacity() const { return _queueCapacity; }
  bool queueUsingPsram() const { return _queueUsingPsram; }
  uint32_t eventDroppedCount() const { return _eventDropped; }
  size_t eventQueueDepth() const { return _eventCount; }
  size_t eventQueueCapacity() const { return _eventQueueCapacity; }
  bool eventQueueUsingPsram() const { return _eventQueueUsingPsram; }
  uint32_t lastErrorMs() const { return _lastErrorMs; }
  const Status& lastError() const { return _lastError; }
  uint32_t ioBudgetMs() const { return _settings.logIoBudgetMs; }
  uint32_t lastTickElapsedMs() const { return _lastTickElapsedMs; }
  uint32_t budgetExceededCount() const { return _budgetExceededCount; }
  bool sessionActive() const { return _allSessionReady; }
  const char* sessionDirPath() const { return _allSessionDirPath; }
  const char* currentAllFilePath() const { return _allCurrentFilePath; }
  const char* currentEventFilePath() const { return _eventCurrentFilePath; }
  uint16_t currentAllFilePart() const { return _allFilePart; }
  uint16_t currentEventFilePart() const { return _eventFilePart; }
  uint32_t sampleWriteSuccessCount() const { return _sampleWriteSuccessCount; }
  uint32_t sampleWriteFailureCount() const { return _sampleWriteFailureCount; }
  uint32_t eventWriteSuccessCount() const { return _eventWriteSuccessCount; }
  uint32_t eventWriteFailureCount() const { return _eventWriteFailureCount; }
  uint32_t sampleRotateCount() const { return _sampleRotateCount; }
  uint32_t eventRotateCount() const { return _eventRotateCount; }
  uint32_t sampleLinesCurrentFile() const { return _sampleLinesCurrentFile; }
  uint32_t eventLinesCurrentFile() const { return _eventLinesCurrentFile; }
  uint32_t sampleLinesTotal() const { return _sampleLinesTotal; }
  uint32_t eventLinesTotal() const { return _eventLinesTotal; }

  Status remount(uint32_t nowMs = 0);
  Status probe(uint32_t nowMs = 0);

  /// @brief Check if log I/O attempt is due based on flush interval.
  /// @note Exposed for deterministic host-side tests.
  static bool shouldAttemptFlush(uint32_t nowMs, uint32_t lastFlushMs, uint32_t flushIntervalMs);

 private:
  struct LogRecord {
    char line[HardwareSettings::SD_SAMPLE_LINE_BYTES];
    char dayKey[11];
    bool valid = false;
    bool pendingDaily = false;
    bool pendingAll = false;
    uint8_t dailyRetries = 0;
    uint8_t allRetries = 0;
    uint32_t nextAttemptMs = 0;
  };

  struct EventRecord {
    char line[HardwareSettings::SD_EVENT_LINE_BYTES];
    bool valid = false;
    uint8_t retries = 0;
    uint32_t nextAttemptMs = 0;
  };

  Status mount(uint32_t nowMs);
  Status allocateQueues(bool preferPsram);
  void releaseQueues();
  bool isCardPresent() const;
  void invalidateInfoCache();
  void markUnmounted(uint32_t nowMs, const Status& cause);
  void setLastError(const Status& error, uint32_t nowMs);
  void popSampleHead();
  void popEventHead();
  bool buildAllSessionCandidate(uint16_t seq, char* outPath, size_t outLen, char* outName, size_t outNameLen);
  bool buildSessionAllFilePath(uint16_t part, char* outPath, size_t outLen);
  bool buildSessionEventFilePath(uint16_t part, char* outPath, size_t outLen);
  bool buildSessionMarkerPath(char* outPath, size_t outLen) const;
  bool ensureSessionMarkerName(uint32_t nowMs);
  bool issueOpenForCurrentJob(uint32_t nowMs);
  bool rotateAllFilePart(uint32_t nowMs);
  bool rotateEventFilePart(uint32_t nowMs);
  void resetAllSessionState(bool clearNames);
  bool ensureAllSessionReady(uint32_t nowMs);

#if CO2CONTROL_HAS_ASYNC_SD
  enum class MountStage : uint8_t {
    IDLE = 0,
    WAIT_MOUNT,
    WAIT_MKDIR_LOGS,
    WAIT_MKDIR_DAILY,
    READY
  };

  enum class JobType : uint8_t {
    NONE = 0,
    SAMPLE_DAILY,
    SAMPLE_ALL,
    EVENT
  };

  enum class JobOp : uint8_t {
    NONE = 0,
    WAIT_STAT,
    WAIT_ROLLOVER_REMOVE_PREV,
    WAIT_ROLLOVER_RENAME,
    WAIT_OPEN,
    WAIT_HEADER_WRITE,
    WAIT_LINE_WRITE,
    WAIT_CLOSE
  };

  struct FileJob {
    static constexpr uint8_t MAX_BATCH_LINES = 8U;
    bool active = false;
    JobType type = JobType::NONE;
    JobOp op = JobOp::NONE;
    AsyncSD::RequestId requestId = AsyncSD::INVALID_REQUEST_ID;
    AsyncSD::FileHandle handle = AsyncSD::INVALID_FILE_HANDLE;
    size_t queueIndex = 0;
    uint8_t batchCount = 0;
    uint8_t writeIndex = 0;
    uint32_t startMs = 0;
    char path[HardwareSettings::SD_PATH_BYTES] = {};
    const char* header = nullptr;
    const char* lines[MAX_BATCH_LINES] = {};
    size_t headerLen = 0;
    uint16_t lineLens[MAX_BATCH_LINES] = {};
    uint32_t lineBytesTotal = 0;
    uint32_t sizeLimit = 0;
    uint32_t knownSize = 0;
    bool headerNeeded = false;
    bool rolloverOnLimit = false;
    bool failurePending = false;
    Status failureStatus = Ok();
  };

  bool setupAsyncSd(uint32_t nowMs);
  bool enqueueMountRequest(uint32_t nowMs);
  bool enqueueInfoRequest(uint32_t nowMs);
  bool startFileJob(JobType type,
                    size_t queueIndex,
                    uint8_t batchCount,
                    const char* path,
                    const char* header,
                    uint32_t sizeLimit,
                    bool rolloverOnLimit,
                    uint32_t nowMs);
  bool enqueueJobWrite(const void* data, size_t len, const char* errMsg, uint32_t nowMs);
  void popCompletedSampleHeadRecords();
  void popInvalidEventHeadRecords();
  void handleAsyncResult(const AsyncSD::RequestResult& result, uint32_t nowMs);
  void handleMountResult(const AsyncSD::RequestResult& result, uint32_t nowMs);
  void handleFileJobResult(const AsyncSD::RequestResult& result, uint32_t nowMs);
  void completeFileJobSuccess(uint32_t nowMs);
  void applyFileJobFailure(const Status& st, uint32_t nowMs);
  void failFileJob(const Status& st, uint32_t nowMs);
  static bool isAsyncMediaFault(AsyncSD::ErrorCode code);
  static Status mapAsyncResult(const AsyncSD::RequestResult& result, const char* msg);

  AsyncSD::SdCardManager _sd;
  static constexpr uint16_t SESSION_DIR_LIST_MAX = 128U;
  AsyncSD::DirEntry _sessionDirEntries[SESSION_DIR_LIST_MAX] = {};
  bool _sdStarted = false;
  bool _deferredMount = false;
  bool _deferredRemount = false;
  bool _deferredTeardown = false;
  MountStage _mountStage = MountStage::IDLE;
  AsyncSD::RequestId _mountRequestId = AsyncSD::INVALID_REQUEST_ID;
  AsyncSD::RequestId _infoRequestId = AsyncSD::INVALID_REQUEST_ID;
  AsyncSD::RequestId _sessionRequestId = AsyncSD::INVALID_REQUEST_ID;
  AsyncSD::FileHandle _sessionSetupHandle = AsyncSD::INVALID_FILE_HANDLE;
  FileJob _job{};
#endif

  LogRecord* _queue = nullptr;
  size_t _queueCapacity = 0;
  bool _queueUsingPsram = false;
  size_t _head = 0;
  size_t _tail = 0;
  size_t _count = 0;
  EventRecord* _eventQueue = nullptr;
  size_t _eventQueueCapacity = 0;
  bool _eventQueueUsingPsram = false;
  size_t _eventHead = 0;
  size_t _eventTail = 0;
  size_t _eventCount = 0;

  HardwareSettings _config{};
  AppSettings _appSettings{};
  RuntimeSettings _settings{};
  bool _enabled = false;
  bool _mounted = false;
  bool _sdInfoValid = false;
  bool _sdUsageValid = false;
  bool _dailyOk = false;
  bool _allOk = false;
  bool _allCapped = false;
  bool _allSizeKnown = false;
  uint32_t _allBytes = 0;
  uint32_t _lastWriteMs = 0;
  uint32_t _lastFlushMs = 0;
  uint32_t _lastMountAttemptMs = 0;
  uint8_t _mountCycleCount = 0;
  uint32_t _lastInfoRequestMs = 0;
  uint8_t _infoTimeoutStreak = 0;
  uint32_t _sdInfoLastUpdateMs = 0;
  uint32_t _lastTickElapsedMs = 0;
  uint32_t _budgetExceededCount = 0;
  uint32_t _dropped = 0;
  uint32_t _eventDropped = 0;
  uint32_t _lastErrorMs = 0;
  uint64_t _sdFsCapacityBytes = 0;
  uint64_t _sdFsUsedBytes = 0;
  uint64_t _sdFsFreeBytes = 0;
  uint64_t _sdCardCapacityBytes = 0;
  uint8_t _sdFsTypeCode = 0;
  uint8_t _sdCardTypeCode = 0;
  bool _allSessionReady = false;
  uint16_t _allSessionSeq = 1;
  uint16_t _allSessionProbeCount = 0;
  uint16_t _allFilePart = 1;
  uint16_t _eventFilePart = 1;
  uint32_t _allSessionRetryAfterMs = 0;
  bool _allSessionNeedsFreshName = false;
  uint8_t _allSessionStage = 0;  // 0=idle, 1=mkdir runs, 2=mkdir session, 3=list session, 4=stat data, 5=stat events, 6=verify fresh dir, 7=open marker, 8=close marker
  char _allSessionDirName[32] = {0};
  char _allSessionDirPath[HardwareSettings::SD_PATH_BYTES] = {0};
  char _allSessionMarkerName[32] = {0};
  char _allCurrentFilePath[HardwareSettings::SD_PATH_BYTES] = {0};
  char _eventCurrentFilePath[HardwareSettings::SD_PATH_BYTES] = {0};
  uint32_t _sampleWriteSuccessCount = 0;
  uint32_t _sampleWriteFailureCount = 0;
  uint32_t _eventWriteSuccessCount = 0;
  uint32_t _eventWriteFailureCount = 0;
  uint32_t _sampleRotateCount = 0;
  uint32_t _eventRotateCount = 0;
  uint32_t _sampleLinesCurrentFile = 0;
  uint32_t _eventLinesCurrentFile = 0;
  uint32_t _sampleLinesTotal = 0;
  uint32_t _eventLinesTotal = 0;
  Status _lastError = Ok();
};

}  // namespace CO2Control

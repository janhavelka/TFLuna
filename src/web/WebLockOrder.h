#pragma once

namespace TFLunaControl {

class IWebTryLock {
 public:
  virtual ~IWebTryLock() = default;
  virtual bool tryLock() = 0;
  virtual void unlock() = 0;
};

class OrderedWebReadGuard {
 public:
  explicit OrderedWebReadGuard(IWebTryLock& scratchLock) : _scratchLock(&scratchLock) {}

  ~OrderedWebReadGuard() { release(); }

  template <typename SnapshotFn>
  bool tryAcquireScratchThenSnapshot(SnapshotFn snapshotFn) {
    if (!acquireScratch()) {
      return false;
    }
    if (!snapshotFn()) {
      release();
      return false;
    }
    return true;
  }

  bool acquireScratch() {
    if (_scratchHeld) {
      return true;
    }
    _scratchHeld = _scratchLock->tryLock();
    return _scratchHeld;
  }

  bool isHeld() const { return _scratchHeld; }

  void release() {
    if (_scratchHeld) {
      _scratchLock->unlock();
      _scratchHeld = false;
    }
  }

 private:
  IWebTryLock* _scratchLock = nullptr;
  bool _scratchHeld = false;
};

}  // namespace TFLunaControl

#pragma once
#include <stdint.h>

namespace pet_refresh {

// A trial for animation continuity, not a panel specification. Longer stretches
// of partial updates can retain more ghosting; verify this setting on the panel.
class Policy {
public:
  static constexpr uint32_t cleanupIntervalMs = 30000U;
  static constexpr uint32_t maxCleanupDelayMs = 60000U;

  bool needsFull(uint32_t now, bool resting, bool requested = false) const {
    if (requested || !hasFull_) return true;
    if (partialCount_ == 0U) return false;
    const uint32_t elapsed = now - lastFullAt_;
    return elapsed >= maxCleanupDelayMs ||
           (resting && elapsed >= cleanupIntervalMs);
  }

  void didFull(uint32_t completedAt) {
    hasFull_ = true;
    lastFullAt_ = completedAt;
    partialCount_ = 0U;
  }

  void didPartial() {
    if (partialCount_ != UINT32_MAX) ++partialCount_;
  }

  uint32_t partialSinceFull() const { return partialCount_; }

private:
  bool hasFull_ = false;
  uint32_t lastFullAt_ = 0U, partialCount_ = 0U;
};

} // namespace pet_refresh

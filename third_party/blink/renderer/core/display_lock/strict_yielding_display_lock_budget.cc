// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/display_lock/strict_yielding_display_lock_budget.h"

#include <algorithm>

namespace blink {

StrictYieldingDisplayLockBudget::StrictYieldingDisplayLockBudget(
    DisplayLockContext* context)
    : DisplayLockBudget(context) {}

bool StrictYieldingDisplayLockBudget::ShouldPerformPhase(Phase phase) const {
  // We should perform any phase earlier than the one we already completed.
  // Also, we should complete a new phase once per cycle.
  return (last_completed_phase_ && phase <= *last_completed_phase_) ||
         !completed_new_phase_this_cycle_;
}

void StrictYieldingDisplayLockBudget::DidPerformPhase(Phase phase) {
  if (!completed_new_phase_this_cycle_ &&
      (!last_completed_phase_ || phase > *last_completed_phase_)) {
    last_completed_phase_ = phase;
    completed_new_phase_this_cycle_ = true;
  }

#if DCHECK_IS_ON()
  // If we completed a new phase this cycle, then we should not complete any
  // later phases in the same cycle.
  if (completed_new_phase_this_cycle_) {
    DCHECK(last_completed_phase_);
    DCHECK(phase <= *last_completed_phase_);
  }
#endif
}

void StrictYieldingDisplayLockBudget::WillStartLifecycleUpdate() {
  // Figure out the next phase we would run. If we had completed a phase before,
  // then we should try to complete the next one, otherwise we'll start with the
  // first phase.
  Phase next_phase =
      last_completed_phase_
          ? static_cast<Phase>(
                std::min(static_cast<unsigned>(*last_completed_phase_) + 1,
                         static_cast<unsigned>(Phase::kLast)))
          : Phase::kFirst;

  // Mark the next phase we're scheduled to run.
  for (auto phase = static_cast<unsigned>(next_phase);
       phase <= static_cast<unsigned>(Phase::kLast); ++phase) {
    if (MarkAncestorsDirtyForPhaseIfNeeded(static_cast<Phase>(phase)))
      break;
  }

  completed_new_phase_this_cycle_ = false;
}

bool StrictYieldingDisplayLockBudget::DidFinishLifecycleUpdate() {
  return !completed_new_phase_this_cycle_ || !last_completed_phase_ ||
         *last_completed_phase_ != Phase::kLast;
}

}  // namespace blink

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webrunner/common/test/test_common.h"

#include <string>
#include <utility>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/run_loop.h"
#include "webrunner/common/mem_buffer_util.h"

namespace webrunner {

MockNavigationObserver::MockNavigationObserver() = default;

MockNavigationObserver::~MockNavigationObserver() = default;

void MockNavigationObserver::Acknowledge() {
  DCHECK(navigation_ack_callback_);
  std::move(navigation_ack_callback_)();

  // Pump the acknowledgement message over IPC.
  base::RunLoop().RunUntilIdle();
}

void MockNavigationObserver::OnNavigationStateChanged(
    chromium::web::NavigationEvent change,
    OnNavigationStateChangedCallback callback) {
  MockableOnNavigationStateChanged(std::move(change));
  navigation_ack_callback_ = std::move(callback);
}

std::string StringFromMemBufferOrDie(const fuchsia::mem::Buffer& buffer) {
  std::string output;
  CHECK(StringFromMemBuffer(buffer, &output));
  return output;
}

}  // namespace webrunner

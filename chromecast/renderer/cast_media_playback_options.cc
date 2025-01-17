// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/cast_media_playback_options.h"

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/logging.h"
#include "content/public/renderer/render_frame.h"
#include "mojo/public/cpp/bindings/associated_binding_set.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

namespace chromecast {

CastMediaPlaybackOptions::CastMediaPlaybackOptions(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<CastMediaPlaybackOptions>(
          render_frame),
      render_frame_action_blocked_(false) {
  // Override default content MediaPlaybackOptions
  renderer_media_playback_options_.is_background_suspend_enabled = true;
  renderer_media_playback_options_
      .is_background_video_track_optimization_supported = false;
  render_frame->SetRenderFrameMediaPlaybackOptions(
      renderer_media_playback_options_);

  render_frame->GetAssociatedInterfaceRegistry()->AddInterface(
      base::BindRepeating(
          &CastMediaPlaybackOptions::OnMediaPlaybackOptionsAssociatedRequest,
          base::Unretained(this)));
}

CastMediaPlaybackOptions::~CastMediaPlaybackOptions() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CastMediaPlaybackOptions::OnDestruct() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delete this;
}

bool CastMediaPlaybackOptions::RunWhenInForeground(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!render_frame_action_blocked_) {
    std::move(closure).Run();
    return false;
  }

  LOG(WARNING) << "A render frame action is being blocked.";
  pending_closures_.push_back(std::move(closure));
  return true;
}

void CastMediaPlaybackOptions::SetMediaLoadingBlocked(bool blocked) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  render_frame_action_blocked_ = blocked;
  if (blocked) {
    LOG(INFO) << "Render frame actions are blocked.";
    return;
  }
  // Move callbacks in case OnBlockMediaLoading() is called somehow
  // during iteration.
  std::vector<base::OnceClosure> callbacks;
  callbacks.swap(pending_closures_);
  for (auto& cb : callbacks) {
    std::move(cb).Run();
  }
  LOG(INFO) << "Render frame actions are unblocked.";
}

void CastMediaPlaybackOptions::SetBackgroundSuspendEnabled(bool enabled) {
  renderer_media_playback_options_.is_background_suspend_enabled = enabled;
  render_frame()->SetRenderFrameMediaPlaybackOptions(
      renderer_media_playback_options_);
}

void CastMediaPlaybackOptions::OnMediaPlaybackOptionsAssociatedRequest(
    chromecast::shell::mojom::MediaPlaybackOptionsAssociatedRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

}  // namespace chromecast

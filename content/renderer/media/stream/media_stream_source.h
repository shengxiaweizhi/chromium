// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_STREAM_MEDIA_STREAM_SOURCE_H_
#define CONTENT_RENDERER_MEDIA_STREAM_MEDIA_STREAM_SOURCE_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "content/public/common/media_stream_request.h"
#include "third_party/blink/public/platform/web_media_stream_source.h"

namespace content {

// Names for media stream source capture types.
// These are values set via the "chromeMediaSource" constraint.
CONTENT_EXPORT extern const char kMediaStreamSourceTab[];
CONTENT_EXPORT extern const char kMediaStreamSourceScreen[]; /* video only */
CONTENT_EXPORT extern const char kMediaStreamSourceDesktop[];
CONTENT_EXPORT extern const char kMediaStreamSourceSystem[]; /* audio only */

class CONTENT_EXPORT MediaStreamSource
    : public blink::WebMediaStreamSource::ExtraData {
 public:
  using SourceStoppedCallback =
      base::Callback<void(const blink::WebMediaStreamSource& source)>;

  using ConstraintsCallback =
      base::Callback<void(MediaStreamSource* source,
                          MediaStreamRequestResult result,
                          const blink::WebString& result_name)>;

  // Source constraints key for
  // https://dev.w3.org/2011/webrtc/editor/getusermedia.html.
  static const char kSourceId[];

  MediaStreamSource();
  ~MediaStreamSource() override;

  // Returns device information about a source that has been created by a
  // JavaScript call to GetUserMedia, e.g., a camera or microphone.
  const MediaStreamDevice& device() const { return device_; }

  // Stops the source (by calling DoStopSource()) and runs FinalizeStopSource().
  void StopSource();

  // Sets the source's state to muted or to live.
  void SetSourceMuted(bool is_muted);

  // Sets device information about a source that has been created by a
  // JavaScript call to GetUserMedia. F.E a camera or microphone.
  void SetDevice(const MediaStreamDevice& device);

  // Sets a callback that will be triggered when StopSource is called.
  void SetStopCallback(const SourceStoppedCallback& stop_callback);

  // Clears the previously-set SourceStoppedCallback so that it will not be run
  // in the future.
  void ResetSourceStoppedCallback();

  // Change the source to the |new_device| by calling DoChangeSource().
  void ChangeSource(const MediaStreamDevice& new_device);

 protected:
  // Called when StopSource is called. It allows derived classes to implement
  // its own Stop method.
  virtual void DoStopSource() = 0;

  // Called when ChangeSource is called. It allows derived class to implement
  // it's own ChangeSource method.
  virtual void DoChangeSource(const MediaStreamDevice& new_device) = 0;

  // Runs the stop callback (if set) and sets the
  // WebMediaStreamSource::readyState to ended. This can be used by
  // implementations to implement custom stop methods.
  void FinalizeStopSource();

 private:
  MediaStreamDevice device_;
  SourceStoppedCallback stop_callback_;

  // In debug builds, check that all methods are being called on the main
  // thread.
  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(MediaStreamSource);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_STREAM_MEDIA_STREAM_SOURCE_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_METRICS_ANDROID_PROFILE_SESSION_DURATIONS_SERVICE_H_
#define CHROME_BROWSER_ANDROID_METRICS_ANDROID_PROFILE_SESSION_DURATIONS_SERVICE_H_

#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/driver/sync_session_durations_metrics_recorder.h"

class GaiaCookieManagerService;
namespace identity {
class IdentityManager;
}
namespace syncer {
class SyncService;
}

// Tracks the active browsing time that the user spends signed in and/or syncing
// as fraction of their total browsing time.
class AndroidProfileSessionDurationsService : public KeyedService {
 public:
  // Callers must ensure that the parameters outlive this object.
  AndroidProfileSessionDurationsService(
      syncer::SyncService* sync_service,
      identity::IdentityManager* identity_manager,
      GaiaCookieManagerService* cookie_manager);
  ~AndroidProfileSessionDurationsService() override;

  // KeyedService:
  void Shutdown() override;

  // A session is defined as the time spent with the application in foreground
  // (the time duration between the application enters foreground until the
  // application enters background).
  void OnAppEnterForeground(base::TimeTicks session_start);
  void OnAppEnterBackground(base::TimeDelta session_length);

 private:
  std::unique_ptr<syncer::SyncSessionDurationsMetricsRecorder>
      metrics_recorder_;

  DISALLOW_COPY_AND_ASSIGN(AndroidProfileSessionDurationsService);
};

#endif  // CHROME_BROWSER_ANDROID_METRICS_ANDROID_PROFILE_SESSION_DURATIONS_SERVICE_H_

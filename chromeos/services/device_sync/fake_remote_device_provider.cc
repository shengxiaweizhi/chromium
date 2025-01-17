// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/fake_remote_device_provider.h"

namespace chromeos {

namespace device_sync {

FakeRemoteDeviceProvider::FakeRemoteDeviceProvider() = default;

FakeRemoteDeviceProvider::~FakeRemoteDeviceProvider() = default;

void FakeRemoteDeviceProvider::NotifyObserversDeviceListChanged() {
  RemoteDeviceProvider::NotifyObserversDeviceListChanged();
}

const chromeos::multidevice::RemoteDeviceList&
FakeRemoteDeviceProvider::GetSyncedDevices() const {
  return synced_remote_devices_;
}

}  // namespace device_sync

}  // namespace chromeos

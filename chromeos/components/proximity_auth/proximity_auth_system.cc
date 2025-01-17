// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/proximity_auth/proximity_auth_system.h"

#include "chromeos/components/proximity_auth/logging/logging.h"
#include "chromeos/components/proximity_auth/proximity_auth_client.h"
#include "chromeos/components/proximity_auth/remote_device_life_cycle_impl.h"
#include "chromeos/components/proximity_auth/unlock_manager_impl.h"
#include "chromeos/services/secure_channel/public/cpp/client/secure_channel_client.h"

namespace proximity_auth {

ProximityAuthSystem::ProximityAuthSystem(
    ScreenlockType screenlock_type,
    ProximityAuthClient* proximity_auth_client,
    chromeos::secure_channel::SecureChannelClient* secure_channel_client)
    : secure_channel_client_(secure_channel_client),
      unlock_manager_(
          new UnlockManagerImpl(screenlock_type,
                                proximity_auth_client,
                                proximity_auth_client->GetPrefManager())),
      suspended_(false),
      started_(false) {}

ProximityAuthSystem::ProximityAuthSystem(
    chromeos::secure_channel::SecureChannelClient* secure_channel_client,
    std::unique_ptr<UnlockManager> unlock_manager)
    : secure_channel_client_(secure_channel_client),
      unlock_manager_(std::move(unlock_manager)),
      suspended_(false),
      started_(false) {}

ProximityAuthSystem::~ProximityAuthSystem() {
  ScreenlockBridge::Get()->RemoveObserver(this);
  unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);
}

void ProximityAuthSystem::Start() {
  if (started_)
    return;
  started_ = true;
  ScreenlockBridge::Get()->AddObserver(this);
  const AccountId& focused_account_id =
      ScreenlockBridge::Get()->focused_account_id();
  if (focused_account_id.is_valid())
    OnFocusedUserChanged(focused_account_id);
}

void ProximityAuthSystem::Stop() {
  if (!started_)
    return;
  started_ = false;
  ScreenlockBridge::Get()->RemoveObserver(this);
  OnFocusedUserChanged(EmptyAccountId());
}

void ProximityAuthSystem::SetRemoteDevicesForUser(
    const AccountId& account_id,
    const chromeos::multidevice::RemoteDeviceRefList& remote_devices,
    base::Optional<chromeos::multidevice::RemoteDeviceRef> local_device) {
  remote_devices_map_[account_id] = remote_devices;
  local_device_map_.emplace(account_id, *local_device);

  if (started_) {
    const AccountId& focused_account_id =
        ScreenlockBridge::Get()->focused_account_id();
    if (focused_account_id.is_valid())
      OnFocusedUserChanged(focused_account_id);
  }
}

chromeos::multidevice::RemoteDeviceRefList
ProximityAuthSystem::GetRemoteDevicesForUser(
    const AccountId& account_id) const {
  if (remote_devices_map_.find(account_id) == remote_devices_map_.end())
    return chromeos::multidevice::RemoteDeviceRefList();
  return remote_devices_map_.at(account_id);
}

void ProximityAuthSystem::OnAuthAttempted(const AccountId& /* account_id */) {
  // TODO(tengs): There is no reason to pass the |account_id| argument anymore.
  unlock_manager_->OnAuthAttempted(mojom::AuthType::USER_CLICK);
}

void ProximityAuthSystem::OnSuspend() {
  PA_LOG(INFO) << "Preparing for device suspension.";
  DCHECK(!suspended_);
  suspended_ = true;
  unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);
  remote_device_life_cycle_.reset();
}

void ProximityAuthSystem::OnSuspendDone() {
  PA_LOG(INFO) << "Device resumed from suspension.";
  DCHECK(suspended_);
  suspended_ = false;

  if (!ScreenlockBridge::Get()->IsLocked()) {
    PA_LOG(INFO) << "Suspend done, but no lock screen.";
  } else if (!started_) {
    PA_LOG(INFO) << "Suspend done, but not system started.";
  } else {
    OnFocusedUserChanged(ScreenlockBridge::Get()->focused_account_id());
  }
}

void ProximityAuthSystem::CancelConnectionAttempt() {
  unlock_manager_->CancelConnectionAttempt();
}

std::unique_ptr<RemoteDeviceLifeCycle>
ProximityAuthSystem::CreateRemoteDeviceLifeCycle(
    chromeos::multidevice::RemoteDeviceRef remote_device,
    base::Optional<chromeos::multidevice::RemoteDeviceRef> local_device) {
  return std::make_unique<RemoteDeviceLifeCycleImpl>(
      remote_device, local_device, secure_channel_client_);
}

void ProximityAuthSystem::OnLifeCycleStateChanged(
    RemoteDeviceLifeCycle::State old_state,
    RemoteDeviceLifeCycle::State new_state) {
  unlock_manager_->OnLifeCycleStateChanged();
}

void ProximityAuthSystem::OnScreenDidLock(
    ScreenlockBridge::LockHandler::ScreenType screen_type) {
  const AccountId& focused_account_id =
      ScreenlockBridge::Get()->focused_account_id();
  if (focused_account_id.is_valid())
    OnFocusedUserChanged(focused_account_id);
}

void ProximityAuthSystem::OnScreenDidUnlock(
    ScreenlockBridge::LockHandler::ScreenType screen_type) {
  unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);
  remote_device_life_cycle_.reset();
}

void ProximityAuthSystem::OnFocusedUserChanged(const AccountId& account_id) {
  // Update the current RemoteDeviceLifeCycle to the focused user.
  if (remote_device_life_cycle_) {
    if (remote_device_life_cycle_->GetRemoteDevice().user_id() !=
        account_id.GetUserEmail()) {
      PA_LOG(INFO) << "Focused user changed, destroying life cycle for "
                   << account_id.Serialize() << ".";
      unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);
      remote_device_life_cycle_.reset();
    } else {
      PA_LOG(INFO) << "Refocused on a user who is already focused.";
      return;
    }
  }

  if (remote_devices_map_.find(account_id) == remote_devices_map_.end() ||
      remote_devices_map_[account_id].size() == 0) {
    PA_LOG(INFO) << "User " << account_id.Serialize()
                 << " does not have a Smart Lock host device.";
    return;
  }
  if (local_device_map_.find(account_id) == local_device_map_.end()) {
    PA_LOG(INFO) << "User " << account_id.Serialize()
                 << " does not have a local device.";
    return;
  }

  // TODO(tengs): We currently assume each user has only one RemoteDevice, so we
  // can simply take the first item in the list.
  chromeos::multidevice::RemoteDeviceRef remote_device =
      remote_devices_map_[account_id][0];

  base::Optional<chromeos::multidevice::RemoteDeviceRef> local_device;
  local_device = local_device_map_.at(account_id);

  if (!suspended_) {
    PA_LOG(INFO) << "Creating RemoteDeviceLifeCycle for focused user: "
                 << account_id.Serialize();
    remote_device_life_cycle_ =
        CreateRemoteDeviceLifeCycle(remote_device, local_device);
    remote_device_life_cycle_->AddObserver(this);

    // UnlockManager listens for Bluetooth power change events, and is therefore
    // responsible for starting RemoteDeviceLifeCycle when Bluetooth becomes
    // powered.
    unlock_manager_->SetRemoteDeviceLifeCycle(remote_device_life_cycle_.get());
  }
}

}  // namespace proximity_auth

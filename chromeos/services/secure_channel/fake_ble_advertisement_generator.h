// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_BLE_ADVERTISEMENT_GENERATOR_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_BLE_ADVERTISEMENT_GENERATOR_H_

#include "base/macros.h"
#include "chromeos/services/secure_channel/ble_advertisement_generator.h"
#include "chromeos/services/secure_channel/data_with_timestamp.h"

namespace chromeos {

namespace secure_channel {

// Test double for BleAdvertisementGenerator.
class FakeBleAdvertisementGenerator : public BleAdvertisementGenerator {
 public:
  FakeBleAdvertisementGenerator();
  ~FakeBleAdvertisementGenerator() override;

  // Sets the advertisement to be returned by the next call to
  // GenerateBleAdvertisementInternal(). Note that, because |advertisement| is
  // a std::unique_ptr, set_advertisement() must be called each time an
  // advertisement is expected to be returned.
  void set_advertisement(std::unique_ptr<DataWithTimestamp> advertisement) {
    advertisement_ = std::move(advertisement);
  }

 protected:
  std::unique_ptr<DataWithTimestamp> GenerateBleAdvertisementInternal(
      chromeos::multidevice::RemoteDeviceRef remote_device,
      const std::string& local_device_public_key) override;

 private:
  std::unique_ptr<DataWithTimestamp> advertisement_;

  DISALLOW_COPY_AND_ASSIGN(FakeBleAdvertisementGenerator);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_BLE_ADVERTISEMENT_GENERATOR_H_

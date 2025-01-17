// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/ble_advertisement_generator.h"

#include <memory>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "chromeos/components/multidevice/remote_device_ref.h"
#include "chromeos/components/multidevice/remote_device_test_util.h"
#include "chromeos/services/device_sync/proto/cryptauth_api.pb.h"
#include "chromeos/services/secure_channel/mock_foreground_eid_generator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::StrictMock;

namespace chromeos {

namespace secure_channel {

namespace {

const char kLocalDevicePublicKey[] = "localDevicePublicKey";

std::vector<cryptauth::BeaconSeed> CreateBeaconSeedsForDevice(
    const std::string& device_id) {
  cryptauth::BeaconSeed seed1;
  seed1.set_data("seed1Data" + device_id);
  seed1.set_start_time_millis(1000L);
  seed1.set_start_time_millis(2000L);

  cryptauth::BeaconSeed seed2;
  seed2.set_data("seed2Data" + device_id);
  seed2.set_start_time_millis(2000L);
  seed2.set_start_time_millis(3000L);

  std::vector<cryptauth::BeaconSeed> seeds = {seed1, seed2};
  return seeds;
}

}  // namespace

class SecureChannelBleAdvertisementGeneratorTest : public testing::Test {
 protected:
  SecureChannelBleAdvertisementGeneratorTest()
      : test_remote_device_(
            chromeos::multidevice::RemoteDeviceRefBuilder()
                .SetBeaconSeeds(chromeos::multidevice::FromCryptAuthSeedList(
                    CreateBeaconSeedsForDevice("remote device id")))
                .Build()),
        fake_advertisement_("advertisement1", 1000L, 2000L) {}

  void SetUp() override {
    generator_ = base::WrapUnique(new BleAdvertisementGenerator());

    mock_eid_generator_ = new MockForegroundEidGenerator();
    generator_->SetEidGeneratorForTesting(
        base::WrapUnique(mock_eid_generator_));
  }

  void TearDown() override { generator_.reset(); }

  std::unique_ptr<DataWithTimestamp> CallGenerateBleAdvertisement(
      chromeos::multidevice::RemoteDeviceRef remote_device,
      const std::string& local_device_public_key) {
    return generator_->GenerateBleAdvertisementInternal(
        remote_device, local_device_public_key);
  }

  const chromeos::multidevice::RemoteDeviceRef test_remote_device_;
  const DataWithTimestamp fake_advertisement_;

  MockForegroundEidGenerator* mock_eid_generator_;

  std::unique_ptr<BleAdvertisementGenerator> generator_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SecureChannelBleAdvertisementGeneratorTest);
};

TEST_F(SecureChannelBleAdvertisementGeneratorTest, EmptyPublicKey) {
  EXPECT_FALSE(
      CallGenerateBleAdvertisement(test_remote_device_, std::string()));
}

TEST_F(SecureChannelBleAdvertisementGeneratorTest, EmptyBeaconSeeds) {
  EXPECT_FALSE(CallGenerateBleAdvertisement(
      chromeos::multidevice::CreateRemoteDeviceRefForTest(),
      kLocalDevicePublicKey));
}

TEST_F(SecureChannelBleAdvertisementGeneratorTest,
       CannotGenerateAdvertisement) {
  mock_eid_generator_->set_advertisement(nullptr);
  EXPECT_FALSE(
      CallGenerateBleAdvertisement(test_remote_device_, kLocalDevicePublicKey));
}

TEST_F(SecureChannelBleAdvertisementGeneratorTest, AdvertisementGenerated) {
  mock_eid_generator_->set_advertisement(
      std::make_unique<DataWithTimestamp>(fake_advertisement_));
  EXPECT_EQ(fake_advertisement_,
            *CallGenerateBleAdvertisement(test_remote_device_,
                                          kLocalDevicePublicKey));
}

}  // namespace secure_channel

}  // namespace chromeos

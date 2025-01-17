// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>
#import <ImageCaptureCore/ImageCaptureCore.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/mac/foundation_util.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/memory/weak_ptr.h"
#include "components/storage_monitor/image_capture_device.h"
#include "components/storage_monitor/image_capture_device_manager.h"
#include "components/storage_monitor/test_storage_monitor.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kDeviceId[] = "id";
const char kTestFileContents[] = "test";

}  // namespace

// Private ICCameraDevice method needed to properly initialize the object.
@interface NSObject ()
- (instancetype)initWithDictionary:(id)properties NS_DESIGNATED_INITIALIZER;
@end

@interface MockICCameraDevice : ICCameraDevice {
 @private
  base::scoped_nsobject<NSMutableArray> allMediaFiles_;
}

- (void)addMediaFile:(ICCameraFile*)file;

@end

@implementation MockICCameraDevice

- (instancetype)init {
  if ((self = [super initWithDictionary:@{}])) {
  }
  return self;
}

- (NSString*)mountPoint {
  return @"mountPoint";
}

- (NSString*)name {
  return @"name";
}

- (NSString*)UUIDString {
  return base::SysUTF8ToNSString(kDeviceId);
}

- (ICDeviceType)type {
  return ICDeviceTypeCamera;
}

- (void)requestOpenSession {
}

- (void)requestCloseSession {
}

- (NSArray*)mediaFiles {
  return allMediaFiles_;
}

- (void)addMediaFile:(ICCameraFile*)file {
  if (!allMediaFiles_.get())
    allMediaFiles_.reset([[NSMutableArray alloc] init]);
  [allMediaFiles_ addObject:file];
}

// This method does approximately what the internal ImageCapture platform
// library is observed to do: take the download save-as filename and mangle
// it to attach an extension, then return that new filename to the caller
// in the options.
- (void)requestDownloadFile:(ICCameraFile*)file
                    options:(NSDictionary*)options
           downloadDelegate:(id<ICCameraDeviceDownloadDelegate>)downloadDelegate
        didDownloadSelector:(SEL)selector
                contextInfo:(void*)contextInfo {
  base::FilePath saveDir(
      base::SysNSStringToUTF8([options[ICDownloadsDirectoryURL] path]));
  std::string saveAsFilename =
      base::SysNSStringToUTF8(options[ICSaveAsFilename]);
  // It appears that the ImageCapture library adds an extension to the requested
  // filename. Do that here to require a rename.
  saveAsFilename += ".jpg";
  base::FilePath toBeSaved = saveDir.Append(saveAsFilename);
  ASSERT_EQ(static_cast<int>(strlen(kTestFileContents)),
            base::WriteFile(toBeSaved, kTestFileContents,
                            strlen(kTestFileContents)));

  NSMutableDictionary* returnOptions =
      [NSMutableDictionary dictionaryWithDictionary:options];
  returnOptions[ICSavedFilename] = base::SysUTF8ToNSString(saveAsFilename);

  [static_cast<NSObject<ICCameraDeviceDownloadDelegate>*>(downloadDelegate)
   didDownloadFile:file
             error:nil
           options:returnOptions
       contextInfo:contextInfo];
}

@end

@interface MockICCameraFolder : ICCameraFolder {
 @private
  base::scoped_nsobject<NSString> name_;
}

- (instancetype)initWithName:(NSString*)name;

@end

@implementation MockICCameraFolder

- (instancetype)initWithName:(NSString*)name {
  if ((self = [super init])) {
    name_.reset([name retain]);
  }
  return self;
}

- (NSString*)name {
  return name_;
}

- (ICCameraFolder*)parentFolder {
  return nil;
}

@end

@interface MockICCameraFile : ICCameraFile {
 @private
  base::scoped_nsobject<NSString> name_;
  base::scoped_nsobject<NSDate> date_;
  base::scoped_nsobject<MockICCameraFolder> parent_;
}

- (instancetype)init:(NSString*)name;
- (void)setParent:(NSString*)parent;

@end

@implementation MockICCameraFile

- (instancetype)init:(NSString*)name {
  if ((self = [super init])) {
    name_.reset([name retain]);
    date_.reset([[NSDate dateWithNaturalLanguageString:@"12/12/12"] retain]);
  }
  return self;
}

- (void)setParent:(NSString*)parent {
  parent_.reset([[MockICCameraFolder alloc] initWithName:parent]);
}

- (ICCameraFolder*)parentFolder {
  return parent_.get();
}

- (NSString*)name {
  return name_;
}

- (NSString*)UTI {
  return base::mac::CFToNSCast(kUTTypeImage);
}

- (NSDate*)modificationDate {
  return date_.get();
}

- (NSDate*)creationDate {
  return date_.get();
}

- (off_t)fileSize {
  return 1000;
}

@end

namespace storage_monitor {

class TestCameraListener
    : public ImageCaptureDeviceListener,
      public base::SupportsWeakPtr<TestCameraListener> {
 public:
  TestCameraListener()
      : completed_(false),
        removed_(false),
        last_error_(base::File::FILE_ERROR_INVALID_URL) {}
  ~TestCameraListener() override {}

  void ItemAdded(const std::string& name,
                 const base::File::Info& info) override {
    items_.push_back(name);
  }

  void NoMoreItems() override { completed_ = true; }

  void DownloadedFile(const std::string& name,
                      base::File::Error error) override {
    EXPECT_TRUE(content::BrowserThread::CurrentlyOn(
        content::BrowserThread::UI));
    downloads_.push_back(name);
    last_error_ = error;
  }

  void DeviceRemoved() override { removed_ = true; }

  std::vector<std::string> items() const { return items_; }
  std::vector<std::string> downloads() const { return downloads_; }
  bool completed() const { return completed_; }
  bool removed() const { return removed_; }
  base::File::Error last_error() const { return last_error_; }

 private:
  std::vector<std::string> items_;
  std::vector<std::string> downloads_;
  bool completed_;
  bool removed_;
  base::File::Error last_error_;
};

class ImageCaptureDeviceManagerTest : public testing::Test {
 public:
  ImageCaptureDeviceManagerTest() {}

  void SetUp() override { monitor_ = TestStorageMonitor::CreateAndInstall(); }

  void TearDown() override { TestStorageMonitor::Destroy(); }

  MockICCameraDevice* AttachDevice(ImageCaptureDeviceManager* manager) {
    // Ownership will be passed to the device browser delegate.
    base::scoped_nsobject<MockICCameraDevice> device(
        [[MockICCameraDevice alloc] init]);
    id<ICDeviceBrowserDelegate> delegate = manager->device_browser_delegate();
    [delegate deviceBrowser:manager->device_browser_for_test()
               didAddDevice:device
                 moreComing:NO];
    return device.autorelease();
  }

  void DetachDevice(ImageCaptureDeviceManager* manager,
                    ICCameraDevice* device) {
    id<ICDeviceBrowserDelegate> delegate = manager->device_browser_delegate();
    [delegate deviceBrowser:manager->device_browser_for_test()
            didRemoveDevice:device
                  moreGoing:NO];
  }

  void RunUntilIdle() { thread_bundle_.RunUntilIdle(); }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  TestStorageMonitor* monitor_;
  TestCameraListener listener_;
};

TEST_F(ImageCaptureDeviceManagerTest, TestAttachDetach) {
  ImageCaptureDeviceManager manager;
  manager.SetNotifications(monitor_->receiver());
  ICCameraDevice* device = AttachDevice(&manager);
  std::vector<StorageInfo> devices = monitor_->GetAllAvailableStorages();

  ASSERT_EQ(1U, devices.size());
  EXPECT_EQ(std::string("ic:") + kDeviceId, devices[0].device_id());

  DetachDevice(&manager, device);
  devices = monitor_->GetAllAvailableStorages();
  ASSERT_EQ(0U, devices.size());
};

TEST_F(ImageCaptureDeviceManagerTest, OpenCamera) {
  ImageCaptureDeviceManager manager;
  manager.SetNotifications(monitor_->receiver());
  ICCameraDevice* device = AttachDevice(&manager);

  EXPECT_FALSE(ImageCaptureDeviceManager::deviceForUUID(
      "nonexistent"));

  base::scoped_nsobject<ImageCaptureDevice> camera(
      [ImageCaptureDeviceManager::deviceForUUID(kDeviceId) retain]);

  [camera setListener:listener_.AsWeakPtr()];
  [camera open];

  base::scoped_nsobject<MockICCameraFile> picture1(
      [[MockICCameraFile alloc] init:@"pic1"]);
  [camera cameraDevice:device didAddItem:picture1];
  base::scoped_nsobject<MockICCameraFile> picture2(
      [[MockICCameraFile alloc] init:@"pic2"]);
  [camera cameraDevice:device didAddItem:picture2];
  ASSERT_EQ(2U, listener_.items().size());
  EXPECT_EQ("pic1", listener_.items()[0]);
  EXPECT_EQ("pic2", listener_.items()[1]);
  EXPECT_FALSE(listener_.completed());

  [camera deviceDidBecomeReadyWithCompleteContentCatalog:device];

  ASSERT_EQ(2U, listener_.items().size());
  EXPECT_TRUE(listener_.completed());

  [camera close];
  DetachDevice(&manager, device);
  EXPECT_FALSE(ImageCaptureDeviceManager::deviceForUUID(kDeviceId));
}

TEST_F(ImageCaptureDeviceManagerTest, RemoveCamera) {
  ImageCaptureDeviceManager manager;
  manager.SetNotifications(monitor_->receiver());
  ICCameraDevice* device = AttachDevice(&manager);

  base::scoped_nsobject<ImageCaptureDevice> camera(
      [ImageCaptureDeviceManager::deviceForUUID(kDeviceId) retain]);

  [camera setListener:listener_.AsWeakPtr()];
  [camera open];

  [camera didRemoveDevice:device];
  EXPECT_TRUE(listener_.removed());
}

TEST_F(ImageCaptureDeviceManagerTest, DownloadFile) {
  ImageCaptureDeviceManager manager;
  manager.SetNotifications(monitor_->receiver());
  MockICCameraDevice* device = AttachDevice(&manager);

  base::scoped_nsobject<ImageCaptureDevice> camera(
      [ImageCaptureDeviceManager::deviceForUUID(kDeviceId) retain]);

  [camera setListener:listener_.AsWeakPtr()];
  [camera open];

  std::string kTestFileName("pic1");

  base::scoped_nsobject<MockICCameraFile> picture1(
      [[MockICCameraFile alloc] init:base::SysUTF8ToNSString(kTestFileName)]);
  [device addMediaFile:picture1];
  [camera cameraDevice:device didAddItem:picture1];

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  EXPECT_EQ(0U, listener_.downloads().size());

  // Test that a nonexistent file we ask to be downloaded will
  // return us a not-found error.
  base::FilePath temp_file = temp_dir.GetPath().Append("tempfile");
  [camera downloadFile:std::string("nonexistent") localPath:temp_file];
  RunUntilIdle();
  ASSERT_EQ(1U, listener_.downloads().size());
  EXPECT_EQ("nonexistent", listener_.downloads()[0]);
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, listener_.last_error());

  // Test that an existing file we ask to be downloaded will end up in
  // the location we specify. The mock system will copy testing file
  // contents to a separate filename, mimicking the ImageCaptureCore
  // library behavior. Our code then renames the file onto the requested
  // destination.
  [camera downloadFile:kTestFileName localPath:temp_file];
  RunUntilIdle();

  ASSERT_EQ(2U, listener_.downloads().size());
  EXPECT_EQ(kTestFileName, listener_.downloads()[1]);
  ASSERT_EQ(base::File::FILE_OK, listener_.last_error());
  char file_contents[5];
  ASSERT_EQ(4, base::ReadFile(temp_file, file_contents,
                              strlen(kTestFileContents)));
  EXPECT_EQ(kTestFileContents,
            std::string(file_contents, strlen(kTestFileContents)));

  [camera didRemoveDevice:device];
}

TEST_F(ImageCaptureDeviceManagerTest, TestSubdirectories) {
  ImageCaptureDeviceManager manager;
  manager.SetNotifications(monitor_->receiver());
  MockICCameraDevice* device = AttachDevice(&manager);

  base::scoped_nsobject<ImageCaptureDevice> camera(
      [ImageCaptureDeviceManager::deviceForUUID(kDeviceId) retain]);

  [camera setListener:listener_.AsWeakPtr()];
  [camera open];

  std::string kTestFileName("pic1");
  base::scoped_nsobject<MockICCameraFile> picture1(
      [[MockICCameraFile alloc] init:base::SysUTF8ToNSString(kTestFileName)]);
  [picture1 setParent:base::SysUTF8ToNSString("dir")];
  [device addMediaFile:picture1];
  [camera cameraDevice:device didAddItem:picture1];

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath temp_file = temp_dir.GetPath().Append("tempfile");

  [camera downloadFile:("dir/" + kTestFileName) localPath:temp_file];
  RunUntilIdle();

  char file_contents[5];
  ASSERT_EQ(4, base::ReadFile(temp_file, file_contents,
                              strlen(kTestFileContents)));
  EXPECT_EQ(kTestFileContents,
            std::string(file_contents, strlen(kTestFileContents)));

  [camera didRemoveDevice:device];
}

}  // namespace storage_monitor

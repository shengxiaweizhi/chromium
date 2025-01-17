// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "build/build_config.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/passwords/google_password_manager_navigation_throttle.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/sync/test/fake_server/fake_server_network_resources.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/url_loader_interceptor.h"
#include "url/gurl.h"
#include "url/url_canon_stdstring.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/signin/core/browser/signin_manager_base.h"
#endif

namespace {

GURL GetExampleURL() {
  return GURL("https://example.com");
}

GURL GetGooglePasswordManagerURL() {
  return GURL(chrome::kGooglePasswordManagerURL);
}

// Starts to navigate to the |url| and then returns the GURL that ultimately was
// navigated to.
GURL NavigateToURL(Browser* browser,
                   const GURL& url,
                   ui::PageTransition transition) {
  NavigateParams params(browser, url, transition);
  ui_test_utils::NavigateToURL(&params);
  return browser->tab_strip_model()
      ->GetWebContentsAt(0)
      ->GetController()
      .GetLastCommittedEntry()
      ->GetURL();
}

}  // namespace

class GooglePasswordManagerNavigationThrottleTest : public SyncTest {
 public:
  GooglePasswordManagerNavigationThrottleTest()
      : SyncTest(TestType::SINGLE_CLIENT),
        interceptor_(
            std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
                [](content::URLLoaderInterceptor::RequestParams* params) {
                  params->client->OnComplete(
                      network::URLLoaderCompletionStatus(net::ERR_FAILED));
                  return true;
                }))) {}

  std::unique_ptr<ProfileSyncServiceHarness> EnableGooglePasswordManagerAndSync(
      Profile* profile) {
    feature_list_.InitAndEnableFeature(
        password_manager::features::kGooglePasswordManager);

    ProfileSyncServiceFactory::GetForProfile(profile)
        ->OverrideNetworkResourcesForTest(
            std::make_unique<fake_server::FakeServerNetworkResources>(
                GetFakeServer()->AsWeakPtr()));

    std::string username;
#if defined(OS_CHROMEOS)
    // In browser tests, the profile may already by authenticated with stub
    // account |user_manager::kStubUserEmail|.
    AccountInfo info = SigninManagerFactory::GetForProfile(profile)
                           ->GetAuthenticatedAccountInfo();
    username = info.email;
#endif
    if (username.empty())
      username = "user@gmail.com";

    std::unique_ptr<ProfileSyncServiceHarness> harness =
        ProfileSyncServiceHarness::Create(
            profile, username, "password",
            ProfileSyncServiceHarness::SigninType::FAKE_SIGNIN);

    EXPECT_TRUE(harness->SetupSync());
    return harness;
  }

 protected:
  void TearDownOnMainThread() override {
    interceptor_.reset();
    SyncTest::TearDownOnMainThread();
  }

 private:
  // Instantiate a content::URLLoaderInterceptor that will fail all requests
  // with net::ERR_FAILED. This is done, because we are interested in being
  // redirected when a navigation fails.
  std::unique_ptr<content::URLLoaderInterceptor> interceptor_;
};

// No navigation should be redirected in case the Google Password Manager and
// Sync are not enabled.
IN_PROC_BROWSER_TEST_F(GooglePasswordManagerNavigationThrottleTest,
                       ExampleWithoutGPMAndSync) {
  EXPECT_EQ(GetExampleURL(),
            NavigateToURL(browser(), GetExampleURL(),
                          ui::PageTransition::PAGE_TRANSITION_LINK));
}

IN_PROC_BROWSER_TEST_F(GooglePasswordManagerNavigationThrottleTest,
                       PasswordsWithoutGPMAndSync) {
  EXPECT_EQ(GetGooglePasswordManagerURL(),
            NavigateToURL(browser(), GetGooglePasswordManagerURL(),
                          ui::PageTransition::PAGE_TRANSITION_LINK));
}

// Accessing a web resource from within this browser test will fail (see
// |interceptor_| above), thus we expect to be redirected to the Passwords
// Settings Subpage when trying to access the Google Password Manager when the
// user's profile should be considered and the user clicked a link to get to the
// Google Password Manager page.
IN_PROC_BROWSER_TEST_F(GooglePasswordManagerNavigationThrottleTest,
                       ExampleWithGPMAndSync) {
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableGooglePasswordManagerAndSync(browser()->profile());
  EXPECT_EQ(GetExampleURL(),
            NavigateToURL(browser(), GetExampleURL(),
                          ui::PageTransition::PAGE_TRANSITION_LINK));
}

IN_PROC_BROWSER_TEST_F(GooglePasswordManagerNavigationThrottleTest,
                       PasswordsWithGPMAndSyncUserTyped) {
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableGooglePasswordManagerAndSync(browser()->profile());
  EXPECT_EQ(GetGooglePasswordManagerURL(),
            NavigateToURL(browser(), GetGooglePasswordManagerURL(),
                          ui::PageTransition::PAGE_TRANSITION_TYPED));
}

IN_PROC_BROWSER_TEST_F(GooglePasswordManagerNavigationThrottleTest,
                       PasswordsWithGPMAndSyncUserClickedLink) {
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableGooglePasswordManagerAndSync(browser()->profile());
  EXPECT_EQ(chrome::GetSettingsUrl(chrome::kPasswordManagerSubPage),
            NavigateToURL(browser(), GetGooglePasswordManagerURL(),
                          ui::PageTransition::PAGE_TRANSITION_LINK));
}

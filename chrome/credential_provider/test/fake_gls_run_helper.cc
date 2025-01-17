// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_gls_run_helper.h"

#include "chrome/credential_provider/gaiacp/stdafx.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/multiprocess_test.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"
#include "chrome/credential_provider/gaiacp/scoped_lsa_policy.h"
#include "chrome/credential_provider/test/test_credential.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace credential_provider {

namespace switches {

constexpr char kGlsUserEmail[] = "gls-user-email";
constexpr char kStartGlsEventName[] = "start-gls-event-name";

}  // namespace switches

namespace testing {

// Corresponding default email and username for tests that don't override them.
const char kDefaultEmail[] = "foo@gmail.com";
const wchar_t kDefaultUsername[] = L"foo";

}  // namespace testing

namespace {

// Generates a common signin result given an email pass through the command
// line and writes this result to stdout.  This is used as a fake GLS process
// for testing.
MULTIPROCESS_TEST_MAIN(gls_main) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  // If a start event name is specified, the process waits for an event from the
  // tester telling it that it can start running.
  if (command_line->HasSwitch(switches::kStartGlsEventName)) {
    base::string16 start_event_name =
        command_line->GetSwitchValueNative(switches::kStartGlsEventName);
    if (!start_event_name.empty()) {
      base::win::ScopedHandle start_event_handle(::CreateEvent(
          nullptr, false, false, base::UTF16ToWide(start_event_name).c_str()));
      if (start_event_handle.IsValid()) {
        base::WaitableEvent start_event(std::move(start_event_handle));
        start_event.Wait();
      }
    }
  }

  std::string gls_email =
      command_line->GetSwitchValueASCII(switches::kGlsUserEmail);

  base::DictionaryValue dict;
  dict.SetString(kKeyEmail, gls_email);
  dict.SetString(kKeyFullname, "Full Name");
  dict.SetString(kKeyId, "1234567890");
  dict.SetString(kKeyMdmIdToken, "idt-123456");
  dict.SetString(kKeyPassword, "password");
  dict.SetString(kKeyRefreshToken, "rt-123456");
  dict.SetString(kKeyTokenHandle, "th-123456");

  std::string json;
  if (!base::JSONWriter::Write(dict, &json))
    return -1;

  HANDLE hstdout = ::GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD written;
  if (::WriteFile(hstdout, json.c_str(), json.length(), &written, nullptr)) {
    return 0;
  }

  return -1;
}
}  // namespace

FakeGlsRunHelper::FakeGlsRunHelper() {
  // Create the special gaia account used to run GLS and save its password.

  BSTR sid;
  DWORD error;
  EXPECT_EQ(S_OK, fake_os_user_manager_.AddUser(
                      kDefaultGaiaAccountName, L"password", L"fullname",
                      L"comment", true, &sid, &error));

  auto policy = ScopedLsaPolicy::Create(POLICY_ALL_ACCESS);
  EXPECT_EQ(S_OK, policy->StorePrivateData(kLsaKeyGaiaUsername,
                                           kDefaultGaiaAccountName));
  EXPECT_EQ(S_OK, policy->StorePrivateData(kLsaKeyGaiaPassword, L"password"));
}

FakeGlsRunHelper::~FakeGlsRunHelper() = default;

void FakeGlsRunHelper::SetUp() {
  // Make sure not to read random GCPW settings from the machine that is running
  // the tests.
  ASSERT_NO_FATAL_FAILURE(
      registry_override_.OverrideRegistry(HKEY_LOCAL_MACHINE));
}

HRESULT FakeGlsRunHelper::StartLogonProcess(ICredentialProviderCredential* cred,
                                            bool succeeds) {
  BOOL auto_login;
  EXPECT_EQ(S_OK, cred->SetSelected(&auto_login));

  // Logging on is an async process, so the call to GetSerialization() starts
  // the process, but when it returns it has not completed.
  CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE cpgsr;
  CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION cpcs;
  wchar_t* status_text;
  CREDENTIAL_PROVIDER_STATUS_ICON status_icon;
  EXPECT_EQ(S_OK,
            cred->GetSerialization(&cpgsr, &cpcs, &status_text, &status_icon));
  EXPECT_EQ(CPSI_NONE, status_icon);
  if (succeeds) {
    EXPECT_EQ(nullptr, status_text);
    EXPECT_EQ(CPGSR_NO_CREDENTIAL_NOT_FINISHED, cpgsr);
  } else {
    EXPECT_NE(nullptr, status_text);
    EXPECT_EQ(CPGSR_NO_CREDENTIAL_FINISHED, cpgsr);
  }
  return S_OK;
}

HRESULT FakeGlsRunHelper::WaitForLogonProcess(
    ICredentialProviderCredential* cred) {
  CComPtr<testing::ITestCredential> test;
  EXPECT_EQ(S_OK, cred->QueryInterface(__uuidof(testing::ITestCredential),
                                       reinterpret_cast<void**>(&test)));
  EXPECT_EQ(S_OK, test->WaitForGls());

  return S_OK;
}

HRESULT FakeGlsRunHelper::StartLogonProcessAndWait(
    ICredentialProviderCredential* cred) {
  EXPECT_EQ(S_OK, StartLogonProcess(cred, /*succeeds=*/true));
  EXPECT_EQ(S_OK, WaitForLogonProcess(cred));
  return S_OK;
}

// static
HRESULT FakeGlsRunHelper::GetMockGlsCommandline(
    const std::string& gls_email,
    const base::string16& start_gls_event_name,
    base::CommandLine* command_line) {
  *command_line = base::GetMultiProcessTestChildBaseCommandLine();
  command_line->AppendSwitchASCII(::switches::kTestChildProcess, "gls_main");
  command_line->AppendSwitchASCII(switches::kGlsUserEmail, gls_email);

  if (!start_gls_event_name.empty()) {
    command_line->AppendSwitchNative(switches::kStartGlsEventName,
                                     start_gls_event_name);
  }

  return S_OK;
}

}  // namespace credential_provider

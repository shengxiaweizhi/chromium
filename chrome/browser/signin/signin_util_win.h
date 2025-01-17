// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_UTIL_WIN_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_UTIL_WIN_H_

#include <memory>

#include "chrome/browser/ui/webui/signin/dice_turn_sync_on_helper.h"

class Profile;

namespace signin_util {

// Attempt to sign in with a credentials from a system installed credential
// provider if available.
void SigninWithCredentialProviderIfPossible(Profile* profile);

// Sets the DiceTurnSyncOnHelper delegate for browser tests.
void SetDiceTurnSyncOnHelperDelegateForTesting(
    std::unique_ptr<DiceTurnSyncOnHelper::Delegate> delegate);

}  // namespace signin_util

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_UTIL_WIN_H_

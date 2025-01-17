// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_REAUTH_CREDENTIAL_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_REAUTH_CREDENTIAL_H_

#include "chrome/credential_provider/gaiacp/gaia_credential_base.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"

namespace credential_provider {

// Implementation of a ICredentialProviderCredential backed by a Gaia account.
class ATL_NO_VTABLE CReauthCredential
    : public CComObjectRootEx<CComMultiThreadModel>,
      public CGaiaCredentialBase,
      public IReauthCredential {
 public:
  DECLARE_NO_REGISTRY()

  CReauthCredential();
  ~CReauthCredential();

  HRESULT FinalConstruct();
  void FinalRelease();

 protected:
  HRESULT GetEmailForReauth(wchar_t* email, size_t length) override;

 private:
  // This class does not say it implements ICredentialProviderCredential2.
  // It only implements ICredentialProviderCredential.  Otherwise the
  // credential will show up on the welcome screen only for domain joined
  // machines.
  BEGIN_COM_MAP(CReauthCredential)
  COM_INTERFACE_ENTRY(IGaiaCredential)
  COM_INTERFACE_ENTRY(ICredentialProviderCredential)
  COM_INTERFACE_ENTRY(ICredentialProviderCredential2)
  COM_INTERFACE_ENTRY(IReauthCredential)
  END_COM_MAP()

  DECLARE_PROTECT_FINAL_CONSTRUCT()

  // IReauthCredential
  IFACEMETHODIMP SetUserInfo(BSTR sid, BSTR username, BSTR email) override;

  CComBSTR email_for_reauth_;
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_REAUTH_CREDENTIAL_H_

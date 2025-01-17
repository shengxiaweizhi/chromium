// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/trustedtypes/trusted_types_util.h"

#include "third_party/blink/renderer/bindings/core/v8/string_or_trusted_html.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_trusted_html_or_trusted_script_or_trusted_script_url_or_trusted_url.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_trusted_script.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_trusted_script_url.h"
#include "third_party/blink/renderer/bindings/core/v8/usv_string_or_trusted_url.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_html.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_script.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_script_url.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_type_policy.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_type_policy_factory.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_url.h"

namespace blink {

String GetStringFromTrustedType(
    const StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURLOrTrustedURL&
        string_or_trusted_type,
    const Document* doc,
    ExceptionState& exception_state) {
  DCHECK(string_or_trusted_type.IsString() ||
         RuntimeEnabledFeatures::TrustedDOMTypesEnabled());
  DCHECK(!string_or_trusted_type.IsNull());

  if (string_or_trusted_type.IsString() && doc && doc->RequireTrustedTypes()) {
    exception_state.ThrowTypeError(
        "This document requires a Trusted Type assignment.");
    return g_empty_string;
  }

  if (string_or_trusted_type.IsTrustedHTML())
    return string_or_trusted_type.GetAsTrustedHTML()->toString();
  if (string_or_trusted_type.IsTrustedScript())
    return string_or_trusted_type.GetAsTrustedScript()->toString();
  if (string_or_trusted_type.IsTrustedScriptURL())
    return string_or_trusted_type.GetAsTrustedScriptURL()->toString();
  if (string_or_trusted_type.IsTrustedURL())
    return string_or_trusted_type.GetAsTrustedURL()->toString();

  return string_or_trusted_type.GetAsString();
}

String GetStringFromTrustedTypeWithoutCheck(
    const StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURLOrTrustedURL&
        string_or_trusted_type) {
  if (string_or_trusted_type.IsTrustedHTML())
    return string_or_trusted_type.GetAsTrustedHTML()->toString();
  if (string_or_trusted_type.IsTrustedScript())
    return string_or_trusted_type.GetAsTrustedScript()->toString();
  if (string_or_trusted_type.IsTrustedScriptURL())
    return string_or_trusted_type.GetAsTrustedScriptURL()->toString();
  if (string_or_trusted_type.IsTrustedURL())
    return string_or_trusted_type.GetAsTrustedURL()->toString();
  if (string_or_trusted_type.IsString())
    return string_or_trusted_type.GetAsString();

  return g_empty_string;
}

String GetStringFromTrustedHTML(StringOrTrustedHTML string_or_trusted_html,
                                const Document* doc,
                                ExceptionState& exception_state) {
  DCHECK(string_or_trusted_html.IsString() ||
         RuntimeEnabledFeatures::TrustedDOMTypesEnabled());
  DCHECK(!string_or_trusted_html.IsNull());

  bool require_trusted_type = doc && doc->RequireTrustedTypes();
  if (!require_trusted_type && string_or_trusted_html.IsString()) {
    return string_or_trusted_html.GetAsString();
  }

  if (string_or_trusted_html.IsTrustedHTML()) {
    return string_or_trusted_html.GetAsTrustedHTML()->toString();
  }

  TrustedTypePolicy* default_policy =
      doc->ExecutingWindow()->trustedTypes()->getExposedPolicy("default");
  if (!default_policy) {
    exception_state.ThrowTypeError(
        "This document requires `TrustedHTML` assignment.");
    return g_empty_string;
  }

  TrustedHTML* result = default_policy->CreateHTML(
      doc->GetIsolate(), string_or_trusted_html.GetAsString(), exception_state);
  if (exception_state.HadException()) {
    exception_state.ClearException();
    exception_state.ThrowTypeError(
        "This document requires `TrustedHTML` assignment and 'default' policy "
        "failed to execute.");
    return g_empty_string;
  }

  return result->toString();
}

String GetStringFromTrustedScript(
    StringOrTrustedScript string_or_trusted_script,
    const Document* doc,
    ExceptionState& exception_state) {
  DCHECK(string_or_trusted_script.IsString() ||
         RuntimeEnabledFeatures::TrustedDOMTypesEnabled());

  // To remain compatible with legacy behaviour, HTMLElement uses extended IDL
  // attributes to allow for nullable union of (DOMString or TrustedScript).
  // Thus, this method is required to handle the case where
  // string_or_trusted_script.IsNull(), unlike the various similar methods in
  // this file.

  bool require_trusted_type = doc && doc->RequireTrustedTypes();
  if (!require_trusted_type) {
    if (string_or_trusted_script.IsString()) {
      return string_or_trusted_script.GetAsString();
    }
    if (string_or_trusted_script.IsNull()) {
      return g_empty_string;
    }
  }

  if (string_or_trusted_script.IsTrustedScript()) {
    return string_or_trusted_script.GetAsTrustedScript()->toString();
  }

  DCHECK(require_trusted_type);
  DCHECK(string_or_trusted_script.IsNull() ||
         string_or_trusted_script.IsString());

  TrustedTypePolicy* default_policy =
      doc->ExecutingWindow()->trustedTypes()->getExposedPolicy("default");
  if (!default_policy) {
    exception_state.ThrowTypeError(
        "This document requires `TrustedScript` assignment.");
    return g_empty_string;
  }

  const String& string_value_or_empty =
      string_or_trusted_script.IsNull()
          ? g_empty_string
          : string_or_trusted_script.GetAsString();
  TrustedScript* result = default_policy->CreateScript(
      doc->GetIsolate(), string_value_or_empty, exception_state);
  DCHECK_EQ(!result, exception_state.HadException());
  if (exception_state.HadException()) {
    exception_state.ClearException();
    exception_state.ThrowTypeError(
        "This document requires `TrustedScript` assignment and 'default' "
        "policy "
        "failed to execute.");
    return g_empty_string;
  }

  return result->toString();
}

String GetStringFromTrustedScriptURL(
    StringOrTrustedScriptURL string_or_trusted_script_url,
    const Document* doc,
    ExceptionState& exception_state) {
  DCHECK(string_or_trusted_script_url.IsString() ||
         RuntimeEnabledFeatures::TrustedDOMTypesEnabled());
  DCHECK(!string_or_trusted_script_url.IsNull());

  bool require_trusted_type = doc && doc->RequireTrustedTypes();
  if (!require_trusted_type && string_or_trusted_script_url.IsString()) {
    return string_or_trusted_script_url.GetAsString();
  }

  if (string_or_trusted_script_url.IsTrustedScriptURL()) {
    return string_or_trusted_script_url.GetAsTrustedScriptURL()->toString();
  }

  TrustedTypePolicy* default_policy =
      doc->ExecutingWindow()->trustedTypes()->getExposedPolicy("default");
  if (!default_policy) {
    exception_state.ThrowTypeError(
        "This document requires `TrustedScriptURL` assignment.");
    return g_empty_string;
  }

  TrustedScriptURL* result = default_policy->CreateScriptURL(
      doc->GetIsolate(), string_or_trusted_script_url.GetAsString(),
      exception_state);

  if (exception_state.HadException()) {
    exception_state.ClearException();
    exception_state.ThrowTypeError(
        "This document requires `TrustedScriptURL` assignment and 'default' "
        "policy "
        "failed to execute.");
    return g_empty_string;
  }

  return result->toString();
}

String GetStringFromTrustedURL(USVStringOrTrustedURL string_or_trusted_url,
                               const Document* doc,
                               ExceptionState& exception_state) {
  DCHECK(string_or_trusted_url.IsUSVString() ||
         RuntimeEnabledFeatures::TrustedDOMTypesEnabled());
  DCHECK(!string_or_trusted_url.IsNull());

  bool require_trusted_type = doc && doc->RequireTrustedTypes();
  if (!require_trusted_type && string_or_trusted_url.IsUSVString()) {
    return string_or_trusted_url.GetAsUSVString();
  }

  if (string_or_trusted_url.IsTrustedURL()) {
    return string_or_trusted_url.GetAsTrustedURL()->toString();
  }

  TrustedTypePolicy* default_policy =
      doc->ExecutingWindow()->trustedTypes()->getExposedPolicy("default");
  if (!default_policy) {
    exception_state.ThrowTypeError(
        "This document requires `TrustedURL` assignment.");
    return g_empty_string;
  }

  TrustedURL* result = default_policy->CreateURL(
      doc->GetIsolate(), string_or_trusted_url.GetAsUSVString(),
      exception_state);
  if (exception_state.HadException()) {
    exception_state.ClearException();
    exception_state.ThrowTypeError(
        "This document requires `TrustedURL` assignment and 'default' policy "
        "failed to execute.");
    return g_empty_string;
  }

  return result->toString();
}
}  // namespace blink

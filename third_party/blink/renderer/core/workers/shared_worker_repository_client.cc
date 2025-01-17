/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/workers/shared_worker_repository_client.h"

#include <memory>
#include <utility>
#include "base/logging.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/mojom/worker/shared_worker_info.mojom-blink.h"
#include "third_party/blink/public/platform/web_content_security_policy.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_shared_worker.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/workers/shared_worker.h"
#include "third_party/blink/renderer/core/workers/shared_worker_connect_listener.h"

namespace blink {

const char SharedWorkerRepositoryClient::kSupplementName[] =
    "SharedWorkerRepositoryClient";

SharedWorkerRepositoryClient* SharedWorkerRepositoryClient::From(
    Document& document) {
  DCHECK(IsMainThread());
  SharedWorkerRepositoryClient* client =
      Supplement<Document>::From<SharedWorkerRepositoryClient>(document);
  if (!client) {
    client = MakeGarbageCollected<SharedWorkerRepositoryClient>(document);
    Supplement<Document>::ProvideTo(document, client);
  }
  return client;
}

SharedWorkerRepositoryClient::SharedWorkerRepositoryClient(Document& document)
    : ContextLifecycleObserver(&document) {
  DCHECK(IsMainThread());
  document.GetInterfaceProvider()->GetInterface(mojo::MakeRequest(&connector_));
}

void SharedWorkerRepositoryClient::Connect(
    SharedWorker* worker,
    MessagePortChannel port,
    const KURL& url,
    mojom::blink::BlobURLTokenPtr blob_url_token,
    const String& name) {
  DCHECK(IsMainThread());
  DCHECK(!name.IsNull());

  // TODO(estark): this is broken, as it only uses the first header
  // when multiple might have been sent. Fix by making the
  // mojom::blink::SharedWorkerInfo take a map that can contain multiple
  // headers.
  Vector<CSPHeaderAndType> headers =
      worker->GetExecutionContext()->GetContentSecurityPolicy()->Headers();
  WebString header = "";
  auto header_type = mojom::ContentSecurityPolicyType::kReport;
  if (headers.size() > 0) {
    header = headers[0].first;
    header_type =
        static_cast<mojom::ContentSecurityPolicyType>(headers[0].second);
  }

  mojom::blink::SharedWorkerInfoPtr info(mojom::blink::SharedWorkerInfo::New(
      url, name, header, header_type,
      worker->GetExecutionContext()->GetSecurityContext().AddressSpace()));

  mojom::blink::SharedWorkerClientPtr client_ptr;
  client_set_.AddBinding(std::make_unique<SharedWorkerConnectListener>(worker),
                         mojo::MakeRequest(&client_ptr));

  connector_->Connect(
      std::move(info), std::move(client_ptr),
      worker->GetExecutionContext()->IsSecureContext()
          ? mojom::SharedWorkerCreationContextType::kSecure
          : mojom::SharedWorkerCreationContextType::kNonsecure,
      port.ReleaseHandle(),
      mojom::blink::BlobURLTokenPtr(mojom::blink::BlobURLTokenPtrInfo(
          blob_url_token.PassInterface().PassHandle(),
          mojom::blink::BlobURLToken::Version_)));
}

void SharedWorkerRepositoryClient::ContextDestroyed(ExecutionContext*) {
  DCHECK(IsMainThread());
  // Close mojo connections which will signal disinterest in the associated
  // shared worker.
  client_set_.CloseAllBindings();
}

void SharedWorkerRepositoryClient::Trace(Visitor* visitor) {
  Supplement<Document>::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink

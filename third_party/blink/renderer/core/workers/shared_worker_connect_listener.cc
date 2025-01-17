// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/shared_worker_connect_listener.h"

#include "base/logging.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/workers/shared_worker.h"

namespace blink {

SharedWorkerConnectListener::SharedWorkerConnectListener(SharedWorker* worker)
    : worker_(worker) {}

SharedWorkerConnectListener::~SharedWorkerConnectListener() {
  // We have lost our connection to the worker. If this happens before
  // OnConnected() is called, then it suggests that the document is gone or
  // going away.
}

void SharedWorkerConnectListener::OnCreated(
    mojom::SharedWorkerCreationContextType creation_context_type) {
  worker_->SetIsBeingConnected(true);

  // No nested workers (for now) - connect() can only be called from a
  // document context.
  DCHECK(worker_->GetExecutionContext()->IsDocument());
  DCHECK_EQ(creation_context_type,
            worker_->GetExecutionContext()->IsSecureContext()
                ? mojom::SharedWorkerCreationContextType::kSecure
                : mojom::SharedWorkerCreationContextType::kNonsecure);
}

void SharedWorkerConnectListener::OnConnected(
    const Vector<mojom::WebFeature>& features_used) {
  worker_->SetIsBeingConnected(false);
  for (auto feature : features_used)
    OnFeatureUsed(feature);
}

void SharedWorkerConnectListener::OnScriptLoadFailed() {
  worker_->DispatchEvent(*Event::CreateCancelable(event_type_names::kError));
  worker_->SetIsBeingConnected(false);
}

void SharedWorkerConnectListener::OnFeatureUsed(mojom::WebFeature feature) {
  UseCounter::Count(worker_->GetExecutionContext(), feature);
}

}  // namespace blink

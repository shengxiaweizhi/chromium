/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_SHARED_WORKER_REPOSITORY_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_SHARED_WORKER_REPOSITORY_CLIENT_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/bindings/strong_binding_set.h"
#include "third_party/blink/public/mojom/blob/blob_url_store.mojom-blink.h"
#include "third_party/blink/public/mojom/worker/shared_worker_client.mojom-blink.h"
#include "third_party/blink/public/mojom/worker/shared_worker_connector.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class MessagePortChannel;
class KURL;
class SharedWorker;

// This manages connections with SharedWorkerServiceImpl in the browser process.
// This is owned by Document on the main thread.
// TODO(nhiroki): Rename this class because SharedWorkerRepository doesn't
// exist.
class CORE_EXPORT SharedWorkerRepositoryClient final
    : public GarbageCollectedFinalized<SharedWorkerRepositoryClient>,
      public Supplement<Document>,
      public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(SharedWorkerRepositoryClient);

 public:
  static const char kSupplementName[];
  static SharedWorkerRepositoryClient* From(Document& document);

  explicit SharedWorkerRepositoryClient(Document&);
  virtual ~SharedWorkerRepositoryClient() = default;

  // Establishes a connection with SharedWorkerServiceImpl in the browser
  // process.
  void Connect(SharedWorker*,
               MessagePortChannel,
               const KURL&,
               mojom::blink::BlobURLTokenPtr,
               const String& name);

  // Overrides ContextLifecycleObserver.
  void ContextDestroyed(ExecutionContext*) override;

  void Trace(Visitor* visitor) override;

 private:
  mojom::blink::SharedWorkerConnectorPtr connector_;
  mojo::StrongBindingSet<mojom::blink::SharedWorkerClient> client_set_;

  DISALLOW_COPY_AND_ASSIGN(SharedWorkerRepositoryClient);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_SHARED_WORKER_REPOSITORY_CLIENT_H_

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/command_buffer_direct.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "gpu/command_buffer/service/transfer_buffer_manager.h"

namespace gpu {

namespace {

uint64_t g_next_command_buffer_id = 1;

}  // anonymous namespace

CommandBufferDirect::CommandBufferDirect(
    TransferBufferManager* transfer_buffer_manager)
    : service_(this, transfer_buffer_manager),
      command_buffer_id_(
          CommandBufferId::FromUnsafeValue(g_next_command_buffer_id++)) {}

CommandBufferDirect::~CommandBufferDirect() = default;

CommandBuffer::State CommandBufferDirect::GetLastState() {
  service_.UpdateState();
  return service_.GetState();
}

CommandBuffer::State CommandBufferDirect::WaitForTokenInRange(int32_t start,
                                                              int32_t end) {
  State state = GetLastState();
  DCHECK(state.error != error::kNoError || InRange(start, end, state.token));
  return state;
}

CommandBuffer::State CommandBufferDirect::WaitForGetOffsetInRange(
    uint32_t set_get_buffer_count,
    int32_t start,
    int32_t end) {
  State state = GetLastState();
  DCHECK(state.error != error::kNoError ||
         (InRange(start, end, state.get_offset) &&
          (set_get_buffer_count == state.set_get_buffer_count)));
  return state;
}

void CommandBufferDirect::Flush(int32_t put_offset) {
  DCHECK(handler_);
  service_.Flush(put_offset, handler_);
}

void CommandBufferDirect::OrderingBarrier(int32_t put_offset) {
  Flush(put_offset);
}

void CommandBufferDirect::SetGetBuffer(int32_t transfer_buffer_id) {
  service_.SetGetBuffer(transfer_buffer_id);
}

scoped_refptr<Buffer> CommandBufferDirect::CreateTransferBuffer(size_t size,
                                                                int32_t* id) {
  return service_.CreateTransferBuffer(size, id);
}

void CommandBufferDirect::DestroyTransferBuffer(int32_t id) {
  service_.DestroyTransferBuffer(id);
}

CommandBufferServiceClient::CommandBatchProcessedResult
CommandBufferDirect::OnCommandBatchProcessed() {
  return kContinueExecution;
}

void CommandBufferDirect::OnParseError() {}

void CommandBufferDirect::OnConsoleMessage(int32_t id,
                                           const std::string& message) {}

void CommandBufferDirect::CacheShader(const std::string& key,
                                      const std::string& shader) {}

void CommandBufferDirect::OnFenceSyncRelease(uint64_t release) {
  NOTIMPLEMENTED();
}

void CommandBufferDirect::OnDescheduleUntilFinished() {
  service_.SetScheduled(false);
}

void CommandBufferDirect::OnRescheduleAfterFinished() {
  service_.SetScheduled(true);
}

void CommandBufferDirect::OnSwapBuffers(uint64_t swap_id, uint32_t flags) {}

scoped_refptr<Buffer> CommandBufferDirect::CreateTransferBufferWithId(
    size_t size,
    int32_t id) {
  return service_.CreateTransferBufferWithId(size, id);
}

}  // namespace gpu

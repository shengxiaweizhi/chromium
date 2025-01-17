// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

goog.require('mojo.interfaceControl');
goog.require('mojo.internal');

goog.provide('mojo.internal.interfaceSupport');


/**
 * Handles incoming interface control messages on a proxy or router endpoint.
 */
mojo.internal.interfaceSupport.ControlMessageHandler = class {
  /** @param {!MojoHandle} handle */
  constructor(handle) {
    /** @private {!MojoHandle} */
    this.handle_ = handle;

    /** @private {!Map<number, function()>} */
    this.pendingFlushResolvers_ = new Map;
  }

  sendRunMessage(requestId, input) {
    return new Promise(resolve => {
      mojo.internal.serializeAndSendMessage(
          this.handle_, mojo.interfaceControl.kRunMessageId, requestId,
          mojo.internal.kMessageFlagExpectsResponse,
          mojo.interfaceControl.RunMessageParams.$, {'input': input});
      this.pendingFlushResolvers_.set(requestId, resolve);
    });
  }

  maybeHandleControlMessage(header, buffer) {
    if (header.ordinal === mojo.interfaceControl.kRunMessageId) {
      const data = new DataView(buffer, header.headerSize);
      const decoder = new mojo.internal.Decoder(data, []);
      if (header.flags & mojo.internal.kMessageFlagExpectsResponse)
        return this.handleRunRequest_(header.requestId, decoder);
      else
        return this.handleRunResponse_(header.requestId, decoder);
    }

    return false;
  }

  handleRunRequest_(requestId, decoder) {
    const input = decoder.decodeStructInline(
        mojo.interfaceControl.RunMessageParams.$.$.structSpec)['input'];
    if (input.hasOwnProperty('flushForTesting')) {
      mojo.internal.serializeAndSendMessage(
          this.handle_, mojo.interfaceControl.kRunMessageId, requestId,
          mojo.internal.kMessageFlagIsResponse,
          mojo.interfaceControl.RunResponseMessageParams.$, {'output': null});
      return true;
    }

    return false;
  }

  handleRunResponse_(requestId, decoder) {
    const resolver = this.pendingFlushResolvers_.get(requestId);
    if (!resolver)
      return false;

    resolver();
    return true;
  }
};

/**
 * Captures metadata about a request which was sent by a local proxy, for which
 * a response is expected.
 */
mojo.internal.interfaceSupport.PendingResponse = class {
  /**
   * @param {number} requestId
   * @param {number} ordinal
   * @param {!mojo.internal.MojomType} responseStruct
   * @param {!Function} resolve
   * @param {!Function} reject
   * @private
   */
  constructor(requestId, ordinal, responseStruct, resolve, reject) {
    /** @public {number} */
    this.requestId = requestId;

    /** @public {number} */
    this.ordinal = ordinal;

    /** @public {!mojo.internal.MojomType} */
    this.responseStruct = responseStruct;

    /** @public {!Function} */
    this.resolve = resolve;

    /** @public {!Function} */
    this.reject = reject;
  }
};

/**
 * Generic helper used to implement all generated proxy classes. Knows how to
 * serialize requests and deserialize their replies, both according to
 * declarative message structure specs.
 * @export
 */
mojo.internal.interfaceSupport.InterfaceProxyBase = class {
  /**
   * @param {MojoHandle=} opt_handle The message pipe handle to use as a proxy
   *     endpoint. If null, this object must be bound with bindHandle before
   *     it can be used to send any messages.
   * @public
   */
  constructor(opt_handle) {
    /** @public {?MojoHandle} */
    this.handle = null;

    /** @private {?mojo.internal.interfaceSupport.HandleReader} */
    this.reader_ = null;

    /** @private {number} */
    this.nextRequestId_ = 0;

    /**
     * @private {!Map<number, !mojo.internal.interfaceSupport.PendingResponse>}
     */
    this.pendingResponses_ = new Map;

    /** @private {mojo.internal.interfaceSupport.ControlMessageHandler} */
    this.controlMessageHandler_ = null;

    if (opt_handle instanceof MojoHandle)
      this.bindHandle(opt_handle);
  }

  /**
   * @param {!MojoHandle} handle
   * @export
   */
  bindHandle(handle) {
    if (this.handle)
      throw new Error('Proxy already bound.');
    this.handle = handle;

    const reader = new mojo.internal.interfaceSupport.HandleReader(handle);
    reader.onRead = this.onMessageReceived_.bind(this);
    reader.onError = this.onError_.bind(this);
    reader.start();
    this.controlMessageHandler_ =
        new mojo.internal.interfaceSupport.ControlMessageHandler(handle);

    this.reader_ = reader;
    this.nextRequestId_ = 0;
    this.pendingResponses_ = new Map;
  }

  /** @export */
  unbind() {
    if (this.reader_)
      this.reader_.stop();
  }

  /**
   * @param {number} ordinal
   * @param {!mojo.internal.MojomType} paramStruct
   * @param {?mojo.internal.MojomType} responseStruct
   * @param {!Array} args
   * @return {!Promise}
   * @export
   */
  sendMessage(ordinal, paramStruct, responseStruct, args) {
    if (!this.handle) {
      throw new Error(
          'Attempting to use an unbound proxy. Try createRequest() first.')
    }

    // The pipe has already been closed, so just drop the message.
    if (!this.reader_ || this.reader_.isStopped())
      return Promise.reject();

    const requestId = this.nextRequestId_++;
    const value = {};
    paramStruct.$.structSpec.fields.forEach(
        (field, index) => value[field.name] = args[index]);
    mojo.internal.serializeAndSendMessage(
        this.handle, ordinal, requestId,
        responseStruct ? mojo.internal.kMessageFlagExpectsResponse : 0,
        paramStruct, value);
    if (!responseStruct)
      return Promise.resolve();

    return new Promise((resolve, reject) => {
      this.pendingResponses_.set(
          requestId,
          new mojo.internal.interfaceSupport.PendingResponse(
              requestId, ordinal,
              /** @type {!mojo.internal.MojomType} */ (responseStruct), resolve,
              reject));
    });
  }

  /**
   * @return {!Promise}
   * @export
   */
  flushForTesting() {
    return this.controlMessageHandler_.sendRunMessage(
        this.nextRequestId_++, {'flushForTesting': {}});
  }

  /**
   * @param {!ArrayBuffer} buffer
   * @param {!Array<MojoHandle>} handles
   * @private
   */
  onMessageReceived_(buffer, handles) {
    const data = new DataView(buffer);
    const header = mojo.internal.deserializeMessageHeader(data);
    if (this.controlMessageHandler_.maybeHandleControlMessage(header, buffer))
      return;
    if (!(header.flags & mojo.internal.kMessageFlagIsResponse) ||
        header.flags & mojo.internal.kMessageFlagExpectsResponse) {
      return this.onError_('Received unexpected request message');
    }
    const pendingResponse = this.pendingResponses_.get(header.requestId);
    this.pendingResponses_.delete(header.requestId);
    if (!pendingResponse)
      return this.onError_('Received unexpected response message');
    const decoder = new mojo.internal.Decoder(
        new DataView(buffer, header.headerSize), handles);
    const responseValue = decoder.decodeStructInline(
        /** @type {!mojo.internal.StructSpec} */ (
            pendingResponse.responseStruct.$.structSpec));
    if (!responseValue)
      return this.onError_('Received malformed response message');
    if (header.ordinal !== pendingResponse.ordinal)
      return this.onError_('Received malformed response message');

    pendingResponse.resolve(responseValue);
  }

  /**
   * @param {string=} opt_reason
   * @private
   */
  onError_(opt_reason) {
    this.reader_.stopAndCloseHandle();
    this.reader_ = null;
    for (const id of this.pendingResponses_.keys())
      this.pendingResponses_.get(id).reject(new Error(opt_reason));
    this.pendingResponses_ = new Map;
  }
};

/**
 * Helper used by generated EventRouter types to dispatch incoming interface
 * messages as Event-like things.
 * @export
 */
mojo.internal.interfaceSupport.CallbackRouter = class {
  constructor() {
    /** @type {!Map<number, !Function>} */
    this.removeCallbacks = new Map;

    /** @private {number} */
    this.nextListenerId_ = 0;
  }

  /** @return {number} */
  getNextId() {
    return ++this.nextListenerId_;
  }

  /**
   * @param {number} id An ID returned by a prior call to addListener.
   * @return {boolean} True iff the identified listener was found and removed.
   * @export
   */
  removeListener(id) {
    this.removeCallbacks.get(id)();
    return this.removeCallbacks.delete(id);
  }
};

/**
 * Helper used by generated CallbackRouter types to dispatch incoming interface
 * messages to listeners.
 * @export
 */
mojo.internal.interfaceSupport.InterfaceCallbackTarget = class {
  /**
   * @public
   * @param {!mojo.internal.interfaceSupport.CallbackRouter} callbackRouter
   */
  constructor(callbackRouter) {
    /** @private {!Map<number, !Function>} */
    this.listeners_ = new Map;

    /** @private {!mojo.internal.interfaceSupport.CallbackRouter} */
    this.callbackRouter_ = callbackRouter;
  }

  /**
   * @param {!Function} listener
   * @return {number} A unique ID for the added listener.
   * @export
   */
  addListener(listener) {
    const id = this.callbackRouter_.getNextId();
    this.listeners_.set(id, listener);
    this.callbackRouter_.removeCallbacks.set(id, () => {
      return this.listeners_.delete(id);
    });
    return id;
  }

  /**
   * @param {boolean} expectsResponse
   * @return {!Function}
   * @export
   */
  createTargetHandler(expectsResponse) {
    if (expectsResponse)
      return this.dispatchWithResponse_.bind(this);
    return this.dispatch_.bind(this);
  }

  /**
   * @param {...*} varArgs
   * @private
   */
  dispatch_(varArgs) {
    const args = Array.from(arguments);
    this.listeners_.forEach(listener => listener.apply(null, args));
  }

  /**
   * @param {...*} varArgs
   * @return {?Object}
   * @private
   */
  dispatchWithResponse_(varArgs) {
    const args = Array.from(arguments);
    const returnValues = Array.from(this.listeners_.values())
                             .map(listener => listener.apply(null, args));

    let returnValue;
    for (const value of returnValues) {
      if (value === undefined)
        continue;
      if (returnValue !== undefined)
        throw new Error('Multiple listeners attempted to reply to a message');
      returnValue = value;
    }

    return returnValue;
  }
};

/**
 * Wraps message handlers attached to an InterfaceTarget.
 */
mojo.internal.interfaceSupport.MessageHandler = class {
  /**
   * @param {!mojo.internal.MojomType} paramStruct
   * @param {?mojo.internal.MojomType} responseStruct
   * @param {!Function} handler
   * @private
   */
  constructor(paramStruct, responseStruct, handler) {
    /** @public {!mojo.internal.MojomType} */
    this.paramStruct = paramStruct;

    /** @public {?mojo.internal.MojomType} */
    this.responseStruct = responseStruct;

    /** @public {!Function} */
    this.handler = handler;
  }
};

/**
 * Listens for incoming request messages on a message pipe, dispatching them to
 * any registered handlers. Handlers are registered against a specific ordinal
 * message number.
 * @export
 */
mojo.internal.interfaceSupport.InterfaceTarget = class {
  /** @public */
  constructor() {
    /**
     * @private {!Map<MojoHandle,
     *     !mojo.internal.interfaceSupport.HandleReader>}
     */
    this.readers_ = new Map;

    /**
     * @private {!Map<number, !mojo.internal.interfaceSupport.MessageHandler>}
     */
    this.messageHandlers_ = new Map;

    /** @private {mojo.internal.interfaceSupport.ControlMessageHandler} */
    this.controlMessageHandler_ = null;
  }

  /**
   * @param {number} ordinal
   * @param {!mojo.internal.MojomType} paramStruct
   * @param {?mojo.internal.MojomType} responseStruct
   * @param {!Function} handler
   * @export
   */
  registerHandler(ordinal, paramStruct, responseStruct, handler) {
    this.messageHandlers_.set(
        ordinal,
        new mojo.internal.interfaceSupport.MessageHandler(
            paramStruct, responseStruct, handler));
  }

  /**
   * @param {!MojoHandle} handle
   * @export
   */
  bindHandle(handle) {
    const reader = new mojo.internal.interfaceSupport.HandleReader(handle);
    this.readers_.set(handle, reader);
    reader.onRead = this.onMessageReceived_.bind(this, handle);
    reader.onError = this.onError_.bind(this, handle);
    reader.start();
    this.controlMessageHandler_ =
        new mojo.internal.interfaceSupport.ControlMessageHandler(handle);
  }

  /**
   * @param {!MojoHandle} handle
   * @param {!ArrayBuffer} buffer
   * @param {!Array<MojoHandle>} handles
   * @private
   */
  onMessageReceived_(handle, buffer, handles) {
    const data = new DataView(buffer);
    const header = mojo.internal.deserializeMessageHeader(data);
    if (this.controlMessageHandler_.maybeHandleControlMessage(header, buffer))
      return;
    if (header.flags & mojo.internal.kMessageFlagIsResponse)
      throw new Error('Received unexpected response on interface target');
    const handler = this.messageHandlers_.get(header.ordinal);
    if (!handler)
      throw new Error('Received unknown message');
    const decoder = new mojo.internal.Decoder(
        new DataView(buffer, header.headerSize), handles);
    const request = decoder.decodeStructInline(
        /** @type {!mojo.internal.StructSpec} */ (
            handler.paramStruct.$.structSpec));
    if (!request)
      throw new Error('Received malformed message');

    let result = handler.handler.apply(
        null,
        handler.paramStruct.$.structSpec.fields.map(
            field => request[field.name]));

    // If the message expects a response, the handler must return either a
    // well-formed response object, or a Promise that will eventually yield one.
    if (handler.responseStruct) {
      if (result === undefined) {
        this.onError_(handle);
        throw new Error(
            'Message expects a reply but its handler did not provide one.');
      }

      if (!(result instanceof Promise))
        result = Promise.resolve(result);

      result
          .then(value => {
            mojo.internal.serializeAndSendMessage(
                handle, header.ordinal, header.requestId,
                mojo.internal.kMessageFlagIsResponse,
                /** @type {!mojo.internal.MojomType} */
                (handler.responseStruct), value);
          })
          .catch(() => {
            // If the handler rejects, that means it didn't like the request's
            // contents for whatever reason. We close the binding to prevent
            // further messages from being received from that client.
            this.onError_(handle);
          });
    }
  }

  /**
   * @param {!MojoHandle} handle
   * @private
   */
  onError_(handle) {
    const reader = this.readers_.get(handle);
    if (!reader)
      return;
    reader.stopAndCloseHandle();
    this.readers_.delete(handle);
  }
};

/**
 * Watches a MojoHandle for readability or peer closure, forwarding either event
 * to one of two callbacks on the reader. Used by both InterfaceProxyBase and
 * InterfaceTarget to watch for incoming messages.
 */
mojo.internal.interfaceSupport.HandleReader = class {
  /**
   * @param {!MojoHandle} handle
   * @private
   */
  constructor(handle) {
    /** @private {MojoHandle} */
    this.handle_ = handle;

    /** @public {?function(!ArrayBuffer, !Array<MojoHandle>)} */
    this.onRead = null;

    /** @public {!Function} */
    this.onError = () => {};

    /** @public {?MojoWatcher} */
    this.watcher_ = null;
  }

  isStopped() {
    return this.watcher_ === null;
  }

  start() {
    this.watcher_ = this.handle_.watch({readable: true}, this.read_.bind(this));
  }

  stop() {
    if (!this.watcher_)
      return;
    this.watcher_.cancel();
    this.watcher_ = null;
  }

  stopAndCloseHandle() {
    this.stop();
    this.handle_.close();
  }

  /** @private */
  read_(result) {
    for (;;) {
      if (!this.watcher_)
        return;

      const read = this.handle_.readMessage();

      // No messages available.
      if (read.result == Mojo.RESULT_SHOULD_WAIT)
        return;

      // Remote endpoint has been closed *and* no messages available.
      if (read.result == Mojo.RESULT_FAILED_PRECONDITION) {
        this.onError();
        return;
      }

      // Something terrible happened.
      if (read.result != Mojo.RESULT_OK)
        throw new Error('Unexpected error on HandleReader: ' + read.result);

      this.onRead(read.buffer, read.handles);
    }
  }
};

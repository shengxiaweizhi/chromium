// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router.caf;

import android.support.annotation.Nullable;

import com.google.android.gms.cast.CastDevice;
import com.google.android.gms.cast.framework.CastSession;
import com.google.android.gms.cast.framework.media.RemoteMediaClient;

import org.chromium.base.Log;
import org.chromium.chrome.browser.media.router.CastSessionUtil;
import org.chromium.chrome.browser.media.router.FlingingController;
import org.chromium.chrome.browser.media.router.MediaSink;
import org.chromium.chrome.browser.media.router.MediaSource;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

/**
 * A base wrapper for {@link CastSession}, extending its functionality for Chrome MediaRouter.
 *
 * Has persistent lifecycle and always attaches itself to the current {@link CastSession}.
 */
public abstract class BaseSessionController {
    private static final String TAG = "BaseSessionCtrl";

    /** Callback class for listening to state changes. */
    public static interface Callback {
        /** Called when session started. */
        void onSessionStarted();

        /** Called when session ended. */
        void onSessionEnded();

        /** Called when status updated. */
        void onStatusUpdated();

        /** Called when metadata updated. */
        void onMetadataUpdated();
    }

    private CastSession mCastSession;
    private final CafBaseMediaRouteProvider mProvider;
    private CreateRouteRequestInfo mRouteCreationInfo;
    private final RemoteMediaClient.Callback mRemoteMediaClientCallback;
    private final List<WeakReference<Callback>> mCallbacks = new ArrayList<>();

    public BaseSessionController(CafBaseMediaRouteProvider provider) {
        mProvider = provider;
        mRemoteMediaClientCallback = new RemoteMediaClientCallback();
    }

    public void addCallback(Callback callback) {
        mCallbacks.add(new WeakReference<>(callback));
    }

    public void removeCallback(Callback callback) {
        Iterator<WeakReference<Callback>> iterator = mCallbacks.iterator();

        while (iterator.hasNext()) {
            if (iterator.next().get() == callback) {
                iterator.remove();
            }
        }
    }

    public void requestSessionLaunch() {
        mRouteCreationInfo = mProvider.getPendingCreateRouteRequestInfo();
        CastUtils.getCastContext().setReceiverApplicationId(
                mRouteCreationInfo.source.getApplicationId());

        // When the user clicks a route on the MediaRouteChooserDialog, we intercept the click event
        // and do not select the route. Instead the route selection is postponed to here. This will
        // trigger CAF to launch the session.
        mRouteCreationInfo.routeInfo.select();
    }

    public MediaSource getSource() {
        return (mRouteCreationInfo != null) ? mRouteCreationInfo.source : null;
    }

    public MediaSink getSink() {
        return (mRouteCreationInfo != null) ? mRouteCreationInfo.sink : null;
    }

    public CreateRouteRequestInfo getRouteCreationInfo() {
        return mRouteCreationInfo;
    }

    public CastSession getSession() {
        return mCastSession;
    }

    public RemoteMediaClient getRemoteMediaClient() {
        return isConnected() ? mCastSession.getRemoteMediaClient() : null;
    }

    public abstract BaseNotificationController getNotificationController();

    public void endSession() {
        CastUtils.getCastContext().getSessionManager().endCurrentSession(/* stopCasting= */ true);
        CastUtils.getCastContext().setReceiverApplicationId(null);
    }

    public List<String> getCapabilities() {
        List<String> capabilities = new ArrayList<>();
        if (mCastSession == null || !mCastSession.isConnected()) return capabilities;
        CastDevice device = mCastSession.getCastDevice();
        if (device.hasCapability(CastDevice.CAPABILITY_AUDIO_IN)) {
            capabilities.add("audio_in");
        }
        if (device.hasCapability(CastDevice.CAPABILITY_AUDIO_OUT)) {
            capabilities.add("audio_out");
        }
        if (device.hasCapability(CastDevice.CAPABILITY_VIDEO_IN)) {
            capabilities.add("video_in");
        }
        if (device.hasCapability(CastDevice.CAPABILITY_VIDEO_OUT)) {
            capabilities.add("video_out");
        }
        return capabilities;
    }

    public boolean isConnected() {
        return mCastSession != null && mCastSession.isConnected();
    }

    private void updateRemoteMediaClient(String message) {
        if (!isConnected()) return;

        mCastSession.getRemoteMediaClient().onMessageReceived(
                mCastSession.getCastDevice(), CastSessionUtil.MEDIA_NAMESPACE, message);
    }

    /** Attaches the controller to the current {@link CastSession}. */
    public void attachToCastSession(CastSession session) {
        mCastSession = session;
        RemoteMediaClient uncheckedRemoteMediaClient = mCastSession.getRemoteMediaClient();
        if (uncheckedRemoteMediaClient != null) {
            uncheckedRemoteMediaClient.registerCallback(mRemoteMediaClientCallback);
        }
    }

    /** Detaches the controller from any {@link CastSession}. */
    public void detachFromCastSession() {
        if (mCastSession == null) return;

        RemoteMediaClient uncheckedRemoteMediaClient = mCastSession.getRemoteMediaClient();
        if (uncheckedRemoteMediaClient != null) {
            uncheckedRemoteMediaClient.unregisterCallback(mRemoteMediaClientCallback);
        }
        mCastSession = null;
    }

    /** Called when session started. */
    public void onSessionStarted() {
        notifyCallback((Callback callback) -> callback.onSessionStarted());
    }

    /** Called when session ended. */
    public void onSessionEnded() {
        notifyCallback((Callback callback) -> callback.onSessionEnded());
    }

    protected final CafBaseMediaRouteProvider getProvider() {
        return mProvider;
    }

    /**
     * All sub-classes need to register this method to listen to messages of the namespaces they are
     * interested in.
     */
    protected void onMessageReceived(CastDevice castDevice, String namespace, String message) {
        Log.d(TAG,
                "Received message from Cast device: namespace=\"" + namespace + "\" message=\""
                        + message + "\"");
        if (CastSessionUtil.MEDIA_NAMESPACE.equals(namespace)) {
            updateRemoteMediaClient(message);
        }
    }

    private class RemoteMediaClientCallback extends RemoteMediaClient.Callback {
        @Override
        public void onStatusUpdated() {
            BaseSessionController.this.onStatusUpdated();
        }

        @Override
        public void onMetadataUpdated() {
            BaseSessionController.this.onMetadataUpdated();
        }
    }

    protected void onStatusUpdated() {
        notifyCallback((Callback callback) -> callback.onStatusUpdated());
    }

    protected void onMetadataUpdated() {
        notifyCallback((Callback callback) -> callback.onMetadataUpdated());
    }

    @Nullable
    public FlingingController getFlingingController() {
        return null;
    }

    /**
     *  Helper message to get the session ID of the attached session. For stubbing in tests as
     * {@link CastSession#getSessionId()} is final.
     */
    public String getSessionId() {
        return isConnected() ? getSession().getSessionId() : null;
    }

    private void notifyCallback(NotifyCallbackAction action) {
        Iterator<WeakReference<Callback>> iterator = mCallbacks.iterator();

        while (iterator.hasNext()) {
            Callback callback = iterator.next().get();
            if (callback == null) {
                iterator.remove();
            } else {
                action.notify(callback);
            }
        }
    }

    private interface NotifyCallbackAction { void notify(Callback callback); }
}

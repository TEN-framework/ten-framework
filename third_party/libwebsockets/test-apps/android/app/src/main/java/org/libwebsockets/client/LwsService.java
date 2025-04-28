/*
 * LwsService.java - libwebsockets test service for Android
 *
 * Copyright (C) 2016 Alexander Bruines <alexander.bruines@gmail.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * The person who associated a work with this deed has dedicated
 * the work to the public domain by waiving all of his or her rights
 * to the work worldwide under copyright law, including all related
 * and neighboring rights, to the extent allowed by law. You can copy,
 * modify, distribute and perform the work, even for commercial purposes,
 * all without asking permission.
 *
 * The test apps are intended to be adapted for use in your code, which
 * may be proprietary.  So unlike the library itself, they are licensed
 * Public Domain.
 */

package org.libwebsockets.client;

import android.os.Message;
import android.os.RemoteException;
import android.util.Log;

public class LwsService extends ThreadService {

    /**
     * Commands that can be send to this service
     */
    public final static int MSG_SET_CONNECTION_PARAMETERS = 1;

    /**
     * Messages that may be send to output Messenger
     * Clients should handle these messages.
     **/
    public final static int MSG_DUMB_INCREMENT_PROTOCOL_COUNTER = 1;
    public final static int MSG_LWS_CALLBACK_CLIENT_CONNECTION_ERROR = 2;
    public final static int MSG_LWS_CALLBACK_CLIENT_ESTABLISHED = 3;

    public static class ConnectionParameters {
        String serverAddress;
        int serverPort;

        ConnectionParameters(
                String serverAddress,
                int serverPort
        ){
            this.serverAddress = serverAddress;
            this.serverPort = serverPort;
        }
    }

    /**
     * Handle incoming messages from clients of this service
     */
    @Override
    public void handleInputMessage(Message msg) {
        Message m;
        switch(msg.what) {
            case MSG_SET_CONNECTION_PARAMETERS: {
                LwsService.ConnectionParameters parameters = (ConnectionParameters) msg.obj;
                setConnectionParameters(
                        parameters.serverAddress,
                        parameters.serverPort
                );
                break;
            }
            default:
                super.handleInputMessage(msg);
                break;
        }
    }

    /**
     *  The run() function for the thread.
     *  For this test we implement a very long lived task
     *  that sends many messages back to the client.
     *  **/
    public void workerThreadRun() {

        initLws();
        connectLws();

        while(true) {

            // service the websockets
            serviceLws();

            // Check if we must quit or suspend
            synchronized (mThreadLock){
                while(mMustSuspend) {
                    // We are asked to suspend the thread
                    try {
                        mThreadLock.wait();

                    } catch (InterruptedException e) {}
                }
                if(mMustQuit) {
                    // The signal to quit was given
                    break;
                }
            }

            // Throttle the loop so that it iterates once every 50ms
            try {
                Thread.sleep(50);
            }
            catch (InterruptedException e) {
                e.printStackTrace();
            }

        }
        exitLws();
    }

    /** Load the native libwebsockets code */
    static {
        try {
            System.loadLibrary("lwsservice");
        }
        catch(UnsatisfiedLinkError ule) {
            Log.e("LwsService", "Warning: Could not load native library: " + ule.getMessage());
        }
    }
    public native boolean initLws();
    public native void exitLws();
    public native void serviceLws();
    public native void setConnectionParameters(String serverAddress, int serverPort);
    public native boolean connectLws();
}

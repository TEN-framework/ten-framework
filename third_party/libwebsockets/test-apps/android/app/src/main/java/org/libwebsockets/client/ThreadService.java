/*
 * ThreadService.java - libwebsockets test service for Android
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

import android.app.Service;
import android.content.Intent;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.Messenger;
import android.os.RemoteException;
import android.util.Log;

import java.lang.ref.WeakReference;

public abstract class ThreadService extends Service {
    /** Messages that can be send to the Service: **/
    public final static int MSG_SET_OUTPUT_HANDLER  = 1001;
    public final static int MSG_THREAD_START        = 1002;
    public final static int MSG_THREAD_STOP         = 1003;
    public final static int MSG_THREAD_SUSPEND      = 1004;
    public final static int MSG_THREAD_RESUME       = 1005;

    /**
     * Messages that may be send from the Service
     * (Clients should handle these messages)
     **/
    public final static int MSG_THREAD_STARTED      = 2001;
    public final static int MSG_THREAD_STOPPED      = 2002;
    public final static int MSG_THREAD_SUSPENDED    = 2003;
    public final static int MSG_THREAD_RESUMED      = 2004;

    /** Data accessed by both worker thread and the UI-thread must be synchronized **/
    public final Object mThreadLock = new Object();;
    public volatile boolean mMustQuit;
    public volatile boolean mWorkThreadIsRunning;
    public volatile boolean mMustSuspend;

    /** Handler for incoming messages **/
    public static class InputHandler extends Handler {
        private final WeakReference<ThreadService> mService;
        InputHandler(ThreadService service) {
            mService = new WeakReference<ThreadService>(service);
        }
        @Override
        public void handleMessage(Message msg) {
            ThreadService service = mService.get();
            if(service != null) {
                service.handleInputMessage(msg);
            }
        }
    }

    /**
     * Interface and Handler for outgoing messages to clients of this service.
     * (Must be implemented by the client.)
     */
    public interface OutputInterface {
        void handleOutputMessage(Message message);
    }
    public static class OutputHandler extends Handler {
        // Notice that we do NOT use a WeakReference here
        // (If we did the service would lose mOutputMessenger the moment
        // that garbage collection is performed by the Java VM)
        private final OutputInterface mInterface;
        OutputHandler(OutputInterface object) {
            mInterface = object;
        }
        @Override
        public void handleMessage(Message msg) {
            mInterface.handleOutputMessage(msg);
        }
    }

    /** The Messengers used to communicate with the clients of this service **/
    public final Messenger mInputMessenger = new Messenger(new InputHandler(this));
    public Messenger mOutputMessenger;

    /** The worker thread and its runnable **/
    public static class WorkerThreadRunnable implements Runnable {
        private final WeakReference<ThreadService> mService;
        WorkerThreadRunnable(ThreadService service){
            mService = new WeakReference<ThreadService>(service);
        }
        @Override
        public void run() {
            ThreadService service = mService.get();
            if(service != null) {
                service.mWorkThreadIsRunning = true;
                service.workerThreadRun();
                service.mWorkThreadIsRunning = false;
            }
        }
    }
    public Thread mWorkerThread;

    /** Handle incoming messages from the client **/
    public void handleInputMessage(Message msg) {
        try {
            Message m;
            switch(msg.what) {
                case MSG_SET_OUTPUT_HANDLER:
                    // set the output messenger then
                    // send a message indicating the thread status
                    mOutputMessenger = msg.replyTo;
                    break;
                case MSG_THREAD_START:
                    try {
                        // reset thread vars
                        synchronized (mThreadLock) {
                            // thread allready running?
                            if(!mWorkThreadIsRunning){
                                // no, start it
                                mMustQuit = false;
                                mMustSuspend = false;
                                mWorkerThread = new Thread(new WorkerThreadRunnable(this));
                                mWorkerThread.start();
                            }
                            else {
                                // yes, resume it
                                mMustQuit = false;
                                mMustSuspend = false;
                                mThreadLock.notifyAll();
                            }
                        }
                    }
                    catch(NullPointerException e) {
                        e.printStackTrace();
                    }
                    if(mOutputMessenger != null) {
                        m = Message.obtain(null, MSG_THREAD_STARTED, 0, 0);
                        mOutputMessenger.send(m);
                    }
                    break;
                case MSG_THREAD_STOP:
                    try {
                        synchronized(mThreadLock) {
                            if(mWorkThreadIsRunning) {
                                mMustQuit = true;
                                mMustSuspend = false;
                                mThreadLock.notifyAll();
                            }
                        }
                        mWorkerThread.join();
                    }
                    catch(InterruptedException e) {
                        Log.e("ThreadService","handleInputMessage join() interrupted");
                    }
                    if(mOutputMessenger != null) {
                        m = Message.obtain(null, MSG_THREAD_STOPPED, 0, 0);
                        mOutputMessenger.send(m);
                    }
                    break;
                case MSG_THREAD_SUSPEND:
                    synchronized (mThreadLock) {
                        if(mWorkThreadIsRunning) {
                            mMustSuspend = true;
                        }
                    }
                    if(mOutputMessenger != null) {
                        m = Message.obtain(null, MSG_THREAD_SUSPENDED, 0, 0);
                        mOutputMessenger.send(m);
                    }
                    break;
                case MSG_THREAD_RESUME:
                    synchronized (mThreadLock) {
                        if(mWorkThreadIsRunning) {
                            mMustSuspend = false;
                            mThreadLock.notifyAll();
                        }
                    }
                    if(mOutputMessenger != null) {
                        m = Message.obtain(null, MSG_THREAD_RESUMED, 0, 0);
                        mOutputMessenger.send(m);
                    }
                    break;
                default:
                    break;
            }
        }
        catch(RemoteException e) {
            e.printStackTrace();
        }
    }

    /**
     * This can be called from the JNI functions to send output messages to the client
     */
    public void sendMessage(int msg, Object obj){
        Message m = Message.obtain(null, msg, 0, 0);
        m.obj = obj;
        try {
            mOutputMessenger.send(m);
        }
        catch(RemoteException e) {
            e.printStackTrace();
        }
    }

    /** The run() function for the worker thread **/
    public abstract void workerThreadRun();

    /**
     *  Called when the service is being created.
     *  ie. When the first client calls bindService() or startService().
     **/
    @Override
    public void onCreate() {
        super.onCreate();
        // initialize variables
        mWorkThreadIsRunning = false;
        mMustQuit = false;
        mOutputMessenger = null;
        mWorkerThread = null;
    }

    /**
     *  Called when the first client is binding to the service with bindService()
     *
     *  If the service was started with bindService() it will automatically stop when the last
     *  client unbinds from the service. If you want the service to continue running even if it
     *  is not bound to anything then start the service with startService() before
     *  calling bindService(). In this case stopService() must be called after unbinding
     *  to stop the service.
     */
    @Override
    public IBinder onBind(Intent intent) {
        return mInputMessenger.getBinder();
    }

    /** Called if the service is started with startService(). */
    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        return START_STICKY;
    }

    /** Called when the first client is binds to the service with bindService() */
    @Override
    public void onRebind(Intent intent) {}

    /** Called when all clients have unbound with unbindService() */
    @Override
    public boolean onUnbind(Intent intent) {
        //mOutputMessenger = null;
        return false; // do not allow to rebind.
    }

    /** Called when the service is no longer used and is being destroyed */
    @Override
    public void onDestroy() {
        super.onDestroy();
        try {
            if(mWorkThreadIsRunning){
                synchronized(mThreadLock) {
                    mMustQuit = true;
                    mMustSuspend = false;
                    mThreadLock.notifyAll();
                }
                mWorkerThread.join();
            }
        }
        catch(NullPointerException | InterruptedException e) {
            e.printStackTrace();
        }
    }
}

/*
 * MainActivity.java - libwebsockets test service for Android
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


import android.content.ComponentName;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;
import android.os.Message;
import android.os.Messenger;
import android.os.RemoteException;
import android.support.v7.app.AlertDialog;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.view.inputmethod.InputMethodManager;
import android.view.View;
import android.widget.EditText;
import android.widget.TextView;

public class MainActivity extends AppCompatActivity implements LwsService.OutputInterface {

    /** This is the Messenger that handles output from the Service */
    private Messenger mMessenger = null;

    /** The Messenger for sending commands to the Service  */
    private Messenger mService = null;
    private ServiceConnection mServiceConnection = null;

    private boolean mThreadIsRunning = false;
    private boolean mThreadIsSuspended = false;

    private TextView tvCounter;
    private EditText etServer;
    private EditText etPort;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // Get the layout items
        tvCounter = (TextView) findViewById(R.id.textView_counter);
        etServer = (EditText) findViewById(R.id.editText_serverLocation);
        etPort = (EditText) findViewById(R.id.editText_portNumber);

        // Create the Messenger for handling output from the service
        mMessenger = new Messenger(new LwsService.OutputHandler(this));

        // Restore state from the Bundle when restarting due to a device change.
        if(savedInstanceState!=null) {
            mThreadIsRunning = savedInstanceState.getBoolean("mThreadIsRunning");
        }

        mServiceConnection = new ServiceConnection() {
            @Override
            public void onServiceConnected(ComponentName name, IBinder service) {
                mService = new Messenger(service);
                try {
                    // Set the output messenger by starting the thread
                    Message msg = Message.obtain(null, LwsService.MSG_SET_OUTPUT_HANDLER, 0, 0);
                    msg.replyTo = mMessenger;
                    mService.send(msg);
                    if(mThreadIsRunning){
                        // If the thread is already running at this point it means
                        // that the application was restarted after a device change.
                        // This implies that the thread was suspended by the onStop method.
                        msg = Message.obtain(null, LwsService.MSG_THREAD_RESUME, 0, 0);
                        mService.send(msg);
                        mThreadIsSuspended = false;
                    }
                }
                catch(RemoteException e) {
                    e.printStackTrace();
                }
            }

            @Override
            public void onServiceDisconnected(ComponentName name) {
                Log.e("MainActivity","onServiceDisconnected !");
                mService = null;
            }
        };

        if(savedInstanceState==null){
            startService(new Intent(getBaseContext(), LwsService.class));
        }
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        outState.putBoolean("mThreadIsRunning", mThreadIsRunning);
    }

    @Override
    protected void onStart() {
        super.onStart();
        bindService(new Intent(getBaseContext(), LwsService.class), mServiceConnection, Context.BIND_AUTO_CREATE);
    }

    @Override
    protected void onStop() {
        super.onStop();
        if(mThreadIsRunning) {
            if (!mThreadIsSuspended) {
                try {
                    mService.send(Message.obtain(null, LwsService.MSG_THREAD_SUSPEND, 0, 0));
                } catch (RemoteException e) {
                    e.printStackTrace();
                }
                mThreadIsSuspended = true;
            }
        }
        unbindService(mServiceConnection);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if(isFinishing()){
            stopService(new Intent(getBaseContext(), LwsService.class));
        }
    }

    /** Implement the interface to receive output from the LwsService */
    @Override
    public void handleOutputMessage(Message message) {
        switch(message.what) {
            case LwsService.MSG_DUMB_INCREMENT_PROTOCOL_COUNTER:
                tvCounter.setText((String)message.obj);
                break;
            case LwsService.MSG_LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
                connectErrorListener();
                break;
            case LwsService.MSG_LWS_CALLBACK_CLIENT_ESTABLISHED:
                break;
            case LwsService.MSG_THREAD_STARTED:
                // The thread was started
                mThreadIsRunning = true;
                mThreadIsSuspended = false;
                break;
            case LwsService.MSG_THREAD_STOPPED:
                // The thread was stopped
                mThreadIsRunning = false;
                mThreadIsSuspended = false;
                break;
            case LwsService.MSG_THREAD_SUSPENDED:
                // The thread is suspended
                mThreadIsRunning = true;
                mThreadIsSuspended = true;
                break;
            case LwsService.MSG_THREAD_RESUMED:
                // the thread was resumed
                mThreadIsRunning = true;
                mThreadIsSuspended = false;
                break;
            default:
                break;
        }
    }

    private void connectErrorListener(){
        try {
            Message msg;
            if(mThreadIsRunning) {
                msg = Message.obtain(null, LwsService.MSG_THREAD_STOP);
                mService.send(msg);
            }
            AlertDialog.Builder adb = new AlertDialog.Builder(this);
            adb.setTitle("Error");
            adb.setPositiveButton("OK", new DialogInterface.OnClickListener() {
                public void onClick(DialogInterface dialog, int which) {
                }
            });
            adb.setMessage("Could not connect to the server.");
            adb.show();
        }
        catch (RemoteException e){}
    }

    /**
     * Start/Stop Button Handler
     */

    public void clickStart(View v) {
        if(!mThreadIsRunning) {
            View view = this.getCurrentFocus();
            if (view != null) {
                InputMethodManager imm = (InputMethodManager)getSystemService(Context.INPUT_METHOD_SERVICE);
                imm.hideSoftInputFromWindow(view.getWindowToken(), 0);
            }
            mThreadIsRunning = true;
            mThreadIsSuspended = false;
            try {
                Message msg = Message.obtain(null, LwsService.MSG_SET_CONNECTION_PARAMETERS, 0, 0);
                int port = 0;
                if(!etPort.getText().toString().equals("")) // prevent NumberformatException
                    port = Integer.parseInt(etPort.getText().toString());
                LwsService.ConnectionParameters parameters = new LwsService.ConnectionParameters(
                        etServer.getText().toString(),
                        port
                );
                msg.obj = parameters;
                mService.send(msg);
                msg = Message.obtain(null, LwsService.MSG_THREAD_START, 0, 0);
                mService.send(msg);
            }
            catch(RemoteException e) {
                e.printStackTrace();
            }
        }
        else {
            try {
                mService.send(Message.obtain(null, LwsService.MSG_THREAD_STOP, 0, 0));
            }
            catch(RemoteException e) {
                e.printStackTrace();
            }
            mThreadIsRunning = false;
            mThreadIsSuspended = false;
        }
    }

}

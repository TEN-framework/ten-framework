# lws minimal ws server (threads) + SMP

This demonstrates both independent threads creating content as in the
-threads example, multiple service threads as in the http-server-smp
example (but with ws), and using the foreign libuv loop.

## build

You must first build libwebsockets itself with cmake `-DLWS_MAX_SMP=8`
or some other number greater than one, as well as `-DLWS_WITH_LIBUV=1`

```
 $ cmake . && make
```

Pthreads is required on your system.

## usage

```
 $ ./lws-minimal-ws-server-threads-smp
[2019/01/28 06:59:17:4217] USER: LWS minimal ws server + threads + smp | visit http://localhost:7681
[2019/01/28 06:59:17:4219] NOTICE:   Service threads: 2
[2019/01/28 06:59:17:4220] NOTICE: LWS_CALLBACK_EVENT_WAIT_CANCELLED in svc tid 0x7fec48af8700
[2019/01/28 06:59:17:4220] NOTICE: LWS_CALLBACK_EVENT_WAIT_CANCELLED in svc tid 0x7fec48af8700
...
```

Visit http://localhost:7681 on multiple browser windows.  You may need to open
4 before the second service thread is used (check "svc tid" in the browser output).

Two lws service threads are started.

Two separate asynchronous threads generate strings and add them to a ringbuffer,
signalling all lws service threads to send new entries to all the browser windows.

This demonstrates how to safely manage asynchronously generated content
and hook it up to the lws service threads.


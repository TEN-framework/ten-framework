# lws minimal http client attach

This demonstrates how other threads can reach out to an existing lws_context
and join its event loop cleanly and safely.

## build

```
 $ cmake . && make
```

Pthreads is required on your system.

## usage

```
 $ ./lws-minimal-http-client-attach
[2019/12/31 18:30:49:3495] U: main: main thread tid 0x503e1c0
[2019/12/31 18:30:50:3584] U: LWS minimal http client attach
[2019/12/31 18:30:50:4002] U: lws_create: tid 0x5c41700
[2019/12/31 18:30:50:5727] E: callback_ntpc: set up system ops for set_clock
[2019/12/31 18:30:50:2110] N: callback_ntpc: Unix time: 1577817053
[2019/12/31 18:30:50:2136] U: attach_callback: called from tid 0x5c41700
[2019/12/31 18:30:51:8733] U: Connected to 46.105.127.147, http response: 200
[2019/12/31 18:30:51:8818] U: RECEIVE_CLIENT_HTTP_READ: read 4087
[2019/12/31 18:30:51:8823] U: RECEIVE_CLIENT_HTTP_READ: read 4096
[2019/12/31 18:30:51:8846] U: RECEIVE_CLIENT_HTTP_READ: read 4087
[2019/12/31 18:30:51:8847] U: RECEIVE_CLIENT_HTTP_READ: read 4096
[2019/12/31 18:30:51:8855] U: RECEIVE_CLIENT_HTTP_READ: read 4087
[2019/12/31 18:30:51:8856] U: RECEIVE_CLIENT_HTTP_READ: read 4096
[2019/12/31 18:30:51:8860] U: RECEIVE_CLIENT_HTTP_READ: read 1971
[2019/12/31 18:30:51:8873] U: LWS_CALLBACK_COMPLETED_CLIENT_HTTP
[2019/12/31 18:30:51:9629] U: main: finished
```


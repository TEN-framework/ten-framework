# lws minimal http server eventlib

WARNING: this is under development, it's not stable.

This demonstrates a minimal http server that can use any of the event libraries

Commandline option|Meaning
---|---
-d <loglevel>|Debug verbosity in decimal, eg, -d15
-t <threads>|Number of threads to use.
--uv|Use the libuv event library (lws must have been configured with `-DLWS_WITH_LIBUV=1`)
--event|Use the libevent library (lws must have been configured with `-DLWS_WITH_LIBEVENT=1`)
--ev|Use the libev event library (lws must have been configured with `-DLWS_WITH_LIBEV=1`)

## build

lilbwebsockets must have been built with `LWS_MAX_SMP` greater than 1 to use
multiple threads.

```
 $ cmake . && make
```

## usage

```
 $ ./lws-minimal-http-server-eventlib-smp
[2018/03/04 09:30:02:7986] USER: LWS minimal http server-eventlib | visit http://localhost:7681
[2018/03/04 09:30:02:7986] NOTICE: Creating Vhost 'default' port 7681, 1 protocols, IPv6 on
```

Visit http://localhost:7681


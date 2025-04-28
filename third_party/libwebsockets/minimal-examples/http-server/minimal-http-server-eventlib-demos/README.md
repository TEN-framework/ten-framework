# lws minimal http server eventlib demos

This demonstrates a slightly more complex demo that can use
any of the event loops (it defaults to poll)

It uses statically included plugins to provide the lws test server functions

Commandline option|Meaning
---|---
-d <loglevel>|Debug verbosity in decimal, eg, -d15
--uv|Use the libuv event library (lws must have been configured with `-DLWS_WITH_LIBUV=1`)
--event|Use the libevent library (lws must have been configured with `-DLWS_WITH_LIBEVENT=1`)
--ev|Use the libev event library (lws must have been configured with `-DLWS_WITH_LIBEV=1`)

## build

```
 $ cmake . && make
```

## usage

```
 $ ./lws-minimal-http-server-eventlib-demos
[2018/03/04 09:30:02:7986] USER: LWS minimal http server-eventlib-demos | visit http://localhost:7681
[2018/03/04 09:30:02:7986] NOTICE: Creating Vhost 'default' port 7681, 1 protocols, IPv6 on
```

Visit http://localhost:7681


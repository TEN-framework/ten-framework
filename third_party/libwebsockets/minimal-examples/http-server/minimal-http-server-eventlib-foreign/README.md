# lws minimal http server eventlib foreign

Commandline option|Meaning
---|---
-d <loglevel>|Debug verbosity in decimal, eg, -d15
--uv|Use the libuv event library (lws must have been configured with `-DLWS_WITH_LIBUV=1`)
--event|Use the libevent library (lws must have been configured with `-DLWS_WITH_LIBEVENT=1`)
--ev|Use the libev event library (lws must have been configured with `-DLWS_WITH_LIBEV=1`)
--sd|Use the systemd event library (lws must have been configured with `-DLWS_WITH_SDEVENT=1`)

Notice libevent and libev cannot coexist in the one library.  But all the other combinations are OK.

x|libuv|libevent|libev
---|---|---|---
libuv|-|OK|OK
libevent|OK|-|no
libev|OK|no|-

This demonstrates having lws take part in a libuv loop owned by
something else, with its own objects running in the loop.

Lws can join the loop, and clean up perfectly after itself without
leaving anything behind or making trouble in the larger loop, which
does not need to stop during lws creation or destruction.

First the foreign loop is created with a 1s timer, and runs alone for 5s.

Then the lws context is created inside the timer callback and runs for 10s...
during this period you can visit http://localhost:7681 for normal lws
service using the foreign loop.

After the 10s are up, the lws context is destroyed inside the foreign loop
timer.  The foreign loop runs alone again for a further 5s and then
exits itself.

## build

```
 $ cmake . && make
```

## usage

```
 $ ./lws-minimal-http-server-eventlib-foreign
[2018/03/29 12:19:31:3480] USER: LWS minimal http server eventlib + foreign loop | visit http://localhost:7681
[2018/03/29 12:19:31:3724] NOTICE: Creating Vhost 'default' port 7681, 1 protocols, IPv6 off
[2018/03/29 12:19:31:3804] NOTICE:  Using foreign event loop...
[2018/03/29 12:19:31:3938] USER: Foreign 1Hz timer
[2018/03/29 12:19:32:4011] USER: Foreign 1Hz timer
[2018/03/29 12:19:33:4024] USER: Foreign 1Hz timer
^C[2018/03/29 12:19:33:8868] NOTICE: Signal 2 caught, exiting...
[2018/03/29 12:19:33:8963] USER: main: starting exit cleanup...
[2018/03/29 12:19:33:9064] USER: main: lws context destroyed: cleaning the foreign loop
[2018/03/29 12:19:33:9108] USER: main: exiting...
```

Visit http://localhost:7681


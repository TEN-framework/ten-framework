# lws minimal ws server timer

This is designed to confirm long term stability of ws timers on a
particular platform.

## build

```
 $ cmake . && make
```

## Commandline Options

Option|Meaning
---|---
-d|Set logging verbosity
-s|Serve using TLS selfsigned cert (ie, connect to it with https://...)
-h|Strict Host: header checking against vhost name (localhost) and port
-v|Connection validity use 3s / 10s instead of default 5m / 5m10s

## usage

```
 $ ./lws-minimal-ws-server-timer
[2018/03/04 09:30:02:7986] USER: LWS minimal ws server | visit http://localhost:7681
[2018/03/04 09:30:02:7986] NOTICE: Creating Vhost 'default' port 7681, 1 protocols, IPv6 on
```

Visit http://localhost:7681 and the browser will connect back to the test
server, you'll see ESTABLISHED logged.  That triggers a TIMER event at 20s
intervals which sets the wsi timeout to 60s.  It should just stay like
that forever doing the TIMER events at 20s intervals and not sending any
traffic either way.


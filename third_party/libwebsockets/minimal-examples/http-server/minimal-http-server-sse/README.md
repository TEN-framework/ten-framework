# lws minimal http Server Side Events

This demonstates serving both normal content and
content over Server Side Events.

## build

```
 $ cmake . && make
```

## usage

You can give -s to listen using https on port :443

```
 $ ./lws-minimal-http-server-sse
[2018/04/20 06:09:56:9974] USER: LWS minimal http Server-Side Events | visit http://localhost:7681
[2018/04/20 06:09:57:0148] NOTICE: Creating Vhost 'default' port 7681, 2 protocols, IPv6 off
```

Visit http://localhost:7681, which connects back to the server using SSE
and displays the incoming data.  Connecting from multiple browsers shows
content individual to the connection.


# lws minimal http Server Side Events + ringbuffer

This demonstates serving both normal content and
content over Server Side Events, where all clients
see the same data via a ringbuffer.

Two separate threads generate content into the
ringbuffer at random intervals.

## build

```
 $ cmake . && make
```

## usage

```
 $ ./lws-minimal-http-server-sse
[2018/04/20 06:09:56:9974] USER: LWS minimal http Server-Side Events + ring | visit http://localhost:7681
[2018/04/20 06:09:57:0148] NOTICE: Creating Vhost 'default' port 7681, 2 protocols, IPv6 off
```

Visit http://localhost:7681, which connects back to the server using SSE
and displays the incoming data.  Connecting from multiple browsers shows
the same content from the server ringbuffer.


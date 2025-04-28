# lws minimal ws server (lws_ring)

## build

```
 $ cmake . && make
```

## usage

```
 $ ./lws-minimal-ws-server
[2018/03/04 09:30:02:7986] USER: LWS minimal ws server (lws_ring) | visit http://localhost:7681
[2018/03/04 09:30:02:7986] NOTICE: Creating Vhost 'default' port 7681, 1 protocols, IPv6 on
```

Visit http://localhost:7681 on multiple browser windows

Text you type in any browser window is sent to all of them.

A ringbuffer holds up to 8 lines of text.

This also demonstrates how the ringbuffer can take action against lagging or
disconnected clients that cause the ringbuffer to fill.

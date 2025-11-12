# lws minimal ws server raw vhost

This demonstrates setting up a vhost to listen and accept raw sockets.
Raw sockets are just sockets... lws does not send anything on them or
interpret by itself what it receives on them.  So you can implement
arbitrary tcp protocols using them.

This isn't very useful standalone as shown here for clarity, but you can
freely combine a raw socket vhost with other lws server
and client features and other vhosts handling http or ws.

Becuase raw socket events have their own callback reasons, the handlers can
be integrated in a single protocol that also handles http and ws
server and client callbacks without conflict.

## build

```
 $ cmake . && make
```

## usage

 -s means listen using tls

```
 $ ./lws-minimal-raw-vhost
[2018/03/22 14:49:47:9516] USER: LWS minimal raw vhost
[2018/03/22 14:49:47:9673] NOTICE: Creating Vhost 'default' port 7681, 1 protocols, IPv6 off
[2018/03/22 14:49:52:3789] USER: LWS_CALLBACK_RAW_ADOPT
[2018/03/22 14:49:57:4271] USER: LWS_CALLBACK_RAW_CLOSE
```

```
 $ nc localhost 7681
hello
hello
```

Connect one or more sessions to the server using netcat... lines you type
into netcat are sent to the server, which echos them to all connected clients.


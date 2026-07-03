# lws minimal ws proxy

## Build

```
 $ cmake . && make
```

## Description

This is the same as minimal-ws-server-ring, but with the
inclusion of a ws client connection to https://libwebsockets.org
using the dumb-increment protocol feeding the ringbuffer.

Each client that connect to this server receives the content that
had arrived on the client connection feeding the ringbuffer proxied
to their browser window over a ws connection.

## Usage

```
 $ ./lws-minimal-ws-proxy 
[2018/03/14 17:50:10:6938] USER: LWS minimal ws proxy | visit http://localhost:7681
[2018/03/14 17:50:10:6955] NOTICE: Creating Vhost 'default' port 7681, 2 protocols, IPv6 off
[2018/03/14 17:50:10:6955] NOTICE:  Using non-SSL mode
[2018/03/14 17:50:10:7035] NOTICE: created client ssl context for default
[2018/03/14 17:50:11:7047] NOTICE: binding to lws-minimal-proxy
[2018/03/14 17:50:11:7047] NOTICE: lws_client_connect_2: 0x872e60: address libwebsockets.org
[2018/03/14 17:50:12:3282] NOTICE: lws_client_connect_2: 0x872e60: address libwebsockets.org
[2018/03/14 17:50:13:8195] USER: callback_minimal: established
```

Visit http://localhost:7681 on multiple browser windows

Data received on the remote wss connection is copied to all open browser windows.

A ringbuffer holds up to 8 lines of text in the server, and the browser shows
the last 20 lines of received text.

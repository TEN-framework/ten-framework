# lws minimal ws client rx

## build

```
 $ cmake . && make
```

## usage

The application goes to https://libwebsockets.org and makes a wss connection
using the dumb-increment-protocol.  It shows the incrementing number it is
being sent over ws as it arrives.

This example only receives things to keep it simple.  See minimal-ws-client-tx
for code related to sending things.  Of course rx and tx are supported in the
same protocol.

```
./lws-minimal-ws-client-rx
[2018/03/14 11:57:24:0689] USER: LWS minimal ws client rx
[2018/03/14 11:57:24:0705] NOTICE: Creating Vhost 'default' port -1, 1 protocols, IPv6 off
[2018/03/14 11:57:24:0710] NOTICE: created client ssl context for default
[2018/03/14 11:57:24:0788] NOTICE: lws_client_connect_2: 0x15b8310: address libwebsockets.org
[2018/03/14 11:57:24:7643] NOTICE: lws_client_connect_2: 0x15b8310: address libwebsockets.org
[2018/03/14 11:57:26:9191] USER: RX: 0
[2018/03/14 11:57:26:9318] USER: RX: 1
[2018/03/14 11:57:27:2182] USER: RX: 2
[2018/03/14 11:57:27:2336] USER: RX: 3
[2018/03/14 11:57:27:2838] USER: RX: 4
[2018/03/14 11:57:27:5173] USER: RX: 5
[2018/03/14 11:57:27:5352] USER: RX: 6
[2018/03/14 11:57:27:5854] USER: RX: 7
[2018/03/14 11:57:27:8156] USER: RX: 8
[2018/03/14 11:57:27:8359] USER: RX: 9
^C[2018/03/14 11:57:27:9884] USER: Completed
```



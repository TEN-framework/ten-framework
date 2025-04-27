# lws minimal ws server raw adopt tcp

This example is only meaningful if you are integrating lws in another
app which generates its own connected sockets.  In some cases you may
want lws to "adopt" the socket.

(If you simply want a connected client raw socket using lws alone, you
can just use lws_client_connect_via_info() with info.method = "RAW".
http-client/minimal-http-client shows how to do that, just set
info.method to "RAW".)

This example demonstrates how to adopt a foreign, connected socket into lws
as a raw wsi, bound to a specific lws protocol.

The example connects a socket itself to libwebsockets.org:80, and then
has lws adopt it as a raw wsi.  The lws protocol writes "GET / HTTP/1.1"
to the socket and hexdumps what was sent back.

The socket won't close until the server side times it out, since it's
a raw socket that doesn't understand it's looking at http.

## build

```
 $ cmake . && make
```

## usage

```
 $ ./lws-minimal-raw-adopt-tcp
[2018/03/23 09:03:57:1960] USER: LWS minimal raw adopt tcp
[2018/03/23 09:03:57:1961] NOTICE: Creating Vhost 'default' port 7681, 1 protocols, IPv6 off
[2018/03/23 09:03:57:2079] USER: Starting connect...
[2018/03/23 09:03:57:4963] USER: Connected...
[2018/03/23 09:03:57:4963] USER: LWS_CALLBACK_RAW_ADOPT
[2018/03/23 09:03:57:7842] USER: LWS_CALLBACK_RAW_RX (186)
[2018/03/23 09:03:57:7842] NOTICE: 
[2018/03/23 09:03:57:7842] NOTICE: 0000: 48 54 54 50 2F 31 2E 31 20 33 30 31 20 52 65 64    HTTP/1.1 301 Red
[2018/03/23 09:03:57:7842] NOTICE: 0010: 69 72 65 63 74 0D 0A 73 65 72 76 65 72 3A 20 6C    irect..server: l
[2018/03/23 09:03:57:7842] NOTICE: 0020: 77 73 77 73 0D 0A 53 74 72 69 63 74 2D 54 72 61    wsws..Strict-Tra
[2018/03/23 09:03:57:7843] NOTICE: 0030: 6E 73 70 6F 72 74 2D 53 65 63 75 72 69 74 79 3A    nsport-Security:
[2018/03/23 09:03:57:7843] NOTICE: 0040: 20 6D 61 78 2D 61 67 65 3D 31 35 37 36 38 30 30     max-age=1576800
[2018/03/23 09:03:57:7843] NOTICE: 0050: 30 20 3B 20 69 6E 63 6C 75 64 65 53 75 62 44 6F    0 ; includeSubDo
[2018/03/23 09:03:57:7843] NOTICE: 0060: 6D 61 69 6E 73 0D 0A 6C 6F 63 61 74 69 6F 6E 3A    mains..location:
[2018/03/23 09:03:57:7843] NOTICE: 0070: 20 68 74 74 70 73 3A 2F 2F 6C 69 62 77 65 62 73     https://libwebs
[2018/03/23 09:03:57:7843] NOTICE: 0080: 6F 63 6B 65 74 73 2E 6F 72 67 0D 0A 63 6F 6E 74    ockets.org..cont
[2018/03/23 09:03:57:7843] NOTICE: 0090: 65 6E 74 2D 74 79 70 65 3A 20 74 65 78 74 2F 68    ent-type: text/h
[2018/03/23 09:03:57:7843] NOTICE: 00A0: 74 6D 6C 0D 0A 63 6F 6E 74 65 6E 74 2D 6C 65 6E    tml..content-len
[2018/03/23 09:03:57:7843] NOTICE: 00B0: 67 74 68 3A 20 30 0D 0A 0D 0A                      gth: 0....      
[2018/03/23 09:03:57:7843] NOTICE: 
[2018/03/23 09:04:03:3627] USER: LWS_CALLBACK_RAW_CLOSE

```

Note the example does everything itself, after 5s idle the remote server closes the connection
after which the example continues until you ^C it.

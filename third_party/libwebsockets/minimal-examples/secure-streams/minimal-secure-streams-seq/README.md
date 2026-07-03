# lws minimal sequre streams seq

The application goes to https://warmcat.com and reads index.html there.

It does it using Secure Streams... the main code in minimal-secure-streams.c
just sets up the context and opens a secure stream of type "mintest".

The handler for state changes and payloads for "mintest" is in ss-myss.c

The information about how a "mintest" stream should connect and the
protocol it uses is kept separated in policy-database.c

## build

```
 $ cmake . && make
```

## usage

Commandline option|Meaning
---|---
-d <loglevel>|Debug verbosity in decimal, eg, -d15
-f| Force connecting to the wrong endpoint to check backoff retry flow

```
 $ ./lws-minimal-secure-streams-seq
[2018/03/04 14:43:20:8562] USER: LWS minimal http client
[2018/03/04 14:43:20:8571] NOTICE: Creating Vhost 'default' port -1, 1 protocols, IPv6 on
[2018/03/04 14:43:20:8616] NOTICE: created client ssl context for default
[2018/03/04 14:43:20:8617] NOTICE: lws_client_connect_2: 0x1814dc0: address warmcat.com
[2018/03/04 14:43:21:1496] NOTICE: lws_client_connect_2: 0x1814dc0: address warmcat.com
[2018/03/04 14:43:22:0154] NOTICE: lws_client_interpret_server_handshake: incoming content length 26520
[2018/03/04 14:43:22:0154] NOTICE: lws_client_interpret_server_handshake: client connection up
[2018/03/04 14:43:22:0169] USER: RECEIVE_CLIENT_HTTP_READ: read 1024
[2018/03/04 14:43:22:0169] USER: RECEIVE_CLIENT_HTTP_READ: read 1024
[2018/03/04 14:43:22:0169] USER: RECEIVE_CLIENT_HTTP_READ: read 1024
[2018/03/04 14:43:22:0169] USER: RECEIVE_CLIENT_HTTP_READ: read 1015
[2018/03/04 14:43:22:0174] USER: RECEIVE_CLIENT_HTTP_READ: read 1024
[2018/03/04 14:43:22:0174] USER: RECEIVE_CLIENT_HTTP_READ: read 1024
[2018/03/04 14:43:22:0174] USER: RECEIVE_CLIENT_HTTP_READ: read 1024
[2018/03/04 14:43:22:0174] USER: RECEIVE_CLIENT_HTTP_READ: read 1015
[2018/03/04 14:43:22:0179] USER: RECEIVE_CLIENT_HTTP_READ: read 1024
[2018/03/04 14:43:22:0179] USER: RECEIVE_CLIENT_HTTP_READ: read 1024
[2018/03/04 14:43:22:0179] USER: RECEIVE_CLIENT_HTTP_READ: read 1024
[2018/03/04 14:43:22:0179] USER: RECEIVE_CLIENT_HTTP_READ: read 1015
[2018/03/04 14:43:22:3010] USER: RECEIVE_CLIENT_HTTP_READ: read 1024
[2018/03/04 14:43:22:3010] USER: RECEIVE_CLIENT_HTTP_READ: read 1024
[2018/03/04 14:43:22:3010] USER: RECEIVE_CLIENT_HTTP_READ: read 1024
[2018/03/04 14:43:22:3010] USER: RECEIVE_CLIENT_HTTP_READ: read 1015
[2018/03/04 14:43:22:3015] USER: RECEIVE_CLIENT_HTTP_READ: read 1024
[2018/03/04 14:43:22:3015] USER: RECEIVE_CLIENT_HTTP_READ: read 1024
[2018/03/04 14:43:22:3015] USER: RECEIVE_CLIENT_HTTP_READ: read 1024
[2018/03/04 14:43:22:3015] USER: RECEIVE_CLIENT_HTTP_READ: read 1015
[2018/03/04 14:43:22:3020] USER: RECEIVE_CLIENT_HTTP_READ: read 1024
[2018/03/04 14:43:22:3020] USER: RECEIVE_CLIENT_HTTP_READ: read 1024
[2018/03/04 14:43:22:3020] USER: RECEIVE_CLIENT_HTTP_READ: read 1024
[2018/03/04 14:43:22:3020] USER: RECEIVE_CLIENT_HTTP_READ: read 1015
[2018/03/04 14:43:22:3022] USER: RECEIVE_CLIENT_HTTP_READ: read 1024
[2018/03/04 14:43:22:3022] USER: RECEIVE_CLIENT_HTTP_READ: read 974
[2018/03/04 14:43:22:3022] NOTICE: lws_http_client_read: transaction completed says -1
[2018/03/04 14:43:23:3042] USER: Completed
```



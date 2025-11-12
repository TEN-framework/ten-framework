# lws minimal http client

The application goes to either https://warmcat.com or
https://localhost:7681 (with `-l` option) and receives the page data.

## build

```
 $ cmake . && make
```

## usage

Commandline option|Meaning
---|---
-d <loglevel>|Debug verbosity in decimal, eg, -d15
-l| Connect to https://localhost:7681 and accept selfsigned cert
--h1|Specify http/1.1 only using ALPN, rejects h2 even if server supports it
--server <name>|set server name to connect to
-k|Apply tls option LCCSCF_ALLOW_INSECURE
-j|Apply tls option LCCSCF_ALLOW_SELFSIGNED
-m|Apply tls option LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK
-e|Apply tls option LCCSCF_ALLOW_EXPIRED
-b|Apply tls option LCCSCF_CACHE_COOKIES
-w|For mbedtls/wolfssl, load wrong CA cert (expected to fail)
-c <cookie jar file>|Set filepath used for cookie jar
-v|Connection validity use 3s / 10s instead of default 5m / 5m10s
--nossl| disable ssl connection
--user <username>| Set Basic Auth username
--password <password> | Set Basic Auth password

```
 $ ./lws-minimal-http-client
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

You can also test the client Basic Auth support against the http-server/minimal-http-server-basicauth
example.  In one console window run the server and in the other

```
$ lws-minimal-http-client -l --nossl --path /secret/index.html --user user --password password
```

The Basic Auth credentials for the test server are literally username "user" and password "password".


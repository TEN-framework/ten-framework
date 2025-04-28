# lws minimal http client JIT Trust

This example turns off any existing trusted CAs and then tries to connect to a server, by default, warmcat.com.

It validates the remote certificates using trusted CAs from a JIT Trust blob compiled into the code.

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
-v|Connection validity use 3s / 10s instead of default 5m / 5m10s
--nossl| disable ssl connection
--user <username>| Set Basic Auth username
--password <password> | Set Basic Auth password

```
 $ ./bin/lws-minimal-http-client-jit-trust --h1 --server ebay.com --path /
==1302866== 
[2021/06/17 14:33:54:7500] U: LWS minimal http client JIT Trust [-d<verbosity>] [-l] [--h1]
[2021/06/17 14:33:54:7956] N: LWS: 4.2.99-v4.2.0-70-g80e7e39bae, loglevel 1031
[2021/06/17 14:33:54:7960] N: NET CLI SRV H1 H2 WS MbedTLS ConMon IPv6-absent
[2021/06/17 14:33:54:8165] N:  ++ [wsi|0|pipe] (1)
[2021/06/17 14:33:54:8227] N:  ++ [vh|0|netlink] (1)
[2021/06/17 14:33:54:8319] N:  ++ [vh|1|default||-1] (2)
[2021/06/17 14:33:55:0107] N:  ++ [wsicli|0|GET/h1/ebay.com] (1)
[2021/06/17 14:33:56:0291] N:  ++ [vh|2|jitt-7F69A044||-1] (3)
[2021/06/17 14:33:56:0355] E: CLIENT_CONNECTION_ERROR: server's cert didn't look good, invalidca (use_ssl 0x20000061) X509_V_ERR = 24: CA is not trusted

[2021/06/17 14:33:56:0376] N:  ++ [wsicli|1|GET/h1/ebay.com] (2)
[2021/06/17 14:33:56:0746] N:  -- [wsicli|0|GET/h1/ebay.com] (1) 1.061s
[2021/06/17 14:33:56:7555] N: lws_client_reset: REDIRECT www.ebay.com:443, path='/', ssl = 1, alpn='http/1.1'
[2021/06/17 14:33:57:0205] N:  ++ [vh|3|jitt-DFF2B5B4||-1] (4)
[2021/06/17 14:33:57:0208] E: CLIENT_CONNECTION_ERROR: server's cert didn't look good, invalidca (use_ssl 0x1) X509_V_ERR = 24: CA is not trusted

[2021/06/17 14:33:57:0210] N:  ++ [wsicli|2|GET/h1/ebay.com] (2)
[2021/06/17 14:33:57:0288] N:  -- [wsicli|1|GET/h1/ebay.com] (1) 991.119ms
[2021/06/17 14:33:57:7528] N: lws_client_reset: REDIRECT www.ebay.com:443, path='/', ssl = 1, alpn='http/1.1'
[2021/06/17 14:33:58:1564] U: Connected to 195.95.193.127, http response: 200
[2021/06/17 14:33:58:1637] U: RECEIVE_CLIENT_HTTP_READ: read 209
[2021/06/17 14:33:58:1796] U: RECEIVE_CLIENT_HTTP_READ: read 197
[2021/06/17 14:33:58:1822] U: RECEIVE_CLIENT_HTTP_READ: read 1014
[2021/06/17 14:33:58:1847] U: RECEIVE_CLIENT_HTTP_READ: read 1024
[2021/06/17 14:33:58:1851] U: RECEIVE_CLIENT_HTTP_READ: read 1022
[2021/06/17 14:33:58:2748] U: RECEIVE_CLIENT_HTTP_READ: read 242
[2021/06/17 14:33:58:2782] U: RECEIVE_CLIENT_HTTP_READ: read 1014
[2021/06/17 14:33:58:2784] U: RECEIVE_CLIENT_HTTP_READ: read 1024
[2021/06/17 14:33:58:2785] U: RECEIVE_CLIENT_HTTP_READ: read 1024
...
[2021/06/17 14:33:58:4661] U: RECEIVE_CLIENT_HTTP_READ: read 1024
[2021/06/17 14:33:58:4662] U: RECEIVE_CLIENT_HTTP_READ: read 1024
[2021/06/17 14:33:58:4663] U: RECEIVE_CLIENT_HTTP_READ: read 1024
[2021/06/17 14:33:58:4664] U: RECEIVE_CLIENT_HTTP_READ: read 1024
[2021/06/17 14:33:58:4665] U: RECEIVE_CLIENT_HTTP_READ: read 1024
[2021/06/17 14:33:58:4666] U: RECEIVE_CLIENT_HTTP_READ: read 1024
[2021/06/17 14:33:58:4667] U: RECEIVE_CLIENT_HTTP_READ: read 1024
[2021/06/17 14:33:58:4668] U: RECEIVE_CLIENT_HTTP_READ: read 1024
[2021/06/17 14:33:58:4669] U: RECEIVE_CLIENT_HTTP_READ: read 1024
[2021/06/17 14:33:58:4670] U: RECEIVE_CLIENT_HTTP_READ: read 1024
[2021/06/17 14:33:58:4671] U: RECEIVE_CLIENT_HTTP_READ: read 1024
[2021/06/17 14:33:58:4672] U: RECEIVE_CLIENT_HTTP_READ: read 1024
[2021/06/17 14:33:58:4673] U: RECEIVE_CLIENT_HTTP_READ: read 286
[2021/06/17 14:33:58:4690] U: LWS_CALLBACK_COMPLETED_CLIENT_HTTP
[2021/06/17 14:33:58:4712] E: main: destroying context, interrupted = 1
[2021/06/17 14:33:58:4774] N:  -- [wsi|0|pipe] (0) 3.661s
[2021/06/17 14:33:58:4780] N: callback_http: LWS_CALLBACK_CLOSED_CLIENT_HTTP
[2021/06/17 14:33:58:4829] N:  -- [vh|3|jitt-DFF2B5B4||-1] (3) 1.462s
[2021/06/17 14:33:58:4833] N:  -- [wsicli|2|GET/h1/ebay.com] (0) 1.462s
[2021/06/17 14:33:58:4834] N:  -- [vh|0|netlink] (2) 3.660s
[2021/06/17 14:33:58:4858] N:  -- [vh|1|default||-1] (1) 3.654s
[2021/06/17 14:33:58:4860] N:  -- [vh|2|jitt-7F69A044||-1] (0) 2.456s
[2021/06/17 14:33:58:4974] U: Completed: OK (seen expected 0)
```

You can also test the client Basic Auth support against the http-server/minimal-http-server-basicauth
example.  In one console window run the server and in the other

```
$ lws-minimal-http-client -l --nossl --path /secret/index.html --user user --password password
```

The Basic Auth credentials for the test server are literally username "user" and password "password".


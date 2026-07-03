# lws minimal raw fallback http server

This is the same as the minimal http server, with one difference...
if you connect to localhost:7681 with something that doesn't send
recognizable http, then the connection will be switched to a
raw-skt role and bind to a protocol that echoes anything sent back
to the sender.

## build

```
 $ cmake . && make
```

## usage

Commandline option|Meaning
---|---
-d <loglevel>|Debug verbosity in decimal, eg, -d15
-s|Configure the server for tls / https and `LWS_SERVER_OPTION_ALLOW_NON_SSL_ON_SSL_PORT`
-h|(needs -s) Configure the vhost also for `LWS_SERVER_OPTION_ALLOW_HTTP_ON_HTTPS_LISTENER`, allowing http service on tls port (caution... it's insecure then)
-u|(needs -s) Configure the vhost also for `LWS_SERVER_OPTION_REDIRECT_HTTP_TO_HTTPS`, so the server issues a redirect to https to clients that attempt to connect to a server configured for tls with http.

```
 $ ./lws-minimal-raw-fallback-http-server
[2018/11/29 14:27:34:3014] USER: LWS minimal raw fallback http server | visit http://localhost:7681
[2018/11/29 14:27:34:3243] NOTICE: Creating Vhost 'default' port 7681, 1 protocols, IPv6 off
```

Visit http://127.0.0.1:7681

This allows testing of various combinations of special features for unexpected
content on an http(s) listening socket.

|cmdline args|http://127.0.0.1:7681|https://127.0.0.1:7681|ssh -p7681 127.0.0.1|flags|
|---|---|---|---|---|
|none|served|no tls|echos hello|LWS_SERVER_OPTION_FALLBACK_TO_APPLY_LISTEN_ACCEPT_CONFIG
|-s|echos http GET|served|echos hello|LWS_SERVER_OPTION_FALLBACK_TO_APPLY_LISTEN_ACCEPT_CONFIG, LWS_SERVER_OPTION_ALLOW_NON_SSL_ON_SSL_PORT
|-s -h|served|served|echos hello|LWS_SERVER_OPTION_FALLBACK_TO_APPLY_LISTEN_ACCEPT_CONFIG, LWS_SERVER_OPTION_ALLOW_NON_SSL_ON_SSL_PORT, LWS_SERVER_OPTION_ALLOW_HTTP_ON_HTTPS_LISTENER
|-s -u|redirected to https|served|echos hello|LWS_SERVER_OPTION_FALLBACK_TO_APPLY_LISTEN_ACCEPT_CONFIG, LWS_SERVER_OPTION_ALLOW_NON_SSL_ON_SSL_PORT, LWS_SERVER_OPTION_REDIRECT_HTTP_TO_HTTPS


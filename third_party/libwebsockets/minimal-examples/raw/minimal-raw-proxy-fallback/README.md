# lws minimal ws server raw proxy fallback

This demonstrates how a vhost doing normal http or http(s) duty can be also be
bound to a specific role and protocol as a fallback if the incoming protocol is
unexpected for tls or http.  The example makes the fallback role + protocol
an lws plugin that performs raw packet proxying.

By default the fallback in the example will proxy 127.0.0.1:22, which is usually
your ssh server listen port, on 127.0.0.1:7681.  You should be able to ssh into
port 7681 the same as you can port 22.  At the same time, you should be able to
visit http://127.0.0.1:7681 in a browser (and if you give -s, to
https://127.0.0.1:7681 while your ssh client can still connect to the same
port.

## build

To build this standalone, you must tell cmake where the lws source tree
./plugins directory can be found, since it relies on including the source
of the raw-proxy plugin.

```
 $ cmake . -DLWS_PLUGINS_DIR=~/libwebsockets/plugins && make
```

## usage

Commandline option|Meaning
---|---
-d <loglevel>|Debug verbosity in decimal, eg, -d15
-r ipv4:address:port|Configure the remote IP and port that will be proxied, by default ipv4:127.0.0.1:22
-s|Configure the server for tls / https and `LWS_SERVER_OPTION_ALLOW_NON_SSL_ON_SSL_PORT`
-h|(needs -s) Configure the vhost also for `LWS_SERVER_OPTION_ALLOW_HTTP_ON_HTTPS_LISTENER`, allowing http service on tls port (caution... it's insecure then)
-u|(needs -s) Configure the vhost also for `LWS_SERVER_OPTION_REDIRECT_HTTP_TO_HTTPS`, so the server issues a redirect to https to clients that attempt to connect to a server configured for tls with http.
```
 $ ./lws-minimal-raw-proxy
[2018/11/30 19:22:35:7290] USER: LWS minimal raw proxy-fallback
[2018/11/30 19:22:35:7291] NOTICE: Creating Vhost 'default' port 7681, 1 protocols, IPv6 off
[2018/11/30 19:22:35:7336] NOTICE: callback_raw_proxy: onward ipv4 127.0.0.1:22
...
```

```
 $ ssh -p7681 me@127.0.0.1
Last login: Fri Nov 30 19:29:23 2018 from 127.0.0.1
[me@learn ~]$
```

At the same time, visiting http(s)://127.0.0.1:7681 in a browser works fine.


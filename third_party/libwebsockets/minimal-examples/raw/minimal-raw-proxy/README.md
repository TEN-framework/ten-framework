# lws minimal ws server raw proxy

This demonstrates how a vhost can be bound to a specific role and protocol,
with the example using a lws plugin that performs raw packet proxying.

By default the example will proxy 127.0.0.1:22, usually your ssh server
listen port, on 127.0.0.1:7681.  You should be able to ssh into port 7681
the same as you can port 22.  But your ssh server is only listening on port 22...

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

```
 $ ./lws-minimal-raw-proxy
[2018/11/30 19:22:35:7290] USER: LWS minimal raw proxy | nc localhost 7681
[2018/11/30 19:22:35:7291] NOTICE: Creating Vhost 'default' port 7681, 1 protocols, IPv6 off
[2018/11/30 19:22:35:7336] NOTICE: callback_raw_proxy: onward ipv4 127.0.0.1:22
...
```

```
 $ ssh -p7681 me@127.0.0.1
Last login: Fri Nov 30 19:29:23 2018 from 127.0.0.1
[me@learn ~]$
```



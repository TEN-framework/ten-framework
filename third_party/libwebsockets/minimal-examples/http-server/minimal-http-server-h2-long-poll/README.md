# lws minimal http server

## build

```
 $ cmake . && make
```
## Commandline Options

Option|Meaning
---|---
-d|Set logging verbosity
-s|Serve using TLS selfsigned cert (ie, connect to it with https://...)
-v|Connection validity use 3s / 10s instead of default 5m / 5m10s

## usage

```
 $ ./lws-minimal-http-server
[2018/03/04 09:30:02:7986] USER: LWS minimal http server | visit http://localhost:7681
[2018/03/04 09:30:02:7986] NOTICE: Creating Vhost 'default' port 7681, 1 protocols, IPv6 on
```

Visit http://localhost:7681


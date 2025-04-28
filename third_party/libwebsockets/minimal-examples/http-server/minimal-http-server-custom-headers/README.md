# lws minimal http server dynamic content

## build

```
 $ cmake . && make
```

## usage

```
 $ ./lws-minimal-http-server-dynamic
[2018/03/20 10:24:24:7099] USER: LWS minimal http server dynamic | visit http://localhost:7681
[2018/03/20 10:24:24:7099] NOTICE: Creating Vhost 'default' port 7681, 1 protocols, IPv6 off
```

Visit http://localhost:7681, which is all static content.

Click on the link to /dyn/anything, this opens a new tab with dynamicly-produced content.


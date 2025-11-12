# lws minimal http server mimetypes

This is the same as the basic minimal http server, but it demonstrates how to
add support for extra mimetypes to a mount.

## build

```
 $ cmake . && make
```

## usage

```
 $ ./lws-minimal-http-server
[2018/03/04 09:30:02:7986] USER: LWS minimal http server | visit http://localhost:7681
[2018/03/04 09:30:02:7986] NOTICE: Creating Vhost 'default' port 7681, 1 protocols, IPv6 on
```

Visit http://localhost:7681 and click on the link to download the test.tar.bz2 file.


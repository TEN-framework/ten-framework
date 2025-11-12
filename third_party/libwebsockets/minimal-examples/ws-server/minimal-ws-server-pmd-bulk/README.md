# lws minimal ws server + permessage-deflate for bulk traffic

## build

```
 $ cmake . && make
```

## usage

```
 $ ./lws-minimal-ws-server-pmd-bulk
[2018/03/04 09:30:02:7986] USER: LWS minimal ws server | visit http://localhost:7681
[2018/03/04 09:30:02:7986] NOTICE: Creating Vhost 'default' port 7681, 1 protocols, IPv6 on
```

Visit http://localhost:7681 in your browser

One or another kind of bulk ws transfer is made to the browser.

The ws connection is made via permessage-deflate extension.

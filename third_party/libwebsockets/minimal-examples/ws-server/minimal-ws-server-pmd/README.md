# lws minimal ws server + permessage-deflate

## build

```
 $ cmake . && make
```

## usage

```
 $ ./lws-minimal-ws-server
[2018/03/04 09:30:02:7986] USER: LWS minimal ws server | visit http://localhost:7681
[2018/03/04 09:30:02:7986] NOTICE: Creating Vhost 'default' port 7681, 1 protocols, IPv6 on
```

Visit http://localhost:7681 on multiple browser windows

Text you type in any browser window is sent to all of them.

For simplicity of this example, only one line of text is cached at the server.

The ws connection is made via permessage-deflate extension.

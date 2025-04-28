# lws minimal ws server + permessage-deflate corner case tests

## build

```
 $ cmake . && make
```

## usage

```
 $ ./lws-minimal-ws-server-pmd-corner
[2018/11/21 16:47:49:0171] USER: LWS minimal ws server + permessage-deflate Corner Cases | visit http://localhost:7681
[2018/11/21 16:47:49:0172] NOTICE: Creating Vhost 'default' port 7681, 2 protocols, IPv6 off

```

Visit http://localhost:7681 

5 ws connections are made via permessage-deflate extension.

When the ws connection is established, various amounts of data are sent
resulting in ciphertext packets of a known size.


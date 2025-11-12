# lws minimal http server multivhost

This creates a single server that creates three vhosts listening on both :7681 and
:7682.  Two separate vhosts share listening on :7682.

|vhost|listens on port|serves|
---|---|---
localhost1|7681|./mount-origin-localhost1
localhost2|7682|./mount-origin-localhost2
localhost3|7682|./mount-origin-localhost3

Notice the last two both listen on 7682.  If you visit http://localhost:7682,
by default you will get mapped to the first one, localhost2.

However if you edit /etc/hosts on your machine and add

```
127.0.0.1 localhost3
```

so that you can visit http://localhost3:7682 in your browser, lws will use the
`Host: localhost3` header sent by your browser to select the localhost3 vhost
for the connection, and you will be served content from ./mount-origin-localhost3

## build

```
 $ cmake . && make
```

## usage

Commandline option|Meaning
---|---
-d <loglevel>|Debug verbosity in decimal, eg, -d15
--die-after-vhost | For testing failure handling

```
 $ ./lws-minimal-http-server-multivhost
[2018/03/16 09:37:20:0866] USER: LWS minimal http server-multivhost | visit http://localhost:7681 / 7682
[2018/03/16 09:37:20:0867] NOTICE: Creating Vhost 'localhost1' port 7681, 1 protocols, IPv6 off
[2018/03/16 09:37:20:0868] NOTICE: Creating Vhost 'localhost2' port 7682, 1 protocols, IPv6 off
[2018/03/16 09:37:20:0869] NOTICE: Creating Vhost 'localhost3' port 7682, 1 protocols, IPv6 off
[2018/03/16 09:37:20:0869] NOTICE:  using listen skt from vhost localhost2
```

Visit http://localhost:7681 and http://localhost:7682


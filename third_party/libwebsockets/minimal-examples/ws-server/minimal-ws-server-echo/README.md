# lws minimal ws server + permessage-deflate echo

This example serves no-protocl-name ws on localhost:7681
and echoes back anything that comes from the client.

You can use it for testing lws against Autobahn (use the
-p option to tell it to listen on 9001 for that)

## build

```
 $ cmake . && make
```

## usage

Commandline option|Meaning
---|---
-d <loglevel>|Debug verbosity in decimal, eg, -d15
-p port|Port to connect to
-u url|URL path part to connect to
-o|Finish after one connection

```
 $ ./lws-minimal-ws-server-echo
[2018/04/24 10:29:34:6212] USER: LWS minimal ws server echo + permessage-deflate + multifragment bulk message
[2018/04/24 10:29:34:6213] NOTICE: Creating Vhost 'default' port 7681, 1 protocols, IPv6 off
...
```


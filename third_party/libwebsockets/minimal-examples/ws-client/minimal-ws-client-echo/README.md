# lws minimal ws client + permessage-deflate echo

This example opens a ws client connection to localhost:7681 and
echoes back anything that comes from the server.

You can use it for testing lws against Autobahn.

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
--ssl|Open client connection with ssl
-i <iface>|Bind the client connection to interface iface

```
 $ ./lws-minimal-ws-client-echo
[2018/04/22 20:03:50:2343] USER: LWS minimal ws client echo + permessage-deflate + multifragment bulk message
[2018/04/22 20:03:50:2344] USER:    lws-minimal-ws-client-echo [-n (no exts)] [-u url] [-o (once)]
[2018/04/22 20:03:50:2344] USER: options 0
[2018/04/22 20:03:50:2345] NOTICE: Creating Vhost 'default' (serving disabled), 1 protocols, IPv6 off
[2018/04/22 20:03:51:2356] USER: connecting to localhost:9001//runCase?case=362&agent=libwebsockets
[2018/04/22 20:03:51:2385] NOTICE: checking client ext permessage-deflate
[2018/04/22 20:03:51:2386] NOTICE: instantiating client ext permessage-deflate
[2018/04/22 20:03:51:2386] USER: LWS_CALLBACK_CLIENT_ESTABLISHED
...
```


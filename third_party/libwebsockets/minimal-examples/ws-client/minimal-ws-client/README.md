# lws minimal ws client

This connects to libwebsockets.org using the dumb-increment-protocol.

It demonstrates how to use the connection retry and backoff stuff in lws.

## build

```
 $ cmake . && make
```

## Commandline Options

Option|Meaning
---|---
-d|Set logging verbosity
-s|Use a specific server instead of libwebsockets.org, eg `--server localhost`.  Implies LCCSCF_ALLOW_SELFSIGNED
-p|Use a specific port instead of 443, eg `--port 7681`
-j|Allow selfsigned tls cert
-k|Allow insecure certs
-m|Skip server hostname check
-n|Skip tls usage
-e|Allow expired certs
--protocol|Use a specific ws subprotocol rather than dumb-increment-protocol, eg, `--protocol myprotocol`


## usage

Just run it, it will connect to libwebsockets.org and spew incrementing numbers
sent by the server at 20Hz

```
 $ ./lws-minimal-ws-client
[2020/01/22 05:38:47:3409] U: LWS minimal ws client
[2020/01/22 05:38:47:4456] N: Loading client CA for verification ./libwebsockets.org.cer
[2020/01/22 05:38:48:1649] U: callback_minimal: established
[2020/01/22 05:38:48:1739] N: 
[2020/01/22 05:38:48:1763] N: 0000: 30                                                 0               
[2020/01/22 05:38:48:1765] N: 

...
```

To test against the lws test server instead of libwebsockets.org, run the test
server as

```
$ libwebsockets-test-server -s
```

and run this test app with

```
$ ./lws-minimal-ws-client -s localhost -p 7681 -j
```

You can kill and restart the server to confirm the client connection is re-
established if done within the backoff period.

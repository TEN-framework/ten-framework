# lws minimal ws client SPAM

This connects to libwebsockets.org using the lws-mirror-protocol.

By default is has 10 concurrent connections and connects 100 times.

## build

```
 $ cmake . && make
```

## Commandline Options

Option|Meaning
---|---
-d|Set logging verbosity
--server|Use a specific server instead of libwebsockets.org, eg `--server localhost`.  Implies LCCSCF_ALLOW_SELFSIGNED
--port|Use a specific port instead of 443, eg `--port 7681`
-c|Amount of concurrent connections
-l|Test limit (total number of connections to make)

## usage

Just run it, it will repeatedly connect and reconnect to libwebsockets.org
until it hits the test limit.

You can also direct it to use the lws test server in tls mode by running that
with `libwebsockets-test-server -s` and running this using, eg

```
 $ ./lws-minimal-ws-client-spam -c 20 -l 200 --server localhost --port 7681
```

```
 $ ./lws-minimal-ws-client-spam
[2018/11/15 09:53:19:9639] USER: LWS minimal ws client SPAM
[2018/11/15 09:53:19:9647] NOTICE: Creating Vhost 'default' (serving disabled), 1 protocols, IPv6 off
[2018/11/15 09:53:19:9695] NOTICE: created client ssl context for default
[2018/11/15 09:53:21:0976] USER: callback_minimal_spam: established (try 10, est 0, closed 0, err 0)
[2018/11/15 09:53:21:1041] USER: callback_minimal_spam: established (try 10, est 1, closed 0, err 0)
[2018/11/15 09:53:21:1089] USER: callback_minimal_spam: established (try 10, est 2, closed 0, err 0)
[2018/11/15 09:53:21:1132] USER: callback_minimal_spam: established (try 10, est 3, closed 0, err 0)
[2018/11/15 09:53:21:1166] USER: callback_minimal_spam: established (try 10, est 4, closed 0, err 0)
[2018/11/15 09:53:21:1531] USER: callback_minimal_spam: established (try 10, est 5, closed 0, err 0)
[2018/11/15 09:53:21:1563] USER: callback_minimal_spam: established (try 10, est 6, closed 0, err 0)
[2018/11/15 09:53:21:1589] USER: callback_minimal_spam: established (try 10, est 7, closed 0, err 0)
[2018/11/15 09:53:21:1616] USER: callback_minimal_spam: established (try 10, est 8, closed 0, err 0)
[2018/11/15 09:53:21:1671] USER: callback_minimal_spam: established (try 10, est 9, closed 0, err 0)
[2018/11/15 09:53:21:3778] USER: callback_minimal_spam: reopening (try 11, est 10, closed 1, err 0)
...
```


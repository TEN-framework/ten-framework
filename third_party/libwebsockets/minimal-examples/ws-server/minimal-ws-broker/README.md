# lws minimal ws broker

## build

```
 $ cmake . && make
```

## usage

```
 $ ./lws-minimal-ws-broker
[2018/03/15 12:23:12:1559] USER: LWS minimal ws broker | visit http://localhost:7681
[2018/03/15 12:23:12:1560] NOTICE: Creating Vhost 'default' port 7681, 2 protocols, IPv6 off
```

Visit http://localhost:7681 on multiple browser windows

The page opens a subscribe mode ws connection back to the broker,
and a publisher mode ws connection back to the broker.

The textarea shows the data from the subscription connection.

If you type text is in the text box and press send, the text
is passed to the broker on the publisher ws connection and
sent to all subscribers.

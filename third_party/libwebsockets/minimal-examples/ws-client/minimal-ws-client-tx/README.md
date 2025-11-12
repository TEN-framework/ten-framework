# lws minimal ws client tx

This demonstrates a ws "publisher" to go with the minimal-ws-broker example.

Two threads are spawned that produce messages to be sent to the broker,
via a local ringbuffer.  Locking is provided to make ringbuffer access threadsafe.

When a nailed-up client connection to the broker is established, the
ringbuffer is sent to the broker, which distributes the events to all
connected clients.

## build

```
 $ cmake . && make
```

## usage

This example connects to ws-server/minimal-ws-broker, so you need to build and run
that in another terminal.

```
 $ ./lws-minimal-ws-client-tx
[2018/03/16 16:04:33:5774] USER: LWS minimal ws client tx
[2018/03/16 16:04:33:5774] USER:   Run minimal-ws-broker and browse to that
[2018/03/16 16:04:33:5774] NOTICE: Creating Vhost 'default' port -1, 1 protocols, IPv6 off
[2018/03/16 16:04:34:5794] USER: callback_minimal_broker: established
```

If you open a browser on http://localhost:7681 , you will see the subscribed
messages from the threads in this app via the broker app.


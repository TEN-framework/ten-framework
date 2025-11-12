# lws minimal ws server (threadpool)

## build

```
 $ cmake . && make
```

Pthreads is required on your system.

This demonstrates how to cleanly assign tasks bound to a wsi to a thread pool,
with a queue if the pool is occupied.

It creates a threadpool with 3 worker threads and a maxiumum queue size of 4.

The web page at http://localhost:7681 then starts up 8 x ws connections.

## usage

```
 $ ./lws-minimal-ws-server-threadpool 
[2018/03/13 13:09:52:2208] USER: LWS minimal ws server + threadpool | visit http://localhost:7681
[2018/03/13 13:09:52:2365] NOTICE: Creating Vhost 'default' port 7681, 2 protocols, IPv6 off
```



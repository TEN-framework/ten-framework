# lws minimal ws server raw adopt udp

This example demonstrates echoing packets on a UDP socket in lws.

A "foreign" UDP socket is created, bound (so it can "listen"), and
adopted into lws event loop.  It acts like a tcp RAW mode connection in
lws and uses the same callbacks.

Writing is a bit different for UDP.  By default, the system has no
idea about the receiver state and so asking for a callback_on_writable()
always believes that the socket is writeable... the callback will
happen next time around the event loop if there are no pending partials.

With UDP, there is no "connection".  You need to write with sendto() and
direct the packets to a specific destination.  You can learn the source
of the last packet that arrived at the LWS_CALLBACK_RAW_RX callback by
getting a `struct lws_udp *` from `lws_get_udp(wsi)`.  To be able to
send back to that guy, you should take a copy of the `struct lws_udp *` and
use the .sa and .salen members in your sendto().

However the kernel may not accept to buffer / write everything you wanted to send.
So you are responsible to watch the result of sendto() and resend the
unsent part next time.

## build

```
 $ cmake . && make
```

## usage

```
 $ ./lws-minimal-raw-adopt-udp
$ ./lws-minimal-raw-adopt-udp 
[2018/03/24 08:12:37:8869] USER: LWS minimal raw adopt udp | nc -u 127.0.0.1 7681
[2018/03/24 08:12:37:8870] NOTICE: Creating Vhost 'default' (no listener), 1 protocols, IPv6 off
[2018/03/24 08:12:37:8878] USER: LWS_CALLBACK_RAW_ADOPT
[2018/03/24 08:12:41:5656] USER: LWS_CALLBACK_RAW_RX (6)
[2018/03/24 08:12:41:5656] NOTICE: 
[2018/03/24 08:12:41:5656] NOTICE: 0000: 68 65 6C 6C 6F 0A                                  hello.          
[2018/03/24 08:12:41:5656] NOTICE: 
```

```
 $ nc -u 127.0.0.1 7681
hello
hello
```

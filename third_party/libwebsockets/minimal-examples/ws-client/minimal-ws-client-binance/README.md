# lws minimal ws client binance

This connects to the binance ws server and monitors transactions with
an eye on low latency.

Latency seems to be associated with server-side coalescing at tls
layer, and the coalescing at server side seems somewhat correlated to number
of transactions per second, which seems to cause increased packet sizes from the
server as a reaction.  The relationship is more complex probably according to what
actually happens at the server backend, but it seems to be broadly related
reliably.

Typically when showing low latency at ~70msg/s, the messages on the wire are
eg, ~70 byte packets containing small tls records

10:14:40.682293 IP ec2-54-249-113-172.ap-northeast-1.compute.amazonaws.com.https > constance.42952: Flags [P.], seq 50846:50927, ack 1, win 11, options [nop,nop,TS val 366445630 ecr 3893437035], length 81

under pressure from increased messages per second, the tls records increase above 2KB

08:06:02.825160 IP ec2-54-249-113-172.ap-northeast-1.compute.amazonaws.com.https > constance.42688: Flags [.], seq 512319:513643, ack 1, win 11, options [nop,nop,TS val 3990208942 ecr 3885719233], length 1324
08:06:02.825290 IP constance.42688 > ec2-54-249-113-172.ap-northeast-1.compute.amazonaws.com.https: Flags [.], ack 513643, win 14248, options [nop,nop,TS val 3885719479 ecr 3990208942], length 0
08:06:02.891646 IP ec2-54-249-113-172.ap-northeast-1.compute.amazonaws.com.https > constance.42688: Flags [.], seq 513643:516291, ack 1, win 11, options [nop,nop,TS val 3990209006 ecr 3885719296], length 2648

The larger the packets, the longer the first item in the packet had to
wait before it was sent, and a tls record cannot be authenticated until
all of it has been received.

The example circumvents this somewhat by using `permessage_deflate`, which reduces
the packet size before tls by applying compression, making even coalesced packets
smaller, and a new option for adjusting how lws manages conflicting requirements to
clear pending rx and allow interleaved tx, `LCCSCF_PRIORITIZE_READS` that causes the
stream to prioritize handling any pending rx, not just pending at ssl layer, in one
event loop trip.

## build

Lws must have been built with `LWS_ROLE_WS=1` and `LWS_WITHOUT_EXTENSIONS=0`

```
 $ cmake . && make
```

## Commandline Options

Option|Meaning
---|---
-d|Set logging verbosity

## usage

```
$ ./bin/lws-minimal-ws-client-binance 
[2020/08/23 10:22:49:3003] U: LWS minimal binance client
[2020/08/23 10:22:49:3005] N: LWS: 4.0.99-v4.1.0-rc2-4-g3cf133aef, loglevel 1031
[2020/08/23 10:22:49:3005] N: NET CLI SRV H1 H2 WS MQTT SS-JSON-POL SSPROX ASYNC_DNS IPv6-absent
[2020/08/23 10:22:50:8243] N: checking client ext permessage-deflate
[2020/08/23 10:22:50:8244] N: instantiating client ext permessage-deflate
[2020/08/23 10:22:50:8244] U: callback_minimal: established
[2020/08/23 10:22:51:8244] N: sul_hz_cb: price: min: 1160284¢, max: 1163794¢, avg: 1160516¢, (150 prices/s)
[2020/08/23 10:22:51:8245] N: sul_hz_cb: elatency: min: 112ms, max: 547ms, avg: 259ms, (155 msg/s)
[2020/08/23 10:22:52:8244] N: sul_hz_cb: price: min: 1160287¢, max: 1178845¢, avg: 1160897¢, (112 prices/s)
[2020/08/23 10:22:52:8245] N: sul_hz_cb: elatency: min: 111ms, max: 226ms, avg: 152ms, (134 msg/s)
[2020/08/23 10:22:53:8247] N: sul_hz_cb: price: min: 1160287¢, max: 1168005¢, avg: 1160806¢, (86 prices/s)
[2020/08/23 10:22:53:8248] N: sul_hz_cb: elatency: min: 112ms, max: 476ms, avg: 287ms, (101 msg/s)
[2020/08/23 10:22:54:8247] N: sul_hz_cb: price: min: 1160284¢, max: 1162780¢, avg: 1160698¢, (71 prices/s)
...
```

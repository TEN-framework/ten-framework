# lws minimal secure streams

The application goes to https://warmcat.com and reads index.html there.

It does it using Secure Streams... the main code in minimal-secure-streams.c
just sets up the context and opens a secure stream of type "mintest".

The handler for state changes and payloads for "mintest" is in ss-myss.c

The information about how a "mintest" stream should connect and the
protocol it uses is kept separated in policy-database.c

## build

```
 $ cmake . && make
```

## usage

Commandline option|Meaning
---|---
-d <loglevel>|Debug verbosity in decimal, eg, -d15
-f| Force connecting to the wrong endpoint to check backoff retry flow
-p| Run as proxy server for clients to connect to over unix domain socket
--force-portal|Force the SS Captive Portal Detection to feel it's behind a portal
--force-no-internet|Force the SS Captive Portal Detection to feel it can't reach the internet
--blob|Download a 50MiB blob from warmact.com, using flow control at the proxy

```
[2019/08/12 07:16:11:0045] USR: LWS minimal secure streams [-d<verbosity>] [-f]
[2019/08/12 07:16:12:6102] USR: myss_state: LWSSSCS_CREATING, ord 0x0
[2019/08/12 07:16:12:6107] USR: myss_state: LWSSSCS_POLL, ord 0x0
[2019/08/12 07:16:12:6117] N: lws_ss_client_connect: connecting h1get warmcat.com /
[2019/08/12 07:16:12:6118] USR: myss_state: LWSSSCS_CONNECTING, ord 0x0
[2019/08/12 07:16:13:4171] USR: myss_state: LWSSSCS_CONNECTED, ord 0x0
[2019/08/12 07:16:13:4222] USR: myss_rx: len 1024, flags: 1
[2019/08/12 07:16:13:4243] USR: myss_rx: len 1024, flags: 0
[2019/08/12 07:16:13:4244] USR: myss_rx: len 1024, flags: 0
[2019/08/12 07:16:13:4244] USR: myss_rx: len 1024, flags: 0
[2019/08/12 07:16:13:4245] USR: myss_rx: len 1024, flags: 0
[2019/08/12 07:16:13:4246] USR: myss_rx: len 1024, flags: 0
[2019/08/12 07:16:13:4247] USR: myss_rx: len 1024, flags: 0
[2019/08/12 07:16:13:4252] USR: myss_rx: len 1015, flags: 0
[2019/08/12 07:16:13:4264] USR: myss_rx: len 1024, flags: 0
[2019/08/12 07:16:13:4265] USR: myss_rx: len 1024, flags: 0
[2019/08/12 07:16:13:4266] USR: myss_rx: len 1024, flags: 0
[2019/08/12 07:16:13:4267] USR: myss_rx: len 1024, flags: 0
[2019/08/12 07:16:13:4268] USR: myss_rx: len 1024, flags: 0
[2019/08/12 07:16:13:4268] USR: myss_rx: len 1024, flags: 0
[2019/08/12 07:16:13:4269] USR: myss_rx: len 1024, flags: 0
[2019/08/12 07:16:13:4270] USR: myss_rx: len 1015, flags: 0
[2019/08/12 07:16:13:4278] USR: myss_rx: len 1024, flags: 0
[2019/08/12 07:16:13:4279] USR: myss_rx: len 1024, flags: 0
[2019/08/12 07:16:13:4280] USR: myss_rx: len 1024, flags: 0
[2019/08/12 07:16:13:4281] USR: myss_rx: len 1024, flags: 0
[2019/08/12 07:16:13:4282] USR: myss_rx: len 1024, flags: 0
[2019/08/12 07:16:13:4283] USR: myss_rx: len 1024, flags: 0
[2019/08/12 07:16:13:4283] USR: myss_rx: len 1024, flags: 0
[2019/08/12 07:16:13:4284] USR: myss_rx: len 1015, flags: 0
[2019/08/12 07:16:13:4287] USR: myss_rx: len 1024, flags: 0
[2019/08/12 07:16:13:4288] USR: myss_rx: len 947, flags: 0
[2019/08/12 07:16:13:4293] USR: myss_rx: len 0, flags: 2
[2019/08/12 07:16:13:4399] USR: myss_state: LWSSSCS_DISCONNECTED, ord 0x0
[2019/08/12 07:16:13:4761] USR: myss_state: LWSSSCS_DESTROYING, ord 0x0
[2019/08/12 07:16:13:4781] USR: Completed: OK
```

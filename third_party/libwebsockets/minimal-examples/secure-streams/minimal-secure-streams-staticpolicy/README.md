# lws minimal secure streams static policy

The application goes to https://warmcat.com and reads index.html there.

It does it using a static Secure Streams policy generated from JSON by
policy2c example. 

## build

```
 $ cmake . && make
```

## usage

Commandline option|Meaning
---|---
-d <loglevel>|Debug verbosity in decimal, eg, -d15

```
$ ./lws-minimal-secure-streams-staticpolicy
[2020/03/26 15:49:12:6640] U: LWS secure streams static policy test client [-d<verb>]
[2020/03/26 15:49:12:7067] N: lws_create_context: using ss proxy bind '(null)', port 0, ads '(null)'
[2020/03/26 15:49:12:7567] N: lws_tls_client_create_vhost_context: using mem client CA cert 914
[2020/03/26 15:49:12:7597] N: lws_tls_client_create_vhost_context: using mem client CA cert 1011
[2020/03/26 15:49:12:7603] N: lws_tls_client_create_vhost_context: using mem client CA cert 1425
[2020/03/26 15:49:12:7605] N: lws_tls_client_create_vhost_context: using mem client CA cert 1011
[2020/03/26 15:49:12:9713] N: lws_system_cpd_set: setting CPD result OK
[2020/03/26 15:49:13:9625] N: ss_api_amazon_auth_rx: acquired 588-byte api.amazon.com auth token, exp 3600s
[2020/03/26 15:49:13:9747] U: myss_state: LWSSSCS_CREATING, ord 0x0
[2020/03/26 15:49:13:9774] U: myss_state: LWSSSCS_CONNECTING, ord 0x0
[2020/03/26 15:49:14:1897] U: myss_state: LWSSSCS_CONNECTED, ord 0x0
[2020/03/26 15:49:14:1926] U: myss_rx: len 1520, flags: 1
[2020/03/26 15:49:14:1945] U: myss_rx: len 1520, flags: 0
[2020/03/26 15:49:14:1946] U: myss_rx: len 1520, flags: 0
[2020/03/26 15:49:14:1947] U: myss_rx: len 1520, flags: 0
[2020/03/26 15:49:14:1948] U: myss_rx: len 1520, flags: 0
[2020/03/26 15:49:14:1949] U: myss_rx: len 583, flags: 0
[2020/03/26 15:49:14:2087] U: myss_rx: len 1520, flags: 0
[2020/03/26 15:49:14:2089] U: myss_rx: len 1520, flags: 0
[2020/03/26 15:49:14:2090] U: myss_rx: len 1520, flags: 0
[2020/03/26 15:49:14:2091] U: myss_rx: len 1520, flags: 0
[2020/03/26 15:49:14:2092] U: myss_rx: len 1520, flags: 0
[2020/03/26 15:49:14:2093] U: myss_rx: len 583, flags: 0
[2020/03/26 15:49:14:2109] U: myss_rx: len 1520, flags: 0
[2020/03/26 15:49:14:2110] U: myss_rx: len 1520, flags: 0
[2020/03/26 15:49:14:2111] U: myss_rx: len 1520, flags: 0
[2020/03/26 15:49:14:2112] U: myss_rx: len 1520, flags: 0
[2020/03/26 15:49:14:2113] U: myss_rx: len 1520, flags: 0
[2020/03/26 15:49:14:2114] U: myss_rx: len 583, flags: 0
[2020/03/26 15:49:14:2135] U: myss_rx: len 1520, flags: 0
[2020/03/26 15:49:14:2136] U: myss_rx: len 1358, flags: 0
[2020/03/26 15:49:14:2136] U: myss_rx: len 0, flags: 2
[2020/03/26 15:49:14:2138] U: myss_state: LWSSSCS_QOS_ACK_REMOTE, ord 0x0
[2020/03/26 15:49:14:2139] N: myss_state: LWSSSCS_QOS_ACK_REMOTE
[2020/03/26 15:49:14:2170] U: myss_state: LWSSSCS_DISCONNECTED, ord 0x0
[2020/03/26 15:49:14:2192] U: myss_state: LWSSSCS_DESTROYING, ord 0x0
[2020/03/26 15:49:14:2265] E: lws_context_destroy3
[2020/03/26 15:49:14:2282] U: Completed: OK

```

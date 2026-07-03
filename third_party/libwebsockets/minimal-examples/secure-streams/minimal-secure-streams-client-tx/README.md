# lws minimal secure streams client tx

The application connects to the secure stream proxy, and opens a streamtype
"spam"... this is a websocket connection to libwebsockets.org.

It then issues 100 x ws messages at 20Hz and exits.

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

```
[2021/02/19 11:25:20:1396] U: LWS secure streams client TX [-d<verb>]
[2021/02/19 11:25:20:1756] N: LWS: 4.1.99-v4.1.0-280-ga329c51485, loglevel 1031
[2021/02/19 11:25:20:1761] N: NET CLI SRV H1 H2 WS SS-JSON-POL SSPROX IPV6-on
[2021/02/19 11:25:20:2055] N:  ++ [1100944|wsi|0|pipe] (1)
[2021/02/19 11:25:20:2133] N:  ++ [1100944|vh|0|netlink] (1)
[2021/02/19 11:25:20:3647] N:  ++ [1100944|vh|1|default] (2)
[2021/02/19 11:25:20:8590] N:  ++ [1100944|SSPcli|0|spam] (1)
[2021/02/19 11:25:20:8810] N:  ++ [1100944|wsiSSPcli|0|RAW/raw-skt/+@proxy.ss.lws/([1100944|SSPcli|0|spam])] (1)
[2021/02/19 11:25:20:9103] N: lws_sspc_sul_retry_cb: [1100944|wsiSSPcli|0|RAW/raw-skt/+@proxy.ss.lws/([1100944|SSPcli|0|spam|default])]
[2021/02/19 11:25:20:9795] U: myss_state: LWSSSCS_CREATING, ord 0x0
[2021/02/19 11:25:20:9869] U: myss_state: LWSSSCS_CONNECTING, ord 0x0
[2021/02/19 11:25:21:0791] U: myss_state: LWSSSCS_CONNECTED, ord 0x0
[2021/02/19 11:25:21:1444] U: myss_tx: sending pkt 1
[2021/02/19 11:25:21:1945] U: myss_tx: sending pkt 2
[2021/02/19 11:25:21:2459] U: myss_tx: sending pkt 3
[2021/02/19 11:25:21:2971] U: myss_tx: sending pkt 4
...
```

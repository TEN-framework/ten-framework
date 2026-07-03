# lws minimal secure streams perf

The application goes to https://warmcat.com and reads index.html there.

The streamtype used is marked with a "perf": true policy, it returns additional
rx payload marked with the `LWSSS_FLAG_PERF_JSON` flag containing a JSON rundown
of the connection performance.

This builds both lws-minimal-secure-streams-perf that connects directly, and
lws-minimal-secure-streams-perf-client that connects via the proxy, giving the
same results.

## build

```
 $ cmake . && make
```

## usage

Commandline option|Meaning
---|---
-d <loglevel>|Debug verbosity in decimal, eg, -d15

```
[2021/03/31 15:29:46:5162] U: LWS secure streams test client [-d<verb>]
[2021/03/31 15:29:46:5625] N: LWS: 4.1.99-v4.2-rc1-50-g8b5acf835c, loglevel 1031
[2021/03/31 15:29:46:5629] N: NET CLI SRV H1 H2 WS SS-JSON-POL SSPROX ConMon IPV6-on
[2021/03/31 15:29:46:5829] N:  ++ [795209|wsi|0|pipe] (1)
[2021/03/31 15:29:46:5892] N:  ++ [795209|vh|0|netlink] (1)
[2021/03/31 15:29:46:5983] N:  ++ [795209|vh|1|default||-1] (2)
[2021/03/31 15:29:46:7638] N:  ++ [795209|SSPcli|0|mintest] (1)
[2021/03/31 15:29:46:7957] N:  ++ [795209|wsiSSPcli|0|RAW/raw-skt/+@proxy.ss.lws/([795209|SSPcli|0|mintest])] (1)
[2021/03/31 15:29:46:8335] N:  -- [795209|wsiSSPcli|0|RAW/raw-skt/+@proxy.ss.lws/([795209|SSPcli|0|mintest])] (0) 35.608ms
[2021/03/31 15:29:47:9096] N:  ++ [795209|wsiSSPcli|1|RAW/raw-skt/+@proxy.ss.lws/([795209|SSPcli|0|mintest])] (1)
[2021/03/31 15:29:47:9103] N:  -- [795209|wsiSSPcli|1|RAW/raw-skt/+@proxy.ss.lws/([795209|SSPcli|0|mintest])] (0) 215μs
[2021/03/31 15:29:48:9117] N:  ++ [795209|wsiSSPcli|2|RAW/raw-skt/+@proxy.ss.lws/([795209|SSPcli|0|mintest])] (1)
[2021/03/31 15:29:48:9339] N: lws_sspc_sul_retry_cb: [795209|wsiSSPcli|2|RAW/raw-skt/+@proxy.ss.lws/([795209|SSPcli|0|mintest])]
[2021/03/31 15:29:48:9625] N: lws_ss_check_next_state: [795209|SSPcli|0|mintest]: (unset) -> LWSSSCS_CREATING
[2021/03/31 15:29:48:9633] U: myss_state: LWSSSCS_CREATING (1), ord 0x0
[2021/03/31 15:29:48:9728] N: lws_ss_check_next_state: [795209|SSPcli|0|mintest]: LWSSSCS_CREATING -> LWSSSCS_CONNECTING
[2021/03/31 15:29:48:9731] U: myss_state: LWSSSCS_CONNECTING (6), ord 0x0
[2021/03/31 15:29:49:0670] N: lws_ss_deserialize_parse: RX METADATA test
[2021/03/31 15:29:49:0696] N: lws_ss_check_next_state: [795209|SSPcli|0|mintest]: LWSSSCS_CONNECTING -> LWSSSCS_CONNECTED
[2021/03/31 15:29:49:0698] U: myss_state: LWSSSCS_CONNECTED (5), ord 0x0
[2021/03/31 15:29:49:0716] N: lws_ss_deserialize_parse: RX METADATA srv
[2021/03/31 15:29:49:0882] U: myss_rx: len 1380, flags: 1, srv: lwsws, test: hello
[2021/03/31 15:29:49:0907] U: myss_rx: len 140, flags: 0, srv: lwsws, test: hello
[2021/03/31 15:29:49:0926] U: {"peer":"46.105.127.147","dns_us":536,"sockconn_us":30183,"tls_us":29343,"txn_resp_us":25990,"dns":["2001:41d0:2:ee93::1","46.105.127.147"]}
[2021/03/31 15:29:49:0937] U: myss_rx: len 1380, flags: 0, srv: lwsws, test: hello
[2021/03/31 15:29:49:0938] U: myss_rx: len 140, flags: 0, srv: lwsws, test: hello
[2021/03/31 15:29:49:0940] U: myss_rx: len 829, flags: 0, srv: lwsws, test: hello
[2021/03/31 15:29:49:0942] U: myss_rx: len 691, flags: 0, srv: lwsws, test: hello
[2021/03/31 15:29:49:0943] U: myss_rx: len 1380, flags: 0, srv: lwsws, test: hello
[2021/03/31 15:29:49:0944] U: myss_rx: len 140, flags: 0, srv: lwsws, test: hello
[2021/03/31 15:29:49:0945] U: myss_rx: len 1380, flags: 0, srv: lwsws, test: hello
[2021/03/31 15:29:49:0947] U: myss_rx: len 140, flags: 0, srv: lwsws, test: hello
[2021/03/31 15:29:49:0948] U: myss_rx: len 292, flags: 0, srv: lwsws, test: hello
[2021/03/31 15:29:49:0950] U: myss_rx: len 291, flags: 0, srv: lwsws, test: hello
[2021/03/31 15:29:49:0951] U: myss_rx: len 1380, flags: 0, srv: lwsws, test: hello
[2021/03/31 15:29:49:0952] U: myss_rx: len 140, flags: 0, srv: lwsws, test: hello
[2021/03/31 15:29:49:0953] U: myss_rx: len 1380, flags: 0, srv: lwsws, test: hello
[2021/03/31 15:29:49:0955] U: myss_rx: len 140, flags: 0, srv: lwsws, test: hello
[2021/03/31 15:29:49:0956] U: myss_rx: len 692, flags: 0, srv: lwsws, test: hello
[2021/03/31 15:29:49:0957] U: myss_rx: len 828, flags: 0, srv: lwsws, test: hello
[2021/03/31 15:29:49:0958] U: myss_rx: len 1380, flags: 0, srv: lwsws, test: hello
[2021/03/31 15:29:49:0960] U: myss_rx: len 140, flags: 0, srv: lwsws, test: hello
[2021/03/31 15:29:49:0961] U: myss_rx: len 1380, flags: 0, srv: lwsws, test: hello
[2021/03/31 15:29:49:0962] U: myss_rx: len 140, flags: 0, srv: lwsws, test: hello
[2021/03/31 15:29:49:0963] U: myss_rx: len 155, flags: 0, srv: lwsws, test: hello
[2021/03/31 15:29:49:0965] U: myss_rx: len 428, flags: 0, srv: lwsws, test: hello
[2021/03/31 15:29:49:0966] U: myss_rx: len 1380, flags: 0, srv: lwsws, test: hello
[2021/03/31 15:29:49:0967] U: myss_rx: len 140, flags: 0, srv: lwsws, test: hello
[2021/03/31 15:29:49:0968] U: myss_rx: len 1380, flags: 0, srv: lwsws, test: hello
[2021/03/31 15:29:49:0969] U: myss_rx: len 140, flags: 0, srv: lwsws, test: hello
[2021/03/31 15:29:49:0970] U: myss_rx: len 555, flags: 0, srv: lwsws, test: hello
[2021/03/31 15:29:49:0972] U: myss_rx: len 965, flags: 0, srv: lwsws, test: hello
[2021/03/31 15:29:49:0973] U: myss_rx: len 1380, flags: 0, srv: lwsws, test: hello
[2021/03/31 15:29:49:0975] U: myss_rx: len 140, flags: 0, srv: lwsws, test: hello
[2021/03/31 15:29:49:0976] U: myss_rx: len 1380, flags: 0, srv: lwsws, test: hello
[2021/03/31 15:29:49:0977] U: myss_rx: len 140, flags: 0, srv: lwsws, test: hello
[2021/03/31 15:29:49:0978] U: myss_rx: len 18, flags: 0, srv: lwsws, test: hello
[2021/03/31 15:29:49:0979] U: myss_rx: len 565, flags: 0, srv: lwsws, test: hello
[2021/03/31 15:29:49:0980] U: myss_rx: len 1380, flags: 0, srv: lwsws, test: hello
[2021/03/31 15:29:49:0981] U: myss_rx: len 140, flags: 0, srv: lwsws, test: hello
[2021/03/31 15:29:49:0982] U: myss_rx: len 1380, flags: 0, srv: lwsws, test: hello
[2021/03/31 15:29:49:0983] U: myss_rx: len 140, flags: 0, srv: lwsws, test: hello
[2021/03/31 15:29:49:0984] U: myss_rx: len 418, flags: 0, srv: lwsws, test: hello
[2021/03/31 15:29:49:0985] U: myss_rx: len 44, flags: 0, srv: lwsws, test: hello
[2021/03/31 15:29:49:0989] U: myss_rx: len 0, flags: 2, srv: lwsws, test: hello
[2021/03/31 15:29:49:0994] N: lws_ss_check_next_state: [795209|SSPcli|0|mintest]: LWSSSCS_CONNECTED -> LWSSSCS_QOS_ACK_REMOTE
[2021/03/31 15:29:49:0995] U: myss_state: LWSSSCS_QOS_ACK_REMOTE (10), ord 0x0
[2021/03/31 15:29:49:0998] N: myss_state: LWSSSCS_QOS_ACK_REMOTE
[2021/03/31 15:29:49:1008] N: lws_ss_check_next_state: [795209|SSPcli|0|mintest]: LWSSSCS_QOS_ACK_REMOTE -> LWSSSCS_DISCONNECTED
[2021/03/31 15:29:49:1010] U: myss_state: LWSSSCS_DISCONNECTED (2), ord 0x0
[2021/03/31 15:29:49:1106] N:  -- [795209|wsi|0|pipe] (0) 2.527s
[2021/03/31 15:29:49:1169] N:  -- [795209|vh|1|default||-1] (1) 2.518s
[2021/03/31 15:29:49:1172] N:  -- [795209|wsiSSPcli|2|RAW/raw-skt/+@proxy.ss.lws/([795209|SSPcli|0|mintest])] (0) 205.495ms
[2021/03/31 15:29:49:1174] N:  -- [795209|vh|0|netlink] (0) 2.528s
[2021/03/31 15:29:49:1203] N: lws_ss_check_next_state: [795209|SSPcli|0|mintest]: LWSSSCS_DISCONNECTED -> LWSSSCS_DESTROYING
[2021/03/31 15:29:49:1206] U: myss_state: LWSSSCS_DESTROYING (7), ord 0x0
[2021/03/31 15:29:49:1210] N:  -- [795209|SSPcli|0|mintest] (0) 2.357s
[2021/03/31 15:29:49:1292] U: Completed: OK (seen expected 0)
```

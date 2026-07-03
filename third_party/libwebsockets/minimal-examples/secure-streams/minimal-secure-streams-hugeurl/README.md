# lws minimal secure streams hugeurl

This application sends a huge url to httpbin.org, by default 4000 bytes in
a urlarg ?x=xxxxxx..., where the argument is a random string in hex.

Notice that httpbin.org has its own limit for urlsize, of 4094 bytes for
the entire URL.

## build

```
 $ cmake . && make
```

## usage

Commandline option|Meaning
---|---
-d <loglevel>|Debug verbosity in decimal, eg, -d15
-h <hugeurl size>|Default 4000
--h1|Force http/1.1 instead of default h2

```
[2021/03/02 16:38:00:2662] U: LWS secure streams hugeurl test client [-d<verb>][-h <urlarg len>]
[2021/03/02 16:38:00:2662] U: main: huge argument size: 4000 bytes
[2021/03/02 16:38:00:2662] N: LWS: 4.1.99-v4.1.0-294-g85c1fe07a7, loglevel 1031
[2021/03/02 16:38:00:2662] N: NET CLI SRV H1 H2 WS SS-JSON-POL SSPROX IPV6-on
[2021/03/02 16:38:00:2663] N:  ++ [1903157|wsi|0|pipe] (1)
[2021/03/02 16:38:00:2663] N:  ++ [1903157|vh|0|netlink] (1)
[2021/03/02 16:38:00:2677] N:  ++ [1903157|vh|1|_ss_default||-1] (2)
[2021/03/02 16:38:00:2736] N:  ++ [1903157|vh|2|arca1||-1] (3)
[2021/03/02 16:38:00:2798] N:  ++ [1903157|wsiSScli|0|captive_portal_detect] (1)
[2021/03/02 16:38:00:2798] N: lws_ss_check_next_state: [1903157|wsiSScli|0|captive_portal_detect]: (unset) -> LWSSSCS_CREATING
[2021/03/02 16:38:00:2798] N: lws_ss_check_next_state: [1903157|wsiSScli|0|captive_portal_detect]: LWSSSCS_CREATING -> LWSSSCS_POLL
[2021/03/02 16:38:00:2800] N: lws_ss_check_next_state: [1903157|wsiSScli|0|captive_portal_detect]: LWSSSCS_POLL -> LWSSSCS_CONNECTING
[2021/03/02 16:38:00:2801] N:  ++ [1903157|wsicli|0|GET/h1/connectivitycheck.android.com/([1903157|wsiSScli|0|captive_portal_det] (1)
[2021/03/02 16:38:00:3227] W: lws_metrics_hist_bump_priv_tagged: 'ss="captive_portal_detect",http_resp="204"'
[2021/03/02 16:38:00:3227] N: lws_ss_check_next_state: [1903157|wsiSScli|0|captive_portal_detect|204]: LWSSSCS_CONNECTING -> LWSSSCS_CONNECTED
[2021/03/02 16:38:00:3227] N: lws_ss_check_next_state: [1903157|wsiSScli|0|captive_portal_detect|204]: LWSSSCS_CONNECTED -> LWSSSCS_QOS_ACK_REMOTE
[2021/03/02 16:38:00:3227] N: lws_system_cpd_set: setting CPD result OK
[2021/03/02 16:38:00:3227] N: lws_ss_check_next_state: [1903157|wsiSScli|0|captive_portal_detect|204]: LWSSSCS_QOS_ACK_REMOTE -> LWSSSCS_DISCONNECTED
[2021/03/02 16:38:00:3228] N: lws_ss_check_next_state: [1903157|wsiSScli|0|captive_portal_detect|204]: LWSSSCS_DISCONNECTED -> LWSSSCS_DESTROYING
[2021/03/02 16:38:00:3228] N:  -- [1903157|wsiSScli|0|captive_portal_detect|204] (0) 42.928ms
[2021/03/02 16:38:00:3231] N:  -- [1903157|wsicli|0|GET/h1/connectivitycheck.android.com/([1903157|wsiSScli|0|captive_portal_det] (0) 42.994ms
[2021/03/02 16:38:00:3853] N:  ++ [1903157|wsiSScli|1|httpbin_anything] (1)
[2021/03/02 16:38:00:3854] N: lws_ss_check_next_state: [1903157|wsiSScli|1|httpbin_anything]: (unset) -> LWSSSCS_CREATING
[2021/03/02 16:38:00:3854] U: myss_state: LWSSSCS_CREATING (1), ord 0x0
[2021/03/02 16:38:00:3855] N: lws_ss_check_next_state: [1903157|wsiSScli|1|httpbin_anything]: LWSSSCS_CREATING -> LWSSSCS_CONNECTING
[2021/03/02 16:38:00:3855] U: myss_state: LWSSSCS_CONNECTING (6), ord 0x0
[2021/03/02 16:38:00:3855] N:  ++ [1903157|wsicli|1|GET/h1/httpbin.org/([1903157|wsiSScli|1|httpbin_anything])] (1)
[2021/03/02 16:38:00:6855] N:  ++ [1903157|mux|0|h2_sid1_(1903157|wsicli|1)] (1)
[2021/03/02 16:38:00:6857] N: secstream_h1: [1903157|wsiSScli|1|httpbin_anything] no handle / tx
[2021/03/02 16:38:00:7904] W: lws_metrics_hist_bump_priv_tagged: 'ss="httpbin_anything",http_resp="200"'
[2021/03/02 16:38:00:7904] N: lws_ss_check_next_state: [1903157|wsiSScli|1|httpbin_anything|200]: LWSSSCS_CONNECTING -> LWSSSCS_CONNECTED
[2021/03/02 16:38:00:7904] U: myss_state: LWSSSCS_CONNECTED (5), ord 0x0
[2021/03/02 16:38:00:7907] U: myss_rx: return hugeurl len 4000 matches OK
[2021/03/02 16:38:00:7907] N: lws_ss_check_next_state: [1903157|wsiSScli|1|httpbin_anything|200]: LWSSSCS_CONNECTED -> LWSSSCS_QOS_ACK_REMOTE
[2021/03/02 16:38:00:7907] U: myss_state: LWSSSCS_QOS_ACK_REMOTE (10), ord 0x0
[2021/03/02 16:38:00:7908] N: myss_state: LWSSSCS_QOS_ACK_REMOTE
[2021/03/02 16:38:00:7908] N:  -- [1903157|wsi|0|pipe] (0) 524.500ms
[2021/03/02 16:38:00:7908] N:  -- [1903157|mux|0|h2_sid1_(1903157|wsicli|1)] (0) 105.284ms
[2021/03/02 16:38:00:7912] N:  -- [1903157|vh|2|arca1||-1] (2) 517.621ms
[2021/03/02 16:38:00:7912] N:  -- [1903157|wsicli|1|GET/h1/httpbin.org/([1903157|wsiSScli|1|httpbin_anything|arca1|h2|h2])] (0) 405.690ms
[2021/03/02 16:38:00:7912] N:  -- [1903157|vh|0|netlink] (1) 524.918ms
[2021/03/02 16:38:00:7913] N: lws_ss_check_next_state: [1903157|wsiSScli|1|httpbin_anything|200]: LWSSSCS_QOS_ACK_REMOTE -> LWSSSCS_DISCONNECTED
[2021/03/02 16:38:00:7913] U: myss_state: LWSSSCS_DISCONNECTED (2), ord 0x0
[2021/03/02 16:38:00:7913] N: lws_ss_check_next_state: [1903157|wsiSScli|1|httpbin_anything|200]: LWSSSCS_DISCONNECTED -> LWSSSCS_DESTROYING
[2021/03/02 16:38:00:7913] U: myss_state: LWSSSCS_DESTROYING (7), ord 0x0
[2021/03/02 16:38:00:7913] N:  -- [1903157|wsiSScli|1|httpbin_anything|200] (0) 405.986ms
[2021/03/02 16:38:00:7925] N:  -- [1903157|vh|1|_ss_default||-1] (0) 524.844ms
[2021/03/02 16:38:00:7926] U: Completed: OK
```

# lws minimal secure streams server

The application sets up a tls + ws server on https://localhost:7681

It does it using Secure Streams... information about how the server should
operate is held in JSON policy in main.c

Visiting the server in a modern browser will fetch some html + JS, the JS will
create a ws link back to the server and the server will spam an incrementing
number that is displayed in the browser every 100ms.

The app also has a SS client that works, but it's disabled by default since
we're interested in server.

## build

```
 $ cmake . && make
```

## usage

Commandline option|Meaning
---|---
-d <loglevel>|Debug verbosity in decimal, eg, -d15

```
[2020/07/27 10:51:04:8994] U: LWS Secure Streams Server
[2020/07/27 10:51:04:9440] N: LWS: 4.0.99-v4.0.0-245-ge6eb4417a, loglevel 1031
[2020/07/27 10:51:04:9444] N: NET CLI SRV H1 H2 WS MQTT SS-JSON-POL SSPROX ASYNC_DNS IPv6-absent
[2020/07/27 10:51:05:1685] N: lws_adopt_descriptor_vhost2: wsi 0x5317d30, vhost system ss_handle (nil)
[2020/07/27 10:51:05:1753] N: lws_adopt_descriptor_vhost2: wsi 0x53182c0, vhost system ss_handle (nil)
[2020/07/27 10:51:05:2129] N: lws_ss_policy_parser_cb: server 'self_localhost' keep 52 0x5318cc0
[2020/07/27 10:51:05:2134] N: lws_ss_policy_parser_cb: server 'self_localhost_key' keep 53 0x5318cf8
[2020/07/27 10:51:05:2192] N: lws_ss_policy_ref_trust_store: le_via_isrg trust store initial 'isrg_root_x1'
[2020/07/27 10:51:05:7804] N: smd_cb: creating server stream
[2020/07/27 10:51:05:7851] N:  Vhost 'myserver' using TLS mode
[2020/07/27 10:51:05:8660] N:  SSL ECDH curve 'prime256v1'
[2020/07/27 10:51:06:1035] N:    vhost myserver: cert expiry: 729599d
[2020/07/27 10:51:06:1039] N: lws_ss_create: created server myserver
[2020/07/27 10:51:11:8650] N: lws_adopt_descriptor_vhost2: wsi 0x5b046e0, vhost myserver ss_handle 0x56e2be0
[2020/07/27 10:51:11:8672] U: myss_srv_state: 0x5b52f60 LWSSSCS_CREATING, ord 0x0
[2020/07/27 10:51:11:8693] U: myss_srv_state: 0x5b52f60 LWSSSCS_CONNECTING, ord 0x0
[2020/07/27 10:51:11:8696] U: myss_srv_state: 0x5b52f60 LWSSSCS_CONNECTED, ord 0x0
[2020/07/27 10:51:11:9743] U: myss_srv_state: 0x5ba2bd0 LWSSSCS_CREATING, ord 0x0
[2020/07/27 10:51:11:9747] U: myss_srv_state: 0x5ba2bd0 LWSSSCS_CONNECTING, ord 0x0
[2020/07/27 10:51:11:9747] U: myss_srv_state: 0x5ba2bd0 LWSSSCS_CONNECTED, ord 0x0
[2020/07/27 10:51:12:0192] U: myss_srv_state: 0x5bad0a0 LWSSSCS_CREATING, ord 0x0
[2020/07/27 10:51:12:0193] U: myss_srv_state: 0x5bad0a0 LWSSSCS_CONNECTING, ord 0x0
[2020/07/27 10:51:12:0194] U: myss_srv_state: 0x5bad0a0 LWSSSCS_CONNECTED, ord 0x0
[2020/07/27 10:51:12:0306] N: secstream_h1: LWS_CALLBACK_HTTP
[2020/07/27 10:51:12:0329] U: myss_srv_state: 0x5bad0a0 LWSSSCS_SERVER_TXN, ord 0x0
[2020/07/27 10:51:12:0481] N: lws_h2_ws_handshake: Server SS 0x5ba2bd0 .wsi 0x5ba27b0 switching to ws protocol
[2020/07/27 10:51:12:0484] U: myss_srv_state: 0x5ba2bd0 LWSSSCS_SERVER_UPGRADE, ord 0x0
[2020/07/27 10:51:12:0541] U: myss_srv_state: 0x5ba2bd0 LWSSSCS_CONNECTED, ord 0x0
[2020/07/27 10:51:12:1222] U: myss_srv_state: 0x5bd1100 LWSSSCS_CREATING, ord 0x0
[2020/07/27 10:51:12:1222] U: myss_srv_state: 0x5bd1100 LWSSSCS_CONNECTING, ord 0x0
[2020/07/27 10:51:12:1223] U: myss_srv_state: 0x5bd1100 LWSSSCS_CONNECTED, ord 0x0
[2020/07/27 10:51:12:1242] N: lws_h2_ws_handshake: Server SS 0x5bd1100 .wsi 0x5bd0ce0 switching to ws protocol
[2020/07/27 10:51:12:1243] U: myss_srv_state: 0x5bd1100 LWSSSCS_SERVER_UPGRADE, ord 0x0
[2020/07/27 10:51:12:1246] U: myss_srv_state: 0x5bd1100 LWSSSCS_CONNECTED, ord 0x0
^C[2020/07/27 10:51:15:2809] U: myss_srv_state: 0x5bad0a0 LWSSSCS_DISCONNECTED, ord 0x0
[2020/07/27 10:51:15:2838] U: myss_srv_state: 0x5bad0a0 LWSSSCS_DESTROYING, ord 0x0
[2020/07/27 10:51:15:2938] U: myss_srv_state: 0x5ba2bd0 LWSSSCS_DISCONNECTED, ord 0x0
[2020/07/27 10:51:15:2946] U: myss_srv_state: 0x5ba2bd0 LWSSSCS_DESTROYING, ord 0x0
[2020/07/27 10:51:15:2952] U: myss_srv_state: 0x5bd1100 LWSSSCS_DISCONNECTED, ord 0x0
[2020/07/27 10:51:15:2953] U: myss_srv_state: 0x5bd1100 LWSSSCS_DESTROYING, ord 0x0
[2020/07/27 10:51:15:2960] U: myss_srv_state: 0x5b52f60 LWSSSCS_DISCONNECTED, ord 0x0
[2020/07/27 10:51:15:2961] U: myss_srv_state: 0x5b52f60 LWSSSCS_DESTROYING, ord 0x0
[2020/07/27 10:51:15:3042] U: myss_srv_state: 0x56e2be0 LWSSSCS_DESTROYING, ord 0x0
[2020/07/27 10:51:15:3378] U: Completed: OK
```

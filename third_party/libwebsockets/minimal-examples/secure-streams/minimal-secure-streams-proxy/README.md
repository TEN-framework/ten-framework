# lws minimal secure streams proxy

Operates as a secure streams proxy, by default on a listening unix domain socket
"proxy.ss.lws" in the Linux abstract namespace.

Give -p <port> to have it listen on a specific tcp port instead.

## build

```
 $ cmake . && make
```

## usage

Commandline option|Meaning
---|---
-d <loglevel>|Debug verbosity in decimal, eg, -d15
-f| Force connecting to the wrong endpoint to check backoff retry flow
-p <port>|If not given, proxy listens on a Unix Domain Socket, if given listen on specified tcp port
-i <iface>|Optionally specify the UDS path (no -p) or network interface to bind to (if -p also given)

```
[2020/02/26 15:41:27:5768] U: LWS secure streams Proxy [-d<verb>]
[2020/02/26 15:41:27:5770] N: lws_ss_policy_set:     2.064KiB, pad 70%: hardcoded
[2020/02/26 15:41:27:5771] N: lws_tls_client_create_vhost_context: using mem client CA cert 1391
[2020/02/26 15:41:27:8681] N: lws_ss_policy_set:     4.512KiB, pad 15%: updated
[2020/02/26 15:41:27:8682] N: lws_tls_client_create_vhost_context: using mem client CA cert 837
[2020/02/26 15:41:27:8683] N: lws_tls_client_create_vhost_context: using mem client CA cert 1043
[2020/02/26 15:41:27:8684] N: lws_tls_client_create_vhost_context: using mem client CA cert 1167
[2020/02/26 15:41:27:8684] N: lws_tls_client_create_vhost_context: using mem client CA cert 1391
[2020/02/26 15:41:28:4226] N: ss_api_amazon_auth_rx: acquired 567-byte api.amazon.com auth token, exp 3600s
```

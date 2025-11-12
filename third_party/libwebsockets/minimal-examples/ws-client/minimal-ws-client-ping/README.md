# lws minimal ws client PING

This connects to libwebsockets.org using the lws-mirror-protocol.

It sets a validity regime of testing validity with PING every 3s and failing
if it didn't get the PONG back within 10s.

## build

```
 $ cmake . && make
```

## Commandline Options

Option|Meaning
---|---
-d|Set logging verbosity (you want 1039 to see the validity ping / pong)
--server|Use a specific server instead of libwebsockets.org, eg `--server localhost`.  Implies LCCSCF_ALLOW_SELFSIGNED
--port|Use a specific port instead of 443, eg `--port 7681`
--protocol|Use a specific ws subprotocol rather than lws-mirror-protocol, eg, `--protocol myprotocol`


## usage

Just run it, wait for the connect and then there will be PINGs sent
at 5s intervals.

```
 $ ./lws-minimal-ws-client-ping -d1039
[2020/03/18 13:13:47:1114] U: LWS minimal ws client PING
[2020/03/18 13:13:47:1503] I: Initial logging level 1039
[2020/03/18 13:13:47:1507] I: Libwebsockets version: 4.0.99 v4.0.0-20-gc6165f868
[2020/03/18 13:13:47:1508] I: IPV6 not compiled in
[2020/03/18 13:13:47:1512] I:  LWS_DEF_HEADER_LEN    : 4096
[2020/03/18 13:13:47:1514] I:  LWS_MAX_SMP           : 1
[2020/03/18 13:13:47:1519] I:  sizeof (*info)        : 720
[2020/03/18 13:13:47:1520] I:  SYSTEM_RANDOM_FILEPATH: '/dev/urandom'
[2020/03/18 13:13:47:1522] I:  HTTP2 support         : available
[2020/03/18 13:13:47:1552] N: lws_create_context: using ss proxy bind '(null)', port 0, ads '(null)'
[2020/03/18 13:13:47:1557] I: context created
[2020/03/18 13:13:47:1575] I: Using event loop: poll
[2020/03/18 13:13:47:1583] I: Default ALPN advertisment: h2,http/1.1
[2020/03/18 13:13:47:1585] I:  default timeout (secs): 20
[2020/03/18 13:13:47:1614] I:  Threads: 1 each 5 fds
[2020/03/18 13:13:47:1623] I:  mem: context:          8152 B (4056 ctx + (1 thr x 4096))
[2020/03/18 13:13:47:1625] I:  mem: http hdr size:   (4096 + 976), max count 5
[2020/03/18 13:13:47:1629] I:  mem: pollfd map:         40 B
[2020/03/18 13:13:47:1633] I:  mem: platform fd map:    40 B
[2020/03/18 13:13:47:1692] I:  Compiled with OpenSSL support
[2020/03/18 13:13:47:1695] I: Doing SSL library init
[2020/03/18 13:13:47:3103] I:  canonical_hostname = constance
[2020/03/18 13:13:47:3140] I: Creating Vhost 'default' (serving disabled), 4 protocols, IPv6 off
[2020/03/18 13:13:47:4072] I: lws_tls_client_create_vhost_context: vh default: created new client ctx 0
[2020/03/18 13:13:47:7468] I: created client ssl context for default
[2020/03/18 13:13:47:7482] I: Creating Vhost 'default' (serving disabled), 4 protocols, IPv6 off
[2020/03/18 13:13:47:7490] I: lws_tls_client_create_vhost_context: vh default: reusing client ctx 0: use 2
[2020/03/18 13:13:47:7491] I: created client ssl context for default
[2020/03/18 13:13:47:7494] I:  mem: per-conn:          792 bytes + protocol rx buf
[2020/03/18 13:13:47:7497] I: lws_plat_drop_app_privileges: not changing group
[2020/03/18 13:13:47:7499] I: lws_plat_drop_app_privileges: not changing user
[2020/03/18 13:13:47:7512] I: lws_cancel_service
[2020/03/18 13:13:47:7568] I: lws_state_notify_protocol_init: LWS_SYSTATE_CPD_PRE_TIME
[2020/03/18 13:13:47:7577] N: lws_ss_create: unknown stream type captive_portal_detect
[2020/03/18 13:13:47:7580] I: lws_ss_sys_cpd: Create stream failed (policy?)
[2020/03/18 13:13:47:7582] I: lws_state_notify_protocol_init: LWS_SYSTATE_CPD_PRE_TIME
[2020/03/18 13:13:47:7582] N: lws_ss_create: unknown stream type captive_portal_detect
[2020/03/18 13:13:47:7583] I: lws_ss_sys_cpd: Create stream failed (policy?)
[2020/03/18 13:13:47:7585] I: lws_state_notify_protocol_init: doing protocol init on POLICY_VALID
[2020/03/18 13:13:47:7588] I: lws_protocol_init
[2020/03/18 13:13:47:7623] I: lws_state_transition_steps: CONTEXT_CREATED -> OPERATIONAL
[2020/03/18 13:13:47:7628] N: connect_cb: connecting
[2020/03/18 13:13:47:7656] I: lws_client_connect_via_info: role binding to h1
[2020/03/18 13:13:47:7662] I: lws_client_connect_via_info: protocol binding to lws-ping-test
[2020/03/18 13:13:47:7699] I: lws_client_connect_via_info: wsi 0x5669090: h1 lws-ping-test entry
[2020/03/18 13:13:47:7720] I: lws_header_table_attach: wsi 0x5669090: ah (nil) (tsi 0, count = 0) in
[2020/03/18 13:13:47:7729] I: _lws_create_ah: created ah 0x5669620 (size 4096): pool length 1
[2020/03/18 13:13:47:7735] I: lws_header_table_attach: did attach wsi 0x5669090: ah 0x5669620: count 1 (on exit)
[2020/03/18 13:13:47:7780] I: lws_client_connect_2_dnsreq: 0x5669090: lookup libwebsockets.org:443
[2020/03/18 13:13:47:8784] I: lws_getaddrinfo46: getaddrinfo 'libwebsockets.org' says 0
[2020/03/18 13:13:47:8804] I: lws_client_connect_3_connect: libwebsockets.org ipv4 46.105.127.147
[2020/03/18 13:13:47:9176] I: lws_client_connect_3_connect: getsockopt check: conn OK
[2020/03/18 13:13:47:9179] I: lws_client_connect_3_connect: Connection started 0x5682cc0
[2020/03/18 13:13:47:9197] I: lws_client_connect_4_established: wsi 0x5669090: h1 lws-ping-test client created own conn (raw 0) vh defaultm st 0x202
[2020/03/18 13:13:47:9418] I: h1 client conn using alpn list 'http/1.1'
[2020/03/18 13:13:48:4523] I: lws_role_call_alpn_negotiated: 'http/1.1'
[2020/03/18 13:13:48:4531] I: client connect OK
[2020/03/18 13:13:48:4543] I: lws_openssl_describe_cipher: wsi 0x5669090: TLS_AES_256_GCM_SHA384, TLS_AES_256_GCM_SHA384, 256 bits, TLSv1.3
[2020/03/18 13:13:48:4717] I: lws_client_socket_service: HANDSHAKE2: 0x5669090: sending headers (wsistate 0x10000204), w sock 5
[2020/03/18 13:13:48:4992] I: lws_buflist_aware_read: wsi 0x5669090: lws_client_socket_service: ssl_capable_read -4
[2020/03/18 13:13:48:5005] I: lws_buflist_aware_read: wsi 0x5669090: lws_client_socket_service: ssl_capable_read 174
[2020/03/18 13:13:48:5166] I: __lws_header_table_detach: wsi 0x5669090: ah 0x5669620 (tsi=0, count = 1)
[2020/03/18 13:13:48:5171] I: __lws_header_table_detach: nobody usable waiting
[2020/03/18 13:13:48:5175] I: _lws_destroy_ah: freed ah 0x5669620 : pool length 0
[2020/03/18 13:13:48:5180] I: __lws_header_table_detach: wsi 0x5669090: ah 0x5669620 (tsi=0, count = 0)
[2020/03/18 13:13:48:5197] I: _lws_validity_confirmed_role: wsi 0x5669090: setting validity timer 3s (hup 0)
[2020/03/18 13:13:48:5208] U: callback_minimal_broker: established
[2020/03/18 13:13:51:5218] I: lws_validity_cb: wsi 0x5669090: scheduling validity check
[2020/03/18 13:13:51:5325] I: rops_handle_POLLOUT_ws: issuing ping on wsi 0x5669090: ws lws-ping-test h2: 0
[2020/03/18 13:13:51:5504] I: lws_issue_raw: ssl_capable_write (6) says 6
[2020/03/18 13:13:51:5809] I: lws_ws_client_rx_sm: client 0x5669090 received pong
[2020/03/18 13:13:51:5819] I: _lws_validity_confirmed_role: wsi 0x5669090: setting validity timer 3s (hup 0)
[2020/03/18 13:13:51:5831] I: Client doing pong callback
[2020/03/18 13:13:54:5821] I: lws_validity_cb: wsi 0x5669090: scheduling validity check
[2020/03/18 13:13:54:5825] I: rops_handle_POLLOUT_ws: issuing ping on wsi 0x5669090: ws lws-ping-test h2: 0
[2020/03/18 13:13:54:5833] I: lws_issue_raw: ssl_capable_write (6) says 6
[2020/03/18 13:13:54:6258] I: lws_ws_client_rx_sm: client 0x5669090 received pong
[2020/03/18 13:13:54:6261] I: _lws_validity_confirmed_role: wsi 0x5669090: setting validity timer 3s (hup 0)
[2020/03/18 13:13:54:6263] I: Client doing pong callback
[2020/03/18 13:13:57:6263] I: lws_validity_cb: wsi 0x5669090: scheduling validity check
[2020/03/18 13:13:57:6267] I: rops_handle_POLLOUT_ws: issuing ping on wsi 0x5669090: ws lws-ping-test h2: 0
[2020/03/18 13:13:57:6275] I: lws_issue_raw: ssl_capable_write (6) says 6
[2020/03/18 13:13:58:0034] I: lws_ws_client_rx_sm: client 0x5669090 received pong

...
```


# lws minimal secure streams binance

This is a Secure Streams version of minimal-ws-client-binance.

"policy.json" contains all the information about endpoints, protocols and
connection validation, tagged by streamtype name.

The example tries to load it from the cwd, it lives in
./minimal-examples/secure-streams/minimal-secure-streams-binance dir, so
either run it from there, or copy the policy.json to your cwd.  It's also
possible to put the policy json in the code as a string and pass that at
context creation time.

The secure stream object represents a nailed-up connection that outlives any
single socket connection, and can manage reconnections / retries according to
the policy to keep the connection nailed up automatically.

Secure Streams provides the same simplified communication api without any
protocol dependencies.

## build

Lws must have been built with `LWS_ROLE_WS=1`, `LWS_WITH_SECURE_STREAMS=1`, and
`LWS_WITHOUT_EXTENSIONS=0`

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
[2021/08/15 06:42:40:8409] U: LWS minimal Secure Streams binance client
[2021/08/15 06:42:40:8410] N: LWS: 4.2.99-v4.2.0-156-g8f352f65e8, NET CLI SRV H1 H2 WS SS-JSON-POL SSPROX ConMon FLTINJ IPV6-on
[2021/08/15 06:42:40:8410] N:  ++ [495958|wsi|0|pipe] (1)
[2021/08/15 06:42:40:8411] N:  ++ [495958|vh|0|netlink] (1)
[2021/08/15 06:42:40:8433] N:  ++ [495958|vh|1|digicert||-1] (2)
[2021/08/15 06:42:40:8471] N:  ++ [495958|wsiSScli|0|binance] (1)
[2021/08/15 06:42:40:8471] N: [495958|wsiSScli|0|binance]: lws_ss_check_next_state_ss: (unset) -> LWSSSCS_CREATING
[2021/08/15 06:42:40:8472] N: [495958|wsiSScli|0|binance]: lws_ss_check_next_state_ss: LWSSSCS_CREATING -> LWSSSCS_CONNECTING
[2021/08/15 06:42:40:8472] N:  ++ [495958|wsicli|0|WS/h1/fstream.binance.com/([495958|wsiSScli|0|binance])] (1)
[2021/08/15 06:42:41:8802] N: [495958|wsiSScli|0|binance]: lws_ss_check_next_state_ss: LWSSSCS_CONNECTING -> LWSSSCS_CONNECTED
[2021/08/15 06:42:42:8803] N: sul_hz_cb: price: min: 4669185¢, max: 4672159¢, avg: 4670061¢, (53 prices/s)
[2021/08/15 06:42:42:8803] N: sul_hz_cb: elatency: min: 131ms, max: 292ms, avg: 154ms, (53 msg/s)
[2021/08/15 06:42:43:8803] N: sul_hz_cb: price: min: 4669646¢, max: 4672159¢, avg: 4669953¢, (34 prices/s)
[2021/08/15 06:42:43:8803] N: sul_hz_cb: elatency: min: 130ms, max: 149ms, avg: 133ms, (34 msg/s)
[2021/08/15 06:42:44:8804] N: sul_hz_cb: price: min: 4669455¢, max: 4672159¢, avg: 4669904¢, (26 prices/s)
...
```

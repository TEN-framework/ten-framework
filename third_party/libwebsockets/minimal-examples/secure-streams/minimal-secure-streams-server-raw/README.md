# lws minimal secure streams server raw

The application sets up a raw tcp server on localhost:7681

It does it using Secure Streams... information about how the server should
operate is held in JSON policy in main.c

Connecting to the server using `echo "hello" | nc --no-shutdown 127.0.0.1 7681`
will send "hello" which is hexdumped to console by the rx function, then
will receive an incrementing message at 100ms intervals.

Note there are two incomaptible versions of netcat around, this is from Fedora's
nmap-ncat, the --no-shutdown is needed to stop it hanging up itself after it
has sent its stdin.

## build

```
 $ cmake . && make
```

## usage

Commandline option|Meaning
---|---
-d <loglevel>|Debug verbosity in decimal, eg, -d15

```
[2020/07/28 10:25:54:6747] U: LWS Secure Streams Server Raw
[2020/07/28 10:25:54:7194] N: LWS: 4.0.99-v4.0.0-247-g58be599aa, loglevel 1031
[2020/07/28 10:25:54:7198] N: NET CLI SRV H1 H2 WS MQTT SS-JSON-POL SSPROX ASYNC_DNS IPv6-absent
[2020/07/28 10:25:54:9376] N: lws_adopt_descriptor_vhost2: wsi 0x5317d30, vhost system ss_handle (nil)
[2020/07/28 10:25:54:9442] N: lws_adopt_descriptor_vhost2: wsi 0x53182c0, vhost system ss_handle (nil)
[2020/07/28 10:25:54:9920] N: smd_cb: creating server stream
[2020/07/28 10:25:54:9963] N: lws_ss_create: created server myrawserver
[2020/07/28 10:26:00:1065] N: secstream_raw: RAW_ADOPT
[2020/07/28 10:26:00:1068] N: lws_adopt_descriptor_vhost2: wsi 0x531a6b0, vhost myrawserver ss_handle 0x5319ac0
[2020/07/28 10:26:00:1088] U: myss_raw_state: 0x531aad0 LWSSSCS_CREATING, ord 0x0
[2020/07/28 10:26:00:1094] U: myss_raw_state: 0x531aad0 LWSSSCS_CONNECTING, ord 0x0
[2020/07/28 10:26:00:1096] U: myss_raw_state: 0x531aad0 LWSSSCS_CONNECTED, ord 0x0
[2020/07/28 10:26:00:1172] U: myss_raw_rx: len 6, flags: 0
[2020/07/28 10:26:02:8516] U: myss_raw_state: 0x531aad0 LWSSSCS_DISCONNECTED, ord 0x0
[2020/07/28 10:26:02:8545] U: myss_raw_state: 0x531aad0 LWSSSCS_DESTROYING, ord 0x0
^C[2020/07/28 10:26:04:9608] U: myss_raw_state: 0x5319ac0 LWSSSCS_DESTROYING, ord 0x0
[2020/07/28 10:26:04:9723] U: Completed: OK
```

```
$ echo "hello" | nc --no-shutdown 127.0.0.1 7681
hello from raw 0
hello from raw 1
hello from raw 2
...
```

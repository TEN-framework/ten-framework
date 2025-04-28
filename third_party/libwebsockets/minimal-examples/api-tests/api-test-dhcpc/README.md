# api test dhcpc

The application confirms it can set DHCP on the given interface

## build

```
 $ cmake . && make
```

## usage

Commandline option|Meaning
---|---
-d <loglevel>|Debug verbosity in decimal, eg, -d15
-i <netif>|Network interface name to set by DHCP, eg, eth0 or wlo1

```
 $ ./lws-api-test-dhcpc -i wlo1
[2019/10/06 14:56:41:7683] U: LWS API selftest: Async DNS
[2019/10/06 14:56:42:4461] U: main: requesting DHCP for wlo1
[2019/10/06 14:56:42:5207] N: callback_dhcpc: DHCP configured wlo1
[2019/10/06 14:56:42:5246] U: lws_dhcpc_cb: dhcp set OK
[2019/10/06 14:56:42:5999] U: Completed: ALL PASS: 1 / 1
```



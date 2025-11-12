# lws minimal raw serial example

This demonstrates adopting a file descriptor representing a serial device
into the event loop, printing a string on it every couple of seconds and
showing any serial that is received.

The serial terminal is configured for 115200 8N1.


```
 $ ./lws-minimal-raw-serial <tty, eg, /dev/ttyUSB0>
```


## build

```
 $ cmake . && make
```

## usage

```
[2019/12/08 16:30:53:4436] U: LWS minimal raw serial
[2019/12/08 16:30:53:5016] E: callback_ntpc: set up system ops for set_clock
[2019/12/08 16:30:54:8061] N: callback_ntpc: Unix time: 1575822654
[2019/12/08 16:30:54:8253] N: LWS_CALLBACK_RAW_ADOPT_FILE
[2019/12/08 16:30:54:8364] N: callback_ntpc: LWS_CALLBACK_RAW_CLOSE
[2019/12/08 16:30:54:8456] N: LWS_CALLBACK_RAW_WRITEABLE_FILE
[2019/12/08 16:30:56:8455] N: LWS_CALLBACK_RAW_WRITEABLE_FILE
[2019/12/08 16:30:58:8460] N: LWS_CALLBACK_RAW_WRITEABLE_FILE
[2019/12/08 16:30:59:1570] N: LWS_CALLBACK_RAW_RX_FILE
[2019/12/08 16:30:59:1604] N: 
[2019/12/08 16:30:59:1641] N: 0000: 62                                                 b               
[2019/12/08 16:30:59:1644] N: 
[2019/12/08 16:31:00:8463] N: LWS_CALLBACK_RAW_WRITEABLE_FILE
[2019/12/08 16:31:01:6392] N: LWS_CALLBACK_RAW_RX_FILE
[2019/12/08 16:31:01:6397] N: 
[2019/12/08 16:31:01:6407] N: 0000: 65                                                 e               
[2019/12/08 16:31:01:6411] N: 
[2019/12/08 16:31:02:8463] N: LWS_CALLBACK_RAW_WRITEABLE_FILE
...                                               .               

```

The remote serial connection will show the string sent every 2s.

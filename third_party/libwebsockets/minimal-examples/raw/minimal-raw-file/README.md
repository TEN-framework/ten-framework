# lws minimal ws server

This demonstrates adopting a file descriptor into the lws event
loop.  The filepath to open and adopt is given as an argument to the example app, eg

```
 $ ./lws-minimal-raw-file <file>
```

On a Linux system, some example files for testing might be

 - /proc/self/fd/0      (stdin)
 - /dev/ttyUSB0         (a USB <-> serial converter)
 - /dev/input/event<n>  (needs root... input device events)

The example application opens the file in the protocol init
handler, and hexdumps data from the file to the lws log
as it becomes available.

This isn't very useful standalone as shown here for clarity, but you can
freely combine raw file descriptor adoption with other lws server
and client features.

Becuase raw file events have their own callback reasons, the handlers can
be integrated in a single protocol that also handles http and ws
server and client callbacks without conflict.

## build

```
 $ cmake . && make
```

## usage

```
 $ ./lws-minimal-raw-file /proc/self/fd/0
[2018/03/22 10:48:53:9709] USER: LWS minimal raw file
[2018/03/22 10:48:53:9876] NOTICE: Creating Vhost 'default' port -2, 1 protocols, IPv6 off
[2018/03/22 10:48:55:0037] NOTICE: LWS_CALLBACK_RAW_ADOPT_FILE

[2018/03/22 10:48:55:9370] NOTICE: LWS_CALLBACK_RAW_RX_FILE
[2018/03/22 10:48:55:9377] NOTICE: 
[2018/03/22 10:48:55:9408] NOTICE: 0000: 0A                                                 .               

```

The example logs above show the result of typing the Enter key.

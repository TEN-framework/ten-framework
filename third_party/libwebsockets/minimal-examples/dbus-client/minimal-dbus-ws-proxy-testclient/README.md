# lws minimal dbus ws proxy testclient

This is a test client used to test `./minimal-examples/dbus-server/minimal-dbus-ws-proxy`

It asks the minimal dbus ws proxy application to connect to libwebsockets.org
over the mirror protocol.  And it proxies back the ASCII packets used to
communicate the mirror sample drawing vectors over dbus to this test client
if you draw on the [mirror example app](https://libwebsockets.org/testserver/)
in a browser.

## build

Using libdbus requires additional non-default include paths setting, same as
is necessary for lws build described in ./lib/roles/dbus/README.md

CMake can guess one path and the library name usually, see the README above
for details of how to override for custom libdbus and cross build.

Fedora example:
```
$ cmake .. -DLWS_DBUS_INCLUDE2="/usr/lib64/dbus-1.0/include"
$ make
```

Ubuntu example:
```
$ cmake .. -DLWS_DBUS_INCLUDE2="/usr/lib/x86_64-linux-gnu/dbus-1.0/include"
$ make
```

## usage

Commandline option|Meaning
---|---
-d <loglevel>|Debug verbosity in decimal, eg, -d15

This connects to the minimal-dbus-ws-proxy example running in another terminal.

```
 $ ./lws-minimal-dbus-ws-proxy-testclient
[2018/10/05 14:17:16:6286] USER: LWS minimal DBUS ws proxy testclient
[2018/10/05 14:17:16:6538] NOTICE: Creating Vhost 'default' port 0, 1 protocols, IPv6 off
[2018/10/05 14:17:16:6617] USER: create_dbus_client_conn: connecting to 'unix:abstract=org.libwebsockets.wsclientproxy'
[2018/10/05 14:17:16:7189] NOTICE: create_dbus_client_conn: created OK
[2018/10/05 14:17:16:7429] USER: remote_method_call: requesting proxy connection wss://libwebsockets.org/ lws-mirror-protocol
[2018/10/05 14:17:17:0387] USER: pending_call_notify: received 'Connecting'
[2018/10/05 14:17:18:7475] NOTICE: client_message_handler: (type 7) 'ws client connection established'
[2018/10/05 14:17:21:2028] NOTICE: client_message_handler: (type 6) 'd #000000 323 63 323 67;'
[2018/10/05 14:17:21:2197] NOTICE: client_message_handler: (type 6) 'd #000000 323 67 327 73;'
...
```


# lws minimal dbus ws proxy

This is an application which presents a DBUS server on one side, and a
websocket client proxy on the other.

You connect to it over DBUS, send a Connect method on its interface giving
a URI and a ws subprotocol name.

It replies with a string "Connecting" if all is well.

Connection progress (including close) is then provided using type 7 messages
sent back to the dbus client.

Payload from the ws connection is provided using type 6 messages sent back to
the dbus client.

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

## Configuration

The dbus-ws-proxy server tries to register its actual bus name with the SYSTEM
bus in DBUS.  If it fails, eg because of insufficient permissions on the user,
then it continues without that and starts its own daemon normally.

The main dbus daemon must be told how to accept these registrations if that's
what you want.  A config file is provided that tells dbus to allow the
well-known busname for this daemon to be registered, but only by root.

``` 
$ sudo cp org.libwebsockets.wsclientproxy.conf /etc/dbus-1/system.d
$ sudo systemctl restart dbus
```

## usage

Run the dbus-ws-proxy server, then start lws-minimal-dbus-ws-proxy-testclient in
another terminal.

This test app sends a random line drawing message to the mirror example on
https://libwebsockets.org/testserver every couple of seconds, and displays
any received messages (such as its own sends mirrored back, or anything
drawn in the canvas in a browser).

```
 $ sudo ./lws-minimal-dbus-ws-proxy-testclient
[2018/10/07 10:05:29:2084] USER: LWS minimal DBUS ws proxy testclient
[2018/10/07 10:05:29:2345] NOTICE: Creating Vhost 'default' port 0, 1 protocols, IPv6 off
[2018/10/07 10:05:29:2424] USER: create_dbus_client_conn: connecting to 'unix:abstract=org.libwebsockets.wsclientproxy'
[2018/10/07 10:05:29:2997] NOTICE: state_transition: 0x5679720: from state 0 -> 1
[2018/10/07 10:05:29:2999] NOTICE: create_dbus_client_conn: created OK
[2018/10/07 10:05:29:3232] USER: remote_method_call: requesting proxy connection wss://libwebsockets.org/ lws-mirror-protocol
[2018/10/07 10:05:29:3450] NOTICE: state_transition: 0x5679720: from state 1 -> 2
[2018/10/07 10:05:29:5972] USER: pending_call_notify: received 'Connecting'
[2018/10/07 10:05:31:3387] NOTICE: state_transition: 0x5679720: from state 2 -> 3
[2018/10/07 10:05:33:6672] USER: filter: Received 'd #B0DC51 115 177 166 283;'
[2018/10/07 10:05:35:9723] USER: filter: Received 'd #E87CCD 9 192 106 235;'
[2018/10/07 10:05:38:2784] USER: filter: Received 'd #E2A9E3 379 290 427 62;'
[2018/10/07 10:05:39:5833] USER: filter: Received 'd #B127F8 52 126 60 226;'
[2018/10/07 10:05:41:8908] USER: filter: Received 'd #0E0F76 429 267 8 11;'
...
```

## ws proxy DBUS details

### Fixed details

Item|Value
---|---
Address|unix:abstract=org.libwebsockets.wsclientproxy
Interface|org.libwebsockets.wsclientproxy
Bus Name|org.libwebsockets.wsclientproxy
Object path|/org/libwebsockets/wsclientproxy

### Interface Methods

Method|Arguments|Returns
---|---|---
Connect|s: ws URI, s: ws subprotocol name|"Bad Uri", "Connecting" or "Failed"
Send|s: payload|Empty message if no problem, or error message

When Connecting, the actual connection happens asynchronously if the initial
connection attempt doesn't fail immediately.  If it's continuing in the
background, the reply will have been "Connecting".

### Signals

Signal Name|Argument|Meaning
---|---|---
Receive|s: payload|Received data from the ws link
Status|s: status|See table below

Status String|Meaning
---|---
"ws client connection error"|The ws connection attempt ended with a fatal error
"ws client connection established"|The ws connection attempt succeeded
"ws client connection closed"|The ws connection has closed


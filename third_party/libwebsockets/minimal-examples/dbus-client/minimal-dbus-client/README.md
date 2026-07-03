# lws minimal dbus client

This demonstrates nonblocking, asynchronous dbus method calls as the client.

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

The minimal client connects to the minimal dbus server example, which is
expected to be listening on its default abstract unix domain socket path.

It call the server Echo method with "Hello!" and returns to the event loop.
When the reply comes, it prints the returned message.

Afterwards it just sits there receiving unsolicited messages from the server
example, until closed by the user.

```
 $ ./lws-minimal-dbus-client
ctx
[2018/10/05 06:08:31:4901] NOTICE: pending_call_notify
[2018/10/05 06:08:31:4929] USER: pending_call_notify: received 'Hello!'
^C[2018/10/05 06:09:22:4409] NOTICE: destroy_dbus_client_conn
[2018/10/05 06:09:22:4691] NOTICE: Exiting cleanly
...
```


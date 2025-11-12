# lws minimal dbus server

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
--session | Bind to session bus instead of creating private abstract unix socket

By default the minimal server listens using its own abstract unix socket
at `unix:abstract=org.libwebsockets.test`.

You can also run it instead as a participant on the session bus, without its own
unix socket, by giving `--session`.

### Examples using the default private abstract unix socket

```
 $ ./lws-minimal-dbus-server
[2018/10/03 07:08:02:6448] USER: LWS minimal dbus server
[2018/10/03 07:08:02:6693] NOTICE: Creating Vhost 'default' port 0, 1 protocols, IPv6 off
...
```

You can communicate with the dbus server over its private abstract socket using, eg

```
$ gdbus introspect --address unix:abstract=org.libwebsockets.test --dest org.libwebsockets.test --object-path /org/libwebsockets/test
node /org/example/TestObject {
  interface org.freedesktop.DBus.Introspectable {
    methods:
      Introspect(out s data);
    signals:
    properties:
  };
  interface org.freedesktop.DBus.Properties {
    methods:
      Get(in  s interface,
...
```

```
$ gdbus call --address unix:abstract=org.libwebsockets.test --dest org.libwebsockets.test --object-path /org/libwebsockets/test --method org.libwebsockets.test.Echo HELLO
('HELLO',)
```

### Examples using the DBUS session bus

```
 $ ./lws-minimal-dbus-server --session
[2018/10/03 07:08:02:6448] USER: LWS minimal dbus server
[2018/10/03 07:08:02:6693] NOTICE: Creating Vhost 'default' port 0, 1 protocols, IPv6 off
...
```

You can communicate with the dbus server over the session bus using, eg

```
$ gdbus introspect --session --dest org.libwebsockets.test --object-path /org/libwebsockets/test
node /org/example/TestObject {
  interface org.freedesktop.DBus.Introspectable {
    methods:
      Introspect(out s data);
    signals:
    properties:
  };
  interface org.freedesktop.DBus.Properties {
    methods:
      Get(in  s interface,
...
```

```
$ gdbus call --session --dest org.libwebsockets.test --object-path /org/libwebsockets/test --method org.libwebsockets.test.Echo HELLO
('HELLO',)
```

# DBUS Role Support

## DBUS-related distro packages

Fedora: dbus-devel
Debian / Ubuntu: libdbus-1-dev

## Enabling for build at cmake

Fedora example:
```
$ cmake .. -DLWS_ROLE_DBUS=1 -DLWS_DBUS_INCLUDE2="/usr/lib64/dbus-1.0/include"
```

Ubuntu example:
```
$ cmake .. -DLWS_ROLE_DBUS=1 -DLWS_DBUS_INCLUDE2="/usr/lib/x86_64-linux-gnu/dbus-1.0/include"
```

Dbus requires two include paths, which you can force by setting `LWS_DBUS_INCLUDE1`
and `LWS_DBUS_INCLUDE2`.  Although INCLUDE1 is usually guessable, both can be
forced to allow cross-build.

If these are not forced, then lws cmake will try to check some popular places,
for `LWS_DBUS_INCLUDE1`, on both Fedora and Debian / Ubuntu, this is
`/usr/include/dbus-1.0`... if the directory exists, it is used.

For `LWS_DBUS_INCLUDE2`, it is the arch-specific dbus header which may be
packaged separately than the main dbus headers.  On Fedora, this is in
`/usr/lib[64]/dbus-1.0/include`... if not given externally, lws cmake will
try `/usr/lib64/dbus-1.0/include`.  On Debian / Ubuntu, the package installs
it in an arch-specific dir like `/usr/lib/x86_64-linux-gnu/dbus-1.0/include`,
you should force the path.

The library path is usually \[lib\] "dbus-1", but this can also be forced if
you want to build cross or use a special build, via `LWS_DBUS_LIB`.

## Building against local dbus build

If you built your own local dbus and installed it in /usr/local, then
this is the incantation to direct lws to use the local version of dbus:

```
cmake .. -DLWS_ROLE_DBUS=1 -DLWS_DBUS_INCLUDE1="/usr/local/include/dbus-1.0" -DLWS_DBUS_INCLUDE2="/usr/local/lib/dbus-1.0/include" -DLWS_DBUS_LIB="/usr/local/lib/libdbus-1.so"
```

You'll also need to give the loader a helping hand to do what you want if
there's a perfectly good dbus lib already in `/usr/lib[64]` using `LD_PRELOAD`
like this

```
LD_PRELOAD=/usr/local/lib/libdbus-1.so.3.24.0 myapp
```

## Lws dbus api exports

Because of the irregular situation with libdbus includes, if lws exports the
dbus helpers, which use dbus types, as usual from `#include <libwebsockets.h>`
then if lws was compiled with dbus role support it forces all users to take
care about the dbus include path mess whether they use dbus themselves or not.

For that reason, if you need access to the lws dbus apis, you must explicitly
include them by

```
#include <libwebsockets/lws-dbus.h>
```

This includes `<dbus/dbus.h>` and so requires the include paths set up.  But
otherwise non-dbus users that don't include `libwebsockets/lws-dbus.h` don't
have to care about it.

## DBUS and valgrind

https://cgit.freedesktop.org/dbus/dbus/tree/README.valgrind

1) One-time 6KiB "Still reachable" caused by abstract unix domain socket + libc
`getgrouplist()` via nss... bug since 2004(!)

https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=273051




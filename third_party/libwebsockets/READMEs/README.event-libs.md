# lws event library support

## v4.0 and below

Before v4.1, lws allowed selecting some event library support for inclusion
in the libwebsockets library

Option|Feature
---|---
`LWS_WITH_GLIB`|glib
`LWS_WITH_LIBEVENT`|libevent
`LWS_WITH_LIBUV`|libuv
`LWS_WITH_LIBEV`|libev

The user code can select by `info->options` flags at runtime which event loop
it wants to use.

The only restriction is that libev and libevent can't coexist, because their
header namespace conflicts.

## v4.1 and above

Lws continues to support the old way described above, but there's an additional
new cmake option that decides how they are built if any are selected,
`LWS_WITH_EVLIB_PLUGINS`.

The old behaviour is set by `LWS_WITH_EVLIB_PLUGINS=0`, for UNIX platforms, this
is set to 1 by default.  This causes the enabled event lib support to each be built into
its own dynamically linked plugin, and lws will bring in the requested support alone
at runtime after seeing the `info->options` flags requested by the user code.

This has two main benefits, first the conflict around building libevent and libev
together is removed, they each build isolated in their own plugin; the libwebsockets
core library build doesn't import any of their headers (see below for exception).
And second, for distro packaging, the event lib support plugins can be separately
packaged, and apps take dependencies on the specific event lib plugin package, which
itself depends on the libwebsockets core library.  This allows just the needed
dependencies for the packageset without forcing everything to bring everything in.

Separately, lws itself has some optional dependencies on libuv, if you build lwsws
or on Windows you want plugins at all.  CMake will detect these situations and
select to link the lws library itself to libuv if so as well, independent of whatever
is happening with the event lib support.

## evlib plugin install

The produced plugins are named

event lib|plugin name
---|---
glib|`libwebsockets-evlib_glib.so`
event|`libwebsockets-evlib_event.so`
uv|`libwebsockets-evlib_uv.so`
ev|`libwebsockets-evlib_ev.so`

The evlib plugins are installed alongside libwebsockets.so/.a into the configured
library dir, it's often `/usr/local/lib/` by default on linux.

Lws looks for them at runtime using the build-time-configured path.

## Component packaging

The canonical package name is `libwebsockets`, the recommended way to split the
packaging is put the expected libs and pkgconfig in `libwebsockets` or `libwebsockets-core`,
the latter is followed by the provided cmake, and produce an additional package per build
event library plugin, named, eg `libwebsockets-evlib_glib`, which has a dependency on
`libwebsockets[-core]`.

Applications that use the default event loop can directly require `libwebsockets[-core]`,
and application packages that need specific event loop support can just require, eg,
`libwebsockets-evlib_glib`, which will bring that in and the core lws pieces in one step.
There is then no problem with multiple apps requiring different event libs, they will
bring in all the necessary pieces which will not conflict either as packages or at
runtime.

## `LWS_WITH_DISTRO_RECOMMENDED`

The cmake helper config `LWS_WITH_DISTRO_RECOMMENDED` is adapted to build all the
event libs with the event lib plugin support enabled.


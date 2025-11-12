## Information for new event lib implementers

### Introduction

By default lws has built-in support for POSIX poll() as the event loop on unix,
and native WSA on windows.

To get access to epoll() or other platform specific better poll waits, or to
integrate with existing applications already using a specific event loop, it can
be desirable for lws to use another external event library, like libuv, glib,
libevent, libev, or sdevent.

Lws supports wholesale replacement of its wait selectable at runtime, either by
building support for one or more event lib into the libwebsockets library, or by
building runtime-loadable plugins.  CMake symbol `LWS_WITH_EVLIB_PLUGINS`
decides if the support is built as plugins or included into the lws lib.

Due to their history libevent and libev have conflicting defines in the same
namespace and cannot be built together if included into the lib, however when
built as plugins they are built separately without problems.
See ./READMEs/README.event-libs.md for more details.

Despite it may be more work, lws event lib implementations must support
"foreign" loops cleanly, that is integration with an already-existing loop and
the ability to destroy the lws_context without stopping or leaving the foreign
loop in any different state than when lws found it.  For most loops this is
fairly simple, but with libuv async close, it required refcounting lws libuv
handles and deferring the actual destroy until they were all really closed.

### Code placement

The code specific to the event library should live in `./lib/event-libs/**lib name**`

### Allowing control over enabling event libs

All event libs should add a cmake define `LWS_WITH_**lib name**` and make its
build dependent on it in CMakeLists.txt.  Export the cmakedefine in
`./cmake/lws_config.h.in` as well so user builds can understand if the event
lib is available in the lws build it is trying to bind to.

If the event lib is disabled in cmake, nothing in its directory is built or
referenced.

### Event loop ops struct

The event lib support is defined by `struct lws_event_loop_ops` in
`lib/event-libs/private-lib-event-libs.h`,
each event lib support instantiates one of these and fills in the appropriate
ops callbacks to perform its job.  By convention that lives in
`./lib/event-libs/**lib name**/**lib_name**.c`.

The ops struct must be public, not static, and must be named using `**lib_name**`,
eg

```
```

### Private event lib declarations

Truly private declarations for the event lib support that are only referenced by
that code can go in the event-libs directory as you like.  The convention is
they should be in the event lib support directory in a file
`private-lib-event-libs-**lib name**.h`.

### Integration with lws

There are a couple of places to add refererences in ./lib/core/context.c, in a
table of context creation time server option flags mapped to the **lib_name**,
used for plugin mode, like this...

```
#if defined(LWS_WITH_EVLIB_PLUGINS) && defined(LWS_WITH_EVENT_LIBS)
static const struct lws_evlib_map {
	uint64_t	flag;
	const char	*name;
} map[] = {
	{ LWS_SERVER_OPTION_LIBUV,    "evlib_uv" },
	{ LWS_SERVER_OPTION_LIBEVENT, "evlib_event" },
	{ LWS_SERVER_OPTION_GLIB,     "evlib_glib" },
	{ LWS_SERVER_OPTION_LIBEV,    "evlib_ev" },
};
```

and for backwards compatibility add a stanza to the built-in checks like this

```
#if defined(LWS_WITH_LIBUV)
	if (lws_check_opt(info->options, LWS_SERVER_OPTION_LIBUV)) {
		extern const lws_plugin_evlib_t evlib_uv;
		plev = &evlib_uv;
	}
#endif
```

Both entries are the way the main libs hook up to the selected event lib ops
struct at runtime.

### Integrating event lib assets to lws

Declare "container structs" in your private....h for anything you need at
wsi, pt, vhost and context levels, eg, the libuv event lib support need to
add its own assets in the perthread struct, it declares in its private....h

```
struct lws_pt_eventlibs_libuv {
	uv_loop_t *io_loop;
	struct lws_context_per_thread *pt;
	uv_signal_t signals[8];
	uv_timer_t sultimer;
	uv_idle_t idle;
	struct lws_signal_watcher_libuv w_sigint;
};
```

this is completely private and opaque, but in the ops struct there are provided
four entries to export the sizes of these event-lib specific objects

```
...
	/* evlib_size_ctx */	sizeof(struct lws_context_eventlibs_libuv),
	/* evlib_size_pt */	sizeof(struct lws_pt_eventlibs_libuv),
	/* evlib_size_vh */	0,
	/* evlib_size_wsi */	sizeof(struct lws_io_watcher_libuv),
};
```

If the particular event lib doesn't need to have a private footprint in an
object, it can just set the size it needs there to 0.

When the context, pts, vhosts or wsis are created in lws, they over-allocate
to also allow for the event lib object, and set a pointer in the lws object
being created to point at the over-allocation.  For example for the wsi

```
#if defined(LWS_WITH_EVENT_LIBS)
	void				*evlib_wsi; /* overallocated */
#endif
```

and similarly there are `evlib_pt` and so on for those objects, usable by the
event lib and opaque to everyone else.  Once the event lib is selected at
runtime, all of these objects are guaranteed to have the right size object at
`wsi->evlib_wsi` initialized to zeroes.

### Enabling event lib adoption

You need to add a `LWS_SERVER_OPTION...` flag as necessary in `./lib/libwebsockets.h`
`enum lws_context_options`, and follow the existing code in `lws_create_context()`
to convert the flag into binding your ops struct to the context.

### Implementation of the event lib bindings

Study eg libuv implementation, using the available ops in the struct lws_event_loop_ops
as a guide.

### Destruction

Ending the event loop is generally a bit tricky, because if the event loop is
internal to the lws context, you cannot destroy it while the event loop is
running.

Don't add special exports... we tried that, it's a huge mess.  The same user
code should be able work with any of the event loops including poll.

The solution we found was hide the different processing necessary for the
different cases in `lws_destroy_context()`.  To help with that there are event
lib ops available that will be called at two different places in the context
destroy processing.


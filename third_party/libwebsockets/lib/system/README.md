# LWS System Helpers

Lws now has a little collection of helper utilities for common network-based
functions necessary for normal device operation, eg, async DNS, ntpclient
(necessary for tls validation), and DHCP client.

## Conventions

If any system helper is enabled for build, lws creates an additional vhost
"system" at Context Creation time.  Wsi that are created for the system
features are bound to this.  In the context object, this is available as
`.vhost_system`.

# Attaching to an existing context from other threads

To simplify the case different pieces of code want to attach to a single
lws_context at runtime, from different thread contexts, lws_system has an api
via an lws_system operation function pointer where the other threads can use
platform-specific locking to request callbacks to their own code from the
lws event loop thread context safely.

For convenience, the callback can be delayed until the system has entered or
passed a specified system state, eg, LWS_SYSTATE_OPERATIONAL so the code will
only get called back after the network, ntpclient and auth have been done.
Additionally an opaque pointer can be passed to the callback when it is called
from the lws event loop context.

## Implementing the system-specific locking

`lws_system_ops_t` struct has a member `.attach`

```
	int (*attach)(struct lws_context *context, int tsi, lws_attach_cb_t *cb,
		      lws_system_states_t state, void *opaque,
		      struct lws_attach_item **get);
```

This should be defined in user code as setting locking, then passing the
arguments through to a non-threadsafe helper

```
int
__lws_system_attach(struct lws_context *context, int tsi, lws_attach_cb_t *cb,
		    lws_system_states_t state, void *opaque,
		    struct lws_attach_item **get);
```

that does the actual attach work.  When it returns, the locking should be
unlocked and the return passed back.

## Attaching the callback request

User code should call the lws_system_ops_t `.attach` function like

```
	lws_system_get_ops(context)->attach(...);
```

The callback function which will be called from the lws event loop context
should look like this

```
void my_callback(struct lws_context *context, int tsi, void *opaque);
```

with the callback function name passed into the (*attach)() call above.  When
the callback happens, the opaque user pointer set at the (*attach)() call is
passed back to it as an argument.

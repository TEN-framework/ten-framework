## Information for new role implementers

### Introduction

In lws the "role" is the job the wsi is doing in the system, eg,
being an http1 or h2, or ws connection, or being a listen socket, etc.

This is different than, eg, a new ws protocol or a different callback
for an existing role.  A new role is needed when you want to add support for
something completely new, like a completely new wire protocol that
doesn't use http or ws.

So... what's the point of implementing the protocol inside the lws role framework?

You inherit all the well-maintained lws core functionality around:

 - connection lifecycle sequencing in a valgrind-clean way

 - client connection proxy support, for HTTP and Socks5

 - tls support working equally on mbedTLS and OpenSSL and derivatives without any code in the role

 - apis for cert lifecycle management and parsing

 - event loop support working on all the lws event loops (poll, libuv , ev, and event)

 - clean connection tracking and closing even on advanced event loops

 - user code follows the same simple callbacks on wsi

 - multi-vhost support

 - core multithreaded service support with usually no locking requirement on the role code

 - direct compatibility with all other lws roles + protocols in the same event loop

 - compatibility with higher-level stuff like lwsws as the server application

### Code placement

The code specific to that role should live in `./lib/roles/**role name**`

If a role is asymmetic between a client and server side, like http is, it
should generally be implemented as a single role.

### Allowing control over enabling roles

All roles should add a cmake define `LWS_ROLE_**role name**` and make its build
dependent on it in CMakeLists.txt.  Export the cmakedefine in `./cmake/lws_config.h.in`
as well so user builds can understand if the role is available in the lws build it is
trying to bind to.

If the role is disabled in cmake, nothing in its directory is built.

### Role ops struct

The role is defined by `struct lws_role_ops` in `lib/roles/private-lib-roles.h`,
each role instantiates one of these and fills in the appropriate ops
callbacks to perform its job.  By convention that lives in
`./lib/roles/**role name**/ops-**role_name**.c`.

### Private role declarations

Truly private declarations for the role can go in the role directory as you like.
However when the declarations must be accessible to other things in lws build, eg,
the role adds members to `struct lws` when enabled, they should be in the role
directory in a file `private-lib-roles-myrole.h`.

Search for "bring in role private declarations" in `./lib/roles/private-lib-roles.h
and add your private role file there following the style used for the other roles,
eg,

```
#if defined(LWS_ROLE_WS)
 #include "roles/ws/private-lib-roles-ws.h"
#else
 #define lwsi_role_ws(wsi) (0)
#endif
```

If the role is disabled at cmake, nothing from its private.h should be used anywhere.

### Integrating role assets to lws

If your role needs special storage in lws objects, that's no problem.  But to keep
things sane, there are some rules.

 - declare a "container struct" in your private.h for everything, eg, the ws role wants
   to add storage in lws_vhost for enabled extensions, it declares in its private.h

```
struct lws_vhost_role_ws {
#if !defined(LWS_WITHOUT_EXTENSIONS)
	const struct lws_extension *extensions;
#endif
};
```

 - add your role content in one place in the lws struct, protected by `#if defined(LWS_ROLE_**role name**)`,
   eg, again for LWS_ROLE_WS

```
	struct lws_vhost {

...

#if defined(LWS_ROLE_WS)
	struct lws_vhost_role_ws ws;
#endif

...
```

### Adding to lws available roles list

Edit the NULL-terminated array `available_roles` at the top of `./lib/core/context.c` to include
a pointer to your new role's ops struct, following the style already there.

```
const struct lws_role_ops * available_roles[] = {
#if defined(LWS_ROLE_H2)
	&role_ops_h2,
#endif
...
```

This makes lws aware that your role exists, and it can auto-generate some things like
ALPN lists, and call your role ops callbacks for things like hooking vhost creation.

### Enabling role adoption

The primary way wsi get bound to a specific role is via the lws adoption api
`lws_adopt_descriptor_vhost()`.  Add flags as necessary in `./include/libwebsockets/lws-adopt.h`
`enum lws_adoption_type` and follow the existing code in `lws_adopt_descriptor_vhost()`
to bind a wsi with suitable flags to your role ops.

### Implementation of the role

After that plumbing-in is completed, the role ops you declare are "live" on a wsi
bound to them via the adoption api.

The core support for wsis in lws has some generic concepts

 - the wsi holds a pointer member `role_ops` that indicates which role ops the
   wsi is bound to

 - the wsi holds a generic uint32 `wsistate` that contains role flags and wsi state

 - role flags are provided (LWSIFR_CLIENT, LWSIFR_SERVER) to differentiate between
   client and server connections inside a wsi, along with helpers `lwsi_role_client(wsi)`
   and `lwsi_role_server(wsi)`.

 - lws provides around 30 generic states for the wsi starting from 'unconnected' through
   various proxy or tunnel states, to 'established', and then various states shutting
   down until 'dead socket'.  The states have testable flags and helpers to discover if
   the wsi state is before establishment `lwsi_state_est(wsi)` and if in the state it is
   in, it can handle pollout `lwsi_state_can_handle_POLLOUT(wsi)`.

 - You set the initial binding, role flags and state using `lws_role_transition()`.  Afterwards
   you can adjust the state using `lwsi_set_state()`.

### Role ops compression

Since the role ops struct is typically only sparsely filled, rather than have 20 function
pointers most of which may be NULL, there is a separate array of a union of function
pointers that is just long enough for functions that exist in the role, and a nybble index
table with a nybble for each possible op, either 0 indicating that the operation is not
provided in this role, or 1 - 15 indicating the position of the function pointer in the
array.


## `lws_conmon` apis

`LWS_WITH_CONMON` build option enables `lws_conmon` apis for user code... these add
some staticistic and information to client connections that can use useful for devices
to introspect how the connection to their servers is actually performing.

The public apis can be found in `libwebsockets/lws-conmon.h`.

A struct is provided that describes

 - the peer sockaddr the wsi actually connected to, if any

 - a deep copy of the aggregate DNS results (struct addrinfo list) that the
   client had access to for the peer

 - the number of us dns lookup took

 - the number of us the socket connection took

 - the number of us the tls link establishment took

 - the number of us from the transaction request to the first response, if
   the protocol has a transaction concept

Because the user code may want to hold on to the DNS list for longer than the
life of the wsi that originated it, the `lws_conmon_wsi_take()` api allows
the ownership of the allocated list to be transferred to the user code (as
well as copying data out into the user's struct so it no longer has any
dependency on wsi lifetime either).  The DNS list copy in the struct must be
released at some point by calling `lws_conmon_release()`, but that
can be at any time afterwards.

The lws-minimal-http-client example shows how user code can use the apis, build
lws with the `LWS_WITH_CONMON` cmake option and run with `--conmon` to get a
dump of the collected information.


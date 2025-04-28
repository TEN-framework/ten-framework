# Implementation background

## Client connection Queueing

By default lws treats each client connection as completely separate, and each is
made from scratch with its own network connection independently.

If the user code sets the `LCCSCF_PIPELINE` bit on `info.ssl_connection` when
creating the client connection though, lws attempts to optimize multiple client
connections to the same place by sharing any existing connection and its tls
tunnel where possible.

There are two basic approaches, for h1 additional connections of the same type
and endpoint basically queue on a leader and happen sequentially.

For muxed protocols like h2, they may also queue if the initial connection is
not up yet, but subsequently the will all join the existing connection
simultaneously "broadside".

## h1 queueing

The initial wsi to start the network connection becomes the "leader" that
subsequent connection attempts will queue against.  Each vhost has a dll2_owner
`wsi->dll_cli_active_conns_owner` that "leaders" who are actually making network
connections themselves can register on as "active client connections".

Other client wsi being created who find there is already a leader on the active
client connection list for the vhost, can join their dll2 wsi->dll2_cli_txn_queue
to the leader's wsi->dll2_cli_txn_queue_owner to "queue" on the leader.

The user code does not know which wsi was first or is queued, it just waits for
stuff to happen the same either way.

When the "leader" wsi connects, it performs its client transaction as normal,
and at the end arrives at `lws_http_transaction_completed_client()`.  Here, it
calls through to the lws_mux `_lws_generic_transaction_completed_active_conn()`
helper.  This helper sees if anything else is queued, and if so, migrates assets
like the SSL *, the socket fd, and any remaining queue from the original leader
to the head of the list, which replaces the old leader as the "active client
connection" any subsequent connects would queue on.

It has to be done this way so that user code which may know each client wsi by
its wsi, or have marked it with an opaque_user_data pointer, is getting its
specific request handled by the wsi it expects it to be handled by.

A side effect of this, and in order to be able to handle POSTs cleanly, lws
does not attempt to send the headers for the next queued child before the
previous child has finished.

The process of moving the SSL context and fd etc between the queued wsi continues
until the queue is all handled.

## muxed protocol queueing and stream binding

h2 connections act the same as h1 before the initial connection has been made,
but once it is made all the queued connections join the network connection as
child mux streams immediately, "broadside", binding the stream to the existing
network connection.

# Using TLS Session resumption

Lws supports clientside session caching and session resumption on both mbedtls
and openssl-type tls library backends, to accellerate connection re-
establishment.

## Background

TLS specifies logical "sessions" that get "established" on both sides when the
tls tunnel is negotiated... these are the object that gets validated by the
certificate PKI.  They each have a server-unique "Session ID" of up to 32 bytes
each.

Normally the default is that there is a new session negotiated per connection,
so multiple connections to the same endpoint each negotiate fresh sessions from
scratch.

However tls servers typically maintain a cache of recent sessions, and where
both the server and client still have a copy of a previously-negotiated session
around, support the client explicitly requesting additional connections binding
to the old session by asking for it by its Session ID at negotiation time.

### Re-use of validated sessions

The advantage is that the timeconsuming key exchange part of the negotiation can
be skipped, and a connection-specific AES key agreed at both sides just by
hashing on the secret held in the session object at each side.  This allows new
tunnels to be established much faster after the first, while the session from
the first is still valid and available at both sides.

Both the server and client may apply their own lifetime restriction to their
copy of the session, the first side to expire it will cause a new session to be
forced at the next reuse attempt.  Lifetimes above 24h are not recommended by
RFC5246.

### Multiple concurrent use of validated sessions

In addition, the session's scope is any connection to the server that knows the
original session ID, because individual new AES keys are hashed from the session
secret, multiple connections to the same endpoint can take advantage of a single
valid session object.

### Difference from Session Tickets

TLS also supports sessions as bearer tokens, but these are generally considered
as degrading security.  Lws doesn't support Session Tickets, just reuse by
Session IDs.

## Support in lws

Server-side TLS generally has session caching enabled by default.  For client
side, lws now enables `LWS_WITH_TLS_SESSIONS` at cmake by default, which adds
a configurable tls session cache that is automatically kept updated with a
MRU-sorted list of established sessions.

It's also possible to serialize sessions and save and load them, but this has to
be treated with caution.

Filling, expiring and consulting the session cache for client connections is
performed automatically.

### tls library differences

Mbedtls supports clientside session caching in lws, but it does not have a
session message arrival callback to synchronize updating the client session
cache like openssl does.

Separately, the session cb in boringssl is reportedly nonfunctional at the
moment.

To solve both cases, lws will schedule a check for the session at +500ms after
the tls negotiation completed, and for the case the connection doesn't last
500ms or the server is slow issuing the message, also attempt to update the
cache at the time the tls connection object is closing.

### Session namespacing in lws

Internally sessions are referred to by a vhostname.hostname.port tuple.

### Configuring the clientside cache

Session caches in lws exist in and are bound to the vhost.  Different vhosts may
provide different authentication (eg, client certs) to the same endpoint that
another connection should not be able to take advantage of.

The max size of this cache can be set at `.tls_session_cache_max` in the vhost
creation info struct, if left at 0 then a default of 10 is applied.

The Time-To-Live policy for sessions at the client can be set in seconds at
`.tls_session_timeout`, by default whatever the tls library thinks it should be,
perhaps 300s.

You can disable session caching for a particular vhost by adding the vhost
option flag `LWS_SERVER_OPTION_DISABLE_TLS_SESSION_CACHE` to `.options` at
vhost creation time.

### Session saving and loading

Trying to make sessions really persistent is supported but requires extra
caution.  RFC5246 says

   Applications that may be run in relatively insecure environments should not
   write session IDs to stable storage.

The issue is that while in process memory the session object is relatively
secure compared to sensitive secrets and tls library data already in process
memory.

But when serialized to, eg, some external, unencrypted medium, the accessibility
of what is basically a secret able to decrypt tls connections can become a
security hazard.  It's left to the user to take any necessary steps to secure
sessions stored that way.

For openssl, Public APIs are provided in `libwebsockets/lws-tls-sessions.h` to
serialize any session in the cache associated with a vhost/host/port tuple, and
to preload any available session into a vhost session cache by describing the
endpoint hostname and port.

The session saving and loading apis aren't supported for mbedtls yet.

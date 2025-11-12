# Lws routing

lws is mainly built around POSIX sockets and operates from the
information available from those.  But in some cases, it needs to go
a step further and monitor and understand the device routing table.

## Recognizing loss of routability

On mobile devices, switching between interfaces and losing / regaining
connections quickly is a given.  But POSIX sockets do not act like
that, the socket remains connected until something times it out if it
no longer has a route to its peer, and the tcp timeouts can be in the
order of minutes.

In order to do better, lws must monitor and understand how the routing
table relates to existing connections, dynamically.

## Linux: netlink

For linux-based devices you can build in netlink-based route monitoring
with `-DLWS_WITH_NETLINK=1`, lws aquires a copy of the routing table
when the context / pt starts up and modifies it according to netlink
messages from then on.

On Linux routing table events do not take much care about backing out
changes made on interface up by, eg, NetworkManager.  So lws also
monitors for link / interface down to remove the related routes.

## Actions in lws based on routing table

Both server and client connections now store their peer sockaddr in the
wsi, and when the routing table changes, all active wsi on a pt are
checked against the routing table to confirm the peer is still
routable.

For example if there is no net route matching the peer and no gateway,
the connection is invalidated and closed.  Similarly, if we are
removing the highest priority gateway route, all connections to a peer
without a net route match are invalidated.  However connections with
an unaffected  matching net route like 127.0.0.0/8 are left alone.

## Intergration to other subsystems

If SMD is built in, on any route change a NETWORK message
`{"rt":"add|del"}` is issued.

If SMD is built in, on any route change involving a gateway, a NETWORK
message `{"trigger":"cpdcheck", "src":"gw-change"}` is issued.  If
Captive Portal Detection is built in, this will cause a new captive
portal detection sequence.


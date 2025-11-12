# Asynchronous DNS

## Introduction

Lws now features optional asynchronous, ie, nonblocking recursive DNS
resolution done on the event loop, enable `-DLWS_WITH_SYS_ASYNC_DNS=1`
at cmake to build it in.

## Description

The default libc name resolution is via libc `getaddrinfo()`, which is
blocking, possibly for quite long periods (seconds).  If you are
taking care about latency, but want to create outgoing connections,
you can't tolerate this exception from the rule that everything in
lws is nonblocking.

Lws' asynchronous DNS resolver creates a caching name resolver
that directly queries the configured nameserver itself over UDP,
from the event loop.

It supports both ipv4 / A records and ipv6 / AAAA records (see later
for a description about how).  One server supported over UDP :53,
and the nameserver is autodicovered on linux, windows, and freertos.
    
Other features

 - lws-style paranoid response parsing
 - random unique tid generation to increase difficulty of poisoning
 - it's really integrated with the lws event loop, it does not spawn
   threads or use the libc resolver, and of course no blocking at all
 - platform-specific server address capturing (from /etc/resolv.conf
   on linux, windows apis on windows)
 - LRU caching
 - piggybacking (multiple requests before the first completes go on
    a list on the first request, not spawn multiple requests)
 - observes TTL in cache
 - TTL and timeout use `lws_sul` timers on the event loop
 - Uses CNAME resolution inside the same response if present, otherwise
   recurses to resolve the CNAME (up to 3 deep)
 - ipv6 pieces only built if cmake `LWS_IPV6` enabled

## Api

If enabled at cmake, the async DNS implementation is used automatically
for lws client connections.  It's also possible to call it directly, see
the api-test-async-dns example for how.

The Api follows that of `getaddrinfo()` but results are not created on
the heap.  Instead a single, const cached copy of the addrinfo struct
chain is reference-counted, with `lws_async_dns_freeaddrinfo()` provided
to deduct from the reference count.  Cached items with a nonzero
reference count can't be destroyed from the cache, so it's safe to keep
a pointer to the results and iterate through them.

## Dealing with IPv4 and IPv6

DNS is a very old standard that has some quirks... one of them is that
multiple queries are not supported in one packet, even though the protocol
suggests it is.  This creates problems on ipv6 enabled systems, where
it may prefer to have AAAA results, but the server may only have A records.

To square the circle, for ipv4 only systems (`LWS_IPV6=0`) the resolver
requests only A records.  For ipv6-capable systems, it always requests
first A and then immediately afterwards AAAA records.

To simplify the implementation, the tid b0 is used to differentiate
between A (b0 = 0) and AAAA (b0 = 1) requests and responses using the
same query body.

The first response to come back is parsed, and a cache entry made...
it leaves a note in the query about the address of the last `struct addrinfo`
record.  When the second response comes, a second allocation is made,
but not added to the logical cache... instead it's chained on to the
first cache entry and the `struct addrinfo` linked-list from the
first cache entry is extended into the second one.  At the time the
second result arrives, the query is destroyed and the cached results
provided on the result callback.

## Recursion

Where CNAMEs are returned, DNS servers may take two approaches... if the
CNAME is also resolved by the same server and so it knows what it should
resolve to, it may provide the CNAME resolution in the same response
packet.

In the case the CNAME is actually resolved by a different name server,
the server with the CNAME does not have the information to hand to also
resolve the CNAME in the same response.  So it just leaves it for the
client to sort out.

The lws implementation can deal with both of these, first it "recurses"
(it does not recurse on the process stack but uses its own manual stack)
to look for results in the same packet that told it about the CNAME.  If
there are no results, it resets the query to look instead for the CNAME,
and restarts it.  It allows this to happen for 3 CNAME deep.

At the end, either way, the cached result is set using the original
query name and the results from the last CNAME in the chain.



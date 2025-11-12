# Client http cookie storage, caching and application

lws now has the option to store incoming cookies in a Netscape cookie jar file
persistently, and auto-apply relevant cookies to future outgoing requests.

A L1 heap cache of recent cookies is maintained, along with LRU tracking and
removal of entries from cache and the cookie jar file according to their cookie
expiry time.

The cookie handling is off by default per-connection for backwards compatibility
and to avoid unexpected tracking.

## Enabling at build-time

Make sure `-DLWS_WITH_CACHE_NSCOOKIEJAR=1` is enabled at cmake (it is on by
default now).

## Configuring the cookie cache

The cookie cache is managed through context creation info struct members.

|member|function|
|---|---|
|`.http_nsc_filepath`|Filepath to store the cookie jar file at|
|`.http_nsc_heap_max_footprint`|0, or Max size in bytes for the L1 heap cache|
|`.http_nsc_heap_max_items`|0, or Max number of cookies allowed in L1 heap cache|
|`.http_nsc_heap_max_payload`|0, or Largest cookie we are willing to handle|

## Enabling per-connection in lws

To enable it on connections at lws level, add the flag `LCCSCF_CACHE_COOKIES` to
the client connection info struct `.ssl_connection` flags.

## Enabling per-connection in Secure Streams policy

To enable it on Secure Streams, in the streamtype policy add

```
	"http_cookies":		true
```

Notes about coding with lws
===========================

@section era Old lws and lws v2.0+

Originally lws only supported the "manual" method of handling everything in the
user callback found in test-server.c / test-server-http.c.

Since v2.0, the need for most or all of this manual boilerplate has been
eliminated: the protocols[0] http stuff is provided by a generic lib export
`lws_callback_http_dummy()`.  You can serve parts of your filesystem at part of
the URL space using mounts, the dummy http callback will do the right thing.

It's much preferred to use the "automated" v2.0 type scheme, because it's less
code and it's easier to support.

The minimal examples all use the modern, recommended way.

If you just need generic serving capability, without the need to integrate lws
to some other app, consider not writing any server code at all, and instead use
the generic server `lwsws`, and writing your special user code in a standalone
"plugin".  The server is configured for mounts etc using JSON, see
./READMEs/README.lwsws.md.

Although the "plugins" are dynamically loaded if you use lwsws or lws built
with libuv, actually they may perfectly well be statically included if that
suits your situation better, eg, ESP32 test server, where the platform does
not support processes or dynamic loading, just #includes the plugins
one after the other and gets the same benefit from the same code.

Isolating and collating the protocol code in one place also makes it very easy
to maintain and understand.

So it if highly recommended you put your protocol-specific code into the
form of a "plugin" at the source level, even if you have no immediate plan to
use it dynamically-loaded.

@section writeable Only send data when socket writeable

You should only send data on a websocket connection from the user callback
`LWS_CALLBACK_SERVER_WRITEABLE` (or `LWS_CALLBACK_CLIENT_WRITEABLE` for
clients).

If you want to send something, do NOT just send it but request a callback
when the socket is writeable using

 - `lws_callback_on_writable(wsi)` for a specific `wsi`, or
 
 - `lws_callback_on_writable_all_protocol(protocol)` for all connections
using that protocol to get a callback when next writeable.

Usually you will get called back immediately next time around the service
loop, but if your peer is slow or temporarily inactive the callback will be
delayed accordingly.  Generating what to write and sending it should be done
in the ...WRITEABLE callback.

See the test server code for an example of how to do this.

Otherwise evolved libs like libuv get this wrong, they will allow you to "send"
anything you want but it only uses up your local memory (and costs you
memcpys) until the socket can actually accept it.  It is much better to regulate
your send action by the downstream peer readiness to take new data in the first
place, avoiding all the wasted buffering.

Libwebsockets' concept is that the downstream peer is truly the boss, if he,
or our connection to him, cannot handle anything new, we should not generate
anything new for him.  This is how unix shell piping works, you may have
`cat a.txt | grep xyz > remote", but actually that does not cat anything from
a.txt while remote cannot accept anything new. 

@section oneper Only one lws_write per WRITEABLE callback

From v2.5, lws strictly enforces only one lws_write() per WRITEABLE callback.

You will receive a message about "Illegal back-to-back write of ... detected"
if there is a second lws_write() before returning to the event loop.

This is because with http/2, the state of the network connection carrying a
wsi is unrelated to any state of the wsi.  The situation on http/1 where a
new request implied a new tcp connection and new SSL buffer, so you could
assume some window for writes is no longer true.  Any lws_write() can fail
and be buffered for completion by lws; it will be auto-completed by the
event loop.

Note that if you are handling your own http responses, writing the headers
needs to be done with a separate lws_write() from writing any payload.  That
means after writing the headers you must call `lws_callback_on_writable(wsi)`
and send any payload from the writable callback.

@section otherwr Do not rely on only your own WRITEABLE requests appearing

Libwebsockets may generate additional `LWS_CALLBACK_CLIENT_WRITEABLE` events
if it met network conditions where it had to buffer your send data internally.

So your code for `LWS_CALLBACK_CLIENT_WRITEABLE` needs to own the decision
about what to send, it can't assume that just because the writeable callback
came something is ready to send.

It's quite possible you get an 'extra' writeable callback at any time and
just need to `return 0` and wait for the expected callback later.

@section dae Daemonization

There's a helper api `lws_daemonize` built by default that does everything you
need to daemonize well, including creating a lock file.  If you're making
what's basically a daemon, just call this early in your init to fork to a
headless background process and exit the starting process.

Notice stdout, stderr, stdin are all redirected to /dev/null to enforce your
daemon is headless, so you'll need to sort out alternative logging, by, eg,
syslog via `lws_set_log_level(..., lwsl_emit_syslog)`.

@section conns Maximum number of connections

The maximum number of connections the library can deal with is decided when
it starts by querying the OS to find out how many file descriptors it is
allowed to open (1024 on Fedora for example).  It then allocates arrays that
allow up to that many connections, minus whatever other file descriptors are
in use by the user code.

If you want to restrict that allocation, or increase it, you can use ulimit or
similar to change the available number of file descriptors, and when restarted
**libwebsockets** will adapt accordingly.

@section peer_limits optional LWS_WITH_PEER_LIMITS

If you select `LWS_WITH_PEER_LIMITS` at cmake, then lws will track peer IPs
and monitor how many connections and ah resources they are trying to use
at one time.  You can choose to limit these at context creation time, using
`info.ip_limit_ah` and `info.ip_limit_wsi`.

Note that although the ah limit is 'soft', ie, the connection will just wait
until the IP is under the ah limit again before attaching a new ah, the
wsi limit is 'hard', lws will drop any additional connections from the
IP until it's under the limit again.

If you use these limits, you should consider multiple clients may simultaneously
try to access the site through NAT, etc.  So the limits should err on the side
of being generous, while still making it impossible for one IP to exhaust
all the server resources.

@section evtloop Libwebsockets is singlethreaded

Libwebsockets works in a serialized event loop, in a single thread.  It supports
the default poll() backend, and libuv, libev, and libevent event loop
libraries that also take this locking-free, nonblocking event loop approach that
is not threadsafe.  There are several advantages to this technique, but one
disadvantage, it doesn't integrate easily if there are multiple threads that
want to use libwebsockets.

However integration to multithreaded apps is possible if you follow some guidelines.

1) Aside from two APIs, directly calling lws apis from other threads is not allowed.

2) If you want to keep a list of live wsi, you need to use lifecycle callbacks on
the protocol in the service thread to manage the list, with your own locking.
Typically you use an ESTABLISHED callback to add ws wsi to your list and a CLOSED
callback to remove them.

3) LWS regulates your write activity by being able to let you know when you may
write more on a connection.  That reflects the reality that you cannot succeed to
send data to a peer that has no room for it, so you should not generate or buffer
write data until you know the peer connection can take more.

Other libraries pretend that the guy doing the writing is the boss who decides
what happens, and absorb as much as you want to write to local buffering.  That does
not scale to a lot of connections, because it will exhaust your memory and waste
time copying data around in memory needlessly.

The truth is the receiver, along with the network between you, is the boss who
decides what will happen.  If he stops accepting data, no data will move.  LWS is
designed to reflect that.

If you have something to send, you call `lws_callback_on_writable()` on the
connection, and when it is writeable, you will get a `LWS_CALLBACK_SERVER_WRITEABLE`
callback, where you should generate the data to send and send it with `lws_write()`.

You cannot send data using `lws_write()` outside of the WRITEABLE callback.

4) For multithreaded apps, this corresponds to a need to be able to provoke the
`lws_callback_on_writable()` action and to wake the service thread from its event
loop wait (sleeping in `poll()` or `epoll()` or whatever).  The rules above
mean directly sending data on the connection from another thread is out of the
question.

The only lws api that's safe to call from other thread contexts is `lws_cancel_service()`.
This will take a platform-specific action to wake the lws event loop thread wait,
either put a byte into a pipe2() the event loop is waiting on, or send a packet on
a UDP socket pair that the event loop waits on.  When the wake is handled by the
lws event loop thread, it will broadcast a `LWS_CALLBACK_EVENT_WAIT_CANCELLED`
message to every vhost-protocol instantiation, so you can handle this callback,
usually lock a shared data region, and if you see you need to write, call
`lws_callback_on_writeable()` for the wsi(s) that need to write.

There's no restriction on multiple threads calling `lws_cancel_service()`, it's
unconditionally safe due to how it is implemented underneath.

5) The obverse of this truism about the receiver being the boss is the case where
we are receiving.  If we get into a situation we actually can't usefully
receive any more, perhaps because we are passing the data on and the guy we want
to send to can't receive any more, then we should "turn off RX" by using the
RX flow control API, `lws_rx_flow_control(wsi, 0)`.  When something happens where we
can accept more RX, (eg, we learn our onward connection is writeable) we can call
it again to re-enable it on the incoming wsi.

LWS stops calling back about RX immediately you use flow control to disable RX, it
buffers the data internally if necessary.  So you will only see RX when you can
handle it.  When flow control is disabled, LWS stops taking new data in... this makes
the situation known to the sender by TCP "backpressure", the tx window fills and the
sender finds he cannot write any more to the connection.

See the mirror protocol implementations for example code.

If you need to service other socket or file descriptors as well as the
websocket ones, you can combine them together with the websocket ones
in one poll loop, see "External Polling Loop support" below, and
still do it all in one thread / process context.  If the need is less
architectural, you can also create RAW mode client and serving sockets; this
is how the lws plugin for the ssh server works.

@section anonprot Working without a protocol name

Websockets allows connections to negotiate without a protocol name...
in that case by default it will bind to the first protocol in your
vhost protocols[] array.

You can tell the vhost to use a different protocol by attaching a
pvo (per-vhost option) to the 

```
/*
 * this sets a per-vhost, per-protocol option name:value pair
 * the effect is to set this protocol to be the default one for the vhost,
 * ie, selected if no Protocol: header is sent with the ws upgrade.
 */

static const struct lws_protocol_vhost_options pvo_opt = {
	NULL,
	NULL,
	"default",
	"1"
};

static const struct lws_protocol_vhost_options pvo = {
	NULL,
	&pvo_opt,
	"my-protocol",
	""
};

...

	context_info.pvo = &pvo;
...

```

Will select "my-protocol" from your protocol list (even if it came
in by plugin) as being the target of client connections that don't
specify a protocol.

@section closing Closing connections from the user side

When you want to close a connection, you do it by returning `-1` from a
callback for that connection.

You can provoke a callback by calling `lws_callback_on_writable` on
the wsi, then notice in the callback you want to close it and just return -1.
But usually, the decision to close is made in a callback already and returning
-1 is simple.

If the socket knows the connection is dead, because the peer closed or there
was an affirmitive network error like a FIN coming, then **libwebsockets**  will
take care of closing the connection automatically.

If you have a silently dead connection, it's possible to enter a state where
the send pipe on the connection is choked but no ack will ever come, so the
dead connection will never become writeable.  To cover that, you can use TCP
keepalives (see later in this document) or pings.

@section gzip Serving from inside a zip file

Lws now supports serving gzipped files from inside a zip container.  Thanks to
Per Bothner for contributing the code.

This has the advtantage that if the client can accept GZIP encoding, lws can
simply send the gzip-compressed file from inside the zip file with no further
processing, saving time and bandwidth.

In the case the client can't understand gzip compression, lws automatically
decompressed the file and sends it normally.

Clients with limited storage and RAM will find this useful; the memory needed
for the inflate case is constrained so that only one input buffer at a time
is ever in memory.

To use this feature, ensure LWS_WITH_ZIP_FOPS is enabled at CMake.

`libwebsockets-test-server-v2.0` includes a mount using this technology
already, run that test server and navigate to http://localhost:7681/ziptest/candide.html

This will serve the book Candide in html, together with two jpgs, all from
inside a .zip file in /usr/[local/]share-libwebsockets-test-server/candide.zip

Usage is otherwise automatic, if you arrange a mount that points to the zipfile,
eg, "/ziptest" -> "mypath/test.zip", then URLs like `/ziptest/index.html` will be
servied from `index.html` inside `mypath/test.zip`

@section frags Fragmented messages

To support fragmented messages you need to check for the final
frame of a message with `lws_is_final_fragment`. This
check can be combined with `libwebsockets_remaining_packet_payload`
to gather the whole contents of a message, eg:

```
	    case LWS_CALLBACK_RECEIVE:
	    {
	        Client * const client = (Client *)user;
	        const size_t remaining = lws_remaining_packet_payload(wsi);
	
	        if (!remaining && lws_is_final_fragment(wsi)) {
	            if (client->HasFragments()) {
	                client->AppendMessageFragment(in, len, 0);
	                in = (void *)client->GetMessage();
	                len = client->GetMessageLength();
	            }
	
	            client->ProcessMessage((char *)in, len, wsi);
	            client->ResetMessage();
	        } else
	            client->AppendMessageFragment(in, len, remaining);
	    }
	    break;
```

The test app libwebsockets-test-fraggle sources also show how to
deal with fragmented messages.


@section debuglog Debug Logging

See ./READMEs/README.logging.md

@section asan Building with ASAN

Under GCC you can select for the build to be instrumented with the Address
Sanitizer, using `cmake .. -DCMAKE_BUILD_TYPE=DEBUG -DLWS_WITH_ASAN=1`.  LWS is routinely run during development with valgrind, but ASAN is capable of finding different issues at runtime, like operations which are not strictly defined in the C
standard and depend on platform behaviours.

Run your application like this

```
	$ sudo ASAN_OPTIONS=verbosity=2:halt_on_error=1  /usr/local/bin/lwsws
```

and attach gdb to catch the place it halts.

@section extpoll External Polling Loop support

**libwebsockets** maintains an internal `poll()` array for all of its
sockets, but you can instead integrate the sockets into an
external polling array.  That's needed if **libwebsockets** will
cooperate with an existing poll array maintained by another
server.

Three callbacks `LWS_CALLBACK_ADD_POLL_FD`, `LWS_CALLBACK_DEL_POLL_FD`
and `LWS_CALLBACK_CHANGE_MODE_POLL_FD` appear in the callback for protocol 0
and allow interface code to manage socket descriptors in other poll loops.

You can pass all pollfds that need service to `lws_service_fd()`, even
if the socket or file does not belong to **libwebsockets** it is safe.

If **libwebsocket** handled it, it zeros the pollfd `revents` field before returning.
So you can let **libwebsockets** try and if `pollfd->revents` is nonzero on return,
you know it needs handling by your code.

Also note that when integrating a foreign event loop like libev or libuv where
it doesn't natively use poll() semantics, and you must return a fake pollfd
reflecting the real event:

 - be sure you set .events to .revents value as well in the synthesized pollfd

 - check the built-in support for the event loop if possible (eg, ./lib/libuv.c)
   to see how it interfaces to lws
   
 - use LWS_POLLHUP / LWS_POLLIN / LWS_POLLOUT from libwebsockets.h to avoid
   losing windows compatibility

You also need to take care about "forced service" somehow... these are cases
where the network event was consumed, incoming data was all read, for example,
but the work arising from it was not completed.  There will not be any more
network event to trigger the remaining work, Eg, we read compressed data, but
we did not use up all the decompressed data before returning to the event loop
because we had to write some of it.

Lws provides an API to determine if anyone is waiting for forced service,
`lws_service_adjust_timeout(context, 1, tsi)`, normally tsi is 0.  If it returns
0, then at least one connection has pending work you can get done by calling
`lws_service_tsi(context, -1, tsi)`, again normally tsi is 0.

For eg, the default poll() event loop, or libuv/ev/event, lws does this
checking for you and handles it automatically.  But in the external polling
loop case, you must do it explicitly.  Handling it after every normal service
triggered by the external poll fd should be enough, since the situations needing
it are initially triggered by actual network events.

An example of handling it is shown in the test-server code specific to
external polling.

@section cpp Using with in c++ apps

The library is ready for use by C++ apps.  You can get started quickly by
copying the test server

```
	$ cp test-apps/test-server.c test.cpp
```

and building it in C++ like this

```
	$ g++ -DINSTALL_DATADIR=\"/usr/share\" -ocpptest test.cpp -lwebsockets
```

`INSTALL_DATADIR` is only needed because the test server uses it as shipped, if
you remove the references to it in your app you don't need to define it on
the g++ line either.


@section headerinfo Availability of header information

HTTP Header information is managed by a pool of "ah" structs.  These are a
limited resource so there is pressure to free the headers and return the ah to
the pool for reuse.

For that reason header information on HTTP connections that get upgraded to
websockets is lost after the ESTABLISHED callback.  Anything important that
isn't processed by user code before then should be copied out for later.

For HTTP connections that don't upgrade, header info remains available the
whole time.

@section http2compat Code Requirements for HTTP/2 compatibility

Websocket connections only work over http/1, so there is nothing special to do
when you want to enable -DLWS_WITH_HTTP2=1.

The internal http apis already follow these requirements and are compatible with
http/2 already.  So if you use stuff like mounts and serve stuff out of the
filesystem, there's also nothing special to do.

However if you are getting your hands dirty with writing response headers, or
writing bulk data over http/2, you need to observe these rules so that it will
work over both http/1.x and http/2 the same.

1) LWS_PRE requirement applies on ALL lws_write().  For http/1, you don't have
to take care of LWS_PRE for http data, since it is just sent straight out.
For http/2, it will write up to LWS_PRE bytes behind the buffer start to create
the http/2 frame header.

This has implications if you treated the input buffer to lws_write() as const...
it isn't any more with http/2, up to 9 bytes behind the buffer will be trashed.

2) Headers are encoded using a sophisticated scheme in http/2.  The existing
header access apis are already made compatible for incoming headers,
for outgoing headers you must:

 - observe the LWS_PRE buffer requirement mentioned above
 
 - Use `lws_add_http_header_status()` to add the transaction status (200 etc)
 
 - use lws apis `lws_add_http_header_by_name()` and `lws_add_http_header_by_token()`
   to put the headers into the buffer (these will translate what is actually
   written to the buffer depending on if the connection is in http/2 mode or not)
   
 - use the `lws api lws_finalize_http_header()` api after adding the last
   response header
   
 - write the header using lws_write(..., `LWS_WRITE_HTTP_HEADERS`);
 
 3) http/2 introduces per-stream transmit credit... how much more you can send
 on a stream is decided by the peer.  You start off with some amount, as the
 stream sends stuff lws will reduce your credit accordingly, when it reaches
 zero, you must not send anything further until lws receives "more credit" for
 that stream the peer.  Lws will suppress writable callbacks if you hit 0 until
 more credit for the stream appears, and lws built-in file serving (via mounts
 etc) already takes care of observing the tx credit restrictions.  However if
 you write your own code that wants to send http data, you must consult the
 `lws_get_peer_write_allowance()` api to find out the state of your tx credit.
 For http/1, it will always return (size_t)-1, ie, no limit.
 
 This is orthogonal to the question of how much space your local side's kernel
 will make to buffer your send data on that connection.  So although the result
 from `lws_get_peer_write_allowance()` is "how much you can send" logically,
 and may be megabytes if the peer allows it, you should restrict what you send
 at one time to whatever your machine will generally accept in one go, and
 further reduce that amount if `lws_get_peer_write_allowance()` returns
 something smaller.  If it returns 0, you should not consume or send anything
 and return having asked for callback on writable, it will only come back when
 more tx credit has arrived for your stream.
 
 4) Header names with captital letters are illegal in http/2.  Header names in
 http/1 are case insensitive.  So if you generate headers by name, change all
 your header name strings to lower-case to be compatible both ways.
 
 5) Chunked Transfer-encoding is illegal in http/2, http/2 peers will actively
 reject it.  Lws takes care of removing the header and converting CGIs that
 emit chunked into unchunked automatically for http/2 connections.
 
If you follow these rules, your code will automatically work with both http/1.x
and http/2.

@section ka TCP Keepalive

It is possible for a connection which is not being used to send to die
silently somewhere between the peer and the side not sending.  In this case
by default TCP will just not report anything and you will never get any more
incoming data or sign the link is dead until you try to send.

To deal with getting a notification of that situation, you can choose to
enable TCP keepalives on all **libwebsockets** sockets, when you create the
context.

To enable keepalive, set the ka_time member of the context creation parameter
struct to a nonzero value (in seconds) at context creation time.  You should
also fill ka_probes and ka_interval in that case.

With keepalive enabled, the TCP layer will send control packets that should
stimulate a response from the peer without affecting link traffic.  If the
response is not coming, the socket will announce an error at `poll()` forcing
a close.

Note that BSDs don't support keepalive time / probes / interval per-socket
like Linux does.  On those systems you can enable keepalive by a nonzero
value in `ka_time`, but the systemwide kernel settings for the time / probes/
interval are used, regardless of what nonzero value is in `ka_time`.


@section sslopt Optimizing SSL connections

There's a member `ssl_cipher_list` in the `lws_context_creation_info` struct
which allows the user code to restrict the possible cipher selection at
context-creation time.

You might want to look into that to stop the ssl peers selecting a cipher which
is too computationally expensive.  To use it, point it to a string like

	`"RC4-MD5:RC4-SHA:AES128-SHA:AES256-SHA:HIGH:!DSS:!aNULL"`

if left `NULL`, then the "DEFAULT" set of ciphers are all possible to select.

You can also set it to `"ALL"` to allow everything (including insecure ciphers).


@section sslcerts Passing your own cert information direct to SSL_CTX

For most users it's enough to pass the SSL certificate and key information by
giving filepaths to the info.ssl_cert_filepath and info.ssl_private_key_filepath
members when creating the vhost.

If you want to control that from your own code instead, you can do so by leaving
the related info members NULL, and setting the info.options flag
LWS_SERVER_OPTION_CREATE_VHOST_SSL_CTX at vhost creation time.  That will create
the vhost SSL_CTX without any certificate, and allow you to use the callback
LWS_CALLBACK_OPENSSL_LOAD_EXTRA_SERVER_VERIFY_CERTS to add your certificate to
the SSL_CTX directly.  The vhost SSL_CTX * is in the user parameter in that
callback.

@section clientasync Async nature of client connections

When you call `lws_client_connect_info(..)` and get a `wsi` back, it does not
mean your connection is active.  It just means it started trying to connect.

Your client connection is actually active only when you receive
`LWS_CALLBACK_CLIENT_ESTABLISHED` for it.

There's a 5 second timeout for the connection, and it may give up or die for
other reasons, if any of that happens you'll get a
`LWS_CALLBACK_CLIENT_CONNECTION_ERROR` callback on protocol 0 instead for the
`wsi`.

After attempting the connection and getting back a non-`NULL` `wsi` you should
loop calling `lws_service()` until one of the above callbacks occurs.

As usual, see [test-client.c](../test-apps/test-client.c) for example code.

Notice that the client connection api tries to progress the connection
somewhat before returning.  That means it's possible to get callbacks like
CONNECTION_ERROR on the new connection before your user code had a chance to
get the wsi returned to identify it (in fact if the connection did fail early,
NULL will be returned instead of the wsi anyway).

To avoid that problem, you can fill in `pwsi` in the client connection info
struct to point to a struct lws that get filled in early by the client
connection api with the related wsi.  You can then check for that in the
callback to confirm the identity of the failing client connection.


@section fileapi Lws platform-independent file access apis

lws now exposes his internal platform file abstraction in a way that can be
both used by user code to make it platform-agnostic, and be overridden or
subclassed by user code.  This allows things like handling the URI "directory
space" as a virtual filesystem that may or may not be backed by a regular
filesystem.  One example use is serving files from inside large compressed
archive storage without having to unpack anything except the file being
requested.

The test server shows how to use it, basically the platform-specific part of
lws prepares a file operations structure that lives in the lws context.

The user code can get a pointer to the file operations struct

```
	LWS_VISIBLE LWS_EXTERN struct lws_plat_file_ops *
		`lws_get_fops`(struct lws_context *context);
```

and then can use helpers to also leverage these platform-independent
file handling apis

```
	lws_fop_fd_t
	`lws_plat_file_open`(struct lws_plat_file_ops *fops, const char *filename,
			   lws_fop_flags_t *flags)
	int
	`lws_plat_file_close`(lws_fop_fd_t fop_fd)

	unsigned long
	`lws_plat_file_seek_cur`(lws_fop_fd_t fop_fd, lws_fileofs_t offset)

	int
	`lws_plat_file_read`(lws_fop_fd_t fop_fd, lws_filepos_t *amount,
		   uint8_t *buf, lws_filepos_t len)

	int
	`lws_plat_file_write`(lws_fop_fd_t fop_fd, lws_filepos_t *amount,
		   uint8_t *buf, lws_filepos_t len )
```

Generic helpers are provided which provide access to generic fops information or
call through to the above fops

```
lws_filepos_t
lws_vfs_tell(lws_fop_fd_t fop_fd);

lws_filepos_t
lws_vfs_get_length(lws_fop_fd_t fop_fd);

uint32_t
lws_vfs_get_mod_time(lws_fop_fd_t fop_fd);

lws_fileofs_t
lws_vfs_file_seek_set(lws_fop_fd_t fop_fd, lws_fileofs_t offset);

lws_fileofs_t
lws_vfs_file_seek_end(lws_fop_fd_t fop_fd, lws_fileofs_t offset);
```


The user code can also override or subclass the file operations, to either
wrap or replace them.  An example is shown in test server.

### Changes from v2.1 and before fops

There are several changes:

1) Pre-2.2 fops directly used platform file descriptors.  Current fops returns and accepts a wrapper type lws_fop_fd_t which is a pointer to a malloc'd struct containing information specific to the filesystem implementation.

2) Pre-2.2 fops bound the fops to a wsi.  This is completely removed, you just give a pointer to the fops struct that applies to this file when you open it.  Afterwards, the operations in the fops just need the lws_fop_fd_t returned from the open.

3) Everything is wrapped in typedefs.  See lws-plat-unix.c for examples of how to implement.

4) Position in the file, File Length, and a copy of Flags left after open are now generically held in the fop_fd.
VFS implementation must set and manage this generic information now.  See the implementations in lws-plat-unix.c for
examples.

5) The file length is no longer set at a pointer provided by the open() fop.  The api `lws_vfs_get_length()` is provided to
get the file length after open.

6) If your file namespace is virtual, ie, is not reachable by platform fops directly, you must set LWS_FOP_FLAG_VIRTUAL
on the flags during open.

7) There is an optional `mod_time` uint32_t member in the generic fop_fd.  If you are able to set it during open, you
should indicate it by setting `LWS_FOP_FLAG_MOD_TIME_VALID` on the flags.

@section rawfd RAW file descriptor polling

LWS allows you to include generic platform file descriptors in the lws service / poll / event loop.

Open your fd normally and then

```
	lws_sock_file_fd_type u;

	u.filefd = your_open_file_fd;

	if (!lws_adopt_descriptor_vhost(vhost, 0, u,
					"protocol-name-to-bind-to",
					optional_wsi_parent_or_NULL)) {
		// failed
	}

	// OK
```

A wsi is created for the file fd that acts like other wsi, you will get these
callbacks on the named protocol

```
	LWS_CALLBACK_RAW_ADOPT_FILE
	LWS_CALLBACK_RAW_RX_FILE
	LWS_CALLBACK_RAW_WRITEABLE_FILE
	LWS_CALLBACK_RAW_CLOSE_FILE
```

starting with LWS_CALLBACK_RAW_ADOPT_FILE.

The minimal example `raw/minimal-raw-file` demonstrates how to use it.

`protocol-lws-raw-test` plugin also provides a method for testing this with
`libwebsockets-test-server-v2.0`:

The plugin creates a FIFO on your system called "/tmp/lws-test-raw"

You can feed it data through the FIFO like this

```
  $ sudo sh -c "echo hello > /tmp/lws-test-raw"
```

This plugin simply prints the data.  But it does it through the lws event
loop / service poll.

@section rawsrvsocket RAW server socket descriptor polling

You can also enable your vhost to accept RAW socket connections, in addition to
HTTP[s] and WS[s].  If the first bytes written on the connection are not a
valid HTTP method, then the connection switches to RAW mode.

This is disabled by default, you enable it by setting the `.options` flag
LWS_SERVER_OPTION_FALLBACK_TO_APPLY_LISTEN_ACCEPT_CONFIG, and setting
`.listen_accept_role` to `"raw-skt"` when creating the vhost.

RAW mode socket connections receive the following callbacks

```
	LWS_CALLBACK_RAW_ADOPT
	LWS_CALLBACK_RAW_RX
	LWS_CALLBACK_RAW_WRITEABLE
	LWS_CALLBACK_RAW_CLOSE
```

You can control which protocol on your vhost handles these RAW mode
incoming connections by setting the vhost info struct's `.listen_accept_protocol`
to the vhost protocol name to use.

`protocol-lws-raw-test` plugin provides a method for testing this with
`libwebsockets-test-server-v2.0`:

Run libwebsockets-test-server-v2.0 and connect to it by telnet, eg

```
    $ telnet 127.0.0.1 7681
```

type something that isn't a valid HTTP method and enter, before the
connection times out.  The connection will switch to RAW mode using this
protocol, and pass the unused rx as a raw RX callback.
    
The test protocol echos back what was typed on telnet to telnet.

@section rawclientsocket RAW client socket descriptor polling

You can now also open RAW socket connections in client mode.

Follow the usual method for creating a client connection, but set the
`info.method` to "RAW".  When the connection is made, the wsi will be
converted to RAW mode and operate using the same callbacks as the
server RAW sockets described above.

The libwebsockets-test-client supports this using raw:// URLS.  To
test, open a netcat listener in one window

```
 $ nc -l 9999
```

and in another window, connect to it using the test client

```
 $ libwebsockets-test-client raw://127.0.0.1:9999
```

The connection should succeed, and text typed in the netcat window (including a CRLF)
will be received in the client.

@section rawudp RAW UDP socket integration

Lws provides an api to create, optionally bind, and adopt a RAW UDP
socket (RAW here means an uninterpreted normal UDP socket, not a
"raw socket").

```
LWS_VISIBLE LWS_EXTERN struct lws *
lws_create_adopt_udp(struct lws_vhost *vhost, int port, int flags,
		     const char *protocol_name, struct lws *parent_wsi);
```

`flags` should be `LWS_CAUDP_BIND` if the socket will receive packets.

The callbacks `LWS_CALLBACK_RAW_ADOPT`, `LWS_CALLBACK_RAW_CLOSE`,
`LWS_CALLBACK_RAW_RX` and `LWS_CALLBACK_RAW_WRITEABLE` apply to the
wsi.  But UDP is different than TCP in some fundamental ways.

For receiving on a UDP connection, data becomes available at
`LWS_CALLBACK_RAW_RX` as usual, but because there is no specific
connection with UDP, it is necessary to also get the source address of
the data separately, using `struct lws_udp * lws_get_udp(wsi)`.
You should take a copy of the `struct lws_udp` itself (not the
pointer) and save it for when you want to write back to that peer.

Writing is also a bit different for UDP.  By default, the system has no
idea about the receiver state and so asking for a `callback_on_writable()`
always believes that the socket is writeable... the callback will
happen next time around the event loop.

With UDP, there is no single "connection".  You need to write with sendto() and
direct the packets to a specific destination.  To return packets to a
peer who sent something earlier and you copied his `struct lws_udp`, you
use the .sa and .salen members as the last two parameters of the sendto().

The kernel may not accept to buffer / write everything you wanted to send.
So you are responsible to watch the result of sendto() and resend the
unsent part next time (which may involve adding new protocol headers to
the remainder depending on what you are doing).

@section ecdh ECDH Support

ECDH Certs are now supported.  Enable the CMake option

	cmake .. -DLWS_SSL_SERVER_WITH_ECDH_CERT=1 

**and** the info->options flag

	LWS_SERVER_OPTION_SSL_ECDH

to build in support and select it at runtime.

@section sslinfo SSL info callbacks

OpenSSL allows you to receive callbacks for various events defined in a
bitmask in openssl/ssl.h.  The events include stuff like TLS Alerts.

By default, lws doesn't register for these callbacks.

However if you set the info.ssl_info_event_mask to nonzero (ie, set some
of the bits in it like `SSL_CB_ALERT` at vhost creation time, then
connections to that vhost will call back using LWS_CALLBACK_SSL_INFO
for the wsi, and the `in` parameter will be pointing to a struct of
related args:

```
struct lws_ssl_info {
	int where;
	int ret;
};
```

The default callback handler in lws has a handler for LWS_CALLBACK_SSL_INFO
which prints the related information,  You can test it using the switch
-S -s  on `libwebsockets-test-server-v2.0`.

Returning nonzero from the callback will close the wsi.

@section smp SMP / Multithreaded service

SMP support is integrated into LWS without any internal threading.  It's
very simple to use, libwebsockets-test-server-pthread shows how to do it,
use -j n argument there to control the number of service threads up to 32.

Two new members are added to the info struct

	unsigned int count_threads;
	unsigned int fd_limit_per_thread;
	
leave them at the default 0 to get the normal singlethreaded service loop.

Set count_threads to n to tell lws you will have n simultaneous service threads
operating on the context.

There is still a single listen socket on one port, no matter how many
service threads.

When a connection is made, it is accepted by the service thread with the least
connections active to perform load balancing.

The user code is responsible for spawning n threads running the service loop
associated to a specific tsi (Thread Service Index, 0 .. n - 1).  See
the libwebsockets-test-server-pthread for how to do.

If you leave fd_limit_per_thread at 0, then the process limit of fds is shared
between the service threads; if you process was allowed 1024 fds overall then
each thread is limited to 1024 / n.

You can set fd_limit_per_thread to a nonzero number to control this manually, eg
the overall supported fd limit is less than the process allowance.

You can control the context basic data allocation for multithreading from Cmake
using -DLWS_MAX_SMP=, if not given it's set to 1.  The serv_buf allocation
for the threads (currently 4096) is made at runtime only for active threads.

Because lws will limit the requested number of actual threads supported
according to LWS_MAX_SMP, there is an api lws_get_count_threads(context) to
discover how many threads were actually allowed when the context was created.

See the test-server-pthreads.c sample for how to use.

@section smplocking SMP Locking Helpers

Lws provide a set of pthread mutex helpers that reduce to no code or
variable footprint in the case that LWS_MAX_SMP == 1.

Define your user mutex like this

```
	lws_pthread_mutex(name);
```

If LWS_MAX_SMP > 1, this produces `pthread_mutex_t name;`.  In the case
LWS_MAX_SMP == 1, it produces nothing.

Likewise these helpers for init, destroy, lock and unlock


```
	void lws_pthread_mutex_init(pthread_mutex_t *lock)
	void lws_pthread_mutex_destroy(pthread_mutex_t *lock)
	void lws_pthread_mutex_lock(pthread_mutex_t *lock)
	void lws_pthread_mutex_unlock(pthread_mutex_t *lock)
```

resolve to nothing if LWS_MAX_SMP == 1, otherwise produce the equivalent
pthread api.

pthreads is required in lws only if LWS_MAX_SMP > 1.


@section libevuv libev / libuv / libevent support

You can select either or both

	-DLWS_WITH_LIBEV=1
	-DLWS_WITH_LIBUV=1
	-DLWS_WITH_LIBEVENT=1

at cmake configure-time.  The user application may use one of the
context init options flags

	LWS_SERVER_OPTION_LIBEV
	LWS_SERVER_OPTION_LIBUV
	LWS_SERVER_OPTION_LIBEVENT

to indicate it will use one of the event libraries at runtime.

libev and libevent headers conflict, they both define critical constants like
EV_READ to different values.  Attempts to discuss clearing that up with both
libevent and libev did not get anywhere useful.  Therefore CMakeLists.txt will
error out if you enable both LWS_WITH_LIBEV and LWS_WITH_LIBEVENT.

In addition depending on libev / compiler version, building anything with libev
apis using gcc may blow strict alias warnings (which are elevated to errors in
lws).  I did some googling at found these threads related to it, the issue goes
back at least to 2010 on and off

https://github.com/redis/hiredis/issues/434
https://bugs.gentoo.org/show_bug.cgi?id=615532
http://lists.schmorp.de/pipermail/libev/2010q1/000916.html
http://lists.schmorp.de/pipermail/libev/2010q1/000920.html
http://lists.schmorp.de/pipermail/libev/2010q1/000923.html

We worked around this problem by disabling -Werror on the parts of lws that
use libev.  FWIW as of Dec 2019 using Fedora 31 libev 4.27.1 and its gcc 9.2.1
doesn't seem to trigger the problem even without the workaround.

For these reasons and the response I got trying to raise these issues with
them, if you have a choice about event loop, I would gently encourage you
to avoid libev.  Where lws uses an event loop itself, eg in lwsws, we use
libuv.

@section extopts Extension option control from user code

User code may set per-connection extension options now, using a new api
`lws_set_extension_option()`.

This should be called from the ESTABLISHED callback like this
```
	 lws_set_extension_option(wsi, "permessage-deflate",
	                          "rx_buf_size", "12"); /* 1 << 12 */
```

If the extension is not active (missing or not negotiated for the
connection, or extensions are disabled on the library) the call is
just returns -1.  Otherwise the connection's extension has its
named option changed.

The extension may decide to alter or disallow the change, in the
example above permessage-deflate restricts the size of his rx
output buffer also considering the protocol's rx_buf_size member.


@section httpsclient Client connections as HTTP[S] rather than WS[S]

You may open a generic http client connection using the same
struct lws_client_connect_info used to create client ws[s]
connections.

To stay in http[s], set the optional info member "method" to
point to the string "GET" instead of the default NULL.

After the server headers are processed, when payload from the
server is available the callback LWS_CALLBACK_RECEIVE_CLIENT_HTTP
will be made.

You can choose whether to process the data immediately, or
queue a callback when an outgoing socket is writeable to provide
flow control, and process the data in the writable callback.

Either way you use the api `lws_http_client_read()` to access the
data, eg

```
	case LWS_CALLBACK_RECEIVE_CLIENT_HTTP:
		{
			char buffer[1024 + LWS_PRE];
			char *px = buffer + LWS_PRE;
			int lenx = sizeof(buffer) - LWS_PRE;

			lwsl_notice("LWS_CALLBACK_RECEIVE_CLIENT_HTTP\n");

			/*
			 * Often you need to flow control this by something
			 * else being writable.  In that case call the api
			 * to get a callback when writable here, and do the
			 * pending client read in the writeable callback of
			 * the output.
			 */
			if (lws_http_client_read(wsi, &px, &lenx) < 0)
				return -1;
			while (lenx--)
				putchar(*px++);
		}
		break;
```

Notice that if you will use SSL client connections on a vhost, you must
prepare the client SSL context for the vhost after creating the vhost, since
this is not normally done if the vhost was set up to listen / serve.  Call
the api lws_init_vhost_client_ssl() to also allow client SSL on the vhost.

@section clipipe Pipelining Client Requests to same host

If you are opening more client requests to the same host and port, you
can give the flag LCCSCF_PIPELINE on `info.ssl_connection` to indicate
you wish to pipeline them.

Without the flag, the client connections will occur concurrently using a
socket and tls wrapper if requested for each connection individually.
That is fast, but resource-intensive.

With the flag, lws will queue subsequent client connections on the first
connection to the same host and port.  When it has confirmed from the
first connection that pipelining / keep-alive is supported by the server,
it lets the queued client pipeline connections send their headers ahead
of time to create a pipeline of requests on the server side.

In this way only one tcp connection and tls wrapper is required to transfer
all the transactions sequentially.  It takes a little longer but it
can make a significant difference to resources on both sides.

If lws learns from the first response header that keepalive is not possible,
then it marks itself with that information and detaches any queued clients
to make their own individual connections as a fallback.

Lws can also intelligently combine multiple ongoing client connections to
the same host and port into a single http/2 connection with multiple
streams if the server supports it.

Unlike http/1 pipelining, with http/2 the client connections all occur
simultaneously using h2 stream multiplexing inside the one tcp + tls
connection.

You can turn off the h2 client support either by not building lws with
`-DLWS_WITH_HTTP2=1` or giving the `LCCSCF_NOT_H2` flag in the client
connection info struct `ssl_connection` member.

@section vhosts Using lws vhosts

If you set LWS_SERVER_OPTION_EXPLICIT_VHOSTS options flag when you create
your context, it won't create a default vhost using the info struct
members for compatibility.  Instead you can call lws_create_vhost()
afterwards to attach one or more vhosts manually.

```
	LWS_VISIBLE struct lws_vhost *
	lws_create_vhost(struct lws_context *context,
			 struct lws_context_creation_info *info);
```

lws_create_vhost() uses the same info struct as lws_create_context(),
it ignores members related to context and uses the ones meaningful
for vhost (marked with VH in libwebsockets.h).

```
	struct lws_context_creation_info {
		int port;					/* VH */
		const char *iface;				/* VH */
		const struct lws_protocols *protocols;		/* VH */
		const struct lws_extension *extensions;		/* VH */
	...
```

When you attach the vhost, if the vhost's port already has a listen socket
then both vhosts share it and use SNI (is SSL in use) or the Host: header
from the client to select the right one.  Or if no other vhost already
listening the a new listen socket is created.

There are some new members but mainly it's stuff you used to set at
context creation time.


@section sni How lws matches hostname or SNI to a vhost

LWS first strips any trailing :port number.

Then it tries to find an exact name match for a vhost listening on the correct
port, ie, if SNI or the Host: header provided abc.com:1234, it will match on a
vhost named abc.com that is listening on port 1234.

If there is no exact match, lws will consider wildcard matches, for example
if cats.abc.com:1234 is provided by the client by SNI or Host: header, it will
accept a vhost "abc.com" listening on port 1234.  If there was a better, exact,
match, it will have been chosen in preference to this.

Connections with SSL will still have the client go on to check the
certificate allows wildcards and error out if not.
 


@section mounts Using lws mounts on a vhost

The last argument to lws_create_vhost() lets you associate a linked
list of lws_http_mount structures with that vhost's URL 'namespace', in
a similar way that unix lets you mount filesystems into areas of your /
filesystem how you like and deal with the contents transparently.

```
	struct lws_http_mount {
		struct lws_http_mount *mount_next;
		const char *mountpoint; /* mountpoint in http pathspace, eg, "/" */
		const char *origin; /* path to be mounted, eg, "/var/www/warmcat.com" */
		const char *def; /* default target, eg, "index.html" */
	
		struct lws_protocol_vhost_options *cgienv;
	
		int cgi_timeout;
		int cache_max_age;
	
		unsigned int cache_reusable:1;
		unsigned int cache_revalidate:1;
		unsigned int cache_intermediaries:1;
	
		unsigned char origin_protocol;
		unsigned char mountpoint_len;
	};
```

The last mount structure should have a NULL mount_next, otherwise it should
point to the 'next' mount structure in your list.

Both the mount structures and the strings must persist until the context is
destroyed, since they are not copied but used in place.

`.origin_protocol` should be one of

```
	enum {
		LWSMPRO_HTTP,
		LWSMPRO_HTTPS,
		LWSMPRO_FILE,
		LWSMPRO_CGI,
		LWSMPRO_REDIR_HTTP,
		LWSMPRO_REDIR_HTTPS,
		LWSMPRO_CALLBACK,
	};
```

 - LWSMPRO_FILE is used for mapping url namespace to a filesystem directory and
serve it automatically.

 - LWSMPRO_CGI associates the url namespace with the given CGI executable, which
runs when the URL is accessed and the output provided to the client.

 - LWSMPRO_REDIR_HTTP and LWSMPRO_REDIR_HTTPS auto-redirect clients to the given
origin URL.

 - LWSMPRO_CALLBACK causes the http connection to attach to the callback
associated with the named protocol (which may be a plugin).


@section mountcallback Operation of LWSMPRO_CALLBACK mounts

The feature provided by CALLBACK type mounts is binding a part of the URL
namespace to a named protocol callback handler.

This allows protocol plugins to handle areas of the URL namespace.  For example
in test-server-v2.0.c, the URL area "/formtest" is associated with the plugin
providing "protocol-post-demo" like this

```
	static const struct lws_http_mount mount_post = {
		NULL,		/* linked-list pointer to next*/
		"/formtest",		/* mountpoint in URL namespace on this vhost */
		"protocol-post-demo",	/* handler */
		NULL,	/* default filename if none given */
		NULL,
		0,
		0,
		0,
		0,
		0,
		LWSMPRO_CALLBACK,	/* origin points to a callback */
		9,			/* strlen("/formtest"), ie length of the mountpoint */
	};
```

Client access to /formtest[anything] will be passed to the callback registered
with the named protocol, which in this case is provided by a protocol plugin.

Access by all methods, eg, GET and POST are handled by the callback.

protocol-post-demo deals with accepting and responding to the html form that
is in the test server HTML.

When a connection accesses a URL related to a CALLBACK type mount, the
connection protocol is changed until the next access on the connection to a
URL outside the same CALLBACK mount area.  User space on the connection is
arranged to be the size of the new protocol user space allocation as given in
the protocol struct.

This allocation is only deleted / replaced when the connection accesses a
URL region with a different protocol (or the default protocols[0] if no
CALLBACK area matches it).

This "binding connection to a protocol" lifecycle in managed by
`LWS_CALLBACK_HTTP_BIND_PROTOCOL` and `LWS_CALLBACK_HTTP_DROP_PROTOCOL`.
Because of HTTP/1.1 connection pipelining, one connection may perform
many transactions, each of which may map to different URLs and need
binding to different protocols.  So these messages are used to
create the binding of the wsi to your protocol including any
allocations, and to destroy the binding, at which point you should
destroy any related allocations.

@section BINDTODEV SO_BIND_TO_DEVICE

The .bind_iface flag in the context / vhost creation struct lets you
declare that you want all traffic for listen and transport on that
vhost to be strictly bound to the network interface named in .iface.

This Linux-only feature requires SO_BIND_TO_DEVICE, which in turn
requires CAP_NET_RAW capability... root has this capability.

However this feature needs to apply the binding also to accepted
sockets during normal operation, which implies the server must run
the whole time as root.

You can avoid this by using the Linux capabilities feature to have
the unprivileged user inherit just the CAP_NET_RAW capability.

You can confirm this with the test server


```
 $ sudo /usr/local/bin/libwebsockets-test-server -u agreen -i eno1 -k
```

The part that ensures the capability is inherited by the unprivileged
user is

```
#if defined(LWS_HAVE_SYS_CAPABILITY_H) && defined(LWS_HAVE_LIBCAP)
                        info.caps[0] = CAP_NET_RAW;
                        info.count_caps = 1;
#endif
```


@section dim Dimming webpage when connection lost

The lws test plugins' html provides useful feedback on the webpage about if it
is still connected to the server, by greying out the page if not.  You can
also add this to your own html easily

 - include lws-common.js from your HEAD section
 
   \<script src="/lws-common.js">\</script>
   
 - dim the page during initialization, in a script section on your page
 
   lws_gray_out(true,{'zindex':'499'});
   
 - in your ws onOpen(), remove the dimming
 
   lws_gray_out(false);
   
 - in your ws onClose(), reapply the dimming
 
   lws_gray_out(true,{'zindex':'499'});

@section errstyle Styling http error pages

In the code, http errors should be handled by `lws_return_http_status()`.

There are basically two ways... the vhost can be told to redirect to an "error
page" URL in response to specifically a 404... this is controlled by the
context / vhost info struct (`struct lws_context_creation_info`) member
`.error_document_404`... if non-null the client is redirected to this string.

If it wasn't redirected, then the response code html is synthesized containing
the user-selected text message and attempts to pull in `/error.css` for styling.

If this file exists, it can be used to style the error page.  See 
https://libwebsockets.org/git/badrepo for an example of what can be done (
and https://libwebsockets.org/error.css for the corresponding css).


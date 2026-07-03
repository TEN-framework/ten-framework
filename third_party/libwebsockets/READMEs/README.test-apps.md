Overview of lws test apps
=========================

Are you building a client?  You just need to look at the test client
[libwebsockets-test-client](../test-apps/test-client.c).

If you are building a standalone server, there are three choices, in order of
preferability.

1) lwsws + protocol plugins

Lws provides a generic web server app that can be configured with JSON
config files.  https://libwebsockets.org itself uses this method.

With lwsws handling the serving part, you only need to write an lws protocol
plugin.  See [plugin-standalone](../plugin-standalone) for an example of how
to do that outside lws itself, using lws public apis.

 $ cmake .. -DLWS_WITH_LWSWS=1

See [README.lwsws.md](../READMEs/README.lwsws.md) for information on how to configure
lwsws.

NOTE this method implies libuv is used by lws, to provide crossplatform
implementations of timers, dynamic lib loading etc for plugins and lwsws.

2) Using plugins in code

This method lets you configure web serving in code, instead of using lwsws.

Plugins are still used, but you have a choice whether to dynamically load
them or statically include them.  In this example, they are dynamically
loaded.

 $ cmake .. -DLWS_WITH_PLUGINS=1

See, eg, the [test-server](../test-apps/test-server.c)

3) protocols in the server app

This is the original way lws implemented servers, plugins and libuv are not
required, but without plugins separating the protocol code directly, the
combined code is all squidged together and is much less maintainable.

This method is still supported in lws but all ongoing and future work is
being done in protocol plugins only.

You can simply include the plugin contents and have it buit statically into
your server, just define this before including the plugin source

```
#define LWS_PLUGIN_STATIC
```

This gets you most of the advantages without needing dynamic loading +
libuv.


Notes about lws test apps
=========================

@section tsb Testing server with a browser

If you run [libwebsockets-test-server](../test-apps/test-server.c) and point your browser
(eg, Chrome) to

	http://127.0.0.1:7681

It will fetch a script in the form of `test.html`, and then run the
script in there on the browser to open a websocket connection.
Incrementing numbers should appear in the browser display.

By default the test server logs to both stderr and syslog, you can control
what is logged using `-d <log level>`, see later.


@section tsd Running test server as a Daemon

You can use the -D option on the test server to have it fork into the
background and return immediately.  In this daemonized mode all stderr is
disabled and logging goes only to syslog, eg, `/var/log/messages` or similar.

The server maintains a lockfile at `/tmp/.lwsts-lock` that contains the pid
of the parent process, and deletes this file when the parent process
terminates.

To stop the daemon, do
```
       $ kill \`cat /tmp/.lwsts-lock\`
```
If it finds a stale lock (the pid mentioned in the file does not exist
any more) it will delete the lock and create a new one during startup.

If the lock is valid, the daemon will exit with a note on stderr that
it was already running.

@section clicert Testing Client Certs

Here is a very quick way to create a CA, and a client and server cert from it,
for testing.

```
$ cp -rp ./scripts/client-ca /tmp
$ cd /tmp/client-ca
$ ./create-ca.sh
$ ./create-server-cert.sh server
$ ./create-client-cert.sh client
```

The last step wants an export password, you will need this password again to
import the p12 format certificate into your browser.

This will get you the following

|name|function|
|----|--------|
|ca.pem|Your Certificate Authority cert|
|ca.key|Private key for the CA cert|
|client.pem|Client certificate, signed by your CA|
|client.key|Client private key|
|client.p12|combined client.pem + client.key in p12 format for browsers|
|server.pem|Server cert, signed by your CA|
|server.key|Server private key|

You can confirm yourself the client and server certs are signed by the CA.

```
 $ openssl verify -verbose -trusted ca.pem server.pem
 $ openssl verify -verbose -trusted ca.pem client.pem
```

Import the client.p12 file into your browser.  In FFOX57 it's

 - preferences
 - Privacy & Security
 - Certificates | View Certificates
 - Certificate Manager | Your Certificates | Import...
 - Enter the password you gave when creating client1.p12
 - Click OK.

You can then run the test server like this:

```
 $ libwebsockets-test-server -s -A ca.pem -K server.key -C server.pem -v
```

When you connect your browser to https://localhost:7681 after accepting the
selfsigned server cert, your browser will pop up a prompt to send the server
your client cert (the -v switch enables this).  The server will only accept
a client cert that has been signed by ca.pem.

@section sssl Using SSL on the server side

To test it using SSL/WSS, just run the test server with
```
	$ libwebsockets-test-server --ssl
```
and use the URL
```
	https://127.0.0.1:7681
```
The connection will be entirely encrypted using some generated
certificates that your browser will not accept, since they are
not signed by any real Certificate Authority.  Just accept the
certificates in the browser and the connection will proceed
in first https and then websocket wss, acting exactly the
same.

[test-server.c](../test-apps/test-server.c) is all that is needed to use libwebsockets for
serving both the script html over http and websockets.

@section lwstsdynvhost Dynamic Vhosts

You can send libwebsockets-test-server or libwebsockets-test-server-v2.0 a SIGUSR1
to toggle the creation and destruction of an identical second vhost on port + 1.

This is intended as a test and demonstration for how to bring up and remove
vhosts dynamically.

@section unixskt Testing Unix Socket Server support

Start the test server with -U and the path to create the unix domain socket

```
 $ libwebsockets-test-server -U /tmp/uds
```

On exit, lws will delete the socket inode.

To test the client side, eg

```
 $ nc -C -U /tmp/uds -i 30
```

and type

`GET / HTTP/1.1`

followed by two ENTER.  The contents of test.html should be returned.

@section wscl Testing websocket client support

If you run the test server as described above, you can also
connect to it using the test client as well as a browser.

```
	$ libwebsockets-test-client localhost
```

will by default connect to the test server on localhost:7681
and print the dumb increment number from the server at the
same time as drawing random circles in the mirror protocol;
if you connect to the test server using a browser at the
same time you will be able to see the circles being drawn.

The test client supports SSL too, use

```
	$ libwebsockets-test-client localhost --ssl -s
```

the -s tells it to accept the default self-signed cert from the server,
otherwise it will strictly fail the connection if there is no CA cert to
validate the server's certificate.


@section choosingts Choosing between test server variations

If you will be doing standalone serving with lws, ideally you should avoid
making your own server at all, and use lwsws with your own protocol plugins.

The second best option is follow test-server-v2.0.c, which uses a mount to
autoserve a directory, and lws protocol plugins for ws, without needing any
user callback code (other than what's needed in the protocol plugin).

For those two options libuv is needed to support the protocol plugins, if
that's not possible then the other variations with their own protocol code
should be considered.

@section tassl Testing SSL on the client side

To test SSL/WSS client action, just run the client test with
```
	$ libwebsockets-test-client localhost --ssl
```
By default the client test applet is set to accept self-signed
certificates used by the test server, this is indicated by the
`use_ssl` var being set to `2`.  Set it to `1` to reject any server
certificate that it doesn't have a trusted CA cert for.


@section taping Using the websocket ping utility

libwebsockets-test-ping connects as a client to a remote
websocket server and pings it like the
normal unix ping utility.
```
	$ libwebsockets-test-ping localhost
	handshake OK for protocol lws-mirror-protocol
	Websocket PING localhost.localdomain (127.0.0.1) 64 bytes of data.
	64 bytes from localhost: req=1 time=0.1ms
	64 bytes from localhost: req=2 time=0.1ms
	64 bytes from localhost: req=3 time=0.1ms
	64 bytes from localhost: req=4 time=0.2ms
	64 bytes from localhost: req=5 time=0.1ms
	64 bytes from localhost: req=6 time=0.2ms
	64 bytes from localhost: req=7 time=0.2ms
	64 bytes from localhost: req=8 time=0.1ms
	^C
	--- localhost.localdomain websocket ping statistics ---
	8 packets transmitted, 8 received, 0% packet loss, time 7458ms
	rtt min/avg/max = 0.110/0.185/0.218 ms
	$
```
By default it sends 64 byte payload packets using the 04
PING packet opcode type.  You can change the payload size
using the `-s=` flag, up to a maximum of 125 mandated by the
04 standard.

Using the lws-mirror protocol that is provided by the test
server, libwebsockets-test-ping can also use larger payload
sizes up to 4096 is BINARY packets; lws-mirror will copy
them back to the client and they appear as a PONG.  Use the
`-m` flag to select this operation.

The default interval between pings is 1s, you can use the -i=
flag to set this, including fractions like `-i=0.01` for 10ms
interval.

Before you can even use the PING opcode that is part of the
standard, you must complete a handshake with a specified
protocol.  By default lws-mirror-protocol is used which is
supported by the test server.  But if you are using it on
another server, you can specify the protocol to handshake with
by `--protocol=protocolname`


@section ta fraggle Fraggle test app

By default it runs in server mode
```
	$ libwebsockets-test-fraggle
	libwebsockets test fraggle
	(C) Copyright 2010-2011 Andy Green <andy@warmcat.com> licensed under MIT
	 Compiled with SSL support, not using it
	 Listening on port 7681
	server sees client connect
	accepted v06 connection
	Spamming 360 random fragments
	Spamming session over, len = 371913. sum = 0x2D3C0AE
	Spamming 895 random fragments
	Spamming session over, len = 875970. sum = 0x6A74DA1
	...
```
You need to run a second session in client mode, you have to
give the `-c` switch and the server address at least:
```
	$ libwebsockets-test-fraggle -c localhost
	libwebsockets test fraggle
	(C) Copyright 2010-2011 Andy Green <andy@warmcat.com> licensed under MIT
	 Client mode
	Connecting to localhost:7681
	denied deflate-stream extension
	handshake OK for protocol fraggle-protocol
	client connects to server
	EOM received 371913 correctly from 360 fragments
	EOM received 875970 correctly from 895 fragments
	EOM received 247140 correctly from 258 fragments
	EOM received 695451 correctly from 692 fragments
	...
```
The fraggle test sends a random number up to 1024 fragmented websocket frames
each of a random size between 1 and 2001 bytes in a single message, then sends
a checksum and starts sending a new randomly sized and fragmented message.

The fraggle test client receives the same message fragments and computes the
same checksum using websocket framing to see when the message has ended.  It
then accepts the server checksum message and compares that to its checksum.


@section taproxy proxy support

The http_proxy environment variable is respected by the client
connection code for both `ws://` and `wss://`.  It doesn't support
authentication.

You use it like this
```
	$ export http_proxy=myproxy.com:3128
	$ libwebsockets-test-client someserver.com
```

@section talog debug logging

By default logging of severity "notice", "warn" or "err" is enabled to stderr.

Again by default other logging is compiled in but disabled from printing.

By default debug logs below "notice" in severity are not compiled in.  To get
them included, add this option in CMAKE

```
	$ cmake .. -DCMAKE_BUILD_TYPE=DEBUG
```

If you want to see more detailed debug logs, you can control a bitfield to
select which logs types may print using the `lws_set_log_level()` api, in the
test apps you can use `-d <number>` to control this.  The types of logging
available are (OR together the numbers to select multiple)

 - 1   ERR
 - 2   WARN
 - 4   NOTICE
 - 8   INFO
 - 16  DEBUG
 - 32  PARSER
 - 64  HEADER
 - 128 EXTENSION
 - 256 CLIENT
 - 512 LATENCY


@section ws13 Websocket version supported

The final IETF standard is supported for both client and server, protocol
version 13.


@section latency Latency Tracking

Since libwebsockets runs using `poll()` and a single threaded approach, any
unexpected latency coming from system calls would be bad news.  There's now
a latency tracking scheme that can be built in with `-DLWS_WITH_LATENCY=1` at
cmake, logging the time taken for system calls to complete and if
the whole action did complete that time or was deferred.

You can see the detailed data by enabling logging level 512 (eg, `-d 519` on
the test server to see that and the usual logs), however even without that
the "worst" latency is kept and reported to the logs with NOTICE severity
when the context is destroyed.

Some care is needed interpreting them, if the action completed the first figure
(in us) is the time taken for the whole action, which may have retried through
the poll loop many times and will depend on network roundtrip times.  High
figures here don't indicate a problem.  The figure in us reported after "lat"
in the logging is the time taken by this particular attempt.  High figures
here may indicate a problem, or if you system is loaded with another app at
that time, such as the browser, it may simply indicate the OS gave preferential
treatment to the other app during that call.


@section autobahn Autobahn Test Suite

Lws can be tested against the autobahn websocket fuzzer in both client and
server modes

1) pip install autobahntestsuite

2) From your build dir:

```
 $ cmake .. -DLWS_WITHOUT_EXTENSIONS=0 -DLWS_WITH_MINIMAL_EXAMPLES=1 && make
```

3) ../scripts/autobahn-test.sh

4) In a browser go to the directory you ran wstest in (eg, /projects/libwebsockets)

file:///projects/libwebsockets/build/reports/clients/index.html

to see the results


@section autobahnnotes Autobahn Test Notes

1) Two of the tests make no sense for Libwebsockets to support and we fail them.

 - Tests 2.10 + 2.11: sends multiple pings on one connection.  Lws policy is to
only allow one active ping in flight on each connection, the rest are dropped.
The autobahn test itself admits this is not part of the standard, just someone's
random opinion about how they think a ws server should act.  So we will fail
this by design and it is no problem about RFC6455 compliance.

2) Currently two parts of autobahn are broken and we skip them

https://github.com/crossbario/autobahn-testsuite/issues/71
 

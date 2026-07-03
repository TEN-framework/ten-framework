# Http fallback and raw proxying

Lws has several interesting options and features that can be applied to get
some special behaviours... this article discusses them and how they work.

## Overview of normal vhost selection

Lws supports multiple http or https vhosts sharing a listening socket on the
same port.

For unencrypted http, the Host: header is used to select which vhost the
connection should bind to, by comparing what is given there against the
names the server was configured with for the various vhosts.  If no match, it
selects the first configured vhost.

For TLS, it has an extension called SNI (Server Name Indication) which tells
the server early in the TLS handshake the host name the connection is aimed at.
That allows lws to select the vhost early, and use vhost-specific TLS certs
so everything is happy.  Again, if there is no match the connection proceeds
using the first configured vhost and its certs.

## Http(s) fallback options

What happens if you try to connect, eg, an ssh client to the http server port
(this is not an idle question...)?  Obviously the http server part or the tls
part of lws will fail the connection and close it.  (We will look at that flow
in a moment in detail for both unencrypted and tls listeners.)

However if the first configured vhost for the port was created with the
vhost creation info struct `.options` flag `LWS_SERVER_OPTION_FALLBACK_TO_APPLY_LISTEN_ACCEPT_CONFIG`,
then instead of the error, the connection transitions to whatever role was
given in the vhost creation info struct `.listen_accept_role` and `.listen_accept_protocol`.

With lejp-conf / lwsws, the options can be applied to the first vhost using:

```
   "listen-accept-role": "the-role-name",
   "listen-accept-protocol": "the-protocol-name",
   "fallback-listen-accept": "1"
```

See `./minimal-examples/raw/minimal-raw-fallback-http-server` for examples of
all the options in use via commandline flags.

So long as the first packet for the protocol doesn't look like GET, POST, or
a valid tls packet if connection to an https vhost, this allows the one listen
socket to handle both http(s) and a second protocol, as we will see, like ssh.

Notice there is a restriction that no vhost selection processing is possible,
neither for tls listeners nor plain http ones... the packet belonging to a
different protocol will not send any Host: header nor tls SNI.

Therefore although the flags and settings are applied to the first configured
vhost, actually their effect is global for a given listen port.  If enabled,
all vhosts on the same listen port will do the fallback action.

### Plain http flow

![plain http flow](/doc-assets/accept-flow-1.svg)

Normally, if the first received packet does not contain a valid HTTP method,
then the connection is dropped.  Which is what you want from an http server.

However if enabled, the connection can transition to the defined secondary
role / protocol.

|Flag|lejp-conf / lwsws|Function|
|---|---|---|
|`LWS_SERVER_OPTION_FALLBACK_TO_APPLY_LISTEN_ACCEPT_CONFIG`|`"fallback-listen-accept": "1"`|Enable fallback processing|

### TLS https flow

![tls https flow](/doc-assets/accept-flow-2.svg)

If the port is listening with tls, the point that a packet from a different
protocol will fail is earlier, when the tls tunnel is being set up.

|Flag|lejp-conf / lwsws|Function|
|---|---|---|
|`LWS_SERVER_OPTION_FALLBACK_TO_APPLY_LISTEN_ACCEPT_CONFIG`|`"fallback-listen-accept": "1"`|Enable fallback processing|
|`LWS_SERVER_OPTION_REDIRECT_HTTP_TO_HTTPS`|`"redirect-http": "1"`|Treat invalid tls packet as http, issue http redirect to https://|
|`LWS_SERVER_OPTION_ALLOW_HTTP_ON_HTTPS_LISTENER`|`"allow-http-on-https": "1"`|Accept unencrypted http connections on this tls port (dangerous)|

The latter two options are higher priority than, and defeat, the first one.

### Non-http listener

![non-http flow](/doc-assets/accept-flow-3.svg)

It's also possible to skip the fallback processing and just force the first
vhost on the port to use the specified role and protocol in the first place.

|Flag|lejp-conf / lwsws|Function|
|---|---|---|
|LWS_SERVER_OPTION_ADOPT_APPLY_LISTEN_ACCEPT_CONFIG|`"apply-listen-accept": "1"`|Force vhost to use listen-accept-role / listen-accept-protocol|

## Using http(s) fallback with raw-proxy

If enabled for build with `cmake .. -DLWS_ROLE_RAW_PROXY=1 -DLWS_WITH_PLUGINS=1`
then lws includes ready-to-use support for raw tcp proxying.

This can be used standalone on the first vhost on a port, but most intriguingly
it can be specified as the fallback for http(s)...

See `./minimal-examples/raw/minimal-raw-proxy-fallback.c` for a working example.

### fallback with raw-proxy in code

On the first vhost for the port, specify the required "onward" pvo to configure
the raw-proxy protocol...you can adjust the "ipv4:127.0.0.1:22" to whatever you
want...

```
	static struct lws_protocol_vhost_options pvo1 = {
	        NULL,
	        NULL,
	        "onward",		/* pvo name */
	        "ipv4:127.0.0.1:22"	/* pvo value */
	};

	static const struct lws_protocol_vhost_options pvo = {
	        NULL,           	/* "next" pvo linked-list */
	        &pvo1,			/* "child" pvo linked-list */
	        "raw-proxy",		/* protocol name we belong to on this vhost */
	        ""              	/* ignored */
	};
```

... and set up the fallback enable and bindings...

```
	info.options |= LWS_SERVER_OPTION_FALLBACK_TO_APPLY_LISTEN_ACCEPT_CONFIG;
	info.listen_accept_role = "raw_proxy";
	info.listen_accept_proxy = "raw_proxy";
	info.pvo = &pvo;
```

### fallback with raw-proxy in JSON conf

On the first vhost for the port, enable the raw-proxy protocol on the vhost and
set the pvo config

```
                "ws-protocols": [{
                        "raw-proxy": {
                         "status": "ok",
                         "onward": "ipv4:127.0.0.1:22"
                        }
                 }],
```

Enable the fallback behaviour on the vhost and the role / protocol binding

```
	"listen-accept-role": "raw-proxy",
	"listen-accept-protocol": "raw-proxy",
	"fallback-listen-accept": "1"
```

### Testing

With this configured, the listen port will function normally for http or https
depending on how it was set up.

But if you try to connect to it with an ssh client, that will also work fine.

The libwebsockets.org server is set up in this way, you can confirm it by
visiting `https://libwebsockets.org` on port 443 as usual, but also trying
`ssh -p 443 invalid@libwebsockets.org`... you will get permission denied from
your ssh client.  With valid credentials in fact that works perfectly for
ssh, scp, git-over-ssh etc all on port 443...


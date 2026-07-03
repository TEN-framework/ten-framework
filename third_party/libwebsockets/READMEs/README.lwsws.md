Notes about lwsws
=================

@section lwsws Libwebsockets Web Server

lwsws is an implementation of a very lightweight, ws-capable generic web
server, which uses libwebsockets to implement everything underneath.

If you are basically implementing a standalone server with lws, you can avoid
reinventing the wheel and use a debugged server including lws.


@section lwswsb Build

Just enable -DLWS_WITH_LWSWS=1 at cmake-time.

It enables libuv and plugin support automatically.

NOTICE on Ubuntu, the default libuv package is called "libuv-0.10".  This is ancient.

You should replace this with libuv1 and libuv1-dev before proceeding.

@section lwswsc Lwsws Configuration

lwsws uses JSON config files, they're pure JSON except:

 - '#' may be used to turn the rest of the line into a comment.

 - There's also a single substitution, if a string contains "_lws_ddir_", then that is
replaced with the LWS install data directory path, eg, "/usr/share" or whatever was
set when LWS was built + installed.  That lets you refer to installed paths without
having to change the config if your install path was different.

There is a single file intended for global settings

/etc/lwsws/conf
```
	# these are the server global settings
	# stuff related to vhosts should go in one
	# file per vhost in ../conf.d/

	{
	  "global": {
	   "username": "apache",
	   "groupname": "apache",
	   "count-threads": "1",
	   "server-string": "myserver v1", # returned in http headers
	   "ws-pingpong-secs": "200", # confirm idle established ws connections this often
	   "init-ssl": "yes"
	 }
	}
```
and a config directory intended to take one file per vhost

/etc/lwsws/conf.d/warmcat.com
```
	{
		"vhosts": [{
			"name": "warmcat.com",
			"port": "443",
			"interface": "eth0",  # optional
			"host-ssl-key": "/etc/pki/tls/private/warmcat.com.key",  # if given enable ssl
			"host-ssl-cert": "/etc/pki/tls/certs/warmcat.com.crt",
			"host-ssl-ca": "/etc/pki/tls/certs/warmcat.com.cer",
			"mounts": [{  # autoserve
				"mountpoint": "/",
				"origin": "file:///var/www/warmcat.com",
				"default": "index.html"
			}]
		}]
	}
```
To get started quickly, an example config reproducing the old test server
on port 7681, non-SSL is provided.  To set it up
```
	# mkdir -p /etc/lwsws/conf.d /var/log/lwsws
	# cp ./lwsws/etc-lwsws-conf-EXAMPLE /etc/lwsws/conf
	# cp ./lwsws/etc-lwsws-conf.d-localhost-EXAMPLE /etc/lwsws/conf.d/test-server
	# sudo lwsws
```

@section lwswsacme Using Letsencrypt or other ACME providers

Lws supports automatic provisioning and renewal of TLS certificates.

See ./READMEs/README.plugin-acme.md for examples of how to set it up on an lwsws vhost.

@section lwsogo Other Global Options

 - `reject-service-keywords` allows you to return an HTTP error code and message of your choice
if a keyword is found in the user agent

```
   "reject-service-keywords": [{
        "scumbot": "404 Not Found"
   }]
```

 - `timeout-secs` lets you set the global timeout for various network-related
 operations in lws, in seconds.  It defaults to 5.
 
@section lwswsv Lwsws Vhosts

One server can run many vhosts, where SSL is in use SNI is used to match
the connection to a vhost and its vhost-specific SSL keys during SSL
negotiation.

Listing multiple vhosts looks something like this
```
	{
	 "vhosts": [ {
	     "name": "localhost",
	     "port": "443",
	     "host-ssl-key":  "/etc/pki/tls/private/libwebsockets.org.key",
	     "host-ssl-cert": "/etc/pki/tls/certs/libwebsockets.org.crt",
	     "host-ssl-ca":   "/etc/pki/tls/certs/libwebsockets.org.cer",
	     "mounts": [{
	       "mountpoint": "/",
	       "origin": "file:///var/www/libwebsockets.org",
	       "default": "index.html"
	       }, {
	        "mountpoint": "/testserver",
	        "origin": "file:///usr/local/share/libwebsockets-test-server",
	        "default": "test.html"
	       }],
	     # which protocols are enabled for this vhost, and optional
	     # vhost-specific config options for the protocol
	     #
	     "ws-protocols": [{
	       "warmcat,timezoom": {
	         "status": "ok"
	       }
	     }]
	    },
	    {
	    "name": "localhost",
	    "port": "7681",
	     "host-ssl-key":  "/etc/pki/tls/private/libwebsockets.org.key",
	     "host-ssl-cert": "/etc/pki/tls/certs/libwebsockets.org.crt",
	     "host-ssl-ca":   "/etc/pki/tls/certs/libwebsockets.org.cer",
	     "mounts": [{
	       "mountpoint": "/",
	       "origin": ">https://localhost"
	     }]
	   },
	    {
	    "name": "localhost",
	    "port": "80",
	     "mounts": [{
	       "mountpoint": "/",
	       "origin": ">https://localhost"
	     }]
	   }
	
	  ]
	}
```

That sets up three vhosts all called "localhost" on ports 443 and 7681 with SSL, and port 80 without SSL but with a forced redirect to https://localhost


@section lwswsvn Lwsws Vhost name and port sharing

The vhost name field is used to match on incoming SNI or Host: header, so it
must always be the host name used to reach the vhost externally.

 - Vhosts may have the same name and different ports, these will each create a
listening socket on the appropriate port.

 - Vhosts may also have the same port and different name: these will be treated as
true vhosts on one listening socket and the active vhost decided at SSL
negotiation time (via SNI) or if no SSL, then after the Host: header from
the client has been parsed.


@section lwswspr Lwsws Protocols

Vhosts by default have available the union of any initial protocols from context creation time, and
any protocols exposed by plugins.

Vhosts can select which plugins they want to offer and give them per-vhost settings using this syntax
```
	     "ws-protocols": [{
	       "warmcat-timezoom": {
	         "status": "ok"
	       }
	     }]
```

The "x":"y" parameters like "status":"ok" are made available to the protocol during its per-vhost
LWS_CALLBACK_PROTOCOL_INIT (in is a pointer to a linked list of struct lws_protocol_vhost_options
containing the name and value pointers).

To indicate that a protocol should be used when no Protocol: header is sent
by the client, you can use "default": "1"
```
	     "ws-protocols": [{
	       "warmcat-timezoom": {
	         "status": "ok",
	         "default": "1"
	       }
	     }]
```

Similarly, if your vhost is serving a raw protocol, you can mark the protocol
to be selected using "raw": "1"
```
	     "ws-protocols": [{
	       "warmcat-timezoom": {
	         "status": "ok",
	         "raw": "1"
	       }
	     }]
```

See also "apply-listen-accept" below.

@section lwswsovo Lwsws Other vhost options

 - If the three options `host-ssl-cert`, `host-ssl-ca` and `host-ssl-key` are given, then the vhost supports SSL.

 Each vhost may have its own certs, SNI is used during the initial connection negotiation to figure out which certs to use by the server name it's asking for from the request DNS name.

 - `keeplive-timeout` (in secs) defaults to 60 for lwsws, it may be set as a vhost option

 - `interface` lets you specify which network interface to listen on, if not given listens on all.  If the network interface is not usable (eg, ethernet cable out) it will be logged at startup with such vhost not listening, and lws will poll for it and bind a listen socket to the interface if and when it becomes available.

 - "`unix-socket`": "1" causes the unix socket specified in the interface option to be used instead of an INET socket

 - "`unix-socket-perms`": "user:group" allows you to control the unix permissons on the listening unix socket.  It's always get to `0600` mode, but you can control the user and group for the socket fd at creation time.  This allows you to use unix user and groups to control who may open the other end of the unix socket on the local system.

 - "`sts`": "1" causes lwsws to send a Strict Transport Security header with responses that informs the client he should never accept to connect to this address using http.  This is needed to get the A+ security rating from SSL Labs for your server.

 - "`access-log`": "filepath"   sets where apache-compatible access logs will be written

 - `"enable-client-ssl"`: `"1"` enables the vhost's client SSL context, you will need this if you plan to create client conections on the vhost that will use SSL.  You don't need it if you only want http / ws client connections.

 - "`ciphers`": "<cipher list>"  OPENSSL only: sets the allowed list of TLS <= 1.2 ciphers and key exchange protocols for the serving SSL_CTX on the vhost.  The default list is restricted to only those providing PFS (Perfect Forward Secrecy) on the author's Fedora system.
 
 If you need to allow weaker ciphers, you can provide an alternative list here per-vhost.

 - "`client-ssl-ciphers`": "<cipher list>"  OPENSSL only: sets the allowed list of <= TLS1.2 ciphers and key exchange protocols for the client SSL_CTX on the vhost

 - "`tls13-ciphers`": "<cipher list>"  OPENSSL 1.1.1+ only: sets allowed list of TLS1.3+ ciphers and key exchange protocols for the client SSL_CTX on the vhost.  The default is to allow all.

 - "`client-tls13-ciphers`": "<cipher list>"  OPENSSL 1.1.1+ only: sets the allowed list of TLS1.3+ ciphers and key exchange protocols for the client SSL_CTX on the vhost.  The default is to allow all.
 
 - "`ecdh-curve`": "<curve name>"   The default ecdh curve is "prime256v1", but you can override it here, per-vhost

 - "`noipv6`": "on"  Disable ipv6 completely for this vhost

 - "`ipv6only`": "on"  Only allow ipv6 on this vhost / "off" only allow ipv4 on this vhost

 - "`ssl-option-set`": "<decimal>"  Sets the SSL option flag value for the vhost.
 It may be used multiple times and OR's the flags together.
 
 The values are derived from /usr/include/openssl/ssl.h
```
	 # define SSL_OP_NO_TLSv1_1                               0x10000000L
```
 
 would equate to
 
```
	 "`ssl-option-set`": "268435456"
 ```
 - "`ssl-option-clear'": "<decimal>"   Clears the SSL option flag value for the vhost.
 It may be used multiple times and OR's the flags together.

 - "`ssl-client-option-set`" and "`ssl-client-option-clear`" work the same way for the vhost Client SSL context

 - "`headers':: [{ "header1": "h1value", "header2": "h2value" }] 

allows you to set arbitrary headers on every file served by the vhost

recommended vhost headers for good client security are

```
                   "headers": [{
                        "Content-Security-Policy": "script-src 'self'",
                        "X-Content-Type-Options": "nosniff",
                        "X-XSS-Protection": "1; mode=block",
                        "X-Frame-Options": "SAMEORIGIN"
                 }]

```

 - "`apply-listen-accept`": "on"  This vhost only serves a non-http protocol, specified in "listen-accept-role" and "listen-accept-protocol"

@section lwswsm Lwsws Mounts

Where mounts are given in the vhost definition, then directory contents may
be auto-served if it matches the mountpoint.

Mount protocols are used to control what kind of translation happens

 - file://  serve the uri using the remainder of the url past the mountpoint based on the origin directory.

 Eg, with this mountpoint
```
	       {
	        "mountpoint": "/",
	        "origin": "file:///var/www/mysite.com",
	        "default": "/"
	       }
```
 The uri /file.jpg would serve /var/www/mysite.com/file.jpg, since / matched.

 - ^http:// or ^https://  these cause any url matching the mountpoint to issue a redirect to the origin url

 - cgi://   this causes any matching url to be given to the named cgi, eg
```
	       {
	        "mountpoint": "/git",
	        "origin": "cgi:///var/www/cgi-bin/cgit",
	        "default": "/"
	       }, {
	        "mountpoint": "/cgit-data",
	        "origin": "file:///usr/share/cgit",
	        "default": "/"
	       },
```
 would cause the url /git/myrepo to pass "myrepo" to the cgi /var/www/cgi-bin/cgit and send the results to the client.

 - http:// or https://  these perform reverse proxying, serving the remote origin content from the mountpoint.  Eg

```
		{
		 "mountpoint": "/proxytest",
		 "origin": "https://libwebsockets.org"
		}
```

This will cause your local url `/proxytest` to serve content fetched from libwebsockets.org over ssl; whether it's served from your server using ssl is unrelated and depends how you configured your local server.  Notice if you will use the proxying feature, `LWS_WITH_HTTP_PROXY` is required to be enabled at cmake, and for `https` proxy origins, your lwsws configuration must include `"init-ssl": "1"` and the vhost with the proxy mount must have `"enable-client-ssl": "1"`, even if you are not using ssl to serve.

`/proxytest/abc`, or `/proxytest/abc?def=ghi` etc map to the origin + the part past `/proxytest`, so links and img src urls etc work as do all urls under the origin path.

In addition link and src urls in the document are rewritten so / or the origin url part are rewritten to the mountpoint part.


@section lwswsomo Lwsws Other mount options

1) Some protocols may want "per-mount options" in name:value format.  You can
provide them using "pmo"

	       {
	        "mountpoint": "/stuff",
	        "origin": "callback://myprotocol",
	        "pmo": [{
	                "myname": "myvalue"
	        }]
	       }

2) When using a cgi:// protocol origin at a mountpoint, you may also give cgi environment variables specific to the mountpoint like this
```
	       {
	        "mountpoint": "/git",
	        "origin": "cgi:///var/www/cgi-bin/cgit",
	        "default": "/",
	        "cgi-env": [{
	                "CGIT_CONFIG": "/etc/cgitrc/libwebsockets.org"
	        }]
	       }
```
 This allows you to customize one cgi depending on the mountpoint (and / or vhost).

3) It's also possible to set the cgi timeout (in secs) per cgi:// mount, like this
```
	"cgi-timeout": "30"
```
4) `callback://` protocol may be used when defining a mount to associate a
named protocol callback with the URL namespace area.  For example
```
	       {
	        "mountpoint": "/formtest",
	        "origin": "callback://protocol-post-demo"
	       }
```
All handling of client access to /formtest[anything] will be passed to the
callback registered to the protocol "protocol-post-demo".

This is useful for handling POST http body content or general non-cgi http
payload generation inside a plugin.

See the related notes in README.coding.md

5) Cache policy of the files in the mount can also be set.  If no
options are given, the content is marked uncacheable.
```
	       {
	        "mountpoint": "/",
	        "origin": "file:///var/www/mysite.com",
	        "cache-max-age": "60",      # seconds
	        "cache-reuse": "1",         # allow reuse at client at all
	        "cache-revalidate": "1",    # check it with server each time
	        "cache-intermediaries": "1" # allow intermediary caches to hold
	       }
```

6) You can also define a list of additional mimetypes per-mount
```
	        "extra-mimetypes": {
	                 ".zip": "application/zip",
	                 ".doc": "text/evil"
	         }
```

Normally a file suffix MUST match one of the canned mimetypes or one of the extra
mimetypes, or the file is not served.  This adds a little bit of security because
even if there is a bug somewhere and the mount dirs are circumvented, lws will not
serve, eg, /etc/passwd.

If you provide an extra mimetype entry

			"*": ""

Then any file is served, if the mimetype was not known then it is served without a
Content-Type: header.

7) A mount can be protected by HTTP Basic Auth.  This only makes sense when using
https, since otherwise the password can be sniffed.

You can add a `basic-auth` entry on an http mount like this

```
{
        "mountpoint": "/basic-auth",
        "origin": "file://_lws_ddir_/libwebsockets-test-server/private",
        "basic-auth": "/var/www/balogins-private"
}
```

Before serving anything, lws will signal to the browser that a username / password
combination is required, and it will pop up a dialog.  When the user has filled it
in, lwsws checks the user:password string against the text file named in the `basic-auth`
entry.

The file should contain user:pass one per line

```
testuser:testpass
myuser:hispass
```

The file should be readable by lwsws, and for a little bit of extra security not
have a file suffix, so lws would reject to serve it even if it could find it on
a mount.

After successful authentication, `WSI_TOKEN_HTTP_AUTHORIZATION` contains the
authenticated username.

In the case you want to also protect being able to connect to a ws protocol on
a particular vhost by requiring the http part can authenticate using Basic
Auth before the ws upgrade, this is also possible.  In this case, the
"basic-auth": and filepath to the credentials file is passed as a pvo in the
"ws-protocols" section of the vhost definition.

@section lwswscc Requiring a Client Cert on a vhost

You can make a vhost insist to get a client certificate from the peer before
allowing the connection with

```
	"client-cert-required": "1"
```

the connection will only proceed if the client certificate was signed by the
same CA as the server has been told to trust.

@section rawconf Configuring Fallback and Raw vhosts

Lws supports some unusual modes for vhost listen sockets, which may be
configured entirely using the JSON per-vhost config language in the related
vhost configuration section.

There are three main uses for them

1) A vhost bound to a specific role and protocol, not http.  This binds all
incoming connections on the vhost listen socket to the "raw-proxy" role and
protocol "myprotocol".

```
	"listen-accept-role":		"raw-proxy",
	"listen-accept-protocol":	"myprotocol",
	"apply-listen-accept":		"1"
```

2) A vhost that wants to treat noncompliant connections for http or https as
   belonging to a secondary fallback role and protocol.  This causes non-https
   connections to an https listener to stop being treated as https, to lose the
   tls wrapper, and bind to role "raw-proxy" and protocol "myprotocol".  For
   example, connect a browser on your external IP :443 as usual and it serves
   as normal, but if you have configured the raw-proxy to portforward
   127.0.0.1:22, then connecting your ssh client to your external port 443 will
   instead proxy your sshd over :443 with no http or tls getting in the way.

```
	"listen-accept-role":		"raw-proxy",
	"listen-accept-protocol":	"myprotocol",
	"fallback-listen-accept":	"1",
	"allow-non-tls":		"1"
```

3) A vhost wants to either redirect stray http traffic back to https, or to
   actually serve http on an https listen socket (this is not recommended
   since it allows anyone to drop the security assurances of https by
   accident or design).

```
	"allow-non-tls":		"1",
	"redirect-http":		"1",
```

...or,

```
	"allow-non-tls":		"1",
	"allow-http-on-https":		"1",
```

@section lwswspl Lwsws Plugins

Protcols and extensions may also be provided from "plugins", these are
lightweight dynamic libraries.  They are scanned for at init time, and
any protocols and extensions found are added to the list given at context
creation time.

Protocols receive init (LWS_CALLBACK_PROTOCOL_INIT) and destruction
(LWS_CALLBACK_PROTOCOL_DESTROY) callbacks per-vhost, and there are arrangements
they can make per-vhost allocations and get hold of the correct pointer from
the wsi at the callback.

This allows a protocol to choose to strictly segregate data on a per-vhost
basis, and also allows the plugin to handle its own initialization and
context storage.

To help that happen conveniently, there are some new apis

 - lws_vhost_get(wsi)
 - lws_protocol_get(wsi)
 - lws_callback_on_writable_all_protocol_vhost(vhost, protocol)
 - lws_protocol_vh_priv_zalloc(vhost, protocol, size)
 - lws_protocol_vh_priv_get(vhost, protocol)
 
dumb increment, mirror and status protocol plugins are provided as examples.


@section lwswsplaplp Additional plugin search paths

Packages that have their own lws plugins can install them in their own
preferred dir and ask lwsws to scan there by using a config fragment
like this, in its own conf.d/ file managed by the other package
```
	{
	  "global": {
	   "plugin-dir": "/usr/local/share/coherent-timeline/plugins"
	  }
	}
```

@section lwswsssp lws-server-status plugin

One provided protocol can be used to monitor the server status.

Enable the protocol like this on a vhost's ws-protocols section
```
	       "lws-server-status": {
	         "status": "ok",
	         "update-ms": "5000"
	       }
```
`"update-ms"` is used to control how often updated JSON is sent on a ws link.

And map the provided HTML into the vhost in the mounts section
```
	       {
	        "mountpoint": "/server-status",
	        "origin": "file:///usr/local/share/libwebsockets-test-server/server-status",
	        "default": "server-status.html"
	       }
```
You might choose to put it on its own vhost which has "interface": "lo", so it's not
externally visible, or use the Basic Auth support to require authentication to
access it.

`"hide-vhosts": "{0 | 1}"` lets you control if information about your vhosts is included.
Since this includes mounts, you might not want to leak that information, mount names,
etc.

`"filespath":"{path}"` lets you give a server filepath which is read and sent to the browser
on each refresh.  For example, you can provide server temperature information on most
Linux systems by giving an appropriate path down /sys.

This may be given multiple times.


@section lwswsreload Lwsws Configuration Reload

You may send lwsws a `HUP` signal, by, eg

```
$ sudo killall -HUP lwsws
```

This causes lwsws to "deprecate" the existing lwsws process, and remove and close all of
its listen sockets, but otherwise allowing it to continue to run, until all
of its open connections close.

When a deprecated lwsws process has no open connections left, it is destroyed
automatically.

After sending the SIGHUP to the main lwsws process, a new lwsws process, which can
pick up the newly-available listen sockets, and use the current configuration
files, is automatically started.

The new configuration may differ from the original one in arbitrary ways, the new
context is created from scratch each time without reference to the original one.

Notes

1) Protocols that provide a "shared world" like mirror will have as many "worlds"
as there are lwsws processes still active.  People connected to a deprecated lwsws
process remain connected to the existing peers.

But any new connections will apply to the new lwsws process, which does not share
per-vhost "shared world" data with the deprecated process.  That means no new
connections on the deprecated context, ie a "shrinking world" for those guys, and a
"growing world" for people who connect after the SIGHUP.

2) The new lwsws process owes nothing to the previous one.  It starts with fresh
plugins, fresh configuration, fresh root privileges if that how you start it.

The plugins may have been updated in arbitrary ways including struct size changes
etc, and lwsws or lws may also have been updated arbitrarily.

3) A root parent process is left up that is not able to do anything except
respond to SIGHUP or SIGTERM.  Actual serving and network listening etc happens
in child processes which use the privileges set in the lwsws config files.

@section lwswssysd Lwsws Integration with Systemd

lwsws needs a service file like this as `/usr/lib/systemd/system/lwsws.service`
```
[Unit]
Description=Libwebsockets Web Server
After=syslog.target

[Service]
ExecStart=/usr/local/bin/lwsws 
ExecReload=/usr/bin/killall -s SIGHUP lwsws ; sleep 1 ; /usr/local/bin/lwsws
StandardError=null

[Install]
WantedBy=multi-user.target
```

You can find this prepared in `./lwsws/usr-lib-systemd-system-lwsws.service`


@section lwswslr Lwsws Integration with logrotate

For correct operation with logrotate, `/etc/logrotate.d/lwsws` (if that's
where we're putting the logs) should contain
```
	/var/log/lwsws/*log {
	    copytruncate
	    missingok
	    notifempty
	    delaycompress
	}
```
You can find this prepared in `/lwsws/etc-logrotate.d-lwsws`

Prepare the log directory like this

```
	sudo mkdir /var/log/lwsws
	sudo chmod 700 /var/log/lwsws
```

@section lwswsgdb Debugging lwsws with gdb

Hopefully you won't need to debug lwsws itself, but you may want to debug your plugins.  start lwsws like this to have everything running under gdb

```
sudo gdb -ex "set follow-fork-mode child" -ex "run" --args /usr/local/bin/lwsws

```

this will give nice backtraces in lwsws itself and in plugins, if they were built with symbols.

@section lwswsvgd Running lwsws under valgrind

You can just run lwsws under valgrind as usual and get valid results.  However the results / analysis part of valgrind runs
after the plugins have removed themselves, this means valgrind backtraces into plugin code is opaque, without
source-level info because the dynamic library is gone.

There's a simple workaround, use LD_PRELOAD=<plugin.so> before running lwsws, this has the loader bring the plugin
in before executing lwsws as if it was a direct dependency.  That means it's still mapped until the whole process
exits after valgtind has done its thing.



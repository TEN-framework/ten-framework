|name|demonstrates|
---|---
client-server|Minimal examples providing client and server connections simultaneously
crypto|Minimal examples related to using lws crypto apis
dbus-server|Minimal examples showing how to integrate DBUS into lws event loop
http-client|Minimal examples providing an http client
http-server|Minimal examples providing an http server
raw|Minimal examples related to adopting raw file or socket descriptors into the event loop
secure-streams|Minimal examples related to the Secure Streams client api
ws-client|Minimal examples providing a ws client
ws-server|Minimal examples providing a ws server (and an http server)

## FAQ

### Getting started

Build and install lws itself first (note that after installing lws on \*nix, you need to run `ldconfig` one time so the OS can learn about the new library.  Lws installs in `/usr/local` by default, Debian / Ubuntu ldconfig knows to look there already, but Fedora / CentOS need you to add the line `/usr/local/lib` to `/etc/ld.so.conf` and run ldconfig)

Then start with the simplest:

`http-server/minimal-http-server`

### Why are most of the sources split into a main C file file and a protocol file?

Lws supports three ways to implement the protocol callback code:

 - you can just add it all in the same source file

 - you can separate it as these examples do, and #include it
   into the main sources

 - you can build it as a standalone plugin that is discovered
   and loaded at runtime.

The way these examples are structured, you can easily also build
the protocol callback as a plugin just with a different
CMakeLists.txt... see https://github.com/warmcat/libwebsockets/tree/master/plugin-standalone
for an example.

### Why would we want the protocol as a plugin?

You will notice a lot of the main C code is the same boilerplate
repeated for each example.  The actual interesting part is in
the protocol callback only.

Lws provides (-DLWS_WITH_LWSWS=1) a generic lightweight server app called 'lwsws' that
can be configured by JSON.  Combined with your protocol as a plugin,
it means you don't actually have to make a special server "app"
part, you can just use lwsws and pass per-vhost configuration
from JSON into your protocol.  (Of course in some cases you have
an existing app you are bolting lws on to, then you don't care
about this for that particular case).

Because lwsws has no dependency on whatever your plugin does, it
can mix and match different protocols randomly without needing any code
changes.  It reduces the size of the task to just writing the
code you care about in your protocol handler, and nothing else to write
or maintain.

Lwsws supports advanced features like reload, where it starts a new server
instance with changed config or different plugins, while keeping the old
instance around until the last connection to it closes.

### I get why there is a pss, but why is there a vhd?

The pss is instantiated per-connection.  But there are almost always
other variables that have a lifetime longer than a single connection.

You could make these variables "filescope" one-time globals, but that
means your protocol cannot instantiate multiple times.

Lws supports vhosts (virtual hosts), for example both https://warmcat.com
and https://libwebsockets are running on the same lwsws instance on the
same server and same IP... each of these is a separate vhost.

Your protocol may be enabled on multiple vhosts, each of these vhosts
provides a different vhd specific to the protocol instance on that
vhost.  For example many of the samples keep a linked-list head to
a list of live pss in the vhd... that means it's cleanly a list of
pss opened **on that vhost**.  If another vhost has the protocol
enabled, connections to that will point to a different vhd, and the
linked-list head on that vhd will only list connections to his vhost.

The example "ws-server/minimal-ws-server-threads" demonstrates how to deliver
external configuration data to a specific vhost + protocol
combination using code.  In lwsws, this is simply a matter of setting
the desired JSON config.



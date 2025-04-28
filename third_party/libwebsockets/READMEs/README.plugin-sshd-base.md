ssh-base Plugin
================

## Introduction

lws-ssh-base is a protcol plugin for libwebsockets that implements a
generic, abstract, ssh server.

 - very small footprint in code and memory, takes up small part of ESP32
 
 - written with security in mind: valgrind and Coverity -clean
 
 - binds to one or more vhosts, that controls listen port(s)
 
 - all IO and settings abstracted through a single "ops" struct from user code
 
 - each instance on a vhost has its own "ops" struct, defining server keys,
   auth method and functions to implement IO and other operations

 - The plugin has no built-in behaviours like check ~/.ssh/authorized_keys,
   treat auth usernames as system usernames, or spawn the user's shell.
   Everything potentially dangerous is left to the user ops code to decide
   how to handle.  It's NOT like sshd where running it implies it will accept
   existing keys for any system user, will spawn a shell, etc, unless you
   implement those parts in the ops callbacks.
   
 - The plugin requires extra code around it in the form of the ops struct
   handlers.  So it's role is something like an abstract base class for an ssh
   server.  All the crypto, protocol sequencing and state machine are inside,
   but all the IO except the network connection is outside.
   
 - Built as part of libwebsockets, like all plugins may be dynamically loaded
   at runtime or built statically.  Test app `libwebsockets-test-sshd` provided
   
 - Uses hash and RSA functions from either mbedTLS or OpenSSL automatically,
   according to which library libwebsockets was built for

To maintain its small size, it implements a single "best of breed" crypto for
the following functions:

|Function|Crypto|
|---|---|
|KEX|curve25519-sha256@libssh.org|
|Server host key|ssh-rsa (4096b)|
|Encryption|chacha20-poly1305@openssh.com|
|Compression|None|

## License

lws-ssh-base is Free Software, available under libwebsockets' MIT license.

The crypto parts are available elsewhere under a BSD license.  But for
simplicity the whole plugin is under MIT.

## Generating your own keys

```
 $ ssh-keygen -t rsa -b 4096 -f mykeys
```

will ask for a passphrase and generate the private key in `mykeys` and the
public key in `mykeys.pub`.  If you already have a suitable RSA key you use
with ssh, you can just use that directly.

lws installs a test keypair in /usr[/local]/share/libwebsockets-test-server
that the test apps will accept.

## Example code

1) There's a working example app `libwebsockets-test-sshd` included that
spawns a bash shell when an ssh client authenticates.  The username used on
the remote ssh has no meaning, it spawns the shell under the credentials of
"lws-test-sshd" was run under.  It accepts the lws ssh test key which is
installed into /usr[/local]/share/libwebsockets-test-server.

Start the server like this (it wants root only because the server key is stored
in /etc)

```
 $ sudo libwebsockets-test-sshd
```

Connect to it using the test private key like this

```
 $ ssh -p 2200 -i /usr/local/share/libwebsockets-test-server/lws-ssh-test-keys anyuser@127.0.0.1
```

2) There's also a working example plugin `lws-sshd-demo` that "subclasses" the
abstract `lws-ssh-base` plugin to make a protocol which can be used from,
eg, lwsws.  For an lwsws vhost that listens on port 2222 and responds with
the lws-sshd-demo ssh server, the related config is:

```
        {
                "name": "sshd",
                "port": "2222",
                "onlyraw": "1",
                "ws-protocols": [{
                        "lws-ssh-base": {
                                "status": "ok",
                                "ops-from": "lws-sshd-demo"
                        },
                        "lws-sshd-demo": {
                                "status": "ok",
                                "raw": "1"
                        }
                }]
        }
```



## Integration to other apps

### Step 0: Build and install libwebsockets

For the `libwebsockets-test-sshd` example, you will need CMake options
`LWS_WITH_CGI`, since it uses lws helpers to spawn a shell.

lws-ssh-base itself doesn't require CGI support in libwebsockets.

### Step 1: make the code available in your app

Include `lws-plugin-ssh-base` in your app, either as a runtime plugin or by using
the lws static include scheme.

To bring in the whole of the ssh-base plugin
into your app in one step, statically, just include
`plugins/ssh-base/include/lws-plugin-sshd-static-build-includes.h`, you can see
an example of this in `./test-apps/test-sshd.c`.

### Step 2: define your `struct lws_ssh_ops`

`plugins/ssh-base/include/lws-plugin-ssh.h` defines
`struct lws_ssh_ops` which is used for all customization and integration
of the plugin per vhost.  Eg,

```
static const struct lws_ssh_ops ssh_ops = {
	.channel_create			= ssh_ops_channel_create,
	.channel_destroy		= ssh_ops_channel_destroy,
	.tx_waiting			= ssh_ops_tx_waiting,
	.tx				= ssh_ops_tx,
	.rx				= ssh_ops_rx,
	.get_server_key			= ssh_ops_get_server_key,
	.set_server_key			= ssh_ops_set_server_key,
	.set_env			= ssh_ops_set_env,
	.pty_req			= ssh_ops_pty_req,
	.child_process_io		= ssh_ops_child_process_io,
	.child_process_terminated	= ssh_ops_child_process_terminated,
	.exec				= ssh_ops_exec,
	.shell				= ssh_ops_shell,
	.is_pubkey_authorized		= ssh_ops_is_pubkey_authorized,
	.banner				= ssh_ops_banner,
	.disconnect_reason		= ssh_ops_disconnect_reason,
	.server_string			= "SSH-2.0-Libwebsockets",
	.api_version			= 1,
};
```
The `ssh_ops_...()` functions are your implementations for the operations
needed by the plugin for your purposes.

### Step 3: enable `lws-ssh-base` protocol to a vhost and configure using pvo

A pointer to your struct lws_ssh_ops is passed into the vhost instance of the
protocol using per-vhost options

```
static const struct lws_protocol_vhost_options pvo_ssh_ops = {
	NULL,
	NULL,
	"ops",
	(void *)&ssh_ops
};

static const struct lws_protocol_vhost_options pvo_ssh = {
	NULL,
	&pvo_ssh_ops,
	"lws-sshd-base",
	"" /* ignored, just matches the protocol name above */
};

...
	info.port = 22;
	info.options = LWS_SERVER_OPTION_ONLY_RAW;
	info.vhost_name = "sshd";
	info.protocols = protocols_sshd;
	info.pvo = &pvo_ssh;

	vh_sshd = lws_create_vhost(context, &info);
```

There are two possible pvos supported, "ops", shown above, directly passes the
ops structure in using the value on the "ops" pvo.

To support other protocols that want to provide ops to lws-ssh-base themselves
for a particular vhost, you can also provide a pvo `"ops-from"` whose value is
the name of the protocol also enabled on this vhost, whose protocol ".user"
pointer points to the ops struct lws-ssh-base should use.

## Integration to other plugins

A worked example of using the abstract `lws-ssh-base` plugin from another
plugin that provides the ops struct is in `./plugins/protocol_lws_sshd_demo`.

The key points to note

 - the plugin sets the ops struct for the vhost instantiation of `lws-ssh-base`
 by passing a pointer to the ops struct in its `lws_protocols` struct `user`
 member.
 
 - the config for the vhost tells `lws-ssh-base` to pick up the ops struct
 pointer using an "ops-from" pvo that indicates the protocol name.
 
```
 			"lws-ssh-base": {
                                "status": "ok",
                                "ops-from": "lws-sshd-demo"
                        },
```

 - the config for the vhost tells lws this vhost only serves RAW (ie, no http)
 
```
         {
                "name": "sshd",
                "port": "2222",
                "onlyraw": "1",
                ...
```

 - the config for the vhost marks the protocol that uses `lws-ssh-base`, not
 `lws-ssh-base` itself, as the protocol to be served for raw connections

```
                        "lws-sshd-demo": {
                                "status": "ok",
                                "raw": "1"
                         ...
```

## Notes

You can have the vhost it binds to listen on a nonstandard port.  The ssh
commandline app cane be told to connect to a non-22 port with
`ssh -p portnum user@hostname`



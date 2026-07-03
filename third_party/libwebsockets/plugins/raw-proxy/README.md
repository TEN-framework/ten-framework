# raw-proxy plugin

## Enabling for build

```
$ cmake .. -DLWS_ROLE_RAW_PROXY=1
```

## configuration pvo

|pvo|value meaning|
|---|---|
|onward|The onward proxy destination, in the form `ipv4:addr[:port]`|

## Note for vhost selection

Notice that since it proxies the packets "raw", there's no SNI or Host:
header to resolve amongst multiple vhosts on the same listen port.  So the
vhost you associate with this protocol must be alone on its own port.

It's also possible to apply this or other role + protocols as a fallback after
http[s] processing rejected the first packet from an incoming connection.
See `./READMEs/README-http-fallback.md`

## Note for packet size

For throughput, since often one side is localhost that can handle larger
packets easily, you should create the context used with this plugin with

```
	info.pt_serv_buf_size = 8192;
```

lwsws already does this.

## Using with C

See the minimal example `./minimal-example/raw/minimal-raw-proxy` for
a working example of a vhost that accepts connections and then
proxies them using this plugin.  The example is almost all boilerplate
for setting up the context and the pvo.

## Using with lwsws

For a usage where the plugin "owns" the whole vhost, you should enable the
plugin protocol on the vhost as usual, and specify the "onward" pvo with:

```
                "ws-protocols": [{
                        "raw-proxy": {
                         "status": "ok",
                         "onward": "ipv4:remote.address.com:port"
                        }
                 }],
```

and then define the vhost with:

```
    "apply-listen-accept": "1",
    "listen-accept-role": "raw-proxy",
    "listen-accept-protocol": "raw-proxy"
```

which tells it to apply the role and protocol as soon as a connection is
accepted on the vhost.

# lws minimal http server deaddrop

This demonstrates how you can leverage the lws deaddrop plugin to make a
secure, modern html5 file upload and sharing application.

The demo is protected by basic auth credentials defined in the file at
./ba-passwords - by default the credentials are user: user1, password: password;
and user: user2, password: password again.

You can upload files and have them appear on a shared, downloadable list that
is dynamically updated to all clients open on the page.  Only the authenticated
uploader is able to delete the files he uploaded.

Multiple simultaneous ongoing file uploads are supported.

## build

To build this standalone, you must tell cmake where the lws source tree
./plugins directory can be found, since it relies on including the source
of the raw-proxy plugin.

```
 $ cmake . -DLWS_PLUGINS_DIR=~/libwebsockets/plugins && make
```

## usage

```
 $ ./lws-minimal-http-server-deaddrop
[2018/12/01 10:31:09:7108] USER: LWS minimal http server deaddrop | visit https://localhost:7681
[2018/12/01 10:31:09:8511] NOTICE: Creating Vhost 'default' port 7681, 1 protocols, IPv6 off
[2018/12/01 10:31:09:8522] NOTICE:  Using SSL mode
[2018/12/01 10:31:10:0755] NOTICE:  SSL ECDH curve 'prime256v1'
[2018/12/01 10:31:10:2562] NOTICE: lws_tls_client_create_vhost_context: doing cert filepath localhost-100y.cert
[2018/12/01 10:31:10:2581] NOTICE: Loaded client cert localhost-100y.cert
[2018/12/01 10:31:10:2583] NOTICE: lws_tls_client_create_vhost_context: doing private key filepath
[2018/12/01 10:31:10:2593] NOTICE: Loaded client cert private key localhost-100y.key
[2018/12/01 10:31:10:2596] NOTICE: created client ssl context for default
[2018/12/01 10:31:10:5290] NOTICE:   deaddrop: vh default, upload dir ./uploads, max size 10000000
[2018/12/01 10:31:10:5376] NOTICE:    vhost default: cert expiry: 730203d
...
```

Visit https://localhost:7681, and follow the link there to the secret area.

Give your browser "user1" and "password" as the credentials.  For testing to
confirm what a different user sees, you can also log in as "user2" and
"password".


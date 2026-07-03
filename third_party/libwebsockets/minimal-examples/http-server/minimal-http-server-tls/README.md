# lws minimal http server with tls

## build

```
 $ cmake . && make
```

## usage

```
 $ ./lws-minimal-http-server-tls
[2018/03/20 13:23:13:0131] USER: LWS minimal http server TLS | visit https://localhost:7681
[2018/03/20 13:23:13:0142] NOTICE: Creating Vhost 'default' port 7681, 1 protocols, IPv6 off
[2018/03/20 13:23:13:0142] NOTICE:  Using SSL mode
[2018/03/20 13:23:13:0146] NOTICE:  SSL ECDH curve 'prime256v1'
[2018/03/20 13:23:13:0146] NOTICE:  HTTP2 / ALPN enabled
[2018/03/20 13:23:13:0195] NOTICE: lws_tls_client_create_vhost_context: doing cert filepath localhost-100y.cert
[2018/03/20 13:23:13:0195] NOTICE: Loaded client cert localhost-100y.cert
[2018/03/20 13:23:13:0195] NOTICE: lws_tls_client_create_vhost_context: doing private key filepath
[2018/03/20 13:23:13:0196] NOTICE: Loaded client cert private key localhost-100y.key
[2018/03/20 13:23:13:0196] NOTICE: created client ssl context for default
[2018/03/20 13:23:14:0207] NOTICE:    vhost default: cert expiry: 730459d
```

Visit https://localhost:7681

Because it uses a selfsigned certificate, you will have to make an exception for it in your browser.

## Certificate creation

The selfsigned certs provided were created with

```
echo -e "GB\nErewhon\nAll around\nlibwebsockets-test\n\nlocalhost\nnone@invalid.org\n" | openssl req -new -newkey rsa:4096 -days 36500 -nodes -x509 -keyout "localhost-100y.key" -out "localhost-100y.cert"
```

they cover "localhost" and last 100 years from 2018-03-20.

You can replace them with commercial certificates matching your hostname.

## HTTP/2

If you built lws with `-DLWS_WITH_HTTP2=1` at cmake, this simple server is also http/2 capable
out of the box.  If the index.html was loaded over http/2, it will display an HTTP 2 png.

# lws minimal http server with tls and certs from memory

This is the same as the minimal-http-server-tls example, but shows how
to init the vhost with both PEM or DER certs from memory instead of files.

The server listens on port 7681 (initialized with PEM in-memory certs) and
port 7682 (initialized with DER in-memory certs).

## build

```
 $ cmake . && make
```

## usage

```
 $ ./lws-minimal-http-server-tls-mem
[2019/02/14 14:46:40:9783] USER: LWS minimal http server TLS | visit https://localhost:7681
[2019/02/14 14:46:40:9784] NOTICE:  Using SSL mode
[2019/02/14 14:46:40:9784] NOTICE: lws_tls_server_vhost_backend_init: vh first: mem CA OK
parsing as der
[2019/02/14 14:46:40:9849] NOTICE: no client cert required
[2019/02/14 14:46:40:9849] NOTICE: created client ssl context for first
[2019/02/14 14:46:40:9849] NOTICE:  Using SSL mode
[2019/02/14 14:46:40:9850] NOTICE: lws_tls_server_vhost_backend_init: vh second: mem CA OK
parsing as der
[2019/02/14 14:46:40:9894] NOTICE: no client cert required
[2019/02/14 14:46:40:9894] NOTICE: created client ssl context for second
[2019/02/14 14:46:40:9894] NOTICE:    vhost first: cert expiry: 36167d
[2019/02/14 14:46:40:9894] NOTICE:    vhost second: cert expiry: 36167d
[2018/03/20 13:23:14:0207] NOTICE:    vhost default: cert expiry: 730459d
```

Visit https://127.0.0.1:7681 and https://127.0.0.1:7682

Because it uses a selfsigned certificate, you will have to make an exception for it in your browser.

## Certificate creation

The selfsigned certs provided were created with

```
echo -e "GB\nErewhon\nAll around\nlibwebsockets-test\n\nlocalhost\nnone@invalid.org\n" | openssl req -new -newkey rsa:4096 -days 36500 -nodes -x509 -keyout "localhost-100y.key" -out "localhost-100y.cert"
```

they cover "localhost" and last 100 years from 2018-03-20.

You can replace them with commercial certificates matching your hostname.

The der content was made from PEM like this

```
 $ cat ../minimal-http-server-tls/localhost-100y.key | grep -v ^- | base64 -d | hexdump -C  | tr -s ' ' | cut -d' ' -f2- | cut -d' ' -f-16 | sed "s/|.*//g" | sed "s/0000.*//g" | sed "s/^/0x/g" | sed "s/\ /\,\ 0x/g" | sed "s/\$/,/g" | sed "s/0x,//g"
```

## HTTP/2

If you built lws with `-DLWS_WITH_HTTP2=1` at cmake, this simple server is also http/2 capable
out of the box.  If the index.html was loaded over http/2, it will display an HTTP 2 png.

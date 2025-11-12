# lws minimal http server with tls and port 80 redirect

## build

```
 $ cmake . && make
```

## usage

Because this listens on low ports (80 + 443), it must be run as root.

```
 $ sudo ./lws-minimal-http-server-tls-80
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

Visit http://localhost

This will go first to port 80 using http, where it will be redirected to
https and port 443

```
07:41:48.596918 IP localhost.http > localhost.52662: Flags [P.], seq 1:100, ack 416, win 350, options [nop,nop,TS val 3906619933 ecr 3906619933], length 99: HTTP: HTTP/1.1 301 Redirect
	0x0000:  4500 0097 3f8f 4000 4006 fccf 7f00 0001  E...?.@.@.......
	0x0010:  7f00 0001 0050 cdb6 6601 dfa7 922a 4c06  .....P..f....*L.
	0x0020:  8018 015e fe8b 0000 0101 080a e8da 4a1d  ...^..........J.
	0x0030:  e8da 4a1d 4854 5450 2f31 2e31 2033 3031  ..J.HTTP/1.1.301
	0x0040:  2052 6564 6972 6563 740d 0a6c 6f63 6174  .Redirect..locat
	0x0050:  696f 6e3a 2068 7474 7073 3a2f 2f6c 6f63  ion:.https://loc
	0x0060:  616c 686f 7374 2f0d 0a63 6f6e 7465 6e74  alhost/..content
	0x0070:  2d74 7970 653a 2074 6578 742f 6874 6d6c  -type:.text/html
	0x0080:  0d0a 636f 6e74 656e 742d 6c65 6e67 7468  ..content-length
	0x0090:  3a20 300d 0a0d 0a
```

Because :443 uses a selfsigned certificate, you will have to make an exception for it in your browser.

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

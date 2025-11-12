## Background

libressl is another fork of Openssl.

## Example build for libressl itself

If you unpack or clone into `/path/to/libressl` and enter that dir...

```
$ mkdir build
$ cd build
$ cmake ..
$ make -j8
```

## Example build for lws against libressl

You can just build lws as you would for a specific version of openssl

```
$ mkdir build
$ cd build
$ cmake .. -DLWS_OPENSSL_LIBRARIES='/path/to/libressl/build/tls/libtls.a;/path/to/libressl/build/ssl/libssl.a;/path/to//libressl/build/crypto/libcrypto.a' -DLWS_OPENSSL_INCLUDE_DIRS=/path/to/libressl/include -DLWS_WITH_MINIMAL_EXAMPLES=1
$ make -j8
```

Libressl by default will look for a trust bundle in `/usr/local/etc/ssl/cert.pem`, you either have to
symlink this to your trust bundle if that doesnt happen to be where it is, or give your app the trusted CA
specifically as is done for MBEDTLS and WOLFSSL in the examples.

In Fedora, the system trust store can be found at `/etc/pki/tls/cert.pem`, so you can symlink it

```
$ sudo mkdir -p /usr/local/etc/ssl
$ sudo ln -sf /etc/pki/tls/cert.pem /usr/local/etc/ssl/cert.pem
```

after that you can run examples from the build dir, eg,

```
$ ./bin/lws-minimal-http-client
[2021/02/08 20:10:52:0781] U: LWS minimal http client [-d<verbosity>] [-l] [--h1]
[2021/02/08 20:10:52:0784] N: LWS: 4.1.99-v4.1.0-269-g762ef33fca, loglevel 1031
[2021/02/08 20:10:52:0784] N: NET CLI SRV H1 H2 WS IPv6-absent
[2021/02/08 20:10:52:0786] N:  ++ [wsi|0|pipe] (1)
[2021/02/08 20:10:52:0787] N:  ++ [vh|0|netlink] (1)
[2021/02/08 20:10:52:0802] N:  ++ [vh|1|default] (2)
[2021/02/08 20:10:52:1850] N:  ++ [wsicli|0|GET/h1/warmcat.com] (1)
[2021/02/08 20:10:52:2982] N:  ++ [mux|0|h2_sid1_(wsicli|0|GET/h1/warmcat.com)] (1)
[2021/02/08 20:10:52:3271] U: Connected to 46.105.127.147, http response: 200
[2021/02/08 20:10:52:3335] U: RECEIVE_CLIENT_HTTP_READ: read 4087
[2021/02/08 20:10:52:3335] U: RECEIVE_CLIENT_HTTP_READ: read 4096
[2021/02/08 20:10:52:3526] U: RECEIVE_CLIENT_HTTP_READ: read 4087
[2021/02/08 20:10:52:3526] U: RECEIVE_CLIENT_HTTP_READ: read 4096
[2021/02/08 20:10:52:3543] U: RECEIVE_CLIENT_HTTP_READ: read 4087
[2021/02/08 20:10:52:3543] U: RECEIVE_CLIENT_HTTP_READ: read 4096
[2021/02/08 20:10:52:3545] U: RECEIVE_CLIENT_HTTP_READ: read 3502
[2021/02/08 20:10:52:3546] U: LWS_CALLBACK_COMPLETED_CLIENT_HTTP
[2021/02/08 20:10:52:3546] N:  -- [wsi|0|pipe] (0) 276.019ms
[2021/02/08 20:10:52:3547] N:  -- [mux|0|h2_sid1_(wsicli|0|GET/h1/warmcat.com)] (0) 56.417ms
[2021/02/08 20:10:52:3566] N:  -- [vh|1|default] (1) 276.384ms
[2021/02/08 20:10:52:3566] N:  -- [wsicli|0|GET/h1/warmcat.com|default|h2|h2] (0) 171.599ms
[2021/02/08 20:10:52:3567] N:  -- [vh|0|netlink] (0) 277.974ms
[2021/02/08 20:10:52:3567] U: Completed: OK
```


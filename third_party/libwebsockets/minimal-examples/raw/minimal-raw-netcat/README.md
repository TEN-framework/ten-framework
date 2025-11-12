# lws minimal raw netcat

This example shows to to create a "netcat" that copies its stdin to
a remote socket and prints what is returned in stdout.

It has some advantage over the real netcat, it will wait 1s after stdin closes
to print results that are in flight.

## build

```
 $ cmake . && make
```

## usage

```
 $ echo -e -n "GET / http/1.1\r\n\r\n"| ./lws-minimal-raw-netcat
[2018/05/02 08:53:53:2665] USER: LWS minimal raw netcat [--server ip] [--port port]
[2018/05/02 08:53:53:2667] NOTICE: Creating Vhost 'default' (no listener), 1 protocols, IPv6 off
[2018/05/02 08:53:53:2703] USER: Starting connect...
[2018/05/02 08:53:53:5644] USER: Connected to libwebsockets.org:80...
[2018/05/02 08:53:53:5645] USER: LWS_CALLBACK_RAW_ADOPT
[2018/05/02 08:53:53:5645] USER: LWS_CALLBACK_RAW_ADOPT_FILE
[2018/05/02 08:53:53:5646] USER: LWS_CALLBACK_RAW_RX_FILE
[2018/05/02 08:53:53:5646] USER: LWS_CALLBACK_RAW_CLOSE_FILE
[2018/05/02 08:53:53:8600] USER: LWS_CALLBACK_RAW_RX (186)
HTTP/1.1 301 Redirect
server: lwsws
Strict-Transport-Security: max-age=15768000 ; includeSubDomains
location: https://libwebsockets.org
content-type: text/html
content-length: 0

```

Note the example does everything itself, after 5s idle the remote server closes the connection
after which the example continues until you ^C it.

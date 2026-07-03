# lws minimal ws - raw proxy

This demonstrates how to use a proxy connection object to bind together two or
more connections in a proxy.  This particular example has a ws server that
creates an onward "raw" client connection to 127.0.0.1:1234.

You can make a suitable "raw server" with

```
$ nc -l 127.0.0.1 1234
```

## build

```
 $ cmake . && make
```

## Commandline Options

Option|Meaning
---|---
-d|Set logging verbosity


## usage

```
 $ ./lws-minimal-ws-raw-proxy
[2021/03/04 21:14:45:0540] U: LWS minimal ws-raw proxy | visit http://localhost:7681 (-s = use TLS / https)
[2021/03/04 21:14:45:0898] N: LWS: 4.1.99-v4.1.0-294-g2776b4ce65, loglevel 1031
[2021/03/04 21:14:45:0902] N: NET CLI SRV H1 H2 WS SS-JSON-POL SSPROX IPV6-on
[2021/03/04 21:14:45:1146] N:  ++ [3224086|wsi|0|pipe] (1)
[2021/03/04 21:14:45:1203] N:  ++ [3224086|vh|0|netlink] (1)
[2021/03/04 21:14:45:1284] N:  ++ [3224086|vh|1|localhost||7681] (2)
[2021/03/04 21:14:45:1401] N: lws_socket_bind: nowsi: source ads ::
[2021/03/04 21:14:45:1425] N:  ++ [3224086|wsi|1|listen|localhost||7681] (2)
[2021/03/04 21:14:46:1164] N:  ++ [3224086|wsisrv|0|adopted] (1)
[2021/03/04 21:14:46:2771] N:  ++ [3224086|wsisrv|1|adopted] (2)
[2021/03/04 21:14:46:3159] N:  ++ [3224086|wsicli|0|RAW/raw-skt/127.0.0.1] (1)
[2021/03/04 21:14:46:3451] N:  ++ [3224086|wsisrv|2|adopted] (3)

```

Visit http://localhost:7681 in a browser... it loads JS that opens a ws
connection to the proxy's ws server side.  That causes the proxy to open a
raw client connection to 127.0.0.1:1234, and forward anything you type in the
browser to the raw server, and anything typed in the raw server (you must
press enter on netcat to get it sent) is proxied back to the browser.

The proxy can handle many ws connections each with their individual onward
raw client connections, so you could open multiple browser windows.  But you
will need a better "raw server" than netcat, which is restricted to just the
one peer at a time. 
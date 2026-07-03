# lws minimal http client certinfo

This demonstrates how to dump information from the peer
certificate largely independent of the tls backend.

The application goes to https://warmcat.com and receives the page data.

Before receiving the page it dumps information on the server's cert.

This works independently of the tls backend being OpenSSL or mbedTLS.

However the public keys cannot be compared between the two tls
backends, since they produce different representations.

## build

```
 $ cmake . && make
```

## usage

Commandline option|Meaning
---|---
-d <loglevel>|Debug verbosity in decimal, eg, -d15
-l| Connect to https://localhost:7681 and accept selfsigned cert
--h1|Specify http/1.1 only using ALPN, rejects h2 even if server supports it

```
 $ ./lws-minimal-http-client-certinfo
[2018/04/05 21:39:26:5882] USER: LWS minimal http client
[2018/04/05 21:39:26:5897] NOTICE: Creating Vhost 'default' (serving disabled), 1 protocols, IPv6 on
[2018/04/05 21:39:26:5955] NOTICE: created client ssl context for default
[2018/04/05 21:39:28:0824] NOTICE: lws_http_client_http_response 200
[2018/04/05 21:39:28:0824] NOTICE:  Peer Cert CN        : warmcat.com
[2018/04/05 21:39:28:0824] NOTICE:  Peer Cert issuer    : /C=GB/ST=Greater Manchester/L=Salford/O=COMODO CA Limited
[2018/04/05 21:39:28:0825] NOTICE:  Peer Cert Valid from: Mon Nov  3 00:00:00 2014
[2018/04/05 21:39:28:0825] NOTICE:  Peer Cert Valid to  : Sat Nov  2 23:59:59 2019
[2018/04/05 21:39:28:0825] NOTICE:  Peer Cert usage bits: 0xa0
[2018/04/05 21:39:28:0825] NOTICE:  Peer Cert public key:
[2018/04/05 21:39:28:0825] NOTICE: 
[2018/04/05 21:39:28:0825] NOTICE: 0000: 30 82 01 22 30 0D 06 09 2A 86 48 86 F7 0D 01 01    0.."0...*.H.....
[2018/04/05 21:39:28:0825] NOTICE: 0010: 01 05 00 03 82 01 0F 00 30 82 01 0A 02 82 01 01    ........0.......
[2018/04/05 21:39:28:0825] NOTICE: 0020: 00 EC 39 C1 98 25 A8 99 AC 01 9B D2 16 C0 CA A3    ..9..%..........
[2018/04/05 21:39:28:0825] NOTICE: 0030: 0E 19 57 E5 3D 23 F3 79 7E 63 BF CD B8 88 D1 16    ..W.=#.y~c......
[2018/04/05 21:39:28:0825] NOTICE: 0040: C6 F0 A6 ED 66 CB F3 C3 D6 7E A7 A3 AB 00 0A 3E    ....f....~.....>
[2018/04/05 21:39:28:0825] NOTICE: 0050: AD EF 20 44 85 5A 61 F0 71 20 BD E3 D1 4B B6 53    .. D.Za.q ...K.S
[2018/04/05 21:39:28:0825] NOTICE: 0060: 57 AA 81 E6 ED 74 36 40 E7 FC 62 24 AD E8 82 1D    W....t6@..b$....
[2018/04/05 21:39:28:0826] NOTICE: 0070: 89 C4 3D 64 6C A8 34 4B DB FB DD 7D D2 2D FB 86    ..=dl.4K...}.-..
[2018/04/05 21:39:28:0826] NOTICE: 0080: 97 EA 6B E2 C9 39 D6 19 DE A8 90 E7 86 8F CF 0A    ..k..9..........
[2018/04/05 21:39:28:0826] NOTICE: 0090: CD 09 3C AF FB 0A FF 85 E8 93 D1 4B A0 C5 21 AD    ..<........K..!.
[2018/04/05 21:39:28:0826] NOTICE: 00A0: 58 52 30 0E 4B FE 4F C8 01 B9 BD 0F D4 E4 64 7B    XR0.K.O.......d{
[2018/04/05 21:39:28:0826] NOTICE: 00B0: 04 B4 D2 68 69 8F F1 D5 FD B0 1A CE 55 43 08 B7    ...hi.......UC..
[2018/04/05 21:39:28:0826] NOTICE: 00C0: 9F 57 0D 4E E1 CA E8 5C B4 2A 6B AB 05 B5 57 67    .W.N...\.*k...Wg
[2018/04/05 21:39:28:0826] NOTICE: 00D0: B8 FD 20 F4 4F 6B 0E 47 7C AD EB B4 99 2C 9B 53    .. .Ok.G|....,.S
[2018/04/05 21:39:28:0826] NOTICE: 00E0: DF EA 67 8D 8A 9D A7 17 01 F9 4E BD 56 43 50 53    ..g.......N.VCPS
[2018/04/05 21:39:28:0826] NOTICE: 00F0: 08 4E FE 6A 85 4A 4D 45 03 DA 01 00 96 7A C0 A9    .N.j.JME.....z..
[2018/04/05 21:39:28:0826] NOTICE: 0100: C2 32 5E 1A 9F 6F 7B E2 02 5E 70 12 D3 8E 76 6A    .2^..o{..^p...vj
[2018/04/05 21:39:28:0826] NOTICE: 0110: 0B 59 A4 D7 31 9D C6 86 08 53 2E 02 8A 1E B1 FB    .Y..1....S......
[2018/04/05 21:39:28:0826] NOTICE: 0120: 7B 02 03 01 00 01                                  {.....          
[2018/04/05 21:39:28:0826] NOTICE: 
[2018/04/05 21:39:28:0829] USER: RECEIVE_CLIENT_HTTP_READ: read 503
[2018/04/05 21:39:28:0829] USER: RECEIVE_CLIENT_HTTP_READ: read 512
[2018/04/05 21:39:28:0829] USER: RECEIVE_CLIENT_HTTP_READ: read 512
[2018/04/05 21:39:28:0829] USER: RECEIVE_CLIENT_HTTP_READ: read 512
...
[2018/04/05 21:39:28:3777] USER: RECEIVE_CLIENT_HTTP_READ: read 512
[2018/04/05 21:39:28:3777] USER: RECEIVE_CLIENT_HTTP_READ: read 512
[2018/04/05 21:39:28:3778] USER: RECEIVE_CLIENT_HTTP_READ: read 503
[2018/04/05 21:39:28:3778] USER: RECEIVE_CLIENT_HTTP_READ: read 512
[2018/04/05 21:39:28:3778] USER: RECEIVE_CLIENT_HTTP_READ: read 512
[2018/04/05 21:39:28:3778] USER: RECEIVE_CLIENT_HTTP_READ: read 471
[2018/04/05 21:39:28:3778] USER: LWS_CALLBACK_COMPLETED_CLIENT_HTTP
[2018/04/05 21:39:28:3787] USER: Completed
```



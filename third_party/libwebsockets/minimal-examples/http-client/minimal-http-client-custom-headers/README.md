# lws minimal http client custom headers

This http client application shows how to send and receive custom headers.

This currently only works on http 1, so the app forces that even if h2 enables.

## build

```
 $ cmake . && make
```

## usage

Commandline option|Meaning
---|---
-d <loglevel>|Debug verbosity in decimal, eg, -d15
-l| Connect to https://localhost:7681 and accept selfsigned cert
-n|no TLS

The app looks for a custom header "test-custom-header" sent by warmcat.com.

```
 $ ./lws-minimal-http-client-custom-headers
[2019/03/11 05:46:45:7582] USER: LWS minimal http client Custom Headers [-d<verbosity>] [-l] [--h1]
[2019/03/11 05:46:45:7671] NOTICE: created client ssl context for default
[2019/03/11 05:46:46:7812] USER: Connected with server response: 200
[2019/03/11 05:46:46:7812] NOTICE: callback_http: custom header: 'hello'
[2019/03/11 05:46:46:7814] USER: RECEIVE_CLIENT_HTTP_READ: read 1024
...
```
You can use the -n and -l to make this test app connect to localhost:7681 over http,
and confirm the "dnt:1" header was sent either by tcpdump or by running the test
server on :7681 with -d1151

```
[2019/03/11 05:48:53:6806] PARSER: WSI_TOKEN_NAME_PART 'd' 0x64 (role=0x20000000) wsi->lextable_pos=0
[2019/03/11 05:48:53:6807] PARSER: WSI_TOKEN_NAME_PART 'n' 0x6E (role=0x20000000) wsi->lextable_pos=567
[2019/03/11 05:48:53:6807] PARSER: WSI_TOKEN_NAME_PART 't' 0x74 (role=0x20000000) wsi->lextable_pos=-1
[2019/03/11 05:48:53:6807] PARSER: WSI_TOKEN_NAME_PART ' ' 0x20 (role=0x20000000) wsi->lextable_pos=-1
[2019/03/11 05:48:53:6807] PARSER: WSI_TOKEN_NAME_PART '1' 0x31 (role=0x20000000) wsi->lextable_pos=-1
' 0x0D (role=0x20000000) wsi->lextable_pos=-1NAME_PART '
[2019/03/11 05:48:53:6807] PARSER: WSI_TOKEN_NAME_PART '
```


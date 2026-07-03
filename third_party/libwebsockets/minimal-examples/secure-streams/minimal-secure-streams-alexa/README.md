# lws secure streams alexa

This demonstrates AVS Alexa usage using secure streams.  It connects to AVS,
uses your linux computer's microphone to wait for the 'alexa' wakeword, sends
the utterance to AVS and plays back the result.

## build

There are some special build considerations:

1) Build lws with cmake options `-DLWS_WITH_ALSA=1 -DLWS_WITH_SECURE_STREAMS=1`

2) Install distro build dependency packages:

 |Dependency|Ubuntu package|Fedora Package|
 |---|---|---|
 |libasound|libasound2-dev|alsa-lib-devel|
 |mpg123|libmpg123-dev|mpg123-devel|

3) Clone Picovoice Porcupine Apache-licensed demo version from here

   https://github.com/Picovoice/porcupine

   It provides binary libs for wakeword detection on various platforms.  Copy
   the headers and binary lib to your build context, eg, for native x86_64

```
   $ sudo cp ./include/* /usr/include
   $ sudo cp ./lib/linux/x86_64/libpv_porcupine.* /usr/lib
   $ sudo ldconfig
```

   Enter the minimal example dir for secure-streams-alexa and make the sample

```
   $ cd ./minimal-examples/secure-streams/minimal-secure-streams-alexa
   $ cmake .
   $ make
```

## usage

```
 $ ./lws-minimal-secure-streams-alexa
[2019/10/16 16:22:01:1097] U: LWS secure streams - Alex voice test [-d<verb>]
[2019/10/16 16:22:01:1115] N: lws_create_context: creating Secure Streams policy
[2019/10/16 16:22:01:1115] N: lwsac_use: alloc 1532 for 1
[2019/10/16 16:22:01:1119] N: lwsac_use: alloc 288 for 168
[2019/10/16 16:22:01:1119] N: lws_ss_policy_set: policy lwsac size:     1.796KiB, pad 11%
[2019/10/16 16:22:02:4114] N: lws_ss_client_connect: connecting 0 api.amazon.com /auth/o2/token
[2019/10/16 16:22:02:8686] N: auth_api_amazon_com_parser_cb: expires in 3600
[2019/10/16 16:22:02:8686] N: ss_api_amazon_auth_rx: acquired 656-byte api.amazon.com auth token
[2019/10/16 16:22:02:8754] N: lws_ss_client_connect: connecting 1 alexa.na.gateway.devices.a2z.com /v20160207/directives
[2019/10/16 16:22:02:3182] N: secstream_h2: h2 client entering LONG_POLL
[2019/10/16 16:22:02:3183] U: Connected to Alexa... speak "Alexa, ..."
[2019/10/16 16:22:06:9380] W: ************* Wakeword
[2019/10/16 16:22:06:9380] N: avs_query_start:
[2019/10/16 16:22:06:9381] N: lws_ss_client_connect: connecting 1 alexa.na.gateway.devices.a2z.com /v20160207/events
[2019/10/16 16:22:06:9381] N: lws_vhost_active_conns: just join h2 directly
[2019/10/16 16:22:06:9384] N: metadata done
[2019/10/16 16:22:06:1524] N: est: 42 1
[2019/10/16 16:22:06:3723] N: est: 108 1
[2019/10/16 16:22:07:5914] N: est: 352 1
[2019/10/16 16:22:07:8112] N: est: 4284 1
[2019/10/16 16:22:07:0300] N: est: 3369 1
[2019/10/16 16:22:07:2325] N: est: 577 1
[2019/10/16 16:22:08:4519] N: est: 9 1
[2019/10/16 16:22:08:6716] N: est: 3 1
[2019/10/16 16:22:08:6718] N: est: 11 1
[2019/10/16 16:22:08:8915] N: est: 10 1
[2019/10/16 16:22:08:8915] W: callback_audio: ended capture
[2019/10/16 16:22:09:0993] N: identified reply...
^C[2019/10/16 16:22:14:3067] U: Disconnected from Alexa
[2019/10/16 16:22:14:3123] U: Completed
$

```

# lws minimal raw audio

This demonstrates operating ALSA playback and capture using the lws event loop
via raw file descriptors.

You need the lws cmake option `-DLWS_WITH_ALSA=1`

This example opens the default ALSA playback and capture devices and pipes the
capture data into the playback with something over 1s delay via a ringbuffer.

ALSA doesn't really lend itself to direct use with event loops... this example
uses the capture channel which does create POLLIN normally as the timesource
for the playback as well; they're both set to 16000Hz sample rate.

## build

```
 $ cmake . && make
```

## usage

```
 $ ./lws-minimal-raw-audio
[2019/10/14 18:58:49:3288] U: LWS minimal raw audio
[2019/10/14 18:58:50:3438] N: LWS_CALLBACK_RAW_ADOPT_FILE
[2019/10/14 18:58:50:3455] N: LWS_CALLBACK_RAW_ADOPT_FILE
[2019/10/14 18:58:50:4764] N: LWS_CALLBACK_RAW_RX_FILE: 2062 samples
[2019/10/14 18:58:50:6132] N: LWS_CALLBACK_RAW_RX_FILE: 2205 samples
[2019/10/14 18:58:50:7592] N: LWS_CALLBACK_RAW_RX_FILE: 2328 samples
...
^C[2019/10/14 18:58:56:8460] N: LWS_CALLBACK_RAW_CLOSE_FILE
[2019/10/14 18:58:56:8461] N: LWS_CALLBACK_RAW_CLOSE_FILE
[2019/10/14 18:58:56:8461] N: LWS_CALLBACK_PROTOCOL_DESTROY
$

```

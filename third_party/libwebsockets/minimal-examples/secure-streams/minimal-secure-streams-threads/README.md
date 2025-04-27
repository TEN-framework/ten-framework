# lws minimal secure streams threads

This application creates a thread and calls `lws_cancel_service()`
at 10Hz.

It creates a Secure Stream and checks that it is getting the
`LWSSSCS_EVENT_WAIT_CANCELLED` state for each `lws_cancel_service()`.

It also demonstrates how to protect a shared data area between the
thread(s) and the lws event loop thread to put data there that
describes what the thread wants the service loop to do.

It exits after 3s with a 0 return code if the SS saw the expected
amount of messages.

## build

```
 $ cmake . && make
```

## usage

Commandline option|Meaning
---|---
-d <loglevel>|Debug verbosity in decimal, eg, -d15


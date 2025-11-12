# `TCP_FASTOPEN` support in lws

Lws supports enabling TCP_FASTOPEN oper-vhost for listen sockets.

## Enabling per vhost serving

Set the `info.fo_listen_queue` to nonzero at vhost creation.  Different
platforms interpret this number differently, zero always disables it
but on Linux, the number is interpreted as a SYN queue length.

On FreeBSD, OSX and Windows, the number is basically a bool, with the
extra restriction OSX and Windows only allows 0 or 1.

## Enabling Linux for serving with TCP_FASTOPEN

To configure the kernel for listening socket TCP_FASTOPEN, you need

```
# sysctl -w net.ipv4.tcp_fastopen=3
```

## Enabling BSD for serving with TCP_FASTOPEN

At least on FreeBSD, you need to set the net.inet.tcp.fastopen.enabled
sysctl to 1

## Enabling Windows for serving with TCP_FASTOPEN

```
> netsh int tcp set global fastopenfallback=disabled
> netsh int tcp show global
```

# lws minimal MQTT client multi

## build

```
 $ cmake . && make
```

## usage

The application goes to https://warmcat.com and receives the page data
same as minimal http client.

However it does it for 8 client connections concurrently.

## Commandline Options

Option|Meaning
---|---
-c <conns>|Count of simultaneous connections (default 8)
-s|Stagger the connections by 100ms, the last by 1s
-p|Use stream binding



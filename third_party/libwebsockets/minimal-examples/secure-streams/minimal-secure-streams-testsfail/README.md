# lws minimal secure streams

The application runs some bulk and failure path tests on Secure Streams

## build

```
 $ cmake . && make
```

## usage

Commandline option|Meaning
---|---
-d <loglevel>|Debug verbosity in decimal, eg, -d15
--amount <amount>| Set the amount of bulk data expected, eg, --amount 23456


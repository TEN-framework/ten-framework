# lws minimal secure streams stress

This is the same as minimal-secure-streams, except you can have it perform concurrent
SS connections and a budget of sequential connections.

It basically forks as many times as `-c <concurrent>` and each fork does `--budget <count>`
SS connections one after the other.

## build

```
 $ cmake . && make
```

## usage

Commandline option|Meaning
---|---
-d <loglevel>|Debug verbosity in decimal, eg, -d15|
-c <concurrent>|Fork this many times on init|
--budget <count>|Each fork sequentially does this many SS connections (default 1)|
--pass-limit <count>|By default the pass limit is the budget, but if doing fault injection you can set a lower limit here|

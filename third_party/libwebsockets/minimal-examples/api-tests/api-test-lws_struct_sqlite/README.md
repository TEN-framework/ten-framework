# lws api test lws_struct SQLITE

Demonstrates how to use and performs selftests for lws_struct
SQLITE serialization and deserialization

## build

```
 $ cmake . && make
```

## usage

Commandline option|Meaning
---|---
-d <loglevel>|Debug verbosity in decimal, eg, -d15

```
 $ ./lws-api-test-lws_struct-sqlite
[2020/02/22 09:55:05:4335] U: LWS API selftest: lws_struct SQLite
[2020/02/22 09:55:05:5579] N: lws_struct_sq3_open: created _lws_apitest.sq3 owned by 0:0 mode 0600
[2020/02/22 09:55:05:9206] U: Completed: PASS

```


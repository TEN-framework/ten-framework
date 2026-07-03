# lws api test gencrypto

Demonstrates how to use and performs selftests for Generic Crypto,
which works the same whether the tls backend is OpenSSL or mbedTLS

## build

```
 $ cmake . && make
```

## usage

Commandline option|Meaning
---|---
-d <loglevel>|Debug verbosity in decimal, eg, -d15

```
 $ ./lws-api-test-gencrypto
[2018/12/05 08:30:27:1342] USER: LWS gencrypto apis tests
[2018/12/05 08:30:27:1343] NOTICE: Creating Vhost 'default' (serving disabled), 1 protocols, IPv6 off
[2018/12/05 08:30:27:1343] NOTICE: created client ssl context for default
[2018/12/05 08:30:27:1344] NOTICE: test_genaes: selftest OK
[2018/12/05 08:30:27:1344] USER: Completed: PASS
```


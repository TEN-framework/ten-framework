# lws api test lws_tokenize

Performs selftests for lws_tokenize

## build

```
 $ cmake . && make
```

## usage

Commandline option|Meaning
---|---
-d <loglevel>|Debug verbosity in decimal, eg, -d15
-s "input string"|String to tokenize
-f 15|LWS_TOKENIZE_F_ flag values to apply to processing of -s 

```
 $ ./lws-api-test-lws_tokenize
[2018/10/09 09:14:17:4834] USER: LWS API selftest: lws_tokenize
[2018/10/09 09:14:17:4835] USER: Completed: PASS: 6, FAIL: 0
```

If the `-s string` option is given, the string is tokenized on stdout in
the format used to produce the tests in the sources

```
 $ ./lws-api-test-lws_tokenize -s "hello: 1234,256"
[2018/10/09 09:14:17:4834] USER: LWS API selftest: lws_tokenize
{ LWS_TOKZE_TOKEN_NAME_COLON, "hello", 5 }
{ LWS_TOKZE_INTEGER, "1234", 4 }
{ LWS_TOKZE_DELIMITER, ",", 1 }
{ LWS_TOKZE_INTEGER, "256", 3 }
{ LWS_TOKZE_ENDED, "", 0 }
```


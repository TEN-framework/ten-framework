# Tips on debugging with lws

## Problem with the library, or your code?

Because lws is only really used when already combined with user code,
it can be a headache figuring out if the actual problem is inside lws
or in the user code.

If it's in lws, I would really like to solve it, but if it's in your
code, that's your problem.  Finding out which side it's on when it
involves your code is also something you need to try to resolve.

The minimal examples are useful because if they demonstrate the same
problem, it's something about your platform or lws itself, I have the
minimal examples so I can test it and find out if it's your platform.
If I can reproduce it, it's my problem.

## Debug builds

With cmake, build with `-DCMAKE_BUILD_TYPE=DEBUG` to build in extra
logging, and use a log level bitmap of eg, 1039 or 1151 to enable
the extra logs for print.

The minimal examples take a -d xxx commandline parameter so you can
select the logging level when you run it.

The extra logging can be very useful to understand the sequencing of
problematic actions.

## Valgrind

If your problems involve heap corruption or use-after-free, Valgrind
is indespensible.  It's simple to use, if you normally run `xxx`, just
run `valgrind xxx`.  Your code will run slower, usually something
like 2 - 4x slower but it depends on the exact code.  However you will
get a backtrace as soon as there is some kind of misbehaviour of either
lws or your code.

lws is developed using valgrind routinely and strives to be completely
valgrind-clean.  So typically any problems reported are telling you
about problems in user code (or my bugs).

## Traffic dumping

The best place for dumping traffic, assuming you are linking against a
tls library, is `lws_ssl_capable_read()` and `lws_ssl_capable_write()`
in either `./lib/tls/openssl/openssl-ssl.c` or
`./lib/tls/mbedtls/mbedtls-ssl.c` according to which tls library you
are using.  There are default-`#if 0` sections in each function like

```
#if 0
	/*
	 * If using mbedtls type tls library, this is the earliest point for all
	 * paths to dump what was received as decrypted data from the tls tunnel
	 */
	lwsl_notice("%s: len %d\n", __func__, len);
	lwsl_hexdump_notice(buf, len);
#endif
```

Enable these to get hexdumps for all unencrypted data in both directions.


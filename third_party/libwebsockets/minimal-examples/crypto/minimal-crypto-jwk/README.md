# lws minimal example for JWK

Demonstrates how to generate and format any kind of supported new random JWK keys.

The full private key is output to stdout, a version of the key with the private
part removed and some metadata adapted can be saved to a file at the same time
using `--public <file>`.  In the public form, `key_ops` and `use` elements are
adjusted to remove activities that require a private key.

Key elements are output in strict RFC7638 lexicographic order as required by
some applications.

Keys produced with openssl and mbedtls backends are completely interchangeable.

## build

```
 $ cmake . && make
```

## usage

Commandline option|Meaning
---|---
-d <loglevel>|Debug verbosity in decimal, eg, -d15
-t <type>|RSA, OCT or EC
-b <bits>|For RSA and OCT, key size in bits
-v <curve>|For EC keys, the curve, eg, "P-384"... this implies the key bits
--kid "ID string"|Key identity string
--use "use[ use]"|Key use restriction (mutually exclusive with --key-ops): sig, enc
--alg <alg>|Specify the algorithm the key is designed for, eg "RSA1_5"
--key-ops "op[ op]"|Key valid operations (mutually exclusive with --use): sign, verify, encrypt, decrypt, wrapKey, unwrapKey, deriveKey, deriveBits
-c|Format the jwk as a linebroken C string
--public <filepath>|Only output the full, private key, not the public version first

For legibility the example uses -c, however this

```
 $ ./lws-crypto-jwk -t EC -v P-256 --key-ops "sign verify" --public mykey.pub
[2018/12/18 20:19:29:6972] USER: LWS JWK example
[2018/12/18 20:19:29:7200] NOTICE: Creating Vhost 'default' (serving disabled), 1 protocols, IPv6 off
[2018/12/18 20:19:29:7251] NOTICE: lws_jwk_generate: generating ECDSA key on curve P-256
{"crv":"P-256","d":"eMKM_S4BTL2aiebZLqvxglufV2YX4b3_32DesgEUOaM","key_ops":["sign","verify"],"kty":"EC","x":"OWauiGGtJ60ZegtqlwETQlmO1exTZdWbT2VbUs4a1hg","y":"g_eNOlqPecbguVQArL6Fd4T5xZthBgipNCBypXubPos"}
```

The output in `mykey.pub` is:

```
{"crv":"P-256","key_ops":["verify"],"kty":"EC","x":"OWauiGGtJ60ZegtqlwETQlmO1exTZdWbT2VbUs4a1hg","y":"g_eNOlqPecbguVQArL6Fd4T5xZthBgipNCBypXubPos"}
```

Notice the logging goes out on stderr, the key data goes on stdout.

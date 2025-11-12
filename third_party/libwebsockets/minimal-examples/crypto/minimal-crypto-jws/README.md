# lws minimal example for JWS

Demonstrates how to sign and verify using compact JWS and JWK, providing a
commandline tool for signing and verifying stdin.

## build

```
 $ cmake . && make
```

## usage

Stdin is either the plaintext (if signing) or compact JWS (if verifying).

Stdout is either the JWE (if encrypting) or plaintext (if decrypting).

You must pass a private or public key JWK file in the -k option if encrypting,
and must pass a private key JWK file in the -k option if decrypting.  To be
clear, for asymmetric keys the public part of the key is required to encrypt,
and the private part required to decrypt.

For convenience, a pair of public and private keys are provided,
`key-rsa-4096.private` and `key-rsa-4096.pub`, these were produced with just

```
 $ lws-crypto-jwk -t RSA -b 4096 --public key-rsa-4096.pub >key-rsa-4096.private
```

Similar keys for EC modes may be produced with

```
 $ lws-crypto-jwk -t EC -v P-256 --public key-ecdh-p-256.pub >key-ecdh-p-256.private
```

JWSs produced with openssl and mbedtls backends are completely interchangeable.

Commandline option|Meaning
---|---
-d <loglevel>|Debug verbosity in decimal, eg, -d15
-s "<signature alg>"|Sign (default is verify), eg, -e "ES256".  For verify, the cipher information comes from the input JWS.
-k <jwk file>|JWK file to sign or verify with... sign requires the key has its private part
-c|Format the JWE as a linebroken C string
-f|Output flattened representation (instead of compact by default)

```
 $ echo -n "plaintext0123456" | ./lws-crypto-jws -s "ES256" -k ec-p256.private
[2018/12/19 16:20:25:6519] USER: LWS JWE example tool
[2018/12/19 16:20:25:6749] NOTICE: Creating Vhost 'default' (serving disabled), 1 protocols, IPv6 off
eyJhbGciOiJSU0ExXzUiLCAiZW5jIjoiQTEyOENCQy1IUzI1NiJ9.ivFr7qzx-pQ4V_edbjpdvR9OwWL9KmojPE2rXQM52oLtW0BtnxZu2_ezqhsAelyIcaworgfobs3u4bslXHMFbeJJjPb5xD0fBDe64OYXZH1NpUGTMJh9Ka4CrJ2B3xhxe7EByGAuGqmluqE0Yezj7rhSw7vlr5JAwuOJ8FaGa8aZ8ldki5G5h_S2Furlbjdcw3Rrxk7mCoMHcLoqzfZtggMPwGAMFogCqcwUo7oSLbBeGaa6hpMbfSysugseWdr8TzObQKPM52k6iVAlGwRaOg_qdLMgZiYRhHA6nFKTQd7XBbNY6qAS8sPuj7Zz344tF3RSfJ0zX_telG71sOtVv5fMpeDU-eCdpOWlCBfu6J6FQfAFu6SJryM4ajGOif09CwFI5qUQ33SOfQfS_M3nqSyd6Vu5M4lsDrb5wK7_XX5gqUwvI9wicf_8WWR-CQomRF-JvEASnA2SIf8QqYfa8R2rP9q6Md4vwO4EZrtxIsMDPsH-4ZEFu7vDjyy09QfIWWsnEb8-UgpVXensgt2m_2bZ76r1VB8-0nZLMwMyEhaH2wra9vX2FWao5UkmNJ7ht300f4_V6QzMFoePpwCvsufWBW6jcQLB-frCWe6uitWaZHEB4LxmNPKzQSz4QwwTKhpF1jNn8Xh1-w1m-2h0gj-oe-S8QBwPveqhPI1p2fI.snuhUTXHNu5mJ6dEPQqg6g.yl36qC4o0GE4nrquQ2YyCg.Vf0MoT7_kUrZdCNWXhq1DQ
```

Notice the logging is on stderr, and the output alone on stdout.

When signing, the compact representation of the JWS is output on stdout.

When verifying, if the signature is valid the plaintext is output on stdout
and the tool exits with a 0 exit code.  Otherwise nothing is output on stdout
and it exits with a nonzero exit code.


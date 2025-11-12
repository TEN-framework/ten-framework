# Lws Crypto Apis

## Overview

![lws crypto overview](/doc-assets/lws-crypto-overview.svg)

Lws provides a "generic" crypto layer on top of both OpenSSL and
compatible tls library, and mbedtls.  Using this layer, your code
can work without any changes on both types of tls library crypto
backends... it's as simple as rebuilding lws with `-DLWS_WITH_MBEDTLS=0`
or `=1` at cmake.

The generic layer can be used directly (as in, eg, the sshd plugin),
or via another layer on top, which processes JOSE JSON objects using
JWS (JSON Web Signatures), JWK (JSON Web Keys), and JWE (JSON Web
Encryption).

The `JW` apis use the generic apis (`lws_genrsa_`, etc) to get the crypto tasks
done, so anything they can do you can also get done using the generic apis.
The main difference is that with the generic apis, you must instantiate the
correct types and use type-specfic apis.  With the `JW` apis, there is only
one interface for all operations, with the details hidden in the api and
controlled by the JSON objects.

Because of this, the `JW` apis are often preferred because they give you
"crypto agility" cheaply... to change your crypto to another supported algorithm
once it's working, you literally just change your JSON defining the keys and
JWE or JWS algorithm.  (It's up to you to define your policy for which
combinations are acceptable by querying the parsed JW structs).

## Crypto supported in generic layer

### Generic Hash

 - SHA1
 - SHA256
 - SHA384
 - SHA512

### Generic HMAC

 - SHA256
 - SHA384
 - SHA512

### Generic AES

 - CBC
 - CFB128
 - CFB8
 - CTR
 - ECB
 - OFB
 - XTS
 - GCM
 - KW (Key Wrap)

### Generic RSA

 - PKCS 1.5
 - OAEP / PSS

### Generic EC

 - ECDH
 - ECDSA
 - P256 / P384 / P521 (sic) curves

## Using the generic layer

All the necessary includes are part of `libwebsockets.h`.

Enable `-DLWS_WITH_GENCRYPTO=1` at cmake.

|api|header|Functionality|
|---|---|---|
|genhash|[./include/libwebsockets/lws-genhash.h](https://libwebsockets.org/git/libwebsockets/tree/include/libwebsockets/lws-genhash.h)|Provides SHA1 + SHA2 hashes and hmac|
|genrsa|[./include/libwebsockets/lws-genrsa.h](https://libwebsockets.org/git/libwebsockets/tree/include/libwebsockets/lws-genrsa.h)|Provides RSA encryption, decryption, signing, verification, key generation and creation|
|genaes|[./include/libwebsockets/lws-genaes.h](https://libwebsockets.org/git/libwebsockets/tree/include/libwebsockets/lws-genaes.h)|Provides AES in all common variants for encryption and decryption|
|genec|[./include/libwebsockets/lws-genec.h](https://libwebsockets.org/git/libwebsockets/tree/include/libwebsockets/lws-genec.h)|Provides Elliptic Curve for encryption, decryption, signing, verification, key generation and creation|
|x509|[./include/libwebsockets/lws-x509.h](https://libwebsockets.org/git/libwebsockets/tree/include/libwebsockets/lws-x509.h)|Apis for X.509 Certificate loading, parsing, and stack verification, plus JWK key extraction from PEM X.509 certificate / private key|

Unit tests for these apis, which serve as usage examples, can be found in [./minimal-examples/api-tests/api-test-gencrypto](https://libwebsockets.org/git/libwebsockets/tree/minimal-examples/api-tests/api-test-gencrypto)

### Keys in the generic layer

The necessary types and defines are brought in by `libwebsockets.h`.

Keys are represented only by an array of `struct lws_jwk_elements`... the
length of the array is defined by the cipher... it's one of

|key elements count|definition|
|---|---|
|`LWS_COUNT_OCT_KEY_ELEMENTS`|1|
|`LWS_COUNT_RSA_KEY_ELEMENTS`|8|
|`LWS_COUNT_EC_KEY_ELEMENTS`|4|
|`LWS_COUNT_AES_KEY_ELEMENTS`|1|

`struct lws_jwk_elements` is a simple pointer / length combination used to
store arbitrary octets that make up the key element's binary representation.

## Using the JOSE layer

The JOSE (JWK / JWS / JWE) stuff is a crypto-agile JSON-based layer
that uses the gencrypto support underneath.

"Crypto Agility" means the JSON structs include information about the
algorithms and ciphers used in that particular object, making it easy to
upgrade system crypto strength or cycle keys over time while supporting a
transitional period where the old and new keys or algorithms + ciphers
are also valid.

Uniquely lws generic support means the JOSE stuff also has "tls library
agility", code written to the lws generic or JOSE apis is completely unchanged
even if the underlying tls library changes between OpenSSL and mbedtls, meaning
sharing code between server and client sides is painless.

All the necessary includes are part of `libwebsockets.h`.

Enable `-DLWS_WITH_JOSE=1` at CMake.

|api|header|Functionality|
|---|---|---|
|JOSE|[./include/libwebsockets/lws-jose.h](https://libwebsockets.org/git/libwebsockets/tree/include/libwebsockets/lws-jose.h)|Provides crypto agility for JWS / JWE|
|JWE|[./include/libwebsockets/lws-jwe.h](https://libwebsockets.org/git/libwebsockets/tree/include/libwebsockets/lws-jwe.h)|Provides Encryption and Decryption services for RFC7516 JWE JSON|
|JWS|[./include/libwebsockets/lws-jws.h](https://libwebsockets.org/git/libwebsockets/tree/include/libwebsockets/lws-jws.h)|Provides signature and verifcation services for RFC7515 JWS JSON|
|JWK|[./include/libwebsockets/lws-jwk.h](https://libwebsockets.org/git/libwebsockets/tree/include/libwebsockets/lws-jwk.h)|Provides signature and verifcation services for RFC7517 JWK JSON, both "keys" arrays and singletons|

Minimal examples are provided in the form of commandline tools for JWK / JWS / JWE / x509 handling:

 - [JWK minimal example](https://libwebsockets.org/git/libwebsockets/tree/minimal-examples/crypto/minimal-crypto-jwk)
 - [JWS minimal example](https://libwebsockets.org/git/libwebsockets/tree/minimal-examples/crypto/minimal-crypto-jws)
 - [JWE minimal example](https://libwebsockets.org/git/libwebsockets/tree/minimal-examples/crypto/minimal-crypto-jwe)
 - [X509 minimal example](https://libwebsockets.org/git/libwebsockets/tree/minimal-examples/crypto/minimal-crypto-x509)

Unit tests for these apis, which serve as usage examples, can be found in [./minimal-examples/api-tests/api-test-jose](https://libwebsockets.org/git/libwebsockets/tree/minimal-examples/api-tests/api-test-jose)

## Crypto supported in the JOSE layer

The JOSE RFCs define specific short names for different algorithms

### JWS

|JSOE name|Hash|Signature|
---|---|---
|RS256, RS384, RS512|SHA256/384/512|RSA
|ES256, ES384, ES521|SHA256/384/512|EC

### JWE

|Key Encryption|Payload authentication + crypt|
|---|---|
|`RSAES-PKCS1-v1.5` 2048b & 4096b|`AES_128_CBC_HMAC_SHA_256`|
|`RSAES-PKCS1-v1.5` 2048b|`AES_192_CBC_HMAC_SHA_384`|
|`RSAES-PKCS1-v1.5` 2048b|`AES_256_CBC_HMAC_SHA_512`|
|`RSAES-OAEP`|`AES_256_GCM`|
|`AES128KW`, `AES192KW`, `AES256KW`|`AES_128_CBC_HMAC_SHA_256`|
|`AES128KW`, `AES192KW`, `AES256KW`|`AES_192_CBC_HMAC_SHA_384`|
|`AES128KW`, `AES192KW`, `AES256KW`|`AES_256_CBC_HMAC_SHA_512`|
|`ECDH-ES` (P-256/384/521 key)|`AES_128/192/256_GCM`|
|`ECDH-ES+A128/192/256KW` (P-256/384/521 key)|`AES_128/192/256_GCM`|

### Keys in the JOSE layer

Keys in the JOSE layer use a `struct lws_jwk`, this contains two arrays of
`struct lws_jwk_elements` sized for the worst case (currently RSA).  One
array contains the key elements as described for the generic case, and the
other contains various nonencrypted key metadata taken from JWK JSON.

|metadata index|function|
|---|---|
|`JWK_META_KTY`|Key type, eg, "EC"|
|`JWK_META_KID`|Arbitrary ID string|
|`JWK_META_USE`|What the public key may be used to validate, "enc" or "sig"|
|`JWK_META_KEY_OPS`|Which operations the key is authorized for, eg, "encrypt"|
|`JWK_META_X5C`|Optional X.509 cert version of the key|
|`JWK_META_ALG`|Optional overall crypto algorithm the key is intended for use with|

`lws_jwk_destroy()` should be called when the jwk is going out of scope... this
takes care to zero down any key element data in the jwk.


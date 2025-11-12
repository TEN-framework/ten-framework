# JOSE support

JOSE is a set of web standards aimed at encapsulating crypto
operations flexibly inside JSON objects.

Lws provides lightweight apis to performs operations on JWK, JWS and JWE
independent of the tls backend in use.  The JSON parsing is handled by the lws
lejp stream parser.

|Part|RFC|Function|
|---|---|---|
|JWS|[RFC7515](https://tools.ietf.org/html/rfc7515)|JSON Web Signatures|
|JWE|[RFC7516](https://tools.ietf.org/html/rfc7516)|JSON Web Encryption|
|JWK|[RFC7517](https://tools.ietf.org/html/rfc7517)|JSON Web Keys|
|JWA|[RFC7518](https://tools.ietf.org/html/rfc7518)|JSON Web Algorithms|

JWA is a set of recommendations for which combinations of algorithms
are deemed desirable and secure, which implies what must be done for
useful implementations of JWS, JWE and JWK.

## Supported algorithms

### Supported keys

 - All RFC7517 / JWK forms: octet, RSA and EC

 - singleton and keys[] arrays of keys supported

### Symmetric ciphers

 - All common AES varaiants: CBC, CFB128, CFB8, CTR, EVB, OFB, KW and XTS

### Asymmetric ciphers

 - RSA

 - EC (P-256, P-384 and P-521 JWA curves)

### Payload auth and crypt

 - AES_128_CBC_HMAC_SHA_256
 - AES_192_CBC_HMAC_SHA_384
 - AES_256_CBC_HMAC_SHA_512
 - AES_128_GCM

For the required and recommended asymmetric algorithms, support currently
looks like this

|JWK kty|JWA|lws|
|---|---|---|
|EC|Recommended+|yes|
|RSA|Required|yes|
|oct|Required|yes|

|JWE alg|JWA|lws|
|---|---|---|
|RSA1_5|Recommended-|yes|
|RSA-OAEP|Recommended+|no|
|ECDH-ES|Recommended+|no|

|JWS alg|JWA|lws|
|---|---|---|
|HS256|Required|yes|
|RS256|Recommended+|yes|
|ES256|Recommended|yes|

## Minimal Example tools

[JWK](https://libwebsockets.org/git/libwebsockets/tree/minimal-examples/crypto/minimal-crypto-jwk)

[JWS](https://libwebsockets.org/git/libwebsockets/tree/minimal-examples/crypto/minimal-crypto-jws)

[JWE](https://libwebsockets.org/git/libwebsockets/tree/minimal-examples/crypto/minimal-crypto-jwe)

## API tests

See `./minimal-examples/api-tests/api-test-jose/` for example test code.
The tests are built and confirmed during CI.


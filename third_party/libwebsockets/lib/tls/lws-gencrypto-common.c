/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2010 - 2019 Andy Green <andy@warmcat.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "private-lib-core.h"

/*
 * These came from RFC7518 (JSON Web Algorithms) Section 3
 *
 * Cryptographic Algorithms for Digital Signatures and MACs
 */

static const struct lws_jose_jwe_alg lws_gencrypto_jws_alg_map[] = {

	/*
	 * JWSs MAY also be created that do not provide integrity protection.
	 * Such a JWS is called an Unsecured JWS.  An Unsecured JWS uses the
	 * "alg" value "none" and is formatted identically to other JWSs, but
	 * MUST use the empty octet sequence as its JWS Signature value.
	 * Recipients MUST verify that the JWS Signature value is the empty
	 * octet sequence.
	 *
	 * Implementations that support Unsecured JWSs MUST NOT accept such
	 * objects as valid unless the application specifies that it is
	 * acceptable for a specific object to not be integrity protected.
	 * Implementations MUST NOT accept Unsecured JWSs by default.  In order
	 * to mitigate downgrade attacks, applications MUST NOT signal
	 * acceptance of Unsecured JWSs at a global level, and SHOULD signal
	 * acceptance on a per-object basis.  See Section 8.5 for security
	 * considerations associated with using this algorithm.
	 */
	{	/* optional */
		LWS_GENHASH_TYPE_UNKNOWN,
		LWS_GENHMAC_TYPE_UNKNOWN,
		LWS_JOSE_ENCTYPE_NONE,
		LWS_JOSE_ENCTYPE_NONE,
		"none", NULL, 0, 0, 0
	},

	/*
	 * HMAC with SHA-2 Functions
	 *
	 * The HMAC SHA-256 MAC for a JWS is validated by computing an HMAC
	 * value per RFC 2104, using SHA-256 as the hash algorithm "H", using
	 * the received JWS Signing Input as the "text" value, and using the
	 * shared key.  This computed HMAC value is then compared to the result
	 * of base64url decoding the received encoded JWS Signature value.  The
	 * comparison of the computed HMAC value to the JWS Signature value MUST
	 * be done in a constant-time manner to thwart timing attacks.
	 *
	 * Alternatively, the computed HMAC value can be base64url encoded and
	 * compared to the received encoded JWS Signature value (also in a
	 * constant-time manner), as this comparison produces the same result as
	 * comparing the unencoded values.  In either case, if the values match,
	 * the HMAC has been validated.
	 */

	{	/* required: HMAC using SHA-256 */
		LWS_GENHASH_TYPE_UNKNOWN,
		LWS_GENHMAC_TYPE_SHA256,
		LWS_JOSE_ENCTYPE_NONE,
		LWS_JOSE_ENCTYPE_NONE,
		"HS256", NULL, 0, 0, 0
	},
	{	/* optional: HMAC using SHA-384 */
		LWS_GENHASH_TYPE_UNKNOWN,
		LWS_GENHMAC_TYPE_SHA384,
		LWS_JOSE_ENCTYPE_NONE,
		LWS_JOSE_ENCTYPE_NONE,
		"HS384", NULL, 0, 0, 0
	},
	{	/* optional: HMAC using SHA-512 */
		LWS_GENHASH_TYPE_UNKNOWN,
		LWS_GENHMAC_TYPE_SHA512,
		LWS_JOSE_ENCTYPE_NONE,
		LWS_JOSE_ENCTYPE_NONE,
		"HS512", NULL, 0, 0, 0
	},

	/*
	 * Digital Signature with RSASSA-PKCS1-v1_5
	 *
	 * This section defines the use of the RSASSA-PKCS1-v1_5 digital
	 * signature algorithm as defined in Section 8.2 of RFC 3447 [RFC3447]
	 * (commonly known as PKCS #1), using SHA-2 [SHS] hash functions.
	 *
	 * A key of size 2048 bits or larger MUST be used with these algorithms.
	 *
	 * The RSASSA-PKCS1-v1_5 SHA-256 digital signature is generated as
	 * follows: generate a digital signature of the JWS Signing Input using
	 * RSASSA-PKCS1-v1_5-SIGN and the SHA-256 hash function with the desired
	 * private key.  This is the JWS Signature value.
	 *
	 * The RSASSA-PKCS1-v1_5 SHA-256 digital signature for a JWS is
	 * validated as follows: submit the JWS Signing Input, the JWS
	 * Signature, and the public key corresponding to the private key used
	 * by the signer to the RSASSA-PKCS1-v1_5-VERIFY algorithm using SHA-256
	 * as the hash function.
	 */

	{	/* recommended: RSASSA-PKCS1-v1_5 using SHA-256 */
		LWS_GENHASH_TYPE_SHA256,
		LWS_GENHMAC_TYPE_UNKNOWN,
		LWS_JOSE_ENCTYPE_RSASSA_PKCS1_1_5,
		LWS_JOSE_ENCTYPE_NONE,
		"RS256", NULL, 2048, 4096, 0
	},
	{	/* optional: RSASSA-PKCS1-v1_5 using SHA-384 */
		LWS_GENHASH_TYPE_SHA384,
		LWS_GENHMAC_TYPE_UNKNOWN,
		LWS_JOSE_ENCTYPE_RSASSA_PKCS1_1_5,
		LWS_JOSE_ENCTYPE_NONE,
		"RS384", NULL, 2048, 4096, 0
	},
	{	/* optional: RSASSA-PKCS1-v1_5 using SHA-512 */
		LWS_GENHASH_TYPE_SHA512,
		LWS_GENHMAC_TYPE_UNKNOWN,
		LWS_JOSE_ENCTYPE_RSASSA_PKCS1_1_5,
		LWS_JOSE_ENCTYPE_NONE,
		"RS512", NULL, 2048, 4096, 0
	},

	/*
	 * Digital Signature with ECDSA
	 *
	 * The ECDSA P-256 SHA-256 digital signature is generated as follows:
	 *
	 * 1.  Generate a digital signature of the JWS Signing Input using ECDSA
	 *     P-256 SHA-256 with the desired private key.  The output will be
	 *     the pair (R, S), where R and S are 256-bit unsigned integers.
	 * 2.  Turn R and S into octet sequences in big-endian order, with each
	 *     array being be 32 octets long.  The octet sequence
	 *     representations MUST NOT be shortened to omit any leading zero
	 *     octets contained in the values.
	 *
	 * 3.  Concatenate the two octet sequences in the order R and then S.
	 *     (Note that many ECDSA implementations will directly produce this
	 *     concatenation as their output.)
	 *
	 * 4.  The resulting 64-octet sequence is the JWS Signature value.
	 *
	 * The ECDSA P-256 SHA-256 digital signature for a JWS is validated as
	 * follows:
	 *
	 * 1.  The JWS Signature value MUST be a 64-octet sequence.  If it is
	 *     not a 64-octet sequence, the validation has failed.
	 *
	 * 2.  Split the 64-octet sequence into two 32-octet sequences.  The
	 *     first octet sequence represents R and the second S.  The values R
	 *     and S are represented as octet sequences using the Integer-to-
	 *     OctetString Conversion defined in Section 2.3.7 of SEC1 [SEC1]
	 *     (in big-endian octet order).
	 * 3.  Submit the JWS Signing Input, R, S, and the public key (x, y) to
	 *     the ECDSA P-256 SHA-256 validator.
	 */

	{	/* Recommended+: ECDSA using P-256 and SHA-256 */
		LWS_GENHASH_TYPE_SHA256,
		LWS_GENHMAC_TYPE_UNKNOWN,
		LWS_JOSE_ENCTYPE_ECDSA,
		LWS_JOSE_ENCTYPE_NONE,
		"ES256", "P-256", 256, 256, 0
	},
	{	/* optional: ECDSA using P-384 and SHA-384 */
		LWS_GENHASH_TYPE_SHA384,
		LWS_GENHMAC_TYPE_UNKNOWN,
		LWS_JOSE_ENCTYPE_ECDSA,
		LWS_JOSE_ENCTYPE_NONE,
		"ES384", "P-384", 384, 384, 0
	},
	{	/* optional: ECDSA using P-521 and SHA-512 */
		LWS_GENHASH_TYPE_SHA512,
		LWS_GENHMAC_TYPE_UNKNOWN,
		LWS_JOSE_ENCTYPE_ECDSA,
		LWS_JOSE_ENCTYPE_NONE,
		"ES512", "P-521", 521, 521, 0
	},
#if 0
	Not yet supported

	/*
	 * Digital Signature with RSASSA-PSS
	 *
	 * A key of size 2048 bits or larger MUST be used with this algorithm.
	 *
	 * The RSASSA-PSS SHA-256 digital signature is generated as follows:
	 * generate a digital signature of the JWS Signing Input using RSASSA-
	 * PSS-SIGN, the SHA-256 hash function, and the MGF1 mask generation
	 * function with SHA-256 with the desired private key.  This is the JWS
	 * Signature value.
	 *
	 * The RSASSA-PSS SHA-256 digital signature for a JWS is validated as
	 * follows: submit the JWS Signing Input, the JWS Signature, and the
	 * public key corresponding to the private key used by the signer to the
	 * RSASSA-PSS-VERIFY algorithm using SHA-256 as the hash function and
	 * using MGF1 as the mask generation function with SHA-256.
	 *
	 */
	{	/* optional: RSASSA-PSS using SHA-256 and MGF1 with SHA-256 */
		LWS_GENHASH_TYPE_SHA256,
		LWS_GENHMAC_TYPE_UNKNOWN,
		LWS_JOSE_ENCTYPE_RSASSA_PKCS1_PSS,
		LWS_JOSE_ENCTYPE_NONE,
		"PS256", NULL, 2048, 4096, 0
	},
	{	/* optional: RSASSA-PSS using SHA-384 and MGF1 with SHA-384 */
		LWS_GENHASH_TYPE_SHA384,
		LWS_GENHMAC_TYPE_UNKNOWN,
		LWS_JOSE_ENCTYPE_RSASSA_PKCS1_PSS,
		LWS_JOSE_ENCTYPE_NONE,
		"PS384", NULL, 2048, 4096, 0
	},
	{	/* optional: RSASSA-PSS using SHA-512 and MGF1 with SHA-512*/
		LWS_GENHASH_TYPE_SHA512,
		LWS_GENHMAC_TYPE_UNKNOWN,
		LWS_JOSE_ENCTYPE_RSASSA_PKCS1_PSS,
		LWS_JOSE_ENCTYPE_NONE,
		"PS512", NULL, 2048, 4096, 0
	},
#endif
	/* list terminator */
	{ 0, 0, 0, 0, NULL, NULL, 0, 0, 0}
};

/*
 * These came from RFC7518 (JSON Web Algorithms) Section 4
 *
 * Cryptographic Algorithms for Key Management
 *
 * JWE uses cryptographic algorithms to encrypt or determine the Content
 * Encryption Key (CEK).
 */

static const struct lws_jose_jwe_alg lws_gencrypto_jwe_alg_map[] = {

	/*
	 * This section defines the specifics of encrypting a JWE CEK with
	 * RSAES-PKCS1-v1_5 [RFC3447].  The "alg" (algorithm) Header Parameter
	 * value "RSA1_5" is used for this algorithm.
	 *
	 * A key of size 2048 bits or larger MUST be used with this algorithm.
	 */

	{	/* recommended-: RSAES-PKCS1-v1_5 */
		LWS_GENHASH_TYPE_SHA256,
		LWS_GENHMAC_TYPE_UNKNOWN,
		LWS_JOSE_ENCTYPE_RSASSA_PKCS1_1_5,
		LWS_JOSE_ENCTYPE_NONE,
		"RSA1_5", NULL, 2048, 4096, 0
	},
	{	/* recommended+: RSAES OAEP using default parameters */
		LWS_GENHASH_TYPE_SHA1,
		LWS_GENHMAC_TYPE_UNKNOWN,
		LWS_JOSE_ENCTYPE_RSASSA_PKCS1_OAEP,
		LWS_JOSE_ENCTYPE_NONE,
		"RSA-OAEP", NULL, 2048, 4096, 0
	},
	{	/* recommended+: RSAES OAEP using SHA-256 and MGF1 SHA-256 */
		LWS_GENHASH_TYPE_SHA256,
		LWS_GENHMAC_TYPE_UNKNOWN,
		LWS_JOSE_ENCTYPE_RSASSA_PKCS1_OAEP,
		LWS_JOSE_ENCTYPE_NONE,
		"RSA-OAEP-256", NULL, 2048, 4096, 0
	},

	/*
	 * Key Wrapping with AES Key Wrap
	 *
	 * This section defines the specifics of encrypting a JWE CEK with the
	 * Advanced Encryption Standard (AES) Key Wrap Algorithm [RFC3394] using
	 * the default initial value specified in Section 2.2.3.1 of that
	 * document.
	 *
	 *
	 */
	{	/* recommended: AES Key Wrap with AES Key Wrap with defaults
				using 128-bit key  */
		LWS_GENHASH_TYPE_UNKNOWN,
		LWS_GENHMAC_TYPE_UNKNOWN,
		LWS_JOSE_ENCTYPE_AES_ECB,
		LWS_JOSE_ENCTYPE_NONE,
		"A128KW", NULL, 128, 128, 64
	},

	{	/* optional: AES Key Wrap with AES Key Wrap with defaults
				using 192-bit key */
		LWS_GENHASH_TYPE_UNKNOWN,
		LWS_GENHMAC_TYPE_UNKNOWN,
		LWS_JOSE_ENCTYPE_AES_ECB,
		LWS_JOSE_ENCTYPE_NONE,
		"A192KW", NULL, 192, 192, 64
	},

	{	/* recommended: AES Key Wrap with AES Key Wrap with defaults
				using 256-bit key */
		LWS_GENHASH_TYPE_UNKNOWN,
		LWS_GENHMAC_TYPE_UNKNOWN,
		LWS_JOSE_ENCTYPE_AES_ECB,
		LWS_JOSE_ENCTYPE_NONE,
		"A256KW", NULL, 256, 256, 64
	},

	/*
	 * This section defines the specifics of directly performing symmetric
	 * key encryption without performing a key wrapping step.  In this case,
	 * the shared symmetric key is used directly as the Content Encryption
	 * Key (CEK) value for the "enc" algorithm.  An empty octet sequence is
	 * used as the JWE Encrypted Key value.  The "alg" (algorithm) Header
	 * Parameter value "dir" is used in this case.
	 */
	{	/* recommended */
		LWS_GENHASH_TYPE_UNKNOWN,
		LWS_GENHMAC_TYPE_UNKNOWN,
		LWS_JOSE_ENCTYPE_NONE,
		LWS_JOSE_ENCTYPE_NONE,
		"dir", NULL, 0, 0, 0
	},

	/*
	 * Key Agreement with Elliptic Curve Diffie-Hellman Ephemeral Static
	 * (ECDH-ES)
	 *
	 * This section defines the specifics of key agreement with Elliptic
	 * Curve Diffie-Hellman Ephemeral Static [RFC6090], in combination with
	 * the Concat KDF, as defined in Section 5.8.1 of [NIST.800-56A].  The
	 * key agreement result can be used in one of two ways:
	 *
	 * 1.  directly as the Content Encryption Key (CEK) for the "enc"
	 *     algorithm, in the Direct Key Agreement mode, or
	 *
	 * 2.  as a symmetric key used to wrap the CEK with the "A128KW",
	 *     "A192KW", or "A256KW" algorithms, in the Key Agreement with Key
	 *     Wrapping mode.
	 *
	 * A new ephemeral public key value MUST be generated for each key
	 * agreement operation.
	 *
	 * In Direct Key Agreement mode, the output of the Concat KDF MUST be a
	 * key of the same length as that used by the "enc" algorithm.  In this
	 * case, the empty octet sequence is used as the JWE Encrypted Key
	 * value.  The "alg" (algorithm) Header Parameter value "ECDH-ES" is
	 * used in the Direct Key Agreement mode.
	 *
	 * In Key Agreement with Key Wrapping mode, the output of the Concat KDF
	 * MUST be a key of the length needed for the specified key wrapping
	 * algorithm.  In this case, the JWE Encrypted Key is the CEK wrapped
	 * with the agreed-upon key.
	 */

	{	/* recommended+: ECDH Ephemeral Static Key agreement Concat KDF */
		LWS_GENHASH_TYPE_SHA256,
		LWS_GENHMAC_TYPE_UNKNOWN,
		LWS_JOSE_ENCTYPE_ECDHES,
		LWS_JOSE_ENCTYPE_NONE,
		"ECDH-ES", NULL, 128, 128, 0
	},
	{	/* recommended: ECDH-ES + Concat KDF + wrapped by AES128KW */
		LWS_GENHASH_TYPE_SHA256,
		LWS_GENHMAC_TYPE_UNKNOWN,
		LWS_JOSE_ENCTYPE_ECDHES,
		LWS_JOSE_ENCTYPE_AES_ECB,
		"ECDH-ES+A128KW", NULL, 128, 128, 0
	},
	{	/* optional: ECDH-ES + Concat KDF + wrapped by AES192KW */
		LWS_GENHASH_TYPE_SHA256,
		LWS_GENHMAC_TYPE_UNKNOWN,
		LWS_JOSE_ENCTYPE_ECDHES,
		LWS_JOSE_ENCTYPE_AES_ECB,
		"ECDH-ES+A192KW", NULL, 192, 192, 0
	},
	{	/* recommended: ECDH-ES + Concat KDF + wrapped by AES256KW */
		LWS_GENHASH_TYPE_SHA256,
		LWS_GENHMAC_TYPE_UNKNOWN,
		LWS_JOSE_ENCTYPE_ECDHES,
		LWS_JOSE_ENCTYPE_AES_ECB,
		"ECDH-ES+A256KW", NULL, 256, 256, 0
	},

	/*
	 * Key Encryption with AES GCM
	 *
	 *  This section defines the specifics of encrypting a JWE Content
	 *  Encryption Key (CEK) with Advanced Encryption Standard (AES) in
	 *  Galois/Counter Mode (GCM) ([AES] and [NIST.800-38D]).
	 *
	 * Use of an Initialization Vector (IV) of size 96 bits is REQUIRED with
	 * this algorithm.  The IV is represented in base64url-encoded form as
	 * the "iv" (initialization vector) Header Parameter value.
	 *
	 * The Additional Authenticated Data value used is the empty octet
	 * string.
	 *
	 * The requested size of the Authentication Tag output MUST be 128 bits,
	 * regardless of the key size.
	 *
	 * The JWE Encrypted Key value is the ciphertext output.
	 *
	 * The Authentication Tag output is represented in base64url-encoded
	 * form as the "tag" (authentication tag) Header Parameter value.
	 *
	 *
	 * "iv" (Initialization Vector) Header Parameter
	 *
	 * The "iv" (initialization vector) Header Parameter value is the
	 * base64url-encoded representation of the 96-bit IV value used for the
	 * key encryption operation.  This Header Parameter MUST be present and
	 * MUST be understood and processed by implementations when these
	 * algorithms are used.
	 *
	 * "tag" (Authentication Tag) Header Parameter
	 *
	 * The "tag" (authentication tag) Header Parameter value is the
	 * base64url-encoded representation of the 128-bit Authentication Tag
	 * value resulting from the key encryption operation.  This Header
	 * Parameter MUST be present and MUST be understood and processed by
	 * implementations when these algorithms are used.
	 */
	{	/* optional: Key wrapping with AES GCM using 128-bit key  */
		LWS_GENHASH_TYPE_UNKNOWN,
		LWS_GENHMAC_TYPE_UNKNOWN,
		LWS_JOSE_ENCTYPE_AES_ECB,
		LWS_JOSE_ENCTYPE_NONE,
		"A128GCMKW", NULL, 128, 128, 96
	},

	{	/* optional: Key wrapping with AES GCM using 192-bit key */
		LWS_GENHASH_TYPE_UNKNOWN,
		LWS_GENHMAC_TYPE_UNKNOWN,
		LWS_JOSE_ENCTYPE_AES_ECB,
		LWS_JOSE_ENCTYPE_NONE,
		"A192GCMKW", NULL, 192, 192, 96
	},

	{	/* optional: Key wrapping with AES GCM using 256-bit key */
		LWS_GENHASH_TYPE_UNKNOWN,
		LWS_GENHMAC_TYPE_UNKNOWN,
		LWS_JOSE_ENCTYPE_AES_ECB,
		LWS_JOSE_ENCTYPE_NONE,
		"A256GCMKW", NULL, 256, 256, 96
	},

	/* list terminator */
	{ 0, 0, 0, 0, NULL, NULL, 0, 0, 0 }
};

/*
 * The "enc" (encryption algorithm) Header Parameter identifies the
 * content encryption algorithm used to perform authenticated encryption
 * on the plaintext to produce the ciphertext and the Authentication
 * Tag.  This algorithm MUST be an AEAD algorithm with a specified key
 * length.  The encrypted content is not usable if the "enc" value does
 * not represent a supported algorithm.  "enc" values should either be
 * registered in the IANA "JSON Web Signature and Encryption Algorithms"
 * registry established by [JWA] or be a value that contains a
 * Collision-Resistant Name.  The "enc" value is a case-sensitive ASCII
 * string containing a StringOrURI value.  This Header Parameter MUST be
 * present and MUST be understood and processed by implementations.
 */

static const struct lws_jose_jwe_alg lws_gencrypto_jwe_enc_map[] = {
	/*
	 * AES_128_CBC_HMAC_SHA_256 / 512
	 *
	 * It uses the HMAC message authentication code [RFC2104] with the
	 * SHA-256 hash function [SHS] to provide message authentication, with
	 * the HMAC output truncated to 128 bits, corresponding to the
	 * HMAC-SHA-256-128 algorithm defined in [RFC4868].  For encryption, it
	 * uses AES in the CBC mode of operation as defined in Section 6.2 of
	 * [NIST.800-38A], with PKCS #7 padding and a 128-bit IV value.
	 *
	 * The AES_CBC_HMAC_SHA2 parameters specific to AES_128_CBC_HMAC_SHA_256
	 * are:
	 *
	 * The input key K is 32 octets long.
	 *       ENC_KEY_LEN is 16 octets.
	 *       MAC_KEY_LEN is 16 octets.
	 *       The SHA-256 hash algorithm is used for the HMAC.
	 *       The HMAC-SHA-256 output is truncated to T_LEN=16 octets, by
	 *       stripping off the final 16 octets.
	 */
	{	/* required */
		LWS_GENHASH_TYPE_UNKNOWN,
		LWS_GENHMAC_TYPE_SHA256,
		LWS_JOSE_ENCTYPE_NONE,
		LWS_JOSE_ENCTYPE_AES_CBC,
		"A128CBC-HS256", NULL, 256, 256, 128
	},
	/*
	 * AES_192_CBC_HMAC_SHA_384 is based on AES_128_CBC_HMAC_SHA_256, but
	 * with the following differences:
	 *
	 * The input key K is 48 octets long instead of 32.
	 * ENC_KEY_LEN is 24 octets instead of 16.
	 * MAC_KEY_LEN is 24 octets instead of 16.
	 * SHA-384 is used for the HMAC instead of SHA-256.
	 * The HMAC SHA-384 value is truncated to T_LEN=24 octets instead of 16.
	 */
	{	/* required */
		LWS_GENHASH_TYPE_UNKNOWN,
		LWS_GENHMAC_TYPE_SHA384,
		LWS_JOSE_ENCTYPE_NONE,
		LWS_JOSE_ENCTYPE_AES_CBC,
		"A192CBC-HS384", NULL, 384, 384, 192
	},
	/*
	 * AES_256_CBC_HMAC_SHA_512 is based on AES_128_CBC_HMAC_SHA_256, but
	 * with the following differences:
	 *
	 * The input key K is 64 octets long instead of 32.
	 * ENC_KEY_LEN is 32 octets instead of 16.
	 * MAC_KEY_LEN is 32 octets instead of 16.
	 * SHA-512 is used for the HMAC instead of SHA-256.
	 * The HMAC SHA-512 value is truncated to T_LEN=32 octets instead of 16.
	 */
	{	/* required */
		LWS_GENHASH_TYPE_UNKNOWN,
		LWS_GENHMAC_TYPE_SHA512,
		LWS_JOSE_ENCTYPE_NONE,
		LWS_JOSE_ENCTYPE_AES_CBC,
		"A256CBC-HS512", NULL, 512, 512, 256
	},

	/*
	 * The CEK is used as the encryption key.
	 *
	 * Use of an IV of size 96 bits is REQUIRED with this algorithm.
	 *
	 * The requested size of the Authentication Tag output MUST be 128 bits,
	 * regardless of the key size.
	 */
	{	/* recommended: AES GCM using 128-bit key  */
		LWS_GENHASH_TYPE_UNKNOWN,
		LWS_GENHMAC_TYPE_UNKNOWN,
		LWS_JOSE_ENCTYPE_NONE,
		LWS_JOSE_ENCTYPE_AES_GCM,
		"A128GCM", NULL, 128, 128, 96
	},
	{	/* optional: AES GCM using 192-bit key  */
		LWS_GENHASH_TYPE_UNKNOWN,
		LWS_GENHMAC_TYPE_UNKNOWN,
		LWS_JOSE_ENCTYPE_NONE,
		LWS_JOSE_ENCTYPE_AES_GCM,
		"A192GCM", NULL, 192, 192, 96
	},
	{	/* recommended: AES GCM using 256-bit key */
		LWS_GENHASH_TYPE_UNKNOWN,
		LWS_GENHMAC_TYPE_UNKNOWN,
		LWS_JOSE_ENCTYPE_NONE,
		LWS_JOSE_ENCTYPE_AES_GCM,
		"A256GCM", NULL, 256, 256, 96
	},
	{ 0, 0, 0, 0, NULL, NULL, 0, 0, 0 } /* sentinel */
};

int
lws_gencrypto_jws_alg_to_definition(const char *alg,
				    const struct lws_jose_jwe_alg **jose)
{
	const struct lws_jose_jwe_alg *a = lws_gencrypto_jws_alg_map;

	while (a->alg) {
		if (!strcmp(alg, a->alg)) {
			*jose = a;

			return 0;
		}
		a++;
	}

	return 1;
}

int
lws_gencrypto_jwe_alg_to_definition(const char *alg,
				    const struct lws_jose_jwe_alg **jose)
{
	const struct lws_jose_jwe_alg *a = lws_gencrypto_jwe_alg_map;

	while (a->alg) {
		if (!strcmp(alg, a->alg)) {
			*jose = a;

			return 0;
		}
		a++;
	}

	return 1;
}

int
lws_gencrypto_jwe_enc_to_definition(const char *enc,
				    const struct lws_jose_jwe_alg **jose)
{
	const struct lws_jose_jwe_alg *e = lws_gencrypto_jwe_enc_map;

	while (e->alg) {
		if (!strcmp(enc, e->alg)) {
			*jose = e;

			return 0;
		}
		e++;
	}

	return 1;
}

size_t
lws_genhash_size(enum lws_genhash_types type)
{
	switch(type) {
	case LWS_GENHASH_TYPE_UNKNOWN:
		return 0;
	case LWS_GENHASH_TYPE_MD5:
		return 16;
	case LWS_GENHASH_TYPE_SHA1:
		return 20;
	case LWS_GENHASH_TYPE_SHA256:
		return 32;
	case LWS_GENHASH_TYPE_SHA384:
		return 48;
	case LWS_GENHASH_TYPE_SHA512:
		return 64;
	}

	return 0;
}

size_t
lws_genhmac_size(enum lws_genhmac_types type)
{
	switch(type) {
	case LWS_GENHMAC_TYPE_UNKNOWN:
		return 0;
	case LWS_GENHMAC_TYPE_SHA256:
		return 32;
	case LWS_GENHMAC_TYPE_SHA384:
		return 48;
	case LWS_GENHMAC_TYPE_SHA512:
		return 64;
	}

	return 0;
}

int
lws_gencrypto_bits_to_bytes(int bits)
{
	if (bits & 7)
		return (bits / 8) + 1;

	return bits / 8;
}

int
lws_base64_size(int bytes)
{
	return ((bytes * 4) / 3) + 6;
}

void
lws_gencrypto_destroy_elements(struct lws_gencrypto_keyelem *el, int m)
{
	int n;

	for (n = 0; n < m; n++)
		if (el[n].buf)
			lws_free_set_NULL(el[n].buf);
}

size_t lws_gencrypto_padded_length(size_t pad_block_size, size_t len)
{
	return (len / pad_block_size + 1) * pad_block_size;
}

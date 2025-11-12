/** Code to exercise a PSA key object, i.e. validate that it seems well-formed
 * and can do what it is supposed to do.
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifndef PSA_EXERCISE_KEY_H
#define PSA_EXERCISE_KEY_H

#include "test/helpers.h"
#include "test/psa_crypto_helpers.h"

#include <psa/crypto.h>

/** \def KNOWN_SUPPORTED_HASH_ALG
 *
 * A hash algorithm that is known to be supported.
 *
 * This is used in some smoke tests.
 */
#if defined(PSA_WANT_ALG_MD5)
#define KNOWN_SUPPORTED_HASH_ALG PSA_ALG_MD5
/* MBEDTLS_RIPEMD160_C omitted. This is necessary for the sake of
 * exercise_signature_key() because Mbed TLS doesn't support RIPEMD160
 * in RSA PKCS#1v1.5 signatures. A RIPEMD160-only configuration would be
 * implausible anyway. */
#elif defined(PSA_WANT_ALG_SHA_1)
#define KNOWN_SUPPORTED_HASH_ALG PSA_ALG_SHA_1
#elif defined(PSA_WANT_ALG_SHA_256)
#define KNOWN_SUPPORTED_HASH_ALG PSA_ALG_SHA_256
#elif defined(PSA_WANT_ALG_SHA_384)
#define KNOWN_SUPPORTED_HASH_ALG PSA_ALG_SHA_384
#elif defined(PSA_WANT_ALG_SHA_512)
#define KNOWN_SUPPORTED_HASH_ALG PSA_ALG_SHA_512
#elif defined(PSA_WANT_ALG_SHA3_256)
#define KNOWN_SUPPORTED_HASH_ALG PSA_ALG_SHA3_256
#else
#undef KNOWN_SUPPORTED_HASH_ALG
#endif

/** \def KNOWN_MBEDTLS_SUPPORTED_HASH_ALG
 *
 * A hash algorithm that is known to be supported by Mbed TLS APIs.
 *
 * This is used in some smoke tests where the hash algorithm is used as
 * part of another algorithm like a signature algorithm and the hashing is
 * completed through an Mbed TLS hash API, not the PSA one.
 */
#if defined(MBEDTLS_MD5_C)
#define KNOWN_MBEDTLS_SUPPORTED_HASH_ALG PSA_ALG_MD5
/* MBEDTLS_RIPEMD160_C omitted. This is necessary for the sake of
 * exercise_signature_key() because Mbed TLS doesn't support RIPEMD160
 * in RSA PKCS#1v1.5 signatures. A RIPEMD160-only configuration would be
 * implausible anyway. */
#elif defined(MBEDTLS_SHA1_C)
#define KNOWN_MBEDTLS_SUPPORTED_HASH_ALG PSA_ALG_SHA_1
#elif defined(MBEDTLS_SHA256_C)
#define KNOWN_MBEDTLS_SUPPORTED_HASH_ALG PSA_ALG_SHA_256
#elif defined(MBEDTLS_SHA512_C)
#define KNOWN_MBEDTLS_SUPPORTED_HASH_ALG PSA_ALG_SHA_512
#else
#undef KNOWN_MBEDLTS_SUPPORTED_HASH_ALG
#endif

/** \def KNOWN_SUPPORTED_BLOCK_CIPHER
 *
 * A block cipher that is known to be supported.
 *
 * For simplicity's sake, stick to block ciphers with 16-byte blocks.
 */
#if defined(MBEDTLS_AES_C)
#define KNOWN_SUPPORTED_BLOCK_CIPHER PSA_KEY_TYPE_AES
#elif defined(MBEDTLS_ARIA_C)
#define KNOWN_SUPPORTED_BLOCK_CIPHER PSA_KEY_TYPE_ARIA
#elif defined(MBEDTLS_CAMELLIA_C)
#define KNOWN_SUPPORTED_BLOCK_CIPHER PSA_KEY_TYPE_CAMELLIA
#undef KNOWN_SUPPORTED_BLOCK_CIPHER
#endif

/** \def KNOWN_SUPPORTED_MAC_ALG
 *
 * A MAC mode that is known to be supported.
 *
 * It must either be HMAC with #KNOWN_SUPPORTED_HASH_ALG or
 * a block cipher-based MAC with #KNOWN_SUPPORTED_BLOCK_CIPHER.
 *
 * This is used in some smoke tests.
 */
#if defined(KNOWN_SUPPORTED_HASH_ALG) && defined(PSA_WANT_ALG_HMAC)
#define KNOWN_SUPPORTED_MAC_ALG ( PSA_ALG_HMAC( KNOWN_SUPPORTED_HASH_ALG ) )
#define KNOWN_SUPPORTED_MAC_KEY_TYPE PSA_KEY_TYPE_HMAC
#elif defined(KNOWN_SUPPORTED_BLOCK_CIPHER) && defined(MBEDTLS_CMAC_C)
#define KNOWN_SUPPORTED_MAC_ALG PSA_ALG_CMAC
#define KNOWN_SUPPORTED_MAC_KEY_TYPE KNOWN_SUPPORTED_BLOCK_CIPHER
#else
#undef KNOWN_SUPPORTED_MAC_ALG
#undef KNOWN_SUPPORTED_MAC_KEY_TYPE
#endif

/** \def KNOWN_SUPPORTED_BLOCK_CIPHER_ALG
 *
 * A cipher algorithm and key type that are known to be supported.
 *
 * This is used in some smoke tests.
 */
#if defined(KNOWN_SUPPORTED_BLOCK_CIPHER) && defined(MBEDTLS_CIPHER_MODE_CTR)
#define KNOWN_SUPPORTED_BLOCK_CIPHER_ALG PSA_ALG_CTR
#elif defined(KNOWN_SUPPORTED_BLOCK_CIPHER) && defined(MBEDTLS_CIPHER_MODE_CBC)
#define KNOWN_SUPPORTED_BLOCK_CIPHER_ALG PSA_ALG_CBC_NO_PADDING
#elif defined(KNOWN_SUPPORTED_BLOCK_CIPHER) && defined(MBEDTLS_CIPHER_MODE_CFB)
#define KNOWN_SUPPORTED_BLOCK_CIPHER_ALG PSA_ALG_CFB
#elif defined(KNOWN_SUPPORTED_BLOCK_CIPHER) && defined(MBEDTLS_CIPHER_MODE_OFB)
#define KNOWN_SUPPORTED_BLOCK_CIPHER_ALG PSA_ALG_OFB
#else
#undef KNOWN_SUPPORTED_BLOCK_CIPHER_ALG
#endif
#if defined(KNOWN_SUPPORTED_BLOCK_CIPHER_ALG)
#define KNOWN_SUPPORTED_CIPHER_ALG KNOWN_SUPPORTED_BLOCK_CIPHER_ALG
#define KNOWN_SUPPORTED_CIPHER_KEY_TYPE KNOWN_SUPPORTED_BLOCK_CIPHER
#else
#undef KNOWN_SUPPORTED_CIPHER_ALG
#undef KNOWN_SUPPORTED_CIPHER_KEY_TYPE
#endif

/** Convenience function to set up a key derivation.
 *
 * In case of failure, mark the current test case as failed.
 *
 * The inputs \p input1 and \p input2 are, in order:
 * - HKDF: salt, info.
 * - TKS 1.2 PRF, TLS 1.2 PSK-to-MS: seed, label.
 *
 * \param operation         The operation object to use.
 *                          It must be in the initialized state.
 * \param key               The key to use.
 * \param alg               The algorithm to use.
 * \param input1            The first input to pass.
 * \param input1_length     The length of \p input1 in bytes.
 * \param input2            The first input to pass.
 * \param input2_length     The length of \p input2 in bytes.
 * \param capacity          The capacity to set.
 *
 * \return                  \c 1 on success, \c 0 on failure.
 */
int mbedtls_test_psa_setup_key_derivation_wrap(
    psa_key_derivation_operation_t* operation,
    mbedtls_svc_key_id_t key,
    psa_algorithm_t alg,
    const unsigned char* input1, size_t input1_length,
    const unsigned char* input2, size_t input2_length,
    size_t capacity );

/** Perform a key agreement using the given key pair against its public key
 * using psa_raw_key_agreement().
 *
 * The result is discarded. The purpose of this function is to smoke-test a key.
 *
 * In case of failure, mark the current test case as failed.
 *
 * \param alg               A key agreement algorithm compatible with \p key.
 * \param key               A key that allows key agreement with \p alg.
 *
 * \return                  \c 1 on success, \c 0 on failure.
 */
psa_status_t mbedtls_test_psa_raw_key_agreement_with_self(
    psa_algorithm_t alg,
    mbedtls_svc_key_id_t key );

/** Perform a key agreement using the given key pair against its public key
 * using psa_key_derivation_raw_key().
 *
 * The result is discarded. The purpose of this function is to smoke-test a key.
 *
 * In case of failure, mark the current test case as failed.
 *
 * \param operation         An operation that has been set up for a key
 *                          agreement algorithm that is compatible with
 *                          \p key.
 * \param key               A key pair object that is suitable for a key
 *                          agreement with \p operation.
 *
 * \return                  \c 1 on success, \c 0 on failure.
 */
psa_status_t mbedtls_test_psa_key_agreement_with_self(
    psa_key_derivation_operation_t *operation,
    mbedtls_svc_key_id_t key );

/** Perform sanity checks on the given key representation.
 *
 * If any of the checks fail, mark the current test case as failed.
 *
 * The checks depend on the key type.
 * - All types: check the export size against maximum-size macros.
 * - DES: parity bits.
 * - RSA: check the ASN.1 structure and the size and parity of the integers.
 * - ECC private or public key: exact representation length.
 * - Montgomery public key: first byte.
 *
 * \param type              The key type.
 * \param bits              The key size in bits.
 * \param exported          A buffer containing the key representation.
 * \param exported_length   The length of \p exported in bytes.
 *
 * \return                  \c 1 if all checks passed, \c 0 on failure.
 */
int mbedtls_test_psa_exported_key_sanity_check(
    psa_key_type_t type, size_t bits,
    const uint8_t *exported, size_t exported_length );

/** Do smoke tests on a key.
 *
 * Perform one of each operation indicated by \p alg (decrypt/encrypt,
 * sign/verify, or derivation) that is permitted according to \p usage.
 * \p usage and \p alg should correspond to the expected policy on the
 * key.
 *
 * Export the key if permitted by \p usage, and check that the output
 * looks sensible. If \p usage forbids export, check that
 * \p psa_export_key correctly rejects the attempt. If the key is
 * asymmetric, also check \p psa_export_public_key.
 *
 * If the key fails the tests, this function calls the test framework's
 * `mbedtls_test_fail` function and returns false. Otherwise this function
 * returns true. Therefore it should be used as follows:
 * ```
 * if( ! exercise_key( ... ) ) goto exit;
 * ```
 *
 * \param key       The key to exercise. It should be capable of performing
 *                  \p alg.
 * \param usage     The usage flags to assume.
 * \param alg       The algorithm to exercise.
 *
 * \retval 0 The key failed the smoke tests.
 * \retval 1 The key passed the smoke tests.
 */
int mbedtls_test_psa_exercise_key( mbedtls_svc_key_id_t key,
                                   psa_key_usage_t usage,
                                   psa_algorithm_t alg );

psa_key_usage_t mbedtls_test_psa_usage_to_exercise( psa_key_type_t type,
                                                    psa_algorithm_t alg );

#endif /* PSA_EXERCISE_KEY_H */

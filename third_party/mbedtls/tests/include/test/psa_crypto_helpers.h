/*
 * Helper functions for tests that use the PSA Crypto API.
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

#ifndef PSA_CRYPTO_HELPERS_H
#define PSA_CRYPTO_HELPERS_H

#include "test/helpers.h"

#if defined(MBEDTLS_PSA_CRYPTO_C)

#include "test/psa_helpers.h"

#include <psa/crypto.h>

#if defined(MBEDTLS_USE_PSA_CRYPTO)
#include "mbedtls/psa_util.h"
#endif

#if defined(MBEDTLS_PSA_CRYPTO_STORAGE_C)

/* Internal function for #TEST_USES_KEY_ID. Return 1 on success, 0 on failure. */
int mbedtls_test_uses_key_id( mbedtls_svc_key_id_t key_id );

/** Destroy persistent keys recorded with #TEST_USES_KEY_ID.
 */
void mbedtls_test_psa_purge_key_storage( void );

/** Purge the in-memory cache of persistent keys recorded with
 * #TEST_USES_KEY_ID.
 *
 * Call this function before calling PSA_DONE() if it's ok for
 * persistent keys to still exist at this point.
 */
void mbedtls_test_psa_purge_key_cache( void );

/** \def TEST_USES_KEY_ID
 *
 * Call this macro in a test function before potentially creating a
 * persistent key. Test functions that use this mechanism must call
 * mbedtls_test_psa_purge_key_storage() in their cleanup code.
 *
 * This macro records a persistent key identifier as potentially used in the
 * current test case. Recorded key identifiers will be cleaned up at the end
 * of the test case, even on failure.
 *
 * This macro has no effect on volatile keys. Therefore, it is safe to call
 * this macro in a test function that creates either volatile or persistent
 * keys depending on the test data.
 *
 * This macro currently has no effect on special identifiers
 * used to store implementation-specific files.
 *
 * Calling this macro multiple times on the same key identifier in the same
 * test case has no effect.
 *
 * This macro can fail the test case if there isn't enough memory to
 * record the key id.
 *
 * \param key_id    The PSA key identifier to record.
 */
#define TEST_USES_KEY_ID( key_id )                      \
    TEST_ASSERT( mbedtls_test_uses_key_id( key_id ) )

#else /* MBEDTLS_PSA_CRYPTO_STORAGE_C */

#define TEST_USES_KEY_ID( key_id ) ( (void) ( key_id ) )
#define mbedtls_test_psa_purge_key_storage( ) ( (void) 0 )
#define mbedtls_test_psa_purge_key_cache( ) ( (void) 0 )

#endif /* MBEDTLS_PSA_CRYPTO_STORAGE_C */

#define PSA_INIT( ) PSA_ASSERT( psa_crypto_init( ) )

/** Check for things that have not been cleaned up properly in the
 * PSA subsystem.
 *
 * \return NULL if nothing has leaked.
 * \return A string literal explaining what has not been cleaned up
 *         if applicable.
 */
const char *mbedtls_test_helper_is_psa_leaking( void );

/** Check that no PSA Crypto key slots are in use.
 *
 * If any slots are in use, mark the current test as failed and jump to
 * the exit label. This is equivalent to
 * `TEST_ASSERT( ! mbedtls_test_helper_is_psa_leaking( ) )`
 * but with a more informative message.
 */
#define ASSERT_PSA_PRISTINE( )                                          \
    do                                                                  \
    {                                                                   \
        if( test_fail_if_psa_leaking( __LINE__, __FILE__ ) )            \
            goto exit;                                                  \
    }                                                                   \
    while( 0 )

/** Shut down the PSA Crypto subsystem and destroy persistent keys.
 * Expect a clean shutdown, with no slots in use.
 *
 * If some key slots are still in use, record the test case as failed,
 * but continue executing. This macro is suitable (and primarily intended)
 * for use in the cleanup section of test functions.
 *
 * \note Persistent keys must be recorded with #TEST_USES_KEY_ID before
 *       creating them.
 */
#define PSA_DONE( )                                                     \
    do                                                                  \
    {                                                                   \
        test_fail_if_psa_leaking( __LINE__, __FILE__ );                 \
        mbedtls_test_psa_purge_key_storage( );                          \
        mbedtls_psa_crypto_free( );                                     \
    }                                                                   \
    while( 0 )

/** Shut down the PSA Crypto subsystem, allowing persistent keys to survive.
 * Expect a clean shutdown, with no slots in use.
 *
 * If some key slots are still in use, record the test case as failed and
 * jump to the `exit` label.
 */
#define PSA_SESSION_DONE( )                                             \
    do                                                                  \
    {                                                                   \
        mbedtls_test_psa_purge_key_cache( );                            \
        ASSERT_PSA_PRISTINE( );                                         \
        mbedtls_psa_crypto_free( );                                     \
    }                                                                   \
    while( 0 )



#if defined(RECORD_PSA_STATUS_COVERAGE_LOG)
psa_status_t mbedtls_test_record_status( psa_status_t status,
                                         const char *func,
                                         const char *file, int line,
                                         const char *expr );

/** Return value logging wrapper macro.
 *
 * Evaluate \p expr. Write a line recording its value to the log file
 * #STATUS_LOG_FILE_NAME and return the value. The line is a colon-separated
 * list of fields:
 * ```
 * value of expr:string:__FILE__:__LINE__:expr
 * ```
 *
 * The test code does not call this macro explicitly because that would
 * be very invasive. Instead, we instrument the source code by defining
 * a bunch of wrapper macros like
 * ```
 * #define psa_crypto_init() RECORD_STATUS("psa_crypto_init", psa_crypto_init())
 * ```
 * These macro definitions must be present in `instrument_record_status.h`
 * when building the test suites.
 *
 * \param string    A string, normally a function name.
 * \param expr      An expression to evaluate, normally a call of the function
 *                  whose name is in \p string. This expression must return
 *                  a value of type #psa_status_t.
 * \return          The value of \p expr.
 */
#define RECORD_STATUS( string, expr )                                   \
    mbedtls_test_record_status( ( expr ), string, __FILE__, __LINE__, #expr )

#include "instrument_record_status.h"

#endif /* defined(RECORD_PSA_STATUS_COVERAGE_LOG) */

/** Return extended key usage policies.
 *
 * Do a key policy permission extension on key usage policies always involves
 * permissions of other usage policies
 * (like PSA_KEY_USAGE_SIGN_HASH involves PSA_KEY_USAGE_SIGN_MESSGAE).
 */
psa_key_usage_t mbedtls_test_update_key_usage_flags( psa_key_usage_t usage_flags );

/** Skip a test case if the given key is a 192 bits AES key and the AES
 *  implementation is at least partially provided by an accelerator or
 *  alternative implementation.
 *
 *  Call this macro in a test case when a cryptographic operation that may
 *  involve an AES operation returns a #PSA_ERROR_NOT_SUPPORTED error code.
 *  The macro call will skip and not fail the test case in case the operation
 *  involves a 192 bits AES key and the AES implementation is at least
 *  partially provided by an accelerator or alternative implementation.
 *
 *  Hardware AES implementations not supporting 192 bits keys commonly exist.
 *  Consequently, PSA test cases aim at not failing when an AES operation with
 *  a 192 bits key performed by an alternative AES implementation returns
 *  with the #PSA_ERROR_NOT_SUPPORTED error code. The purpose of this macro
 *  is to facilitate this and make the test case code more readable.
 *
 *  \param key_type  Key type
 *  \param key_bits  Key length in number of bits.
 */
#if defined(MBEDTLS_AES_ALT) || \
    defined(MBEDTLS_AES_SETKEY_ENC_ALT) || \
    defined(MBEDTLS_PSA_ACCEL_KEY_TYPE_AES)
#define MBEDTLS_TEST_HAVE_ALT_AES 1
#else
#define MBEDTLS_TEST_HAVE_ALT_AES 0
#endif

#define MBEDTLS_TEST_PSA_SKIP_IF_ALT_AES_192( key_type, key_bits )        \
    do                                                                    \
    {                                                                     \
        if( ( MBEDTLS_TEST_HAVE_ALT_AES ) &&                              \
            ( ( key_type ) == PSA_KEY_TYPE_AES ) &&                       \
            ( key_bits == 192 ) )                                         \
        {                                                                 \
            mbedtls_test_skip( "AES-192 not supported", __LINE__, __FILE__ );     \
            goto exit;                                                    \
        }                                                                 \
    }                                                                     \
    while( 0 )

/** Skip a test case if a GCM operation with a nonce length different from
 *  12 bytes fails and was performed by an accelerator or alternative
 *  implementation.
 *
 *  Call this macro in a test case when an AEAD cryptography operation that
 *  may involve the GCM mode returns with a #PSA_ERROR_NOT_SUPPORTED error
 *  code. The macro call will skip and not fail the test case in case the
 *  operation involves the GCM mode, a nonce with a length different from
 *  12 bytes and the GCM mode implementation is an alternative one.
 *
 *  Hardware GCM implementations not supporting nonce lengths different from
 *  12 bytes commonly exist, as supporting a non-12-byte nonce requires
 *  additional computations involving the GHASH function.
 *  Consequently, PSA test cases aim at not failing when an AEAD operation in
 *  GCM mode with a nonce length different from 12 bytes is performed by an
 *  alternative GCM implementation and returns with a #PSA_ERROR_NOT_SUPPORTED
 *  error code. The purpose of this macro is to facilitate this check and make
 *  the test case code more readable.
 *
 *  \param  alg             The AEAD algorithm.
 *  \param  nonce_length    The nonce length in number of bytes.
 */
#if defined(MBEDTLS_GCM_ALT) || \
    defined(MBEDTLS_PSA_ACCEL_ALG_GCM)
#define MBEDTLS_TEST_HAVE_ALT_GCM  1
#else
#define MBEDTLS_TEST_HAVE_ALT_GCM  0
#endif

#define MBEDTLS_TEST_PSA_SKIP_IF_ALT_GCM_NOT_12BYTES_NONCE( alg,           \
                                                            nonce_length ) \
    do                                                                     \
    {                                                                      \
        if( ( MBEDTLS_TEST_HAVE_ALT_GCM ) &&                               \
            ( PSA_ALG_AEAD_WITH_SHORTENED_TAG( ( alg ) , 0 ) ==            \
              PSA_ALG_AEAD_WITH_SHORTENED_TAG( PSA_ALG_GCM, 0 ) ) &&       \
            ( ( nonce_length ) != 12 ) )                                   \
        {                                                                  \
            mbedtls_test_skip( "GCM with non-12-byte IV is not supported", __LINE__, __FILE__ ); \
            goto exit;                                                     \
        }                                                                  \
    }                                                                      \
    while( 0 )

#endif /* MBEDTLS_PSA_CRYPTO_C */

/** \def USE_PSA_INIT
 *
 * Call this macro to initialize the PSA subsystem if #MBEDTLS_USE_PSA_CRYPTO
 * or #MBEDTLS_SSL_PROTO_TLS1_3 (In contrast to TLS 1.2 implementation, the
 * TLS 1.3 one uses PSA independently of the definition of
 * #MBEDTLS_USE_PSA_CRYPTO) is enabled and do nothing otherwise. If the
 * initialization fails, mark the test case as failed and jump to the \p exit
 * label.
 */
/** \def USE_PSA_DONE
 *
 * Call this macro at the end of a test case if you called #USE_PSA_INIT.
 * This is like #PSA_DONE, except that it does nothing if
 * #MBEDTLS_USE_PSA_CRYPTO is disabled.
 */
#if defined(MBEDTLS_USE_PSA_CRYPTO) || defined(MBEDTLS_SSL_PROTO_TLS1_3)
#define USE_PSA_INIT( ) PSA_INIT( )
#define USE_PSA_DONE( ) PSA_DONE( )
#else /* MBEDTLS_USE_PSA_CRYPTO || MBEDTLS_SSL_PROTO_TLS1_3 */
/* Define empty macros so that we can use them in the preamble and teardown
 * of every test function that uses PSA conditionally based on
 * MBEDTLS_USE_PSA_CRYPTO. */
#define USE_PSA_INIT( ) ( (void) 0 )
#define USE_PSA_DONE( ) ( (void) 0 )
#endif /* !MBEDTLS_USE_PSA_CRYPTO && !MBEDTLS_SSL_PROTO_TLS1_3 */

#endif /* PSA_CRYPTO_HELPERS_H */

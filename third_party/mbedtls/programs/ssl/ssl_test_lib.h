/*
 *  Common code for SSL test programs
 *
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

#ifndef MBEDTLS_PROGRAMS_SSL_SSL_TEST_LIB_H
#define MBEDTLS_PROGRAMS_SSL_SSL_TEST_LIB_H

#include "mbedtls/build_info.h"

#if defined(MBEDTLS_PLATFORM_C)
#include "mbedtls/platform.h"
#else
#include <stdio.h>
#include <stdlib.h>
#define mbedtls_calloc     calloc
#define mbedtls_free       free
#define mbedtls_time       time
#define mbedtls_time_t     time_t
#define mbedtls_printf     printf
#define mbedtls_fprintf    fprintf
#define mbedtls_snprintf   snprintf
#define mbedtls_setbuf     setbuf
#define mbedtls_exit            exit
#define MBEDTLS_EXIT_SUCCESS    EXIT_SUCCESS
#define MBEDTLS_EXIT_FAILURE    EXIT_FAILURE
#endif

#undef HAVE_RNG
#if defined(MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG) &&         \
    ( defined(MBEDTLS_USE_PSA_CRYPTO) ||                \
      defined(MBEDTLS_TEST_USE_PSA_CRYPTO_RNG) )
#define HAVE_RNG
#elif defined(MBEDTLS_ENTROPY_C) && defined(MBEDTLS_CTR_DRBG_C)
#define HAVE_RNG
#elif defined(MBEDTLS_ENTROPY_C) && defined(MBEDTLS_HMAC_DRBG_C) &&     \
    ( defined(MBEDTLS_SHA256_C) || defined(MBEDTLS_SHA512_C) )
#define HAVE_RNG
#endif

#if !defined(MBEDTLS_NET_C) ||                              \
    !defined(MBEDTLS_SSL_TLS_C)
#define MBEDTLS_SSL_TEST_IMPOSSIBLE                         \
    "MBEDTLS_NET_C and/or "                                 \
    "MBEDTLS_SSL_TLS_C not defined."
#elif !defined(HAVE_RNG)
#define MBEDTLS_SSL_TEST_IMPOSSIBLE                         \
    "No random generator is available.\n"
#else
#undef MBEDTLS_SSL_TEST_IMPOSSIBLE

#undef HAVE_RNG

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/ssl_ciphersuites.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/hmac_drbg.h"
#include "mbedtls/x509.h"
#include "mbedtls/error.h"
#include "mbedtls/debug.h"
#include "mbedtls/timing.h"
#include "mbedtls/base64.h"
#include "test/certs.h"

#if defined(MBEDTLS_USE_PSA_CRYPTO) || defined(MBEDTLS_TEST_USE_PSA_CRYPTO_RNG)
#include "psa/crypto.h"
#include "mbedtls/psa_util.h"
#endif

#if defined(MBEDTLS_MEMORY_BUFFER_ALLOC_C)
#include "mbedtls/memory_buffer_alloc.h"
#endif

#include <test/helpers.h>

#include "../test/query_config.h"

typedef struct eap_tls_keys
{
    unsigned char master_secret[48];
    unsigned char randbytes[64];
    mbedtls_tls_prf_types tls_prf_type;
} eap_tls_keys;

#if defined( MBEDTLS_SSL_DTLS_SRTP )

/* Supported SRTP mode needs a maximum of :
 * - 16 bytes for key (AES-128)
 * - 14 bytes SALT
 * One for sender, one for receiver context
 */
#define MBEDTLS_TLS_SRTP_MAX_KEY_MATERIAL_LENGTH    60

typedef struct dtls_srtp_keys
{
    unsigned char master_secret[48];
    unsigned char randbytes[64];
    mbedtls_tls_prf_types tls_prf_type;
} dtls_srtp_keys;

#endif /* MBEDTLS_SSL_DTLS_SRTP */

typedef struct
{
    mbedtls_ssl_context *ssl;
    mbedtls_net_context *net;
} io_ctx_t;

void my_debug( void *ctx, int level,
               const char *file, int line,
               const char *str );

#if defined(MBEDTLS_HAVE_TIME)
mbedtls_time_t dummy_constant_time( mbedtls_time_t* time );
#endif

#if defined(MBEDTLS_USE_PSA_CRYPTO) && !defined(MBEDTLS_TEST_USE_PSA_CRYPTO_RNG)
/* If MBEDTLS_TEST_USE_PSA_CRYPTO_RNG is defined, the SSL test programs will use
 * mbedtls_psa_get_random() rather than entropy+DRBG as a random generator.
 *
 * The constraints are:
 * - Without the entropy module, the PSA RNG is the only option.
 * - Without at least one of the DRBG modules, the PSA RNG is the only option.
 * - The PSA RNG does not support explicit seeding, so it is incompatible with
 *   the reproducible mode used by test programs.
 * - For good overall test coverage, there should be at least one configuration
 *   where the test programs use the PSA RNG while the PSA RNG is itself based
 *   on entropy+DRBG, and at least one configuration where the test programs
 *   do not use the PSA RNG even though it's there.
 *
 * A simple choice that meets the constraints is to use the PSA RNG whenever
 * MBEDTLS_USE_PSA_CRYPTO is enabled. There's no real technical reason the
 * choice to use the PSA RNG in the test programs and the choice to use
 * PSA crypto when TLS code needs crypto have to be tied together, but it
 * happens to be a good match. It's also a good match from an application
 * perspective: either PSA is preferred for TLS (both for crypto and for
 * random generation) or it isn't.
 */
#define MBEDTLS_TEST_USE_PSA_CRYPTO_RNG
#endif

/** A context for random number generation (RNG).
 */
typedef struct
{
#if defined(MBEDTLS_TEST_USE_PSA_CRYPTO_RNG)
    unsigned char dummy;
#else /* MBEDTLS_TEST_USE_PSA_CRYPTO_RNG */
    mbedtls_entropy_context entropy;
#if defined(MBEDTLS_CTR_DRBG_C)
    mbedtls_ctr_drbg_context drbg;
#elif defined(MBEDTLS_HMAC_DRBG_C)
    mbedtls_hmac_drbg_context drbg;
#else
#error "No DRBG available"
#endif
#endif /* MBEDTLS_TEST_USE_PSA_CRYPTO_RNG */
} rng_context_t;

/** Initialize the RNG.
 *
 * This function only initializes the memory used by the RNG context.
 * Before using the RNG, it must be seeded with rng_seed().
 */
void rng_init( rng_context_t *rng );

/* Seed the random number generator.
 *
 * \param rng           The RNG context to use. It must have been initialized
 *                      with rng_init().
 * \param reproducible  If zero, seed the RNG from entropy.
 *                      If nonzero, use a fixed seed, so that the program
 *                      will produce the same sequence of random numbers
 *                      each time it is invoked.
 * \param pers          A null-terminated string. Different values for this
 *                      string cause the RNG to emit different output for
 *                      the same seed.
 *
 * return 0 on success, a negative value on error.
 */
int rng_seed( rng_context_t *rng, int reproducible, const char *pers );

/** Deinitialize the RNG. Free any embedded resource.
 *
 * \param rng           The RNG context to deinitialize. It must have been
 *                      initialized with rng_init().
 */
void rng_free( rng_context_t *rng );

/** Generate random data.
 *
 * This function is suitable for use as the \c f_rng argument to Mbed TLS
 * library functions.
 *
 * \param p_rng         The random generator context. This must be a pointer to
 *                      a #rng_context_t structure.
 * \param output        The buffer to fill.
 * \param output_len    The length of the buffer in bytes.
 *
 * \return              \c 0 on success.
 * \return              An Mbed TLS error code on error.
 */
int rng_get( void *p_rng, unsigned char *output, size_t output_len );

/** Parse command-line option: key_opaque_algs
 *
 *
 * \param arg           String value of key_opaque_algs
 *                      Coma-separated pair of values among the following:
 *                      - "rsa-sign-pkcs1"
 *                      - "rsa-sign-pss"
 *                      - "rsa-decrypt"
 *                      - "ecdsa-sign"
 *                      - "ecdh"
 *                      - "none" (only acceptable for the second value).
 * \param alg1          Address of pointer to alg #1
 * \param alg2          Address of pointer to alg #2
 *
 * \return              \c 0 on success.
 * \return              \c 1 on parse failure.
 */
int key_opaque_alg_parse( const char *arg, const char **alg1, const char **alg2 );

#if defined(MBEDTLS_USE_PSA_CRYPTO)
/** Parse given opaque key algorithms to obtain psa algs and usage
 *  that will be passed to mbedtls_pk_wrap_as_opaque().
 *
 *
 * \param alg1          input string opaque key algorithm #1
 * \param alg2          input string opaque key algorithm #2
 * \param psa_alg1      output PSA algorithm #1
 * \param psa_alg2      output PSA algorithm #2
 * \param usage         output key usage
 * \param key_type      key type used to set default psa algorithm/usage
 *                      when alg1 in "none"
 *
 * \return              \c 0 on success.
 * \return              \c 1 on parse failure.
 */
int key_opaque_set_alg_usage( const char *alg1, const char *alg2,
                              psa_algorithm_t *psa_alg1,
                              psa_algorithm_t *psa_alg2,
                              psa_key_usage_t *usage,
                              mbedtls_pk_type_t key_type );
#endif /* MBEDTLS_USE_PSA_CRYPTO */

#if defined(MBEDTLS_USE_PSA_CRYPTO) && defined(MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG)
/* The test implementation of the PSA external RNG is insecure. When
 * MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG is enabled, before using any PSA crypto
 * function that makes use of an RNG, you must call
 * mbedtls_test_enable_insecure_external_rng(). */
#include <test/fake_external_rng_for_test.h>
#endif

#if defined(MBEDTLS_X509_TRUSTED_CERTIFICATE_CALLBACK)
int ca_callback( void *data, mbedtls_x509_crt const *child,
                 mbedtls_x509_crt **candidates );
#endif /* MBEDTLS_X509_TRUSTED_CERTIFICATE_CALLBACK */

/*
 * Test recv/send functions that make sure each try returns
 * WANT_READ/WANT_WRITE at least once before succeeding
 */
int delayed_recv( void *ctx, unsigned char *buf, size_t len );
int delayed_send( void *ctx, const unsigned char *buf, size_t len );

/*
 * Wait for an event from the underlying transport or the timer
 * (Used in event-driven IO mode).
 */
int idle( mbedtls_net_context *fd,
#if defined(MBEDTLS_TIMING_C)
          mbedtls_timing_delay_context *timer,
#endif
          int idle_reason );

#if defined(MBEDTLS_TEST_HOOKS)
/** Initialize whatever test hooks are enabled by the compile-time
 * configuration and make sense for the TLS test programs. */
void test_hooks_init( void );

/** Check if any test hooks detected a problem.
 *
 * If a problem was detected, it's ok for the calling program to keep going,
 * but it should ultimately exit with an error status.
 *
 * \note When implementing a test hook that detects errors on its own
 *       (as opposed to e.g. leaving the error for a memory sanitizer to
 *       report), make sure to print a message to standard error either at
 *       the time the problem is detected or during the execution of this
 *       function. This function does not indicate what problem was detected,
 *       so printing a message is the only way to provide feedback in the
 *       logs of the calling program.
 *
 * \return Nonzero if a problem was detected.
 *         \c 0 if no problem was detected.
 */
int test_hooks_failure_detected( void );

/** Free any resources allocated for the sake of test hooks.
 *
 * Call this at the end of the program so that resource leak analyzers
 * don't complain.
 */
void test_hooks_free( void );

#endif /* !MBEDTLS_TEST_HOOKS */

#endif /* MBEDTLS_SSL_TEST_IMPOSSIBLE conditions: else */
#endif /* MBEDTLS_PROGRAMS_SSL_SSL_TEST_LIB_H */

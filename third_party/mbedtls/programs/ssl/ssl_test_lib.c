/*
 *  Common code library for SSL test programs.
 *
 *  In addition to the functions in this file, there is shared source code
 *  that cannot be compiled separately in "ssl_test_common_source.c".
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

#define MBEDTLS_ALLOW_PRIVATE_ACCESS

#include "ssl_test_lib.h"

#if defined(MBEDTLS_TEST_HOOKS)
#include "test/helpers.h"
#endif

#if !defined(MBEDTLS_SSL_TEST_IMPOSSIBLE)

void my_debug( void *ctx, int level,
               const char *file, int line,
               const char *str )
{
    const char *p, *basename;

    /* Extract basename from file */
    for( p = basename = file; *p != '\0'; p++ )
        if( *p == '/' || *p == '\\' )
            basename = p + 1;

    mbedtls_fprintf( (FILE *) ctx, "%s:%04d: |%d| %s",
                     basename, line, level, str );
    fflush( (FILE *) ctx  );
}

#if defined(MBEDTLS_HAVE_TIME)
mbedtls_time_t dummy_constant_time( mbedtls_time_t* time )
{
    (void) time;
    return 0x5af2a056;
}
#endif

#if !defined(MBEDTLS_TEST_USE_PSA_CRYPTO_RNG)
static int dummy_entropy( void *data, unsigned char *output, size_t len )
{
    size_t i;
    int ret;
    (void) data;

    ret = mbedtls_entropy_func( data, output, len );
    for( i = 0; i < len; i++ )
    {
        //replace result with pseudo random
        output[i] = (unsigned char) rand();
    }
    return( ret );
}
#endif

void rng_init( rng_context_t *rng )
{
#if defined(MBEDTLS_TEST_USE_PSA_CRYPTO_RNG)
    (void) rng;
    psa_crypto_init( );
#else /* !MBEDTLS_TEST_USE_PSA_CRYPTO_RNG */

#if defined(MBEDTLS_CTR_DRBG_C)
    mbedtls_ctr_drbg_init( &rng->drbg );
#elif defined(MBEDTLS_HMAC_DRBG_C)
    mbedtls_hmac_drbg_init( &rng->drbg );
#else
#error "No DRBG available"
#endif

    mbedtls_entropy_init( &rng->entropy );
#endif /* !MBEDTLS_TEST_USE_PSA_CRYPTO_RNG */
}

int rng_seed( rng_context_t *rng, int reproducible, const char *pers )
{
#if defined(MBEDTLS_USE_PSA_CRYPTO)
    if( reproducible )
    {
        mbedtls_fprintf( stderr,
                         "MBEDTLS_USE_PSA_CRYPTO does not support reproducible mode.\n" );
        return( -1 );
    }
#endif
#if defined(MBEDTLS_TEST_USE_PSA_CRYPTO_RNG)
    /* The PSA crypto RNG does its own seeding. */
    (void) rng;
    (void) pers;
    if( reproducible )
    {
        mbedtls_fprintf( stderr,
                         "The PSA RNG does not support reproducible mode.\n" );
        return( -1 );
    }
    return( 0 );
#else /* !MBEDTLS_TEST_USE_PSA_CRYPTO_RNG */
    int ( *f_entropy )( void *, unsigned char *, size_t ) =
        ( reproducible ? dummy_entropy : mbedtls_entropy_func );

    if ( reproducible )
        srand( 1 );

#if defined(MBEDTLS_CTR_DRBG_C)
    int ret = mbedtls_ctr_drbg_seed( &rng->drbg,
                                     f_entropy, &rng->entropy,
                                     (const unsigned char *) pers,
                                     strlen( pers ) );
#elif defined(MBEDTLS_HMAC_DRBG_C)
#if defined(MBEDTLS_SHA256_C)
    const mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
#elif defined(MBEDTLS_SHA512_C)
    const mbedtls_md_type_t md_type = MBEDTLS_MD_SHA512;
#else
#error "No message digest available for HMAC_DRBG"
#endif
    int ret = mbedtls_hmac_drbg_seed( &rng->drbg,
                                      mbedtls_md_info_from_type( md_type ),
                                      f_entropy, &rng->entropy,
                                      (const unsigned char *) pers,
                                      strlen( pers ) );
#else /* !defined(MBEDTLS_CTR_DRBG_C) && !defined(MBEDTLS_HMAC_DRBG_C) */
#error "No DRBG available"
#endif /* !defined(MBEDTLS_CTR_DRBG_C) && !defined(MBEDTLS_HMAC_DRBG_C) */

    if( ret != 0 )
    {
        mbedtls_printf( " failed\n  ! mbedtls_ctr_drbg_seed returned -0x%x\n",
                        (unsigned int) -ret );
        return( ret );
    }
#endif /* !MBEDTLS_TEST_USE_PSA_CRYPTO_RNG */

    return( 0 );
}

void rng_free( rng_context_t *rng )
{
#if defined(MBEDTLS_TEST_USE_PSA_CRYPTO_RNG)
    (void) rng;
    /* Deinitialize the PSA crypto subsystem. This deactivates all PSA APIs.
     * This is ok because none of our applications try to do any crypto after
     * deinitializing the RNG. */
    mbedtls_psa_crypto_free( );
#else /* !MBEDTLS_TEST_USE_PSA_CRYPTO_RNG */

#if defined(MBEDTLS_CTR_DRBG_C)
    mbedtls_ctr_drbg_free( &rng->drbg );
#elif defined(MBEDTLS_HMAC_DRBG_C)
    mbedtls_hmac_drbg_free( &rng->drbg );
#else
#error "No DRBG available"
#endif

    mbedtls_entropy_free( &rng->entropy );
#endif /* !MBEDTLS_TEST_USE_PSA_CRYPTO_RNG */
}

int rng_get( void *p_rng, unsigned char *output, size_t output_len )
{
#if defined(MBEDTLS_TEST_USE_PSA_CRYPTO_RNG)
    (void) p_rng;
    return( mbedtls_psa_get_random( MBEDTLS_PSA_RANDOM_STATE,
                                    output, output_len ) );
#else /* !MBEDTLS_TEST_USE_PSA_CRYPTO_RNG */
    rng_context_t *rng = p_rng;

#if defined(MBEDTLS_CTR_DRBG_C)
    return( mbedtls_ctr_drbg_random( &rng->drbg, output, output_len ) );
#elif defined(MBEDTLS_HMAC_DRBG_C)
    return( mbedtls_hmac_drbg_random( &rng->drbg, output, output_len ) );
#else
#error "No DRBG available"
#endif

#endif /* !MBEDTLS_TEST_USE_PSA_CRYPTO_RNG */
}

int key_opaque_alg_parse( const char *arg, const char **alg1, const char **alg2 )
{
    char* separator;
    if( ( separator = strchr( arg, ',' ) ) == NULL )
        return 1;
    *separator = '\0';

    *alg1 = arg;
    *alg2 = separator + 1;

    if( strcmp( *alg1, "rsa-sign-pkcs1" ) != 0 &&
        strcmp( *alg1, "rsa-sign-pss" ) != 0 &&
        strcmp( *alg1, "rsa-decrypt" ) != 0 &&
        strcmp( *alg1, "ecdsa-sign" ) != 0 &&
        strcmp( *alg1, "ecdh" ) != 0 )
        return 1;

    if( strcmp( *alg2, "rsa-sign-pkcs1" ) != 0 &&
        strcmp( *alg2, "rsa-sign-pss" ) != 0 &&
        strcmp( *alg2, "rsa-decrypt" ) != 0 &&
        strcmp( *alg2, "ecdsa-sign" ) != 0 &&
        strcmp( *alg2, "ecdh" ) != 0 &&
        strcmp( *alg2, "none" ) != 0 )
        return 1;

    return 0;
}

#if defined(MBEDTLS_USE_PSA_CRYPTO)
int key_opaque_set_alg_usage( const char *alg1, const char *alg2,
                              psa_algorithm_t *psa_alg1,
                              psa_algorithm_t *psa_alg2,
                              psa_key_usage_t *usage,
                              mbedtls_pk_type_t key_type )
{
    if( strcmp( alg1, "none" ) != 0 )
    {
        const char * algs[] = { alg1, alg2 };
        psa_algorithm_t *psa_algs[] = { psa_alg1, psa_alg2 };

        for ( int i = 0; i < 2; i++ )
        {
            if( strcmp( algs[i], "rsa-sign-pkcs1" ) == 0 )
            {
                *psa_algs[i] = PSA_ALG_RSA_PKCS1V15_SIGN( PSA_ALG_ANY_HASH );
                *usage |= PSA_KEY_USAGE_SIGN_HASH;
            }
            else if( strcmp( algs[i], "rsa-sign-pss" ) == 0 )
            {
                *psa_algs[i] = PSA_ALG_RSA_PSS( PSA_ALG_ANY_HASH );
                *usage |= PSA_KEY_USAGE_SIGN_HASH;
            }
            else if( strcmp( algs[i], "rsa-decrypt" ) == 0 )
            {
                *psa_algs[i] = PSA_ALG_RSA_PKCS1V15_CRYPT;
                *usage |= PSA_KEY_USAGE_DECRYPT;
            }
            else if( strcmp( algs[i], "ecdsa-sign" ) == 0 )
            {
                *psa_algs[i] = PSA_ALG_ECDSA( PSA_ALG_ANY_HASH );
                *usage |= PSA_KEY_USAGE_SIGN_HASH;
            }
            else if( strcmp( algs[i], "ecdh" ) == 0 )
            {
                *psa_algs[i] = PSA_ALG_ECDH;
                *usage |= PSA_KEY_USAGE_DERIVE;
            }
            else if( strcmp( algs[i], "none" ) == 0 )
            {
                *psa_algs[i] = PSA_ALG_NONE;
            }
        }
    }
    else
    {
        if( key_type == MBEDTLS_PK_ECKEY )
        {
            *psa_alg1 = PSA_ALG_ECDSA( PSA_ALG_ANY_HASH );
            *psa_alg2 = PSA_ALG_ECDH;
            *usage = PSA_KEY_USAGE_SIGN_HASH | PSA_KEY_USAGE_DERIVE;
        }
        else if( key_type == MBEDTLS_PK_RSA )
        {
            *psa_alg1 = PSA_ALG_RSA_PKCS1V15_SIGN( PSA_ALG_ANY_HASH );
            *psa_alg2 = PSA_ALG_RSA_PSS( PSA_ALG_ANY_HASH );
            *usage = PSA_KEY_USAGE_SIGN_HASH;
        }
        else
        {
            return 1;
        }
    }

    return 0;
}
#endif /* MBEDTLS_USE_PSA_CRYPTO */

#if defined(MBEDTLS_X509_TRUSTED_CERTIFICATE_CALLBACK)
int ca_callback( void *data, mbedtls_x509_crt const *child,
                 mbedtls_x509_crt **candidates )
{
    int ret = 0;
    mbedtls_x509_crt *ca = (mbedtls_x509_crt *) data;
    mbedtls_x509_crt *first;

    /* This is a test-only implementation of the CA callback
     * which always returns the entire list of trusted certificates.
     * Production implementations managing a large number of CAs
     * should use an efficient presentation and lookup for the
     * set of trusted certificates (such as a hashtable) and only
     * return those trusted certificates which satisfy basic
     * parental checks, such as the matching of child `Issuer`
     * and parent `Subject` field or matching key identifiers. */
    ((void) child);

    first = mbedtls_calloc( 1, sizeof( mbedtls_x509_crt ) );
    if( first == NULL )
    {
        ret = -1;
        goto exit;
    }
    mbedtls_x509_crt_init( first );

    if( mbedtls_x509_crt_parse_der( first, ca->raw.p, ca->raw.len ) != 0 )
    {
        ret = -1;
        goto exit;
    }

    while( ca->next != NULL )
    {
        ca = ca->next;
        if( mbedtls_x509_crt_parse_der( first, ca->raw.p, ca->raw.len ) != 0 )
        {
            ret = -1;
            goto exit;
        }
    }

exit:

    if( ret != 0 )
    {
        mbedtls_x509_crt_free( first );
        mbedtls_free( first );
        first = NULL;
    }

    *candidates = first;
    return( ret );
}
#endif /* MBEDTLS_X509_TRUSTED_CERTIFICATE_CALLBACK */

int delayed_recv( void *ctx, unsigned char *buf, size_t len )
{
    static int first_try = 1;
    int ret;

    if( first_try )
    {
        first_try = 0;
        return( MBEDTLS_ERR_SSL_WANT_READ );
    }

    ret = mbedtls_net_recv( ctx, buf, len );
    if( ret != MBEDTLS_ERR_SSL_WANT_READ )
        first_try = 1; /* Next call will be a new operation */
    return( ret );
}

int delayed_send( void *ctx, const unsigned char *buf, size_t len )
{
    static int first_try = 1;
    int ret;

    if( first_try )
    {
        first_try = 0;
        return( MBEDTLS_ERR_SSL_WANT_WRITE );
    }

    ret = mbedtls_net_send( ctx, buf, len );
    if( ret != MBEDTLS_ERR_SSL_WANT_WRITE )
        first_try = 1; /* Next call will be a new operation */
    return( ret );
}

#if !defined(MBEDTLS_TIMING_C)
int idle( mbedtls_net_context *fd,
          int idle_reason )
#else
int idle( mbedtls_net_context *fd,
          mbedtls_timing_delay_context *timer,
          int idle_reason )
#endif
{
    int ret;
    int poll_type = 0;

    if( idle_reason == MBEDTLS_ERR_SSL_WANT_WRITE )
        poll_type = MBEDTLS_NET_POLL_WRITE;
    else if( idle_reason == MBEDTLS_ERR_SSL_WANT_READ )
        poll_type = MBEDTLS_NET_POLL_READ;
#if !defined(MBEDTLS_TIMING_C)
    else
        return( 0 );
#endif

    while( 1 )
    {
        /* Check if timer has expired */
#if defined(MBEDTLS_TIMING_C)
        if( timer != NULL &&
            mbedtls_timing_get_delay( timer ) == 2 )
        {
            break;
        }
#endif /* MBEDTLS_TIMING_C */

        /* Check if underlying transport became available */
        if( poll_type != 0 )
        {
            ret = mbedtls_net_poll( fd, poll_type, 0 );
            if( ret < 0 )
                return( ret );
            if( ret == poll_type )
                break;
        }
    }

    return( 0 );
}

#if defined(MBEDTLS_TEST_HOOKS)

void test_hooks_init( void )
{
    mbedtls_test_info_reset( );

#if defined(MBEDTLS_TEST_MUTEX_USAGE)
    mbedtls_test_mutex_usage_init( );
#endif
}

int test_hooks_failure_detected( void )
{
#if defined(MBEDTLS_TEST_MUTEX_USAGE)
    /* Errors are reported via mbedtls_test_info. */
    mbedtls_test_mutex_usage_check( );
#endif

    if( mbedtls_test_info.result != MBEDTLS_TEST_RESULT_SUCCESS )
        return( 1 );
    return( 0 );
}

void test_hooks_free( void )
{
}

#endif /* MBEDTLS_TEST_HOOKS */

#endif /* !defined(MBEDTLS_SSL_TEST_IMPOSSIBLE) */

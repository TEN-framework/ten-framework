/*
 *  Self-test demonstration program
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

#include "mbedtls/build_info.h"

#include "mbedtls/entropy.h"
#include "mbedtls/hmac_drbg.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/dhm.h"
#include "mbedtls/gcm.h"
#include "mbedtls/ccm.h"
#include "mbedtls/cmac.h"
#include "mbedtls/md5.h"
#include "mbedtls/ripemd160.h"
#include "mbedtls/sha1.h"
#include "mbedtls/sha256.h"
#include "mbedtls/sha512.h"
#include "mbedtls/des.h"
#include "mbedtls/aes.h"
#include "mbedtls/camellia.h"
#include "mbedtls/aria.h"
#include "mbedtls/chacha20.h"
#include "mbedtls/poly1305.h"
#include "mbedtls/chachapoly.h"
#include "mbedtls/base64.h"
#include "mbedtls/bignum.h"
#include "mbedtls/rsa.h"
#include "mbedtls/x509.h"
#include "mbedtls/pkcs5.h"
#include "mbedtls/ecp.h"
#include "mbedtls/ecjpake.h"
#include "mbedtls/timing.h"
#include "mbedtls/nist_kw.h"
#include "mbedtls/debug.h"

#include <limits.h>
#include <string.h>

#if defined(MBEDTLS_PLATFORM_C)
#include "mbedtls/platform.h"
#else
#include <stdio.h>
#include <stdlib.h>
#define mbedtls_calloc     calloc
#define mbedtls_free       free
#define mbedtls_printf     printf
#define mbedtls_snprintf   snprintf
#define mbedtls_exit       exit
#define MBEDTLS_EXIT_SUCCESS EXIT_SUCCESS
#define MBEDTLS_EXIT_FAILURE EXIT_FAILURE
#endif

#if defined(MBEDTLS_MEMORY_BUFFER_ALLOC_C)
#include "mbedtls/memory_buffer_alloc.h"
#endif


#if defined MBEDTLS_SELF_TEST
/* Sanity check for malloc. This is not expected to fail, and is rather
 * intended to display potentially useful information about the platform,
 * in particular the behavior of malloc(0). */
static int calloc_self_test( int verbose )
{
    int failures = 0;
    void *empty1 = mbedtls_calloc( 0, 1 );
    void *empty2 = mbedtls_calloc( 0, 1 );
    void *buffer1 = mbedtls_calloc( 1, 1 );
    void *buffer2 = mbedtls_calloc( 1, 1 );

    if( empty1 == NULL && empty2 == NULL )
    {
        if( verbose )
            mbedtls_printf( "  CALLOC(0): passed (NULL)\n" );
    }
    else if( empty1 == NULL || empty2 == NULL )
    {
        if( verbose )
            mbedtls_printf( "  CALLOC(0): failed (mix of NULL and non-NULL)\n" );
        ++failures;
    }
    else if( empty1 == empty2 )
    {
        if( verbose )
            mbedtls_printf( "  CALLOC(0): passed (same non-null)\n" );
    }
    else
    {
        if( verbose )
            mbedtls_printf( "  CALLOC(0): passed (distinct non-null)\n" );
    }

    if( buffer1 == NULL || buffer2 == NULL )
    {
        if( verbose )
            mbedtls_printf( "  CALLOC(1): failed (NULL)\n" );
        ++failures;
    }
    else if( buffer1 == buffer2 )
    {
        if( verbose )
            mbedtls_printf( "  CALLOC(1): failed (same buffer twice)\n" );
        ++failures;
    }
    else
    {
        if( verbose )
            mbedtls_printf( "  CALLOC(1): passed\n" );
    }

    mbedtls_free( buffer1 );
    buffer1 = mbedtls_calloc( 1, 1 );
    if( buffer1 == NULL )
    {
        if( verbose )
            mbedtls_printf( "  CALLOC(1 again): failed (NULL)\n" );
        ++failures;
    }
    else
    {
        if( verbose )
            mbedtls_printf( "  CALLOC(1 again): passed\n" );
    }

    if( verbose )
        mbedtls_printf( "\n" );
    mbedtls_free( empty1 );
    mbedtls_free( empty2 );
    mbedtls_free( buffer1 );
    mbedtls_free( buffer2 );
    return( failures );
}
#endif /* MBEDTLS_SELF_TEST */

static int test_snprintf( size_t n, const char *ref_buf, int ref_ret )
{
    int ret;
    char buf[10] = "xxxxxxxxx";
    const char ref[10] = "xxxxxxxxx";

    ret = mbedtls_snprintf( buf, n, "%s", "123" );
    if( ret < 0 || (size_t) ret >= n )
        ret = -1;

    if( strncmp( ref_buf, buf, sizeof( buf ) ) != 0 ||
        ref_ret != ret ||
        memcmp( buf + n, ref + n, sizeof( buf ) - n ) != 0 )
    {
        return( 1 );
    }

    return( 0 );
}

static int run_test_snprintf( void )
{
    return( test_snprintf( 0, "xxxxxxxxx",  -1 ) != 0 ||
            test_snprintf( 1, "",           -1 ) != 0 ||
            test_snprintf( 2, "1",          -1 ) != 0 ||
            test_snprintf( 3, "12",         -1 ) != 0 ||
            test_snprintf( 4, "123",         3 ) != 0 ||
            test_snprintf( 5, "123",         3 ) != 0 );
}

/*
 * Check if a seed file is present, and if not create one for the entropy
 * self-test. If this fails, we attempt the test anyway, so no error is passed
 * back.
 */
#if defined(MBEDTLS_SELF_TEST) && defined(MBEDTLS_ENTROPY_C)
#if defined(MBEDTLS_ENTROPY_NV_SEED) && !defined(MBEDTLS_NO_PLATFORM_ENTROPY)
static void create_entropy_seed_file( void )
{
    int result;
    size_t output_len = 0;
    unsigned char seed_value[MBEDTLS_ENTROPY_BLOCK_SIZE];

    /* Attempt to read the entropy seed file. If this fails - attempt to write
     * to the file to ensure one is present. */
    result = mbedtls_platform_std_nv_seed_read( seed_value,
                                                    MBEDTLS_ENTROPY_BLOCK_SIZE );
    if( 0 == result )
        return;

    result = mbedtls_platform_entropy_poll( NULL,
                                            seed_value,
                                            MBEDTLS_ENTROPY_BLOCK_SIZE,
                                            &output_len );
    if( 0 != result )
        return;

    if( MBEDTLS_ENTROPY_BLOCK_SIZE != output_len )
        return;

    mbedtls_platform_std_nv_seed_write( seed_value, MBEDTLS_ENTROPY_BLOCK_SIZE );
}
#endif

int mbedtls_entropy_self_test_wrapper( int verbose )
{
#if defined(MBEDTLS_ENTROPY_NV_SEED) && !defined(MBEDTLS_NO_PLATFORM_ENTROPY)
    create_entropy_seed_file( );
#endif
    return( mbedtls_entropy_self_test( verbose ) );
}
#endif

#if defined(MBEDTLS_SELF_TEST)
#if defined(MBEDTLS_MEMORY_BUFFER_ALLOC_C)
int mbedtls_memory_buffer_alloc_free_and_self_test( int verbose )
{
    if( verbose != 0 )
    {
#if defined(MBEDTLS_MEMORY_DEBUG)
        mbedtls_memory_buffer_alloc_status( );
#endif
    }
    mbedtls_memory_buffer_alloc_free( );
    return( mbedtls_memory_buffer_alloc_self_test( verbose ) );
}
#endif

typedef struct
{
    const char *name;
    int ( *function )( int );
} selftest_t;

const selftest_t selftests[] =
{
    {"calloc", calloc_self_test},
#if defined(MBEDTLS_MD5_C)
    {"md5", mbedtls_md5_self_test},
#endif
#if defined(MBEDTLS_RIPEMD160_C)
    {"ripemd160", mbedtls_ripemd160_self_test},
#endif
#if defined(MBEDTLS_SHA1_C)
    {"sha1", mbedtls_sha1_self_test},
#endif
#if defined(MBEDTLS_SHA256_C)
    {"sha256", mbedtls_sha256_self_test},
#endif
#if defined(MBEDTLS_SHA512_C)
    {"sha512", mbedtls_sha512_self_test},
#endif
#if defined(MBEDTLS_DES_C)
    {"des", mbedtls_des_self_test},
#endif
#if defined(MBEDTLS_AES_C)
    {"aes", mbedtls_aes_self_test},
#endif
#if defined(MBEDTLS_GCM_C) && defined(MBEDTLS_AES_C)
    {"gcm", mbedtls_gcm_self_test},
#endif
#if defined(MBEDTLS_CCM_C) && defined(MBEDTLS_AES_C)
    {"ccm", mbedtls_ccm_self_test},
#endif
#if defined(MBEDTLS_NIST_KW_C) && defined(MBEDTLS_AES_C)
    {"nist_kw", mbedtls_nist_kw_self_test},
#endif
#if defined(MBEDTLS_CMAC_C)
    {"cmac", mbedtls_cmac_self_test},
#endif
#if defined(MBEDTLS_CHACHA20_C)
    {"chacha20", mbedtls_chacha20_self_test},
#endif
#if defined(MBEDTLS_POLY1305_C)
    {"poly1305", mbedtls_poly1305_self_test},
#endif
#if defined(MBEDTLS_CHACHAPOLY_C)
    {"chacha20-poly1305", mbedtls_chachapoly_self_test},
#endif
#if defined(MBEDTLS_BASE64_C)
    {"base64", mbedtls_base64_self_test},
#endif
#if defined(MBEDTLS_BIGNUM_C)
    {"mpi", mbedtls_mpi_self_test},
#endif
#if defined(MBEDTLS_RSA_C)
    {"rsa", mbedtls_rsa_self_test},
#endif
#if defined(MBEDTLS_CAMELLIA_C)
    {"camellia", mbedtls_camellia_self_test},
#endif
#if defined(MBEDTLS_ARIA_C)
    {"aria", mbedtls_aria_self_test},
#endif
#if defined(MBEDTLS_CTR_DRBG_C)
    {"ctr_drbg", mbedtls_ctr_drbg_self_test},
#endif
#if defined(MBEDTLS_HMAC_DRBG_C)
    {"hmac_drbg", mbedtls_hmac_drbg_self_test},
#endif
#if defined(MBEDTLS_ECP_C)
    {"ecp", mbedtls_ecp_self_test},
#endif
#if defined(MBEDTLS_ECJPAKE_C)
    {"ecjpake", mbedtls_ecjpake_self_test},
#endif
#if defined(MBEDTLS_DHM_C)
    {"dhm", mbedtls_dhm_self_test},
#endif
#if defined(MBEDTLS_ENTROPY_C)
    {"entropy", mbedtls_entropy_self_test_wrapper},
#endif
#if defined(MBEDTLS_PKCS5_C)
    {"pkcs5", mbedtls_pkcs5_self_test},
#endif
/* Heap test comes last */
#if defined(MBEDTLS_MEMORY_BUFFER_ALLOC_C)
    {"memory_buffer_alloc", mbedtls_memory_buffer_alloc_free_and_self_test},
#endif
    {NULL, NULL}
};
#endif /* MBEDTLS_SELF_TEST */

int main( int argc, char *argv[] )
{
#if defined(MBEDTLS_SELF_TEST)
    const selftest_t *test;
#endif /* MBEDTLS_SELF_TEST */
    char **argp;
    int v = 1; /* v=1 for verbose mode */
    int exclude_mode = 0;
    int suites_tested = 0, suites_failed = 0;
#if defined(MBEDTLS_MEMORY_BUFFER_ALLOC_C) && defined(MBEDTLS_SELF_TEST)
    unsigned char buf[1000000];
#endif
    void *pointer;

    /*
     * The C standard doesn't guarantee that all-bits-0 is the representation
     * of a NULL pointer. We do however use that in our code for initializing
     * structures, which should work on every modern platform. Let's be sure.
     */
    memset( &pointer, 0, sizeof( void * ) );
    if( pointer != NULL )
    {
        mbedtls_printf( "all-bits-zero is not a NULL pointer\n" );
        mbedtls_exit( MBEDTLS_EXIT_FAILURE );
    }

    /*
     * The C standard allows padding bits in the representation
     * of standard integer types, but our code does currently not
     * support them.
     *
     * Here we check that the underlying C implementation doesn't
     * use padding bits, and fail cleanly if it does.
     *
     * The check works by casting the maximum value representable
     * by a given integer type into the unpadded integer type of the
     * same bit-width and checking that it agrees with the maximum value
     * of that unpadded type. For example, for a 4-byte int,
     * MAX_INT should be 0x7fffffff in int32_t. This assumes that
     * CHAR_BIT == 8, which is checked in check_config.h.
     *
     * We assume that [u]intxx_t exist and that they don't
     * have padding bits, as the standard requires.
     */

#define CHECK_PADDING_SIGNED(TYPE, NAME)                                \
    do                                                                  \
    {                                                                   \
        if( sizeof( TYPE ) == 2 || sizeof( TYPE ) == 4 ||               \
                sizeof( TYPE ) == 8 ) {                                 \
            if( ( sizeof( TYPE ) == 2 &&                                \
                (int16_t) NAME ## _MAX != 0x7FFF )             ||       \
                ( sizeof( TYPE ) == 4 &&                                \
                (int32_t) NAME ## _MAX != 0x7FFFFFFF )         ||       \
                ( sizeof( TYPE ) == 8 &&                                \
                (int64_t) NAME ## _MAX != 0x7FFFFFFFFFFFFFFF ) )        \
            {                                                           \
                mbedtls_printf( "Type '" #TYPE "' has padding bits\n" );\
                mbedtls_exit( MBEDTLS_EXIT_FAILURE );                   \
            }                                                           \
        } else {                                                        \
            mbedtls_printf( "Padding checks only implemented for types of size 2, 4 or 8" \
                " - cannot check type '" #TYPE "' of size %" MBEDTLS_PRINTF_SIZET "\n",       \
                sizeof( TYPE ) );                                       \
            mbedtls_exit( MBEDTLS_EXIT_FAILURE );                       \
        }                                                               \
    } while( 0 )

#define CHECK_PADDING_UNSIGNED(TYPE, NAME)                              \
    do                                                                  \
    {                                                                   \
        if( ( sizeof( TYPE ) == 2 &&                                    \
              (uint16_t) NAME ## _MAX != 0xFFFF )             ||        \
            ( sizeof( TYPE ) == 4 &&                                    \
              (uint32_t) NAME ## _MAX != 0xFFFFFFFF )         ||        \
            ( sizeof( TYPE ) == 8 &&                                    \
              (uint64_t) NAME ## _MAX != 0xFFFFFFFFFFFFFFFF ) )         \
        {                                                               \
            mbedtls_printf( "Type '" #TYPE "' has padding bits\n" );    \
            mbedtls_exit( MBEDTLS_EXIT_FAILURE );                       \
        }                                                               \
    } while( 0 )

    CHECK_PADDING_SIGNED( short,        SHRT );
    CHECK_PADDING_SIGNED( int,           INT );
    CHECK_PADDING_SIGNED( long,         LONG );
    CHECK_PADDING_SIGNED( long long,   LLONG );
    CHECK_PADDING_SIGNED( ptrdiff_t, PTRDIFF );

    CHECK_PADDING_UNSIGNED( unsigned short,      USHRT );
    CHECK_PADDING_UNSIGNED( unsigned,             UINT );
    CHECK_PADDING_UNSIGNED( unsigned long,       ULONG );
    CHECK_PADDING_UNSIGNED( unsigned long long, ULLONG );
    CHECK_PADDING_UNSIGNED( size_t,               SIZE );

#undef CHECK_PADDING_SIGNED
#undef CHECK_PADDING_UNSIGNED

    /*
     * Make sure we have a snprintf that correctly zero-terminates
     */
    if( run_test_snprintf() != 0 )
    {
        mbedtls_printf( "the snprintf implementation is broken\n" );
        mbedtls_exit( MBEDTLS_EXIT_FAILURE );
    }

    for( argp = argv + ( argc >= 1 ? 1 : argc ); *argp != NULL; ++argp )
    {
        if( strcmp( *argp, "--quiet" ) == 0 ||
            strcmp( *argp, "-q" ) == 0 )
        {
            v = 0;
        }
        else if( strcmp( *argp, "--exclude" ) == 0 ||
                 strcmp( *argp, "-x" ) == 0 )
        {
            exclude_mode = 1;
        }
        else
            break;
    }

    if( v != 0 )
        mbedtls_printf( "\n" );

#if defined(MBEDTLS_SELF_TEST)

#if defined(MBEDTLS_MEMORY_BUFFER_ALLOC_C)
    mbedtls_memory_buffer_alloc_init( buf, sizeof(buf) );
#endif

    if( *argp != NULL && exclude_mode == 0 )
    {
        /* Run the specified tests */
        for( ; *argp != NULL; argp++ )
        {
            for( test = selftests; test->name != NULL; test++ )
            {
                if( !strcmp( *argp, test->name ) )
                {
                    if( test->function( v )  != 0 )
                    {
                        suites_failed++;
                    }
                    suites_tested++;
                    break;
                }
            }
            if( test->name == NULL )
            {
                mbedtls_printf( "  Test suite %s not available -> failed\n\n", *argp );
                suites_failed++;
            }
        }
    }
    else
    {
        /* Run all the tests except excluded ones */
        for( test = selftests; test->name != NULL; test++ )
        {
            if( exclude_mode )
            {
                char **excluded;
                for( excluded = argp; *excluded != NULL; ++excluded )
                {
                    if( !strcmp( *excluded, test->name ) )
                        break;
                }
                if( *excluded )
                {
                    if( v )
                        mbedtls_printf( "  Skip: %s\n", test->name );
                    continue;
                }
            }
            if( test->function( v )  != 0 )
            {
                suites_failed++;
            }
            suites_tested++;
        }
    }

#else
    (void) exclude_mode;
    mbedtls_printf( " MBEDTLS_SELF_TEST not defined.\n" );
#endif

    if( v != 0 )
    {
        mbedtls_printf( "  Executed %d test suites\n\n", suites_tested );

        if( suites_failed > 0)
        {
            mbedtls_printf( "  [ %d tests FAIL ]\n\n", suites_failed );
        }
        else
        {
            mbedtls_printf( "  [ All tests PASS ]\n\n" );
        }
    }

    if( suites_failed > 0)
        mbedtls_exit( MBEDTLS_EXIT_FAILURE );

    mbedtls_exit( MBEDTLS_EXIT_SUCCESS );
}

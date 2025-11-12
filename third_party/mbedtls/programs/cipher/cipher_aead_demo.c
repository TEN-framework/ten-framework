/**
 * Cipher API multi-part AEAD demonstration.
 *
 * This program AEAD-encrypts a message, using the algorithm and key size
 * specified on the command line, using the multi-part API.
 *
 * It comes with a companion program psa/aead_demo.c, which does the same
 * operations with the PSA Crypto API. The goal is that comparing the two
 * programs will help people migrating to the PSA Crypto API.
 *
 * When used with multi-part AEAD operations, the `mbedtls_cipher_context`
 * serves a triple purpose (1) hold the key, (2) store the algorithm when no
 * operation is active, and (3) save progress information for the current
 * operation. With PSA those roles are held by disinct objects: (1) a
 * psa_key_id_t to hold the key, a (2) psa_algorithm_t to represent the
 * algorithm, and (3) a psa_operation_t for multi-part progress.
 *
 * On the other hand, with PSA, the algorithms encodes the desired tag length;
 * with Cipher the desired tag length needs to be tracked separately.
 *
 * This program and its companion psa/aead_demo.c illustrate this by doing the
 * same sequence of multi-part AEAD computation with both APIs; looking at the
 * two side by side should make the differences and similarities clear.
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

/* First include Mbed TLS headers to get the Mbed TLS configuration and
 * platform definitions that we'll use in this program. Also include
 * standard C headers for functions we'll use here. */
#include "mbedtls/build_info.h"

#include "mbedtls/cipher.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* If the build options we need are not enabled, compile a placeholder. */
#if !defined(MBEDTLS_CIPHER_C) || \
    !defined(MBEDTLS_AES_C) || !defined(MBEDTLS_GCM_C) || \
    !defined(MBEDTLS_CHACHAPOLY_C)
int main( void )
{
    printf( "MBEDTLS_MD_C and/or "
            "MBEDTLS_AES_C and/or MBEDTLS_GCM_C and/or "
            "MBEDTLS_CHACHAPOLY_C not defined\r\n" );
    return( 0 );
}
#else

/* The real program starts here. */

const char usage[] =
"Usage: cipher_aead_demo [aes128-gcm|aes256-gcm|aes128-gcm_8|chachapoly]";

/* Dummy data for encryption: IV/nonce, additional data, 2-part message */
const unsigned char iv1[12] = { 0x00 };
const unsigned char add_data1[] = { 0x01, 0x02 };
const unsigned char msg1_part1[] = { 0x03, 0x04 };
const unsigned char msg1_part2[] = { 0x05, 0x06, 0x07 };

/* Dummy data (2nd message) */
const unsigned char iv2[12] = { 0x10 };
const unsigned char add_data2[] = { 0x11, 0x12 };
const unsigned char msg2_part1[] = { 0x13, 0x14 };
const unsigned char msg2_part2[] = { 0x15, 0x16, 0x17 };

/* Maximum total size of the messages */
#define MSG1_SIZE ( sizeof( msg1_part1 ) + sizeof( msg1_part2 ) )
#define MSG2_SIZE ( sizeof( msg2_part1 ) + sizeof( msg2_part2 ) )
#define MSG_MAX_SIZE ( MSG1_SIZE > MSG2_SIZE ? MSG1_SIZE : MSG2_SIZE )

/* Dummy key material - never do this in production!
 * 32-byte is enough to all the key size supported by this program. */
const unsigned char key_bytes[32] = { 0x2a };

/* Print the contents of a buffer in hex */
void print_buf( const char *title, unsigned char *buf, size_t len )
{
    printf( "%s:", title );
    for( size_t i = 0; i < len; i++ )
        printf( " %02x", buf[i] );
    printf( "\n" );
}

/* Run an Mbed TLS function and bail out if it fails.
 * A string description of the error code can be recovered with:
 * programs/util/strerror <value> */
#define CHK( expr )                                             \
    do                                                          \
    {                                                           \
        ret = ( expr );                                         \
        if( ret != 0 )                                          \
        {                                                       \
            printf( "Error %d at line %d: %s\n",                \
                    ret,                                        \
                    __LINE__,                                   \
                    #expr );                                    \
            goto exit;                                          \
        }                                                       \
    } while( 0 )

/*
 * Prepare encryption material:
 * - interpret command-line argument
 * - set up key
 * - outputs: context and tag length, which together hold all the information
 */
static int aead_prepare( const char *info,
                           mbedtls_cipher_context_t *ctx,
                           size_t *tag_len )
{
    int ret;

    /* Convert arg to type + tag_len */
    mbedtls_cipher_type_t type;
    if( strcmp( info, "aes128-gcm" ) == 0 ) {
        type = MBEDTLS_CIPHER_AES_128_GCM;
        *tag_len = 16;
    } else if( strcmp( info, "aes256-gcm" ) == 0 ) {
        type = MBEDTLS_CIPHER_AES_256_GCM;
        *tag_len = 16;
    } else if( strcmp( info, "aes128-gcm_8" ) == 0 ) {
        type = MBEDTLS_CIPHER_AES_128_GCM;
        *tag_len = 8;
    } else if( strcmp( info, "chachapoly" ) == 0 ) {
        type = MBEDTLS_CIPHER_CHACHA20_POLY1305;
        *tag_len = 16;
    } else {
        puts( usage );
        return( MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA );
    }

    /* Prepare context for the given type */
    CHK( mbedtls_cipher_setup( ctx,
                               mbedtls_cipher_info_from_type( type ) ) );

    /* Import key */
    int key_len = mbedtls_cipher_get_key_bitlen( ctx );
    CHK( mbedtls_cipher_setkey( ctx, key_bytes, key_len, MBEDTLS_ENCRYPT ) );

exit:
    return( ret );
}

/*
 * Print out some information.
 *
 * All of this information was present in the command line argument, but his
 * function demonstrates how each piece can be recovered from (ctx, tag_len).
 */
static void aead_info( const mbedtls_cipher_context_t *ctx, size_t tag_len )
{
    mbedtls_cipher_type_t type = mbedtls_cipher_get_type( ctx );
    const mbedtls_cipher_info_t *info = mbedtls_cipher_info_from_type( type );
    const char *ciph = mbedtls_cipher_info_get_name( info );
    int key_bits = mbedtls_cipher_get_key_bitlen( ctx );
    mbedtls_cipher_mode_t mode = mbedtls_cipher_get_cipher_mode( ctx );

    const char *mode_str = mode == MBEDTLS_MODE_GCM ? "GCM"
                         : mode == MBEDTLS_MODE_CHACHAPOLY ? "ChachaPoly"
                         : "???";

    printf( "%s, %d, %s, %u\n",
            ciph, key_bits, mode_str, (unsigned) tag_len );
}

/*
 * Encrypt a 2-part message.
 */
static int aead_encrypt( mbedtls_cipher_context_t *ctx, size_t tag_len,
        const unsigned char *iv, size_t iv_len,
        const unsigned char *ad, size_t ad_len,
        const unsigned char *part1, size_t part1_len,
        const unsigned char *part2, size_t part2_len )
{
    int ret;
    size_t olen;
#define MAX_TAG_LENGTH 16
    unsigned char out[MSG_MAX_SIZE + MAX_TAG_LENGTH];
    unsigned char *p = out;

    CHK( mbedtls_cipher_set_iv( ctx, iv, iv_len ) );
    CHK( mbedtls_cipher_reset( ctx ) );
    CHK( mbedtls_cipher_update_ad( ctx, ad, ad_len ) );
    CHK( mbedtls_cipher_update( ctx, part1, part1_len, p, &olen ) );
    p += olen;
    CHK( mbedtls_cipher_update( ctx, part2, part2_len, p, &olen ) );
    p += olen;
    CHK( mbedtls_cipher_finish( ctx, p, &olen ) );
    p += olen;
    CHK( mbedtls_cipher_write_tag( ctx, p, tag_len ) );
    p += tag_len;

    olen = p - out;
    print_buf( "out", out, olen );

exit:
    return( ret );
}

/*
 * AEAD demo: set up key/alg, print out info, encrypt messages.
 */
static int aead_demo( const char *info )
{
    int ret = 0;

    mbedtls_cipher_context_t ctx;
    size_t tag_len;

    mbedtls_cipher_init( &ctx );

    CHK( aead_prepare( info, &ctx, &tag_len ) );

    aead_info( &ctx, tag_len );

    CHK( aead_encrypt( &ctx, tag_len,
                         iv1, sizeof( iv1 ), add_data1, sizeof( add_data1 ),
                         msg1_part1, sizeof( msg1_part1 ),
                         msg1_part2, sizeof( msg1_part2 ) ) );
    CHK( aead_encrypt( &ctx, tag_len,
                         iv2, sizeof( iv2 ), add_data2, sizeof( add_data2 ),
                         msg2_part1, sizeof( msg2_part1 ),
                         msg2_part2, sizeof( msg2_part2 ) ) );

exit:
    mbedtls_cipher_free( &ctx );

    return( ret );
}


/*
 * Main function
 */
int main( int argc, char **argv )
{
    /* Check usage */
    if( argc != 2 )
    {
        puts( usage );
        return( 1 );
    }

    int ret;

    /* Run the demo */
    CHK( aead_demo( argv[1] ) );

exit:
    return( ret == 0 ? EXIT_SUCCESS : EXIT_FAILURE );
}

#endif

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

#include <test/helpers.h>
#include <test/macros.h>
#include <string.h>

/*----------------------------------------------------------------------------*/
/* Static global variables */

#if defined(MBEDTLS_PLATFORM_C)
static mbedtls_platform_context platform_ctx;
#endif

mbedtls_test_info_t mbedtls_test_info;

/*----------------------------------------------------------------------------*/
/* Helper Functions */

int mbedtls_test_platform_setup( void )
{
    int ret = 0;
#if defined(MBEDTLS_PLATFORM_C)
    ret = mbedtls_platform_setup( &platform_ctx );
#endif /* MBEDTLS_PLATFORM_C */
    return( ret );
}

void mbedtls_test_platform_teardown( void )
{
#if defined(MBEDTLS_PLATFORM_C)
    mbedtls_platform_teardown( &platform_ctx );
#endif /* MBEDTLS_PLATFORM_C */
}

static int ascii2uc(const char c, unsigned char *uc)
{
    if( ( c >= '0' ) && ( c <= '9' ) )
        *uc = c - '0';
    else if( ( c >= 'a' ) && ( c <= 'f' ) )
        *uc = c - 'a' + 10;
    else if( ( c >= 'A' ) && ( c <= 'F' ) )
        *uc = c - 'A' + 10;
    else
        return( -1 );

    return( 0 );
}

void mbedtls_test_fail( const char *test, int line_no, const char* filename )
{
    if( mbedtls_test_info.result == MBEDTLS_TEST_RESULT_FAILED )
    {
        /* We've already recorded the test as having failed. Don't
         * overwrite any previous information about the failure. */
        return;
    }
    mbedtls_test_info.result = MBEDTLS_TEST_RESULT_FAILED;
    mbedtls_test_info.test = test;
    mbedtls_test_info.line_no = line_no;
    mbedtls_test_info.filename = filename;
}

void mbedtls_test_skip( const char *test, int line_no, const char* filename )
{
    mbedtls_test_info.result = MBEDTLS_TEST_RESULT_SKIPPED;
    mbedtls_test_info.test = test;
    mbedtls_test_info.line_no = line_no;
    mbedtls_test_info.filename = filename;
}

void mbedtls_test_set_step( unsigned long step )
{
    mbedtls_test_info.step = step;
}

void mbedtls_test_info_reset( void )
{
    mbedtls_test_info.result = MBEDTLS_TEST_RESULT_SUCCESS;
    mbedtls_test_info.step = (unsigned long)( -1 );
    mbedtls_test_info.test = 0;
    mbedtls_test_info.line_no = 0;
    mbedtls_test_info.filename = 0;
    memset( mbedtls_test_info.line1, 0, sizeof( mbedtls_test_info.line1 ) );
    memset( mbedtls_test_info.line2, 0, sizeof( mbedtls_test_info.line2 ) );
}

int mbedtls_test_equal( const char *test, int line_no, const char* filename,
                        unsigned long long value1, unsigned long long value2 )
{
    if( value1 == value2 )
        return( 1 );
    if( mbedtls_test_info.result == MBEDTLS_TEST_RESULT_FAILED )
    {
        /* We've already recorded the test as having failed. Don't
         * overwrite any previous information about the failure. */
        return( 0 );
    }
    mbedtls_test_fail( test, line_no, filename );
    (void) mbedtls_snprintf( mbedtls_test_info.line1,
                             sizeof( mbedtls_test_info.line1 ),
                             "lhs = 0x%016llx = %lld",
                             value1, (long long) value1 );
    (void) mbedtls_snprintf( mbedtls_test_info.line2,
                             sizeof( mbedtls_test_info.line2 ),
                             "rhs = 0x%016llx = %lld",
                             value2, (long long) value2 );
    return( 0 );
}

int mbedtls_test_le_u( const char *test, int line_no, const char* filename,
                       unsigned long long value1, unsigned long long value2 )
{
    if( value1 <= value2 )
        return( 1 );
    if( mbedtls_test_info.result == MBEDTLS_TEST_RESULT_FAILED )
    {
        /* We've already recorded the test as having failed. Don't
         * overwrite any previous information about the failure. */
        return( 0 );
    }
    mbedtls_test_fail( test, line_no, filename );
    (void) mbedtls_snprintf( mbedtls_test_info.line1,
                             sizeof( mbedtls_test_info.line1 ),
                             "lhs = 0x%016llx = %llu",
                             value1, value1 );
    (void) mbedtls_snprintf( mbedtls_test_info.line2,
                             sizeof( mbedtls_test_info.line2 ),
                             "rhs = 0x%016llx = %llu",
                             value2, value2 );
    return( 0 );
}

int mbedtls_test_le_s( const char *test, int line_no, const char* filename,
                       long long value1, long long value2 )
{
    if( value1 <= value2 )
        return( 1 );
    if( mbedtls_test_info.result == MBEDTLS_TEST_RESULT_FAILED )
    {
        /* We've already recorded the test as having failed. Don't
         * overwrite any previous information about the failure. */
        return( 0 );
    }
    mbedtls_test_fail( test, line_no, filename );
    (void) mbedtls_snprintf( mbedtls_test_info.line1,
                             sizeof( mbedtls_test_info.line1 ),
                             "lhs = 0x%016llx = %lld",
                             (unsigned long long) value1, value1 );
    (void) mbedtls_snprintf( mbedtls_test_info.line2,
                             sizeof( mbedtls_test_info.line2 ),
                             "rhs = 0x%016llx = %lld",
                             (unsigned long long) value2, value2 );
    return( 0 );
}

int mbedtls_test_unhexify( unsigned char *obuf,
                           size_t obufmax,
                           const char *ibuf,
                           size_t *len )
{
    unsigned char uc, uc2;

    *len = strlen( ibuf );

    /* Must be even number of bytes. */
    if ( ( *len ) & 1 )
        return( -1 );
    *len /= 2;

    if ( (*len) > obufmax )
        return( -1 );

    while( *ibuf != 0 )
    {
        if ( ascii2uc( *(ibuf++), &uc ) != 0 )
            return( -1 );

        if ( ascii2uc( *(ibuf++), &uc2 ) != 0 )
            return( -1 );

        *(obuf++) = ( uc << 4 ) | uc2;
    }

    return( 0 );
}

void mbedtls_test_hexify( unsigned char *obuf,
                          const unsigned char *ibuf,
                          int len )
{
    unsigned char l, h;

    while( len != 0 )
    {
        h = *ibuf / 16;
        l = *ibuf % 16;

        if( h < 10 )
            *obuf++ = '0' + h;
        else
            *obuf++ = 'a' + h - 10;

        if( l < 10 )
            *obuf++ = '0' + l;
        else
            *obuf++ = 'a' + l - 10;

        ++ibuf;
        len--;
    }
}

unsigned char *mbedtls_test_zero_alloc( size_t len )
{
    void *p;
    size_t actual_len = ( len != 0 ) ? len : 1;

    p = mbedtls_calloc( 1, actual_len );
    TEST_HELPER_ASSERT( p != NULL );

    memset( p, 0x00, actual_len );

    return( p );
}

unsigned char *mbedtls_test_unhexify_alloc( const char *ibuf, size_t *olen )
{
    unsigned char *obuf;
    size_t len;

    *olen = strlen( ibuf ) / 2;

    if( *olen == 0 )
        return( mbedtls_test_zero_alloc( *olen ) );

    obuf = mbedtls_calloc( 1, *olen );
    TEST_HELPER_ASSERT( obuf != NULL );
    TEST_HELPER_ASSERT( mbedtls_test_unhexify( obuf, *olen, ibuf, &len ) == 0 );

    return( obuf );
}

int mbedtls_test_hexcmp( uint8_t * a, uint8_t * b,
                         uint32_t a_len, uint32_t b_len )
{
    int ret = 0;
    uint32_t i = 0;

    if( a_len != b_len )
        return( -1 );

    for( i = 0; i < a_len; i++ )
    {
        if( a[i] != b[i] )
        {
            ret = -1;
            break;
        }
    }
    return ret;
}

#if defined(MBEDTLS_TEST_HOOKS)
void mbedtls_test_err_add_check( int high, int low,
                                 const char *file, int line )
{
    /* Error codes are always negative (a value of zero is a success) however
     * their positive opposites can be easier to understand. The following
     * examples given in comments have been made positive for ease of
     * understanding. The structure of an error code is such:
     *
     *                                                shhhhhhhhlllllll
     *
     * s = sign bit.
     * h = high level error code (includes high level module ID (bits 12..14)
     *     and module-dependent error code (bits 7..11)).
     * l = low level error code.
     */
    if ( high > -0x1000 && high != 0 )
    /* high < 0001000000000000
     * No high level module ID bits are set.
     */
    {
        mbedtls_test_fail( "'high' is not a high-level error code",
                            line, file );
    }
    else if ( high < -0x7F80 )
    /* high > 0111111110000000
     * Error code is greater than the largest allowed high level module ID.
     */
    {
        mbedtls_test_fail( "'high' error code is greater than 15 bits",
                            line, file );
    }
    else if ( ( high & 0x7F ) != 0 )
    /* high & 0000000001111111
     * Error code contains low level error code bits.
     */
    {
        mbedtls_test_fail( "'high' contains a low-level error code",
                            line, file );
    }
    else if ( low < -0x007F )
    /* low >  0000000001111111
     * Error code contains high or module level error code bits.
     */
    {
        mbedtls_test_fail( "'low' error code is greater than 7 bits",
                            line, file );
    }
    else if ( low > 0 )
    {
        mbedtls_test_fail( "'low' error code is greater than zero",
                            line, file );
    }
}
#endif /* MBEDTLS_TEST_HOOKS */

#if defined(MBEDTLS_BIGNUM_C)
int mbedtls_test_read_mpi( mbedtls_mpi *X, int radix, const char *s )
{
    /* mbedtls_mpi_read_string() currently retains leading zeros.
     * It always allocates at least one limb for the value 0. */
    if( s[0] == 0 )
    {
        mbedtls_mpi_free( X );
        return( 0 );
    }
    else
        return( mbedtls_mpi_read_string( X, radix, s ) );
}
#endif

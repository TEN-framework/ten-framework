/*
 *  Benchmark demonstration program
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

#include "mbedtls/platform.h"
#if !defined(MBEDTLS_PLATFORM_C)
#include <stdio.h>
#include <stdlib.h>
#define mbedtls_exit       exit
#define mbedtls_printf     printf
#define mbedtls_free       free
#endif

#if !defined(MBEDTLS_HAVE_TIME)
int main( void )
{
    mbedtls_printf("MBEDTLS_HAVE_TIME not defined.\n");
    mbedtls_exit( 0 );
}
#else

#include <string.h>
#include <stdlib.h>

#include "mbedtls/md5.h"
#include "mbedtls/ripemd160.h"
#include "mbedtls/sha1.h"
#include "mbedtls/sha256.h"
#include "mbedtls/sha512.h"

#include "mbedtls/des.h"
#include "mbedtls/aes.h"
#include "mbedtls/aria.h"
#include "mbedtls/camellia.h"
#include "mbedtls/chacha20.h"
#include "mbedtls/gcm.h"
#include "mbedtls/ccm.h"
#include "mbedtls/chachapoly.h"
#include "mbedtls/cmac.h"
#include "mbedtls/poly1305.h"

#include "mbedtls/ctr_drbg.h"
#include "mbedtls/hmac_drbg.h"

#include "mbedtls/rsa.h"
#include "mbedtls/dhm.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/ecdh.h"

#include "mbedtls/error.h"

#ifndef asm
#define asm __asm
#endif

#if defined(_WIN32) && !defined(EFIX64) && !defined(EFI32)

#include <windows.h>
#include <process.h>

struct _hr_time
{
    LARGE_INTEGER start;
};

#else

#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <time.h>

struct _hr_time
{
    struct timeval start;
};

#endif /* _WIN32 && !EFIX64 && !EFI32 */

#if defined(MBEDTLS_MEMORY_BUFFER_ALLOC_C)
#include "mbedtls/memory_buffer_alloc.h"
#endif

static void mbedtls_set_alarm( int seconds );

/*
 * For heap usage estimates, we need an estimate of the overhead per allocated
 * block. ptmalloc2/3 (used in gnu libc for instance) uses 2 size_t per block,
 * so use that as our baseline.
 */
#define MEM_BLOCK_OVERHEAD  ( 2 * sizeof( size_t ) )

/*
 * Size to use for the alloc buffer if MEMORY_BUFFER_ALLOC_C is defined.
 */
#define HEAP_SIZE       (1u << 16)  /* 64k */

#define BUFSIZE         1024
#define HEADER_FORMAT   "  %-24s :  "
#define TITLE_LEN       25

#define OPTIONS                                                         \
    "md5, ripemd160, sha1, sha256, sha512,\n"                      \
    "des3, des, camellia, chacha20,\n"                  \
    "aes_cbc, aes_gcm, aes_ccm, aes_xts, chachapoly,\n"                 \
    "aes_cmac, des3_cmac, poly1305\n"                                   \
    "ctr_drbg, hmac_drbg\n"                                     \
    "rsa, dhm, ecdsa, ecdh.\n"

#if defined(MBEDTLS_ERROR_C)
#define PRINT_ERROR                                                     \
        mbedtls_strerror( ret, ( char * )tmp, sizeof( tmp ) );          \
        mbedtls_printf( "FAILED: %s\n", tmp );
#else
#define PRINT_ERROR                                                     \
        mbedtls_printf( "FAILED: -0x%04x\n", (unsigned int) -ret );
#endif

#define TIME_AND_TSC( TITLE, CODE )                                     \
do {                                                                    \
    unsigned long ii, jj, tsc;                                          \
    int ret = 0;                                                        \
                                                                        \
    mbedtls_printf( HEADER_FORMAT, TITLE );                             \
    fflush( stdout );                                                   \
                                                                        \
    mbedtls_set_alarm( 1 );                                             \
    for( ii = 1; ret == 0 && ! mbedtls_timing_alarmed; ii++ )           \
    {                                                                   \
        ret = CODE;                                                     \
    }                                                                   \
                                                                        \
    tsc = mbedtls_timing_hardclock();                                   \
    for( jj = 0; ret == 0 && jj < 1024; jj++ )                          \
    {                                                                   \
        ret = CODE;                                                     \
    }                                                                   \
                                                                        \
    if( ret != 0 )                                                      \
    {                                                                   \
        PRINT_ERROR;                                                    \
    }                                                                   \
    else                                                                \
    {                                                                   \
        mbedtls_printf( "%9lu KiB/s,  %9lu cycles/byte\n",              \
                         ii * BUFSIZE / 1024,                           \
                         ( mbedtls_timing_hardclock() - tsc )           \
                         / ( jj * BUFSIZE ) );                          \
    }                                                                   \
} while( 0 )

#if defined(MBEDTLS_MEMORY_BUFFER_ALLOC_C) && defined(MBEDTLS_MEMORY_DEBUG)

/* How much space to reserve for the title when printing heap usage results.
 * Updated manually as the output of the following command:
 *
 *  sed -n 's/.*[T]IME_PUBLIC.*"\(.*\)",/\1/p' programs/test/benchmark.c |
 *      awk '{print length+3}' | sort -rn | head -n1
 *
 * This computes the maximum length of a title +3, because we appends "/s" and
 * want at least one space. (If the value is too small, the only consequence
 * is poor alignment.) */
#define TITLE_SPACE 17

#define MEMORY_MEASURE_INIT                                             \
    size_t max_used, max_blocks, max_bytes;                             \
    size_t prv_used, prv_blocks;                                        \
    size_t alloc_cnt, free_cnt, prv_alloc, prv_free;                    \
    mbedtls_memory_buffer_alloc_cur_get( &prv_used, &prv_blocks );      \
    mbedtls_memory_buffer_alloc_max_reset( );

#define MEMORY_MEASURE_RESET                                            \
    mbedtls_memory_buffer_alloc_count_get( &prv_alloc, &prv_free );

#define MEMORY_MEASURE_PRINT( title_len )                               \
    mbedtls_memory_buffer_alloc_max_get( &max_used, &max_blocks );      \
    mbedtls_memory_buffer_alloc_count_get( &alloc_cnt, &free_cnt );     \
    ii = TITLE_SPACE > (title_len) ? TITLE_SPACE - (title_len) : 1;     \
    while( ii-- ) mbedtls_printf( " " );                                \
    max_used -= prv_used;                                               \
    max_blocks -= prv_blocks;                                           \
    max_bytes = max_used + MEM_BLOCK_OVERHEAD * max_blocks;             \
    mbedtls_printf( "%6u heap bytes, %6u allocs",                       \
                    (unsigned) max_bytes,                               \
                    (unsigned)( alloc_cnt - prv_alloc ) );

#else
#define MEMORY_MEASURE_INIT
#define MEMORY_MEASURE_RESET
#define MEMORY_MEASURE_PRINT( title_len )
#endif

#define TIME_PUBLIC( TITLE, TYPE, CODE )                                \
do {                                                                    \
    unsigned long ii;                                                   \
    int ret;                                                            \
    MEMORY_MEASURE_INIT;                                                \
                                                                        \
    mbedtls_printf( HEADER_FORMAT, TITLE );                             \
    fflush( stdout );                                                   \
    mbedtls_set_alarm( 3 );                                             \
                                                                        \
    ret = 0;                                                            \
    for( ii = 1; ! mbedtls_timing_alarmed && ! ret ; ii++ )             \
    {                                                                   \
        MEMORY_MEASURE_RESET;                                           \
        CODE;                                                           \
    }                                                                   \
                                                                        \
    if( ret == MBEDTLS_ERR_PLATFORM_FEATURE_UNSUPPORTED )               \
    {                                                                   \
        mbedtls_printf( "Feature Not Supported. Skipping.\n" );         \
        ret = 0;                                                        \
    }                                                                   \
    else if( ret != 0 )                                                 \
    {                                                                   \
        PRINT_ERROR;                                                    \
    }                                                                   \
    else                                                                \
    {                                                                   \
        mbedtls_printf( "%6lu " TYPE "/s", ii / 3 );                    \
        MEMORY_MEASURE_PRINT( sizeof( TYPE ) + 1 );                     \
        mbedtls_printf( "\n" );                                         \
    }                                                                   \
} while( 0 )

#if !defined(HAVE_HARDCLOCK) && defined(MBEDTLS_HAVE_ASM) &&  \
    ( defined(_MSC_VER) && defined(_M_IX86) ) || defined(__WATCOMC__)

#define HAVE_HARDCLOCK

static unsigned long mbedtls_timing_hardclock( void )
{
    unsigned long tsc;
    __asm   rdtsc
    __asm   mov  [tsc], eax
    return( tsc );
}
#endif /* !HAVE_HARDCLOCK && MBEDTLS_HAVE_ASM &&
          ( _MSC_VER && _M_IX86 ) || __WATCOMC__ */

/* some versions of mingw-64 have 32-bit longs even on x84_64 */
#if !defined(HAVE_HARDCLOCK) && defined(MBEDTLS_HAVE_ASM) &&  \
    defined(__GNUC__) && ( defined(__i386__) || (                       \
    ( defined(__amd64__) || defined( __x86_64__) ) && __SIZEOF_LONG__ == 4 ) )

#define HAVE_HARDCLOCK

static unsigned long mbedtls_timing_hardclock( void )
{
    unsigned long lo, hi;
    asm volatile( "rdtsc" : "=a" (lo), "=d" (hi) );
    return( lo );
}
#endif /* !HAVE_HARDCLOCK && MBEDTLS_HAVE_ASM &&
          __GNUC__ && __i386__ */

#if !defined(HAVE_HARDCLOCK) && defined(MBEDTLS_HAVE_ASM) &&  \
    defined(__GNUC__) && ( defined(__amd64__) || defined(__x86_64__) )

#define HAVE_HARDCLOCK

static unsigned long mbedtls_timing_hardclock( void )
{
    unsigned long lo, hi;
    asm volatile( "rdtsc" : "=a" (lo), "=d" (hi) );
    return( lo | ( hi << 32 ) );
}
#endif /* !HAVE_HARDCLOCK && MBEDTLS_HAVE_ASM &&
          __GNUC__ && ( __amd64__ || __x86_64__ ) */

#if !defined(HAVE_HARDCLOCK) && defined(MBEDTLS_HAVE_ASM) &&  \
    defined(__GNUC__) && ( defined(__powerpc__) || defined(__ppc__) )

#define HAVE_HARDCLOCK

static unsigned long mbedtls_timing_hardclock( void )
{
    unsigned long tbl, tbu0, tbu1;

    do
    {
        asm volatile( "mftbu %0" : "=r" (tbu0) );
        asm volatile( "mftb  %0" : "=r" (tbl ) );
        asm volatile( "mftbu %0" : "=r" (tbu1) );
    }
    while( tbu0 != tbu1 );

    return( tbl );
}
#endif /* !HAVE_HARDCLOCK && MBEDTLS_HAVE_ASM &&
          __GNUC__ && ( __powerpc__ || __ppc__ ) */

#if !defined(HAVE_HARDCLOCK) && defined(MBEDTLS_HAVE_ASM) &&  \
    defined(__GNUC__) && defined(__sparc64__)

#if defined(__OpenBSD__)
#warning OpenBSD does not allow access to tick register using software version instead
#else
#define HAVE_HARDCLOCK

static unsigned long mbedtls_timing_hardclock( void )
{
    unsigned long tick;
    asm volatile( "rdpr %%tick, %0;" : "=&r" (tick) );
    return( tick );
}
#endif /* __OpenBSD__ */
#endif /* !HAVE_HARDCLOCK && MBEDTLS_HAVE_ASM &&
          __GNUC__ && __sparc64__ */

#if !defined(HAVE_HARDCLOCK) && defined(MBEDTLS_HAVE_ASM) &&  \
    defined(__GNUC__) && defined(__sparc__) && !defined(__sparc64__)

#define HAVE_HARDCLOCK

static unsigned long mbedtls_timing_hardclock( void )
{
    unsigned long tick;
    asm volatile( ".byte 0x83, 0x41, 0x00, 0x00" );
    asm volatile( "mov   %%g1, %0" : "=r" (tick) );
    return( tick );
}
#endif /* !HAVE_HARDCLOCK && MBEDTLS_HAVE_ASM &&
          __GNUC__ && __sparc__ && !__sparc64__ */

#if !defined(HAVE_HARDCLOCK) && defined(MBEDTLS_HAVE_ASM) &&      \
    defined(__GNUC__) && defined(__alpha__)

#define HAVE_HARDCLOCK

static unsigned long mbedtls_timing_hardclock( void )
{
    unsigned long cc;
    asm volatile( "rpcc %0" : "=r" (cc) );
    return( cc & 0xFFFFFFFF );
}
#endif /* !HAVE_HARDCLOCK && MBEDTLS_HAVE_ASM &&
          __GNUC__ && __alpha__ */

#if !defined(HAVE_HARDCLOCK) && defined(MBEDTLS_HAVE_ASM) &&      \
    defined(__GNUC__) && defined(__ia64__)

#define HAVE_HARDCLOCK

static unsigned long mbedtls_timing_hardclock( void )
{
    unsigned long itc;
    asm volatile( "mov %0 = ar.itc" : "=r" (itc) );
    return( itc );
}
#endif /* !HAVE_HARDCLOCK && MBEDTLS_HAVE_ASM &&
          __GNUC__ && __ia64__ */

#if !defined(HAVE_HARDCLOCK) && defined(_MSC_VER) && \
    !defined(EFIX64) && !defined(EFI32)

#define HAVE_HARDCLOCK

static unsigned long mbedtls_timing_hardclock( void )
{
    LARGE_INTEGER offset;

    QueryPerformanceCounter( &offset );

    return( (unsigned long)( offset.QuadPart ) );
}
#endif /* !HAVE_HARDCLOCK && _MSC_VER && !EFIX64 && !EFI32 */

#if !defined(HAVE_HARDCLOCK)

#define HAVE_HARDCLOCK

static int hardclock_init = 0;
static struct timeval tv_init;

static unsigned long mbedtls_timing_hardclock( void )
{
    struct timeval tv_cur;

    if( hardclock_init == 0 )
    {
        gettimeofday( &tv_init, NULL );
        hardclock_init = 1;
    }

    gettimeofday( &tv_cur, NULL );
    return( ( tv_cur.tv_sec  - tv_init.tv_sec  ) * 1000000
          + ( tv_cur.tv_usec - tv_init.tv_usec ) );
}
#endif /* !HAVE_HARDCLOCK */

volatile int mbedtls_timing_alarmed = 0;

#if defined(_WIN32) && !defined(EFIX64) && !defined(EFI32)

/* It's OK to use a global because alarm() is supposed to be global anyway */
static DWORD alarmMs;

static void TimerProc( void *TimerContext )
{
    (void) TimerContext;
    Sleep( alarmMs );
    mbedtls_timing_alarmed = 1;
    /* _endthread will be called implicitly on return
     * That ensures execution of thread funcition's epilogue */
}

static void mbedtls_set_alarm( int seconds )
{
    if( seconds == 0 )
    {
        /* No need to create a thread for this simple case.
         * Also, this shorcut is more reliable at least on MinGW32 */
        mbedtls_timing_alarmed = 1;
        return;
    }

    mbedtls_timing_alarmed = 0;
    alarmMs = seconds * 1000;
    (void) _beginthread( TimerProc, 0, NULL );
}

#else /* _WIN32 && !EFIX64 && !EFI32 */

static void sighandler( int signum )
{
    mbedtls_timing_alarmed = 1;
    signal( signum, sighandler );
}

static void mbedtls_set_alarm( int seconds )
{
    mbedtls_timing_alarmed = 0;
    signal( SIGALRM, sighandler );
    alarm( seconds );
    if( seconds == 0 )
    {
        /* alarm(0) cancelled any previous pending alarm, but the
           handler won't fire, so raise the flag straight away. */
        mbedtls_timing_alarmed = 1;
    }
}

#endif /* _WIN32 && !EFIX64 && !EFI32 */

static int myrand( void *rng_state, unsigned char *output, size_t len )
{
    size_t use_len;
    int rnd;

    if( rng_state != NULL )
        rng_state  = NULL;

    while( len > 0 )
    {
        use_len = len;
        if( use_len > sizeof(int) )
            use_len = sizeof(int);

        rnd = rand();
        memcpy( output, &rnd, use_len );
        output += use_len;
        len -= use_len;
    }

    return( 0 );
}

#define CHECK_AND_CONTINUE( R )                                         \
    {                                                                   \
        int CHECK_AND_CONTINUE_ret = ( R );                             \
        if( CHECK_AND_CONTINUE_ret == MBEDTLS_ERR_PLATFORM_FEATURE_UNSUPPORTED ) { \
            mbedtls_printf( "Feature not supported. Skipping.\n" );     \
            continue;                                                   \
        }                                                               \
        else if( CHECK_AND_CONTINUE_ret != 0 ) {                        \
            mbedtls_exit( 1 );                                          \
        }                                                               \
    }

#if defined(MBEDTLS_ECP_C)
static int set_ecp_curve( const char *string, mbedtls_ecp_curve_info *curve )
{
    const mbedtls_ecp_curve_info *found =
        mbedtls_ecp_curve_info_from_name( string );
    if( found != NULL )
    {
        *curve = *found;
        return( 1 );
    }
    else
        return( 0 );
}
#endif

unsigned char buf[BUFSIZE];

typedef struct {
    char md5, ripemd160, sha1, sha256, sha512,
         des3, des,
         aes_cbc, aes_gcm, aes_ccm, aes_xts, chachapoly,
         aes_cmac, des3_cmac,
         aria, camellia, chacha20,
         poly1305,
         ctr_drbg, hmac_drbg,
         rsa, dhm, ecdsa, ecdh;
} todo_list;


int main( int argc, char *argv[] )
{
    int i;
    unsigned char tmp[200];
    char title[TITLE_LEN];
    todo_list todo;
#if defined(MBEDTLS_MEMORY_BUFFER_ALLOC_C)
    unsigned char alloc_buf[HEAP_SIZE] = { 0 };
#endif
#if defined(MBEDTLS_ECP_C)
    mbedtls_ecp_curve_info single_curve[2] = {
        { MBEDTLS_ECP_DP_NONE, 0, 0, NULL },
        { MBEDTLS_ECP_DP_NONE, 0, 0, NULL },
    };
    const mbedtls_ecp_curve_info *curve_list = mbedtls_ecp_curve_list( );
#endif

#if defined(MBEDTLS_ECP_C)
    (void) curve_list; /* Unused in some configurations where no benchmark uses ECC */
#endif

    if( argc <= 1 )
    {
        memset( &todo, 1, sizeof( todo ) );
    }
    else
    {
        memset( &todo, 0, sizeof( todo ) );

        for( i = 1; i < argc; i++ )
        {
            if( strcmp( argv[i], "md5" ) == 0 )
                todo.md5 = 1;
            else if( strcmp( argv[i], "ripemd160" ) == 0 )
                todo.ripemd160 = 1;
            else if( strcmp( argv[i], "sha1" ) == 0 )
                todo.sha1 = 1;
            else if( strcmp( argv[i], "sha256" ) == 0 )
                todo.sha256 = 1;
            else if( strcmp( argv[i], "sha512" ) == 0 )
                todo.sha512 = 1;
            else if( strcmp( argv[i], "des3" ) == 0 )
                todo.des3 = 1;
            else if( strcmp( argv[i], "des" ) == 0 )
                todo.des = 1;
            else if( strcmp( argv[i], "aes_cbc" ) == 0 )
                todo.aes_cbc = 1;
            else if( strcmp( argv[i], "aes_xts" ) == 0 )
                todo.aes_xts = 1;
            else if( strcmp( argv[i], "aes_gcm" ) == 0 )
                todo.aes_gcm = 1;
            else if( strcmp( argv[i], "aes_ccm" ) == 0 )
                todo.aes_ccm = 1;
            else if( strcmp( argv[i], "chachapoly" ) == 0 )
                todo.chachapoly = 1;
            else if( strcmp( argv[i], "aes_cmac" ) == 0 )
                todo.aes_cmac = 1;
            else if( strcmp( argv[i], "des3_cmac" ) == 0 )
                todo.des3_cmac = 1;
            else if( strcmp( argv[i], "aria" ) == 0 )
                todo.aria = 1;
            else if( strcmp( argv[i], "camellia" ) == 0 )
                todo.camellia = 1;
            else if( strcmp( argv[i], "chacha20" ) == 0 )
                todo.chacha20 = 1;
            else if( strcmp( argv[i], "poly1305" ) == 0 )
                todo.poly1305 = 1;
            else if( strcmp( argv[i], "ctr_drbg" ) == 0 )
                todo.ctr_drbg = 1;
            else if( strcmp( argv[i], "hmac_drbg" ) == 0 )
                todo.hmac_drbg = 1;
            else if( strcmp( argv[i], "rsa" ) == 0 )
                todo.rsa = 1;
            else if( strcmp( argv[i], "dhm" ) == 0 )
                todo.dhm = 1;
            else if( strcmp( argv[i], "ecdsa" ) == 0 )
                todo.ecdsa = 1;
            else if( strcmp( argv[i], "ecdh" ) == 0 )
                todo.ecdh = 1;
#if defined(MBEDTLS_ECP_C)
            else if( set_ecp_curve( argv[i], single_curve ) )
                curve_list = single_curve;
#endif
            else
            {
                mbedtls_printf( "Unrecognized option: %s\n", argv[i] );
                mbedtls_printf( "Available options: " OPTIONS );
            }
        }
    }

    mbedtls_printf( "\n" );

#if defined(MBEDTLS_MEMORY_BUFFER_ALLOC_C)
    mbedtls_memory_buffer_alloc_init( alloc_buf, sizeof( alloc_buf ) );
#endif
    memset( buf, 0xAA, sizeof( buf ) );
    memset( tmp, 0xBB, sizeof( tmp ) );

    /* Avoid "unused static function" warning in configurations without
     * symmetric crypto. */
    (void) mbedtls_timing_hardclock;

#if defined(MBEDTLS_MD5_C)
    if( todo.md5 )
        TIME_AND_TSC( "MD5", mbedtls_md5( buf, BUFSIZE, tmp ) );
#endif

#if defined(MBEDTLS_RIPEMD160_C)
    if( todo.ripemd160 )
        TIME_AND_TSC( "RIPEMD160", mbedtls_ripemd160( buf, BUFSIZE, tmp ) );
#endif

#if defined(MBEDTLS_SHA1_C)
    if( todo.sha1 )
        TIME_AND_TSC( "SHA-1", mbedtls_sha1( buf, BUFSIZE, tmp ) );
#endif

#if defined(MBEDTLS_SHA256_C)
    if( todo.sha256 )
        TIME_AND_TSC( "SHA-256", mbedtls_sha256( buf, BUFSIZE, tmp, 0 ) );
#endif

#if defined(MBEDTLS_SHA512_C)
    if( todo.sha512 )
        TIME_AND_TSC( "SHA-512", mbedtls_sha512( buf, BUFSIZE, tmp, 0 ) );
#endif

#if defined(MBEDTLS_DES_C)
#if defined(MBEDTLS_CIPHER_MODE_CBC)
    if( todo.des3 )
    {
        mbedtls_des3_context des3;
        mbedtls_des3_init( &des3 );
        if( mbedtls_des3_set3key_enc( &des3, tmp ) != 0 )
            mbedtls_exit( 1 );
        TIME_AND_TSC( "3DES",
                mbedtls_des3_crypt_cbc( &des3, MBEDTLS_DES_ENCRYPT, BUFSIZE, tmp, buf, buf ) );
        mbedtls_des3_free( &des3 );
    }

    if( todo.des )
    {
        mbedtls_des_context des;
        mbedtls_des_init( &des );
        if( mbedtls_des_setkey_enc( &des, tmp ) != 0 )
            mbedtls_exit( 1 );
        TIME_AND_TSC( "DES",
                mbedtls_des_crypt_cbc( &des, MBEDTLS_DES_ENCRYPT, BUFSIZE, tmp, buf, buf ) );
        mbedtls_des_free( &des );
    }

#endif /* MBEDTLS_CIPHER_MODE_CBC */
#if defined(MBEDTLS_CMAC_C)
    if( todo.des3_cmac )
    {
        unsigned char output[8];
        const mbedtls_cipher_info_t *cipher_info;

        memset( buf, 0, sizeof( buf ) );
        memset( tmp, 0, sizeof( tmp ) );

        cipher_info = mbedtls_cipher_info_from_type( MBEDTLS_CIPHER_DES_EDE3_ECB );

        TIME_AND_TSC( "3DES-CMAC",
                      mbedtls_cipher_cmac( cipher_info, tmp, 192, buf,
                      BUFSIZE, output ) );
    }
#endif /* MBEDTLS_CMAC_C */
#endif /* MBEDTLS_DES_C */

#if defined(MBEDTLS_AES_C)
#if defined(MBEDTLS_CIPHER_MODE_CBC)
    if( todo.aes_cbc )
    {
        int keysize;
        mbedtls_aes_context aes;
        mbedtls_aes_init( &aes );
        for( keysize = 128; keysize <= 256; keysize += 64 )
        {
            mbedtls_snprintf( title, sizeof( title ), "AES-CBC-%d", keysize );

            memset( buf, 0, sizeof( buf ) );
            memset( tmp, 0, sizeof( tmp ) );
            CHECK_AND_CONTINUE( mbedtls_aes_setkey_enc( &aes, tmp, keysize ) );

            TIME_AND_TSC( title,
                mbedtls_aes_crypt_cbc( &aes, MBEDTLS_AES_ENCRYPT, BUFSIZE, tmp, buf, buf ) );
        }
        mbedtls_aes_free( &aes );
    }
#endif
#if defined(MBEDTLS_CIPHER_MODE_XTS)
    if( todo.aes_xts )
    {
        int keysize;
        mbedtls_aes_xts_context ctx;

        mbedtls_aes_xts_init( &ctx );
        for( keysize = 128; keysize <= 256; keysize += 128 )
        {
            mbedtls_snprintf( title, sizeof( title ), "AES-XTS-%d", keysize );

            memset( buf, 0, sizeof( buf ) );
            memset( tmp, 0, sizeof( tmp ) );
            CHECK_AND_CONTINUE( mbedtls_aes_xts_setkey_enc( &ctx, tmp, keysize * 2 ) );

            TIME_AND_TSC( title,
                    mbedtls_aes_crypt_xts( &ctx, MBEDTLS_AES_ENCRYPT, BUFSIZE,
                                           tmp, buf, buf ) );

            mbedtls_aes_xts_free( &ctx );
        }
    }
#endif
#if defined(MBEDTLS_GCM_C)
    if( todo.aes_gcm )
    {
        int keysize;
        mbedtls_gcm_context gcm;

        mbedtls_gcm_init( &gcm );
        for( keysize = 128; keysize <= 256; keysize += 64 )
        {
            mbedtls_snprintf( title, sizeof( title ), "AES-GCM-%d", keysize );

            memset( buf, 0, sizeof( buf ) );
            memset( tmp, 0, sizeof( tmp ) );
            mbedtls_gcm_setkey( &gcm, MBEDTLS_CIPHER_ID_AES, tmp, keysize );

            TIME_AND_TSC( title,
                    mbedtls_gcm_crypt_and_tag( &gcm, MBEDTLS_GCM_ENCRYPT, BUFSIZE, tmp,
                        12, NULL, 0, buf, buf, 16, tmp ) );

            mbedtls_gcm_free( &gcm );
        }
    }
#endif
#if defined(MBEDTLS_CCM_C)
    if( todo.aes_ccm )
    {
        int keysize;
        mbedtls_ccm_context ccm;

        mbedtls_ccm_init( &ccm );
        for( keysize = 128; keysize <= 256; keysize += 64 )
        {
            mbedtls_snprintf( title, sizeof( title ), "AES-CCM-%d", keysize );

            memset( buf, 0, sizeof( buf ) );
            memset( tmp, 0, sizeof( tmp ) );
            mbedtls_ccm_setkey( &ccm, MBEDTLS_CIPHER_ID_AES, tmp, keysize );

            TIME_AND_TSC( title,
                    mbedtls_ccm_encrypt_and_tag( &ccm, BUFSIZE, tmp,
                        12, NULL, 0, buf, buf, tmp, 16 ) );

            mbedtls_ccm_free( &ccm );
        }
    }
#endif
#if defined(MBEDTLS_CHACHAPOLY_C)
    if( todo.chachapoly )
    {
        mbedtls_chachapoly_context chachapoly;

        mbedtls_chachapoly_init( &chachapoly );
        memset( buf, 0, sizeof( buf ) );
        memset( tmp, 0, sizeof( tmp ) );

        mbedtls_snprintf( title, sizeof( title ), "ChaCha20-Poly1305" );

        mbedtls_chachapoly_setkey( &chachapoly, tmp );

        TIME_AND_TSC( title,
                mbedtls_chachapoly_encrypt_and_tag( &chachapoly,
                    BUFSIZE, tmp, NULL, 0, buf, buf, tmp ) );

        mbedtls_chachapoly_free( &chachapoly );
    }
#endif
#if defined(MBEDTLS_CMAC_C)
    if( todo.aes_cmac )
    {
        unsigned char output[16];
        const mbedtls_cipher_info_t *cipher_info;
        mbedtls_cipher_type_t cipher_type;
        int keysize;

        for( keysize = 128, cipher_type = MBEDTLS_CIPHER_AES_128_ECB;
             keysize <= 256;
             keysize += 64, cipher_type++ )
        {
            mbedtls_snprintf( title, sizeof( title ), "AES-CMAC-%d", keysize );

            memset( buf, 0, sizeof( buf ) );
            memset( tmp, 0, sizeof( tmp ) );

            cipher_info = mbedtls_cipher_info_from_type( cipher_type );

            TIME_AND_TSC( title,
                          mbedtls_cipher_cmac( cipher_info, tmp, keysize,
                                               buf, BUFSIZE, output ) );
        }

        memset( buf, 0, sizeof( buf ) );
        memset( tmp, 0, sizeof( tmp ) );
        TIME_AND_TSC( "AES-CMAC-PRF-128",
                      mbedtls_aes_cmac_prf_128( tmp, 16, buf, BUFSIZE,
                                                output ) );
    }
#endif /* MBEDTLS_CMAC_C */
#endif /* MBEDTLS_AES_C */

#if defined(MBEDTLS_ARIA_C) && defined(MBEDTLS_CIPHER_MODE_CBC)
    if( todo.aria )
    {
        int keysize;
        mbedtls_aria_context aria;
        mbedtls_aria_init( &aria );
        for( keysize = 128; keysize <= 256; keysize += 64 )
        {
            mbedtls_snprintf( title, sizeof( title ), "ARIA-CBC-%d", keysize );

            memset( buf, 0, sizeof( buf ) );
            memset( tmp, 0, sizeof( tmp ) );
            mbedtls_aria_setkey_enc( &aria, tmp, keysize );

            TIME_AND_TSC( title,
                    mbedtls_aria_crypt_cbc( &aria, MBEDTLS_ARIA_ENCRYPT,
                        BUFSIZE, tmp, buf, buf ) );
        }
        mbedtls_aria_free( &aria );
    }
#endif

#if defined(MBEDTLS_CAMELLIA_C) && defined(MBEDTLS_CIPHER_MODE_CBC)
    if( todo.camellia )
    {
        int keysize;
        mbedtls_camellia_context camellia;
        mbedtls_camellia_init( &camellia );
        for( keysize = 128; keysize <= 256; keysize += 64 )
        {
            mbedtls_snprintf( title, sizeof( title ), "CAMELLIA-CBC-%d", keysize );

            memset( buf, 0, sizeof( buf ) );
            memset( tmp, 0, sizeof( tmp ) );
            mbedtls_camellia_setkey_enc( &camellia, tmp, keysize );

            TIME_AND_TSC( title,
                    mbedtls_camellia_crypt_cbc( &camellia, MBEDTLS_CAMELLIA_ENCRYPT,
                        BUFSIZE, tmp, buf, buf ) );
        }
        mbedtls_camellia_free( &camellia );
    }
#endif

#if defined(MBEDTLS_CHACHA20_C)
    if ( todo.chacha20 )
    {
        TIME_AND_TSC( "ChaCha20", mbedtls_chacha20_crypt( buf, buf, 0U, BUFSIZE, buf, buf ) );
    }
#endif

#if defined(MBEDTLS_POLY1305_C)
    if ( todo.poly1305 )
    {
        TIME_AND_TSC( "Poly1305", mbedtls_poly1305_mac( buf, buf, BUFSIZE, buf ) );
    }
#endif

#if defined(MBEDTLS_CTR_DRBG_C)
    if( todo.ctr_drbg )
    {
        mbedtls_ctr_drbg_context ctr_drbg;

        mbedtls_ctr_drbg_init( &ctr_drbg );
        if( mbedtls_ctr_drbg_seed( &ctr_drbg, myrand, NULL, NULL, 0 ) != 0 )
            mbedtls_exit(1);
        TIME_AND_TSC( "CTR_DRBG (NOPR)",
                mbedtls_ctr_drbg_random( &ctr_drbg, buf, BUFSIZE ) );
        mbedtls_ctr_drbg_free( &ctr_drbg );

        mbedtls_ctr_drbg_init( &ctr_drbg );
        if( mbedtls_ctr_drbg_seed( &ctr_drbg, myrand, NULL, NULL, 0 ) != 0 )
            mbedtls_exit(1);
        mbedtls_ctr_drbg_set_prediction_resistance( &ctr_drbg, MBEDTLS_CTR_DRBG_PR_ON );
        TIME_AND_TSC( "CTR_DRBG (PR)",
                mbedtls_ctr_drbg_random( &ctr_drbg, buf, BUFSIZE ) );
        mbedtls_ctr_drbg_free( &ctr_drbg );
    }
#endif

#if defined(MBEDTLS_HMAC_DRBG_C)
    if( todo.hmac_drbg )
    {
        mbedtls_hmac_drbg_context hmac_drbg;
        const mbedtls_md_info_t *md_info;

        mbedtls_hmac_drbg_init( &hmac_drbg );

#if defined(MBEDTLS_SHA1_C)
        if( ( md_info = mbedtls_md_info_from_type( MBEDTLS_MD_SHA1 ) ) == NULL )
            mbedtls_exit(1);

        if( mbedtls_hmac_drbg_seed( &hmac_drbg, md_info, myrand, NULL, NULL, 0 ) != 0 )
            mbedtls_exit(1);
        TIME_AND_TSC( "HMAC_DRBG SHA-1 (NOPR)",
                mbedtls_hmac_drbg_random( &hmac_drbg, buf, BUFSIZE ) );

        if( mbedtls_hmac_drbg_seed( &hmac_drbg, md_info, myrand, NULL, NULL, 0 ) != 0 )
            mbedtls_exit(1);
        mbedtls_hmac_drbg_set_prediction_resistance( &hmac_drbg,
                                             MBEDTLS_HMAC_DRBG_PR_ON );
        TIME_AND_TSC( "HMAC_DRBG SHA-1 (PR)",
                mbedtls_hmac_drbg_random( &hmac_drbg, buf, BUFSIZE ) );
#endif

#if defined(MBEDTLS_SHA256_C)
        if( ( md_info = mbedtls_md_info_from_type( MBEDTLS_MD_SHA256 ) ) == NULL )
            mbedtls_exit(1);

        if( mbedtls_hmac_drbg_seed( &hmac_drbg, md_info, myrand, NULL, NULL, 0 ) != 0 )
            mbedtls_exit(1);
        TIME_AND_TSC( "HMAC_DRBG SHA-256 (NOPR)",
                mbedtls_hmac_drbg_random( &hmac_drbg, buf, BUFSIZE ) );

        if( mbedtls_hmac_drbg_seed( &hmac_drbg, md_info, myrand, NULL, NULL, 0 ) != 0 )
            mbedtls_exit(1);
        mbedtls_hmac_drbg_set_prediction_resistance( &hmac_drbg,
                                             MBEDTLS_HMAC_DRBG_PR_ON );
        TIME_AND_TSC( "HMAC_DRBG SHA-256 (PR)",
                mbedtls_hmac_drbg_random( &hmac_drbg, buf, BUFSIZE ) );
#endif
        mbedtls_hmac_drbg_free( &hmac_drbg );
    }
#endif

#if defined(MBEDTLS_RSA_C) && defined(MBEDTLS_GENPRIME)
    if( todo.rsa )
    {
        int keysize;
        mbedtls_rsa_context rsa;
        for( keysize = 2048; keysize <= 4096; keysize *= 2 )
        {
            mbedtls_snprintf( title, sizeof( title ), "RSA-%d", keysize );

            mbedtls_rsa_init( &rsa );
            mbedtls_rsa_gen_key( &rsa, myrand, NULL, keysize, 65537 );

            TIME_PUBLIC( title, " public",
                    buf[0] = 0;
                    ret = mbedtls_rsa_public( &rsa, buf, buf ) );

            TIME_PUBLIC( title, "private",
                    buf[0] = 0;
                    ret = mbedtls_rsa_private( &rsa, myrand, NULL, buf, buf ) );

            mbedtls_rsa_free( &rsa );
        }
    }
#endif

#if defined(MBEDTLS_DHM_C) && defined(MBEDTLS_BIGNUM_C)
    if( todo.dhm )
    {
        int dhm_sizes[] = { 2048, 3072 };
        static const unsigned char dhm_P_2048[] =
            MBEDTLS_DHM_RFC3526_MODP_2048_P_BIN;
        static const unsigned char dhm_P_3072[] =
            MBEDTLS_DHM_RFC3526_MODP_3072_P_BIN;
        static const unsigned char dhm_G_2048[] =
            MBEDTLS_DHM_RFC3526_MODP_2048_G_BIN;
        static const unsigned char dhm_G_3072[] =
            MBEDTLS_DHM_RFC3526_MODP_3072_G_BIN;

        const unsigned char *dhm_P[] = { dhm_P_2048, dhm_P_3072 };
        const size_t dhm_P_size[] = { sizeof( dhm_P_2048 ),
                                      sizeof( dhm_P_3072 ) };

        const unsigned char *dhm_G[] = { dhm_G_2048, dhm_G_3072 };
        const size_t dhm_G_size[] = { sizeof( dhm_G_2048 ),
                                      sizeof( dhm_G_3072 ) };

        mbedtls_dhm_context dhm;
        size_t olen;
        size_t n;
        for( i = 0; (size_t) i < sizeof( dhm_sizes ) / sizeof( dhm_sizes[0] ); i++ )
        {
            mbedtls_dhm_init( &dhm );

            if( mbedtls_mpi_read_binary( &dhm.P, dhm_P[i],
                                         dhm_P_size[i] ) != 0 ||
                mbedtls_mpi_read_binary( &dhm.G, dhm_G[i],
                                         dhm_G_size[i] ) != 0 )
            {
                mbedtls_exit( 1 );
            }

            n = mbedtls_mpi_size( &dhm.P );
            mbedtls_dhm_make_public( &dhm, (int) n, buf, n, myrand, NULL );
            if( mbedtls_mpi_copy( &dhm.GY, &dhm.GX ) != 0 )
                mbedtls_exit( 1 );

            mbedtls_snprintf( title, sizeof( title ), "DHE-%d", dhm_sizes[i] );
            TIME_PUBLIC( title, "handshake",
                    ret |= mbedtls_dhm_make_public( &dhm, (int) n, buf, n,
                                            myrand, NULL );
                    ret |= mbedtls_dhm_calc_secret( &dhm, buf, sizeof( buf ), &olen, myrand, NULL ) );

            mbedtls_snprintf( title, sizeof( title ), "DH-%d", dhm_sizes[i] );
            TIME_PUBLIC( title, "handshake",
                    ret |= mbedtls_dhm_calc_secret( &dhm, buf, sizeof( buf ), &olen, myrand, NULL ) );

            mbedtls_dhm_free( &dhm );
        }
    }
#endif

#if defined(MBEDTLS_ECDSA_C) && defined(MBEDTLS_SHA256_C)
    if( todo.ecdsa )
    {
        mbedtls_ecdsa_context ecdsa;
        const mbedtls_ecp_curve_info *curve_info;
        size_t sig_len;

        memset( buf, 0x2A, sizeof( buf ) );

        for( curve_info = curve_list;
             curve_info->grp_id != MBEDTLS_ECP_DP_NONE;
             curve_info++ )
        {
            if( ! mbedtls_ecdsa_can_do( curve_info->grp_id ) )
                continue;

            mbedtls_ecdsa_init( &ecdsa );

            if( mbedtls_ecdsa_genkey( &ecdsa, curve_info->grp_id, myrand, NULL ) != 0 )
                mbedtls_exit( 1 );

            mbedtls_snprintf( title, sizeof( title ), "ECDSA-%s",
                                              curve_info->name );
            TIME_PUBLIC( title, "sign",
                    ret = mbedtls_ecdsa_write_signature( &ecdsa, MBEDTLS_MD_SHA256, buf, curve_info->bit_size,
                                                tmp, sizeof( tmp ), &sig_len, myrand, NULL ) );

            mbedtls_ecdsa_free( &ecdsa );
        }

        for( curve_info = curve_list;
             curve_info->grp_id != MBEDTLS_ECP_DP_NONE;
             curve_info++ )
        {
            if( ! mbedtls_ecdsa_can_do( curve_info->grp_id ) )
                continue;

            mbedtls_ecdsa_init( &ecdsa );

            if( mbedtls_ecdsa_genkey( &ecdsa, curve_info->grp_id, myrand, NULL ) != 0 ||
                mbedtls_ecdsa_write_signature( &ecdsa, MBEDTLS_MD_SHA256, buf, curve_info->bit_size,
                                               tmp, sizeof( tmp ), &sig_len, myrand, NULL ) != 0 )
            {
                mbedtls_exit( 1 );
            }

            mbedtls_snprintf( title, sizeof( title ), "ECDSA-%s",
                                              curve_info->name );
            TIME_PUBLIC( title, "verify",
                    ret = mbedtls_ecdsa_read_signature( &ecdsa, buf, curve_info->bit_size,
                                                tmp, sig_len ) );

            mbedtls_ecdsa_free( &ecdsa );
        }
    }
#endif

#if defined(MBEDTLS_ECDH_C) && defined(MBEDTLS_ECDH_LEGACY_CONTEXT)
    if( todo.ecdh )
    {
        mbedtls_ecdh_context ecdh;
        mbedtls_mpi z;
        const mbedtls_ecp_curve_info montgomery_curve_list[] = {
#if defined(MBEDTLS_ECP_DP_CURVE25519_ENABLED)
            { MBEDTLS_ECP_DP_CURVE25519, 0, 0, "Curve25519" },
#endif
#if defined(MBEDTLS_ECP_DP_CURVE448_ENABLED)
            { MBEDTLS_ECP_DP_CURVE448, 0, 0, "Curve448" },
#endif
            { MBEDTLS_ECP_DP_NONE, 0, 0, 0 }
        };
        const mbedtls_ecp_curve_info *curve_info;
        size_t olen;
        const mbedtls_ecp_curve_info *selected_montgomery_curve_list =
            montgomery_curve_list;

        if( curve_list == (const mbedtls_ecp_curve_info*) &single_curve )
        {
            mbedtls_ecp_group grp;
            mbedtls_ecp_group_init( &grp );
            if( mbedtls_ecp_group_load( &grp, curve_list->grp_id ) != 0 )
                mbedtls_exit( 1 );
            if( mbedtls_ecp_get_type( &grp ) == MBEDTLS_ECP_TYPE_MONTGOMERY )
                selected_montgomery_curve_list = single_curve;
            else /* empty list */
                selected_montgomery_curve_list = single_curve + 1;
            mbedtls_ecp_group_free( &grp );
        }

        for( curve_info = curve_list;
             curve_info->grp_id != MBEDTLS_ECP_DP_NONE;
             curve_info++ )
        {
            if( ! mbedtls_ecdh_can_do( curve_info->grp_id ) )
                continue;

            mbedtls_ecdh_init( &ecdh );

            CHECK_AND_CONTINUE( mbedtls_ecp_group_load( &ecdh.grp, curve_info->grp_id ) );
            CHECK_AND_CONTINUE( mbedtls_ecdh_make_public( &ecdh, &olen, buf, sizeof( buf),
                                                    myrand, NULL ) );
            CHECK_AND_CONTINUE( mbedtls_ecp_copy( &ecdh.Qp, &ecdh.Q ) );

            mbedtls_snprintf( title, sizeof( title ), "ECDHE-%s",
                                              curve_info->name );
            TIME_PUBLIC( title, "handshake",
                    CHECK_AND_CONTINUE( mbedtls_ecdh_make_public( &ecdh, &olen, buf, sizeof( buf),
                                             myrand, NULL ) );
                    CHECK_AND_CONTINUE( mbedtls_ecdh_calc_secret( &ecdh, &olen, buf, sizeof( buf ),
                                             myrand, NULL ) ) );
            mbedtls_ecdh_free( &ecdh );
        }

        /* Montgomery curves need to be handled separately */
        for ( curve_info = selected_montgomery_curve_list;
              curve_info->grp_id != MBEDTLS_ECP_DP_NONE;
              curve_info++ )
        {
            mbedtls_ecdh_init( &ecdh );
            mbedtls_mpi_init( &z );

            CHECK_AND_CONTINUE( mbedtls_ecp_group_load( &ecdh.grp, curve_info->grp_id ) );
            CHECK_AND_CONTINUE( mbedtls_ecdh_gen_public( &ecdh.grp, &ecdh.d, &ecdh.Qp, myrand, NULL ) );

            mbedtls_snprintf( title, sizeof(title), "ECDHE-%s",
                              curve_info->name );
            TIME_PUBLIC(  title, "handshake",
                    CHECK_AND_CONTINUE( mbedtls_ecdh_gen_public( &ecdh.grp, &ecdh.d, &ecdh.Q,
                                            myrand, NULL ) );
                    CHECK_AND_CONTINUE( mbedtls_ecdh_compute_shared( &ecdh.grp, &z, &ecdh.Qp, &ecdh.d,
                                                myrand, NULL ) ) );

            mbedtls_ecdh_free( &ecdh );
            mbedtls_mpi_free( &z );
        }

        for( curve_info = curve_list;
             curve_info->grp_id != MBEDTLS_ECP_DP_NONE;
             curve_info++ )
        {
            if( ! mbedtls_ecdh_can_do( curve_info->grp_id ) )
                continue;

            mbedtls_ecdh_init( &ecdh );

            CHECK_AND_CONTINUE( mbedtls_ecp_group_load( &ecdh.grp, curve_info->grp_id ) );
            CHECK_AND_CONTINUE( mbedtls_ecdh_make_public( &ecdh, &olen, buf, sizeof( buf),
                                  myrand, NULL ) );
            CHECK_AND_CONTINUE( mbedtls_ecp_copy( &ecdh.Qp, &ecdh.Q ) );
            CHECK_AND_CONTINUE( mbedtls_ecdh_make_public( &ecdh, &olen, buf, sizeof( buf),
                                  myrand, NULL ) );

            mbedtls_snprintf( title, sizeof( title ), "ECDH-%s",
                                              curve_info->name );
            TIME_PUBLIC( title, "handshake",
                    CHECK_AND_CONTINUE( mbedtls_ecdh_calc_secret( &ecdh, &olen, buf, sizeof( buf ),
                                             myrand, NULL ) ) );
            mbedtls_ecdh_free( &ecdh );
        }

        /* Montgomery curves need to be handled separately */
        for ( curve_info = selected_montgomery_curve_list;
              curve_info->grp_id != MBEDTLS_ECP_DP_NONE;
              curve_info++)
        {
            mbedtls_ecdh_init( &ecdh );
            mbedtls_mpi_init( &z );

            CHECK_AND_CONTINUE( mbedtls_ecp_group_load( &ecdh.grp, curve_info->grp_id ) );
            CHECK_AND_CONTINUE( mbedtls_ecdh_gen_public( &ecdh.grp, &ecdh.d, &ecdh.Qp,
                                 myrand, NULL ) );
            CHECK_AND_CONTINUE( mbedtls_ecdh_gen_public( &ecdh.grp, &ecdh.d, &ecdh.Q, myrand, NULL ) );

            mbedtls_snprintf( title, sizeof(title), "ECDH-%s",
                              curve_info->name );
            TIME_PUBLIC(  title, "handshake",
                    CHECK_AND_CONTINUE( mbedtls_ecdh_compute_shared( &ecdh.grp, &z, &ecdh.Qp, &ecdh.d,
                                                myrand, NULL ) ) );

            mbedtls_ecdh_free( &ecdh );
            mbedtls_mpi_free( &z );
        }
    }
#endif

#if defined(MBEDTLS_ECDH_C)
    if( todo.ecdh )
    {
        mbedtls_ecdh_context ecdh_srv, ecdh_cli;
        unsigned char buf_srv[BUFSIZE], buf_cli[BUFSIZE];
        const mbedtls_ecp_curve_info *curve_info;
        size_t olen;

        for( curve_info = curve_list;
            curve_info->grp_id != MBEDTLS_ECP_DP_NONE;
            curve_info++ )
        {
            if( ! mbedtls_ecdh_can_do( curve_info->grp_id ) )
                continue;

            mbedtls_ecdh_init( &ecdh_srv );
            mbedtls_ecdh_init( &ecdh_cli );

            mbedtls_snprintf( title, sizeof( title ), "ECDHE-%s", curve_info->name );
            TIME_PUBLIC( title, "full handshake",
                const unsigned char * p_srv = buf_srv;

                CHECK_AND_CONTINUE( mbedtls_ecdh_setup( &ecdh_srv, curve_info->grp_id ) );
                CHECK_AND_CONTINUE( mbedtls_ecdh_make_params( &ecdh_srv, &olen, buf_srv, sizeof( buf_srv ), myrand, NULL ) );

                CHECK_AND_CONTINUE( mbedtls_ecdh_read_params( &ecdh_cli, &p_srv, p_srv + olen ) );
                CHECK_AND_CONTINUE( mbedtls_ecdh_make_public( &ecdh_cli, &olen, buf_cli, sizeof( buf_cli ), myrand, NULL ) );

                CHECK_AND_CONTINUE( mbedtls_ecdh_read_public( &ecdh_srv, buf_cli, olen ) );
                CHECK_AND_CONTINUE( mbedtls_ecdh_calc_secret( &ecdh_srv, &olen, buf_srv, sizeof( buf_srv ), myrand, NULL ) );

                CHECK_AND_CONTINUE( mbedtls_ecdh_calc_secret( &ecdh_cli, &olen, buf_cli, sizeof( buf_cli ), myrand, NULL ) );
                mbedtls_ecdh_free( &ecdh_cli );

                mbedtls_ecdh_free( &ecdh_srv );
            );

        }
    }
#endif

    mbedtls_printf( "\n" );

#if defined(MBEDTLS_MEMORY_BUFFER_ALLOC_C)
    mbedtls_memory_buffer_alloc_free();
#endif

    mbedtls_exit( 0 );
}

#endif /* MBEDTLS_HAVE_TIME */

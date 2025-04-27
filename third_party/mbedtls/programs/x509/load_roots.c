/*
 *  Root CA reading application
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 *
 *  This file is provided under the Apache License 2.0, or the
 *  GNU General Public License v2.0 or later.
 *
 *  **********
 *  Apache License 2.0:
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
 *
 *  **********
 *
 *  **********
 *  GNU General Public License v2.0 or later:
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 *  **********
 */

#include "mbedtls/build_info.h"

#if defined(MBEDTLS_PLATFORM_C)
#include "mbedtls/platform.h"
#else
#include <stdio.h>
#include <stdlib.h>
#define mbedtls_time            time
#define mbedtls_time_t          time_t
#define mbedtls_fprintf         fprintf
#define mbedtls_printf          printf
#define mbedtls_exit            exit
#define MBEDTLS_EXIT_SUCCESS    EXIT_SUCCESS
#define MBEDTLS_EXIT_FAILURE    EXIT_FAILURE
#endif /* MBEDTLS_PLATFORM_C */

#if !defined(MBEDTLS_X509_CRT_PARSE_C) || !defined(MBEDTLS_FS_IO) ||  \
    !defined(MBEDTLS_TIMING_C)
int main( void )
{
    mbedtls_printf("MBEDTLS_X509_CRT_PARSE_C and/or MBEDTLS_FS_IO and/or "
           "MBEDTLS_TIMING_C not defined.\n");
    mbedtls_exit( 0 );
}
#else

#include "mbedtls/error.h"
#include "mbedtls/timing.h"
#include "mbedtls/x509_crt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DFL_ITERATIONS          1
#define DFL_PRIME_CACHE         1

#define USAGE \
    "\n usage: load_roots param=<>... [--] FILE...\n"   \
    "\n acceptable parameters:\n"                       \
    "    iterations=%%d        Iteration count (not including cache priming); default: 1\n"  \
    "    prime=%%d             Prime the disk read cache? Default: 1 (yes)\n"  \
    "\n"


/*
 * global options
 */
struct options
{
    const char **filenames;     /* NULL-terminated list of file names */
    unsigned iterations;        /* Number of iterations to time */
    int prime_cache;            /* Prime the disk read cache? */
} opt;


int read_certificates( const char *const *filenames )
{
    mbedtls_x509_crt cas;
    int ret = 0;
    const char *const *cur;

    mbedtls_x509_crt_init( &cas );

    for( cur = filenames; *cur != NULL; cur++ )
    {
        ret = mbedtls_x509_crt_parse_file( &cas, *cur );
        if( ret != 0 )
        {
#if defined(MBEDTLS_ERROR_C) || defined(MBEDTLS_ERROR_STRERROR_DUMMY)
            char error_message[200];
            mbedtls_strerror( ret, error_message, sizeof( error_message ) );
            printf( "\n%s: -0x%04x (%s)\n",
                    *cur, (unsigned) -ret, error_message );
#else
            printf( "\n%s: -0x%04x\n",
                    *cur, (unsigned) -ret );
#endif
            goto exit;
        }
    }

exit:
    mbedtls_x509_crt_free( &cas );
    return( ret == 0 );
}

int main( int argc, char *argv[] )
{
    int exit_code = MBEDTLS_EXIT_FAILURE;
    unsigned i, j;
    struct mbedtls_timing_hr_time timer;
    unsigned long ms;

    if( argc <= 1 )
    {
        mbedtls_printf( USAGE );
        goto exit;
    }

    opt.filenames = NULL;
    opt.iterations = DFL_ITERATIONS;
    opt.prime_cache = DFL_PRIME_CACHE;

    for( i = 1; i < (unsigned) argc; i++ )
    {
        char *p = argv[i];
        char *q = NULL;

        if( strcmp( p, "--" ) == 0 )
            break;
        if( ( q = strchr( p, '=' ) ) == NULL )
            break;
        *q++ = '\0';

        for( j = 0; p + j < q; j++ )
        {
            if( argv[i][j] >= 'A' && argv[i][j] <= 'Z' )
                argv[i][j] |= 0x20;
        }

        if( strcmp( p, "iterations" ) == 0 )
        {
            opt.iterations = atoi( q );
        }
        else if( strcmp( p, "prime" ) == 0 )
        {
            opt.iterations = atoi( q ) != 0;
        }
        else
        {
            mbedtls_printf( "Unknown option: %s\n", p );
            mbedtls_printf( USAGE );
            goto exit;
        }
    }

    opt.filenames = (const char**) argv + i;
    if( *opt.filenames == 0 )
    {
        mbedtls_printf( "Missing list of certificate files to parse\n" );
        goto exit;
    }

    mbedtls_printf( "Parsing %u certificates", argc - i );
    if( opt.prime_cache )
    {
        if( ! read_certificates( opt.filenames ) )
            goto exit;
        mbedtls_printf( " " );
    }

    (void) mbedtls_timing_get_timer( &timer, 1 );
    for( i = 1; i <= opt.iterations; i++ )
    {
        if( ! read_certificates( opt.filenames ) )
            goto exit;
        mbedtls_printf( "." );
    }
    ms = mbedtls_timing_get_timer( &timer, 0 );
    mbedtls_printf( "\n%u iterations -> %lu ms\n", opt.iterations, ms );
    exit_code = MBEDTLS_EXIT_SUCCESS;

exit:
    mbedtls_exit( exit_code );
}
#endif /* necessary configuration */

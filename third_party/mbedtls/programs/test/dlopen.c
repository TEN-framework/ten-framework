/*
 *  Test dynamic loading of libmbed*
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

#include "mbedtls/build_info.h"

#if defined(MBEDTLS_PLATFORM_C)
#include "mbedtls/platform.h"
#else
#include <stdio.h>
#include <stdlib.h>
#define mbedtls_fprintf    fprintf
#define mbedtls_printf     printf
#define mbedtls_exit       exit
#define MBEDTLS_EXIT_SUCCESS EXIT_SUCCESS
#define MBEDTLS_EXIT_FAILURE EXIT_FAILURE
#endif

#if defined(MBEDTLS_X509_CRT_PARSE_C)
#include "mbedtls/x509_crt.h"
#endif

#if defined(__APPLE__)
#define SO_SUFFIX ".dylib"
#else
#define SO_SUFFIX ".so"
#endif

#define CRYPTO_SO_FILENAME "libmbedcrypto" SO_SUFFIX
#define X509_SO_FILENAME "libmbedx509" SO_SUFFIX
#define TLS_SO_FILENAME "libmbedtls" SO_SUFFIX

#include <dlfcn.h>

#define CHECK_DLERROR( function, argument )                             \
    do                                                                  \
    {                                                                   \
        char *CHECK_DLERROR_error = dlerror ( );                        \
        if( CHECK_DLERROR_error != NULL )                               \
        {                                                               \
            fprintf( stderr, "Dynamic loading error for %s(%s): %s\n",  \
                     function, argument, CHECK_DLERROR_error );         \
            mbedtls_exit( MBEDTLS_EXIT_FAILURE );                       \
        }                                                               \
    }                                                                   \
    while( 0 )

int main( void )
{
#if defined(MBEDTLS_MD_C) || defined(MBEDTLS_SSL_TLS_C)
    unsigned n;
#endif

#if defined(MBEDTLS_SSL_TLS_C)
    void *tls_so = dlopen( TLS_SO_FILENAME, RTLD_NOW );
    CHECK_DLERROR( "dlopen", TLS_SO_FILENAME );
    const int *( *ssl_list_ciphersuites )( void ) =
        dlsym( tls_so, "mbedtls_ssl_list_ciphersuites" );
    CHECK_DLERROR( "dlsym", "mbedtls_ssl_list_ciphersuites" );
    const int *ciphersuites = ssl_list_ciphersuites( );
    for( n = 0; ciphersuites[n] != 0; n++ )
        /* nothing to do, we're just counting */;
    mbedtls_printf( "dlopen(%s): %u ciphersuites\n",
                    TLS_SO_FILENAME, n );
    dlclose( tls_so );
    CHECK_DLERROR( "dlclose", TLS_SO_FILENAME );
#endif  /* MBEDTLS_SSL_TLS_C */

#if defined(MBEDTLS_X509_CRT_PARSE_C)
    void *x509_so = dlopen( X509_SO_FILENAME, RTLD_NOW );
    CHECK_DLERROR( "dlopen", X509_SO_FILENAME );
    const mbedtls_x509_crt_profile *profile =
        dlsym( x509_so, "mbedtls_x509_crt_profile_default" );
    CHECK_DLERROR( "dlsym", "mbedtls_x509_crt_profile_default" );
    mbedtls_printf( "dlopen(%s): Allowed md mask: %08x\n",
                    X509_SO_FILENAME, (unsigned) profile->allowed_mds );
    dlclose( x509_so );
    CHECK_DLERROR( "dlclose", X509_SO_FILENAME );
#endif  /* MBEDTLS_X509_CRT_PARSE_C */

#if defined(MBEDTLS_MD_C)
    void *crypto_so = dlopen( CRYPTO_SO_FILENAME, RTLD_NOW );
    CHECK_DLERROR( "dlopen", CRYPTO_SO_FILENAME );
    const int *( *md_list )( void ) =
        dlsym( crypto_so, "mbedtls_md_list" );
    CHECK_DLERROR( "dlsym", "mbedtls_md_list" );
    const int *mds = md_list( );
    for( n = 0; mds[n] != 0; n++ )
        /* nothing to do, we're just counting */;
    mbedtls_printf( "dlopen(%s): %u hashes\n",
                    CRYPTO_SO_FILENAME, n );
    dlclose( crypto_so );
    CHECK_DLERROR( "dlclose", CRYPTO_SO_FILENAME );
#endif  /* MBEDTLS_MD_C */

    return( 0 );
}


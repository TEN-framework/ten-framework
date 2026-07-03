/* mbedtls_config.h wrapper that forces calloc(0) to return NULL.
 * Used for testing.
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

#include "mbedtls/mbedtls_config.h"

#include <stdlib.h>

#ifndef MBEDTLS_PLATFORM_STD_CALLOC
static inline void *custom_calloc( size_t nmemb, size_t size )
{
    if( nmemb == 0 || size == 0 )
        return( NULL );
    return( calloc( nmemb, size ) );
}

#define MBEDTLS_PLATFORM_MEMORY
#define MBEDTLS_PLATFORM_STD_CALLOC custom_calloc
#endif

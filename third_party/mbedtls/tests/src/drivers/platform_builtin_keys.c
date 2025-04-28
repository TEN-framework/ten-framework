/** \file platform_builtin_keys.c
 *
 * \brief Test driver implementation of the builtin key support
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

#include <test/helpers.h>

#include <psa/crypto.h>
#include <psa/crypto_extra.h>

#if defined(PSA_CRYPTO_DRIVER_TEST)
#include <test/drivers/test_driver.h>
#endif

typedef struct
{
    psa_key_id_t builtin_key_id;
    psa_key_lifetime_t lifetime;
    psa_drv_slot_number_t slot_number;
} mbedtls_psa_builtin_key_description_t;

static const mbedtls_psa_builtin_key_description_t builtin_keys[] = {
#if defined(PSA_CRYPTO_DRIVER_TEST)
    /* For testing, assign the AES builtin key slot to the boundary values.
     * ECDSA can be exercised on key ID MBEDTLS_PSA_KEY_ID_BUILTIN_MIN + 1. */
    { MBEDTLS_PSA_KEY_ID_BUILTIN_MIN - 1,
      PSA_KEY_LIFETIME_FROM_PERSISTENCE_AND_LOCATION(
        PSA_KEY_PERSISTENCE_READ_ONLY, PSA_CRYPTO_TEST_DRIVER_LOCATION ),
      PSA_CRYPTO_TEST_DRIVER_BUILTIN_AES_KEY_SLOT },
    { MBEDTLS_PSA_KEY_ID_BUILTIN_MIN,
      PSA_KEY_LIFETIME_FROM_PERSISTENCE_AND_LOCATION(
        PSA_KEY_PERSISTENCE_READ_ONLY, PSA_CRYPTO_TEST_DRIVER_LOCATION ),
      PSA_CRYPTO_TEST_DRIVER_BUILTIN_AES_KEY_SLOT },
    { MBEDTLS_PSA_KEY_ID_BUILTIN_MIN + 1,
      PSA_KEY_LIFETIME_FROM_PERSISTENCE_AND_LOCATION(
        PSA_KEY_PERSISTENCE_READ_ONLY, PSA_CRYPTO_TEST_DRIVER_LOCATION ),
      PSA_CRYPTO_TEST_DRIVER_BUILTIN_ECDSA_KEY_SLOT},
    { MBEDTLS_PSA_KEY_ID_BUILTIN_MAX - 1,
      PSA_KEY_LIFETIME_FROM_PERSISTENCE_AND_LOCATION(
        PSA_KEY_PERSISTENCE_READ_ONLY, PSA_CRYPTO_TEST_DRIVER_LOCATION ),
      PSA_CRYPTO_TEST_DRIVER_BUILTIN_AES_KEY_SLOT},
    { MBEDTLS_PSA_KEY_ID_BUILTIN_MAX,
      PSA_KEY_LIFETIME_FROM_PERSISTENCE_AND_LOCATION(
        PSA_KEY_PERSISTENCE_READ_ONLY, PSA_CRYPTO_TEST_DRIVER_LOCATION ),
      PSA_CRYPTO_TEST_DRIVER_BUILTIN_AES_KEY_SLOT},
    { MBEDTLS_PSA_KEY_ID_BUILTIN_MAX + 1,
      PSA_KEY_LIFETIME_FROM_PERSISTENCE_AND_LOCATION(
        PSA_KEY_PERSISTENCE_READ_ONLY, PSA_CRYPTO_TEST_DRIVER_LOCATION ),
      PSA_CRYPTO_TEST_DRIVER_BUILTIN_AES_KEY_SLOT},
#else
    {0, 0, 0}
#endif
};

psa_status_t mbedtls_psa_platform_get_builtin_key(
    mbedtls_svc_key_id_t key_id,
    psa_key_lifetime_t *lifetime,
    psa_drv_slot_number_t *slot_number )
{
    psa_key_id_t app_key_id = MBEDTLS_SVC_KEY_ID_GET_KEY_ID( key_id );
    const mbedtls_psa_builtin_key_description_t *builtin_key;

    for( size_t i = 0;
         i < ( sizeof( builtin_keys ) / sizeof( builtin_keys[0] ) ); i++ )
    {
        builtin_key = &builtin_keys[i];
        if( builtin_key->builtin_key_id == app_key_id )
        {
            *lifetime = builtin_key->lifetime;
            *slot_number = builtin_key->slot_number;
            return( PSA_SUCCESS );
        }
    }

    return( PSA_ERROR_DOES_NOT_EXIST );
}

/*
 * Test driver for asymmetric encryption.
 */
/*  Copyright The Mbed TLS Contributors
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

#if defined(MBEDTLS_PSA_CRYPTO_DRIVERS) && defined(PSA_CRYPTO_DRIVER_TEST)
#include "psa/crypto.h"
#include "mbedtls/rsa.h"
#include "psa_crypto_rsa.h"
#include "string.h"
#include "test/drivers/asymmetric_encryption.h"

#if defined(MBEDTLS_TEST_LIBTESTDRIVER1)
#include "libtestdriver1/library/psa_crypto_rsa.h"
#endif

mbedtls_test_driver_asymmetric_encryption_hooks_t mbedtls_test_driver_asymmetric_encryption_hooks =
    MBEDTLS_TEST_DRIVER_ASYMMETRIC_ENCRYPTION_INIT;

psa_status_t mbedtls_test_transparent_asymmetric_encrypt(
    const psa_key_attributes_t *attributes, const uint8_t *key_buffer,
    size_t key_buffer_size, psa_algorithm_t alg, const uint8_t *input,
    size_t input_length, const uint8_t *salt, size_t salt_length,
    uint8_t *output, size_t output_size, size_t *output_length )
{
    mbedtls_test_driver_asymmetric_encryption_hooks.hits++;

    if( mbedtls_test_driver_asymmetric_encryption_hooks.forced_output != NULL )
    {
        if( output_size < mbedtls_test_driver_asymmetric_encryption_hooks.forced_output_length )
            return( PSA_ERROR_BUFFER_TOO_SMALL );

        memcpy( output,
                mbedtls_test_driver_asymmetric_encryption_hooks.forced_output,
                mbedtls_test_driver_asymmetric_encryption_hooks.forced_output_length );
        *output_length = mbedtls_test_driver_asymmetric_encryption_hooks.forced_output_length;

        return( mbedtls_test_driver_asymmetric_encryption_hooks.forced_status );
    }

    if( mbedtls_test_driver_asymmetric_encryption_hooks.forced_status != PSA_SUCCESS )
        return( mbedtls_test_driver_asymmetric_encryption_hooks.forced_status );

#if defined(MBEDTLS_TEST_LIBTESTDRIVER1) && \
    defined(LIBTESTDRIVER1_MBEDTLS_PSA_BUILTIN_CIPHER)
    return( libtestdriver1_mbedtls_psa_asymmetric_encrypt(
                (const libtestdriver1_psa_key_attributes_t *)attributes,
                key_buffer, key_buffer_size,
                alg, input, input_length, salt, salt_length,
                output, output_size, output_length ) );
#else
    return( mbedtls_psa_asymmetric_encrypt(
                attributes, key_buffer, key_buffer_size,
                alg, input, input_length, salt, salt_length,
                output, output_size, output_length ) );
#endif

    return( PSA_ERROR_NOT_SUPPORTED );
}

psa_status_t mbedtls_test_transparent_asymmetric_decrypt(
    const psa_key_attributes_t *attributes, const uint8_t *key_buffer,
    size_t key_buffer_size, psa_algorithm_t alg, const uint8_t *input,
    size_t input_length, const uint8_t *salt, size_t salt_length,
    uint8_t *output, size_t output_size, size_t *output_length )
{
    mbedtls_test_driver_asymmetric_encryption_hooks.hits++;

    if( mbedtls_test_driver_asymmetric_encryption_hooks.forced_output != NULL )
    {
        if( output_size < mbedtls_test_driver_asymmetric_encryption_hooks.forced_output_length )
            return( PSA_ERROR_BUFFER_TOO_SMALL );

        memcpy( output,
                mbedtls_test_driver_asymmetric_encryption_hooks.forced_output,
                mbedtls_test_driver_asymmetric_encryption_hooks.forced_output_length );
        *output_length = mbedtls_test_driver_asymmetric_encryption_hooks.forced_output_length;

        return( mbedtls_test_driver_asymmetric_encryption_hooks.forced_status );
    }

    if( mbedtls_test_driver_asymmetric_encryption_hooks.forced_status != PSA_SUCCESS )
        return( mbedtls_test_driver_asymmetric_encryption_hooks.forced_status );

#if defined(MBEDTLS_TEST_LIBTESTDRIVER1) && \
    defined(LIBTESTDRIVER1_MBEDTLS_PSA_BUILTIN_CIPHER)
    return( libtestdriver1_mbedtls_psa_asymmetric_decrypt(
                (const libtestdriver1_psa_key_attributes_t *)attributes,
                key_buffer, key_buffer_size,
                alg, input, input_length, salt, salt_length,
                output, output_size, output_length ) );
#else
    return( mbedtls_psa_asymmetric_decrypt(
                attributes, key_buffer, key_buffer_size,
                alg, input, input_length, salt, salt_length,
                output, output_size, output_length ) );
#endif

    return( PSA_ERROR_NOT_SUPPORTED );
}

/*
 * opaque versions - TODO
 */
psa_status_t mbedtls_test_opaque_asymmetric_encrypt(
    const psa_key_attributes_t *attributes, const uint8_t *key,
    size_t key_length, psa_algorithm_t alg, const uint8_t *input,
    size_t input_length, const uint8_t *salt, size_t salt_length,
    uint8_t *output, size_t output_size, size_t *output_length )
{
    (void) attributes;
    (void) key;
    (void) key_length;
    (void) alg;
    (void) input;
    (void) input_length;
    (void) salt;
    (void) salt_length;
    (void) output;
    (void) output_size;
    (void) output_length;
    return( PSA_ERROR_NOT_SUPPORTED );
}

psa_status_t mbedtls_test_opaque_asymmetric_decrypt(
    const psa_key_attributes_t *attributes, const uint8_t *key,
    size_t key_length, psa_algorithm_t alg, const uint8_t *input,
    size_t input_length, const uint8_t *salt, size_t salt_length,
    uint8_t *output, size_t output_size, size_t *output_length )
{
    (void) attributes;
    (void) key;
    (void) key_length;
    (void) alg;
    (void) input;
    (void) input_length;
    (void) salt;
    (void) salt_length;
    (void) output;
    (void) output_size;
    (void) output_length;
    return( PSA_ERROR_NOT_SUPPORTED );
}

#endif /* MBEDTLS_PSA_CRYPTO_DRIVERS && PSA_CRYPTO_DRIVER_TEST */

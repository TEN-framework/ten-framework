/*
 * Insecure but standalone implementation of mbedtls_psa_external_get_random().
 * Only for use in tests!
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

#ifndef FAKE_EXTERNAL_RNG_FOR_TEST_H
#define FAKE_EXTERNAL_RNG_FOR_TEST_H

#include "mbedtls/build_info.h"

#if defined(MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG)
/** Enable the insecure implementation of mbedtls_psa_external_get_random().
 *
 * The insecure implementation of mbedtls_psa_external_get_random() is
 * disabled by default.
 *
 * When MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG is enabled and the test
 * helpers are linked into a program, you must enable this before running any
 * code that uses the PSA subsystem to generate random data (including internal
 * random generation for purposes such as blinding when the random generation
 * is routed through PSA).
 *
 * You can enable and disable it at any time, regardless of the state
 * of the PSA subsystem. You may disable it temporarily to simulate a
 * depleted entropy source.
 */
void mbedtls_test_enable_insecure_external_rng( void );

/** Disable the insecure implementation of mbedtls_psa_external_get_random().
 *
 * See mbedtls_test_enable_insecure_external_rng().
 */
void mbedtls_test_disable_insecure_external_rng( void );
#endif /* MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG */

#endif /* FAKE_EXTERNAL_RNG_FOR_TEST_H */

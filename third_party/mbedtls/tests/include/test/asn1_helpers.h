/** Helper functions for tests that manipulate ASN.1 data.
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

#ifndef ASN1_HELPERS_H
#define ASN1_HELPERS_H

#include "test/helpers.h"

/** Skip past an INTEGER in an ASN.1 buffer.
 *
 * Mark the current test case as failed in any of the following conditions:
 * - The buffer does not start with an ASN.1 INTEGER.
 * - The integer's size or parity does not match the constraints expressed
 *   through \p min_bits, \p max_bits and \p must_be_odd.
 *
 * \param p             Upon entry, `*p` points to the first byte of the
 *                      buffer to parse.
 *                      On successful return, `*p` points to the first byte
 *                      after the parsed INTEGER.
 *                      On failure, `*p` is unspecified.
 * \param end           The end of the ASN.1 buffer.
 * \param min_bits      Fail the test case if the integer does not have at
 *                      least this many significant bits.
 * \param max_bits      Fail the test case if the integer has more than
 *                      this many significant bits.
 * \param must_be_odd   Fail the test case if the integer is even.
 *
 * \return              \c 0 if the test failed, otherwise 1.
 */
int mbedtls_test_asn1_skip_integer( unsigned char **p, const unsigned char *end,
                                    size_t min_bits, size_t max_bits,
                                    int must_be_odd );

#endif /* ASN1_HELPERS_H */

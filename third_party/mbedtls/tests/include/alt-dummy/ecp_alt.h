/* ecp_alt.h with dummy types for MBEDTLS_ECP_ALT */
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

#ifndef ECP_ALT_H
#define ECP_ALT_H

typedef struct mbedtls_ecp_group
{
    const mbedtls_ecp_group_id id;
    const mbedtls_mpi P;
    const mbedtls_mpi A;
    const mbedtls_mpi B;
    const mbedtls_ecp_point G;
    const mbedtls_mpi N;
    const size_t pbits;
    const size_t nbits;
}
mbedtls_ecp_group;

#endif /* ecp_alt.h */

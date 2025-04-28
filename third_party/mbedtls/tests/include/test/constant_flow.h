/**
 * \file constant_flow.h
 *
 * \brief   This file contains tools to ensure tested code has constant flow.
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

#ifndef TEST_CONSTANT_FLOW_H
#define TEST_CONSTANT_FLOW_H

#include "mbedtls/build_info.h"

/*
 * This file defines the two macros
 *
 *  #define TEST_CF_SECRET(ptr, size)
 *  #define TEST_CF_PUBLIC(ptr, size)
 *
 * that can be used in tests to mark a memory area as secret (no branch or
 * memory access should depend on it) or public (default, only needs to be
 * marked explicitly when it was derived from secret data).
 *
 * Arguments:
 * - ptr: a pointer to the memory area to be marked
 * - size: the size in bytes of the memory area
 *
 * Implementation:
 * The basic idea is that of ctgrind <https://github.com/agl/ctgrind>: we can
 * re-use tools that were designed for checking use of uninitialized memory.
 * This file contains two implementations: one based on MemorySanitizer, the
 * other on valgrind's memcheck. If none of them is enabled, dummy macros that
 * do nothing are defined for convenience.
 */

#if defined(MBEDTLS_TEST_CONSTANT_FLOW_MEMSAN)
#include <sanitizer/msan_interface.h>

/* Use macros to avoid messing up with origin tracking */
#define TEST_CF_SECRET  __msan_allocated_memory
// void __msan_allocated_memory(const volatile void* data, size_t size);
#define TEST_CF_PUBLIC  __msan_unpoison
// void __msan_unpoison(const volatile void *a, size_t size);

#elif defined(MBEDTLS_TEST_CONSTANT_FLOW_VALGRIND)
#include <valgrind/memcheck.h>

#define TEST_CF_SECRET  VALGRIND_MAKE_MEM_UNDEFINED
// VALGRIND_MAKE_MEM_UNDEFINED(_qzz_addr, _qzz_len)
#define TEST_CF_PUBLIC  VALGRIND_MAKE_MEM_DEFINED
// VALGRIND_MAKE_MEM_DEFINED(_qzz_addr, _qzz_len)

#else /* MBEDTLS_TEST_CONSTANT_FLOW_MEMSAN ||
         MBEDTLS_TEST_CONSTANT_FLOW_VALGRIND */

#define TEST_CF_SECRET(ptr, size)
#define TEST_CF_PUBLIC(ptr, size)

#endif /* MBEDTLS_TEST_CONSTANT_FLOW_MEMSAN ||
          MBEDTLS_TEST_CONSTANT_FLOW_VALGRIND */

#endif /* TEST_CONSTANT_FLOW_H */

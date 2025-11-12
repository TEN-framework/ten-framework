/**
 * \file helpers.h
 *
 * \brief   This file contains the prototypes of helper functions for the
 *          purpose of testing.
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

#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

/* Most fields of publicly available structs are private and are wrapped with
 * MBEDTLS_PRIVATE macro. This define allows tests to access the private fields
 * directly (without using the MBEDTLS_PRIVATE wrapper). */
#define MBEDTLS_ALLOW_PRIVATE_ACCESS

#include "mbedtls/build_info.h"

#if defined(MBEDTLS_THREADING_C) && defined(MBEDTLS_THREADING_PTHREAD) && \
    defined(MBEDTLS_TEST_HOOKS)
#define MBEDTLS_TEST_MUTEX_USAGE
#endif

#if defined(MBEDTLS_PLATFORM_C)
#include "mbedtls/platform.h"
#else
#include <stdio.h>
#define mbedtls_fprintf    fprintf
#define mbedtls_snprintf   snprintf
#define mbedtls_calloc     calloc
#define mbedtls_free       free
#define mbedtls_exit       exit
#define mbedtls_time       time
#define mbedtls_time_t     time_t
#define MBEDTLS_EXIT_SUCCESS EXIT_SUCCESS
#define MBEDTLS_EXIT_FAILURE EXIT_FAILURE
#endif

#include <stddef.h>
#include <stdint.h>

#if defined(MBEDTLS_BIGNUM_C)
#include "mbedtls/bignum.h"
#endif

typedef enum
{
    MBEDTLS_TEST_RESULT_SUCCESS = 0,
    MBEDTLS_TEST_RESULT_FAILED,
    MBEDTLS_TEST_RESULT_SKIPPED
} mbedtls_test_result_t;

typedef struct
{
    mbedtls_test_result_t result;
    const char *test;
    const char *filename;
    int line_no;
    unsigned long step;
    char line1[76];
    char line2[76];
#if defined(MBEDTLS_TEST_MUTEX_USAGE)
    const char *mutex_usage_error;
#endif
}
mbedtls_test_info_t;
extern mbedtls_test_info_t mbedtls_test_info;

int mbedtls_test_platform_setup( void );
void mbedtls_test_platform_teardown( void );

/**
 * \brief           Record the current test case as a failure.
 *
 *                  This function can be called directly however it is usually
 *                  called via macros such as TEST_ASSERT, TEST_EQUAL,
 *                  PSA_ASSERT, etc...
 *
 * \note            If the test case was already marked as failed, calling
 *                  `mbedtls_test_fail( )` again will not overwrite any
 *                  previous information about the failure.
 *
 * \param test      Description of the failure or assertion that failed. This
 *                  MUST be a string literal.
 * \param line_no   Line number where the failure originated.
 * \param filename  Filename where the failure originated.
 */
void mbedtls_test_fail( const char *test, int line_no, const char* filename );

/**
 * \brief           Record the current test case as skipped.
 *
 *                  This function can be called directly however it is usually
 *                  called via the TEST_ASSUME macro.
 *
 * \param test      Description of the assumption that caused the test case to
 *                  be skipped. This MUST be a string literal.
 * \param line_no   Line number where the test case was skipped.
 * \param filename  Filename where the test case was skipped.
 */
void mbedtls_test_skip( const char *test, int line_no, const char* filename );

/**
 * \brief       Set the test step number for failure reports.
 *
 *              Call this function to display "step NNN" in addition to the
 *              line number and file name if a test fails. Typically the "step
 *              number" is the index of a for loop but it can be whatever you
 *              want.
 *
 * \param step  The step number to report.
 */
void mbedtls_test_set_step( unsigned long step );

/**
 * \brief       Reset mbedtls_test_info to a ready/starting state.
 */
void mbedtls_test_info_reset( void );

/**
 * \brief           Record the current test case as a failure if two integers
 *                  have a different value.
 *
 *                  This function is usually called via the macro
 *                  #TEST_EQUAL.
 *
 * \param test      Description of the failure or assertion that failed. This
 *                  MUST be a string literal. This normally has the form
 *                  "EXPR1 == EXPR2" where EXPR1 has the value \p value1
 *                  and EXPR2 has the value \p value2.
 * \param line_no   Line number where the failure originated.
 * \param filename  Filename where the failure originated.
 * \param value1    The first value to compare.
 * \param value2    The second value to compare.
 *
 * \return          \c 1 if the values are equal, otherwise \c 0.
 */
int mbedtls_test_equal( const char *test, int line_no, const char* filename,
                        unsigned long long value1, unsigned long long value2 );

/**
 * \brief           Record the current test case as a failure based
 *                  on comparing two unsigned integers.
 *
 *                  This function is usually called via the macro
 *                  #TEST_LE_U.
 *
 * \param test      Description of the failure or assertion that failed. This
 *                  MUST be a string literal. This normally has the form
 *                  "EXPR1 <= EXPR2" where EXPR1 has the value \p value1
 *                  and EXPR2 has the value \p value2.
 * \param line_no   Line number where the failure originated.
 * \param filename  Filename where the failure originated.
 * \param value1    The first value to compare.
 * \param value2    The second value to compare.
 *
 * \return          \c 1 if \p value1 <= \p value2, otherwise \c 0.
 */
int mbedtls_test_le_u( const char *test, int line_no, const char* filename,
                       unsigned long long value1, unsigned long long value2 );

/**
 * \brief           Record the current test case as a failure based
 *                  on comparing two signed integers.
 *
 *                  This function is usually called via the macro
 *                  #TEST_LE_S.
 *
 * \param test      Description of the failure or assertion that failed. This
 *                  MUST be a string literal. This normally has the form
 *                  "EXPR1 <= EXPR2" where EXPR1 has the value \p value1
 *                  and EXPR2 has the value \p value2.
 * \param line_no   Line number where the failure originated.
 * \param filename  Filename where the failure originated.
 * \param value1    The first value to compare.
 * \param value2    The second value to compare.
 *
 * \return          \c 1 if \p value1 <= \p value2, otherwise \c 0.
 */
int mbedtls_test_le_s( const char *test, int line_no, const char* filename,
                       long long value1, long long value2 );

/**
 * \brief          This function decodes the hexadecimal representation of
 *                 data.
 *
 * \note           The output buffer can be the same as the input buffer. For
 *                 any other overlapping of the input and output buffers, the
 *                 behavior is undefined.
 *
 * \param obuf     Output buffer.
 * \param obufmax  Size in number of bytes of \p obuf.
 * \param ibuf     Input buffer.
 * \param len      The number of unsigned char written in \p obuf. This must
 *                 not be \c NULL.
 *
 * \return         \c 0 on success.
 * \return         \c -1 if the output buffer is too small or the input string
 *                 is not a valid hexadecimal representation.
 */
int mbedtls_test_unhexify( unsigned char *obuf, size_t obufmax,
                           const char *ibuf, size_t *len );

void mbedtls_test_hexify( unsigned char *obuf,
                          const unsigned char *ibuf,
                          int len );

/**
 * Allocate and zeroize a buffer.
 *
 * If the size if zero, a pointer to a zeroized 1-byte buffer is returned.
 *
 * For convenience, dies if allocation fails.
 */
unsigned char *mbedtls_test_zero_alloc( size_t len );

/**
 * Allocate and fill a buffer from hex data.
 *
 * The buffer is sized exactly as needed. This allows to detect buffer
 * overruns (including overreads) when running the test suite under valgrind.
 *
 * If the size if zero, a pointer to a zeroized 1-byte buffer is returned.
 *
 * For convenience, dies if allocation fails.
 */
unsigned char *mbedtls_test_unhexify_alloc( const char *ibuf, size_t *olen );

int mbedtls_test_hexcmp( uint8_t * a, uint8_t * b,
                         uint32_t a_len, uint32_t b_len );

#if defined(MBEDTLS_PSA_CRYPTO_C) && defined(MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG)
#include "test/fake_external_rng_for_test.h"
#endif

#if defined(MBEDTLS_TEST_MUTEX_USAGE)
/** Permanently activate the mutex usage verification framework. See
 * threading_helpers.c for information. */
void mbedtls_test_mutex_usage_init( void );

/** Call this function after executing a test case to check for mutex usage
 * errors. */
void mbedtls_test_mutex_usage_check( void );
#endif /* MBEDTLS_TEST_MUTEX_USAGE */

#if defined(MBEDTLS_TEST_HOOKS)
/**
 * \brief   Check that only a pure high-level error code is being combined with
 *          a pure low-level error code as otherwise the resultant error code
 *          would be corrupted.
 *
 * \note    Both high-level and low-level error codes cannot be greater than
 *          zero however can be zero. If one error code is zero then the
 *          other error code is returned even if both codes are zero.
 *
 * \note    If the check fails, fail the test currently being run.
 */
void mbedtls_test_err_add_check( int high, int low,
                                 const char *file, int line);
#endif

#if defined(MBEDTLS_BIGNUM_C)
/** Read an MPI from a string.
 *
 * Like mbedtls_mpi_read_string(), but size the resulting bignum based
 * on the number of digits in the string. In particular, construct a
 * bignum with 0 limbs for an empty string, and a bignum with leading 0
 * limbs if the string has sufficiently many leading 0 digits.
 *
 * This is important so that the "0 (null)" and "0 (1 limb)" and
 * "leading zeros" test cases do what they claim.
 *
 * \param[out] X        The MPI object to populate. It must be initialized.
 * \param radix         The radix (2 to 16).
 * \param[in] s         The null-terminated string to read from.
 *
 * \return \c 0 on success, an \c MBEDTLS_ERR_MPI_xxx error code otherwise.
 */
/* Since the library has exactly the desired behavior, this is trivial. */
int mbedtls_test_read_mpi( mbedtls_mpi *X, int radix, const char *s );
#endif /* MBEDTLS_BIGNUM_C */

#endif /* TEST_HELPERS_H */

/**
 * \file macros.h
 *
 * \brief   This file contains generic macros for the purpose of testing.
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

#ifndef TEST_MACROS_H
#define TEST_MACROS_H

#include "mbedtls/build_info.h"

#include <stdlib.h>

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

#if defined(MBEDTLS_MEMORY_BUFFER_ALLOC_C)
#include "mbedtls/memory_buffer_alloc.h"
#endif

/**
 * \brief   This macro tests the expression passed to it as a test step or
 *          individual test in a test case.
 *
 *          It allows a library function to return a value and return an error
 *          code that can be tested.
 *
 *          Failing the test means:
 *          - Mark this test case as failed.
 *          - Print a message identifying the failure.
 *          - Jump to the \c exit label.
 *
 *          This macro expands to an instruction, not an expression.
 *          It may jump to the \c exit label.
 *
 * \param   TEST    The test expression to be tested.
 */
#define TEST_ASSERT( TEST )                                 \
    do {                                                    \
       if( ! (TEST) )                                       \
       {                                                    \
          mbedtls_test_fail( #TEST, __LINE__, __FILE__ );   \
          goto exit;                                        \
       }                                                    \
    } while( 0 )

/** Evaluate two integer expressions and fail the test case if they have
 * different values.
 *
 * The two expressions should have the same signedness, otherwise the
 * comparison is not meaningful if the signed value is negative.
 *
 * \param expr1     An integral-typed expression to evaluate.
 * \param expr2     Another integral-typed expression to evaluate.
 */
#define TEST_EQUAL( expr1, expr2 )                                      \
    do {                                                                \
        if( ! mbedtls_test_equal( #expr1 " == " #expr2, __LINE__, __FILE__, \
                                  expr1, expr2 ) )                      \
            goto exit;                                                  \
    } while( 0 )

/** Evaluate two unsigned integer expressions and fail the test case
 * if they are not in increasing order (left <= right).
 *
 * \param expr1     An integral-typed expression to evaluate.
 * \param expr2     Another integral-typed expression to evaluate.
 */
#define TEST_LE_U( expr1, expr2 )                                       \
    do {                                                                \
        if( ! mbedtls_test_le_u( #expr1 " <= " #expr2, __LINE__, __FILE__, \
                                 expr1, expr2 ) )                      \
            goto exit;                                                  \
    } while( 0 )

/** Evaluate two signed integer expressions and fail the test case
 * if they are not in increasing order (left <= right).
 *
 * \param expr1     An integral-typed expression to evaluate.
 * \param expr2     Another integral-typed expression to evaluate.
 */
#define TEST_LE_S( expr1, expr2 )                                       \
    do {                                                                \
        if( ! mbedtls_test_le_s( #expr1 " <= " #expr2, __LINE__, __FILE__, \
                                 expr1, expr2 ) )                      \
            goto exit;                                                  \
    } while( 0 )

/** Allocate memory dynamically and fail the test case if this fails.
 * The allocated memory will be filled with zeros.
 *
 * You must set \p pointer to \c NULL before calling this macro and
 * put `mbedtls_free( pointer )` in the test's cleanup code.
 *
 * If \p length is zero, the resulting \p pointer will be \c NULL.
 * This is usually what we want in tests since API functions are
 * supposed to accept null pointers when a buffer size is zero.
 *
 * This macro expands to an instruction, not an expression.
 * It may jump to the \c exit label.
 *
 * \param pointer   An lvalue where the address of the allocated buffer
 *                  will be stored.
 *                  This expression may be evaluated multiple times.
 * \param length    Number of elements to allocate.
 *                  This expression may be evaluated multiple times.
 *
 */
#define ASSERT_ALLOC( pointer, length )                           \
    do                                                            \
    {                                                             \
        TEST_ASSERT( ( pointer ) == NULL );                       \
        if( ( length ) != 0 )                                     \
        {                                                         \
            ( pointer ) = mbedtls_calloc( sizeof( *( pointer ) ), \
                                          ( length ) );           \
            TEST_ASSERT( ( pointer ) != NULL );                   \
        }                                                         \
    }                                                             \
    while( 0 )

/** Allocate memory dynamically. If the allocation fails, skip the test case.
 *
 * This macro behaves like #ASSERT_ALLOC, except that if the allocation
 * fails, it marks the test as skipped rather than failed.
 */
#define ASSERT_ALLOC_WEAK( pointer, length )                      \
    do                                                            \
    {                                                             \
        TEST_ASSERT( ( pointer ) == NULL );                       \
        if( ( length ) != 0 )                                     \
        {                                                         \
            ( pointer ) = mbedtls_calloc( sizeof( *( pointer ) ), \
                                          ( length ) );           \
            TEST_ASSUME( ( pointer ) != NULL );                   \
        }                                                         \
    }                                                             \
    while( 0 )

/** Compare two buffers and fail the test case if they differ.
 *
 * This macro expands to an instruction, not an expression.
 * It may jump to the \c exit label.
 *
 * \param p1        Pointer to the start of the first buffer.
 * \param size1     Size of the first buffer in bytes.
 *                  This expression may be evaluated multiple times.
 * \param p2        Pointer to the start of the second buffer.
 * \param size2     Size of the second buffer in bytes.
 *                  This expression may be evaluated multiple times.
 */
#define ASSERT_COMPARE( p1, size1, p2, size2 )                          \
    do                                                                  \
    {                                                                   \
        TEST_ASSERT( ( size1 ) == ( size2 ) );                          \
        if( ( size1 ) != 0 )                                            \
            TEST_ASSERT( memcmp( ( p1 ), ( p2 ), ( size1 ) ) == 0 );    \
    }                                                                   \
    while( 0 )

/**
 * \brief   This macro tests the expression passed to it and skips the
 *          running test if it doesn't evaluate to 'true'.
 *
 * \param   TEST    The test expression to be tested.
 */
#define TEST_ASSUME( TEST )                                 \
    do {                                                    \
        if( ! (TEST) )                                      \
        {                                                   \
            mbedtls_test_skip( #TEST, __LINE__, __FILE__ ); \
            goto exit;                                      \
        }                                                   \
    } while( 0 )

#define TEST_HELPER_ASSERT(a) if( !( a ) )                          \
{                                                                   \
    mbedtls_fprintf( stderr, "Assertion Failed at %s:%d - %s\n",    \
                             __FILE__, __LINE__, #a );              \
    mbedtls_exit( 1 );                                              \
}

/** \def ARRAY_LENGTH
 * Return the number of elements of a static or stack array.
 *
 * \param array         A value of array (not pointer) type.
 *
 * \return The number of elements of the array.
 */
/* A correct implementation of ARRAY_LENGTH, but which silently gives
 * a nonsensical result if called with a pointer rather than an array. */
#define ARRAY_LENGTH_UNSAFE( array )            \
    ( sizeof( array ) / sizeof( *( array ) ) )

#if defined(__GNUC__)
/* Test if arg and &(arg)[0] have the same type. This is true if arg is
 * an array but not if it's a pointer. */
#define IS_ARRAY_NOT_POINTER( arg )                                     \
    ( ! __builtin_types_compatible_p( __typeof__( arg ),                \
                                      __typeof__( &( arg )[0] ) ) )
/* A compile-time constant with the value 0. If `const_expr` is not a
 * compile-time constant with a nonzero value, cause a compile-time error. */
#define STATIC_ASSERT_EXPR( const_expr )                                \
    ( 0 && sizeof( struct { unsigned int STATIC_ASSERT : 1 - 2 * ! ( const_expr ); } ) )

/* Return the scalar value `value` (possibly promoted). This is a compile-time
 * constant if `value` is. `condition` must be a compile-time constant.
 * If `condition` is false, arrange to cause a compile-time error. */
#define STATIC_ASSERT_THEN_RETURN( condition, value )   \
    ( STATIC_ASSERT_EXPR( condition ) ? 0 : ( value ) )

#define ARRAY_LENGTH( array )                                           \
    ( STATIC_ASSERT_THEN_RETURN( IS_ARRAY_NOT_POINTER( array ),         \
                                 ARRAY_LENGTH_UNSAFE( array ) ) )

#else
/* If we aren't sure the compiler supports our non-standard tricks,
 * fall back to the unsafe implementation. */
#define ARRAY_LENGTH( array ) ARRAY_LENGTH_UNSAFE( array )
#endif

/** Return the smaller of two values.
 *
 * \param x         An integer-valued expression without side effects.
 * \param y         An integer-valued expression without side effects.
 *
 * \return The smaller of \p x and \p y.
 */
#define MIN( x, y ) ( ( x ) < ( y ) ? ( x ) : ( y ) )

/** Return the larger of two values.
 *
 * \param x         An integer-valued expression without side effects.
 * \param y         An integer-valued expression without side effects.
 *
 * \return The larger of \p x and \p y.
 */
#define MAX( x, y ) ( ( x ) > ( y ) ? ( x ) : ( y ) )

/*
 * 32-bit integer manipulation macros (big endian)
 */
#ifndef GET_UINT32_BE
#define GET_UINT32_BE(n,b,i)                            \
{                                                       \
    (n) = ( (uint32_t) (b)[(i)    ] << 24 )             \
        | ( (uint32_t) (b)[(i) + 1] << 16 )             \
        | ( (uint32_t) (b)[(i) + 2] <<  8 )             \
        | ( (uint32_t) (b)[(i) + 3]       );            \
}
#endif

#ifndef PUT_UINT32_BE
#define PUT_UINT32_BE(n,b,i)                            \
{                                                       \
    (b)[(i)    ] = (unsigned char) ( (n) >> 24 );       \
    (b)[(i) + 1] = (unsigned char) ( (n) >> 16 );       \
    (b)[(i) + 2] = (unsigned char) ( (n) >>  8 );       \
    (b)[(i) + 3] = (unsigned char) ( (n)       );       \
}
#endif

#endif /* TEST_MACROS_H */

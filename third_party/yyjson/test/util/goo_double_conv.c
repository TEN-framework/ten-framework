// Source code from: https://github.com/google/double-conversion (2022-01-31)
// The code was rewritten from C++ into a single C file for easier integration.



// Copyright 2006-2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.



#include "goo_double_conv.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <assert.h>



/// ============================================================================
/// Compile Hint Begin
/// ============================================================================

/* warning suppress begin */
#if defined(__clang__)
#   pragma clang diagnostic push
#   pragma clang diagnostic ignored "-Wunused-function"
#   pragma clang diagnostic ignored "-Wunused-parameter"
#   pragma clang diagnostic ignored "-Wunused-label"
#   pragma clang diagnostic ignored "-Wunused-macros"
#   pragma clang diagnostic ignored "-Wunused-variable"
#elif defined(__GNUC__)
#   if (__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#   pragma GCC diagnostic push
#   endif
#   pragma GCC diagnostic ignored "-Wunused-function"
#   pragma GCC diagnostic ignored "-Wunused-parameter"
#   pragma GCC diagnostic ignored "-Wunused-label"
#   pragma GCC diagnostic ignored "-Wunused-macros"
#   pragma GCC diagnostic ignored "-Wunused-variable"
#elif defined(_MSC_VER)
#   pragma warning(push)
#   pragma warning(disable:4100) /* unreferenced formal parameter */
#   pragma warning(disable:4102) /* unreferenced label */
#   pragma warning(disable:4127) /* conditional expression is constant */
#   pragma warning(disable:4706) /* assignment within conditional expression */
#endif



/// ============================================================================
/// ceil.c
/// ============================================================================

static void fp_force_eval(double x) {
    volatile double y;
    y = x;
}

#if FLT_EVAL_METHOD==0 || FLT_EVAL_METHOD==1
#define EPS DBL_EPSILON
#elif FLT_EVAL_METHOD==2
#define EPS LDBL_EPSILON
#endif
static const double toint = 1/EPS;

static double fp_ceil(double x) {
    union {double f; uint64_t i;} u;
    memcpy((void *)&u, (void *)&x, sizeof(uint64_t));
    
    int e = u.i >> 52 & 0x7ff;
    double y;
    
    if (e >= 0x3ff+52 || x == 0)
        return x;
    /* y = int(x) - x, where int(x) is an integer neighbor of x */
    if (u.i >> 63)
        y = x - toint + toint - x;
    else
        y = x + toint - toint - x;
    /* special case because of non-nearest rounding modes */
    if (e <= 0x3ff-1) {
        fp_force_eval(y);
        return u.i >> 63 ? -0.0 : 1;
    }
    if (y < 0)
        return x + y + 1;
    return x + y;
}



/// ============================================================================
/// utils.h
/// ============================================================================

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

// Use DOUBLE_CONVERSION_NON_PREFIXED_MACROS to get unprefixed macros as was
// the case in double-conversion releases prior to 3.1.6

#ifndef DOUBLE_CONVERSION_ASSERT
#define DOUBLE_CONVERSION_ASSERT(condition) assert(condition)
#endif
#if defined(DOUBLE_CONVERSION_NON_PREFIXED_MACROS) && !defined(ASSERT)
#define ASSERT DOUBLE_CONVERSION_ASSERT
#endif

#ifndef DOUBLE_CONVERSION_UNIMPLEMENTED
#define DOUBLE_CONVERSION_UNIMPLEMENTED() (abort())
#endif
#if defined(DOUBLE_CONVERSION_NON_PREFIXED_MACROS) && !defined(UNIMPLEMENTED)
#define UNIMPLEMENTED DOUBLE_CONVERSION_UNIMPLEMENTED
#endif

#ifndef DOUBLE_CONVERSION_NO_RETURN
#ifdef _MSC_VER
#define DOUBLE_CONVERSION_NO_RETURN __declspec(noreturn)
#else
#define DOUBLE_CONVERSION_NO_RETURN __attribute__((noreturn))
#endif
#endif
#if defined(DOUBLE_CONVERSION_NON_PREFIXED_MACROS) && !defined(NO_RETURN)
#define NO_RETURN DOUBLE_CONVERSION_NO_RETURN
#endif

#ifndef DOUBLE_CONVERSION_UNREACHABLE
#ifdef _MSC_VER
void DOUBLE_CONVERSION_NO_RETURN abort_noreturn();
static void abort_noreturn() { abort(); }
#define DOUBLE_CONVERSION_UNREACHABLE()   (abort_noreturn())
#else
#define DOUBLE_CONVERSION_UNREACHABLE()   (abort())
#endif
#endif
#if defined(DOUBLE_CONVERSION_NON_PREFIXED_MACROS) && !defined(UNREACHABLE)
#define UNREACHABLE DOUBLE_CONVERSION_UNREACHABLE
#endif

// Not all compilers support __has_attribute and combining a check for both
// ifdef and __has_attribute on the same preprocessor line isn't portable.
#ifdef __has_attribute
#   define DOUBLE_CONVERSION_HAS_ATTRIBUTE(x) __has_attribute(x)
#else
#   define DOUBLE_CONVERSION_HAS_ATTRIBUTE(x) 0
#endif

#ifndef DOUBLE_CONVERSION_UNUSED
#if DOUBLE_CONVERSION_HAS_ATTRIBUTE(unused)
#define DOUBLE_CONVERSION_UNUSED __attribute__((unused))
#else
#define DOUBLE_CONVERSION_UNUSED
#endif
#endif
#if defined(DOUBLE_CONVERSION_NON_PREFIXED_MACROS) && !defined(UNUSED)
#define UNUSED DOUBLE_CONVERSION_UNUSED
#endif

#if DOUBLE_CONVERSION_HAS_ATTRIBUTE(uninitialized)
#define DOUBLE_CONVERSION_STACK_UNINITIALIZED __attribute__((uninitialized))
#else
#define DOUBLE_CONVERSION_STACK_UNINITIALIZED
#endif
#if defined(DOUBLE_CONVERSION_NON_PREFIXED_MACROS) && !defined(STACK_UNINITIALIZED)
#define STACK_UNINITIALIZED DOUBLE_CONVERSION_STACK_UNINITIALIZED
#endif

// Double operations detection based on target architecture.
// Linux uses a 80bit wide floating point stack on x86. This induces double
// rounding, which in turn leads to wrong results.
// An easy way to test if the floating-point operations are correct is to
// evaluate: 89255.0/1e22. If the floating-point stack is 64 bits wide then
// the result is equal to 89255e-22.
// The best way to test this, is to create a division-function and to compare
// the output of the division with the expected result. (Inlining must be
// disabled.)
// On Linux,x86 89255e-22 != Div_double(89255.0/1e22)
//
// For example:
/*
// -- in div.c
double Div_double(double x, double y) { return x / y; }

// -- in main.c
double Div_double(double x, double y);  // Forward declaration.

int main(int argc, char** argv) {
    return Div_double(89255.0, 1e22) == 89255e-22;
}
*/
// Run as follows ./main || echo "correct"
//
// If it prints "correct" then the architecture should be here, in the "correct" section.
#if defined(_M_X64) || defined(__x86_64__) || \
    defined(__ARMEL__) || defined(__avr32__) || defined(_M_ARM) || defined(_M_ARM64) || \
    defined(__hppa__) || defined(__ia64__) || \
    defined(__mips__) || \
    defined(__loongarch__) || \
    defined(__nios2__) || defined(__ghs) || \
    defined(__powerpc__) || defined(__ppc__) || defined(__ppc64__) || \
    defined(_POWER) || defined(_ARCH_PPC) || defined(_ARCH_PPC64) || \
    defined(__sparc__) || defined(__sparc) || defined(__s390__) || \
    defined(__SH4__) || defined(__alpha__) || \
    defined(_MIPS_ARCH_MIPS32R2) || defined(__ARMEB__) ||\
    defined(__AARCH64EL__) || defined(__aarch64__) || defined(__AARCH64EB__) || \
    defined(__riscv) || defined(__e2k__) || \
    defined(__or1k__) || defined(__arc__) || defined(__ARC64__) || \
    defined(__microblaze__) || defined(__XTENSA__) || \
    defined(__EMSCRIPTEN__) || defined(__wasm32__)
#define DOUBLE_CONVERSION_CORRECT_DOUBLE_OPERATIONS 1
#elif defined(__mc68000__) || \
    defined(__pnacl__) || defined(__native_client__)
#undef DOUBLE_CONVERSION_CORRECT_DOUBLE_OPERATIONS
#elif defined(_M_IX86) || defined(__i386__) || defined(__i386)
#if defined(_WIN32)
// Windows uses a 64bit wide floating point stack.
#define DOUBLE_CONVERSION_CORRECT_DOUBLE_OPERATIONS 1
#else
#undef DOUBLE_CONVERSION_CORRECT_DOUBLE_OPERATIONS
#endif  // _WIN32
#else
#error Target architecture was not detected as supported by Double-Conversion.
#endif
#if defined(DOUBLE_CONVERSION_NON_PREFIXED_MACROS) && !defined(CORRECT_DOUBLE_OPERATIONS)
#define CORRECT_DOUBLE_OPERATIONS DOUBLE_CONVERSION_CORRECT_DOUBLE_OPERATIONS
#endif


typedef uint16_t uc16;
typedef const char *Iterator;


// The following macro works on both 32 and 64-bit platforms.
// Usage: instead of writing 0x1234567890123456
//      write DOUBLE_CONVERSION_UINT64_2PART_C(0x12345678,90123456);
#define DOUBLE_CONVERSION_UINT64_2PART_C(a, b) ((((uint64_t)(a) << 32) + (uint64_t)0x##b##u))
#if defined(DOUBLE_CONVERSION_NON_PREFIXED_MACROS) && !defined(UINT64_2PART_C)
#define UINT64_2PART_C DOUBLE_CONVERSION_UINT64_2PART_C
#endif

// The expression DOUBLE_CONVERSION_ARRAY_SIZE(a) is a compile-time constant of type
// size_t which represents the number of elements of the given
// array. You should only use DOUBLE_CONVERSION_ARRAY_SIZE on statically allocated
// arrays.
#ifndef DOUBLE_CONVERSION_ARRAY_SIZE
#define DOUBLE_CONVERSION_ARRAY_SIZE(a)                                   \
  ((sizeof(a) / sizeof(*(a))) /                         \
  (size_t)(!(sizeof(a) % sizeof(*(a)))))
#endif
#if defined(DOUBLE_CONVERSION_NON_PREFIXED_MACROS) && !defined(ARRAY_SIZE)
#define ARRAY_SIZE DOUBLE_CONVERSION_ARRAY_SIZE
#endif

static int StrLength(const char *string) {
    size_t length = strlen(string);
    DOUBLE_CONVERSION_ASSERT(length == (size_t)((int)(length)));
    return (int)(length);
}



// This is a simplified version of V8's Vector class.
typedef struct Vector {
    char *start;
    int length;
} Vector;

static void Vector_init(Vector *vec, char *data, int len) {
    DOUBLE_CONVERSION_ASSERT(len == 0 || (len > 0 && data != NULL));
    vec->start = data;
    vec->length = len;
}

static Vector Vector_make(char *data, int len) {
    Vector vec;
    Vector_init(&vec, data, len);
    return vec;
}

// Returns a vector using the same backing storage as this one,
// spanning from and including 'from', to but not including 'to'.
static Vector Vector_SubVector(Vector *vec, int from, int to) {
    DOUBLE_CONVERSION_ASSERT(to <= vec->length);
    DOUBLE_CONVERSION_ASSERT(from < to);
    DOUBLE_CONVERSION_ASSERT(0 <= from);
    return Vector_make(vec->start + from, to - from);
}

// Returns the length of the vector.
static int Vector_length(Vector *vec) {
    return vec->length;
}

// Returns whether or not the vector is empty.
static bool Vector_is_empty(Vector *vec) {
    return vec->length == 0;
}

// Returns the pointer to the start of the data in the vector.
static char *Vector_start(Vector *vec) {
    return vec->start;
}

// Access individual vector elements
static char Vector_at(Vector *vec, int index) {
    return vec->start[index];
}

static char Vector_first(Vector *vec) {
    return vec->start[0];
}

static char Vector_last(Vector *vec) {
    return vec->start[vec->length - 1];
}

static void Vector_pop_back(Vector *vec) {
    DOUBLE_CONVERSION_ASSERT(!Vector_is_empty(vec));
    vec->length--;
}



// Helper class for building result strings in a character buffer. The
// purpose of the class is to use safe operations that checks the
// buffer bounds on all operations in debug mode.
typedef struct StringBuilder {
    Vector buffer;
    int position;
} StringBuilder;

static void StringBuilder_init(StringBuilder *sb, char *buffer, int buffer_size) {
    sb->buffer = Vector_make(buffer, buffer_size);
    sb->position = 0;
}

static StringBuilder StringBuilder_make(char *buffer, int buffer_size) {
    StringBuilder sb;
    StringBuilder_init(&sb, buffer, buffer_size);
    return sb;
}

static bool StringBuilder_is_finalized(StringBuilder *sb) {
    return sb->position < 0;
}

// Finalize the string by 0-terminating it and returning the buffer.
static char *StringBuilder_Finalize(StringBuilder *sb) {
    DOUBLE_CONVERSION_ASSERT(!StringBuilder_is_finalized(sb) && sb->position < sb->buffer.length);
    sb->buffer.start[sb->position] = '\0';
    // Make sure nobody managed to add a 0-character to the
    // buffer while building the string.
    DOUBLE_CONVERSION_ASSERT(strlen(sb->buffer.start) == (size_t)(sb->position));
    sb->position = -1;
    DOUBLE_CONVERSION_ASSERT(StringBuilder_is_finalized(sb));
    return Vector_start(&sb->buffer);
}

static int StringBuilder_size(StringBuilder *sb) {
    return sb->buffer.length;
}
     
// Get the current position in the builder.
static int StringBuilder_position(StringBuilder *sb) {
    DOUBLE_CONVERSION_ASSERT(!StringBuilder_is_finalized(sb));
    return sb->position;
}

// Reset the position.
static void StringBuilder_Reset(StringBuilder *sb) {
    sb->position = 0;
}

// Add a single character to the builder. It is not allowed to add
// 0-characters; use the Finalize() method to terminate the string
// instead.
static void StringBuilder_AddCharacter(StringBuilder *sb, char c) {
    DOUBLE_CONVERSION_ASSERT(c != '\0');
    DOUBLE_CONVERSION_ASSERT(!StringBuilder_is_finalized(sb) && sb->position < sb->buffer.length);
    sb->buffer.start[sb->position++] = c;
}

// Add the first 'n' characters of the given string 's' to the
// builder. The input string must have enough characters.
static void StringBuilder_AddSubstring(StringBuilder *sb, const char* s, int n) {
    DOUBLE_CONVERSION_ASSERT(!StringBuilder_is_finalized(sb) && sb->position + n < sb->buffer.length);
    DOUBLE_CONVERSION_ASSERT((size_t)(n) <= strlen(s));
    memmove(sb->buffer.start + sb->position, s, n);
    sb->position += n;
}

// Add an entire string to the builder. Uses strlen() internally to
// compute the length of the input string.
static void StringBuilder_AddString(StringBuilder *sb, const char* s) {
    StringBuilder_AddSubstring(sb, s, StrLength(s));
}

// Add character padding to the builder. If count is non-positive,
// nothing is added to the builder.
static void StringBuilder_AddPadding(StringBuilder *sb, char c, int count) {
    for (int i = 0; i < count; i++) {
        StringBuilder_AddCharacter(sb, c);
    }
}



/// ============================================================================
/// diy-fp.h
/// ============================================================================

static const uint64_t DiyFp_kUint64MSB = DOUBLE_CONVERSION_UINT64_2PART_C(0x80000000, 00000000);
static const int DiyFp_kSignificandSize = 64;

// This "Do It Yourself Floating Point" class implements a floating-point number
// with a uint64 significand and an int exponent. Normalized DiyFp numbers will
// have the most significant bit of the significand set.
// Multiplication and Subtraction do not normalize their results.
// DiyFp store only non-negative numbers and are not designed to contain special
// doubles (NaN and Infinity).
typedef struct DiyFp {
    uint64_t f;
    int32_t e;
} DiyFp;

static void DiyFp_init(DiyFp *fp, uint64_t significand, int32_t exponent) {
    fp->f = significand;
    fp->e = exponent;
}

static DiyFp DiyFp_make(uint64_t significand, int32_t exponent) {
    DiyFp fp;
    DiyFp_init(&fp, significand, exponent);
    return fp;
}

// this -= other.
// The exponents of both numbers must be the same and the significand of this
// must be greater or equal than the significand of other.
// The result will not be normalized.
static void DiyFp_Subtract(DiyFp *fp, const DiyFp *other) {
    DOUBLE_CONVERSION_ASSERT(fp->e == other->e);
    DOUBLE_CONVERSION_ASSERT(fp->f >= other->f);
    fp->f -= other->f;
}

// Returns a - b.
// The exponents of both numbers must be the same and a must be greater
// or equal than b. The result will not be normalized.
static DiyFp DiyFp_Minus(const DiyFp *a, const DiyFp *b) {
    DiyFp result = *a;
    DiyFp_Subtract(&result, b);
    return result;
}

// this *= other.
static void DiyFp_Multiply(DiyFp *fp, const DiyFp *other) {
    // Simply "emulates" a 128 bit multiplication.
    // However: the resulting number only contains 64 bits. The least
    // significant 64 bits are only used for rounding the most significant 64
    // bits.
    const uint64_t kM32 = 0xFFFFFFFFU;
    const uint64_t a = fp->f >> 32;
    const uint64_t b = fp->f & kM32;
    const uint64_t c = other->f >> 32;
    const uint64_t d = other->f & kM32;
    const uint64_t ac = a * c;
    const uint64_t bc = b * c;
    const uint64_t ad = a * d;
    const uint64_t bd = b * d;
    // By adding 1U << 31 to tmp we round the final result.
    // Halfway cases will be rounded up.
    const uint64_t tmp = (bd >> 32) + (ad & kM32) + (bc & kM32) + (1U << 31);
    fp->e += other->e + 64;
    fp->f = ac + (ad >> 32) + (bc >> 32) + (tmp >> 32);
}

// returns a * b;
static DiyFp DiyFp_Times(const DiyFp *a, const DiyFp *b) {
    DiyFp result = *a;
    DiyFp_Multiply(&result, b);
    return result;
}

static void DiyFp_Normalize(DiyFp *fp) {
    DOUBLE_CONVERSION_ASSERT(fp->f != 0);
    uint64_t significand = fp->f;
    int32_t exponent = fp->e;

    // This method is mainly called for normalizing boundaries. In general,
    // boundaries need to be shifted by 10 bits, and we optimize for this case.
    const uint64_t k10MSBits = DOUBLE_CONVERSION_UINT64_2PART_C(0xFFC00000, 00000000);
    while ((significand & k10MSBits) == 0) {
        significand <<= 10;
        exponent -= 10;
    }
    while ((significand & DiyFp_kUint64MSB) == 0) {
        significand <<= 1;
        exponent--;
    }
    fp->f = significand;
    fp->e = exponent;
}

static DiyFp DiyFp_Normalize_make(const DiyFp *a) {
    DiyFp result = *a;
    DiyFp_Normalize(&result);
    return result;
}



/// ============================================================================
/// ieee.h
/// ============================================================================

// We assume that doubles and uint64_t have the same endianness.
static uint64_t double_to_uint64(double d) {
    uint64_t u;
    memcpy(&u, &d, sizeof(uint64_t));
    return u;
}

static double uint64_to_double(uint64_t d64) {
    double d;
    memcpy(&d, &d64, sizeof(uint64_t));
    return d;
}

static uint32_t float_to_uint32(float f) {
    uint32_t u;
    memcpy(&u, &f, sizeof(uint32_t));
    return u;
}

static float uint32_to_float(uint32_t d32) {
    float f;
    memcpy(&f, &d32, sizeof(uint32_t));
    return f;
}

static const uint64_t Double_kSignMask = DOUBLE_CONVERSION_UINT64_2PART_C(0x80000000, 00000000);
static const uint64_t Double_kExponentMask = DOUBLE_CONVERSION_UINT64_2PART_C(0x7FF00000, 00000000);
static const uint64_t Double_kSignificandMask = DOUBLE_CONVERSION_UINT64_2PART_C(0x000FFFFF, FFFFFFFF);
static const uint64_t Double_kHiddenBit = DOUBLE_CONVERSION_UINT64_2PART_C(0x00100000, 00000000);
static const uint64_t Double_kQuietNanBit = DOUBLE_CONVERSION_UINT64_2PART_C(0x00080000, 00000000);
#define Double_kPhysicalSignificandSize ((int)52)  // Excludes the hidden bit.
#define Double_kSignificandSize ((int)53)
#define Double_kExponentBias ((int)(0x3FF + Double_kPhysicalSignificandSize))
#define Double_kMaxExponent ((int)(0x7FF - Double_kExponentBias))
#define Double_kDenormalExponent  ((int)(-Double_kExponentBias + 1))
static const uint64_t Double_kInfinity = DOUBLE_CONVERSION_UINT64_2PART_C(0x7FF00000, 00000000);
#if (defined(__mips__) && !defined(__mips_nan2008)) || defined(__hppa__)
static const uint64_t Double_kNaN = DOUBLE_CONVERSION_UINT64_2PART_C(0x7FF7FFFF, FFFFFFFF);
#else
static const uint64_t Double_kNaN = DOUBLE_CONVERSION_UINT64_2PART_C(0x7FF80000, 00000000);
#endif

static uint64_t DiyFpToUint64(DiyFp *fp) {
    uint64_t significand = fp->f;
    int exponent = fp->e;
    while (significand > Double_kHiddenBit + Double_kSignificandMask) {
        significand >>= 1;
        exponent++;
    }
    if (exponent >= Double_kMaxExponent) {
        return Double_kInfinity;
    }
    if (exponent < Double_kDenormalExponent) {
        return 0;
    }
    while (exponent > Double_kDenormalExponent && (significand & Double_kHiddenBit) == 0) {
        significand <<= 1;
        exponent--;
    }
    uint64_t biased_exponent;
    if (exponent == Double_kDenormalExponent && (significand & Double_kHiddenBit) == 0) {
        biased_exponent = 0;
    } else {
        biased_exponent = (uint64_t)(exponent + Double_kExponentBias);
    }
    return (significand & Double_kSignificandMask) |
        (biased_exponent << Double_kPhysicalSignificandSize);
}



// Helper functions for doubles.
typedef struct Double {
    uint64_t d64;
} Double;
    
static Double Double_make_u(uint64_t d64) {
    Double d;
    d.d64 = d64;
    return d;
}

static Double Double_make(double d) {
    return Double_make_u(double_to_uint64(d));
}

static Double Double_make_diyfp(DiyFp *fp) {
    return Double_make_u(DiyFpToUint64(fp));
}

// Returns the double's bit as uint64.
static uint64_t Double_AsUint64(Double *d) {
    return d->d64;
}

static double Double_value(Double *d) {
    return uint64_to_double(d->d64);
}

static double Double_Infinity(void) {
    return uint64_to_double(Double_make_u(Double_kInfinity).d64);
}

static double Double_NaN(void) {
    return uint64_to_double(Double_make_u(Double_kNaN).d64);
}

// Returns true if the double is a denormal.
static bool Double_IsDenormal(Double *d) {
    uint64_t d64 = d->d64;
    return (d64 & Double_kExponentMask) == 0;
}

// We consider denormals not to be special.
// Hence only Infinity and NaN are special.
static bool Double_IsSpecial(Double *d) {
    uint64_t d64 = d->d64;
    return (d64 & Double_kExponentMask) == Double_kExponentMask;
}

static bool Double_IsNan(Double *d) {
    uint64_t d64 = d->d64;
    return ((d64 & Double_kExponentMask) == Double_kExponentMask) &&
        ((d64 & Double_kSignificandMask) != 0);
}

static bool Double_IsQuietNan(Double *d) {
#if (defined(__mips__) && !defined(__mips_nan2008)) || defined(__hppa__)
    return Double_IsNan(d) && ((d->d64 & Double_kQuietNanBit) == 0);
#else
    return Double_IsNan(d) && ((d->d64 & Double_kQuietNanBit) != 0);
#endif
}

static bool Double_IsSignalingNan(Double *d) {
#if (defined(__mips__) && !defined(__mips_nan2008)) || defined(__hppa__)
    return Double_IsNan(d) && ((d->d64 & Double_kQuietNanBit) != 0);
#else
    return Double_IsNan(d) && ((d->d64 & Double_kQuietNanBit) == 0);
#endif
}

static bool Double_IsInfinite(Double *d) {
    uint64_t d64 = d->d64;
    return ((d64 & Double_kExponentMask) == Double_kExponentMask) &&
      ((d64 & Double_kSignificandMask) == 0);
}

static int Double_Sign(Double *d) {
    uint64_t d64 = d->d64;
    return (d64 & Double_kSignMask) == 0? 1: -1;
}

static int Double_Exponent(Double *d) {
    if (Double_IsDenormal(d)) return Double_kDenormalExponent;
    
    uint64_t d64 = d->d64;
    int biased_e = (int)((d64 & Double_kExponentMask) >> Double_kPhysicalSignificandSize);
    return biased_e - Double_kExponentBias;
}

static uint64_t Double_Significand(Double *d) {
    uint64_t d64 = d->d64;
    uint64_t significand = d64 & Double_kSignificandMask;
    if (!Double_IsDenormal(d)) {
        return significand + Double_kHiddenBit;
    } else {
        return significand;
    }
}

// The value encoded by this Double must be greater or equal to +0.0.
// It must not be special (infinity, or NaN).
static DiyFp Double_AsDiyFp(Double *d) {
    DOUBLE_CONVERSION_ASSERT(Double_Sign(d) > 0);
    DOUBLE_CONVERSION_ASSERT(!Double_IsSpecial(d));
    return DiyFp_make(Double_Significand(d), Double_Exponent(d));
}

// The value encoded by this Double must be strictly greater than 0.
static DiyFp Double_AsNormalizedDiyFp(Double *d) {
    DOUBLE_CONVERSION_ASSERT(d->d64 > 0.0);
    uint64_t f = Double_Significand(d);
    int e = Double_Exponent(d);

    // The current double could be a denormal.
    while ((f & Double_kHiddenBit) == 0) {
        f <<= 1;
        e--;
    }
    // Do the final shifts in one go.
    f <<= DiyFp_kSignificandSize - Double_kSignificandSize;
    e -= DiyFp_kSignificandSize - Double_kSignificandSize;
    return DiyFp_make(f, e);
}

// Returns the next greater double. Returns +infinity on input +infinity.
static double Double_NextDouble(Double *d) {
    if (d->d64 == Double_kInfinity) return uint64_to_double(Double_make_u(Double_kInfinity).d64);
    if (Double_Sign(d) < 0 && Double_Significand(d) == 0) {
        // -0.0
        return 0.0;
    }
    if (Double_Sign(d) < 0) {
        return uint64_to_double(Double_make_u(d->d64 - 1).d64);
    } else {
        return uint64_to_double(Double_make_u(d->d64 + 1).d64);
    }
}

static double Double_PreviousDouble(Double *d) {
    if (d->d64 == (Double_kInfinity | Double_kSignMask)) return -Double_Infinity();
    if (Double_Sign(d) < 0) {
        return uint64_to_double(Double_make_u(d->d64 + 1).d64);
    } else {
        if (Double_Significand(d) == 0) return -0.0;
        return uint64_to_double(Double_make_u(d->d64 - 1).d64);
    }
}

// Precondition: the value encoded by this Double must be greater or equal
// than +0.0.
static DiyFp Double_UpperBoundary(Double *d) {
    DOUBLE_CONVERSION_ASSERT(Double_Sign(d) > 0);
    return DiyFp_make(Double_Significand(d) * 2 + 1, Double_Exponent(d) - 1);
}

static bool Double_LowerBoundaryIsCloser(Double *d) {
    // The boundary is closer if the significand is of the form f == 2^p-1 then
    // the lower boundary is closer.
    // Think of v = 1000e10 and v- = 9999e9.
    // Then the boundary (== (v - v-)/2) is not just at a distance of 1e9 but
    // at a distance of 1e8.
    // The only exception is for the smallest normal: the largest denormal is
    // at the same distance as its successor.
    // Note: denormals have the same exponent as the smallest normals.
    bool physical_significand_is_zero = ((d->d64 & Double_kSignificandMask) == 0);
    return physical_significand_is_zero && (Double_Exponent(d) != Double_kDenormalExponent);
}

// Computes the two boundaries of this.
// The bigger boundary (m_plus) is normalized. The lower boundary has the same
// exponent as m_plus.
// Precondition: the value encoded by this Double must be greater than 0.
static void Double_NormalizedBoundaries(Double *d, DiyFp *out_m_minus, DiyFp *out_m_plus) {
    DOUBLE_CONVERSION_ASSERT(d->d64 > 0.0);
    DiyFp v = Double_AsDiyFp(d);
    DiyFp t = DiyFp_make((v.f << 1) + 1, v.e - 1);
    DiyFp m_plus = DiyFp_Normalize_make(&t);
    DiyFp m_minus;
    if (Double_LowerBoundaryIsCloser(d)) {
      m_minus = DiyFp_make((v.f << 2) - 1, v.e - 2);
    } else {
      m_minus = DiyFp_make((v.f << 1) - 1, v.e - 1);
    }
    m_minus.f = (m_minus.f << (m_minus.e - m_plus.e));
    m_minus.e = (m_plus.e);
    *out_m_plus = m_plus;
    *out_m_minus = m_minus;
}

// Returns the significand size for a given order of magnitude.
// If v = f*2^e with 2^p-1 <= f <= 2^p then p+e is v's order of magnitude.
// This function returns the number of significant binary digits v will have
// once it's encoded into a double. In almost all cases this is equal to
// kSignificandSize. The only exceptions are denormals. They start with
// leading zeroes and their effective significand-size is hence smaller.
static int Double_SignificandSizeForOrderOfMagnitude(int order) {
    if (order >= (Double_kDenormalExponent + Double_kSignificandSize)) {
      return Double_kSignificandSize;
    }
    if (order <= Double_kDenormalExponent) return 0;
    return order - Double_kDenormalExponent;
}



static const uint32_t Single_kSignMask = 0x80000000;
static const uint32_t Single_kExponentMask = 0x7F800000;
static const uint32_t Single_kSignificandMask = 0x007FFFFF;
static const uint32_t Single_kHiddenBit = 0x00800000;
static const uint32_t Single_kQuietNanBit = 0x00400000;
#define Single_kPhysicalSignificandSize ((int)23)  // Excludes the hidden bit.
#define Single_kSignificandSize ((int)24)
#define Single_kExponentBias ((int)(0x7F + Single_kPhysicalSignificandSize))
#define Single_kDenormalExponent ((int)(-Single_kExponentBias + 1))
#define Single_kMaxExponent ((int)(0xFF - Single_kExponentBias))
static const uint32_t Single_kInfinity = 0x7F800000;
#if (defined(__mips__) && !defined(__mips_nan2008)) || defined(__hppa__)
static const uint32_t Single_kNaN = 0x7FBFFFFF;
#else
static const uint32_t Single_kNaN = 0x7FC00000;
#endif

typedef struct Single {
    uint32_t d32;
} Single;

static Single Single_make(float f) {
    Single s;
    s.d32 = float_to_uint32(f);
    return s;
}

static Single Single_make_u(uint32_t u) {
    Single s;
    s.d32 = u;
    return s;
}

static float Single_value(Single *s) {
    return uint32_to_float(s->d32);
}

static float Single_Infinity(void) {
    return uint32_to_float(Single_make_u(Single_kInfinity).d32);
}

static float Single_NaN(void) {
    return uint32_to_float(Single_make_u(Single_kNaN).d32);
}

// Returns the single's bit as uint64.
static uint32_t Single_AsUint32(Single *s) {
    return s->d32;
}

// Returns true if the single is a denormal.
static bool Single_IsDenormal(Single *s) {
    uint32_t d32 = Single_AsUint32(s);
    return (d32 & Single_kExponentMask) == 0;
}

// We consider denormals not to be special.
// Hence only Infinity and NaN are special.
static bool Single_IsSpecial(Single *s) {
    uint32_t d32 = Single_AsUint32(s);
    return (d32 & Single_kExponentMask) == Single_kExponentMask;
}

static bool Single_IsNan(Single *s) {
    uint32_t d32 = Single_AsUint32(s);
    return ((d32 & Single_kExponentMask) == Single_kExponentMask) &&
        ((d32 & Single_kSignificandMask) != 0);
}

static bool Single_IsQuietNan(Single *s) {
#if (defined(__mips__) && !defined(__mips_nan2008)) || defined(__hppa__)
    return Single_IsNan(s) && ((Single_AsUint32(s) & Single_kQuietNanBit) == 0);
#else
    return Single_IsNan(s) && ((Single_AsUint32(s) & Single_kQuietNanBit) != 0);
#endif
}

static bool Single_IsSignalingNan(Single *s) {
#if (defined(__mips__) && !defined(__mips_nan2008)) || defined(__hppa__)
    return Single_IsNan(s) && ((Single_AsUint32(s) & Single_kQuietNanBit) != 0);
#else
    return Single_IsNan(s) && ((Single_AsUint32(s) & Single_kQuietNanBit) == 0);
#endif
}

static int Single_Exponent(Single *s) {
    if (Single_IsDenormal(s)) return Single_kDenormalExponent;
    
    uint32_t d32 = Single_AsUint32(s);
    int biased_e =
        (int)((d32 & Single_kExponentMask) >> Single_kPhysicalSignificandSize);
    return biased_e - Single_kExponentBias;
}

static uint32_t Single_Significand(Single *s) {
    uint32_t d32 = Single_AsUint32(s);
    uint32_t significand = d32 & Single_kSignificandMask;
    if (!Single_IsDenormal(s)) {
        return significand + Single_kHiddenBit;
    } else {
        return significand;
    }
}

static bool Single_IsInfinite(Single *s) {
    uint32_t d32 = Single_AsUint32(s);
    return ((d32 & Single_kExponentMask) == Single_kExponentMask) &&
        ((d32 & Single_kSignificandMask) == 0);
}

static int Single_Sign(Single *s) {
    uint32_t d32 = Single_AsUint32(s);
    return (d32 & Single_kSignMask) == 0? 1: -1;
}

// The value encoded by this Single must be greater or equal to +0.0.
// It must not be special (infinity, or NaN).
static DiyFp Single_AsDiyFp(Single *s)  {
    DOUBLE_CONVERSION_ASSERT(Single_Sign(s) > 0);
    DOUBLE_CONVERSION_ASSERT(!Single_IsSpecial(s));
    return DiyFp_make(Single_Significand(s), Single_Exponent(s));
}

// Precondition: the value encoded by this Single must be greater or equal
// than +0.0.
static DiyFp Single_UpperBoundary(Single *s) {
    DOUBLE_CONVERSION_ASSERT(Single_Sign(s) > 0);
    return DiyFp_make(Single_Significand(s) * 2 + 1, Single_Exponent(s) - 1);
}

static bool Single_LowerBoundaryIsCloser(Single *s) {
    // The boundary is closer if the significand is of the form f == 2^p-1 then
    // the lower boundary is closer.
    // Think of v = 1000e10 and v- = 9999e9.
    // Then the boundary (== (v - v-)/2) is not just at a distance of 1e9 but
    // at a distance of 1e8.
    // The only exception is for the smallest normal: the largest denormal is
    // at the same distance as its successor.
    // Note: denormals have the same exponent as the smallest normals.
    bool physical_significand_is_zero = ((Single_AsUint32(s) & Single_kSignificandMask) == 0);
    return physical_significand_is_zero && (Single_Exponent(s) != Single_kDenormalExponent);
}

// Computes the two boundaries of this.
// The bigger boundary (m_plus) is normalized. The lower boundary has the same
// exponent as m_plus.
// Precondition: the value encoded by this Single must be greater than 0.
static void Single_NormalizedBoundaries(Single *s, DiyFp *out_m_minus, DiyFp *out_m_plus) {
    DOUBLE_CONVERSION_ASSERT(Single_value(s) > 0.0);
    DiyFp v = Single_AsDiyFp(s);
    DiyFp t = DiyFp_make((v.f << 1) + 1, v.e - 1);
    DiyFp m_plus = DiyFp_Normalize_make(&t);
    DiyFp m_minus;
    if (Single_LowerBoundaryIsCloser(s)) {
        m_minus = DiyFp_make((v.f << 2) - 1, v.e - 2);
    } else {
        m_minus = DiyFp_make((v.f << 1) - 1, v.e - 1);
    }
    m_minus.f = (m_minus.f << (m_minus.e - m_plus.e));
    m_minus.e = (m_plus.e);
    *out_m_plus = m_plus;
    *out_m_minus = m_minus;
}



/// ============================================================================
/// bignum.h
/// ============================================================================

typedef uint32_t Bignum_Chunk;
typedef uint64_t Bignum_DoubleChunk;

// 3584 = 128 * 28. We can represent 2^3584 > 10^1000 accurately.
// This bignum can encode much bigger numbers, since it contains an
// exponent.
#define Bignum_kMaxSignificantBits ((int)3584)
static const int Bignum_kChunkSize = sizeof(Bignum_Chunk) * 8;
static const int Bignum_kDoubleChunkSize = sizeof(Bignum_DoubleChunk) * 8;
// With bigit size of 28 we loose some bits, but a double still fits easily
// into two chunks, and more importantly we can use the Comba multiplication.
#define Bignum_kBigitSize ((int)28)
static const Bignum_Chunk Bignum_kBigitMask = (1 << Bignum_kBigitSize) - 1;
// Every instance allocates kBigitLength chunks on the stack. Bignums cannot
// grow. There are no checks if the stack-allocated space is sufficient.
#define Bignum_kBigitCapacity ((int)(Bignum_kMaxSignificantBits / Bignum_kBigitSize))

typedef struct Bignum {
    // The Bignum's value is value(bigits_buffer_) * 2^(exponent_ * kBigitSize),
    // where the value of the buffer consists of the lower kBigitSize bits of
    // the first used_bigits_ Chunks in bigits_buffer_, first chunk has lowest
    // significant bits.
    int16_t used_bigits;
    int16_t exponent;
    Bignum_Chunk bigits_buffer[Bignum_kBigitCapacity];
} Bignum;



/// ============================================================================
/// bignum.cc
/// ============================================================================

static Bignum_Chunk *Bignum_RawBigit(Bignum *b, int index) {
    return &b->bigits_buffer[index];
}

static void Bignum_Zero(Bignum *b) {
    b->used_bigits = 0;
    b->exponent = 0;
}

// BigitLength includes the "hidden" bigits encoded in the exponent.
static int Bignum_BigitLength(Bignum *b) {
    return b->used_bigits + b->exponent;
}

static void Bignum_EnsureCapacity(int size) {
    if (size > Bignum_kBigitCapacity) {
        DOUBLE_CONVERSION_UNREACHABLE();
    }
}

static void Bignum_Align(Bignum *b, Bignum *other) {
    if (b->exponent > other->exponent) {
        // If "X" represents a "hidden" bigit (by the exponent) then we are in the
        // following case (a == this, b == other):
        // a:  aaaaaaXXXX   or a:   aaaaaXXX
        // b:     bbbbbbX      b: bbbbbbbbXX
        // We replace some of the hidden digits (X) of a with 0 digits.
        // a:  aaaaaa000X   or a:   aaaaa0XX
        const int zero_bigits = b->exponent - other->exponent;
        Bignum_EnsureCapacity(b->used_bigits + zero_bigits);
        for (int i = b->used_bigits - 1; i >= 0; --i) {
            *Bignum_RawBigit(b, i + zero_bigits) = *Bignum_RawBigit(b, i);
        }
        for (int i = 0; i < zero_bigits; ++i) {
            *Bignum_RawBigit(b, i) = 0;
        }
        b->used_bigits += zero_bigits;
        b->exponent -= zero_bigits;
        
        DOUBLE_CONVERSION_ASSERT(b->used_bigits >= 0);
        DOUBLE_CONVERSION_ASSERT(b->exponent >= 0);
    }
}

static void Bignum_Clamp(Bignum *b) {
    while (b->used_bigits > 0 && *Bignum_RawBigit(b, b->used_bigits - 1) == 0) {
        b->used_bigits--;
    }
    if (b->used_bigits == 0) {
      // Zero.
      b->exponent = 0;
    }
}

static bool Bignum_IsClamped(Bignum *b) {
    return b->used_bigits == 0 || *Bignum_RawBigit(b, b->used_bigits - 1) != 0;
}

static Bignum_Chunk Bignum_BigitOrZero(Bignum *b, const int index) {
    if (index >= Bignum_BigitLength(b)) {
      return 0;
    }
    if (index < b->exponent) {
        return 0;
    }
    return *Bignum_RawBigit(b, index - b->exponent);
}

// Returns
//  -1 if a < b,
//   0 if a == b, and
//  +1 if a > b.
static int Bignum_Compare(Bignum *a, Bignum *b) {
    DOUBLE_CONVERSION_ASSERT(Bignum_IsClamped(a));
    DOUBLE_CONVERSION_ASSERT(Bignum_IsClamped(b));
    const int bigit_length_a = Bignum_BigitLength(a);
    const int bigit_length_b = Bignum_BigitLength(b);
    if (bigit_length_a < bigit_length_b) {
        return -1;
    }
    if (bigit_length_a > bigit_length_b) {
        return +1;
    }
    for (int i = bigit_length_a - 1; i >= MIN(a->exponent, b->exponent); --i) {
        const Bignum_Chunk bigit_a = Bignum_BigitOrZero(a, i);
        const Bignum_Chunk bigit_b = Bignum_BigitOrZero(b, i);
        if (bigit_a < bigit_b) {
            return -1;
        }
        if (bigit_a > bigit_b) {
            return +1;
        }
        // Otherwise they are equal up to this digit. Try the next digit.
    }
    return 0;
}

static bool Bignum_Equal(Bignum *a, Bignum *b) {
    return Bignum_Compare(a, b) == 0;
}

static bool Bignum_LessEqual(Bignum *a, Bignum *b) {
    return Bignum_Compare(a, b) <= 0;
}

static bool Bignum_Less(Bignum *a, Bignum *b) {
    return Bignum_Compare(a, b) < 0;
}

// Returns Compare(a + b, c);
static int Bignum_PlusCompare(Bignum *a, Bignum *b, Bignum *c) {
    DOUBLE_CONVERSION_ASSERT(Bignum_IsClamped(a));
    DOUBLE_CONVERSION_ASSERT(Bignum_IsClamped(b));
    DOUBLE_CONVERSION_ASSERT(Bignum_IsClamped(c));
    if (Bignum_BigitLength(a) < Bignum_BigitLength(b)) {
        return Bignum_PlusCompare(b, a, c);
    }
    if (Bignum_BigitLength(a) + 1 < Bignum_BigitLength(c)) {
        return -1;
    }
    if (Bignum_BigitLength(a) > Bignum_BigitLength(c)) {
        return +1;
    }
    // The exponent encodes 0-bigits. So if there are more 0-digits in 'a' than
    // 'b' has digits, then the bigit-length of 'a'+'b' must be equal to the one
    // of 'a'.
    if (a->exponent >= Bignum_BigitLength(b) && Bignum_BigitLength(a) < Bignum_BigitLength(c)) {
        return -1;
    }
    
    Bignum_Chunk borrow = 0;
    // Starting at min_exponent all digits are == 0. So no need to compare them.
    const int min_exponent = MIN(MIN(a->exponent, b->exponent), c->exponent);
    for (int i = Bignum_BigitLength(c) - 1; i >= min_exponent; --i) {
      const Bignum_Chunk chunk_a = Bignum_BigitOrZero(a, i);
      const Bignum_Chunk chunk_b = Bignum_BigitOrZero(b, i);
      const Bignum_Chunk chunk_c = Bignum_BigitOrZero(c, i);
      const Bignum_Chunk sum = chunk_a + chunk_b;
      if (sum > chunk_c + borrow) {
          return +1;
      } else {
          borrow = chunk_c + borrow - sum;
          if (borrow > 1) {
              return -1;
          }
          borrow <<= Bignum_kBigitSize;
        }
    }
    if (borrow == 0) {
        return 0;
    }
    return -1;
}

// Returns a + b == c
static bool Bignum_PlusEqual(Bignum *a, Bignum *b, Bignum *c) {
    return Bignum_PlusCompare(a, b, c) == 0;
}

// Returns a + b <= c
static bool Bignum_PlusLessEqual(Bignum *a, Bignum *b, Bignum *c) {
    return Bignum_PlusCompare(a, b, c) <= 0;
}

// Returns a + b < c
static bool Bignum_PlusLess(Bignum *a, Bignum *b, Bignum *c) {
    return Bignum_PlusCompare(a, b, c) < 0;
}

// Guaranteed to lie in one Bigit.
static void Bignum_AssignUInt16(Bignum *b, uint16_t value) {
    Bignum_Zero(b);
    if (value > 0) {
        *Bignum_RawBigit(b, 0) = value;
        b->used_bigits = 1;
    }
}

static void Bignum_AssignUInt64(Bignum *b, uint64_t value) {
    Bignum_Zero(b);
    for(int i = 0; value > 0; ++i) {
        *Bignum_RawBigit(b, i) = value & Bignum_kBigitMask;
        value >>= Bignum_kBigitSize;
        b->used_bigits++;
    }
}

static void Bignum_AssignBignum(Bignum *b, Bignum *other) {
    b->exponent = other->exponent;
    for (int i = 0; i < other->used_bigits; ++i) {
        *Bignum_RawBigit(b, i) = *Bignum_RawBigit(other, i);
    }
    b->used_bigits = other->used_bigits;
}

static void Bignum_AddBignum(Bignum *b, Bignum *other) {
    DOUBLE_CONVERSION_ASSERT(Bignum_IsClamped(b));
    DOUBLE_CONVERSION_ASSERT(Bignum_IsClamped(other));
    
    // If this has a greater exponent than other append zero-bigits to this.
    // After this call exponent_ <= other.exponent_.
    Bignum_Align(b, other);
    
    // There are two possibilities:
    //   aaaaaaaaaaa 0000  (where the 0s represent a's exponent)
    //     bbbbb 00000000
    //   ----------------
    //   ccccccccccc 0000
    // or
    //    aaaaaaaaaa 0000
    //  bbbbbbbbb 0000000
    //  -----------------
    //  cccccccccccc 0000
    // In both cases we might need a carry bigit.
    
    Bignum_EnsureCapacity(1 + MAX(Bignum_BigitLength(b), Bignum_BigitLength(other)) - b->exponent);
    Bignum_Chunk carry = 0;
    int bigit_pos = other->exponent - b->exponent;
    DOUBLE_CONVERSION_ASSERT(bigit_pos >= 0);
    for (int i = b->used_bigits; i < bigit_pos; ++i) {
        *Bignum_RawBigit(b, i) = 0;
    }
    for (int i = 0; i < other->used_bigits; ++i) {
        const Bignum_Chunk my = (bigit_pos < b->used_bigits) ? *Bignum_RawBigit(b, bigit_pos) : 0;
        const Bignum_Chunk sum = my + *Bignum_RawBigit(other, i) + carry;
        *Bignum_RawBigit(b, bigit_pos) = sum & Bignum_kBigitMask;
        carry = sum >> Bignum_kBigitSize;
        ++bigit_pos;
    }
    while (carry != 0) {
        const Bignum_Chunk my = (bigit_pos < b->used_bigits) ? *Bignum_RawBigit(b, bigit_pos) : 0;
        const Bignum_Chunk sum = my + carry;
        *Bignum_RawBigit(b, bigit_pos) = sum & Bignum_kBigitMask;
        carry = sum >> Bignum_kBigitSize;
        ++bigit_pos;
    }
    b->used_bigits = (int16_t)MAX(bigit_pos, (int)(b->used_bigits));
    DOUBLE_CONVERSION_ASSERT(Bignum_IsClamped(b));
}

static void Bignum_SubtractBignum(Bignum *b, Bignum *other) {
    DOUBLE_CONVERSION_ASSERT(Bignum_IsClamped(b));
    DOUBLE_CONVERSION_ASSERT(Bignum_IsClamped(other));
    // We require this to be bigger than other.
    DOUBLE_CONVERSION_ASSERT(Bignum_LessEqual(other, b));
    
    Bignum_Align(b, other);
    
    const int offset = other->exponent - b->exponent;
    Bignum_Chunk borrow = 0;
    int i;
    for (i = 0; i < other->used_bigits; ++i) {
        DOUBLE_CONVERSION_ASSERT((borrow == 0) || (borrow == 1));
        const Bignum_Chunk difference = *Bignum_RawBigit(b, i + offset) - *Bignum_RawBigit(other, i) - borrow;
        *Bignum_RawBigit(b, i + offset) = difference & Bignum_kBigitMask;
        borrow = difference >> (Bignum_kChunkSize - 1);
    }
    while (borrow != 0) {
        const Bignum_Chunk difference = *Bignum_RawBigit(b, i + offset) - borrow;
        *Bignum_RawBigit(b, i + offset) = difference & Bignum_kBigitMask;
        borrow = difference >> (Bignum_kChunkSize - 1);
        ++i;
    }
    Bignum_Clamp(b);
}

static void Bignum_AddUInt64(Bignum *b, uint64_t operand) {
    if (operand == 0) {
        return;
    }
    Bignum other;
    Bignum_AssignUInt64(&other, operand);
    Bignum_AddBignum(b, &other);
}

static uint64_t Bignum_ReadUInt64(const Vector *buffer,
                                  int from,
                                  int digits_to_read) {
    uint64_t result = 0;
    for (int i = from; i < from + digits_to_read; ++i) {
        const int digit = buffer->start[i] - '0';
        DOUBLE_CONVERSION_ASSERT(0 <= digit && digit <= 9);
        result = result * 10 + digit;
    }
    return result;
}

static void Bignum_BigitsShiftLeft(Bignum *b, int shift_amount) {
    DOUBLE_CONVERSION_ASSERT(shift_amount < Bignum_kBigitSize);
    DOUBLE_CONVERSION_ASSERT(shift_amount >= 0);
    Bignum_Chunk carry = 0;
    for (int i = 0; i < b->used_bigits; ++i) {
        const Bignum_Chunk new_carry = *Bignum_RawBigit(b, i) >> (Bignum_kBigitSize - shift_amount);
        *Bignum_RawBigit(b, i) = ((*Bignum_RawBigit(b, i) << shift_amount) + carry) & Bignum_kBigitMask;
        carry = new_carry;
    }
    if (carry != 0) {
        *Bignum_RawBigit(b, b->used_bigits) = carry;
        b->used_bigits++;
    }
}

static void Bignum_ShiftLeft(Bignum *b, int shift_amount) {
    if (b->used_bigits == 0) {
        return;
    }
    b->exponent += (shift_amount / Bignum_kBigitSize);
    const int local_shift = shift_amount % Bignum_kBigitSize;
    Bignum_EnsureCapacity(b->used_bigits + 1);
    Bignum_BigitsShiftLeft(b, local_shift);
}

static void Bignum_MultiplyByUInt32(Bignum *b, uint32_t factor) {
    if (factor == 1) {
        return;
    }
    if (factor == 0) {
        Bignum_Zero(b);
        return;
    }
    if (b->used_bigits == 0) {
        return;
    }
    // The product of a bigit with the factor is of size kBigitSize + 32.
    // Assert that this number + 1 (for the carry) fits into double chunk.
    DOUBLE_CONVERSION_ASSERT(Bignum_kDoubleChunkSize >= Bignum_kBigitSize + 32 + 1);
    Bignum_DoubleChunk carry = 0;
    for (int i = 0; i < b->used_bigits; ++i) {
        const Bignum_DoubleChunk product = (Bignum_DoubleChunk)(factor) * *Bignum_RawBigit(b, i) + carry;
        *Bignum_RawBigit(b, i) = (Bignum_Chunk)(product & Bignum_kBigitMask);
        carry = (product >> Bignum_kBigitSize);
    }
    while (carry != 0) {
        Bignum_EnsureCapacity(b->used_bigits + 1);
        *Bignum_RawBigit(b, b->used_bigits) = carry & Bignum_kBigitMask;
        b->used_bigits++;
        carry >>= Bignum_kBigitSize;
    }
}

static void Bignum_MultiplyByUInt64(Bignum *b, uint64_t factor) {
    if (factor == 1) {
        return;
    }
    if (factor == 0) {
        Bignum_Zero(b);
        return;
    }
    if (b->used_bigits == 0) {
        return;
    }
    DOUBLE_CONVERSION_ASSERT(Bignum_kBigitSize < 32);
    uint64_t carry = 0;
    const uint64_t low = factor & 0xFFFFFFFF;
    const uint64_t high = factor >> 32;
    for (int i = 0; i < b->used_bigits; ++i) {
        const uint64_t product_low = low * *Bignum_RawBigit(b, i);
        const uint64_t product_high = high * *Bignum_RawBigit(b, i);
        const uint64_t tmp = (carry & Bignum_kBigitMask) + product_low;
        *Bignum_RawBigit(b, i) = tmp & Bignum_kBigitMask;
        carry = (carry >> Bignum_kBigitSize) + (tmp >> Bignum_kBigitSize) +
            (product_high << (32 - Bignum_kBigitSize));
    }
    while (carry != 0) {
        Bignum_EnsureCapacity(b->used_bigits + 1);
        *Bignum_RawBigit(b, b->used_bigits) = carry & Bignum_kBigitMask;
        b->used_bigits++;
        carry >>= Bignum_kBigitSize;
    }
}

static void Bignum_MultiplyByPowerOfTen(Bignum *b, int exponent) {
    const uint64_t kFive27 = DOUBLE_CONVERSION_UINT64_2PART_C(0x6765c793, fa10079d);
    const uint16_t kFive1 = 5;
    const uint16_t kFive2 = kFive1 * 5;
    const uint16_t kFive3 = kFive2 * 5;
    const uint16_t kFive4 = kFive3 * 5;
    const uint16_t kFive5 = kFive4 * 5;
    const uint16_t kFive6 = kFive5 * 5;
    const uint32_t kFive7 = kFive6 * 5;
    const uint32_t kFive8 = kFive7 * 5;
    const uint32_t kFive9 = kFive8 * 5;
    const uint32_t kFive10 = kFive9 * 5;
    const uint32_t kFive11 = kFive10 * 5;
    const uint32_t kFive12 = kFive11 * 5;
    const uint32_t kFive13 = kFive12 * 5;
    const uint32_t kFive1_to_12[] =
      { kFive1, kFive2, kFive3, kFive4, kFive5, kFive6,
        kFive7, kFive8, kFive9, kFive10, kFive11, kFive12 };
    
    DOUBLE_CONVERSION_ASSERT(exponent >= 0);
    
    if (exponent == 0) {
        return;
    }
    if (b->used_bigits == 0) {
        return;
    }
    // We shift by exponent at the end just before returning.
    int remaining_exponent = exponent;
    while (remaining_exponent >= 27) {
        Bignum_MultiplyByUInt64(b, kFive27);
        remaining_exponent -= 27;
    }
    while (remaining_exponent >= 13) {
        Bignum_MultiplyByUInt32(b, kFive13);
        remaining_exponent -= 13;
    }
    if (remaining_exponent > 0) {
        Bignum_MultiplyByUInt32(b, kFive1_to_12[remaining_exponent - 1]);
    }
    Bignum_ShiftLeft(b, exponent);
}

static void Bignum_AssignDecimalString(Bignum *b, const Vector *value) {
    // 2^64 = 18446744073709551616 > 10^19
    static const int kMaxUint64DecimalDigits = 19;
    Bignum_Zero(b);
    int length = value->length;
    unsigned pos = 0;
    // Let's just say that each digit needs 4 bits.
    while (length >= kMaxUint64DecimalDigits) {
        const uint64_t digits = Bignum_ReadUInt64(value, pos, kMaxUint64DecimalDigits);
        pos += kMaxUint64DecimalDigits;
        length -= kMaxUint64DecimalDigits;
        Bignum_MultiplyByPowerOfTen(b, kMaxUint64DecimalDigits);
        Bignum_AddUInt64(b, digits);
    }
    const uint64_t digits = Bignum_ReadUInt64(value, pos, length);
    Bignum_MultiplyByPowerOfTen(b, length);
    Bignum_AddUInt64(b, digits);
    Bignum_Clamp(b);
}

static uint64_t Bignum_HexCharValue(int c) {
    if ('0' <= c && c <= '9') {
        return c - '0';
    }
    if ('a' <= c && c <= 'f') {
        return 10 + c - 'a';
    }
    DOUBLE_CONVERSION_ASSERT('A' <= c && c <= 'F');
    return 10 + c - 'A';
}

// Unlike AssignDecimalString(), this function is "only" used
// for unit-tests and therefore not performance critical.
static void Bignum_AssignHexString(Bignum *b, Vector *value) {
    Bignum_Zero(b);
    // Required capacity could be reduced by ignoring leading zeros.
    Bignum_EnsureCapacity(((value->length * 4) + Bignum_kBigitSize - 1) / Bignum_kBigitSize);
    DOUBLE_CONVERSION_ASSERT(sizeof(uint64_t) * 8 >= Bignum_kBigitSize + 4);  // TODO: static_assert
    // Accumulates converted hex digits until at least kBigitSize bits.
    // Works with non-factor-of-four kBigitSizes.
    uint64_t tmp = 0;
    for (int cnt = 0; !Vector_is_empty(value); Vector_pop_back(value)) {
        tmp |= (Bignum_HexCharValue(Vector_last(value)) << cnt);
        if ((cnt += 4) >= Bignum_kBigitSize) {
            *Bignum_RawBigit(b, b->used_bigits++) = (tmp & Bignum_kBigitMask);
            cnt -= Bignum_kBigitSize;
            tmp >>= Bignum_kBigitSize;
        }
    }
    if (tmp > 0) {
        DOUBLE_CONVERSION_ASSERT(tmp <= Bignum_kBigitMask);
        *Bignum_RawBigit(b, b->used_bigits++) = (tmp & Bignum_kBigitMask);
    }
    Bignum_Clamp(b);
}

static void Bignum_Times10(Bignum *b) {
    Bignum_MultiplyByUInt32(b, 10);
}

static void Bignum_Square(Bignum *b) {
    DOUBLE_CONVERSION_ASSERT(Bignum_IsClamped(b));
    const int product_length = 2 * b->used_bigits;
    Bignum_EnsureCapacity(product_length);

    // Comba multiplication: compute each column separately.
    // Example: r = a2a1a0 * b2b1b0.
    //    r =  1    * a0b0 +
    //        10    * (a1b0 + a0b1) +
    //        100   * (a2b0 + a1b1 + a0b2) +
    //        1000  * (a2b1 + a1b2) +
    //        10000 * a2b2
    //
    // In the worst case we have to accumulate nb-digits products of digit*digit.
    //
    // Assert that the additional number of bits in a DoubleChunk are enough to
    // sum up used_digits of Bigit*Bigit.
    if ((1 << (2 * (Bignum_kChunkSize - Bignum_kBigitSize))) <= b->used_bigits) {
        DOUBLE_CONVERSION_UNIMPLEMENTED();
    }
    Bignum_DoubleChunk accumulator = 0;
    // First shift the digits so we don't overwrite them.
    const int copy_offset = b->used_bigits;
    for (int i = 0; i < b->used_bigits; ++i) {
        *Bignum_RawBigit(b, copy_offset + i) = *Bignum_RawBigit(b, i);
    }
    // We have two loops to avoid some 'if's in the loop.
    for (int i = 0; i < b->used_bigits; ++i) {
        // Process temporary digit i with power i.
        // The sum of the two indices must be equal to i.
        int bigit_index1 = i;
        int bigit_index2 = 0;
        // Sum all of the sub-products.
        while (bigit_index1 >= 0) {
            const Bignum_Chunk chunk1 = *Bignum_RawBigit(b, copy_offset + bigit_index1);
            const Bignum_Chunk chunk2 = *Bignum_RawBigit(b, copy_offset + bigit_index2);
            accumulator += (Bignum_DoubleChunk)(chunk1) * chunk2;
            bigit_index1--;
            bigit_index2++;
        }
        *Bignum_RawBigit(b, i) = (Bignum_Chunk)(accumulator) & Bignum_kBigitMask;
        accumulator >>= Bignum_kBigitSize;
    }
    for (int i = b->used_bigits; i < product_length; ++i) {
        int bigit_index1 = b->used_bigits - 1;
        int bigit_index2 = i - bigit_index1;
        // Invariant: sum of both indices is again equal to i.
        // Inner loop runs 0 times on last iteration, emptying accumulator.
        while (bigit_index2 < b->used_bigits) {
            const Bignum_Chunk chunk1 = *Bignum_RawBigit(b, copy_offset + bigit_index1);
            const Bignum_Chunk chunk2 = *Bignum_RawBigit(b, copy_offset + bigit_index2);
            accumulator += (Bignum_DoubleChunk)(chunk1) * chunk2;
            bigit_index1--;
            bigit_index2++;
        }
        // The overwritten RawBigit(i) will never be read in further loop iterations,
        // because bigit_index1 and bigit_index2 are always greater
        // than i - used_bigits_.
        *Bignum_RawBigit(b, i) = (Bignum_Chunk)(accumulator) & Bignum_kBigitMask;
        accumulator >>= Bignum_kBigitSize;
    }
    // Since the result was guaranteed to lie inside the number the
    // accumulator must be 0 now.
    DOUBLE_CONVERSION_ASSERT(accumulator == 0);

    // Don't forget to update the used_digits and the exponent.
    b->used_bigits = (int16_t)product_length;
    b->exponent *= 2;
    Bignum_Clamp(b);
}

static void Bignum_AssignPowerUInt16(Bignum *b, uint16_t base, int power_exponent) {
    DOUBLE_CONVERSION_ASSERT(base != 0);
    DOUBLE_CONVERSION_ASSERT(power_exponent >= 0);
    if (power_exponent == 0) {
        Bignum_AssignUInt16(b, 1);
        return;
    }
    Bignum_Zero(b);
    int shifts = 0;
    // We expect base to be in range 2-32, and most often to be 10.
    // It does not make much sense to implement different algorithms for counting
    // the bits.
    while ((base & 1) == 0) {
        base >>= 1;
        shifts++;
    }
    int bit_size = 0;
    int tmp_base = base;
    while (tmp_base != 0) {
        tmp_base >>= 1;
        bit_size++;
    }
    const int final_size = bit_size * power_exponent;
    // 1 extra bigit for the shifting, and one for rounded final_size.
    Bignum_EnsureCapacity(final_size / Bignum_kBigitSize + 2);

    // Left to Right exponentiation.
    int mask = 1;
    while (power_exponent >= mask) mask <<= 1;

    // The mask is now pointing to the bit above the most significant 1-bit of
    // power_exponent.
    // Get rid of first 1-bit;
    mask >>= 2;
    uint64_t this_value = base;

    bool delayed_multiplication = false;
    const uint64_t max_32bits = 0xFFFFFFFF;
    while (mask != 0 && this_value <= max_32bits) {
        this_value = this_value * this_value;
        // Verify that there is enough space in this_value to perform the
        // multiplication.  The first bit_size bits must be 0.
        if ((power_exponent & mask) != 0) {
            DOUBLE_CONVERSION_ASSERT(bit_size > 0);
            const uint64_t base_bits_mask =
                ~(((uint64_t)(1) << (64 - bit_size)) - 1);
            const bool high_bits_zero = (this_value & base_bits_mask) == 0;
            if (high_bits_zero) {
                this_value *= base;
            } else {
                delayed_multiplication = true;
            }
        }
        mask >>= 1;
    }
    Bignum_AssignUInt64(b, this_value);
    if (delayed_multiplication) {
        Bignum_MultiplyByUInt32(b, base);
    }

    // Now do the same thing as a bignum.
    while (mask != 0) {
        Bignum_Square(b);
        if ((power_exponent & mask) != 0) {
            Bignum_MultiplyByUInt32(b, base);
        }
        mask >>= 1;
    }
    
    // And finally add the saved shifts.
    Bignum_ShiftLeft(b, shifts * power_exponent);
}

static void Bignum_SubtractTimes(Bignum *b, Bignum *other, int factor) {
    DOUBLE_CONVERSION_ASSERT(b->exponent <= other->exponent);
    if (factor < 3) {
        for (int i = 0; i < factor; ++i) {
            Bignum_SubtractBignum(b, other);
        }
        return;
    }
    Bignum_Chunk borrow = 0;
    const int exponent_diff = other->exponent - b->exponent;
    for (int i = 0; i < other->used_bigits; ++i) {
        const Bignum_DoubleChunk product = (Bignum_DoubleChunk)(factor) * *Bignum_RawBigit(other, i);
        const Bignum_DoubleChunk remove = borrow + product;
        const Bignum_Chunk difference = *Bignum_RawBigit(b, i + exponent_diff) - (remove & Bignum_kBigitMask);
        *Bignum_RawBigit(b, i + exponent_diff) = difference & Bignum_kBigitMask;
        borrow = (Bignum_Chunk)((difference >> (Bignum_kChunkSize - 1)) +
                                (remove >> Bignum_kBigitSize));
    }
    for (int i = other->used_bigits + exponent_diff; i < b->used_bigits; ++i) {
        if (borrow == 0) {
            return;
        }
        const Bignum_Chunk difference = *Bignum_RawBigit(b, i) - borrow;
        *Bignum_RawBigit(b, i) = difference & Bignum_kBigitMask;
        borrow = difference >> (Bignum_kChunkSize - 1);
    }
    Bignum_Clamp(b);
}

// Precondition: this/other < 16bit.
static uint16_t Bignum_DivideModuloIntBignum(Bignum *b, Bignum *other) {
    DOUBLE_CONVERSION_ASSERT(Bignum_IsClamped(b));
    DOUBLE_CONVERSION_ASSERT(Bignum_IsClamped(other));
    DOUBLE_CONVERSION_ASSERT(other->used_bigits > 0);
    
    // Easy case: if we have less digits than the divisor than the result is 0.
    // Note: this handles the case where this == 0, too.
    if (Bignum_BigitLength(b) < Bignum_BigitLength(other)) {
        return 0;
    }
    
    Bignum_Align(b, other);
    
    uint16_t result = 0;
    
    // Start by removing multiples of 'other' until both numbers have the same
    // number of digits.
    while (Bignum_BigitLength(b) > Bignum_BigitLength(other)) {
        // This naive approach is extremely inefficient if `this` divided by other
        // is big. This function is implemented for doubleToString where
        // the result should be small (less than 10).
        DOUBLE_CONVERSION_ASSERT(*Bignum_RawBigit(other, other->used_bigits - 1) >= ((1 << Bignum_kBigitSize) / 16));
        DOUBLE_CONVERSION_ASSERT(*Bignum_RawBigit(b, b->used_bigits - 1) < 0x10000);
        // Remove the multiples of the first digit.
        // Example this = 23 and other equals 9. -> Remove 2 multiples.
        result += (uint16_t)(*Bignum_RawBigit(b, b->used_bigits - 1));
        Bignum_SubtractTimes(b, other, *Bignum_RawBigit(b, b->used_bigits - 1));
    }
    
    DOUBLE_CONVERSION_ASSERT(Bignum_BigitLength(b) == Bignum_BigitLength(other));
    
    // Both bignums are at the same length now.
    // Since other has more than 0 digits we know that the access to
    // RawBigit(used_bigits_ - 1) is safe.
    const Bignum_Chunk this_bigit = *Bignum_RawBigit(b, b->used_bigits - 1);
    const Bignum_Chunk other_bigit = *Bignum_RawBigit(other, other->used_bigits - 1);

    if (other->used_bigits == 1) {
        // Shortcut for easy (and common) case.
        int quotient = this_bigit / other_bigit;
        *Bignum_RawBigit(b, b->used_bigits - 1) = this_bigit - other_bigit * quotient;
        DOUBLE_CONVERSION_ASSERT(quotient < 0x10000);
        result += (uint16_t)(quotient);
        Bignum_Clamp(b);
        return result;
    }
    
    const int division_estimate = this_bigit / (other_bigit + 1);
    DOUBLE_CONVERSION_ASSERT(division_estimate < 0x10000);
    result += (uint16_t)(division_estimate);
    Bignum_SubtractTimes(b, other, division_estimate);
    
    if (other_bigit * (division_estimate + 1) > this_bigit) {
        // No need to even try to subtract. Even if other's remaining digits were 0
        // another subtraction would be too much.
        return result;
    }
    
    while (Bignum_LessEqual(other, b)) {
        Bignum_SubtractBignum(b, other);
        result++;
    }
    return result;
}

static int Bignum_ChunkSizeInHexChars(Bignum_Chunk number) {
    DOUBLE_CONVERSION_ASSERT(number > 0);
    int result = 0;
    while (number != 0) {
        number >>= 4;
        result++;
    }
    return result;
}

static char Bignum_HexCharOfValue(const int value) {
    DOUBLE_CONVERSION_ASSERT(0 <= value && value <= 16);
    if (value < 10) {
        return (char)(value + '0');
    }
    return (char)(value - 10 + 'A');
}

static bool Bignum_ToHexString(Bignum *b, char *buffer, int buffer_size) {
    DOUBLE_CONVERSION_ASSERT(Bignum_IsClamped(b));
    // Each bigit must be printable as separate hex-character.
    DOUBLE_CONVERSION_ASSERT(Bignum_kBigitSize % 4 == 0);
    static const int kHexCharsPerBigit = Bignum_kBigitSize / 4;
    
    if (b->used_bigits == 0) {
        if (buffer_size < 2) {
            return false;
        }
        buffer[0] = '0';
        buffer[1] = '\0';
        return true;
    }
    // We add 1 for the terminating '\0' character.
    const int needed_chars = (Bignum_BigitLength(b) - 1) * kHexCharsPerBigit +
        Bignum_ChunkSizeInHexChars(*Bignum_RawBigit(b, b->used_bigits - 1)) + 1;
    if (needed_chars > buffer_size) {
        return false;
    }
    int string_index = needed_chars - 1;
    buffer[string_index--] = '\0';
    for (int i = 0; i < b->exponent; ++i) {
        for (int j = 0; j < kHexCharsPerBigit; ++j) {
            buffer[string_index--] = '0';
        }
    }
    for (int i = 0; i < b->used_bigits - 1; ++i) {
        Bignum_Chunk current_bigit = *Bignum_RawBigit(b, i);
        for (int j = 0; j < kHexCharsPerBigit; ++j) {
            buffer[string_index--] = Bignum_HexCharOfValue(current_bigit & 0xF);
            current_bigit >>= 4;
        }
    }
    // And finally the last bigit.
    Bignum_Chunk most_significant_bigit = *Bignum_RawBigit(b, b->used_bigits - 1);
    while (most_significant_bigit != 0) {
        buffer[string_index--] = Bignum_HexCharOfValue(most_significant_bigit & 0xF);
        most_significant_bigit >>= 4;
    }
    return true;
}



/// ============================================================================
/// bignum-dtoa.h
/// ============================================================================

typedef enum BignumDtoaMode {
    // Return the shortest correct representation.
    // For example the output of 0.299999999999999988897 is (the less accurate but
    // correct) 0.3.
    BIGNUM_DTOA_SHORTEST,
    // Same as BIGNUM_DTOA_SHORTEST but for single-precision floats.
    BIGNUM_DTOA_SHORTEST_SINGLE,
    // Return a fixed number of digits after the decimal point.
    // For instance fixed(0.1, 4) becomes 0.1000
    // If the input number is big, the output will be big.
    BIGNUM_DTOA_FIXED,
    // Return a fixed number of digits, no matter what the exponent is.
    BIGNUM_DTOA_PRECISION
} BignumDtoaMode;

// Converts the given double 'v' to ascii.
// The result should be interpreted as buffer * 10^(point-length).
// The buffer will be null-terminated.
//
// The input v must be > 0 and different from NaN, and Infinity.
//
// The output depends on the given mode:
//  - SHORTEST: produce the least amount of digits for which the internal
//   identity requirement is still satisfied. If the digits are printed
//   (together with the correct exponent) then reading this number will give
//   'v' again. The buffer will choose the representation that is closest to
//   'v'. If there are two at the same distance, than the number is round up.
//   In this mode the 'requested_digits' parameter is ignored.
//  - FIXED: produces digits necessary to print a given number with
//   'requested_digits' digits after the decimal point. The produced digits
//   might be too short in which case the caller has to fill the gaps with '0's.
//   Example: toFixed(0.001, 5) is allowed to return buffer="1", point=-2.
//   Halfway cases are rounded up. The call toFixed(0.15, 2) thus returns
//     buffer="2", point=0.
//   Note: the length of the returned buffer has no meaning wrt the significance
//   of its digits. That is, just because it contains '0's does not mean that
//   any other digit would not satisfy the internal identity requirement.
//  - PRECISION: produces 'requested_digits' where the first digit is not '0'.
//   Even though the length of produced digits usually equals
//   'requested_digits', the function is allowed to return fewer digits, in
//   which case the caller has to fill the missing digits with '0's.
//   Halfway cases are again rounded up.
// 'BignumDtoa' expects the given buffer to be big enough to hold all digits
// and a terminating null-character.
static void BignumDtoa(double v, BignumDtoaMode mode, int requested_digits,
                       Vector *buffer, int *length, int *point);

static int BignumNormalizedExponent(uint64_t significand, int exponent) {
    DOUBLE_CONVERSION_ASSERT(significand != 0);
    while ((significand & Double_kHiddenBit) == 0) {
        significand = significand << 1;
        exponent = exponent - 1;
    }
    return exponent;
}

// Forward declarations:
// Returns an estimation of k such that 10^(k-1) <= v < 10^k.
static int BignumEstimatePower(int exponent);

// Computes v / 10^estimated_power exactly, as a ratio of two bignums, numerator
// and denominator.
static void BignumInitialScaledStartValues(uint64_t significand,
                                           int exponent,
                                           bool lower_boundary_is_closer,
                                           int estimated_power,
                                           bool need_boundary_deltas,
                                           Bignum *numerator,
                                           Bignum *denominator,
                                           Bignum *delta_minus,
                                           Bignum *delta_plus);

// Multiplies numerator/denominator so that its values lies in the range 1-10.
// Returns decimal_point s.t.
//  v = numerator'/denominator' * 10^(decimal_point-1)
//     where numerator' and denominator' are the values of numerator and
//     denominator after the call to this function.
static void BignumFixupMultiply10(int estimated_power, bool is_even,
                                  int *decimal_point,
                                  Bignum *numerator, Bignum *denominator,
                                  Bignum *delta_minus, Bignum *delta_plus);

// Generates digits from the left to the right and stops when the generated
// digits yield the shortest decimal representation of v.
static void BignumGenerateShortestDigits(Bignum *numerator, Bignum *denominator,
                                         Bignum *delta_minus, Bignum *delta_plus,
                                         bool is_even,
                                         Vector *buffer, int *length);

// Generates 'requested_digits' after the decimal point.
static void BignumToFixed(int requested_digits, int *decimal_point,
                          Bignum *numerator, Bignum *denominator,
                          Vector *buffer, int *length);

// Generates 'count' digits of numerator/denominator.
// Once 'count' digits have been produced rounds the result depending on the
// remainder (remainders of exactly .5 round upwards). Might update the
// decimal_point when rounding up (for example for 0.9999).
static void BignumGenerateCountedDigits(int count, int *decimal_point,
                                        Bignum *numerator, Bignum *denominator,
                                        Vector *buffer, int *length);

static void BignumDtoa(double v, BignumDtoaMode mode, int requested_digits,
                       Vector *buffer, int *length, int *decimal_point) {
    DOUBLE_CONVERSION_ASSERT(v > 0);
    Double t = Double_make(v);
    DOUBLE_CONVERSION_ASSERT(!Double_IsSpecial(&t));
    uint64_t significand;
    int exponent;
    bool lower_boundary_is_closer;
    if (mode == BIGNUM_DTOA_SHORTEST_SINGLE) {
        float f = (float)(v);
        DOUBLE_CONVERSION_ASSERT(f == v);
        Single st = Single_make(f);
        significand = Single_Significand(&st);
        exponent = Single_Exponent(&st);
        lower_boundary_is_closer = Single_LowerBoundaryIsCloser(&st);
    } else {
        Double ft = Double_make(v);
        significand = Double_Significand(&ft);
        exponent = Double_Exponent(&ft);
        lower_boundary_is_closer = Double_LowerBoundaryIsCloser(&ft);
    }
    bool need_boundary_deltas =
        (mode == BIGNUM_DTOA_SHORTEST || mode == BIGNUM_DTOA_SHORTEST_SINGLE);
    
    bool is_even = (significand & 1) == 0;
    int normalized_exponent = BignumNormalizedExponent(significand, exponent);
    // estimated_power might be too low by 1.
    int estimated_power = BignumEstimatePower(normalized_exponent);
    
    // Shortcut for Fixed.
    // The requested digits correspond to the digits after the point. If the
    // number is much too small, then there is no need in trying to get any
    // digits.
    if (mode == BIGNUM_DTOA_FIXED && -estimated_power - 1 > requested_digits) {
        buffer->start[0] = '\0';
        *length = 0;
        // Set decimal-point to -requested_digits. This is what Gay does.
        // Note that it should not have any effect anyways since the string is
        // empty.
        *decimal_point = -requested_digits;
        return;
    }
    
    Bignum numerator;
    Bignum denominator;
    Bignum delta_minus;
    Bignum delta_plus;
    // Make sure the bignum can grow large enough. The smallest double equals
    // 4e-324. In this case the denominator needs fewer than 324*4 binary digits.
    // The maximum double is 1.7976931348623157e308 which needs fewer than
    // 308*4 binary digits.
    DOUBLE_CONVERSION_ASSERT(Bignum_kMaxSignificantBits >= 324*4);
    BignumInitialScaledStartValues(significand, exponent, lower_boundary_is_closer,
                                   estimated_power, need_boundary_deltas,
                                   &numerator, &denominator,
                                   &delta_minus, &delta_plus);
    // We now have v = (numerator / denominator) * 10^estimated_power.
    BignumFixupMultiply10(estimated_power, is_even, decimal_point,
                          &numerator, &denominator,
                          &delta_minus, &delta_plus);
    // We now have v = (numerator / denominator) * 10^(decimal_point-1), and
    //  1 <= (numerator + delta_plus) / denominator < 10
    switch (mode) {
        case BIGNUM_DTOA_SHORTEST:
        case BIGNUM_DTOA_SHORTEST_SINGLE:
            BignumGenerateShortestDigits(&numerator, &denominator,
                                         &delta_minus, &delta_plus,
                                         is_even, buffer, length);
            break;
        case BIGNUM_DTOA_FIXED:
            BignumToFixed(requested_digits, decimal_point,
                          &numerator, &denominator,
                          buffer, length);
            break;
        case BIGNUM_DTOA_PRECISION:
            BignumGenerateCountedDigits(requested_digits, decimal_point,
                                        &numerator, &denominator,
                                        buffer, length);
            break;
        default:
            DOUBLE_CONVERSION_UNREACHABLE();
    }
    buffer->start[*length] = '\0';
}

// The procedure starts generating digits from the left to the right and stops
// when the generated digits yield the shortest decimal representation of v. A
// decimal representation of v is a number lying closer to v than to any other
// double, so it converts to v when read.
//
// This is true if d, the decimal representation, is between m- and m+, the
// upper and lower boundaries. d must be strictly between them if !is_even.
//           m- := (numerator - delta_minus) / denominator
//           m+ := (numerator + delta_plus) / denominator
//
// Precondition: 0 <= (numerator+delta_plus) / denominator < 10.
//   If 1 <= (numerator+delta_plus) / denominator < 10 then no leading 0 digit
//   will be produced. This should be the standard precondition.
static void BignumGenerateShortestDigits(Bignum *numerator, Bignum *denominator,
                                         Bignum *delta_minus, Bignum *delta_plus,
                                         bool is_even,
                                         Vector *buffer, int *length) {
    // Small optimization: if delta_minus and delta_plus are the same just reuse
    // one of the two bignums.
    if (Bignum_Equal(delta_minus, delta_plus)) {
        delta_plus = delta_minus;
    }
    *length = 0;
    for (;;) {
        uint16_t digit;
        digit = Bignum_DivideModuloIntBignum(numerator, denominator);
        DOUBLE_CONVERSION_ASSERT(digit <= 9);  // digit is a uint16_t and therefore always positive.
        // digit = numerator / denominator (integer division).
        // numerator = numerator % denominator.
        buffer->start[(*length)++] = (char)(digit + '0');
        
        // Can we stop already?
        // If the remainder of the division is less than the distance to the lower
        // boundary we can stop. In this case we simply round down (discarding the
        // remainder).
        // Similarly we test if we can round up (using the upper boundary).
        bool in_delta_room_minus;
        bool in_delta_room_plus;
        if (is_even) {
            in_delta_room_minus = Bignum_LessEqual(numerator, delta_minus);
        } else {
            in_delta_room_minus = Bignum_Less(numerator, delta_minus);
        }
        if (is_even) {
            in_delta_room_plus =
                Bignum_PlusCompare(numerator, delta_plus, denominator) >= 0;
        } else {
            in_delta_room_plus =
                Bignum_PlusCompare(numerator, delta_plus, denominator) > 0;
        }
        if (!in_delta_room_minus && !in_delta_room_plus) {
            // Prepare for next iteration.
            Bignum_Times10(numerator);
            Bignum_Times10(delta_minus);
            // We optimized delta_plus to be equal to delta_minus (if they share the
            // same value). So don't multiply delta_plus if they point to the same
            // object.
            if (delta_minus != delta_plus) {
                Bignum_Times10(delta_plus);
            }
        } else if (in_delta_room_minus && in_delta_room_plus) {
            // Let's see if 2*numerator < denominator.
            // If yes, then the next digit would be < 5 and we can round down.
            int compare = Bignum_PlusCompare(numerator, numerator, denominator);
            if (compare < 0) {
                // Remaining digits are less than .5. -> Round down (== do nothing).
            } else if (compare > 0) {
                // Remaining digits are more than .5 of denominator. -> Round up.
                // Note that the last digit could not be a '9' as otherwise the whole
                // loop would have stopped earlier.
                // We still have an assert here in case the preconditions were not
                // satisfied.
                DOUBLE_CONVERSION_ASSERT(buffer->start[(*length) - 1] != '9');
                buffer->start[(*length) - 1]++;
            } else {
                // Halfway case.
                // TODO(floitsch): need a way to solve half-way cases.
                //   For now let's round towards even (since this is what Gay seems to
                //   do).

                if ((buffer->start[(*length) - 1] - '0') % 2 == 0) {
                    // Round down => Do nothing.
                } else {
                    DOUBLE_CONVERSION_ASSERT(buffer->start[(*length) - 1] != '9');
                    buffer->start[(*length) - 1]++;
                }
            }
            return;
        } else if (in_delta_room_minus) {
            // Round down (== do nothing).
            return;
        } else {  // in_delta_room_plus
            // Round up.
            // Note again that the last digit could not be '9' since this would have
            // stopped the loop earlier.
            // We still have an DOUBLE_CONVERSION_ASSERT here, in case the preconditions were not
            // satisfied.
            DOUBLE_CONVERSION_ASSERT(buffer->start[(*length) -1] != '9');
            buffer->start[(*length) - 1]++;
            return;
        }
    }
}

// Let v = numerator / denominator < 10.
// Then we generate 'count' digits of d = x.xxxxx... (without the decimal point)
// from left to right. Once 'count' digits have been produced we decide whether
// to round up or down. Remainders of exactly .5 round upwards. Numbers such
// as 9.999999 propagate a carry all the way, and change the
// exponent (decimal_point), when rounding upwards.
static void BignumGenerateCountedDigits(int count, int *decimal_point,
                                        Bignum *numerator, Bignum *denominator,
                                        Vector *buffer, int *length) {
    DOUBLE_CONVERSION_ASSERT(count >= 0);
    for (int i = 0; i < count - 1; ++i) {
        uint16_t digit;
        digit = Bignum_DivideModuloIntBignum(numerator, denominator);
        DOUBLE_CONVERSION_ASSERT(digit <= 9);  // digit is a uint16_t and therefore always  positive.
        // digit = numerator / denominator (integer division).
        // numerator = numerator % denominator.
        buffer->start[i] = (char)(digit + '0');
        // Prepare for next iteration.
        Bignum_Times10(numerator);
    }
    // Generate the last digit.
    uint16_t digit;
    digit = Bignum_DivideModuloIntBignum(numerator, denominator);
    if (Bignum_PlusCompare(numerator, numerator, denominator) >= 0) {
        digit++;
    }
    DOUBLE_CONVERSION_ASSERT(digit <= 10);
    buffer->start[count - 1] = (char)(digit + '0');
    // Correct bad digits (in case we had a sequence of '9's). Propagate the
    // carry until we hat a non-'9' or til we reach the first digit.
    for (int i = count - 1; i > 0; --i) {
        if (buffer->start[i] != '0' + 10) break;
        buffer->start[i] = '0';
        buffer->start[i - 1]++;
    }
    if (buffer->start[0] == '0' + 10) {
        // Propagate a carry past the top place.
        buffer->start[0] = '1';
        (*decimal_point)++;
    }
    *length = count;
}

// Generates 'requested_digits' after the decimal point. It might omit
// trailing '0's. If the input number is too small then no digits at all are
// generated (ex.: 2 fixed digits for 0.00001).
//
// Input verifies:  1 <= (numerator + delta) / denominator < 10.
static void BignumToFixed(int requested_digits, int *decimal_point,
                          Bignum *numerator, Bignum *denominator,
                          Vector *buffer, int *length) {
    // Note that we have to look at more than just the requested_digits, since
    // a number could be rounded up. Example: v=0.5 with requested_digits=0.
    // Even though the power of v equals 0 we can't just stop here.
    if (-(*decimal_point) > requested_digits) {
        // The number is definitively too small.
        // Ex: 0.001 with requested_digits == 1.
        // Set decimal-point to -requested_digits. This is what Gay does.
        // Note that it should not have any effect anyways since the string is
        // empty.
        *decimal_point = -requested_digits;
        *length = 0;
        return;
    } else if (-(*decimal_point) == requested_digits) {
        // We only need to verify if the number rounds down or up.
        // Ex: 0.04 and 0.06 with requested_digits == 1.
        DOUBLE_CONVERSION_ASSERT(*decimal_point == -requested_digits);
        // Initially the fraction lies in range (1, 10]. Multiply the denominator
        // by 10 so that we can compare more easily.
        Bignum_Times10(denominator);
        if (Bignum_PlusCompare(numerator, numerator, denominator) >= 0) {
            // If the fraction is >= 0.5 then we have to include the rounded
            // digit.
            buffer->start[0] = '1';
            *length = 1;
            (*decimal_point)++;
        } else {
            // Note that we caught most of similar cases earlier.
            *length = 0;
        }
        return;
    } else {
        // The requested digits correspond to the digits after the point.
        // The variable 'needed_digits' includes the digits before the point.
        int needed_digits = (*decimal_point) + requested_digits;
        BignumGenerateCountedDigits(needed_digits, decimal_point,
                              numerator, denominator,
                              buffer, length);
    }
}

// Returns an estimation of k such that 10^(k-1) <= v < 10^k where
// v = f * 2^exponent and 2^52 <= f < 2^53.
// v is hence a normalized double with the given exponent. The output is an
// approximation for the exponent of the decimal approximation .digits * 10^k.
//
// The result might undershoot by 1 in which case 10^k <= v < 10^k+1.
// Note: this property holds for v's upper boundary m+ too.
//    10^k <= m+ < 10^k+1.
//   (see explanation below).
//
// Examples:
//  EstimatePower(0)   => 16
//  EstimatePower(-52) => 0
//
// Note: e >= 0 => EstimatedPower(e) > 0. No similar claim can be made for e<0.
static int BignumEstimatePower(int exponent) {
    // This function estimates log10 of v where v = f*2^e (with e == exponent).
    // Note that 10^floor(log10(v)) <= v, but v <= 10^ceil(log10(v)).
    // Note that f is bounded by its container size. Let p = 53 (the double's
    // significand size). Then 2^(p-1) <= f < 2^p.
    //
    // Given that log10(v) == log2(v)/log2(10) and e+(len(f)-1) is quite close
    // to log2(v) the function is simplified to (e+(len(f)-1)/log2(10)).
    // The computed number undershoots by less than 0.631 (when we compute log3
    // and not log10).
    //
    // Optimization: since we only need an approximated result this computation
    // can be performed on 64 bit integers. On x86/x64 architecture the speedup is
    // not really measurable, though.
    //
    // Since we want to avoid overshooting we decrement by 1e10 so that
    // floating-point imprecisions don't affect us.
    //
    // Explanation for v's boundary m+: the computation takes advantage of
    // the fact that 2^(p-1) <= f < 2^p. Boundaries still satisfy this requirement
    // (even for denormals where the delta can be much more important).
    
    const double k1Log10 = 0.30102999566398114;  // 1/lg(10)
    
    // For doubles len(f) == 53 (don't forget the hidden bit).
    const int kSignificandSize = Double_kSignificandSize;
    double estimate = fp_ceil((exponent + kSignificandSize - 1) * k1Log10 - 1e-10);
    return (int)(estimate);
}

// See comments for InitialScaledStartValues.
static void BignumInitialScaledStartValuesPositiveExponent(
    uint64_t significand, int exponent,
    int estimated_power, bool need_boundary_deltas,
    Bignum *numerator, Bignum *denominator,
    Bignum *delta_minus, Bignum *delta_plus) {
    
    // A positive exponent implies a positive power.
    DOUBLE_CONVERSION_ASSERT(estimated_power >= 0);
    // Since the estimated_power is positive we simply multiply the denominator
    // by 10^estimated_power.
    
    // numerator = v.
    Bignum_AssignUInt64(numerator, significand);
    Bignum_ShiftLeft(numerator, exponent);
    // denominator = 10^estimated_power.
    Bignum_AssignPowerUInt16(denominator, 10, estimated_power);
    
    if (need_boundary_deltas) {
        // Introduce a common denominator so that the deltas to the boundaries are
        // integers.
        Bignum_ShiftLeft(denominator, 1);
        Bignum_ShiftLeft(numerator, 1);
        // Let v = f * 2^e, then m+ - v = 1/2 * 2^e; With the common
        // denominator (of 2) delta_plus equals 2^e.
        Bignum_AssignUInt16(delta_plus, 1);
        Bignum_ShiftLeft(delta_plus, exponent);
        // Same for delta_minus. The adjustments if f == 2^p-1 are done later.
        Bignum_AssignUInt16(delta_minus, 1);
        Bignum_ShiftLeft(delta_minus, exponent);
    }
}

// See comments for InitialScaledStartValues
static void BignumInitialScaledStartValuesNegativeExponentPositivePower(
    uint64_t significand, int exponent,
    int estimated_power, bool need_boundary_deltas,
    Bignum *numerator, Bignum *denominator,
    Bignum *delta_minus, Bignum *delta_plus) {
    
    // v = f * 2^e with e < 0, and with estimated_power >= 0.
    // This means that e is close to 0 (have a look at how estimated_power is
    // computed).
    
    // numerator = significand
    //  since v = significand * 2^exponent this is equivalent to
    //  numerator = v * / 2^-exponent
    Bignum_AssignUInt64(numerator, significand);
    // denominator = 10^estimated_power * 2^-exponent (with exponent < 0)
    Bignum_AssignPowerUInt16(denominator, 10, estimated_power);
    Bignum_ShiftLeft(denominator, -exponent);
    
    if (need_boundary_deltas) {
        // Introduce a common denominator so that the deltas to the boundaries are
        // integers.
        Bignum_ShiftLeft(denominator, 1);
        Bignum_ShiftLeft(numerator, 1);
        // Let v = f * 2^e, then m+ - v = 1/2 * 2^e; With the common
        // denominator (of 2) delta_plus equals 2^e.
        // Given that the denominator already includes v's exponent the distance
        // to the boundaries is simply 1.
        Bignum_AssignUInt16(delta_plus, 1);
        // Same for delta_minus. The adjustments if f == 2^p-1 are done later.
        Bignum_AssignUInt16(delta_minus, 1);
    }
}

// See comments for InitialScaledStartValues
static void BignumInitialScaledStartValuesNegativeExponentNegativePower(
    uint64_t significand, int exponent,
    int estimated_power, bool need_boundary_deltas,
    Bignum *numerator, Bignum *denominator,
    Bignum *delta_minus, Bignum *delta_plus) {
    
    // Instead of multiplying the denominator with 10^estimated_power we
    // multiply all values (numerator and deltas) by 10^-estimated_power.
    
    // Use numerator as temporary container for power_ten.
    Bignum *power_ten = numerator;
    Bignum_AssignPowerUInt16(power_ten, 10, -estimated_power);
    
    if (need_boundary_deltas) {
        // Since power_ten == numerator we must make a copy of 10^estimated_power
        // before we complete the computation of the numerator.
        // delta_plus = delta_minus = 10^estimated_power
        Bignum_AssignBignum(delta_plus, power_ten);
        Bignum_AssignBignum(delta_minus, power_ten);
    }
    
    // numerator = significand * 2 * 10^-estimated_power
    //  since v = significand * 2^exponent this is equivalent to
    // numerator = v * 10^-estimated_power * 2 * 2^-exponent.
    // Remember: numerator has been abused as power_ten. So no need to assign it
    //  to itself.
    DOUBLE_CONVERSION_ASSERT(numerator == power_ten);
    Bignum_MultiplyByUInt64(numerator, significand);
    
    // denominator = 2 * 2^-exponent with exponent < 0.
    Bignum_AssignUInt16(denominator, 1);
    Bignum_ShiftLeft(denominator, -exponent);
    
    if (need_boundary_deltas) {
        // Introduce a common denominator so that the deltas to the boundaries are
        // integers.
        Bignum_ShiftLeft(numerator, 1);
        Bignum_ShiftLeft(denominator, 1);
        // With this shift the boundaries have their correct value, since
        // delta_plus = 10^-estimated_power, and
        // delta_minus = 10^-estimated_power.
        // These assignments have been done earlier.
        // The adjustments if f == 2^p-1 (lower boundary is closer) are done later.
    }
}

// Let v = significand * 2^exponent.
// Computes v / 10^estimated_power exactly, as a ratio of two bignums, numerator
// and denominator. The functions GenerateShortestDigits and
// GenerateCountedDigits will then convert this ratio to its decimal
// representation d, with the required accuracy.
// Then d * 10^estimated_power is the representation of v.
// (Note: the fraction and the estimated_power might get adjusted before
// generating the decimal representation.)
//
// The initial start values consist of:
//  - a scaled numerator: s.t. numerator/denominator == v / 10^estimated_power.
//  - a scaled (common) denominator.
//  optionally (used by GenerateShortestDigits to decide if it has the shortest
//  decimal converting back to v):
//  - v - m-: the distance to the lower boundary.
//  - m+ - v: the distance to the upper boundary.
//
// v, m+, m-, and therefore v - m- and m+ - v all share the same denominator.
//
// Let ep == estimated_power, then the returned values will satisfy:
//  v / 10^ep = numerator / denominator.
//  v's boundaries m- and m+:
//    m- / 10^ep == v / 10^ep - delta_minus / denominator
//    m+ / 10^ep == v / 10^ep + delta_plus / denominator
//  Or in other words:
//    m- == v - delta_minus * 10^ep / denominator;
//    m+ == v + delta_plus * 10^ep / denominator;
//
// Since 10^(k-1) <= v < 10^k    (with k == estimated_power)
//  or       10^k <= v < 10^(k+1)
//  we then have 0.1 <= numerator/denominator < 1
//           or    1 <= numerator/denominator < 10
//
// It is then easy to kickstart the digit-generation routine.
//
// The boundary-deltas are only filled if the mode equals BIGNUM_DTOA_SHORTEST
// or BIGNUM_DTOA_SHORTEST_SINGLE.
static void BignumInitialScaledStartValues(uint64_t significand,
                                           int exponent,
                                           bool lower_boundary_is_closer,
                                           int estimated_power,
                                           bool need_boundary_deltas,
                                           Bignum *numerator,
                                           Bignum *denominator,
                                           Bignum *delta_minus,
                                           Bignum *delta_plus) {
    if (exponent >= 0) {
        BignumInitialScaledStartValuesPositiveExponent(
            significand, exponent, estimated_power, need_boundary_deltas,
            numerator, denominator, delta_minus, delta_plus);
    } else if (estimated_power >= 0) {
        BignumInitialScaledStartValuesNegativeExponentPositivePower(
            significand, exponent, estimated_power, need_boundary_deltas,
            numerator, denominator, delta_minus, delta_plus);
    } else {
        BignumInitialScaledStartValuesNegativeExponentNegativePower(
            significand, exponent, estimated_power, need_boundary_deltas,
            numerator, denominator, delta_minus, delta_plus);
    }
    
    if (need_boundary_deltas && lower_boundary_is_closer) {
        // The lower boundary is closer at half the distance of "normal" numbers.
        // Increase the common denominator and adapt all but the delta_minus.
        Bignum_ShiftLeft(denominator, 1);  // *2
        Bignum_ShiftLeft(numerator, 1);    // *2
        Bignum_ShiftLeft(delta_plus, 1);   // *2
    }
}

// This routine multiplies numerator/denominator so that its values lies in the
// range 1-10. That is after a call to this function we have:
//    1 <= (numerator + delta_plus) /denominator < 10.
// Let numerator the input before modification and numerator' the argument
// after modification, then the output-parameter decimal_point is such that
//  numerator / denominator * 10^estimated_power ==
//    numerator' / denominator' * 10^(decimal_point - 1)
// In some cases estimated_power was too low, and this is already the case. We
// then simply adjust the power so that 10^(k-1) <= v < 10^k (with k ==
// estimated_power) but do not touch the numerator or denominator.
// Otherwise the routine multiplies the numerator and the deltas by 10.
static void BignumFixupMultiply10(int estimated_power, bool is_even,
                                  int *decimal_point,
                                  Bignum *numerator, Bignum *denominator,
                                  Bignum *delta_minus, Bignum *delta_plus) {
    bool in_range;
    if (is_even) {
        // For IEEE doubles half-way cases (in decimal system numbers ending with 5)
        // are rounded to the closest floating-point number with even significand.
        in_range = Bignum_PlusCompare(numerator, delta_plus, denominator) >= 0;
    } else {
        in_range = Bignum_PlusCompare(numerator, delta_plus, denominator) > 0;
    }
    if (in_range) {
        // Since numerator + delta_plus >= denominator we already have
        // 1 <= numerator/denominator < 10. Simply update the estimated_power.
        *decimal_point = estimated_power + 1;
    } else {
        *decimal_point = estimated_power;
        Bignum_Times10(numerator);
        if (Bignum_Equal(delta_minus, delta_plus)) {
            Bignum_Times10(delta_minus);
            Bignum_AssignBignum(delta_plus, delta_minus);
        } else {
            Bignum_Times10(delta_minus);
            Bignum_Times10(delta_plus);
        }
    }
}



/// ============================================================================
/// cached-powers.h
/// ============================================================================

// Not all powers of ten are cached. The decimal exponent of two neighboring
// cached numbers will differ by kDecimalExponentDistance.
static const int Cache_kDecimalExponentDistance = 8;
static const int Cache_kMinDecimalExponent = -348;
static const int Cache_kMaxDecimalExponent = 340;

// Returns a cached power-of-ten with a binary exponent in the range
// [min_exponent; max_exponent] (boundaries included).
static void Cache_GetCachedPowerForBinaryExponentRange(int min_exponent,
                                                       int max_exponent,
                                                       DiyFp *power,
                                                       int *decimal_exponent);

// Returns a cached power of ten x ~= 10^k such that
//   k <= decimal_exponent < k + kCachedPowersDecimalDistance.
// The given decimal_exponent must satisfy
//   kMinDecimalExponent <= requested_exponent, and
//   requested_exponent < kMaxDecimalExponent + kDecimalExponentDistance.
static void Cache_GetCachedPowerForDecimalExponent(int requested_exponent,
                                                   DiyFp *power,
                                                   int *found_exponent);



/// ============================================================================
/// cached-powers.cc
/// ============================================================================

typedef struct CachedPower {
    uint64_t significand;
    int16_t binary_exponent;
    int16_t decimal_exponent;
} CachedPower;

static const CachedPower kCachedPowers[] = {
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xfa8fd5a0, 081c0288), -1220, -348},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xbaaee17f, a23ebf76), -1193, -340},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0x8b16fb20, 3055ac76), -1166, -332},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xcf42894a, 5dce35ea), -1140, -324},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0x9a6bb0aa, 55653b2d), -1113, -316},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xe61acf03, 3d1a45df), -1087, -308},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xab70fe17, c79ac6ca), -1060, -300},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xff77b1fc, bebcdc4f), -1034, -292},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xbe5691ef, 416bd60c), -1007, -284},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0x8dd01fad, 907ffc3c), -980, -276},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xd3515c28, 31559a83), -954, -268},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0x9d71ac8f, ada6c9b5), -927, -260},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xea9c2277, 23ee8bcb), -901, -252},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xaecc4991, 4078536d), -874, -244},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0x823c1279, 5db6ce57), -847, -236},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xc2109436, 4dfb5637), -821, -228},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0x9096ea6f, 3848984f), -794, -220},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xd77485cb, 25823ac7), -768, -212},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xa086cfcd, 97bf97f4), -741, -204},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xef340a98, 172aace5), -715, -196},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xb23867fb, 2a35b28e), -688, -188},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0x84c8d4df, d2c63f3b), -661, -180},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xc5dd4427, 1ad3cdba), -635, -172},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0x936b9fce, bb25c996), -608, -164},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xdbac6c24, 7d62a584), -582, -156},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xa3ab6658, 0d5fdaf6), -555, -148},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xf3e2f893, dec3f126), -529, -140},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xb5b5ada8, aaff80b8), -502, -132},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0x87625f05, 6c7c4a8b), -475, -124},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xc9bcff60, 34c13053), -449, -116},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0x964e858c, 91ba2655), -422, -108},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xdff97724, 70297ebd), -396, -100},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xa6dfbd9f, b8e5b88f), -369, -92},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xf8a95fcf, 88747d94), -343, -84},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xb9447093, 8fa89bcf), -316, -76},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0x8a08f0f8, bf0f156b), -289, -68},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xcdb02555, 653131b6), -263, -60},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0x993fe2c6, d07b7fac), -236, -52},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xe45c10c4, 2a2b3b06), -210, -44},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xaa242499, 697392d3), -183, -36},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xfd87b5f2, 8300ca0e), -157, -28},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xbce50864, 92111aeb), -130, -20},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0x8cbccc09, 6f5088cc), -103, -12},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xd1b71758, e219652c), -77, -4},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0x9c400000, 00000000), -50, 4},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xe8d4a510, 00000000), -24, 12},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xad78ebc5, ac620000), 3, 20},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0x813f3978, f8940984), 30, 28},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xc097ce7b, c90715b3), 56, 36},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0x8f7e32ce, 7bea5c70), 83, 44},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xd5d238a4, abe98068), 109, 52},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0x9f4f2726, 179a2245), 136, 60},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xed63a231, d4c4fb27), 162, 68},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xb0de6538, 8cc8ada8), 189, 76},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0x83c7088e, 1aab65db), 216, 84},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xc45d1df9, 42711d9a), 242, 92},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0x924d692c, a61be758), 269, 100},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xda01ee64, 1a708dea), 295, 108},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xa26da399, 9aef774a), 322, 116},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xf209787b, b47d6b85), 348, 124},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xb454e4a1, 79dd1877), 375, 132},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0x865b8692, 5b9bc5c2), 402, 140},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xc83553c5, c8965d3d), 428, 148},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0x952ab45c, fa97a0b3), 455, 156},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xde469fbd, 99a05fe3), 481, 164},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xa59bc234, db398c25), 508, 172},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xf6c69a72, a3989f5c), 534, 180},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xb7dcbf53, 54e9bece), 561, 188},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0x88fcf317, f22241e2), 588, 196},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xcc20ce9b, d35c78a5), 614, 204},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0x98165af3, 7b2153df), 641, 212},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xe2a0b5dc, 971f303a), 667, 220},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xa8d9d153, 5ce3b396), 694, 228},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xfb9b7cd9, a4a7443c), 720, 236},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xbb764c4c, a7a44410), 747, 244},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0x8bab8eef, b6409c1a), 774, 252},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xd01fef10, a657842c), 800, 260},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0x9b10a4e5, e9913129), 827, 268},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xe7109bfb, a19c0c9d), 853, 276},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xac2820d9, 623bf429), 880, 284},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0x80444b5e, 7aa7cf85), 907, 292},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xbf21e440, 03acdd2d), 933, 300},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0x8e679c2f, 5e44ff8f), 960, 308},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xd433179d, 9c8cb841), 986, 316},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0x9e19db92, b4e31ba9), 1013, 324},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xeb96bf6e, badf77d9), 1039, 332},
    {DOUBLE_CONVERSION_UINT64_2PART_C(0xaf87023b, 9bf0ee6b), 1066, 340},
};

static const int Cache_kCachedPowersOffset = 348;  // -1 * the first decimal_exponent.
static const double Cache_kD_1_LOG2_10 = 0.30102999566398114;  //  1 / lg(10)

static void Cache_GetCachedPowerForBinaryExponentRange(int min_exponent,
                                                       int max_exponent,
                                                       DiyFp *power,
                                                       int *decimal_exponent) {
    int kQ = DiyFp_kSignificandSize;
    double k = fp_ceil((min_exponent + kQ - 1) * Cache_kD_1_LOG2_10);
    int foo = Cache_kCachedPowersOffset;
    int index = (foo + (int)(k) - 1) / Cache_kDecimalExponentDistance + 1;
    DOUBLE_CONVERSION_ASSERT(0 <= index && index < (int)(DOUBLE_CONVERSION_ARRAY_SIZE(kCachedPowers)));
    CachedPower cached_power = kCachedPowers[index];
    DOUBLE_CONVERSION_ASSERT(min_exponent <= cached_power.binary_exponent);
    (void) max_exponent;  // Mark variable as used.
    DOUBLE_CONVERSION_ASSERT(cached_power.binary_exponent <= max_exponent);
    *decimal_exponent = cached_power.decimal_exponent;
    *power = DiyFp_make(cached_power.significand, cached_power.binary_exponent);
}

static void Cache_GetCachedPowerForDecimalExponent(int requested_exponent,
                                                   DiyFp *power,
                                                   int *found_exponent) {
    DOUBLE_CONVERSION_ASSERT(Cache_kMinDecimalExponent <= requested_exponent);
    DOUBLE_CONVERSION_ASSERT(requested_exponent < Cache_kMaxDecimalExponent + Cache_kDecimalExponentDistance);
    int index =
        (requested_exponent + Cache_kCachedPowersOffset) / Cache_kDecimalExponentDistance;
    CachedPower cached_power = kCachedPowers[index];
    *power = DiyFp_make(cached_power.significand, cached_power.binary_exponent);
    *found_exponent = cached_power.decimal_exponent;
    DOUBLE_CONVERSION_ASSERT(*found_exponent <= requested_exponent);
    DOUBLE_CONVERSION_ASSERT(requested_exponent < *found_exponent + Cache_kDecimalExponentDistance);
}



/// ============================================================================
/// fast-dtoa.h
/// ============================================================================

typedef enum FastDtoaMode {
    // Computes the shortest representation of the given input. The returned
    // result will be the most accurate number of this length. Longer
    // representations might be more accurate.
    FAST_DTOA_SHORTEST,
    // Same as FAST_DTOA_SHORTEST but for single-precision floats.
    FAST_DTOA_SHORTEST_SINGLE,
    // Computes a representation where the precision (number of digits) is
    // given as input. The precision is independent of the decimal point.
    FAST_DTOA_PRECISION
} FastDtoaMode;

// FastDtoa will produce at most kFastDtoaMaximalLength digits. This does not
// include the terminating '\0' character.
static const int kFastDtoaMaximalLength = 17;
// Same for single-precision numbers.
static const int kFastDtoaMaximalSingleLength = 9;

// Provides a decimal representation of v.
// The result should be interpreted as buffer * 10^(point - length).
//
// Precondition:
//   * v must be a strictly positive finite double.
//
// Returns true if it succeeds, otherwise the result can not be trusted.
// There will be *length digits inside the buffer followed by a null terminator.
// If the function returns true and mode equals
//   - FAST_DTOA_SHORTEST, then
//     the parameter requested_digits is ignored.
//     The result satisfies
//         v == (double) (buffer * 10^(point - length)).
//     The digits in the buffer are the shortest representation possible. E.g.
//     if 0.099999999999 and 0.1 represent the same double then "1" is returned
//     with point = 0.
//     The last digit will be closest to the actual v. That is, even if several
//     digits might correctly yield 'v' when read again, the buffer will contain
//     the one closest to v.
//   - FAST_DTOA_PRECISION, then
//     the buffer contains requested_digits digits.
//     the difference v - (buffer * 10^(point-length)) is closest to zero for
//     all possible representations of requested_digits digits.
//     If there are two values that are equally close, then FastDtoa returns
//     false.
// For both modes the buffer must be large enough to hold the result.
static bool FastDtoa(double d,
                     FastDtoaMode mode,
                     int requested_digits,
                     Vector *buffer,
                     int *length,
                     int *decimal_point);



/// ============================================================================
/// fast-dtoa.cc
/// ============================================================================

// The minimal and maximal target exponent define the range of w's binary
// exponent, where 'w' is the result of multiplying the input by a cached power
// of ten.
//
// A different range might be chosen on a different platform, to optimize digit
// generation, but a smaller range requires more powers of ten to be cached.
static const int FastDtoa_kMinimalTargetExponent = -60;
static const int FastDtoa_kMaximalTargetExponent = -32;

// Adjusts the last digit of the generated number, and screens out generated
// solutions that may be inaccurate. A solution may be inaccurate if it is
// outside the safe interval, or if we cannot prove that it is closer to the
// input than a neighboring representation of the same length.
//
// Input: * buffer containing the digits of too_high / 10^kappa
//        * the buffer's length
//        * distance_too_high_w == (too_high - w).f() * unit
//        * unsafe_interval == (too_high - too_low).f() * unit
//        * rest = (too_high - buffer * 10^kappa).f() * unit
//        * ten_kappa = 10^kappa * unit
//        * unit = the common multiplier
// Output: returns true if the buffer is guaranteed to contain the closest
//    representable number to the input.
//  Modifies the generated digits in the buffer to approach (round towards) w.
static bool FastDtoa_RoundWeed(Vector *buffer,
                               int length,
                               uint64_t distance_too_high_w,
                               uint64_t unsafe_interval,
                               uint64_t rest,
                               uint64_t ten_kappa,
                               uint64_t unit) {
    uint64_t small_distance = distance_too_high_w - unit;
    uint64_t big_distance = distance_too_high_w + unit;
    // Let w_low  = too_high - big_distance, and
    //     w_high = too_high - small_distance.
    // Note: w_low < w < w_high
    //
    // The real w (* unit) must lie somewhere inside the interval
    // ]w_low; w_high[ (often written as "(w_low; w_high)")
    
    // Basically the buffer currently contains a number in the unsafe interval
    // ]too_low; too_high[ with too_low < w < too_high
    //
    //  too_high - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    //                     ^v 1 unit            ^      ^                 ^      ^
    //  boundary_high ---------------------     .      .                 .      .
    //                     ^v 1 unit            .      .                 .      .
    //   - - - - - - - - - - - - - - - - - - -  +  - - + - - - - - -     .      .
    //                                          .      .         ^       .      .
    //                                          .  big_distance  .       .      .
    //                                          .      .         .       .    rest
    //                              small_distance     .         .       .      .
    //                                          v      .         .       .      .
    //  w_high - - - - - - - - - - - - - - - - - -     .         .       .      .
    //                     ^v 1 unit                   .         .       .      .
    //  w ----------------------------------------     .         .       .      .
    //                     ^v 1 unit                   v         .       .      .
    //  w_low  - - - - - - - - - - - - - - - - - - - - -         .       .      .
    //                                                           .       .      v
    //  buffer --------------------------------------------------+-------+--------
    //                                                           .       .
    //                                                  safe_interval    .
    //                                                           v       .
    //   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -     .
    //                     ^v 1 unit                                     .
    //  boundary_low -------------------------                     unsafe_interval
    //                     ^v 1 unit                                     v
    //  too_low  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    //
    //
    // Note that the value of buffer could lie anywhere inside the range too_low
    // to too_high.
    //
    // boundary_low, boundary_high and w are approximations of the real boundaries
    // and v (the input number). They are guaranteed to be precise up to one unit.
    // In fact the error is guaranteed to be strictly less than one unit.
    //
    // Anything that lies outside the unsafe interval is guaranteed not to round
    // to v when read again.
    // Anything that lies inside the safe interval is guaranteed to round to v
    // when read again.
    // If the number inside the buffer lies inside the unsafe interval but not
    // inside the safe interval then we simply do not know and bail out (returning
    // false).
    //
    // Similarly we have to take into account the imprecision of 'w' when finding
    // the closest representation of 'w'. If we have two potential
    // representations, and one is closer to both w_low and w_high, then we know
    // it is closer to the actual value v.
    //
    // By generating the digits of too_high we got the largest (closest to
    // too_high) buffer that is still in the unsafe interval. In the case where
    // w_high < buffer < too_high we try to decrement the buffer.
    // This way the buffer approaches (rounds towards) w.
    // There are 3 conditions that stop the decrementation process:
    //   1) the buffer is already below w_high
    //   2) decrementing the buffer would make it leave the unsafe interval
    //   3) decrementing the buffer would yield a number below w_high and farther
    //      away than the current number. In other words:
    //              (buffer{-1} < w_high) && w_high - buffer{-1} > buffer - w_high
    // Instead of using the buffer directly we use its distance to too_high.
    // Conceptually rest ~= too_high - buffer
    // We need to do the following tests in this order to avoid over- and
    // underflows.
    DOUBLE_CONVERSION_ASSERT(rest <= unsafe_interval);
    while (rest < small_distance &&  // Negated condition 1
           unsafe_interval - rest >= ten_kappa &&  // Negated condition 2
           (rest + ten_kappa < small_distance ||  // buffer{-1} > w_high
            small_distance - rest >= rest + ten_kappa - small_distance)) {
        buffer->start[length - 1]--;
        rest += ten_kappa;
    }
    
    // We have approached w+ as much as possible. We now test if approaching w-
    // would require changing the buffer. If yes, then we have two possible
    // representations close to w, but we cannot decide which one is closer.
    if (rest < big_distance &&
        unsafe_interval - rest >= ten_kappa &&
        (rest + ten_kappa < big_distance ||
         big_distance - rest > rest + ten_kappa - big_distance)) {
        return false;
    }
    
    // Weeding test.
    //   The safe interval is [too_low + 2 ulp; too_high - 2 ulp]
    //   Since too_low = too_high - unsafe_interval this is equivalent to
    //      [too_high - unsafe_interval + 4 ulp; too_high - 2 ulp]
    //   Conceptually we have: rest ~= too_high - buffer
    return (2 * unit <= rest) && (rest <= unsafe_interval - 4 * unit);
}

// Rounds the buffer upwards if the result is closer to v by possibly adding
// 1 to the buffer. If the precision of the calculation is not sufficient to
// round correctly, return false.
// The rounding might shift the whole buffer in which case the kappa is
// adjusted. For example "99", kappa = 3 might become "10", kappa = 4.
//
// If 2*rest > ten_kappa then the buffer needs to be round up.
// rest can have an error of +/- 1 unit. This function accounts for the
// imprecision and returns false, if the rounding direction cannot be
// unambiguously determined.
//
// Precondition: rest < ten_kappa.
static bool FastDtoa_RoundWeedCounted(Vector *buffer,
                                      int length,
                                      uint64_t rest,
                                      uint64_t ten_kappa,
                                      uint64_t unit,
                                      int *kappa) {
    DOUBLE_CONVERSION_ASSERT(rest < ten_kappa);
    // The following tests are done in a specific order to avoid overflows. They
    // will work correctly with any uint64 values of rest < ten_kappa and unit.
    //
    // If the unit is too big, then we don't know which way to round. For example
    // a unit of 50 means that the real number lies within rest +/- 50. If
    // 10^kappa == 40 then there is no way to tell which way to round.
    if (unit >= ten_kappa) return false;
    // Even if unit is just half the size of 10^kappa we are already completely
    // lost. (And after the previous test we know that the expression will not
    // over/underflow.)
    if (ten_kappa - unit <= unit) return false;
    // If 2 * (rest + unit) <= 10^kappa we can safely round down.
    if ((ten_kappa - rest > rest) && (ten_kappa - 2 * rest >= 2 * unit)) {
        return true;
    }
    // If 2 * (rest - unit) >= 10^kappa, then we can safely round up.
    if ((rest > unit) && (ten_kappa - (rest - unit) <= (rest - unit))) {
        // Increment the last digit recursively until we find a non '9' digit.
        buffer->start[length - 1]++;
        for (int i = length - 1; i > 0; --i) {
            if (buffer->start[i] != '0' + 10) break;
            buffer->start[i] = '0';
            buffer->start[i - 1]++;
        }
        // If the first digit is now '0'+ 10 we had a buffer with all '9's. With the
        // exception of the first digit all digits are now '0'. Simply switch the
        // first digit to '1' and adjust the kappa. Example: "99" becomes "10" and
        // the power (the kappa) is increased.
        if (buffer->start[0] == '0' + 10) {
            buffer->start[0] = '1';
            (*kappa) += 1;
        }
        return true;
    }
    return false;
}

// Returns the biggest power of ten that is less than or equal to the given
// number. We furthermore receive the maximum number of bits 'number' has.
//
// Returns power == 10^(exponent_plus_one-1) such that
//    power <= number < power * 10.
// If number_bits == 0 then 0^(0-1) is returned.
// The number of bits must be <= 32.
// Precondition: number < (1 << (number_bits + 1)).

// Inspired by the method for finding an integer log base 10 from here:
// http://graphics.stanford.edu/~seander/bithacks.html#IntegerLog10
static unsigned int const FastDtoa_kSmallPowersOfTen[] =
    {0, 1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000,
     1000000000};

static void FastDtoa_BiggestPowerTen(uint32_t number,
                                     int number_bits,
                                     uint32_t *power,
                                     int *exponent_plus_one) {
    DOUBLE_CONVERSION_ASSERT(number < (1u << (number_bits + 1)));
    // 1233/4096 is approximately 1/lg(10).
    int exponent_plus_one_guess = ((number_bits + 1) * 1233 >> 12);
    // We increment to skip over the first entry in the kPowersOf10 table.
    // Note: kPowersOf10[i] == 10^(i-1).
    exponent_plus_one_guess++;
    // We don't have any guarantees that 2^number_bits <= number.
    if (number < FastDtoa_kSmallPowersOfTen[exponent_plus_one_guess]) {
        exponent_plus_one_guess--;
    }
    *power = FastDtoa_kSmallPowersOfTen[exponent_plus_one_guess];
    *exponent_plus_one = exponent_plus_one_guess;
}

// Generates the digits of input number w.
// w is a floating-point number (DiyFp), consisting of a significand and an
// exponent. Its exponent is bounded by kMinimalTargetExponent and
// kMaximalTargetExponent.
//       Hence -60 <= w.e() <= -32.
//
// Returns false if it fails, in which case the generated digits in the buffer
// should not be used.
// Preconditions:
//  * low, w and high are correct up to 1 ulp (unit in the last place). That
//    is, their error must be less than a unit of their last digits.
//  * low.e() == w.e() == high.e()
//  * low < w < high, and taking into account their error: low~ <= high~
//  * kMinimalTargetExponent <= w.e() <= kMaximalTargetExponent
// Postconditions: returns false if procedure fails.
//   otherwise:
//     * buffer is not null-terminated, but len contains the number of digits.
//     * buffer contains the shortest possible decimal digit-sequence
//       such that LOW < buffer * 10^kappa < HIGH, where LOW and HIGH are the
//       correct values of low and high (without their error).
//     * if more than one decimal representation gives the minimal number of
//       decimal digits then the one closest to W (where W is the correct value
//       of w) is chosen.
// Remark: this procedure takes into account the imprecision of its input
//   numbers. If the precision is not enough to guarantee all the postconditions
//   then false is returned. This usually happens rarely (~0.5%).
//
// Say, for the sake of example, that
//   w.e() == -48, and w.f() == 0x1234567890abcdef
// w's value can be computed by w.f() * 2^w.e()
// We can obtain w's integral digits by simply shifting w.f() by -w.e().
//  -> w's integral part is 0x1234
//  w's fractional part is therefore 0x567890abcdef.
// Printing w's integral part is easy (simply print 0x1234 in decimal).
// In order to print its fraction we repeatedly multiply the fraction by 10 and
// get each digit. Example the first digit after the point would be computed by
//   (0x567890abcdef * 10) >> 48. -> 3
// The whole thing becomes slightly more complicated because we want to stop
// once we have enough digits. That is, once the digits inside the buffer
// represent 'w' we can stop. Everything inside the interval low - high
// represents w. However we have to pay attention to low, high and w's
// imprecision.
static bool FastDtoa_DigitGen(DiyFp low,
                              DiyFp w,
                              DiyFp high,
                              Vector *buffer,
                              int *length,
                              int *kappa) {
    DOUBLE_CONVERSION_ASSERT(low.e == w.e && w.e == high.e);
    DOUBLE_CONVERSION_ASSERT(low.f + 1 <= high.f - 1);
    DOUBLE_CONVERSION_ASSERT(FastDtoa_kMinimalTargetExponent <= w.e && w.e <= FastDtoa_kMaximalTargetExponent);
    // low, w and high are imprecise, but by less than one ulp (unit in the last
    // place).
    // If we remove (resp. add) 1 ulp from low (resp. high) we are certain that
    // the new numbers are outside of the interval we want the final
    // representation to lie in.
    // Inversely adding (resp. removing) 1 ulp from low (resp. high) would yield
    // numbers that are certain to lie in the interval. We will use this fact
    // later on.
    // We will now start by generating the digits within the uncertain
    // interval. Later we will weed out representations that lie outside the safe
    // interval and thus _might_ lie outside the correct interval.
    uint64_t unit = 1;
    DiyFp too_low = DiyFp_make(low.f - unit, low.e);
    DiyFp too_high = DiyFp_make(high.f + unit, high.e);
    // too_low and too_high are guaranteed to lie outside the interval we want the
    // generated number in.
    DiyFp unsafe_interval = DiyFp_Minus(&too_high, &too_low);
    // We now cut the input number into two parts: the integral digits and the
    // fractionals. We will not write any decimal separator though, but adapt
    // kappa instead.
    // Reminder: we are currently computing the digits (stored inside the buffer)
    // such that:   too_low < buffer * 10^kappa < too_high
    // We use too_high for the digit_generation and stop as soon as possible.
    // If we stop early we effectively round down.
    DiyFp one = DiyFp_make((uint64_t)(1) << -w.e, w.e);
    // Division by one is a shift.
    uint32_t integrals = (uint32_t)(too_high.f >> -one.e);
    // Modulo by one is an and.
    uint64_t fractionals = too_high.f & (one.f - 1);
    uint32_t divisor;
    int divisor_exponent_plus_one;
    FastDtoa_BiggestPowerTen(integrals, DiyFp_kSignificandSize - (-one.e),
                             &divisor, &divisor_exponent_plus_one);
    *kappa = divisor_exponent_plus_one;
    *length = 0;
    // Loop invariant: buffer = too_high / 10^kappa  (integer division)
    // The invariant holds for the first iteration: kappa has been initialized
    // with the divisor exponent + 1. And the divisor is the biggest power of ten
    // that is smaller than integrals.
    while (*kappa > 0) {
        int digit = integrals / divisor;
        DOUBLE_CONVERSION_ASSERT(digit <= 9);
        buffer->start[*length] = (char)('0' + digit);
        (*length)++;
        integrals %= divisor;
        (*kappa)--;
        // Note that kappa now equals the exponent of the divisor and that the
        // invariant thus holds again.
        uint64_t rest = ((uint64_t)(integrals) << -one.e) + fractionals;
        // Invariant: too_high = buffer * 10^kappa + DiyFp(rest, one.e())
        // Reminder: unsafe_interval.e() == one.e()
        if (rest < unsafe_interval.f) {
            // Rounding down (by not emitting the remaining digits) yields a number
            // that lies within the unsafe interval.
            return FastDtoa_RoundWeed(buffer, *length, DiyFp_Minus(&too_high, &w).f,
                                      unsafe_interval.f, rest,
                                      (uint64_t)(divisor) << -one.e, unit);
        }
        divisor /= 10;
    }
    
    // The integrals have been generated. We are at the point of the decimal
    // separator. In the following loop we simply multiply the remaining digits by
    // 10 and divide by one. We just need to pay attention to multiply associated
    // data (like the interval or 'unit'), too.
    // Note that the multiplication by 10 does not overflow, because w.e >= -60
    // and thus one.e >= -60.
    DOUBLE_CONVERSION_ASSERT(one.e >= -60);
    DOUBLE_CONVERSION_ASSERT(fractionals < one.f);
    DOUBLE_CONVERSION_ASSERT(DOUBLE_CONVERSION_UINT64_2PART_C(0xFFFFFFFF, FFFFFFFF) / 10 >= one.f);
    for (;;) {
        fractionals *= 10;
        unit *= 10;
        unsafe_interval.f = (unsafe_interval.f * 10);
        // Integer division by one.
        int digit = (int)(fractionals >> -one.e);
        DOUBLE_CONVERSION_ASSERT(digit <= 9);
        buffer->start[*length] = (char)('0' + digit);
        (*length)++;
        fractionals &= one.f - 1;  // Modulo by one.
        (*kappa)--;
        if (fractionals < unsafe_interval.f) {
          return FastDtoa_RoundWeed(buffer, *length, DiyFp_Minus(&too_high, &w).f * unit,
                           unsafe_interval.f, fractionals, one.f, unit);
        }
    }
}

// Generates (at most) requested_digits digits of input number w.
// w is a floating-point number (DiyFp), consisting of a significand and an
// exponent. Its exponent is bounded by kMinimalTargetExponent and
// kMaximalTargetExponent.
//       Hence -60 <= w.e() <= -32.
//
// Returns false if it fails, in which case the generated digits in the buffer
// should not be used.
// Preconditions:
//  * w is correct up to 1 ulp (unit in the last place). That
//    is, its error must be strictly less than a unit of its last digit.
//  * kMinimalTargetExponent <= w.e() <= kMaximalTargetExponent
//
// Postconditions: returns false if procedure fails.
//   otherwise:
//     * buffer is not null-terminated, but length contains the number of
//       digits.
//     * the representation in buffer is the most precise representation of
//       requested_digits digits.
//     * buffer contains at most requested_digits digits of w. If there are less
//       than requested_digits digits then some trailing '0's have been removed.
//     * kappa is such that
//            w = buffer * 10^kappa + eps with |eps| < 10^kappa / 2.
//
// Remark: This procedure takes into account the imprecision of its input
//   numbers. If the precision is not enough to guarantee all the postconditions
//   then false is returned. This usually happens rarely, but the failure-rate
//   increases with higher requested_digits.
static bool FastDtoa_DigitGenCounted(DiyFp w,
                                     int requested_digits,
                                     Vector *buffer,
                                     int *length,
                                     int *kappa) {
    DOUBLE_CONVERSION_ASSERT(FastDtoa_kMinimalTargetExponent <= w.e && w.e <= FastDtoa_kMaximalTargetExponent);
    DOUBLE_CONVERSION_ASSERT(FastDtoa_kMinimalTargetExponent >= -60);
    DOUBLE_CONVERSION_ASSERT(FastDtoa_kMaximalTargetExponent <= -32);
    // w is assumed to have an error less than 1 unit. Whenever w is scaled we
    // also scale its error.
    uint64_t w_error = 1;
    // We cut the input number into two parts: the integral digits and the
    // fractional digits. We don't emit any decimal separator, but adapt kappa
    // instead. Example: instead of writing "1.2" we put "12" into the buffer and
    // increase kappa by 1.
    DiyFp one = DiyFp_make((uint64_t)(1) << -w.e, w.e);
    // Division by one is a shift.
    uint32_t integrals = (uint32_t)(w.f >> -one.e);
    // Modulo by one is an and.
    uint64_t fractionals = w.f & (one.f - 1);
    uint32_t divisor;
    int divisor_exponent_plus_one;
    FastDtoa_BiggestPowerTen(integrals, DiyFp_kSignificandSize - (-one.e),
                  &divisor, &divisor_exponent_plus_one);
    *kappa = divisor_exponent_plus_one;
    *length = 0;
    
    // Loop invariant: buffer = w / 10^kappa  (integer division)
    // The invariant holds for the first iteration: kappa has been initialized
    // with the divisor exponent + 1. And the divisor is the biggest power of ten
    // that is smaller than 'integrals'.
    while (*kappa > 0) {
        int digit = integrals / divisor;
        DOUBLE_CONVERSION_ASSERT(digit <= 9);
        buffer->start[*length] = (char)('0' + digit);
        (*length)++;
        requested_digits--;
        integrals %= divisor;
        (*kappa)--;
        // Note that kappa now equals the exponent of the divisor and that the
        // invariant thus holds again.
        if (requested_digits == 0) break;
        divisor /= 10;
    }
    
    if (requested_digits == 0) {
        uint64_t rest = ((uint64_t)(integrals) << -one.e) + fractionals;
        return FastDtoa_RoundWeedCounted(buffer, *length, rest,
                            (uint64_t)(divisor) << -one.e, w_error,
                            kappa);
    }
    
    // The integrals have been generated. We are at the point of the decimal
    // separator. In the following loop we simply multiply the remaining digits by
    // 10 and divide by one. We just need to pay attention to multiply associated
    // data (the 'unit'), too.
    // Note that the multiplication by 10 does not overflow, because w.e >= -60
    // and thus one.e >= -60.
    DOUBLE_CONVERSION_ASSERT(one.e >= -60);
    DOUBLE_CONVERSION_ASSERT(fractionals < one.f);
    DOUBLE_CONVERSION_ASSERT(DOUBLE_CONVERSION_UINT64_2PART_C(0xFFFFFFFF, FFFFFFFF) / 10 >= one.f);
    while (requested_digits > 0 && fractionals > w_error) {
        fractionals *= 10;
        w_error *= 10;
        // Integer division by one.
        int digit = (int)(fractionals >> -one.e);
        DOUBLE_CONVERSION_ASSERT(digit <= 9);
        buffer->start[*length] = (char)('0' + digit);
        (*length)++;
        requested_digits--;
        fractionals &= one.f - 1;  // Modulo by one.
        (*kappa)--;
    }
    if (requested_digits != 0) return false;
    return FastDtoa_RoundWeedCounted(buffer, *length, fractionals, one.f, w_error, kappa);
}

// Provides a decimal representation of v.
// Returns true if it succeeds, otherwise the result cannot be trusted.
// There will be *length digits inside the buffer (not null-terminated).
// If the function returns true then
//        v == (double) (buffer * 10^decimal_exponent).
// The digits in the buffer are the shortest representation possible: no
// 0.09999999999999999 instead of 0.1. The shorter representation will even be
// chosen even if the longer one would be closer to v.
// The last digit will be closest to the actual v. That is, even if several
// digits might correctly yield 'v' when read again, the closest will be
// computed.
static bool FastDtoa_Grisu3(double v,
                            FastDtoaMode mode,
                            Vector *buffer,
                            int *length,
                            int *decimal_exponent) {
    Double d = Double_make(v);
    DiyFp w = Double_AsNormalizedDiyFp(&d);
    // boundary_minus and boundary_plus are the boundaries between v and its
    // closest floating-point neighbors. Any number strictly between
    // boundary_minus and boundary_plus will round to v when convert to a double.
    // Grisu3 will never output representations that lie exactly on a boundary.
    DiyFp boundary_minus, boundary_plus;
    if (mode == FAST_DTOA_SHORTEST) {
        Double_NormalizedBoundaries(&d, &boundary_minus, &boundary_plus);
    } else {
        DOUBLE_CONVERSION_ASSERT(mode == FAST_DTOA_SHORTEST_SINGLE);
        float single_v = (float)(v);
        Single s = Single_make(single_v);
        Single_NormalizedBoundaries(&s, &boundary_minus, &boundary_plus);
    }
    DOUBLE_CONVERSION_ASSERT(boundary_plus.e == w.e);
    DiyFp ten_mk = { 0 };   // Cached power of ten: 10^-k
    int mk = 0;             // -k
    int ten_mk_minimal_binary_exponent =
        FastDtoa_kMinimalTargetExponent - (w.e + DiyFp_kSignificandSize);
    int ten_mk_maximal_binary_exponent =
        FastDtoa_kMaximalTargetExponent - (w.e + DiyFp_kSignificandSize);
    Cache_GetCachedPowerForBinaryExponentRange(
        ten_mk_minimal_binary_exponent,
        ten_mk_maximal_binary_exponent,
        &ten_mk, &mk);
    DOUBLE_CONVERSION_ASSERT((FastDtoa_kMinimalTargetExponent <= w.e + ten_mk.e +
          DiyFp_kSignificandSize) &&
         (FastDtoa_kMaximalTargetExponent >= w.e + ten_mk.e +
          DiyFp_kSignificandSize));
    // Note that ten_mk is only an approximation of 10^-k. A DiyFp only contains a
    // 64 bit significand and ten_mk is thus only precise up to 64 bits.
    
    // The DiyFp::Times procedure rounds its result, and ten_mk is approximated
    // too. The variable scaled_w (as well as scaled_boundary_minus/plus) are now
    // off by a small amount.
    // In fact: scaled_w - w*10^k < 1ulp (unit in the last place) of scaled_w.
    // In other words: let f = scaled_w.f() and e = scaled_w.e(), then
    //           (f-1) * 2^e < w*10^k < (f+1) * 2^e
    DiyFp scaled_w = DiyFp_Times(&w, &ten_mk);
    DOUBLE_CONVERSION_ASSERT(scaled_w.e ==
         boundary_plus.e + ten_mk.e + DiyFp_kSignificandSize);
    // In theory it would be possible to avoid some recomputations by computing
    // the difference between w and boundary_minus/plus (a power of 2) and to
    // compute scaled_boundary_minus/plus by subtracting/adding from
    // scaled_w. However the code becomes much less readable and the speed
    // enhancements are not terrific.
    DiyFp scaled_boundary_minus = DiyFp_Times(&boundary_minus, &ten_mk);
    DiyFp scaled_boundary_plus  = DiyFp_Times(&boundary_plus,  &ten_mk);
    
    // DigitGen will generate the digits of scaled_w. Therefore we have
    // v == (double) (scaled_w * 10^-mk).
    // Set decimal_exponent == -mk and pass it to DigitGen. If scaled_w is not an
    // integer than it will be updated. For instance if scaled_w == 1.23 then
    // the buffer will be filled with "123" and the decimal_exponent will be
    // decreased by 2.
    int kappa = 0;
    bool result = FastDtoa_DigitGen(scaled_boundary_minus, scaled_w, scaled_boundary_plus,
                                  buffer, length, &kappa);
    *decimal_exponent = -mk + kappa;
    return result;
}

// The "counted" version of grisu3 (see above) only generates requested_digits
// number of digits. This version does not generate the shortest representation,
// and with enough requested digits 0.1 will at some point print as 0.9999999...
// Grisu3 is too imprecise for real halfway cases (1.5 will not work) and
// therefore the rounding strategy for halfway cases is irrelevant.
static bool FastDtoa_Grisu3Counted(double v,
                                   int requested_digits,
                                   Vector *buffer,
                                   int *length,
                                   int *decimal_exponent) {
    Double d = Double_make(v);
    DiyFp w = Double_AsNormalizedDiyFp(&d);
    DiyFp ten_mk = { 0 };   // Cached power of ten: 10^-k
    int mk = 0;             // -k
    int ten_mk_minimal_binary_exponent =
        FastDtoa_kMinimalTargetExponent - (w.e + DiyFp_kSignificandSize);
    int ten_mk_maximal_binary_exponent =
        FastDtoa_kMaximalTargetExponent - (w.e + DiyFp_kSignificandSize);
    Cache_GetCachedPowerForBinaryExponentRange(
        ten_mk_minimal_binary_exponent,
        ten_mk_maximal_binary_exponent,
        &ten_mk, &mk);
    DOUBLE_CONVERSION_ASSERT((FastDtoa_kMinimalTargetExponent <= w.e + ten_mk.e +
          DiyFp_kSignificandSize) &&
         (FastDtoa_kMaximalTargetExponent >= w.e + ten_mk.e +
          DiyFp_kSignificandSize));
    // Note that ten_mk is only an approximation of 10^-k. A DiyFp only contains a
    // 64 bit significand and ten_mk is thus only precise up to 64 bits.
    
    // The DiyFp::Times procedure rounds its result, and ten_mk is approximated
    // too. The variable scaled_w (as well as scaled_boundary_minus/plus) are now
    // off by a small amount.
    // In fact: scaled_w - w*10^k < 1ulp (unit in the last place) of scaled_w.
    // In other words: let f = scaled_w.f() and e = scaled_w.e(), then
    //           (f-1) * 2^e < w*10^k < (f+1) * 2^e
    DiyFp scaled_w = DiyFp_Times(&w, &ten_mk);
    
    // We now have (double) (scaled_w * 10^-mk).
    // DigitGen will generate the first requested_digits digits of scaled_w and
    // return together with a kappa such that scaled_w ~= buffer * 10^kappa. (It
    // will not always be exactly the same since DigitGenCounted only produces a
    // limited number of digits.)
    int kappa = 0;
    bool result = FastDtoa_DigitGenCounted(scaled_w, requested_digits,
                                           buffer, length, &kappa);
    *decimal_exponent = -mk + kappa;
    return result;
}

static bool FastDtoa(double v,
                     FastDtoaMode mode,
                     int requested_digits,
                     Vector *buffer,
                     int *length,
                     int *decimal_point) {
    DOUBLE_CONVERSION_ASSERT(v > 0);
    Double d = Double_make(v);
    DOUBLE_CONVERSION_ASSERT(!Double_IsSpecial(&d));
    
    bool result = false;
    int decimal_exponent = 0;
    switch (mode) {
        case FAST_DTOA_SHORTEST:
        case FAST_DTOA_SHORTEST_SINGLE:
            result = FastDtoa_Grisu3(v, mode, buffer, length, &decimal_exponent);
            break;
        case FAST_DTOA_PRECISION:
            result = FastDtoa_Grisu3Counted(v, requested_digits,
                                            buffer, length, &decimal_exponent);
            break;
        default:
            DOUBLE_CONVERSION_UNREACHABLE();
    }
    if (result) {
        *decimal_point = *length + decimal_exponent;
        buffer->start[*length] = '\0';
    }
    return result;
}



/// ============================================================================
/// fix-dtoa.h
/// ============================================================================

// Produces digits necessary to print a given number with
// 'fractional_count' digits after the decimal point.
// The buffer must be big enough to hold the result plus one terminating null
// character.
//
// The produced digits might be too short in which case the caller has to fill
// the gaps with '0's.
// Example: FastFixedDtoa(0.001, 5, ...) is allowed to return buffer = "1", and
// decimal_point = -2.
// Halfway cases are rounded towards +/-Infinity (away from 0). The call
// FastFixedDtoa(0.15, 2, ...) thus returns buffer = "2", decimal_point = 0.
// The returned buffer may contain digits that would be truncated from the
// shortest representation of the input.
//
// This method only works for some parameters. If it can't handle the input it
// returns false. The output is null-terminated when the function succeeds.
static bool FastFixedDtoa(double v, int fractional_count,
                          Vector *buffer, int *length, int *decimal_point);



/// ============================================================================
/// fix-dtoa.cc
/// ============================================================================

static const uint64_t UInt128_kMask32 = 0xFFFFFFFF;

// Represents a 128bit type. This class should be replaced by a native type on
// platforms that support 128bit integers.
typedef struct UInt128 {
    // Value == (high_bits_ << 64) + low_bits_
    uint64_t high_bits;
    uint64_t low_bits;
} UInt128;

static UInt128 UInt128_make(uint64_t high, uint64_t low) {
    UInt128 u;
    u.high_bits = high;
    u.low_bits = low;
    return u;
}

static void UInt128_Multiply(UInt128 *u, uint32_t multiplicand) {
    uint64_t accumulator;
    
    accumulator = (u->low_bits & UInt128_kMask32) * multiplicand;
    uint32_t part = (uint32_t)(accumulator & UInt128_kMask32);
    accumulator >>= 32;
    accumulator = accumulator + (u->low_bits >> 32) * multiplicand;
    u->low_bits = (accumulator << 32) + part;
    accumulator >>= 32;
    accumulator = accumulator + (u->high_bits & UInt128_kMask32) * multiplicand;
    part = (uint32_t)(accumulator & UInt128_kMask32);
    accumulator >>= 32;
    accumulator = accumulator + (u->high_bits >> 32) * multiplicand;
    u->high_bits = (accumulator << 32) + part;
    DOUBLE_CONVERSION_ASSERT((accumulator >> 32) == 0);
}

static void UInt128_Shift(UInt128 *u, int shift_amount) {
    DOUBLE_CONVERSION_ASSERT(-64 <= shift_amount && shift_amount <= 64);
    if (shift_amount == 0) {
        return;
    } else if (shift_amount == -64) {
        u->high_bits = u->low_bits;
        u->low_bits = 0;
    } else if (shift_amount == 64) {
        u->low_bits = u->high_bits;
        u->high_bits = 0;
    } else if (shift_amount <= 0) {
        u->high_bits <<= -shift_amount;
        u->high_bits += u->low_bits >> (64 + shift_amount);
        u->low_bits <<= -shift_amount;
    } else {
        u->low_bits >>= shift_amount;
        u->low_bits += u->high_bits << (64 - shift_amount);
        u->high_bits >>= shift_amount;
    }
}

  // Modifies *this to *this MOD (2^power).
  // Returns *this DIV (2^power).
static int UInt128_DivModPowerOf2(UInt128 *u, int power) {
    if (power >= 64) {
        int result = (int)(u->high_bits >> (power - 64));
        u->high_bits -= (uint64_t)(result) << (power - 64);
        return result;
    } else {
        uint64_t part_low = u->low_bits >> power;
        uint64_t part_high = u->high_bits << (64 - power);
        int result = (int)(part_low + part_high);
        u->high_bits = 0;
        u->low_bits -= part_low << power;
        return result;
    }
}

static bool UInt128_IsZero(UInt128 *u) {
    return u->high_bits == 0 && u->low_bits == 0;
}

static int UInt128_BitAt(UInt128 *u, int position) {
    if (position >= 64) {
      return (int)(u->high_bits >> (position - 64)) & 1;
    } else {
      return (int)(u->low_bits >> position) & 1;
    }
}



static const int Fixed_kDoubleSignificandSize = 53;  // Includes the hidden bit.

static void Fixed_FillDigits32FixedLength(uint32_t number, int requested_length,
                                          Vector *buffer, int *length) {
    for (int i = requested_length - 1; i >= 0; --i) {
        buffer->start[(*length) + i] = '0' + number % 10;
        number /= 10;
    }
    *length += requested_length;
}

static void Fixed_FillDigits32(uint32_t number, Vector *buffer, int*length) {
    int number_length = 0;
    // We fill the digits in reverse order and exchange them afterwards.
    while (number != 0) {
        int digit = number % 10;
        number /= 10;
        buffer->start[(*length) + number_length] = (char)('0' + digit);
        number_length++;
    }
    // Exchange the digits.
    int i = *length;
    int j = *length + number_length - 1;
    while (i < j) {
        char tmp = buffer->start[i];
        buffer->start[i] = buffer->start[j];
        buffer->start[j] = tmp;
        i++;
        j--;
    }
    *length += number_length;
}

static void Fixed_FillDigits64FixedLength(uint64_t number,
                                          Vector *buffer, int *length) {
    const uint32_t kTen7 = 10000000;
    // For efficiency cut the number into 3 uint32_t parts, and print those.
    uint32_t part2 = (uint32_t)(number % kTen7);
    number /= kTen7;
    uint32_t part1 = (uint32_t)(number % kTen7);
    uint32_t part0 = (uint32_t)(number / kTen7);

    Fixed_FillDigits32FixedLength(part0, 3, buffer, length);
    Fixed_FillDigits32FixedLength(part1, 7, buffer, length);
    Fixed_FillDigits32FixedLength(part2, 7, buffer, length);
}

static void Fixed_FillDigits64(uint64_t number, Vector *buffer, int *length) {
    const uint32_t kTen7 = 10000000;
    // For efficiency cut the number into 3 uint32_t parts, and print those.
    uint32_t part2 = (uint32_t)(number % kTen7);
    number /= kTen7;
    uint32_t part1 = (uint32_t)(number % kTen7);
    uint32_t part0 = (uint32_t)(number / kTen7);

    if (part0 != 0) {
        Fixed_FillDigits32(part0, buffer, length);
        Fixed_FillDigits32FixedLength(part1, 7, buffer, length);
        Fixed_FillDigits32FixedLength(part2, 7, buffer, length);
    } else if (part1 != 0) {
        Fixed_FillDigits32(part1, buffer, length);
        Fixed_FillDigits32FixedLength(part2, 7, buffer, length);
    } else {
        Fixed_FillDigits32(part2, buffer, length);
    }
}

static void Fixed_RoundUp(Vector *buffer, int *length, int *decimal_point) {
    // An empty buffer represents 0.
    if (*length == 0) {
        buffer->start[0] = '1';
        *decimal_point = 1;
        *length = 1;
        return;
    }
    // Round the last digit until we either have a digit that was not '9' or until
    // we reached the first digit.
    buffer->start[(*length) - 1]++;
    for (int i = (*length) - 1; i > 0; --i) {
        if (buffer->start[i] != '0' + 10) {
            return;
        }
        buffer->start[i] = '0';
        buffer->start[i - 1]++;
    }
    // If the first digit is now '0' + 10, we would need to set it to '0' and add
    // a '1' in front. However we reach the first digit only if all following
    // digits had been '9' before rounding up. Now all trailing digits are '0' and
    // we simply switch the first digit to '1' and update the decimal-point
    // (indicating that the point is now one digit to the right).
    if (buffer->start[0] == '0' + 10) {
        buffer->start[0] = '1';
        (*decimal_point)++;
    }
}

// The given fractionals number represents a fixed-point number with binary
// point at bit (-exponent).
// Preconditions:
//   -128 <= exponent <= 0.
//   0 <= fractionals * 2^exponent < 1
//   The buffer holds the result.
// The function will round its result. During the rounding-process digits not
// generated by this function might be updated, and the decimal-point variable
// might be updated. If this function generates the digits 99 and the buffer
// already contained "199" (thus yielding a buffer of "19999") then a
// rounding-up will change the contents of the buffer to "20000".
static void Fixed_FillFractionals(uint64_t fractionals, int exponent,
                                  int fractional_count, Vector *buffer,
                                  int *length, int *decimal_point) {
    DOUBLE_CONVERSION_ASSERT(-128 <= exponent && exponent <= 0);
    // 'fractionals' is a fixed-point number, with binary point at bit
    // (-exponent). Inside the function the non-converted remainder of fractionals
    // is a fixed-point number, with binary point at bit 'point'.
    if (-exponent <= 64) {
        // One 64 bit number is sufficient.
        DOUBLE_CONVERSION_ASSERT(fractionals >> 56 == 0);
        int point = -exponent;
        for (int i = 0; i < fractional_count; ++i) {
            if (fractionals == 0) break;
            // Instead of multiplying by 10 we multiply by 5 and adjust the point
            // location. This way the fractionals variable will not overflow.
            // Invariant at the beginning of the loop: fractionals < 2^point.
            // Initially we have: point <= 64 and fractionals < 2^56
            // After each iteration the point is decremented by one.
            // Note that 5^3 = 125 < 128 = 2^7.
            // Therefore three iterations of this loop will not overflow fractionals
            // (even without the subtraction at the end of the loop body). At this
            // time point will satisfy point <= 61 and therefore fractionals < 2^point
            // and any further multiplication of fractionals by 5 will not overflow.
            fractionals *= 5;
            point--;
            int digit = (int)(fractionals >> point);
            DOUBLE_CONVERSION_ASSERT(digit <= 9);
            buffer->start[*length] = (char)('0' + digit);
            (*length)++;
            fractionals -= (uint64_t)(digit) << point;
        }
        // If the first bit after the point is set we have to round up.
        DOUBLE_CONVERSION_ASSERT(fractionals == 0 || point - 1 >= 0);
        if ((fractionals != 0) && ((fractionals >> (point - 1)) & 1) == 1) {
            Fixed_RoundUp(buffer, length, decimal_point);
        }
    } else { // We need 128 bits.
        DOUBLE_CONVERSION_ASSERT(64 < -exponent && -exponent <= 128);
        UInt128 fractionals128 = UInt128_make(fractionals, 0);
        UInt128_Shift(&fractionals128, -exponent - 64);
        int point = 128;
        for (int i = 0; i < fractional_count; ++i) {
            if (UInt128_IsZero(&fractionals128)) break;
            // As before: instead of multiplying by 10 we multiply by 5 and adjust the
            // point location.
            // This multiplication will not overflow for the same reasons as before.
            UInt128_Multiply(&fractionals128, 5);
            point--;
            int digit = UInt128_DivModPowerOf2(&fractionals128, point);
            DOUBLE_CONVERSION_ASSERT(digit <= 9);
            buffer->start[*length] = (char)('0' + digit);
            (*length)++;
        }
        if (UInt128_BitAt(&fractionals128, point - 1) == 1) {
            Fixed_RoundUp(buffer, length, decimal_point);
        }
    }
}

// Removes leading and trailing zeros.
// If leading zeros are removed then the decimal point position is adjusted.
static void Fixed_TrimZeros(Vector *buffer, int *length, int *decimal_point) {
    while (*length > 0 && buffer->start[(*length) - 1] == '0') {
        (*length)--;
    }
    int first_non_zero = 0;
    while (first_non_zero < *length && buffer->start[first_non_zero] == '0') {
        first_non_zero++;
    }
    if (first_non_zero != 0) {
        for (int i = first_non_zero; i < *length; ++i) {
            buffer->start[i - first_non_zero] = buffer->start[i];
        }
        *length -= first_non_zero;
        *decimal_point -= first_non_zero;
    }
}

static bool FastFixedDtoa(double v,
                          int fractional_count,
                          Vector *buffer,
                          int *length,
                          int *decimal_point) {
    const uint32_t kMaxUInt32 = 0xFFFFFFFF;
    Double d = Double_make(v);
    uint64_t significand = Double_Significand(&d);
    int exponent = Double_Exponent(&d);
    // v = significand * 2^exponent (with significand a 53bit integer).
    // If the exponent is larger than 20 (i.e. we may have a 73bit number) then we
    // don't know how to compute the representation. 2^73 ~= 9.5*10^21.
    // If necessary this limit could probably be increased, but we don't need
    // more.
    if (exponent > 20) return false;
    if (fractional_count > 20) return false;
    *length = 0;
    // At most kDoubleSignificandSize bits of the significand are non-zero.
    // Given a 64 bit integer we have 11 0s followed by 53 potentially non-zero
    // bits:  0..11*..0xxx..53*..xx
    if (exponent + Fixed_kDoubleSignificandSize > 64) {
        // The exponent must be > 11.
        //
        // We know that v = significand * 2^exponent.
        // And the exponent > 11.
        // We simplify the task by dividing v by 10^17.
        // The quotient delivers the first digits, and the remainder fits into a 64
        // bit number.
        // Dividing by 10^17 is equivalent to dividing by 5^17*2^17.
        const uint64_t kFive17 = DOUBLE_CONVERSION_UINT64_2PART_C(0xB1, A2BC2EC5);  // 5^17
        uint64_t divisor = kFive17;
        int divisor_power = 17;
        uint64_t dividend = significand;
        uint32_t quotient;
        uint64_t remainder;
        // Let v = f * 2^e with f == significand and e == exponent.
        // Then need q (quotient) and r (remainder) as follows:
        //   v            = q * 10^17       + r
        //   f * 2^e      = q * 10^17       + r
        //   f * 2^e      = q * 5^17 * 2^17 + r
        // If e > 17 then
        //   f * 2^(e-17) = q * 5^17        + r/2^17
        // else
        //   f  = q * 5^17 * 2^(17-e) + r/2^e
        if (exponent > divisor_power) {
            // We only allow exponents of up to 20 and therefore (17 - e) <= 3
            dividend <<= exponent - divisor_power;
            quotient = (uint32_t)(dividend / divisor);
            remainder = (dividend % divisor) << divisor_power;
        } else {
            divisor <<= divisor_power - exponent;
            quotient = (uint32_t)(dividend / divisor);
            remainder = (dividend % divisor) << exponent;
        }
        Fixed_FillDigits32(quotient, buffer, length);
        Fixed_FillDigits64FixedLength(remainder, buffer, length);
        *decimal_point = *length;
    } else if (exponent >= 0) {
        // 0 <= exponent <= 11
        significand <<= exponent;
          Fixed_FillDigits64(significand, buffer, length);
        *decimal_point = *length;
    } else if (exponent > -Fixed_kDoubleSignificandSize) {
        // We have to cut the number.
        uint64_t integrals = significand >> -exponent;
        uint64_t fractionals = significand - (integrals << -exponent);
        if (integrals > kMaxUInt32) {
            Fixed_FillDigits64(integrals, buffer, length);
        } else {
            Fixed_FillDigits32((uint32_t)(integrals), buffer, length);
        }
        *decimal_point = *length;
        Fixed_FillFractionals(fractionals, exponent, fractional_count,
                        buffer, length, decimal_point);
    } else if (exponent < -128) {
        // This configuration (with at most 20 digits) means that all digits must be
        // 0.
        DOUBLE_CONVERSION_ASSERT(fractional_count <= 20);
        buffer->start[0] = '\0';
        *length = 0;
        *decimal_point = -fractional_count;
    } else {
        *decimal_point = 0;
        Fixed_FillFractionals(significand, exponent, fractional_count,
                        buffer, length, decimal_point);
    }
    Fixed_TrimZeros(buffer, length, decimal_point);
    buffer->start[*length] = '\0';
    if ((*length) == 0) {
        // The string is empty and the decimal_point thus has no importance. Mimic
        // Gay's dtoa and set it to -fractional_count.
        *decimal_point = -fractional_count;
    }
    return true;
}



/// ============================================================================
/// strtod.h
/// ============================================================================

// The buffer must only contain digits in the range [0-9]. It must not
// contain a dot or a sign. It must not start with '0', and must not be empty.
static double Strtod(Vector *buffer, int exponent);

// The buffer must only contain digits in the range [0-9]. It must not
// contain a dot or a sign. It must not start with '0', and must not be empty.
static float Strtof(Vector *buffer, int exponent);

// Same as Strtod, but assumes that 'trimmed' is already trimmed, as if run
// through TrimAndCut. That is, 'trimmed' must have no leading or trailing
// zeros, must not be a lone zero, and must not have 'too many' digits.
static double StrtodTrimmed(Vector *trimmed, int exponent);

// Same as Strtof, but assumes that 'trimmed' is already trimmed, as if run
// through TrimAndCut. That is, 'trimmed' must have no leading or trailing
// zeros, must not be a lone zero, and must not have 'too many' digits.
static float StrtofTrimmed(Vector *trimmed, int exponent);

static Vector Strtod_TrimTrailingZeros(Vector *buffer) {
    for (int i = buffer->length - 1; i >= 0; --i) {
        if (buffer->start[i] != '0') {
            return Vector_SubVector(buffer, 0, i + 1);
        }
    }
    return Vector_make(buffer->start, 0);
}



/// ============================================================================
/// strtod.cc
/// ============================================================================

#if defined(DOUBLE_CONVERSION_CORRECT_DOUBLE_OPERATIONS)
// 2^53 = 9007199254740992.
// Any integer with at most 15 decimal digits will hence fit into a double
// (which has a 53bit significand) without loss of precision.
static const int Strtod_kMaxExactDoubleIntegerDecimalDigits = 15;
#endif // #if defined(DOUBLE_CONVERSION_CORRECT_DOUBLE_OPERATIONS)
// 2^64 = 18446744073709551616 > 10^19
static const int Strtod_kMaxUint64DecimalDigits = 19;

// Max double: 1.7976931348623157 x 10^308
// Min non-zero double: 4.9406564584124654 x 10^-324
// Any x >= 10^309 is interpreted as +infinity.
// Any x <= 10^-324 is interpreted as 0.
// Note that 2.5e-324 (despite being smaller than the min double) will be read
// as non-zero (equal to the min non-zero double).
static const int Strtod_kMaxDecimalPower = 309;
static const int Strtod_kMinDecimalPower = -324;

// 2^64 = 18446744073709551616
static const uint64_t Strtod_kMaxUint64 = DOUBLE_CONVERSION_UINT64_2PART_C(0xFFFFFFFF, FFFFFFFF);

#if defined(DOUBLE_CONVERSION_CORRECT_DOUBLE_OPERATIONS)
static const double Strtod_exact_powers_of_ten[] = {
  1.0,  // 10^0
  10.0,
  100.0,
  1000.0,
  10000.0,
  100000.0,
  1000000.0,
  10000000.0,
  100000000.0,
  1000000000.0,
  10000000000.0,  // 10^10
  100000000000.0,
  1000000000000.0,
  10000000000000.0,
  100000000000000.0,
  1000000000000000.0,
  10000000000000000.0,
  100000000000000000.0,
  1000000000000000000.0,
  10000000000000000000.0,
  100000000000000000000.0,  // 10^20
  1000000000000000000000.0,
  // 10^22 = 0x21e19e0c9bab2400000 = 0x878678326eac9 * 2^22
  10000000000000000000000.0
};
static const int Strtod_kExactPowersOfTenSize = DOUBLE_CONVERSION_ARRAY_SIZE(Strtod_exact_powers_of_ten);
#endif // #if defined(DOUBLE_CONVERSION_CORRECT_DOUBLE_OPERATIONS)

// Maximum number of significant digits in the decimal representation.
// In fact the value is 772 (see conversions.cc), but to give us some margin
// we round up to 780.
#define Strtod_kMaxSignificantDecimalDigits ((int)780)

static Vector Strtod_TrimLeadingZeros(Vector *buffer) {
    for (int i = 0; i < buffer->length; i++) {
        if (buffer->start[i] != '0') {
            return Vector_SubVector(buffer, i, buffer->length);
        }
    }
    return Vector_make(buffer->start, 0);
}

static void Strtod_CutToMaxSignificantDigits(Vector *buffer,
                                             int exponent,
                                             char *significant_buffer,
                                             int *significant_exponent) {
    for (int i = 0; i < Strtod_kMaxSignificantDecimalDigits - 1; ++i) {
        significant_buffer[i] = buffer->start[i];
    }
    // The input buffer has been trimmed. Therefore the last digit must be
    // different from '0'.
    DOUBLE_CONVERSION_ASSERT(buffer->start[buffer->length - 1] != '0');
    // Set the last digit to be non-zero. This is sufficient to guarantee
    // correct rounding.
    significant_buffer[Strtod_kMaxSignificantDecimalDigits - 1] = '1';
    *significant_exponent =
        exponent + (buffer->length - Strtod_kMaxSignificantDecimalDigits);
}

// Trims the buffer and cuts it to at most kMaxSignificantDecimalDigits.
// If possible the input-buffer is reused, but if the buffer needs to be
// modified (due to cutting), then the input needs to be copied into the
// buffer_copy_space.
static void Strtod_TrimAndCut(Vector *buffer, int exponent,
                              char *buffer_copy_space, int space_size,
                              Vector *trimmed, int *updated_exponent) {
    Vector left_trimmed = Strtod_TrimLeadingZeros(buffer);
    Vector right_trimmed = Strtod_TrimTrailingZeros(&left_trimmed);
    exponent += left_trimmed.length - right_trimmed.length;
    if (right_trimmed.length > Strtod_kMaxSignificantDecimalDigits) {
        (void) space_size;  // Mark variable as used.
        DOUBLE_CONVERSION_ASSERT(space_size >= Strtod_kMaxSignificantDecimalDigits);
        Strtod_CutToMaxSignificantDigits(&right_trimmed, exponent,
                                         buffer_copy_space, updated_exponent);
        *trimmed = Vector_make(buffer_copy_space,
                               Strtod_kMaxSignificantDecimalDigits);
    } else {
        *trimmed = right_trimmed;
        *updated_exponent = exponent;
    }
}

// Reads digits from the buffer and converts them to a uint64.
// Reads in as many digits as fit into a uint64.
// When the string starts with "1844674407370955161" no further digit is read.
// Since 2^64 = 18446744073709551616 it would still be possible read another
// digit if it was less or equal than 6, but this would complicate the code.
static uint64_t Strtod_ReadUint64(Vector *buffer,
                                  int *number_of_read_digits) {
    uint64_t result = 0;
    int i = 0;
    while (i < buffer->length && result <= (Strtod_kMaxUint64 / 10 - 1)) {
        int digit = buffer->start[i++] - '0';
        DOUBLE_CONVERSION_ASSERT(0 <= digit && digit <= 9);
        result = 10 * result + digit;
    }
    *number_of_read_digits = i;
    return result;
}

// Reads a DiyFp from the buffer.
// The returned DiyFp is not necessarily normalized.
// If remaining_decimals is zero then the returned DiyFp is accurate.
// Otherwise it has been rounded and has error of at most 1/2 ulp.
static void Strtod_ReadDiyFp(Vector *buffer,
                             DiyFp *result,
                             int *remaining_decimals) {
    int read_digits = 0;
    uint64_t significand = Strtod_ReadUint64(buffer, &read_digits);
    if (buffer->length == read_digits) {
        *result = DiyFp_make(significand, 0);
        *remaining_decimals = 0;
    } else {
        // Round the significand.
        if (buffer->start[read_digits] >= '5') {
            significand++;
        }
        // Compute the binary exponent.
        int exponent = 0;
        *result = DiyFp_make(significand, exponent);
        *remaining_decimals = buffer->length - read_digits;
    }
}

static bool Strtod_DoubleStrtod(Vector *trimmed,
                                int exponent,
                                double* result) {
#if !defined(DOUBLE_CONVERSION_CORRECT_DOUBLE_OPERATIONS)
    // Avoid "unused parameter" warnings
    (void) trimmed;
    (void) exponent;
    (void) result;
    // On x86 the floating-point stack can be 64 or 80 bits wide. If it is
    // 80 bits wide (as is the case on Linux) then double-rounding occurs and the
    // result is not accurate.
    // We know that Windows32 uses 64 bits and is therefore accurate.
    return false;
#else
    if (trimmed->length <= Strtod_kMaxExactDoubleIntegerDecimalDigits) {
        int read_digits;
        // The trimmed input fits into a double.
        // If the 10^exponent (resp. 10^-exponent) fits into a double too then we
        // can compute the result-double simply by multiplying (resp. dividing) the
        // two numbers.
        // This is possible because IEEE guarantees that floating-point operations
        // return the best possible approximation.
        if (exponent < 0 && -exponent < Strtod_kExactPowersOfTenSize) {
            // 10^-exponent fits into a double.
            *result = (double)(Strtod_ReadUint64(trimmed, &read_digits));
            DOUBLE_CONVERSION_ASSERT(read_digits == trimmed->length);
            *result /= Strtod_exact_powers_of_ten[-exponent];
            return true;
        }
        if (0 <= exponent && exponent < Strtod_kExactPowersOfTenSize) {
            // 10^exponent fits into a double.
            *result = (double)(Strtod_ReadUint64(trimmed, &read_digits));
            DOUBLE_CONVERSION_ASSERT(read_digits == trimmed->length);
            *result *= Strtod_exact_powers_of_ten[exponent];
            return true;
        }
        int remaining_digits = Strtod_kMaxExactDoubleIntegerDecimalDigits - trimmed->length;
        if ((0 <= exponent) && (exponent - remaining_digits < Strtod_kExactPowersOfTenSize)) {
            // The trimmed string was short and we can multiply it with
            // 10^remaining_digits. As a result the remaining exponent now fits
            // into a double too.
            *result = (double)(Strtod_ReadUint64(trimmed, &read_digits));
            DOUBLE_CONVERSION_ASSERT(read_digits == trimmed->length);
            *result *= Strtod_exact_powers_of_ten[remaining_digits];
            *result *= Strtod_exact_powers_of_ten[exponent - remaining_digits];
            return true;
        }
    }
    return false;
#endif
}

// Returns 10^exponent as an exact DiyFp.
// The given exponent must be in the range [1; kDecimalExponentDistance[.
static DiyFp Strtod_AdjustmentPowerOfTen(int exponent) {
    DOUBLE_CONVERSION_ASSERT(0 < exponent);
    DOUBLE_CONVERSION_ASSERT(exponent < Cache_kDecimalExponentDistance);
    // Simply hardcode the remaining powers for the given decimal exponent
    // distance.
    DOUBLE_CONVERSION_ASSERT(Cache_kDecimalExponentDistance == 8);
    switch (exponent) {
        case 1: return DiyFp_make(DOUBLE_CONVERSION_UINT64_2PART_C(0xa0000000, 00000000), -60);
        case 2: return DiyFp_make(DOUBLE_CONVERSION_UINT64_2PART_C(0xc8000000, 00000000), -57);
        case 3: return DiyFp_make(DOUBLE_CONVERSION_UINT64_2PART_C(0xfa000000, 00000000), -54);
        case 4: return DiyFp_make(DOUBLE_CONVERSION_UINT64_2PART_C(0x9c400000, 00000000), -50);
        case 5: return DiyFp_make(DOUBLE_CONVERSION_UINT64_2PART_C(0xc3500000, 00000000), -47);
        case 6: return DiyFp_make(DOUBLE_CONVERSION_UINT64_2PART_C(0xf4240000, 00000000), -44);
        case 7: return DiyFp_make(DOUBLE_CONVERSION_UINT64_2PART_C(0x98968000, 00000000), -40);
        default:
          DOUBLE_CONVERSION_UNREACHABLE();
    }
}

// If the function returns true then the result is the correct double.
// Otherwise it is either the correct double or the double that is just below
// the correct double.
static bool Strtod_DiyFpStrtod(Vector *buffer,
                               int exponent,
                               double *result) {
    DiyFp input = { 0 };
    int remaining_decimals;
    Strtod_ReadDiyFp(buffer, &input, &remaining_decimals);
    // Since we may have dropped some digits the input is not accurate.
    // If remaining_decimals is different than 0 than the error is at most
    // .5 ulp (unit in the last place).
    // We don't want to deal with fractions and therefore keep a common
    // denominator.
    const int kDenominatorLog = 3;
    const int kDenominator = 1 << kDenominatorLog;
    // Move the remaining decimals into the exponent.
    exponent += remaining_decimals;
    uint64_t error = (remaining_decimals == 0 ? 0 : kDenominator / 2);

    int old_e = input.e;
    DiyFp_Normalize(&input);
    error <<= old_e - input.e;

    DOUBLE_CONVERSION_ASSERT(exponent <= Cache_kMaxDecimalExponent);
    if (exponent < Cache_kMinDecimalExponent) {
        *result = 0.0;
        return true;
    }
    DiyFp cached_power;
    int cached_decimal_exponent;
    Cache_GetCachedPowerForDecimalExponent(exponent,
                                           &cached_power,
                                           &cached_decimal_exponent);
    
    if (cached_decimal_exponent != exponent) {
        int adjustment_exponent = exponent - cached_decimal_exponent;
        DiyFp adjustment_power = Strtod_AdjustmentPowerOfTen(adjustment_exponent);
        DiyFp_Multiply(&input, &adjustment_power);
        if (Strtod_kMaxUint64DecimalDigits - buffer->length >= adjustment_exponent) {
            // The product of input with the adjustment power fits into a 64 bit
            // integer.
            DOUBLE_CONVERSION_ASSERT(DiyFp_kSignificandSize == 64);
        } else {
            // The adjustment power is exact. There is hence only an error of 0.5.
            error += kDenominator / 2;
        }
    }
    
    DiyFp_Multiply(&input, &cached_power);
    // The error introduced by a multiplication of a*b equals
    //   error_a + error_b + error_a*error_b/2^64 + 0.5
    // Substituting a with 'input' and b with 'cached_power' we have
    //   error_b = 0.5  (all cached powers have an error of less than 0.5 ulp),
    //   error_ab = 0 or 1 / kDenominator > error_a*error_b/ 2^64
    int error_b = kDenominator / 2;
    int error_ab = (error == 0 ? 0 : 1);  // We round up to 1.
    int fixed_error = kDenominator / 2;
    error += error_b + error_ab + fixed_error;
    
    old_e = input.e;
    DiyFp_Normalize(&input);
    error <<= old_e - input.e;
    
    // See if the double's significand changes if we add/subtract the error.
    int order_of_magnitude = DiyFp_kSignificandSize + input.e;
    int effective_significand_size =
        Double_SignificandSizeForOrderOfMagnitude(order_of_magnitude);
    int precision_digits_count =
        DiyFp_kSignificandSize - effective_significand_size;
    if (precision_digits_count + kDenominatorLog >= DiyFp_kSignificandSize) {
        // This can only happen for very small denormals. In this case the
        // half-way multiplied by the denominator exceeds the range of an uint64.
        // Simply shift everything to the right.
        int shift_amount = (precision_digits_count + kDenominatorLog) -
            DiyFp_kSignificandSize + 1;
        input.f =(input.f >> shift_amount);
        input.e = (input.e + shift_amount);
        // We add 1 for the lost precision of error, and kDenominator for
        // the lost precision of input.f().
        error = (error >> shift_amount) + 1 + kDenominator;
        precision_digits_count -= shift_amount;
    }
    // We use uint64_ts now. This only works if the DiyFp uses uint64_ts too.
    DOUBLE_CONVERSION_ASSERT(DiyFp_kSignificandSize == 64);
    DOUBLE_CONVERSION_ASSERT(precision_digits_count < 64);
    uint64_t one64 = 1;
    uint64_t precision_bits_mask = (one64 << precision_digits_count) - 1;
    uint64_t precision_bits = input.f & precision_bits_mask;
    uint64_t half_way = one64 << (precision_digits_count - 1);
    precision_bits *= kDenominator;
    half_way *= kDenominator;
    DiyFp rounded_input = DiyFp_make(input.f >> precision_digits_count,
                                     input.e + precision_digits_count);
    if (precision_bits >= half_way + error) {
        rounded_input.f = (rounded_input.f + 1);
    }
    // If the last_bits are too close to the half-way case than we are too
    // inaccurate and round down. In this case we return false so that we can
    // fall back to a more precise algorithm.
    
    Double d = Double_make_diyfp(&rounded_input);
    *result = Double_value(&d);
    if (half_way - error < precision_bits && precision_bits < half_way + error) {
        // Too imprecise. The caller will have to fall back to a slower version.
        // However the returned number is guaranteed to be either the correct
        // double, or the next-lower double.
        return false;
    } else {
        return true;
    }
}

// Returns
//   - -1 if buffer*10^exponent < diy_fp.
//   -  0 if buffer*10^exponent == diy_fp.
//   - +1 if buffer*10^exponent > diy_fp.
// Preconditions:
//   buffer.length() + exponent <= kMaxDecimalPower + 1
//   buffer.length() + exponent > kMinDecimalPower
//   buffer.length() <= kMaxDecimalSignificantDigits
static int Strtod_CompareBufferWithDiyFp(Vector *buffer,
                                         int exponent,
                                         DiyFp *diy_fp) {
    DOUBLE_CONVERSION_ASSERT(buffer->length + exponent <= Strtod_kMaxDecimalPower + 1);
    DOUBLE_CONVERSION_ASSERT(buffer->length + exponent > Strtod_kMinDecimalPower);
    DOUBLE_CONVERSION_ASSERT(buffer->length <= Strtod_kMaxSignificantDecimalDigits);
    // Make sure that the Bignum will be able to hold all our numbers.
    // Our Bignum implementation has a separate field for exponents. Shifts will
    // consume at most one bigit (< 64 bits).
    // ln(10) == 3.3219...
    DOUBLE_CONVERSION_ASSERT(((Strtod_kMaxDecimalPower + 1) * 333 / 100) < Bignum_kMaxSignificantBits);
    Bignum buffer_bignum = { 0 };
    Bignum diy_fp_bignum = { 0 };
    Bignum_AssignDecimalString(& buffer_bignum, buffer);
    Bignum_AssignUInt64(&diy_fp_bignum, diy_fp->f);
    if (exponent >= 0) {
        Bignum_MultiplyByPowerOfTen(&buffer_bignum, exponent);
    } else {
        Bignum_MultiplyByPowerOfTen(&diy_fp_bignum, -exponent);
    }
    if (diy_fp->e > 0) {
        Bignum_ShiftLeft(&diy_fp_bignum, diy_fp->e);
    } else {
        Bignum_ShiftLeft(&buffer_bignum, -diy_fp->e);
    }
    return Bignum_Compare(&buffer_bignum, &diy_fp_bignum);
}

// Returns true if the guess is the correct double.
// Returns false, when guess is either correct or the next-lower double.
static bool Strtod_ComputeGuess(Vector *trimmed, int exponent,
                                double *guess) {
    if (trimmed->length == 0) {
        *guess = 0.0;
        return true;
    }
    if (exponent + trimmed->length - 1 >= Strtod_kMaxDecimalPower) {
        *guess = Double_Infinity();
        return true;
    }
    if (exponent + trimmed->length <= Strtod_kMinDecimalPower) {
    *guess = 0.0;
    return true;
    }

    if (Strtod_DoubleStrtod(trimmed, exponent, guess) ||
        Strtod_DiyFpStrtod(trimmed, exponent, guess)) {
        return true;
    }
    if (*guess == Double_Infinity()) {
        return true;
    }
    return false;
}

static bool Strtod_IsDigit(const char d) {
    return ('0' <= d) && (d <= '9');
}

static bool Strtod_IsNonZeroDigit(const char d) {
    return ('1' <= d) && (d <= '9');
}

static bool Strtod_AssertTrimmedDigits(Vector *buffer) {
    for(int i = 0; i < buffer->length; ++i) {
        if(!Strtod_IsDigit(buffer->start[i])) {
            return false;
        }
    }
    return (buffer->length == 0) || (Strtod_IsNonZeroDigit(buffer->start[0]) &&     Strtod_IsNonZeroDigit(buffer->start[buffer->length-1]));
}

static double StrtodTrimmed(Vector *trimmed, int exponent) {
    DOUBLE_CONVERSION_ASSERT(trimmed->length <= Strtod_kMaxSignificantDecimalDigits);
    DOUBLE_CONVERSION_ASSERT(Strtod_AssertTrimmedDigits(trimmed));
    double guess;
    const bool is_correct = Strtod_ComputeGuess(trimmed, exponent, &guess);
    if (is_correct) {
        return guess;
    }
    Double dguess = Double_make(guess);
    DiyFp upper_boundary = Double_UpperBoundary(&dguess);
    int comparison = Strtod_CompareBufferWithDiyFp(trimmed, exponent, &upper_boundary);
    if (comparison < 0) {
        return guess;
    } else if (comparison > 0) {
        return Double_NextDouble(&dguess);
    } else if ((Double_Significand(&dguess) & 1) == 0) {
        // Round towards even.
        return guess;
    } else {
        return Double_NextDouble(&dguess);
    }
}

static double Strtod(Vector *buffer, int exponent) {
    char copy_buffer[Strtod_kMaxSignificantDecimalDigits];
    Vector trimmed = { 0 };
    int updated_exponent;
    Strtod_TrimAndCut(buffer, exponent, copy_buffer, Strtod_kMaxSignificantDecimalDigits,
               &trimmed, &updated_exponent);
    return StrtodTrimmed(&trimmed, updated_exponent);
}

static float Strtod_SanitizedDoubletof(double d) {
    DOUBLE_CONVERSION_ASSERT(d >= 0.0);
    // ASAN has a sanitize check that disallows casting doubles to floats if
    // they are too big.
    // https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html#available-checks
    // The behavior should be covered by IEEE 754, but some projects use this
    // flag, so work around it.
    float max_finite = 3.4028234663852885981170418348451692544e+38;
    // The half-way point between the max-finite and infinity value.
    // Since infinity has an even significand everything equal or greater than
    // this value should become infinity.
    double half_max_finite_infinity = 3.40282356779733661637539395458142568448e+38;
    if (d >= max_finite) {
        if (d >= half_max_finite_infinity) {
            return Single_Infinity();
        } else {
            return max_finite;
        }
    } else {
        return (float)(d);
    }
}

static float Strtof(Vector *buffer, int exponent) {
    char copy_buffer[Strtod_kMaxSignificantDecimalDigits];
    Vector trimmed = { 0 };
    int updated_exponent;
    Strtod_TrimAndCut(buffer, exponent, copy_buffer, Strtod_kMaxSignificantDecimalDigits,
                      &trimmed, &updated_exponent);
    exponent = updated_exponent;
    return StrtofTrimmed(&trimmed, exponent);
}

static float StrtofTrimmed(Vector *trimmed, int exponent) {
    DOUBLE_CONVERSION_ASSERT(trimmed->length <= Strtod_kMaxSignificantDecimalDigits);
    DOUBLE_CONVERSION_ASSERT(Strtod_AssertTrimmedDigits(trimmed));

    double double_guess = 0;
    bool is_correct = Strtod_ComputeGuess(trimmed, exponent, &double_guess);
    
    float float_guess = Strtod_SanitizedDoubletof(double_guess);
    if (float_guess == double_guess) {
        // This shortcut triggers for integer values.
        return float_guess;
    }
    
    // We must catch double-rounding. Say the double has been rounded up, and is
    // now a boundary of a float, and rounds up again. This is why we have to
    // look at previous too.
    // Example (in decimal numbers):
    //    input: 12349
    //    high-precision (4 digits): 1235
    //    low-precision (3 digits):
    //       when read from input: 123
    //       when rounded from high precision: 124.
    // To do this we simply look at the neighbors of the correct result and see
    // if they would round to the same float. If the guess is not correct we have
    // to look at four values (since two different doubles could be the correct
    // double).
    Double d = Double_make(double_guess);
    double double_next = Double_NextDouble(&d);
    double double_previous = Double_PreviousDouble(&d);
    
    float f1 = Strtod_SanitizedDoubletof(double_previous);
    float f2 = float_guess;
    float f3 = Strtod_SanitizedDoubletof(double_next);
    float f4;
    if (is_correct) {
        f4 = f3;
    } else {
        Double d2 = Double_make(double_next);
        double double_next2 = Double_NextDouble(&d2);
        f4 = Strtod_SanitizedDoubletof(double_next2);
    }
    (void) f2;  // Mark variable as used.
    DOUBLE_CONVERSION_ASSERT(f1 <= f2 && f2 <= f3 && f3 <= f4);

    // If the guess doesn't lie near a single-precision boundary we can simply
    // return its float-value.
    if (f1 == f4) {
        return float_guess;
    }
    
    DOUBLE_CONVERSION_ASSERT((f1 != f2 && f2 == f3 && f3 == f4) ||
         (f1 == f2 && f2 != f3 && f3 == f4) ||
         (f1 == f2 && f2 == f3 && f3 != f4));
    
    // guess and next are the two possible candidates (in the same way that
    // double_guess was the lower candidate for a double-precision guess).
    float guess = f1;
    float next = f4;
    DiyFp upper_boundary;
    Single s3 = Single_make(guess);
    if (guess == 0.0f) {
        float min_float = 1e-45f;
        Double d3 = Double_make((double)(min_float) / 2);
        upper_boundary = Double_AsDiyFp(&d3);
    } else {
        upper_boundary = Single_UpperBoundary(&s3);
    }
    int comparison = Strtod_CompareBufferWithDiyFp(trimmed, exponent, &upper_boundary);
    if (comparison < 0) {
        return guess;
    } else if (comparison > 0) {
        return next;
    } else if ((Single_Significand(&s3) & 1) == 0) {
        // Round towards even.
        return guess;
    } else {
        return next;
    }
}



/// ============================================================================
/// string-to-double.h
/// ============================================================================

// Enumeration for allowing octals and ignoring junk when converting
// strings to numbers.
enum S2D_Flags {
    S2D_NO_FLAGS = 0,
    S2D_ALLOW_HEX = 1,
    S2D_ALLOW_OCTALS = 2,
    S2D_ALLOW_TRAILING_JUNK = 4,
    S2D_ALLOW_LEADING_SPACES = 8,
    S2D_ALLOW_TRAILING_SPACES = 16,
    S2D_ALLOW_SPACES_AFTER_SIGN = 32,
    S2D_ALLOW_CASE_INSENSITIVITY = 64,
    S2D_ALLOW_CASE_INSENSIBILITY = 64,  // Deprecated
    S2D_ALLOW_HEX_FLOATS = 128,
};

typedef struct StringToDoubleConverter {
    int flags;
    double empty_string_value;
    double junk_string_value;
    const char *infinity_symbol;
    const char *nan_symbol;
    uc16 separator;
} StringToDoubleConverter;

static const uc16 S2D_kNoSeparator = '\0';

// Flags should be a bit-or combination of the possible Flags-enum.
//  - NO_FLAGS: no special flags.
//  - ALLOW_HEX: recognizes the prefix "0x". Hex numbers may only be integers.
//      Ex: StringToDouble("0x1234") -> 4660.0
//          In StringToDouble("0x1234.56") the characters ".56" are trailing
//          junk. The result of the call is hence dependent on
//          the ALLOW_TRAILING_JUNK flag and/or the junk value.
//      With this flag "0x" is a junk-string. Even with ALLOW_TRAILING_JUNK,
//      the string will not be parsed as "0" followed by junk.
//
//  - ALLOW_OCTALS: recognizes the prefix "0" for octals:
//      If a sequence of octal digits starts with '0', then the number is
//      read as octal integer. Octal numbers may only be integers.
//      Ex: StringToDouble("01234") -> 668.0
//          StringToDouble("012349") -> 12349.0  // Not a sequence of octal
//                                               // digits.
//          In StringToDouble("01234.56") the characters ".56" are trailing
//          junk. The result of the call is hence dependent on
//          the ALLOW_TRAILING_JUNK flag and/or the junk value.
//          In StringToDouble("01234e56") the characters "e56" are trailing
//          junk, too.
//  - ALLOW_TRAILING_JUNK: ignore trailing characters that are not part of
//      a double literal.
//  - ALLOW_LEADING_SPACES: skip over leading whitespace, including spaces,
//                          new-lines, and tabs.
//  - ALLOW_TRAILING_SPACES: ignore trailing whitespace.
//  - ALLOW_SPACES_AFTER_SIGN: ignore whitespace after the sign.
//       Ex: StringToDouble("-   123.2") -> -123.2.
//           StringToDouble("+   123.2") -> 123.2
//  - ALLOW_CASE_INSENSITIVITY: ignore case of characters for special values:
//      infinity and nan.
//  - ALLOW_HEX_FLOATS: allows hexadecimal float literals.
//      This *must* start with "0x" and separate the exponent with "p".
//      Examples: 0x1.2p3 == 9.0
//                0x10.1p0 == 16.0625
//      ALLOW_HEX and ALLOW_HEX_FLOATS are indented.
//
// empty_string_value is returned when an empty string is given as input.
// If ALLOW_LEADING_SPACES or ALLOW_TRAILING_SPACES are set, then a string
// containing only spaces is converted to the 'empty_string_value', too.
//
// junk_string_value is returned when
//  a) ALLOW_TRAILING_JUNK is not set, and a junk character (a character not
//     part of a double-literal) is found.
//  b) ALLOW_TRAILING_JUNK is set, but the string does not start with a
//     double literal.
//
// infinity_symbol and nan_symbol are strings that are used to detect
// inputs that represent infinity and NaN. They can be null, in which case
// they are ignored.
// The conversion routine first reads any possible signs. Then it compares the
// following character of the input-string with the first character of
// the infinity, and nan-symbol. If either matches, the function assumes, that
// a match has been found, and expects the following input characters to match
// the remaining characters of the special-value symbol.
// This means that the following restrictions apply to special-value symbols:
//  - they must not start with signs ('+', or '-'),
//  - they must not have the same first character.
//  - they must not start with digits.
//
// If the separator character is not kNoSeparator, then that specific
// character is ignored when in between two valid digits of the significant.
// It is not allowed to appear in the exponent.
// It is not allowed to lead or trail the number.
// It is not allowed to appear twice next to each other.
//
// Examples:
//  flags = ALLOW_HEX | ALLOW_TRAILING_JUNK,
//  empty_string_value = 0.0,
//  junk_string_value = NaN,
//  infinity_symbol = "infinity",
//  nan_symbol = "nan":
//    StringToDouble("0x1234") -> 4660.0.
//    StringToDouble("0x1234K") -> 4660.0.
//    StringToDouble("") -> 0.0  // empty_string_value.
//    StringToDouble(" ") -> NaN  // junk_string_value.
//    StringToDouble(" 1") -> NaN  // junk_string_value.
//    StringToDouble("0x") -> NaN  // junk_string_value.
//    StringToDouble("-123.45") -> -123.45.
//    StringToDouble("--123.45") -> NaN  // junk_string_value.
//    StringToDouble("123e45") -> 123e45.
//    StringToDouble("123E45") -> 123e45.
//    StringToDouble("123e+45") -> 123e45.
//    StringToDouble("123E-45") -> 123e-45.
//    StringToDouble("123e") -> 123.0  // trailing junk ignored.
//    StringToDouble("123e-") -> 123.0  // trailing junk ignored.
//    StringToDouble("+NaN") -> NaN  // NaN string literal.
//    StringToDouble("-infinity") -> -inf.  // infinity literal.
//    StringToDouble("Infinity") -> NaN  // junk_string_value.
//
//  flags = ALLOW_OCTAL | ALLOW_LEADING_SPACES,
//  empty_string_value = 0.0,
//  junk_string_value = NaN,
//  infinity_symbol = NULL,
//  nan_symbol = NULL:
//    StringToDouble("0x1234") -> NaN  // junk_string_value.
//    StringToDouble("01234") -> 668.0.
//    StringToDouble("") -> 0.0  // empty_string_value.
//    StringToDouble(" ") -> 0.0  // empty_string_value.
//    StringToDouble(" 1") -> 1.0
//    StringToDouble("0x") -> NaN  // junk_string_value.
//    StringToDouble("0123e45") -> NaN  // junk_string_value.
//    StringToDouble("01239E45") -> 1239e45.
//    StringToDouble("-infinity") -> NaN  // junk_string_value.
//    StringToDouble("NaN") -> NaN  // junk_string_value.
//
//  flags = NO_FLAGS,
//  separator = ' ':
//    StringToDouble("1 2 3 4") -> 1234.0
//    StringToDouble("1  2") -> NaN // junk_string_value
//    StringToDouble("1 000 000.0") -> 1000000.0
//    StringToDouble("1.000 000") -> 1.0
//    StringToDouble("1.0e1 000") -> NaN // junk_string_value
static StringToDoubleConverter StringToDoubleConverter_make(int flags,
                                                            double empty_string_value,
                                                            double junk_string_value,
                                                            const char *infinity_symbol,
                                                            const char *nan_symbol,
                                                            uc16 separator) {
    StringToDoubleConverter s;
    s.flags = flags;
    s.empty_string_value = empty_string_value;
    s.junk_string_value = junk_string_value;
    s.infinity_symbol = infinity_symbol;
    s.nan_symbol = nan_symbol;
    s.separator = separator;
    return s;
}

// Performs the conversion.
// The output parameter 'processed_characters_count' is set to the number
// of characters that have been processed to read the number.
// Spaces than are processed with ALLOW_{LEADING|TRAILING}_SPACES are included
// in the 'processed_characters_count'. Trailing junk is never included.
static double StringToDouble(StringToDoubleConverter *conv,
                             const char *buffer,
                             int length,
                             int *processed_characters_count);

// Same as StringToDouble but reads a float.
// Note that this is not equivalent to static_cast<float>(StringToDouble(...))
// due to potential double-rounding.
static float StringToFloat(StringToDoubleConverter *conv,
                           const char *buffer,
                           int length,
                           int *processed_characters_count);

static double StringToIeee(StringToDoubleConverter *conv,
                           Iterator start_pointer,
                           int length,
                           bool read_as_double,
                           int *processed_characters_count);



/// ============================================================================
/// string-to-double.cc
/// ============================================================================

#ifdef _MSC_VER
#  if _MSC_VER >= 1900
// Fix MSVC >= 2015 (_MSC_VER == 1900) warning
// C4244: 'argument': conversion from 'const uc16' to 'char', possible loss of data
// against Advance and friends, when instantiated with **it as char, not uc16.
 __pragma(warning(disable: 4244))
#  endif
#  if _MSC_VER <= 1700 // VS2012, see IsDecimalDigitForRadix warning fix, below
#    define VS2012_RADIXWARN
#  endif
#endif

typedef char (*S2D_Converter)(char);

static char S2D_ToLower(char ch) {
    if ('A' <= ch && ch <= 'Z') return ch + ('a' - 'A');
    return ch;
}

static char S2D_Pass(char ch) {
    return ch;
}

static bool S2D_ConsumeSubStringImpl(Iterator *current,
                                     Iterator end,
                                     const char *substring,
                                     S2D_Converter converter) {
    DOUBLE_CONVERSION_ASSERT(converter(**current) == *substring);
    for (substring++; *substring != '\0'; substring++) {
        ++*current;
        if (*current == end || converter(**current) != *substring) {
            return false;
        }
    }
    ++*current;
    return true;
}

// Consumes the given substring from the iterator.
// Returns false, if the substring does not match.
static bool S2D_ConsumeSubString(Iterator *current,
                                 Iterator end,
                                 const char *substring,
                                 bool allow_case_insensitivity) {
    if (allow_case_insensitivity) {
        return S2D_ConsumeSubStringImpl(current, end, substring, S2D_ToLower);
    } else {
        return S2D_ConsumeSubStringImpl(current, end, substring, S2D_Pass);
    }
}

// Consumes first character of the str is equal to ch
static bool S2D_ConsumeFirstCharacter(char ch,
                                      const char *str,
                                      bool case_insensitivity) {
    return case_insensitivity ? S2D_ToLower(ch) == str[0] : ch == str[0];
}

// Maximum number of significant digits in decimal representation.
// The longest possible double in decimal representation is
// (2^53 - 1) * 2 ^ -1074 that is (2 ^ 53 - 1) * 5 ^ 1074 / 10 ^ 1074
// (768 digits). If we parse a number whose first digits are equal to a
// mean of 2 adjacent doubles (that could have up to 769 digits) the result
// must be rounded to the bigger one unless the tail consists of zeros, so
// we don't need to preserve all the digits.
#define S2D_kMaxSignificantDigits ((int)772)

static const char S2D_kWhitespaceTable7[] = { 32, 13, 10, 9, 11, 12 };
static const int S2D_kWhitespaceTable7Length = DOUBLE_CONVERSION_ARRAY_SIZE(S2D_kWhitespaceTable7);

static const uc16 S2D_kWhitespaceTable16[] = {
  160, 8232, 8233, 5760, 6158, 8192, 8193, 8194, 8195,
  8196, 8197, 8198, 8199, 8200, 8201, 8202, 8239, 8287, 12288, 65279
};
static const int S2D_kWhitespaceTable16Length = DOUBLE_CONVERSION_ARRAY_SIZE(S2D_kWhitespaceTable16);

static bool S2D_isWhitespace(int x) {
    if (x < 128) {
        for (int i = 0; i < S2D_kWhitespaceTable7Length; i++) {
            if (S2D_kWhitespaceTable7[i] == x) return true;
        }
    } else {
        for (int i = 0; i < S2D_kWhitespaceTable16Length; i++) {
            if (S2D_kWhitespaceTable16[i] == x) return true;
        }
    }
    return false;
}

// Returns true if a nonspace found and false if the end has reached.
static bool S2D_AdvanceToNonspace(Iterator *current, Iterator end) {
    while (*current != end) {
        if (!S2D_isWhitespace(**current)) return true;
        ++*current;
    }
    return false;
}

static bool S2D_isDigit(int x, int radix) {
    return (x >= '0' && x <= '9' && x < '0' + radix)
        || (radix > 10 && x >= 'a' && x < 'a' + radix - 10)
        || (radix > 10 && x >= 'A' && x < 'A' + radix - 10);
}

static double S2D_SignedZero(bool sign) {
    return sign ? -0.0 : 0.0;
}

// Returns true if 'c' is a decimal digit that is valid for the given radix.
//
// The function is small and could be inlined, but VS2012 emitted a warning
// because it constant-propagated the radix and concluded that the last
// condition was always true. Moving it into a separate function and
// suppressing optimisation keeps the compiler from warning.
#ifdef VS2012_RADIXWARN
#pragma optimize("",off)
static bool S2D_IsDecimalDigitForRadix(int c, int radix) {
    return '0' <= c && c <= '9' && (c - '0') < radix;
}
#pragma optimize("",on)
#else
static bool inline S2D_IsDecimalDigitForRadix(int c, int radix) {
    return '0' <= c && c <= '9' && (c - '0') < radix;
}
#endif

// Returns true if 'c' is a character digit that is valid for the given radix.
// The 'a_character' should be 'a' or 'A'.
//
// The function is small and could be inlined, but VS2012 emitted a warning
// because it constant-propagated the radix and concluded that the first
// condition was always false. By moving it into a separate function the
// compiler wouldn't warn anymore.
static bool S2D_IsCharacterDigitForRadix(int c, int radix, char a_character) {
    return radix > 10 && c >= a_character && c < a_character + radix - 10;
}

// Returns true, when the iterator is equal to end.
static bool S2D_Advance(Iterator *it, uc16 separator, int base, Iterator end) {
    if (separator == S2D_kNoSeparator) {
        ++(*it);
        return *it == end;
    }
    if (!S2D_isDigit(**it, base)) {
        ++(*it);
        return *it == end;
    }
    ++(*it);
    if (*it == end) return true;
    if (*it + 1 == end) return false;
    if (**it == separator && S2D_isDigit(*(*it + 1), base)) {
        ++(*it);
    }
    return *it == end;
}

// Checks whether the string in the range start-end is a hex-float string.
// This function assumes that the leading '0x'/'0X' is already consumed.
//
// Hex float strings are of one of the following forms:
//   - hex_digits+ 'p' ('+'|'-')? exponent_digits+
//   - hex_digits* '.' hex_digits+ 'p' ('+'|'-')? exponent_digits+
//   - hex_digits+ '.' 'p' ('+'|'-')? exponent_digits+
static bool S2D_IsHexFloatString(Iterator start,
                                 Iterator end,
                                 uc16 separator,
                                 bool allow_trailing_junk) {
    DOUBLE_CONVERSION_ASSERT(start != end);
    
    Iterator current = start;
    
    bool saw_digit = false;
    while (S2D_isDigit(*current, 16)) {
        saw_digit = true;
        if (S2D_Advance(&current, separator, 16, end)) return false;
    }
    if (*current == '.') {
        if (S2D_Advance(&current, separator, 16, end)) return false;
        while (S2D_isDigit(*current, 16)) {
            saw_digit = true;
            if (S2D_Advance(&current, separator, 16, end)) return false;
        }
    }
    if (!saw_digit) return false;
    if (*current != 'p' && *current != 'P') return false;
    if (S2D_Advance(&current, separator, 16, end)) return false;
    if (*current == '+' || *current == '-') {
        if (S2D_Advance(&current, separator, 16, end)) return false;
    }
    if (!S2D_isDigit(*current, 10)) return false;
    if (S2D_Advance(&current, separator, 16, end)) return true;
    while (S2D_isDigit(*current, 10)) {
        if (S2D_Advance(&current, separator, 16, end)) return true;
    }
    return allow_trailing_junk || !S2D_AdvanceToNonspace(&current, end);
}

// Parsing integers with radix 2, 4, 8, 16, 32. Assumes current != end.
//
// If parse_as_hex_float is true, then the string must be a valid
// hex-float.
static double S2D_RadixStringToIeee(int radix_log_2,
                                    Iterator *current,
                                    Iterator end,
                                    bool sign,
                                    uc16 separator,
                                    bool parse_as_hex_float,
                                    bool allow_trailing_junk,
                                    double junk_string_value,
                                    bool read_as_double,
                                    bool *result_is_junk) {
    DOUBLE_CONVERSION_ASSERT(*current != end);
    DOUBLE_CONVERSION_ASSERT(!parse_as_hex_float ||
        S2D_IsHexFloatString(*current, end, separator, allow_trailing_junk));
    
    const int kDoubleSize = Double_kSignificandSize;
    const int kSingleSize = Single_kSignificandSize;
    const int kSignificandSize = read_as_double? kDoubleSize: kSingleSize;
    
    *result_is_junk = true;
    
    int64_t number = 0;
    int exponent = 0;
    const int radix = (1 << radix_log_2);
    // Whether we have encountered a '.' and are parsing the decimal digits.
    // Only relevant if parse_as_hex_float is true.
    bool post_decimal = false;

    // Skip leading 0s.
    while (**current == '0') {
        if (S2D_Advance(current, separator, radix, end)) {
            *result_is_junk = false;
            return S2D_SignedZero(sign);
        }
    }
    
    while (true) {
        int digit;
        if (S2D_IsDecimalDigitForRadix(**current, radix)) {
            digit = (char)(**current) - '0';
            if (post_decimal) exponent -= radix_log_2;
        } else if (S2D_IsCharacterDigitForRadix(**current, radix, 'a')) {
            digit = (char)(**current) - 'a' + 10;
            if (post_decimal) exponent -= radix_log_2;
        } else if (S2D_IsCharacterDigitForRadix(**current, radix, 'A')) {
            digit = (char)(**current) - 'A' + 10;
            if (post_decimal) exponent -= radix_log_2;
        } else if (parse_as_hex_float && **current == '.') {
            post_decimal = true;
            S2D_Advance(current, separator, radix, end);
            DOUBLE_CONVERSION_ASSERT(*current != end);
            continue;
        } else if (parse_as_hex_float && (**current == 'p' || **current == 'P')) {
            break;
        } else {
            if (allow_trailing_junk || !S2D_AdvanceToNonspace(current, end)) {
                break;
          } else {
              return junk_string_value;
          }
        }
        
        number = number * radix + digit;
        int overflow = (int)(number >> kSignificandSize);
        if (overflow != 0) {
            // Overflow occurred. Need to determine which direction to round the
            // result.
            int overflow_bits_count = 1;
            while (overflow > 1) {
                overflow_bits_count++;
                overflow >>= 1;
            }
            
            int dropped_bits_mask = ((1 << overflow_bits_count) - 1);
            int dropped_bits = (int)(number) & dropped_bits_mask;
            number >>= overflow_bits_count;
            exponent += overflow_bits_count;
            
            bool zero_tail = true;
            for (;;) {
                if (S2D_Advance(current, separator, radix, end)) break;
                if (parse_as_hex_float && **current == '.') {
                    // Just run over the '.'. We are just trying to see whether there is
                    // a non-zero digit somewhere.
                    S2D_Advance(current, separator, radix, end);
                    DOUBLE_CONVERSION_ASSERT(*current != end);
                    post_decimal = true;
                }
                if (!S2D_isDigit(**current, radix)) break;
                zero_tail = zero_tail && **current == '0';
                if (!post_decimal) exponent += radix_log_2;
            }
            
            if (!parse_as_hex_float &&
                !allow_trailing_junk &&
                S2D_AdvanceToNonspace(current, end)) {
                return junk_string_value;
            }
            
            int middle_value = (1 << (overflow_bits_count - 1));
            if (dropped_bits > middle_value) {
                number++;  // Rounding up.
            } else if (dropped_bits == middle_value) {
                // Rounding to even to consistency with decimals: half-way case rounds
                // up if significant part is odd and down otherwise.
                if ((number & 1) != 0 || !zero_tail) {
                    number++;  // Rounding up.
                }
            }
            
            // Rounding up may cause overflow.
            if ((number & ((int64_t)1 << kSignificandSize)) != 0) {
                exponent++;
                number >>= 1;
            }
            break;
        }
        if (S2D_Advance(current, separator, radix, end)) break;
    }
    
    DOUBLE_CONVERSION_ASSERT(number < ((int64_t)1 << kSignificandSize));
    DOUBLE_CONVERSION_ASSERT((int64_t)((double)(number)) == number);
    
    *result_is_junk = false;
    
    if (parse_as_hex_float) {
        DOUBLE_CONVERSION_ASSERT(**current == 'p' || **current == 'P');
        S2D_Advance(current, separator, radix, end);
        DOUBLE_CONVERSION_ASSERT(*current != end);
        bool is_negative = false;
        if (**current == '+') {
            S2D_Advance(current, separator, radix, end);
            DOUBLE_CONVERSION_ASSERT(*current != end);
        } else if (**current == '-') {
            is_negative = true;
            S2D_Advance(current, separator, radix, end);
            DOUBLE_CONVERSION_ASSERT(*current != end);
        }
        int written_exponent = 0;
        while (S2D_IsDecimalDigitForRadix(**current, 10)) {
            // No need to read exponents if they are too big. That could potentially overflow
            // the `written_exponent` variable.
            if (abs(written_exponent) <= 100 * Double_kMaxExponent) {
                written_exponent = 10 * written_exponent + **current - '0';
            }
            if (S2D_Advance(current, separator, radix, end)) break;
        }
        if (is_negative) written_exponent = -written_exponent;
        exponent += written_exponent;
    }
    
    if (exponent == 0 || number == 0) {
        if (sign) {
            if (number == 0) return -0.0;
            number = -number;
        }
        return (double)(number);
    }
    
    DOUBLE_CONVERSION_ASSERT(number != 0);
    DiyFp diy = DiyFp_make(number, exponent);
    Double d = Double_make_diyfp(&diy);
    double result = Double_value(&d);
    return sign ? -result : result;
}

static double StringToIeee(StringToDoubleConverter *conv,
                           Iterator input,
                           int length,
                           bool read_as_double,
                           int *processed_characters_count) {
    Iterator current = input;
    Iterator end = input + length;
    
    *processed_characters_count = 0;
    
    const bool allow_trailing_junk = (conv->flags & S2D_ALLOW_TRAILING_JUNK) != 0;
    const bool allow_leading_spaces = (conv->flags & S2D_ALLOW_LEADING_SPACES) != 0;
    const bool allow_trailing_spaces = (conv->flags & S2D_ALLOW_TRAILING_SPACES) != 0;
    const bool allow_spaces_after_sign = (conv->flags & S2D_ALLOW_SPACES_AFTER_SIGN) != 0;
    const bool allow_case_insensitivity = (conv->flags & S2D_ALLOW_CASE_INSENSITIVITY) != 0;
    
    // To make sure that iterator dereferencing is valid the following
    // convention is used:
    // 1. Each '++current' statement is followed by check for equality to 'end'.
    // 2. If AdvanceToNonspace returned false then current == end.
    // 3. If 'current' becomes equal to 'end' the function returns or goes to
    // 'parsing_done'.
    // 4. 'current' is not dereferenced after the 'parsing_done' label.
    // 5. Code before 'parsing_done' may rely on 'current != end'.
    if (current == end) return conv->empty_string_value;
    
    if (allow_leading_spaces || allow_trailing_spaces) {
        if (!S2D_AdvanceToNonspace(&current, end)) {
            *processed_characters_count = (int)(current - input);
            return conv->empty_string_value;
        }
        if (!allow_leading_spaces && (input != current)) {
            // No leading spaces allowed, but AdvanceToNonspace moved forward.
            return conv->junk_string_value;
        }
    }
    
    // Exponent will be adjusted if insignificant digits of the integer part
    // or insignificant leading zeros of the fractional part are dropped.
    int exponent = 0;
    int significant_digits = 0;
    int insignificant_digits = 0;
    bool nonzero_digit_dropped = false;
    
    bool sign = false;
    
    if (*current == '+' || *current == '-') {
        sign = (*current == '-');
        ++current;
        Iterator next_non_space = current;
        // Skip following spaces (if allowed).
        if (!S2D_AdvanceToNonspace(&next_non_space, end)) return conv->junk_string_value;
        if (!allow_spaces_after_sign && (current != next_non_space)) {
            return conv->junk_string_value;
        }
        current = next_non_space;
    }
    
    if (conv->infinity_symbol != NULL) {
        if (S2D_ConsumeFirstCharacter(*current, conv->infinity_symbol, allow_case_insensitivity)) {
            if (!S2D_ConsumeSubString(&current, end, conv->infinity_symbol, allow_case_insensitivity)) {
                return conv->junk_string_value;
            }
            
            if (!(allow_trailing_spaces || allow_trailing_junk) && (current != end)) {
                return conv->junk_string_value;
            }
            if (!allow_trailing_junk && S2D_AdvanceToNonspace(&current, end)) {
                return conv->junk_string_value;
            }
            
            *processed_characters_count = (int)(current - input);
            return sign ? -Double_Infinity() : Double_Infinity();
        }
    }
    
    if (conv->nan_symbol != NULL) {
        if (S2D_ConsumeFirstCharacter(*current, conv->nan_symbol, allow_case_insensitivity)) {
            if (!S2D_ConsumeSubString(&current, end, conv->nan_symbol, allow_case_insensitivity)) {
                return conv->junk_string_value;
            }
            
            if (!(allow_trailing_spaces || allow_trailing_junk) && (current != end)) {
                return conv->junk_string_value;
            }
            if (!allow_trailing_junk && S2D_AdvanceToNonspace(&current, end)) {
                return conv->junk_string_value;
            }
            
            *processed_characters_count = (int)(current - input);
            return sign ? -Double_NaN() : Double_NaN();
        }
    }
    
    bool leading_zero = false;
    if (*current == '0') {
        if (S2D_Advance(&current, conv->separator, 10, end)) {
            *processed_characters_count = (int)(current - input);
            return S2D_SignedZero(sign);
        }
        
        leading_zero = true;
        
        // It could be hexadecimal value.
        if (((conv->flags & S2D_ALLOW_HEX) ||
             (conv->flags & S2D_ALLOW_HEX_FLOATS)) &&
            (*current == 'x' || *current == 'X')) {
            ++current;
            
            if (current == end) return conv->junk_string_value;  // "0x"
            
            bool parse_as_hex_float = (conv->flags & S2D_ALLOW_HEX_FLOATS) &&
                    S2D_IsHexFloatString(current, end, conv->separator, allow_trailing_junk);
            
            if (!parse_as_hex_float && !S2D_isDigit(*current, 16)) {
                return conv->junk_string_value;
            }
            
            bool result_is_junk;
            double result = S2D_RadixStringToIeee(4,
                                                  &current,
                                                  end,
                                                  sign,
                                                  conv->separator,
                                                  parse_as_hex_float,
                                                  allow_trailing_junk,
                                                  conv->junk_string_value,
                                                  read_as_double,
                                                  &result_is_junk);
            if (!result_is_junk) {
                if (allow_trailing_spaces) S2D_AdvanceToNonspace(&current, end);
                *processed_characters_count = (int)(current - input);
            }
            return result;
        }
        
        // Ignore leading zeros in the integer part.
        while (*current == '0') {
            if (S2D_Advance(&current, conv->separator, 10, end)) {
                *processed_characters_count = (int)(current - input);
                return S2D_SignedZero(sign);
            }
        }
    }
    
    bool octal = leading_zero && (conv->flags & S2D_ALLOW_OCTALS) != 0;
    
    // The longest form of simplified number is: "-<significant digits>.1eXXX\0".
    const int kBufferSize = S2D_kMaxSignificantDigits + 10;
    DOUBLE_CONVERSION_STACK_UNINITIALIZED char
        buffer[S2D_kMaxSignificantDigits + 10 /* kBufferSize */];
    int buffer_pos = 0;
    
    // Copy significant digits of the integer part (if any) to the buffer.
    while (*current >= '0' && *current <= '9') {
        if (significant_digits < S2D_kMaxSignificantDigits) {
            DOUBLE_CONVERSION_ASSERT(buffer_pos < kBufferSize);
            buffer[buffer_pos++] = (char)(*current);
            significant_digits++;
            // Will later check if it's an octal in the buffer.
        } else {
            insignificant_digits++;  // Move the digit into the exponential part.
            nonzero_digit_dropped = nonzero_digit_dropped || *current != '0';
        }
        octal = octal && *current < '8';
        if (S2D_Advance(&current, conv->separator, 10, end)) goto parsing_done;
    }
    
    if (significant_digits == 0) {
        octal = false;
    }
    
    if (*current == '.') {
        if (octal && !allow_trailing_junk) return conv->junk_string_value;
        if (octal) goto parsing_done;
        
        if (S2D_Advance(&current, conv->separator, 10, end)) {
            if (significant_digits == 0 && !leading_zero) {
                return conv->junk_string_value;
            } else {
                goto parsing_done;
            }
        }
        
        if (significant_digits == 0) {
            // octal = false;
            // Integer part consists of 0 or is absent. Significant digits start after
            // leading zeros (if any).
            while (*current == '0') {
                if (S2D_Advance(&current, conv->separator, 10, end)) {
                    *processed_characters_count = (int)(current - input);
                    return S2D_SignedZero(sign);
                }
                exponent--;  // Move this 0 into the exponent.
            }
        }
        
        // There is a fractional part.
        // We don't emit a '.', but adjust the exponent instead.
        while (*current >= '0' && *current <= '9') {
            if (significant_digits < S2D_kMaxSignificantDigits) {
                DOUBLE_CONVERSION_ASSERT(buffer_pos < kBufferSize);
                buffer[buffer_pos++] = (char)(*current);
                significant_digits++;
                exponent--;
            } else {
                // Ignore insignificant digits in the fractional part.
                nonzero_digit_dropped = nonzero_digit_dropped || *current != '0';
            }
            if (S2D_Advance(&current, conv->separator, 10, end)) goto parsing_done;
        }
    }
    
    if (!leading_zero && exponent == 0 && significant_digits == 0) {
        // If leading_zeros is true then the string contains zeros.
        // If exponent < 0 then string was [+-]\.0*...
        // If significant_digits != 0 the string is not equal to 0.
        // Otherwise there are no digits in the string.
        return conv->junk_string_value;
    }
    
    // Parse exponential part.
    if (*current == 'e' || *current == 'E') {
        if (octal && !allow_trailing_junk) return conv->junk_string_value;
        if (octal) goto parsing_done;
        Iterator junk_begin = current;
        ++current;
        if (current == end) {
            if (allow_trailing_junk) {
                current = junk_begin;
                goto parsing_done;
            } else {
                return conv->junk_string_value;
            }
        }
        char exponen_sign = '+';
        if (*current == '+' || *current == '-') {
            exponen_sign = (char)(*current);
            ++current;
            if (current == end) {
                if (allow_trailing_junk) {
                    current = junk_begin;
                    goto parsing_done;
                } else {
                    return conv->junk_string_value;
                }
            }
        }
        
        if (current == end || *current < '0' || *current > '9') {
            if (allow_trailing_junk) {
                current = junk_begin;
                goto parsing_done;
            } else {
                return conv->junk_string_value;
            }
        }
        
        const int max_exponent = INT_MAX / 2;
        DOUBLE_CONVERSION_ASSERT(-max_exponent / 2 <= exponent && exponent <= max_exponent / 2);
        int num = 0;
        do {
            // Check overflow.
            int digit = *current - '0';
            if (num >= max_exponent / 10
                && !(num == max_exponent / 10 && digit <= max_exponent % 10)) {
                num = max_exponent;
            } else {
                num = num * 10 + digit;
            }
            ++current;
        } while (current != end && *current >= '0' && *current <= '9');
        
        exponent += (exponen_sign == '-' ? -num : num);
    }
    
    if (!(allow_trailing_spaces || allow_trailing_junk) && (current != end)) {
        return conv->junk_string_value;
    }
    if (!allow_trailing_junk && S2D_AdvanceToNonspace(&current, end)) {
        return conv->junk_string_value;
    }
    if (allow_trailing_spaces) {
        S2D_AdvanceToNonspace(&current, end);
    }
    
parsing_done:
    exponent += insignificant_digits;
    
    if (octal) {
        double result;
        bool result_is_junk;
        char *start = buffer;
        result = S2D_RadixStringToIeee(3,
                                       (Iterator *)&start,
                                       buffer + buffer_pos,
                                       sign,
                                       conv->separator,
                                       false, // Don't parse as hex_float.
                                       allow_trailing_junk,
                                       conv->junk_string_value,
                                       read_as_double,
                                       &result_is_junk);
        DOUBLE_CONVERSION_ASSERT(!result_is_junk);
        *processed_characters_count = (int)(current - input);
        return result;
    }
    
    if (nonzero_digit_dropped) {
        buffer[buffer_pos++] = '1';
        exponent--;
    }
    
    DOUBLE_CONVERSION_ASSERT(buffer_pos < kBufferSize);
    buffer[buffer_pos] = '\0';
    
    // Code above ensures there are no leading zeros and the buffer has fewer than
    // kMaxSignificantDecimalDigits characters. Trim trailing zeros.
    Vector chars = Vector_make(buffer, buffer_pos);
    chars = Strtod_TrimTrailingZeros(&chars);
    exponent += buffer_pos - chars.length;
    
    double converted;
    if (read_as_double) {
        converted = StrtodTrimmed(&chars, exponent);
    } else {
        converted = StrtofTrimmed(&chars, exponent);
    }
    *processed_characters_count = (int)(current - input);
    return sign? -converted: converted;
}

static double StringToDouble(StringToDoubleConverter *conv,
                             const char* buffer,
                             int length,
                             int *processed_characters_count) {
    return StringToIeee(conv, buffer, length, true, processed_characters_count);
}

static float StringToFloat(StringToDoubleConverter *conv,
                           const char *buffer,
                           int length,
                           int *processed_characters_count) {
    return (float)(StringToIeee(conv, buffer, length, false, processed_characters_count));
}



/// ============================================================================
/// double-to-string.h
/// ============================================================================

typedef struct DoubleToStringConverter {
    int flags;
    const char *infinity_symbol;
    const char *nan_symbol;
    char exponent_character;
    int decimal_in_shortest_low;
    int decimal_in_shortest_high;
    int max_leading_padding_zeroes_in_precision_mode;
    int max_trailing_padding_zeroes_in_precision_mode;
    int min_exponent_width;
} DoubleToStringConverter;

// When calling ToFixed with a double > 10^kMaxFixedDigitsBeforePoint
// or a requested_digits parameter > kMaxFixedDigitsAfterPoint then the
// function returns false.
#define D2S_kMaxFixedDigitsBeforePoint ((int)60)
#define D2S_kMaxFixedDigitsAfterPoint ((int)100)

// When calling ToExponential with a requested_digits
// parameter > kMaxExponentialDigits then the function returns false.
#define D2S_kMaxExponentialDigits ((int)120)

// When calling ToPrecision with a requested_digits
// parameter < kMinPrecisionDigits or requested_digits > kMaxPrecisionDigits
// then the function returns false.
#define D2S_kMinPrecisionDigits  ((int)1)
#define D2S_kMaxPrecisionDigits ((int)120)

// The maximal number of digits that are needed to emit a double in base 10.
// A higher precision can be achieved by using more digits, but the shortest
// accurate representation of any double will never use more digits than
// kBase10MaximalLength.
// Note that DoubleToAscii null-terminates its input. So the given buffer
// should be at least kBase10MaximalLength + 1 characters long.
#define D2S_kBase10MaximalLength  ((int)17)

// The maximal number of digits that are needed to emit a single in base 10.
// A higher precision can be achieved by using more digits, but the shortest
// accurate representation of any single will never use more digits than
// kBase10MaximalLengthSingle.
#define D2S_kBase10MaximalLengthSingle ((int)9)

// The length of the longest string that 'ToShortest' can produce when the
// converter is instantiated with EcmaScript defaults (see
// 'EcmaScriptConverter')
// This value does not include the trailing '\0' character.
// This amount of characters is needed for negative values that hit the
// 'decimal_in_shortest_low' limit. For example: "-0.0000033333333333333333"
#define D2S_kMaxCharsEcmaScriptShortest ((int)25)

typedef enum D2S_Flags {
    D2S_NO_FLAGS = 0,
    D2S_EMIT_POSITIVE_EXPONENT_SIGN = 1,
    D2S_EMIT_TRAILING_DECIMAL_POINT = 2,
    D2S_EMIT_TRAILING_ZERO_AFTER_POINT = 4,
    D2S_UNIQUE_ZERO = 8,
    D2S_NO_TRAILING_ZERO = 16
} D2S_Flags;

// Flags should be a bit-or combination of the possible Flags-enum.
//  - NO_FLAGS: no special flags.
//  - EMIT_POSITIVE_EXPONENT_SIGN: when the number is converted into exponent
//    form, emits a '+' for positive exponents. Example: 1.2e+2.
//  - EMIT_TRAILING_DECIMAL_POINT: when the input number is an integer and is
//    converted into decimal format then a trailing decimal point is appended.
//    Example: 2345.0 is converted to "2345.".
//  - EMIT_TRAILING_ZERO_AFTER_POINT: in addition to a trailing decimal point
//    emits a trailing '0'-character. This flag requires the
//    EMIT_TRAILING_DECIMAL_POINT flag.
//    Example: 2345.0 is converted to "2345.0".
//  - UNIQUE_ZERO: "-0.0" is converted to "0.0".
//  - NO_TRAILING_ZERO: Trailing zeros are removed from the fractional portion
//    of the result in precision mode. Matches printf's %g.
//    When EMIT_TRAILING_ZERO_AFTER_POINT is also given, one trailing zero is
//    preserved.
//
// Infinity symbol and nan_symbol provide the string representation for these
// special values. If the string is NULL and the special value is encountered
// then the conversion functions return false.
//
// The exponent_character is used in exponential representations. It is
// usually 'e' or 'E'.
//
// When converting to the shortest representation the converter will
// represent input numbers in decimal format if they are in the interval
// [10^decimal_in_shortest_low; 10^decimal_in_shortest_high[
//    (lower boundary included, greater boundary excluded).
// Example: with decimal_in_shortest_low = -6 and
//               decimal_in_shortest_high = 21:
//   ToShortest(0.000001)  -> "0.000001"
//   ToShortest(0.0000001) -> "1e-7"
//   ToShortest(111111111111111111111.0)  -> "111111111111111110000"
//   ToShortest(100000000000000000000.0)  -> "100000000000000000000"
//   ToShortest(1111111111111111111111.0) -> "1.1111111111111111e+21"
//
// When converting to precision mode the converter may add
// max_leading_padding_zeroes before returning the number in exponential
// format.
// Example with max_leading_padding_zeroes_in_precision_mode = 6.
//   ToPrecision(0.0000012345, 2) -> "0.0000012"
//   ToPrecision(0.00000012345, 2) -> "1.2e-7"
// Similarly the converter may add up to
// max_trailing_padding_zeroes_in_precision_mode in precision mode to avoid
// returning an exponential representation. A zero added by the
// EMIT_TRAILING_ZERO_AFTER_POINT flag is counted for this limit.
// Examples for max_trailing_padding_zeroes_in_precision_mode = 1:
//   ToPrecision(230.0, 2) -> "230"
//   ToPrecision(230.0, 2) -> "230."  with EMIT_TRAILING_DECIMAL_POINT.
//   ToPrecision(230.0, 2) -> "2.3e2" with EMIT_TRAILING_ZERO_AFTER_POINT.
//
// The min_exponent_width is used for exponential representations.
// The converter adds leading '0's to the exponent until the exponent
// is at least min_exponent_width digits long.
// The min_exponent_width is clamped to 5.
// As such, the exponent may never have more than 5 digits in total.
static DoubleToStringConverter DoubleToStringConverter_make(int flags,
                                                            const char *infinity_symbol,
                                                            const char *nan_symbol,
                                                            char exponent_character,
                                                            int decimal_in_shortest_low,
                                                            int decimal_in_shortest_high,
                                                            int max_leading_padding_zeroes_in_precision_mode,
                                                            int max_trailing_padding_zeroes_in_precision_mode,
                                                            int min_exponent_width) {
    DoubleToStringConverter conv;
    conv.flags = flags;
    conv.infinity_symbol = infinity_symbol;
    conv.nan_symbol = nan_symbol;
    conv.exponent_character = exponent_character;
    conv.decimal_in_shortest_low = decimal_in_shortest_low;
    conv.decimal_in_shortest_high = decimal_in_shortest_high;
    conv.max_leading_padding_zeroes_in_precision_mode = max_leading_padding_zeroes_in_precision_mode;
    conv.max_trailing_padding_zeroes_in_precision_mode = max_trailing_padding_zeroes_in_precision_mode;
    conv.min_exponent_width = min_exponent_width;
    // When 'trailing zero after the point' is set, then 'trailing point'
    // must be set too.
    DOUBLE_CONVERSION_ASSERT(((flags & D2S_EMIT_TRAILING_DECIMAL_POINT) != 0) ||
        !((flags & D2S_EMIT_TRAILING_ZERO_AFTER_POINT) != 0));
    return conv;
}

// Returns a converter following the EcmaScript specification.
//
// Flags: UNIQUE_ZERO and EMIT_POSITIVE_EXPONENT_SIGN.
// Special values: "Infinity" and "NaN".
// Lower case 'e' for exponential values.
// decimal_in_shortest_low: -6
// decimal_in_shortest_high: 21
// max_leading_padding_zeroes_in_precision_mode: 6
// max_trailing_padding_zeroes_in_precision_mode: 0
static const DoubleToStringConverter D2S_EcmaScriptConverter = {
    D2S_UNIQUE_ZERO | D2S_EMIT_POSITIVE_EXPONENT_SIGN,
    "Infinity",
    "NaN",
    'e',
    -6, 21,
    6, 0
};

typedef enum DtoaMode {
    // Produce the shortest correct representation.
    // For example the output of 0.299999999999999988897 is (the less accurate
    // but correct) 0.3.
    DtoaMode_SHORTEST,
    // Same as SHORTEST, but for single-precision floats.
    DtoaMode_SHORTEST_SINGLE,
    // Produce a fixed number of digits after the decimal point.
    // For instance fixed(0.1, 4) becomes 0.1000
    // If the input number is big, the output will be big.
    DtoaMode_FIXED,
    // Fixed number of digits (independent of the decimal point).
    DtoaMode_PRECISION
} DtoaMode;

// Computes a decimal representation with a fixed number of digits after the
// decimal point. The last emitted digit is rounded.
//
// Examples:
//   ToFixed(3.12, 1) -> "3.1"
//   ToFixed(3.1415, 3) -> "3.142"
//   ToFixed(1234.56789, 4) -> "1234.5679"
//   ToFixed(1.23, 5) -> "1.23000"
//   ToFixed(0.1, 4) -> "0.1000"
//   ToFixed(1e30, 2) -> "1000000000000000019884624838656.00"
//   ToFixed(0.1, 30) -> "0.100000000000000005551115123126"
//   ToFixed(0.1, 17) -> "0.10000000000000001"
//
// If requested_digits equals 0, then the tail of the result depends on
// the EMIT_TRAILING_DECIMAL_POINT and EMIT_TRAILING_ZERO_AFTER_POINT.
// Examples, for requested_digits == 0,
//   let EMIT_TRAILING_DECIMAL_POINT and EMIT_TRAILING_ZERO_AFTER_POINT be
//    - false and false: then 123.45 -> 123
//                             0.678 -> 1
//    - true and false: then 123.45 -> 123.
//                            0.678 -> 1.
//    - true and true: then 123.45 -> 123.0
//                           0.678 -> 1.0
//
// Returns true if the conversion succeeds. The conversion always succeeds
// except for the following cases:
//   - the input value is special and no infinity_symbol or nan_symbol has
//     been provided to the constructor,
//   - 'value' > 10^kMaxFixedDigitsBeforePoint, or
//   - 'requested_digits' > kMaxFixedDigitsAfterPoint.
// The last two conditions imply that the result for non-special values never
// contains more than
//  1 + kMaxFixedDigitsBeforePoint + 1 + kMaxFixedDigitsAfterPoint characters
// (one additional character for the sign, and one for the decimal point).
// In addition, the buffer must be able to hold the trailing '\0' character.
static bool D2S_ToFixed(DoubleToStringConverter *conv,
                        double value,
                        int requested_digits,
                        StringBuilder *result_builder);

// Computes a representation in exponential format with requested_digits
// after the decimal point. The last emitted digit is rounded.
// If requested_digits equals -1, then the shortest exponential representation
// is computed.
//
// Examples with EMIT_POSITIVE_EXPONENT_SIGN deactivated, and
//               exponent_character set to 'e'.
//   ToExponential(3.12, 1) -> "3.1e0"
//   ToExponential(5.0, 3) -> "5.000e0"
//   ToExponential(0.001, 2) -> "1.00e-3"
//   ToExponential(3.1415, -1) -> "3.1415e0"
//   ToExponential(3.1415, 4) -> "3.1415e0"
//   ToExponential(3.1415, 3) -> "3.142e0"
//   ToExponential(123456789000000, 3) -> "1.235e14"
//   ToExponential(1000000000000000019884624838656.0, -1) -> "1e30"
//   ToExponential(1000000000000000019884624838656.0, 32) ->
//                     "1.00000000000000001988462483865600e30"
//   ToExponential(1234, 0) -> "1e3"
//
// Returns true if the conversion succeeds. The conversion always succeeds
// except for the following cases:
//   - the input value is special and no infinity_symbol or nan_symbol has
//     been provided to the constructor,
//   - 'requested_digits' > kMaxExponentialDigits.
//
// The last condition implies that the result never contains more than
// kMaxExponentialDigits + 8 characters (the sign, the digit before the
// decimal point, the decimal point, the exponent character, the
// exponent's sign, and at most 3 exponent digits).
// In addition, the buffer must be able to hold the trailing '\0' character.
static bool D2S_ToExponential(DoubleToStringConverter *conv,
                              double value,
                              int requested_digits,
                              StringBuilder *result_builder);

// Computes 'precision' leading digits of the given 'value' and returns them
// either in exponential or decimal format, depending on
// max_{leading|trailing}_padding_zeroes_in_precision_mode (given to the
// constructor).
// The last computed digit is rounded.
//
// Example with max_leading_padding_zeroes_in_precision_mode = 6.
//   ToPrecision(0.0000012345, 2) -> "0.0000012"
//   ToPrecision(0.00000012345, 2) -> "1.2e-7"
// Similarly the converter may add up to
// max_trailing_padding_zeroes_in_precision_mode in precision mode to avoid
// returning an exponential representation. A zero added by the
// EMIT_TRAILING_ZERO_AFTER_POINT flag is counted for this limit.
// Examples for max_trailing_padding_zeroes_in_precision_mode = 1:
//   ToPrecision(230.0, 2) -> "230"
//   ToPrecision(230.0, 2) -> "230."  with EMIT_TRAILING_DECIMAL_POINT.
//   ToPrecision(230.0, 2) -> "2.3e2" with EMIT_TRAILING_ZERO_AFTER_POINT.
// Examples for max_trailing_padding_zeroes_in_precision_mode = 3, and no
//    EMIT_TRAILING_ZERO_AFTER_POINT:
//   ToPrecision(123450.0, 6) -> "123450"
//   ToPrecision(123450.0, 5) -> "123450"
//   ToPrecision(123450.0, 4) -> "123500"
//   ToPrecision(123450.0, 3) -> "123000"
//   ToPrecision(123450.0, 2) -> "1.2e5"
//
// Returns true if the conversion succeeds. The conversion always succeeds
// except for the following cases:
//   - the input value is special and no infinity_symbol or nan_symbol has
//     been provided to the constructor,
//   - precision < kMinPericisionDigits
//   - precision > kMaxPrecisionDigits
//
// The last condition implies that the result never contains more than
// kMaxPrecisionDigits + 7 characters (the sign, the decimal point, the
// exponent character, the exponent's sign, and at most 3 exponent digits).
// In addition, the buffer must be able to hold the trailing '\0' character.
static bool D2S_ToPrecision(DoubleToStringConverter *conv,
                            double value,
                            int precision,
                            StringBuilder *result_builder);

// Converts the given double 'v' to digit characters. 'v' must not be NaN,
// +Infinity, or -Infinity. In SHORTEST_SINGLE-mode this restriction also
// applies to 'v' after it has been casted to a single-precision float. That
// is, in this mode static_cast<float>(v) must not be NaN, +Infinity or
// -Infinity.
//
// The result should be interpreted as buffer * 10^(point-length).
//
// The digits are written to the buffer in the platform's charset, which is
// often UTF-8 (with ASCII-range digits) but may be another charset, such
// as EBCDIC.
//
// The output depends on the given mode:
//  - SHORTEST: produce the least amount of digits for which the internal
//   identity requirement is still satisfied. If the digits are printed
//   (together with the correct exponent) then reading this number will give
//   'v' again. The buffer will choose the representation that is closest to
//   'v'. If there are two at the same distance, than the one farther away
//   from 0 is chosen (halfway cases - ending with 5 - are rounded up).
//   In this mode the 'requested_digits' parameter is ignored.
//  - SHORTEST_SINGLE: same as SHORTEST but with single-precision.
//  - FIXED: produces digits necessary to print a given number with
//   'requested_digits' digits after the decimal point. The produced digits
//   might be too short in which case the caller has to fill the remainder
//   with '0's.
//   Example: toFixed(0.001, 5) is allowed to return buffer="1", point=-2.
//   Halfway cases are rounded towards +/-Infinity (away from 0). The call
//   toFixed(0.15, 2) thus returns buffer="2", point=0.
//   The returned buffer may contain digits that would be truncated from the
//   shortest representation of the input.
//  - PRECISION: produces 'requested_digits' where the first digit is not '0'.
//   Even though the length of produced digits usually equals
//   'requested_digits', the function is allowed to return fewer digits, in
//   which case the caller has to fill the missing digits with '0's.
//   Halfway cases are again rounded away from 0.
// DoubleToAscii expects the given buffer to be big enough to hold all
// digits and a terminating null-character. In SHORTEST-mode it expects a
// buffer of at least kBase10MaximalLength + 1. In all other modes the
// requested_digits parameter and the padding-zeroes limit the size of the
// output. Don't forget the decimal point, the exponent character and the
// terminating null-character when computing the maximal output size.
// The given length is only used in debug mode to ensure the buffer is big
// enough.
static void D2S_DoubleToAscii(double v,
                              DtoaMode mode,
                              int requested_digits,
                              char *buffer,
                              int buffer_length,
                              bool *sign,
                              int *length,
                              int *point);

// Implementation for ToShortest and ToShortestSingle.
static bool D2S_ToShortestIeeeNumber(DoubleToStringConverter *conv,
                                     double value,
                                     StringBuilder *result_builder,
                                     DtoaMode mode);

// If the value is a special value (NaN or Infinity) constructs the
// corresponding string using the configured infinity/nan-symbol.
// If either of them is NULL or the value is not special then the
// function returns false.
static bool D2S_HandleSpecialValues(DoubleToStringConverter *conv,
                                    double value, StringBuilder *result_builder);

// Constructs an exponential representation (i.e. 1.234e56).
// The given exponent assumes a decimal point after the first decimal digit.
static void D2S_CreateExponentialRepresentation(DoubleToStringConverter *conv,
                                                const char *decimal_digits,
                                                int length,
                                                int exponent,
                                                StringBuilder *result_builder);

// Creates a decimal representation (i.e 1234.5678).
static void D2S_CreateDecimalRepresentation(DoubleToStringConverter *conv,
                                            const char *decimal_digits,
                                            int length,
                                            int decimal_point,
                                            int digits_after_point,
                                            StringBuilder *result_builder);

// Computes the shortest string of digits that correctly represent the input
// number. Depending on decimal_in_shortest_low and decimal_in_shortest_high
// (see constructor) it then either returns a decimal representation, or an
// exponential representation.
// Example with decimal_in_shortest_low = -6,
//              decimal_in_shortest_high = 21,
//              EMIT_POSITIVE_EXPONENT_SIGN activated, and
//              EMIT_TRAILING_DECIMAL_POINT deactivated:
//   ToShortest(0.000001)  -> "0.000001"
//   ToShortest(0.0000001) -> "1e-7"
//   ToShortest(111111111111111111111.0)  -> "111111111111111110000"
//   ToShortest(100000000000000000000.0)  -> "100000000000000000000"
//   ToShortest(1111111111111111111111.0) -> "1.1111111111111111e+21"
//
// Note: the conversion may round the output if the returned string
// is accurate enough to uniquely identify the input-number.
// For example the most precise representation of the double 9e59 equals
// "899999999999999918767229449717619953810131273674690656206848", but
// the converter will return the shorter (but still correct) "9e59".
//
// Returns true if the conversion succeeds. The conversion always succeeds
// except when the input value is special and no infinity_symbol or
// nan_symbol has been given to the constructor.
//
// The length of the longest result is the maximum of the length of the
// following string representations (each with possible examples):
// - NaN and negative infinity: "NaN", "-Infinity", "-inf".
// - -10^(decimal_in_shortest_high - 1):
//      "-100000000000000000000", "-1000000000000000.0"
// - the longest string in range [0; -10^decimal_in_shortest_low]. Generally,
//   this string is 3 + kBase10MaximalLength - decimal_in_shortest_low.
//   (Sign, '0', decimal point, padding zeroes for decimal_in_shortest_low,
//   and the significant digits).
//      "-0.0000033333333333333333", "-0.0012345678901234567"
// - the longest exponential representation. (A negative number with
//   kBase10MaximalLength significant digits).
//      "-1.7976931348623157e+308", "-1.7976931348623157E308"
// In addition, the buffer must be able to hold the trailing '\0' character.
static bool D2S_ToShortest(DoubleToStringConverter *conv,
                           double value, StringBuilder *result_builder) {
    return D2S_ToShortestIeeeNumber(conv, value, result_builder, DtoaMode_SHORTEST);
}

// Same as ToShortest, but for single-precision floats.
static bool D2S_ToShortestSingle(DoubleToStringConverter *conv,
                                 float value, StringBuilder *result_builder) {
    return D2S_ToShortestIeeeNumber(conv, value, result_builder, DtoaMode_SHORTEST_SINGLE);
}



/// ============================================================================
/// double-to-string.cc
/// ============================================================================

static bool D2S_HandleSpecialValues(DoubleToStringConverter *conv,
                                    double value,
                                    StringBuilder *sb) {
    Double double_inspect = Double_make(value);
    if (Double_IsInfinite(&double_inspect)) {
        if (conv->infinity_symbol == NULL) return false;
        if (value < 0) {
            StringBuilder_AddCharacter(sb, '-');
        }
        StringBuilder_AddString(sb, conv->infinity_symbol);
        return true;
    }
    if (Double_IsNan(&double_inspect)) {
        if (conv->nan_symbol == NULL) return false;
        StringBuilder_AddString(sb, conv->nan_symbol);
        return true;
    }
    return false;
}

static void D2S_CreateExponentialRepresentation(DoubleToStringConverter *conv,
                                                const char *decimal_digits,
                                                int length,
                                                int exponent,
                                                StringBuilder *sb) {
    DOUBLE_CONVERSION_ASSERT(length != 0);
    StringBuilder_AddCharacter(sb, decimal_digits[0]);
    if (length != 1) {
        StringBuilder_AddCharacter(sb, '.');
        StringBuilder_AddSubstring(sb, &decimal_digits[1], length-1);
    }
    StringBuilder_AddCharacter(sb, conv->exponent_character);
    if (exponent < 0) {
        StringBuilder_AddCharacter(sb, '-');
        exponent = -exponent;
    } else {
        if ((conv->flags & D2S_EMIT_POSITIVE_EXPONENT_SIGN) != 0) {
            StringBuilder_AddCharacter(sb, '+');
        }
    }
    DOUBLE_CONVERSION_ASSERT(exponent < 1e4);
    // Changing this constant requires updating the comment of DoubleToStringConverter constructor
    const int kMaxExponentLength = 5;
    char buffer[5 /* kMaxExponentLength */ + 1];
    buffer[kMaxExponentLength] = '\0';
    int first_char_pos = kMaxExponentLength;
    if (exponent == 0) {
        buffer[--first_char_pos] = '0';
    } else {
        while (exponent > 0) {
            buffer[--first_char_pos] = '0' + (exponent % 10);
            exponent /= 10;
        }
    }
    // Add prefix '0' to make exponent width >= min(min_exponent_with_, kMaxExponentLength)
    // For example: convert 1e+9 -> 1e+09, if min_exponent_with_ is set to 2
    while(kMaxExponentLength - first_char_pos < MIN(conv->min_exponent_width, kMaxExponentLength)) {
        buffer[--first_char_pos] = '0';
    }
    StringBuilder_AddSubstring(sb, &buffer[first_char_pos],
                               kMaxExponentLength - first_char_pos);
}

static void D2S_CreateDecimalRepresentation(DoubleToStringConverter *conv,
                                            const char *decimal_digits,
                                            int length,
                                            int decimal_point,
                                            int digits_after_point,
                                            StringBuilder *sb) {
    // Create a representation that is padded with zeros if needed.
    if (decimal_point <= 0) {
        // "0.00000decimal_rep" or "0.000decimal_rep00".
        StringBuilder_AddCharacter(sb, '0');
        if (digits_after_point > 0) {
            StringBuilder_AddCharacter(sb, '.');
            StringBuilder_AddPadding(sb, '0', -decimal_point);
            DOUBLE_CONVERSION_ASSERT(length <= digits_after_point - (-decimal_point));
            StringBuilder_AddSubstring(sb, decimal_digits, length);
            int remaining_digits = digits_after_point - (-decimal_point) - length;
            StringBuilder_AddPadding(sb, '0', remaining_digits);
        }
    } else if (decimal_point >= length) {
        // "decimal_rep0000.00000" or "decimal_rep.0000".
        StringBuilder_AddSubstring(sb, decimal_digits, length);
        StringBuilder_AddPadding(sb, '0', decimal_point - length);
        if (digits_after_point > 0) {
            StringBuilder_AddCharacter(sb, '.');
            StringBuilder_AddPadding(sb, '0', digits_after_point);
        }
    } else {
        // "decima.l_rep000".
        DOUBLE_CONVERSION_ASSERT(digits_after_point > 0);
        StringBuilder_AddSubstring(sb, decimal_digits, decimal_point);
        StringBuilder_AddCharacter(sb, '.');
        DOUBLE_CONVERSION_ASSERT(length - decimal_point <= digits_after_point);
        StringBuilder_AddSubstring(sb, &decimal_digits[decimal_point],
                                   length - decimal_point);
        int remaining_digits = digits_after_point - (length - decimal_point);
        StringBuilder_AddPadding(sb, '0', remaining_digits);
    }
    if (digits_after_point == 0) {
        if ((conv->flags & D2S_EMIT_TRAILING_DECIMAL_POINT) != 0) {
            StringBuilder_AddCharacter(sb, '.');
        }
        if ((conv->flags & D2S_EMIT_TRAILING_ZERO_AFTER_POINT) != 0) {
            StringBuilder_AddCharacter(sb, '0');
        }
    }
}

static bool D2S_ToShortestIeeeNumber(DoubleToStringConverter *conv,
                                     double value,
                                     StringBuilder *sb,
                                     DtoaMode mode) {
    DOUBLE_CONVERSION_ASSERT(mode == DtoaMode_SHORTEST || mode == DtoaMode_SHORTEST_SINGLE);
    Double d = Double_make(value);
    if (Double_IsSpecial(&d)) {
        return D2S_HandleSpecialValues(conv, value, sb);
    }
    
    int decimal_point;
    bool sign;
    const int kDecimalRepCapacity = D2S_kBase10MaximalLength + 1;
    char decimal_rep[D2S_kBase10MaximalLength + 1]; // kDecimalRepCapacity
    int decimal_rep_length;

    D2S_DoubleToAscii(value, mode, 0, decimal_rep, kDecimalRepCapacity,
                      &sign, &decimal_rep_length, &decimal_point);
    
    bool unique_zero = (conv->flags & D2S_UNIQUE_ZERO) != 0;
    if (sign && (value != 0.0 || !unique_zero)) {
        StringBuilder_AddCharacter(sb, '-');
    }
    
    int exponent = decimal_point - 1;
    if ((conv->decimal_in_shortest_low <= exponent) &&
        (exponent < conv->decimal_in_shortest_high)) {
        D2S_CreateDecimalRepresentation(conv, decimal_rep, decimal_rep_length,
                                        decimal_point,
                                        MAX(0, decimal_rep_length - decimal_point),
                                        sb);
    } else {
        D2S_CreateExponentialRepresentation(conv, decimal_rep, decimal_rep_length, exponent,
                                            sb);
    }
    return true;
}

static bool D2S_ToFixed(DoubleToStringConverter *conv,
                        double value,
                        int requested_digits,
                        StringBuilder *sb) {
    DOUBLE_CONVERSION_ASSERT(D2S_kMaxFixedDigitsBeforePoint == 60);
    const double kFirstNonFixed = 1e60;
    
    Double d = Double_make(value);
    if (Double_IsSpecial(&d)) {
        return D2S_HandleSpecialValues(conv, value, sb);
    }
    
    if (requested_digits > D2S_kMaxFixedDigitsAfterPoint) return false;
    if (value >= kFirstNonFixed || value <= -kFirstNonFixed) return false;
    
    // Find a sufficiently precise decimal representation of n.
    int decimal_point;
    bool sign;
    // Add space for the '\0' byte.
    const int kDecimalRepCapacity =
        D2S_kMaxFixedDigitsBeforePoint + D2S_kMaxFixedDigitsAfterPoint + 1;
    char decimal_rep[D2S_kMaxFixedDigitsBeforePoint + D2S_kMaxFixedDigitsAfterPoint + 1]; // kDecimalRepCapacity
    int decimal_rep_length;
    D2S_DoubleToAscii(value, DtoaMode_FIXED, requested_digits,
                      decimal_rep, kDecimalRepCapacity,
                      &sign, &decimal_rep_length, &decimal_point);
    
    bool unique_zero = ((conv->flags & D2S_UNIQUE_ZERO) != 0);
    if (sign && (value != 0.0 || !unique_zero)) {
        StringBuilder_AddCharacter(sb, '-');
    }
    
    D2S_CreateDecimalRepresentation(conv, decimal_rep, decimal_rep_length, decimal_point,
                                    requested_digits, sb);
    return true;
}

static bool D2S_ToExponential(DoubleToStringConverter *conv,
                              double value,
                              int requested_digits,
                              StringBuilder *sb) {
    Double d = Double_make(value);
    if (Double_IsSpecial(&d)) {
        return D2S_HandleSpecialValues(conv, value, sb);
    }
    
    if (requested_digits < -1) return false;
    if (requested_digits > D2S_kMaxExponentialDigits) return false;
    
    int decimal_point;
    bool sign;
    // Add space for digit before the decimal point and the '\0' character.
    const int kDecimalRepCapacity = D2S_kMaxExponentialDigits + 2;
    DOUBLE_CONVERSION_ASSERT(kDecimalRepCapacity > D2S_kBase10MaximalLength);
    char decimal_rep[D2S_kMaxExponentialDigits + 2]; // kDecimalRepCapacity
#ifndef NDEBUG
    // Problem: there is an assert in StringBuilder::AddSubstring() that
    // will pass this buffer to strlen(), and this buffer is not generally
    // null-terminated.
    memset(decimal_rep, 0, sizeof(decimal_rep));
#endif
    int decimal_rep_length;
    
    if (requested_digits == -1) {
        D2S_DoubleToAscii(value, DtoaMode_SHORTEST, 0,
                  decimal_rep, kDecimalRepCapacity,
                  &sign, &decimal_rep_length, &decimal_point);
    } else {
        D2S_DoubleToAscii(value, DtoaMode_PRECISION, requested_digits + 1,
                          decimal_rep, kDecimalRepCapacity,
                          &sign, &decimal_rep_length, &decimal_point);
        DOUBLE_CONVERSION_ASSERT(decimal_rep_length <= requested_digits + 1);
        
        for (int i = decimal_rep_length; i < requested_digits + 1; ++i) {
            decimal_rep[i] = '0';
        }
        decimal_rep_length = requested_digits + 1;
    }
    
    bool unique_zero = ((conv->flags & D2S_UNIQUE_ZERO) != 0);
    if (sign && (value != 0.0 || !unique_zero)) {
        StringBuilder_AddCharacter(sb, '-');
    }
    
    int exponent = decimal_point - 1;
    D2S_CreateExponentialRepresentation(conv,
                                        decimal_rep,
                                        decimal_rep_length,
                                        exponent,
                                        sb);
    return true;
}

static bool D2S_ToPrecision(DoubleToStringConverter *conv,
                            double value,
                            int precision,
                            StringBuilder *sb) {
    Double d = Double_make(value);
    if (Double_IsSpecial(&d)) {
        return D2S_HandleSpecialValues(conv, value, sb);
    }
    
    if (precision < D2S_kMinPrecisionDigits || precision > D2S_kMaxPrecisionDigits) {
        return false;
    }
    
    // Find a sufficiently precise decimal representation of n.
    int decimal_point;
    bool sign;
    // Add one for the terminating null character.
    const int kDecimalRepCapacity = D2S_kMaxPrecisionDigits + 1;
    char decimal_rep[D2S_kMaxPrecisionDigits + 1]; // kDecimalRepCapacity
    int decimal_rep_length;
    
    D2S_DoubleToAscii(value, DtoaMode_PRECISION, precision,
                      decimal_rep, kDecimalRepCapacity,
                      &sign, &decimal_rep_length, &decimal_point);
    DOUBLE_CONVERSION_ASSERT(decimal_rep_length <= precision);

    bool unique_zero = ((conv->flags & D2S_UNIQUE_ZERO) != 0);
    if (sign && (value != 0.0 || !unique_zero)) {
        StringBuilder_AddCharacter(sb, '-');
    }
    
    // The exponent if we print the number as x.xxeyyy. That is with the
    // decimal point after the first digit.
    int exponent = decimal_point - 1;
    
    int extra_zero = ((conv->flags & D2S_EMIT_TRAILING_ZERO_AFTER_POINT) != 0) ? 1 : 0;
    bool as_exponential =
        (-decimal_point + 1 > conv->max_leading_padding_zeroes_in_precision_mode) ||
        (decimal_point - precision + extra_zero >
         conv->max_trailing_padding_zeroes_in_precision_mode);
    if ((conv->flags & D2S_NO_TRAILING_ZERO) != 0) {
        // Truncate trailing zeros that occur after the decimal point (if exponential,
        // that is everything after the first digit).
        int stop = as_exponential ? 1 : MAX(1, decimal_point);
        while (decimal_rep_length > stop && decimal_rep[decimal_rep_length - 1] == '0') {
            --decimal_rep_length;
        }
        // Clamp precision to avoid the code below re-adding the zeros.
        precision = MIN(precision, decimal_rep_length);
    }
    if (as_exponential) {
        // Fill buffer to contain 'precision' digits.
        // Usually the buffer is already at the correct length, but 'DoubleToAscii'
        // is allowed to return less characters.
        for (int i = decimal_rep_length; i < precision; ++i) {
            decimal_rep[i] = '0';
        }
        
        D2S_CreateExponentialRepresentation(conv,
                                            decimal_rep,
                                            precision,
                                            exponent,
                                            sb);
    } else {
        D2S_CreateDecimalRepresentation(conv,decimal_rep, decimal_rep_length, decimal_point,
                                        MAX(0, precision - decimal_point),
                                        sb);
    }
    return true;
}

static BignumDtoaMode DtoaToBignumDtoaMode(DtoaMode dtoa_mode) {
    switch (dtoa_mode) {
        case DtoaMode_SHORTEST:  return BIGNUM_DTOA_SHORTEST;
        case DtoaMode_SHORTEST_SINGLE: return BIGNUM_DTOA_SHORTEST_SINGLE;
        case DtoaMode_FIXED:     return BIGNUM_DTOA_FIXED;
        case DtoaMode_PRECISION: return BIGNUM_DTOA_PRECISION;
        default:
            DOUBLE_CONVERSION_UNREACHABLE();
    }
}

static void D2S_DoubleToAscii(double v,
                              DtoaMode mode,
                              int requested_digits,
                              char *buffer,
                              int buffer_length,
                              bool *sign,
                              int *length,
                              int *point) {
    Vector vector = Vector_make(buffer, buffer_length);
    Double d = Double_make(v);
    DOUBLE_CONVERSION_ASSERT(!Double_IsSpecial(&d));
    DOUBLE_CONVERSION_ASSERT(mode == DtoaMode_SHORTEST ||
                             mode == DtoaMode_SHORTEST_SINGLE ||
                             requested_digits >= 0);
    
    if (Double_Sign(&d) < 0) {
        *sign = true;
        v = -v;
    } else {
        *sign = false;
    }
    
    if (mode == DtoaMode_PRECISION && requested_digits == 0) {
        vector.start[0] = '\0';
        *length = 0;
        return;
    }
    
    if (v == 0) {
        vector.start[0] = '0';
        vector.start[1] = '\0';
        *length = 1;
        *point = 1;
        return;
    }
    
    bool fast_worked;
    switch (mode) {
        case DtoaMode_SHORTEST:
            fast_worked = FastDtoa(v, FAST_DTOA_SHORTEST, 0, &vector, length, point);
            break;
        case DtoaMode_SHORTEST_SINGLE:
            fast_worked = FastDtoa(v, FAST_DTOA_SHORTEST_SINGLE, 0,
                             &vector, length, point);
            break;
        case DtoaMode_FIXED:
            fast_worked = FastFixedDtoa(v, requested_digits, &vector, length, point);
            break;
        case DtoaMode_PRECISION:
            fast_worked = FastDtoa(v, FAST_DTOA_PRECISION, requested_digits,
                             &vector, length, point);
            break;
        default:
            fast_worked = false;
            DOUBLE_CONVERSION_UNREACHABLE();
    }
    if (fast_worked) return;
    
    // If the fast dtoa didn't succeed use the slower bignum version.
    BignumDtoaMode bignum_mode = DtoaToBignumDtoaMode(mode);
    BignumDtoa(v, bignum_mode, requested_digits, &vector, length, point);
    vector.start[*length] = '\0';
}



/// ============================================================================
/// C wrapper
/// ============================================================================

static int dtoa_reformat(double val, char *buf, int pos, int len) {
    // detect inf and nan
    if (pos >= 3) {
        if (memcmp(buf, "Inf", 3) == 0) return pos;
        if (memcmp(buf, "-In", 3) == 0) return pos;
        if (memcmp(buf, "NaN", 3) == 0) return pos;
    }
    
    // add negative sign for -0.0
    if (val == 0.0) {
        Double d = Double_make(val);
        if (Double_Sign(&d) < 0) {
            if (len < 4) return 0;
            memcpy(buf, "0.0", 4);
            return 3;
        }
    }
    
    // get position of '.' and 'e'
    int dot_pos = -1;
    int exp_pos = -1;
    for (int i = 0; i < pos; i++) {
        if (buf[i] == '.') dot_pos = i;
        else if (buf[i] == 'e' || buf[i] == 'E') exp_pos = i;
    }
    
    // add '.0' if output is integer
    if (dot_pos < 0 && exp_pos < 0) {
        if (pos + 3 > len) return 0;
        memcpy(buf + pos, ".0", 3);
        return pos + 2;
    }
    if (dot_pos > 0 || exp_pos > 0) return pos;
    
    // remove positive sign of exponent part
    if (exp_pos > 0 && buf[exp_pos - 1] == '+') {
        memmove(buf + exp_pos - 1, buf + exp_pos, pos - exp_pos + 1);
        pos--;
    }
    return pos;
}

int goo_dtoa(double val, char *buf, int len) {
    if (!buf || len <= 1) return 0;
    StringBuilder sb = StringBuilder_make(buf, len);
    DoubleToStringConverter conv = D2S_EcmaScriptConverter;
    if (!D2S_ToShortest(&conv, val, &sb)) return 0;
    int pos = sb.position;
    if (pos >= len) return 0;
    buf[pos] = '\0';
    return dtoa_reformat(val, buf, pos, len);
}

int goo_dtoa_prec(double val, int prec, char *buf, int len) {
    if (!buf || len <= 1) return 0;
    StringBuilder sb = StringBuilder_make(buf, len);
    DoubleToStringConverter conv = D2S_EcmaScriptConverter;
    if (!D2S_ToPrecision(&conv, val, prec, &sb)) return 0;
    int pos = sb.position;
    if (pos >= len) return 0;
    buf[pos] = '\0';
    return dtoa_reformat(val, buf, pos, len);
}

double goo_strtod(const char *str, int *len) {
    if (!len) return 0.0;
    int input_len = *len;
    *len = 0;
    if (!str || input_len < 1) return 0.0;
    
    StringToDoubleConverter conv;
    conv.flags =
        S2D_ALLOW_HEX |
        S2D_ALLOW_TRAILING_JUNK |
        S2D_ALLOW_LEADING_SPACES |
        S2D_ALLOW_TRAILING_SPACES |
        S2D_ALLOW_CASE_INSENSITIVITY |
        S2D_ALLOW_HEX_FLOATS;
    conv.empty_string_value = 0.0;
    conv.junk_string_value = 0.0;
    conv.infinity_symbol = "inf";
    conv.nan_symbol = "nan";
    conv.separator = '\0';
    
    int processed = 0;
    double val = StringToDouble(&conv, str, input_len, &processed);
    if (processed == 0) {
        *len = 0;
        return 0.0;
    }
    
    // process "infinity" literal
    Double d = Double_make(val);
    if (Double_IsInfinite(&d)) {
        for (int i = 0; i < processed; i++) {
            const char *full = "infinity";
            int full_len = (int)strlen(full);
            bool full_match = true;
            for (int j = 0; j < full_len; j++) {
                if (S2D_ToLower(str[i + j]) != full[j]) {
                    full_match = false;
                    break;
                }
            }
            if (full_match) {
                processed += (int)(strlen(full) - strlen(conv.infinity_symbol));
                break;
            }
        }
    }
    
    // process -0.0
    if (val == 0.0) {
        for (int i = 0; i < processed; i++) {
            if (!S2D_isWhitespace(str[i])) {
                if (str[i] == '-') val = -0.0;
                break;
            }
        }
    }
    
    *len = processed;
    return val;
}



/// ============================================================================
/// Compiler Hint End
/// ============================================================================

#if defined(__clang__)
#   pragma clang diagnostic pop
#elif defined(__GNUC__)
#   if (__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#   pragma GCC diagnostic pop
#   endif
#elif defined(_MSC_VER)
#   pragma warning(pop)
#endif /* warning suppress end */

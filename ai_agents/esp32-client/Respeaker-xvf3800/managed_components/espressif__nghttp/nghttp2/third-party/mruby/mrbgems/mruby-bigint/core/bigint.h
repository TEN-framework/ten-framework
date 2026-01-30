/**
** @file mruby/bigint.h - Multi-precision Integer
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_BIGINT_H
#define MRUBY_BIGINT_H
/*
 * FREE GMP - a public domain implementation of a subset of the
 *           gmp library
 *
 * I hearby place the file in the public domain.
 *
 * Do whatever you want with this code. Change it. Sell it. Claim you
 *  wrote it.
 * Bugs, complaints, flames, rants: please send email to
 *    Mark Henderson <markh@wimsey.bc.ca>
 * I'm already aware that fgmp is considerably slower than gmp
 *
 * CREDITS:
 *  Paul Rouse <par@r-cube.demon.co.uk> - generic bug fixes, mpz_sqrt and
 *    mpz_sqrtrem, and modifications to get fgmp to compile on a system
 *    with int and long of different sizes (specifically MS-DOS,286 compiler)
 *  Also see the file "notes" included with the fgmp distribution, for
 *    more credits.
 *
 * VERSION 1.0 - beta 5
 */

#include <sys/types.h>

#if defined(MRB_INT32) && defined(_WIN32) && !defined(MRB_NO_MPZ64BIT)
#define MRB_NO_MPZ64BIT
#endif

#ifdef MRB_NO_MPZ64BIT
typedef uint16_t mp_limb;
typedef uint32_t mp_dbl_limb;
typedef int32_t mp_dbl_limb_signed;
#define MPZ_DIG_SIZE 16
#else
typedef uint32_t mp_limb;
typedef uint64_t mp_dbl_limb;
typedef int64_t mp_dbl_limb_signed;
#define MPZ_DIG_SIZE 32
#endif

typedef struct _mpz_t {
  mp_limb *p;
  short sn;
  size_t sz;
} mpz_t;

struct RBigint {
  MRB_OBJECT_HEADER;
  mpz_t mp;
};
#define RBIGINT(v) ((struct RBigint*)mrb_ptr(v))

mrb_static_assert_object_size(struct RBigint);

#endif  /* MRUBY_BIGINT_H */

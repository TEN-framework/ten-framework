#include <mruby.h>
#include <mruby/numeric.h>
#include <mruby/array.h>
#include <mruby/internal.h>
#include <mruby/presym.h>

#ifndef MRB_NO_FLOAT
static mrb_value flo_remainder(mrb_state *mrb, mrb_value self);
#endif

/*
 *  call-seq:
 *     num.remainder(numeric)  ->  real
 *
 *  <code>x.remainder(y)</code> means <code>x-y*(x/y).truncate</code>.
 *
 *  See Numeric#divmod.
 */
static mrb_value
int_remainder(mrb_state *mrb, mrb_value x)
{
  mrb_value y = mrb_get_arg1(mrb);
  mrb_int a, b;

#ifdef MRB_USE_BIGINT
  if (mrb_bigint_p(x)) {
    if (mrb_integer_p(y) || mrb_bigint_p(y)) {
      return mrb_bint_rem(mrb, x, y);
    }
    return flo_remainder(mrb, mrb_float_value(mrb, mrb_as_float(mrb, x)));
  }
#endif
  a = mrb_integer(x);
  if (mrb_integer_p(y)) {
    b = mrb_integer(y);
    if (b == 0) mrb_int_zerodiv(mrb);
    if (a == MRB_INT_MIN && b == -1) return mrb_fixnum_value(0);
    return mrb_int_value(mrb, a % b);
  }
#ifdef MRB_NO_FLOAT
  mrb_raise(mrb, E_TYPE_ERROR, "non integer remainder");
#else
  return flo_remainder(mrb, mrb_float_value(mrb, mrb_as_float(mrb, x)));
#endif
}

mrb_value mrb_int_pow(mrb_state *mrb, mrb_value x, mrb_value y);

/*
 * call-seq:
 *    integer.pow(numeric)           ->  numeric
 *    integer.pow(integer, integer)  ->  integer
 *
 * Returns (modular) exponentiation as:
 *
 *   a.pow(b)     #=> same as a**b
 *   a.pow(b, m)  #=> same as (a**b) % m, but avoids huge temporary values
 */
static mrb_value
int_powm(mrb_state *mrb, mrb_value x)
{
  mrb_value m, e;
  mrb_int exp, mod, result = 1;

  if (mrb_get_argc(mrb) == 1) {
    return mrb_int_pow(mrb, x, mrb_get_arg1(mrb));
  }
  mrb_get_args(mrb, "oo", &e, &m);
  if (!mrb_integer_p(e)
#ifdef MRB_USE_BIGINT
      && !mrb_bigint_p(e)
#endif
      ) {
    mrb_raise(mrb, E_TYPE_ERROR, "int.pow(n,m): 2nd argument not allowed unless 1st argument is an integer");
  }
#ifdef MRB_USE_BIGINT
  if (mrb_bigint_p(x)) {
    return mrb_bint_powm(mrb, x, e, m);
  }
  if (mrb_bigint_p(e) || mrb_bigint_p(m)) {
    return mrb_bint_powm(mrb, mrb_bint_new_int(mrb, mrb_integer(x)), e, m);
  }
#endif
  exp = mrb_integer(e);
  if (exp < 0) mrb_raise(mrb, E_ARGUMENT_ERROR, "int.pow(n,m): n must be positive");
  if (!mrb_integer_p(m)) mrb_raise(mrb, E_TYPE_ERROR, "int.pow(n,m): m must be integer");
  mod = mrb_integer(m);
  if (mod < 0) mrb_raise(mrb, E_ARGUMENT_ERROR, "int.pow(n,m): m must be positive when 2nd argument specified");
  if (mod == 0) mrb_int_zerodiv(mrb);
  if (mod == 1) return mrb_fixnum_value(0);
  mrb_int base = mrb_integer(x);
  for (;;) {
    mrb_int tmp;
    if (exp & 1) {
      if (mrb_int_mul_overflow(result, base, &tmp)) {
        result %= mod; base %= mod;
        if (mrb_int_mul_overflow(result, base, &tmp)) {
#ifdef MRB_USE_BIGINT
          return mrb_bint_powm(mrb, mrb_bint_new_int(mrb, mrb_integer(x)), e, m);
#else
          mrb_int_overflow(mrb, "pow");
#endif
        }
      }
      result = tmp % mod;
    }
    exp >>= 1;
    if (exp == 0) break;
    if (mrb_int_mul_overflow(base, base, &tmp)) {
      base %= mod;
      if (mrb_int_mul_overflow(base, base, &tmp)) {
#ifdef MRB_USE_BIGINT
        return mrb_bint_powm(mrb, mrb_bint_new_int(mrb, mrb_integer(x)), e, m);
#else
        mrb_int_overflow(mrb, "pow");
#endif
      }
    }
    base = tmp % mod;
  }
  return mrb_int_value(mrb, result);
}

/*
 *  call-seq:
 *    digits(base = 10) -> array_of_integers
 *
 *  Returns an array of integers representing the +base+-radix
 *  digits of +self+;
 *  the first element of the array represents the least significant digit:
 *
 *    12345.digits      # => [5, 4, 3, 2, 1]
 *    12345.digits(7)   # => [4, 6, 6, 0, 5]
 *    12345.digits(100) # => [45, 23, 1]
 *
 *  Raises an exception if +self+ is negative or +base+ is less than 2.
 *
 */

static mrb_value
int_digits(mrb_state *mrb, mrb_value self)
{
  mrb_int base = 10;

  mrb_get_args(mrb, "|i", &base);
  if (base < 0) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "negative radix");
  }
  else if (base < 2) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "invalid radix %i", base);
  }
#ifdef MRB_USE_BIGINT
  if (mrb_bigint_p(self)) {
    mrb_value x = self;
    mrb_value zero = mrb_fixnum_value(0);
    mrb_value bv = mrb_int_value(mrb, base);
    if (mrb_bint_cmp(mrb, x, zero) < 0) {
      mrb_raise(mrb, E_ARGUMENT_ERROR, "number should be positive");
    }
    mrb_value digits = mrb_ary_new(mrb);

    while (mrb_bint_cmp(mrb, x, zero) == 0) {
      mrb_ary_push(mrb, digits, zero);
      return digits;
    }

    while (mrb_bint_cmp(mrb, x, zero) > 0) {
      mrb_value q = mrb_bint_mod(mrb, x, bv);
      mrb_ary_push(mrb, digits, q);
      x = mrb_bint_div(mrb, x, bv);
      if (!mrb_bigint_p(x)) {
        mrb_int n = mrb_integer(x);
        while (n > 0) {
          mrb_int q = n % base;
          mrb_ary_push(mrb, digits, mrb_int_value(mrb, q));
          n /= base;
        }
        break;
      }
    }
    return digits;
  }
#endif
  mrb_int n = mrb_integer(self);
  if (n < 0) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "number should be positive");
  }

  mrb_value digits = mrb_ary_new(mrb);

  if (n == 0) {
    mrb_ary_push(mrb, digits, mrb_fixnum_value(0));
    return digits;
  }

  while (n > 0) {
    mrb_int q = n % base;
    mrb_ary_push(mrb, digits, mrb_int_value(mrb, q));
    n /= base;
  }
  return digits;
}

#ifndef MRB_NO_FLOAT
static mrb_value
flo_remainder(mrb_state *mrb, mrb_value self)
{
  mrb_float a, b;

  a = mrb_float(self);
  mrb_get_args(mrb, "f", &b);
  if (b == 0) mrb_int_zerodiv(mrb);
  if (isinf(b)) return mrb_float_value(mrb, a);
  return mrb_float_value(mrb, a-b*trunc(a/b));
}
#endif

void
mrb_mruby_numeric_ext_gem_init(mrb_state* mrb)
{
  struct RClass *ic = mrb->integer_class;

  mrb_define_alias(mrb, ic, "modulo", "%");
  mrb_define_method(mrb, ic, "remainder", int_remainder, MRB_ARGS_REQ(1));

  mrb_define_method_id(mrb, ic, MRB_SYM(pow), int_powm, MRB_ARGS_ARG(1,1));
  mrb_define_method_id(mrb, ic, MRB_SYM(digits), int_digits, MRB_ARGS_OPT(1));

#ifndef MRB_NO_FLOAT
  struct RClass *fc = mrb->float_class;

  mrb_define_alias(mrb, fc, "modulo", "%");
  mrb_define_method(mrb, fc, "remainder", flo_remainder, MRB_ARGS_REQ(1));

  mrb_define_const_id(mrb, fc, MRB_SYM(RADIX),        mrb_fixnum_value(MRB_FLT_RADIX));
  mrb_define_const_id(mrb, fc, MRB_SYM(MANT_DIG),     mrb_fixnum_value(MRB_FLT_MANT_DIG));
  mrb_define_const_id(mrb, fc, MRB_SYM(EPSILON),      mrb_float_value(mrb, MRB_FLT_EPSILON));
  mrb_define_const_id(mrb, fc, MRB_SYM(DIG),          mrb_fixnum_value(MRB_FLT_DIG));
  mrb_define_const_id(mrb, fc, MRB_SYM(MIN_EXP),      mrb_fixnum_value(MRB_FLT_MIN_EXP));
  mrb_define_const_id(mrb, fc, MRB_SYM(MIN),          mrb_float_value(mrb, MRB_FLT_MIN));
  mrb_define_const_id(mrb, fc, MRB_SYM(MIN_10_EXP),   mrb_fixnum_value(MRB_FLT_MIN_10_EXP));
  mrb_define_const_id(mrb, fc, MRB_SYM(MAX_EXP),      mrb_fixnum_value(MRB_FLT_MAX_EXP));
  mrb_define_const_id(mrb, fc, MRB_SYM(MAX),          mrb_float_value(mrb, MRB_FLT_MAX));
  mrb_define_const_id(mrb, fc, MRB_SYM(MAX_10_EXP),   mrb_fixnum_value(MRB_FLT_MAX_10_EXP));
#endif /* MRB_NO_FLOAT */
}

void
mrb_mruby_numeric_ext_gem_final(mrb_state* mrb)
{
}

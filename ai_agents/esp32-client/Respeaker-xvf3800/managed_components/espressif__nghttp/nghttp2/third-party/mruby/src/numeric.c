/*
** numeric.c - Numeric, Integer, Float class
**
** See Copyright Notice in mruby.h
*/

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/numeric.h>
#include <mruby/string.h>
#include <mruby/class.h>
#include <mruby/internal.h>
#include <mruby/presym.h>
#include <string.h>

#ifndef MRB_NO_FLOAT
#ifdef MRB_USE_FLOAT32
#define trunc(f) truncf(f)
#define fmod(x,y) fmodf(x,y)
#else
#endif
#endif

mrb_noreturn void
mrb_int_overflow(mrb_state *mrb, const char *reason)
{
  mrb_raisef(mrb, E_RANGE_ERROR, "integer overflow in %s", reason);
}

mrb_noreturn void
mrb_int_zerodiv(mrb_state *mrb)
{
  mrb_raise(mrb, E_ZERODIV_ERROR, "divided by 0");
}

static mrb_noreturn void
mrb_int_noconv(mrb_state *mrb, mrb_value y)
{
  mrb_raisef(mrb, E_TYPE_ERROR, "can't convert %Y into Integer", y);
}

mrb_value
mrb_int_pow(mrb_state *mrb, mrb_value x, mrb_value y)
{
#ifdef MRB_USE_BIGINT
  if (mrb_bigint_p(x)) {
#ifndef MRB_NO_FLOAT
    if (mrb_float_p(y)) {
      return mrb_float_value(mrb, pow(mrb_bint_as_float(mrb, x), mrb_float(y)));
    }
#endif
    return mrb_bint_pow(mrb, x, y);
  }
#endif
  mrb_int base = mrb_integer(x);
  mrb_int result = 1;
  mrb_int exp;

#ifndef MRB_NO_FLOAT
  if (mrb_float_p(y)) {
    return mrb_float_value(mrb, pow((double)base, mrb_float(y)));
  }
  else if (mrb_integer_p(y)) {
    exp = mrb_integer(y);
  }
  else
#endif
  {
    exp = mrb_as_int(mrb, y);
  }
  if (exp < 0) {
#ifndef MRB_NO_FLOAT
    return mrb_float_value(mrb, pow((double)base, (double)exp));
#else
    mrb_int_overflow(mrb, "negative power");
#endif
  }
  for (;;) {
    if (exp & 1) {
      if (mrb_int_mul_overflow(result, base, &result)) {
#ifdef MRB_USE_BIGINT
        return mrb_bint_pow(mrb, mrb_bint_new_int(mrb, mrb_integer(x)), y);
#else
        mrb_int_overflow(mrb, "power");
#endif
      }
    }
    exp >>= 1;
    if (exp == 0) break;
    if (mrb_int_mul_overflow(base, base, &base)) {
#ifdef MRB_USE_BIGINT
      return mrb_bint_pow(mrb, mrb_bint_new_int(mrb, mrb_integer(x)), y);
#else
      mrb_int_overflow(mrb, "power");
#endif
    }
  }
  return mrb_int_value(mrb, result);
}

/*
 * call-seq:
 *
 *  num ** other  ->  num
 *
 * Raises <code>num</code> the <code>other</code> power.
 *
 *    2.0**3      #=> 8.0
 */
static mrb_value
int_pow(mrb_state *mrb, mrb_value x)
{
  return mrb_int_pow(mrb, x, mrb_get_arg1(mrb));
}

mrb_int
mrb_div_int(mrb_int x, mrb_int y)
{
  mrb_int div = x / y;

  if ((x ^ y) < 0 && x != div * y) {
    div -= 1;
  }
  return div;
}

mrb_value
mrb_div_int_value(mrb_state *mrb, mrb_int x, mrb_int y)
{
  if (y == 0) {
    mrb_int_zerodiv(mrb);
  }
  else if (x == MRB_INT_MIN && y == -1) {
#ifdef MRB_USE_BIGINT
    return mrb_bint_mul_ii(mrb, x, y);
#else
    mrb_int_overflow(mrb, "division");
#endif
  }
  return mrb_int_value(mrb, mrb_div_int(x, y));
}

/* 15.2.8.3.6 */
/*
 * call-seq:
 *   int / num  ->  num
 *
 * Performs division: the class of the resulting object depends on
 * the class of <code>num</code> and on the magnitude of the
 * result.
 */
static mrb_value
int_div(mrb_state *mrb, mrb_value x)
{
  mrb_value y = mrb_get_arg1(mrb);
#ifdef MRB_USE_BIGINT
  if (mrb_bigint_p(x)) {
    return mrb_bint_div(mrb, x, y);
  }
#endif
 mrb_int a = mrb_integer(x);

  if (mrb_integer_p(y)) {
    return mrb_div_int_value(mrb, a, mrb_integer(y));
  }
  switch (mrb_type(y)) {
#ifdef MRB_USE_BIGINT
  case MRB_TT_BIGINT:
    return mrb_bint_div(mrb, mrb_bint_new_int(mrb, a), y);
#endif
#ifdef MRB_USE_RATIONAL
  case MRB_TT_RATIONAL:
    return mrb_rational_div(mrb, mrb_rational_new(mrb, a, 1), y);
#endif
#ifdef MRB_USE_COMPLEX
  case MRB_TT_COMPLEX:
    x = mrb_complex_new(mrb, (mrb_float)a, 0);
    return mrb_complex_div(mrb, x, y);
#endif
#ifndef MRB_NO_FLOAT
  case MRB_TT_FLOAT:
    return mrb_float_value(mrb, mrb_div_float((mrb_float)a, mrb_as_float(mrb, y)));
#endif
  default:
    mrb_int_noconv(mrb, y);
  }
}

/* 15.2.9.3.19(x) */
/*
 *  call-seq:
 *     num.quo(numeric)  ->  real
 *
 *  Returns most exact division.
 */

/*
 * call-seq:
 *   int.div(other)  ->  int
 *
 * Performs division: resulting integer.
 */
static mrb_value
int_idiv(mrb_state *mrb, mrb_value x)
{
#ifdef MRB_USE_BIGINT
  if (mrb_bigint_p(x)) {
    return mrb_bint_div(mrb, x, mrb_get_arg1(mrb));
  }
#endif
  mrb_int y = mrb_as_int(mrb,  mrb_get_arg1(mrb));
  return mrb_div_int_value(mrb, mrb_integer(x), y);
}

static mrb_value
int_quo(mrb_state *mrb, mrb_value x)
{
#ifndef MRB_USE_RATIONAL
#ifdef MRB_NO_FLOAT
  return int_idiv(mrb, x);
#else
  mrb_float y = mrb_as_float(mrb,  mrb_get_arg1(mrb));

  if (y == 0) {
    mrb_int_zerodiv(mrb);
  }
#ifdef MRB_USE_BIGINT
  if (mrb_bigint_p(x)) {
    return mrb_float_value(mrb, mrb_bint_as_float(mrb, x) / y);
  }
#endif
  return mrb_float_value(mrb, mrb_integer(x) / y);
#endif
#else
  mrb_int a = mrb_integer(x);
  mrb_value y = mrb_get_arg1(mrb);
  if (mrb_integer_p(y) && mrb_class_defined_id(mrb, MRB_SYM(Rational))) {
    return mrb_rational_new(mrb, a, mrb_integer(y));
  }
  switch (mrb_type(y)) {
  case MRB_TT_RATIONAL:
    x = mrb_rational_new(mrb, a, 1);
    return mrb_rational_div(mrb, x, y);
  default:
#ifndef MRB_NO_FLOAT
    return mrb_float_value(mrb, mrb_div_float((mrb_float)a, mrb_as_float(mrb, y)));
#else
    mrb_int_noconv(mrb, y);
    break;
#endif
  }
#endif
}

static mrb_value
coerce_step_counter(mrb_state *mrb, mrb_value self)
{
  mrb_value num, step;

  mrb_get_args(mrb, "oo", &num, &step);

#ifndef MRB_NO_FLOAT
  mrb->c->ci->mid = 0;
  if (mrb_float_p(num) || mrb_float_p(step)) {
    return mrb_ensure_float_type(mrb, self);
  }
#endif

  return self;
}

#ifndef MRB_NO_FLOAT
/********************************************************************
 *
 * Document-class: Float
 *
 *  <code>Float</code> objects represent inexact real numbers using
 *  the native architecture's double-precision floating-point
 *  representation.
 */

static mrb_value
flo_pow(mrb_state *mrb, mrb_value x)
{
  mrb_value y = mrb_get_arg1(mrb);
  mrb_float d = pow(mrb_as_float(mrb, x), mrb_as_float(mrb, y));
  return mrb_float_value(mrb, d);
}

static mrb_value
flo_idiv(mrb_state *mrb, mrb_value xv)
{
  mrb_float x = mrb_float(xv);
  mrb_check_num_exact(mrb, x);
  mrb_int y = mrb_as_int(mrb, mrb_get_arg1(mrb));
  return mrb_div_int_value(mrb, (mrb_int)x, y);
}

mrb_float
mrb_div_float(mrb_float x, mrb_float y)
{
  if (y != 0.0) {
    return x / y;
  }
  else if (x == 0.0) {
    return NAN;
  }
  else {
    return x * (signbit(y) ? -1.0 : 1.0) * INFINITY;
  }
}

/* 15.2.9.3.6 */
/*
 * call-seq:
 *   float / num  ->  float
 *
 * Returns a new Float which is the result of dividing float by num.
 */
static mrb_value
flo_div(mrb_state *mrb, mrb_value x)
{
  mrb_value y = mrb_get_arg1(mrb);
  mrb_float a = mrb_float(x);

  switch(mrb_type(y)) {
#ifdef MRB_USE_COMPLEX
  case MRB_TT_COMPLEX:
    return mrb_complex_div(mrb, mrb_complex_new(mrb, a, 0), y);
#endif
  case MRB_TT_FLOAT:
    a = mrb_div_float(a, mrb_float(y));
    return mrb_float_value(mrb, a);
  default:
    a = mrb_div_float(a, mrb_as_float(mrb, y));
    return mrb_float_value(mrb, a);
  }
  return mrb_float_value(mrb, a);
}

/* the argument `fmt` is no longer used; you can pass `NULL` */
mrb_value
mrb_float_to_str(mrb_state *mrb, mrb_value flo, const char *fmt)
{
  char buf[25];
#ifdef MRB_USE_FLOAT32
  const int prec =  7;
#else
  const int prec =  15;
#endif

  mrb_format_float(mrb_float(flo), buf, sizeof(buf), 'g', prec, '\0');
  for (char *p = buf; *p; p++) {
    if (*p == '.') goto exit;
    if (*p == 'e') {
      memmove(p+2, p, strlen(p)+1);
      p[0] = '.';
      p[1] = '0';
      goto exit;
    }
  }
  strcat(buf, ".0");
 exit:
  return mrb_str_new_cstr(mrb, buf);
}

/* 15.2.9.3.16(x) */
/*
 *  call-seq:
 *     flt.to_s  ->  string
 *     flt.inspect  ->  string
 *
 *  Returns a string containing a representation of self. As well as a
 *  fixed or exponential form of the number, the call may return
 *  "<code>NaN</code>", "<code>Infinity</code>", and
 *  "<code>-Infinity</code>".
 *
 *     3.0.to_s   #=> 3.0
 *     3.25.to_s  #=> 3.25
 */

static mrb_value
flo_to_s(mrb_state *mrb, mrb_value flt)
{
  mrb_float f = mrb_float(flt);
  mrb_value str;

  if (isinf(f)) {
    str = f < 0 ? mrb_str_new_lit(mrb, "-Infinity")
                : mrb_str_new_lit(mrb, "Infinity");
  }
  else if (isnan(f)) {
    str = mrb_str_new_lit(mrb, "NaN");
  }
  else {
    str = mrb_float_to_str(mrb, flt, NULL);
  }

  RSTR_SET_ASCII_FLAG(mrb_str_ptr(str));
  return str;
}

/* 15.2.9.3.3 */
/*
 * call-seq:
 *   float + other  ->  float
 *
 * Returns a new float which is the sum of <code>float</code>
 * and <code>other</code>.
 */
static mrb_value
flo_add(mrb_state *mrb, mrb_value x)
{
  mrb_value y = mrb_get_arg1(mrb);
  mrb_float a = mrb_float(x);

  switch (mrb_type(y)) {
  case MRB_TT_FLOAT:
    return mrb_float_value(mrb, a + mrb_float(y));
#if defined(MRB_USE_COMPLEX)
  case MRB_TT_COMPLEX:
    return mrb_complex_add(mrb, y, x);
#endif
  default:
    return mrb_float_value(mrb, a + mrb_as_float(mrb, y));
  }
}

/* 15.2.9.3.4 */
/*
 * call-seq:
 *   float - other  ->  float
 *
 * Returns a new float which is the difference of <code>float</code>
 * and <code>other</code>.
 */

static mrb_value
flo_sub(mrb_state *mrb, mrb_value x)
{
  mrb_value y = mrb_get_arg1(mrb);
  mrb_float a = mrb_float(x);

  switch (mrb_type(y)) {
  case MRB_TT_FLOAT:
    return mrb_float_value(mrb, a - mrb_float(y));
#if defined(MRB_USE_COMPLEX)
  case MRB_TT_COMPLEX:
    return mrb_complex_sub(mrb, mrb_complex_new(mrb, a, 0), y);
#endif
  default:
    return mrb_float_value(mrb, a - mrb_as_float(mrb, y));
  }
}

/* 15.2.9.3.5 */
/*
 * call-seq:
 *   float * other  ->  float
 *
 * Returns a new float which is the product of <code>float</code>
 * and <code>other</code>.
 */

static mrb_value
flo_mul(mrb_state *mrb, mrb_value x)
{
  mrb_value y = mrb_get_arg1(mrb);
  mrb_float a = mrb_float(x);

  switch (mrb_type(y)) {
  case MRB_TT_FLOAT:
    return mrb_float_value(mrb, a * mrb_float(y));
#if defined(MRB_USE_COMPLEX)
  case MRB_TT_COMPLEX:
    return mrb_complex_mul(mrb, y, x);
#endif
  default:
    return mrb_float_value(mrb, a * mrb_as_float(mrb, y));
  }
}

static void
flodivmod(mrb_state *mrb, double x, double y, mrb_float *divp, mrb_float *modp)
{
  double div, mod;

  if (isnan(y)) {
    /* y is NaN so all results are NaN */
    div = mod = y;
    goto exit;
  }
  if (y == 0.0) {
    mrb_int_zerodiv(mrb);
  }
  if (isinf(y) && !isinf(x)) {
    mod = x;
  }
  else {
    mod = fmod(x, y);
  }
  if (isinf(x) && !isinf(y)) {
    div = x;
  }
  else {
    div = (x - mod) / y;
    if (modp && divp) div = round(div);
  }
  if (div == 0) div = 0.0;
  if (mod == 0) mod = 0.0;
  if (y*mod < 0) {
    mod += y;
    div -= 1.0;
  }
 exit:
  if (modp) *modp = mod;
  if (divp) *divp = div;
}

/* 15.2.9.3.5 */
/*
 *  call-seq:
 *     flt % other        ->  float
 *     flt.modulo(other)  ->  float
 *
 *  Return the modulo after division of <code>flt</code> by <code>other</code>.
 *
 *     6543.21.modulo(137)      #=> 104.21
 *     6543.21.modulo(137.24)   #=> 92.9299999999996
 */

static mrb_value
flo_mod(mrb_state *mrb, mrb_value x)
{
  mrb_value y = mrb_get_arg1(mrb);
  mrb_float mod;

  flodivmod(mrb, mrb_float(x), mrb_as_float(mrb, y), NULL, &mod);
  return mrb_float_value(mrb, mod);
}
#endif

/* 15.2.8.3.16 */
/*
 *  call-seq:
 *     num.eql?(numeric)  ->  true or false
 *
 *  Returns <code>true</code> if <i>num</i> and <i>numeric</i> are the
 *  same type and have equal values.
 *
 *     1 == 1.0          #=> true
 *     1.eql?(1.0)       #=> false
 *     (1.0).eql?(1.0)   #=> true
 */
static mrb_value
num_eql(mrb_state *mrb, mrb_value x)
{
  mrb_value y = mrb_get_arg1(mrb);

#ifdef MRB_USE_BIGINT
  if (mrb_bigint_p(x)) {
    return mrb_bool_value(mrb_bint_cmp(mrb, x, y) == 0);
  }
#endif
#ifndef MRB_NO_FLOAT
  if (mrb_float_p(x)) {
    if (!mrb_float_p(y)) return mrb_false_value();
    return mrb_bool_value(mrb_float(x) == mrb_float(y));
  }
#endif
  if (mrb_integer_p(x)) {
    if (!mrb_integer_p(y)) return mrb_false_value();
    return mrb_bool_value(mrb_integer(x) == mrb_integer(y));
  }
  return mrb_bool_value(mrb_equal(mrb, x, y));
}

#ifndef MRB_NO_FLOAT
/* 15.2.9.3.7 */
/*
 *  call-seq:
 *     flt == obj  ->  true or false
 *
 *  Returns <code>true</code> only if <i>obj</i> has the same value
 *  as <i>flt</i>. Contrast this with <code>Float#eql?</code>, which
 *  requires <i>obj</i> to be a <code>Float</code>.
 *
 *     1.0 == 1   #=> true
 *
 */

static mrb_value
flo_eq(mrb_state *mrb, mrb_value x)
{
  mrb_value y = mrb_get_arg1(mrb);

  switch (mrb_type(y)) {
  case MRB_TT_INTEGER:
    return mrb_bool_value(mrb_float(x) == (mrb_float)mrb_integer(y));
  case MRB_TT_FLOAT:
    return mrb_bool_value(mrb_float(x) == mrb_float(y));
#ifdef MRB_USE_RATIONAL
  case MRB_TT_RATIONAL:
    return mrb_bool_value(mrb_float(x) == mrb_as_float(mrb, y));
#endif
#ifdef MRB_USE_COMPLEX
  case MRB_TT_COMPLEX:
    return mrb_bool_value(mrb_equal(mrb, y, x));
#endif
  default:
    return mrb_false_value();
  }
}

static int64_t
value_int64(mrb_state *mrb, mrb_value x)
{
  switch (mrb_type(x)) {
  case MRB_TT_INTEGER:
    return (int64_t)mrb_integer(x);
  case MRB_TT_FLOAT:
    {
      double f = mrb_float(x);

      if ((mrb_float)INT64_MAX >= f && f >= (mrb_float)INT64_MIN)
        return (int64_t)f;
    }
  default:
    mrb_raise(mrb, E_TYPE_ERROR, "cannot convert to Integer");
    break;
  }
  /* not reached */
  return 0;
}

static mrb_value
int64_value(mrb_state *mrb, int64_t v)
{
  if (!TYPED_FIXABLE(v,int64_t)) {
    mrb_int_overflow(mrb, "bit operation");
  }
  return mrb_fixnum_value((mrb_int)v);
}

static mrb_value
flo_rev(mrb_state *mrb, mrb_value x)
{
  int64_t v1 = value_int64(mrb, x);
  return int64_value(mrb, ~v1);
}

static mrb_value
flo_and(mrb_state *mrb, mrb_value x)
{
  mrb_value y = mrb_get_arg1(mrb);
  int64_t v1, v2;

  v1 = value_int64(mrb, x);
  v2 = value_int64(mrb, y);
  return int64_value(mrb, v1 & v2);
}

static mrb_value
flo_or(mrb_state *mrb, mrb_value x)
{
  mrb_value y = mrb_get_arg1(mrb);
  int64_t v1, v2;

  v1 = value_int64(mrb, x);
  v2 = value_int64(mrb, y);
  return int64_value(mrb, v1 | v2);
}

static mrb_value
flo_xor(mrb_state *mrb, mrb_value x)
{
  mrb_value y = mrb_get_arg1(mrb);
  int64_t v1, v2;

  v1 = value_int64(mrb, x);
  v2 = value_int64(mrb, y);
  return int64_value(mrb, v1 ^ v2);
}

static mrb_value
flo_shift(mrb_state *mrb, mrb_value x, mrb_int width)
{
  mrb_float val;

  if (width == 0) {
    return x;
  }
  val = mrb_float(x);
  if (width < -MRB_INT_BIT/2) {
    if (val < 0) return mrb_fixnum_value(-1);
    return mrb_fixnum_value(0);
  }
  if (width < 0) {
    while (width++) {
      val /= 2;
      if (val < 1.0) {
        val = 0;
        break;
      }
    }
#if defined(_ISOC99_SOURCE)
    val = trunc(val);
#else
    if (val > 0){
      val = floor(val);
    }
    else {
      val = ceil(val);
    }
#endif
    if (val == 0 && mrb_float(x) < 0) {
      return mrb_fixnum_value(-1);
    }
  }
  else {
    while (width--) {
      val *= 2;
    }
  }
  if (FIXABLE_FLOAT(val))
    return mrb_int_value(mrb, (mrb_int)val);
  return mrb_float_value(mrb, val);
}

static mrb_value
flo_rshift(mrb_state *mrb, mrb_value x)
{
  mrb_int width = mrb_as_int(mrb, mrb_get_arg1(mrb));
  if (width == MRB_INT_MIN) return flo_shift(mrb, x, -MRB_INT_BIT);
  return flo_shift(mrb, x, -width);
}

static mrb_value
flo_lshift(mrb_state *mrb, mrb_value x)
{
  mrb_int width = mrb_as_int(mrb, mrb_get_arg1(mrb));
  return flo_shift(mrb, x, width);
}

/* 15.2.9.3.13 */
/*
 * call-seq:
 *   flt.to_f  ->  self
 *
 * As <code>flt</code> is already a float, returns +self+.
 */

static mrb_value
flo_to_f(mrb_state *mrb, mrb_value num)
{
  return num;
}

/* 15.2.9.3.11 */
/*
 *  call-seq:
 *     flt.infinite?  ->  nil, -1, +1
 *
 *  Returns <code>nil</code>, -1, or +1 depending on whether <i>flt</i>
 *  is finite, -infinity, or +infinity.
 *
 *     (0.0).infinite?        #=> nil
 *     (-1.0/0.0).infinite?   #=> -1
 *     (+1.0/0.0).infinite?   #=> 1
 */

static mrb_value
flo_infinite_p(mrb_state *mrb, mrb_value num)
{
  mrb_float value = mrb_float(num);

  if (isinf(value)) {
    return mrb_fixnum_value(value < 0 ? -1 : 1);
  }
  return mrb_nil_value();
}

/* 15.2.9.3.9 */
/*
 *  call-seq:
 *     flt.finite?  ->  true or false
 *
 *  Returns <code>true</code> if <i>flt</i> is a valid IEEE floating
 *  point number (it is not infinite, and <code>nan?</code> is
 *  <code>false</code>).
 *
 */

static mrb_value
flo_finite_p(mrb_state *mrb, mrb_value num)
{
  return mrb_bool_value(isfinite(mrb_float(num)));
}

/*
 *  Document-class: FloatDomainError
 *
 *  Raised when attempting to convert special float values
 *  (in particular infinite or NaN)
 *  to numerical classes which don't support them.
 *
 *     Float::INFINITY.to_i
 *
 *  <em>raises the exception:</em>
 *
 *     FloatDomainError: Infinity
 */
/* ------------------------------------------------------------------------*/
void
mrb_check_num_exact(mrb_state *mrb, mrb_float num)
{
  if (isinf(num)) {
    mrb_raise(mrb, E_FLOATDOMAIN_ERROR, num < 0 ? "-Infinity" : "Infinity");
  }
  if (isnan(num)) {
    mrb_raise(mrb, E_FLOATDOMAIN_ERROR, "NaN");
  }
}

static mrb_value
flo_rounding_int(mrb_state *mrb, mrb_float f)
{
  if (!FIXABLE_FLOAT(f)) {
#ifdef MRB_USE_BIGINT
    return mrb_bint_new_float(mrb, f);
#else
    mrb_int_overflow(mrb, "rounding");
#endif
  }
  return mrb_int_value(mrb, (mrb_int)f);
}

static mrb_value
flo_rounding(mrb_state *mrb, mrb_value num, double (*func)(double))
{
  mrb_float f = mrb_float(num);
  mrb_int ndigits = 0;
#ifdef MRB_USE_FLOAT32
  const int fprec =  7;
#else
  const int fprec =  15;
#endif

  mrb_get_args(mrb, "|i", &ndigits);
  if (f == 0.0) {
    return ndigits > 0 ? mrb_float_value(mrb, f) : mrb_fixnum_value(0);
  }
  if (ndigits > 0) {
    if (ndigits > fprec) return num;
    mrb_float d = pow(10, (double)ndigits);
    f = func(f * d) / d;
    mrb_check_num_exact(mrb, f);
    return mrb_float_value(mrb, f);
  }
  if (ndigits < 0) {
    mrb_float d = pow(10, -(double)ndigits);
    f = func(f / d) * d;
  }
  else {                        /* ndigits == 0 */
    f = func(f);
  }
  mrb_check_num_exact(mrb, f);
  return flo_rounding_int(mrb, f);
}

/* 15.2.9.3.10 */
/*
 *  call-seq:
 *     float.floor([ndigits])  ->  integer or float
 *
 *  Returns the largest number less than or equal to +float+ with
 *  a precision of +ndigits+ decimal digits (default: 0).
 *
 *  When the precision is negative, the returned value is an integer
 *  with at least <code>ndigits.abs</code> trailing zeros.
 *
 *  Returns a floating-point number when +ndigits+ is positive,
 *  otherwise returns an integer.
 *
 *     1.2.floor      #=> 1
 *     2.0.floor      #=> 2
 *     (-1.2).floor   #=> -2
 *     (-2.0).floor   #=> -2
 *
 *     1.234567.floor(2)   #=> 1.23
 *     1.234567.floor(3)   #=> 1.234
 *     1.234567.floor(4)   #=> 1.2345
 *     1.234567.floor(5)   #=> 1.23456
 *
 *     34567.89.floor(-5)  #=> 0
 *     34567.89.floor(-4)  #=> 30000
 *     34567.89.floor(-3)  #=> 34000
 *     34567.89.floor(-2)  #=> 34500
 *     34567.89.floor(-1)  #=> 34560
 *     34567.89.floor(0)   #=> 34567
 *     34567.89.floor(1)   #=> 34567.8
 *     34567.89.floor(2)   #=> 34567.89
 *     34567.89.floor(3)   #=> 34567.89
 *
 *  Note that the limited precision of floating-point arithmetic
 *  might lead to surprising results:
 *
 *     (0.3 / 0.1).floor  #=> 2 (!)
 */
static mrb_value
flo_floor(mrb_state *mrb, mrb_value num)
{
  return flo_rounding(mrb, num, floor);
}

/* 15.2.9.3.8 */
/*
 *  call-seq:
 *     float.ceil([ndigits])  ->  integer or float
 *
 *  Returns the smallest number greater than or equal to +float+ with
 *  a precision of +ndigits+ decimal digits (default: 0).
 *
 *  When the precision is negative, the returned value is an integer
 *  with at least <code>ndigits.abs</code> trailing zeros.
 *
 *  Returns a floating-point number when +ndigits+ is positive,
 *  otherwise returns an integer.
 *
 *     1.2.ceil      #=> 2
 *     2.0.ceil      #=> 2
 *     (-1.2).ceil   #=> -1
 *     (-2.0).ceil   #=> -2
 *
 *     1.234567.ceil(2)   #=> 1.24
 *     1.234567.ceil(3)   #=> 1.235
 *     1.234567.ceil(4)   #=> 1.2346
 *     1.234567.ceil(5)   #=> 1.23457
 *
 *     34567.89.ceil(-5)  #=> 100000
 *     34567.89.ceil(-4)  #=> 40000
 *     34567.89.ceil(-3)  #=> 35000
 *     34567.89.ceil(-2)  #=> 34600
 *     34567.89.ceil(-1)  #=> 34570
 *     34567.89.ceil(0)   #=> 34568
 *     34567.89.ceil(1)   #=> 34567.9
 *     34567.89.ceil(2)   #=> 34567.89
 *     34567.89.ceil(3)   #=> 34567.89
 *
 *  Note that the limited precision of floating-point arithmetic
 *  might lead to surprising results:
 *
 *     (2.1 / 0.7).ceil  #=> 4 (!)
 */

static mrb_value
flo_ceil(mrb_state *mrb, mrb_value num)
{
  return flo_rounding(mrb, num, ceil);
}

/* 15.2.9.3.12 */
/*
 *  call-seq:
 *     flt.round([ndigits])  ->  integer or float
 *
 *  Rounds <i>flt</i> to a given precision in decimal digits (default 0 digits).
 *  Precision may be negative.  Returns a floating-point number when ndigits
 *  is more than zero.
 *
 *     1.4.round      #=> 1
 *     1.5.round      #=> 2
 *     1.6.round      #=> 2
 *     (-1.5).round   #=> -2
 *
 *     1.234567.round(2)  #=> 1.23
 *     1.234567.round(3)  #=> 1.235
 *     1.234567.round(4)  #=> 1.2346
 *     1.234567.round(5)  #=> 1.23457
 *
 *     34567.89.round(-5) #=> 0
 *     34567.89.round(-4) #=> 30000
 *     34567.89.round(-3) #=> 35000
 *     34567.89.round(-2) #=> 34600
 *     34567.89.round(-1) #=> 34570
 *     34567.89.round(0)  #=> 34568
 *     34567.89.round(1)  #=> 34567.9
 *     34567.89.round(2)  #=> 34567.89
 *     34567.89.round(3)  #=> 34567.89
 *
 */

static mrb_value
flo_round(mrb_state *mrb, mrb_value num)
{
  double number, f;
  mrb_int ndigits = 0;
  mrb_int i;

  mrb_get_args(mrb, "|i", &ndigits);
  number = mrb_float(num);

  if (0 < ndigits && (isinf(number) || isnan(number))) {
    return num;
  }
  mrb_check_num_exact(mrb, number);

  f = 1.0;
  if (ndigits < -DBL_DIG-2) return mrb_fixnum_value(0);
  i = ndigits >= 0 ? ndigits : -ndigits;
  if (ndigits > DBL_DIG+2) return num;
  while  (--i >= 0)
    f = f*10.0;

  if (isinf(f)) {
    if (ndigits < 0) number = 0;
  }
  else {
    double d;

    if (ndigits < 0) number /= f;
    else number *= f;

    /* home-made inline implementation of round(3) */
    if (number > 0.0) {
      d = floor(number);
      number = d + (number - d >= 0.5);
    }
    else if (number < 0.0) {
      d = ceil(number);
      number = d - (d - number >= 0.5);
    }

    if (ndigits < 0) number *= f;
    else number /= f;
  }

  if (ndigits > 0) {
    if (!isfinite(number)) return num;
    return mrb_float_value(mrb, number);
  }
  if (!FIXABLE_FLOAT(number))
    return mrb_float_value(mrb, number);
  return mrb_int_value(mrb, (mrb_int)number);
}

/* 15.2.9.3.14 */
static mrb_value
flo_to_i(mrb_state *mrb, mrb_value num)
{
  mrb_float f = mrb_float(num);

  mrb_check_num_exact(mrb, f);
  if (!FIXABLE_FLOAT(f)) {
#ifdef MRB_USE_BIGINT
    return mrb_bint_new_float(mrb, f);
#else
    mrb_int_overflow(mrb, "to_f");
#endif
  }
  if (f > 0.0) f = floor(f);
  if (f < 0.0) f = ceil(f);

  return mrb_int_value(mrb, (mrb_int)f);
}

/* 15.2.9.3.15 */
/*
 *  call-seq:
 *     flt.to_i      ->  integer
 *     flt.truncate  ->  integer
 *
 *  Returns <i>flt</i> truncated to an <code>Integer</code>.
 */

static mrb_value
flo_truncate(mrb_state *mrb, mrb_value num)
{
  if (signbit(mrb_float(num))) return flo_ceil(mrb, num);
  return flo_floor(mrb, num);
}

static mrb_value
flo_nan_p(mrb_state *mrb, mrb_value num)
{
  return mrb_bool_value(isnan(mrb_float(num)));
}

static mrb_value
flo_abs(mrb_state *mrb, mrb_value num)
{
  mrb_float f = mrb_float(num);

  if (signbit(f)) return mrb_float_value(mrb, -f);
  return num;
}
#endif

/*
 * Document-class: Integer
 *
 *  <code>Integer</code> is hold whole numbers.
 *
 */

/* 15.2.9.3.24 */
/*
 *  call-seq:
 *     int.to_i      ->  integer
 *
 *  As <i>int</i> is already an <code>Integer</code>, all these
 *  methods simply return the receiver.
 */

static mrb_value
int_to_i(mrb_state *mrb, mrb_value num)
{
  return num;
}

mrb_value
mrb_int_mul(mrb_state *mrb, mrb_value x, mrb_value y)
{
  mrb_int a;

  a = mrb_integer(x);
  if (mrb_integer_p(y)) {
    mrb_int b, c;

    if (a == 0) return x;
    if (a == 1) return y;
    b = mrb_integer(y);
    if (b == 0) return y;
    if (b == 1) return x;
    if (mrb_int_mul_overflow(a, b, &c)) {
#ifdef MRB_USE_BIGINT
      x = mrb_bint_new_int(mrb, a);
      return mrb_bint_mul(mrb, x, y);
#else
      mrb_int_overflow(mrb, "multiplication");
#endif
    }
    return mrb_int_value(mrb, c);
  }
  switch (mrb_type(y)) {
#ifdef MRB_USE_BIGINT
  case MRB_TT_BIGINT:
    if (a == 0) return x;
    if (a == 1) return y;
    return mrb_bint_mul(mrb, y, x);
#endif
#ifdef MRB_USE_RATIONAL
  case MRB_TT_RATIONAL:
    if (a == 0) return x;
    if (a == 1) return y;
    return mrb_rational_mul(mrb, y, x);
#endif
#ifdef MRB_USE_COMPLEX
  case MRB_TT_COMPLEX:
    if (a == 0) return x;
    if (a == 1) return y;
    return mrb_complex_mul(mrb, y, x);
#endif
#ifndef MRB_NO_FLOAT
  case MRB_TT_FLOAT:
    return mrb_float_value(mrb, (mrb_float)a * mrb_as_float(mrb, y));
#endif
  default:
    mrb_int_noconv(mrb, y);
  }
}

/* 15.2.8.3.5 */
/*
 * call-seq:
 *   int * numeric  ->  numeric_result
 *
 * Performs multiplication: the class of the resulting object depends on
 * the class of <code>numeric</code> and on the magnitude of the
 * result.
 */

static mrb_value
int_mul(mrb_state *mrb, mrb_value x)
{
  mrb_value y = mrb_get_arg1(mrb);

#ifdef MRB_USE_BIGINT
  if (mrb_bigint_p(x)) {
    return mrb_bint_mul(mrb, x, y);
  }
#endif
  return mrb_int_mul(mrb, x, y);
}

static void
intdivmod(mrb_state *mrb, mrb_int x, mrb_int y, mrb_int *divp, mrb_int *modp)
{
  if (y == 0) {
    mrb_int_zerodiv(mrb);
  }
  else if (x == MRB_INT_MIN && y == -1) {
    mrb_int_overflow(mrb, "division");
  }
  else {
    mrb_int div = x / y;
    mrb_int mod = x - div * y;

    if ((x ^ y) < 0 && x != div * y) {
      mod += y;
      div -= 1;
    }
    if (divp) *divp = div;
    if (modp) *modp = mod;
  }
}

/* 15.2.8.3.7 */
/*
 *  call-seq:
 *    int % num        ->  num
 *
 *  Returns <code>int</code> modulo <code>other</code>.
 *  See <code>numeric.divmod</code> for more information.
 */

static mrb_value
int_mod(mrb_state *mrb, mrb_value x)
{
  mrb_value y = mrb_get_arg1(mrb);
  mrb_int a, b;

#ifdef MRB_USE_BIGINT
  if (mrb_bigint_p(x)) {
    return mrb_bint_mod(mrb, x, y);
  }
#endif
  a = mrb_integer(x);
  if (a == 0) return x;
  if (mrb_integer_p(y)) {
    b = mrb_integer(y);
    if (b == 0) mrb_int_zerodiv(mrb);
    if (a == MRB_INT_MIN && b == -1) return mrb_fixnum_value(0);
    mrb_int mod = a % b;
    if ((a < 0) != (b < 0) && mod != 0) {
      mod += b;
    }
    return mrb_int_value(mrb, mod);
  }
#ifdef MRB_NO_FLOAT
  mrb_raise(mrb, E_TYPE_ERROR, "non integer modulo");
#else
  mrb_float mod;

  flodivmod(mrb, (mrb_float)a, mrb_as_float(mrb, y), NULL, &mod);
  return mrb_float_value(mrb, mod);
#endif
}

#ifndef MRB_NO_FLOAT
static mrb_value flo_divmod(mrb_state *mrb, mrb_value x);
#endif

/*
 *  call-seq:
 *     int.divmod(numeric)  ->  array
 *
 *  See <code>Numeric#divmod</code>.
 */
static mrb_value
int_divmod(mrb_state *mrb, mrb_value x)
{
  mrb_value y = mrb_get_arg1(mrb);

#ifdef MRB_USE_BIGINT
  if (mrb_bigint_p(x)) {
#ifndef MRB_NO_FLOAT
    if (mrb_float_p(y)) {
      mrb_float f = mrb_bint_as_float(mrb, x);
      return flo_divmod(mrb, mrb_float_value(mrb, f));
    }
#endif
    return mrb_bint_divmod(mrb, x, y);
  }
#endif
  if (mrb_integer_p(y)) {
    mrb_int div, mod;

    intdivmod(mrb, mrb_integer(x), mrb_integer(y), &div, &mod);
    return mrb_assoc_new(mrb, mrb_int_value(mrb, div), mrb_int_value(mrb, mod));
  }
#ifdef MRB_NO_FLOAT
  mrb_raise(mrb, E_TYPE_ERROR, "non integer divmod");
#else
  return flo_divmod(mrb, x);
#endif
}

#ifndef MRB_NO_FLOAT
static mrb_value
flo_divmod(mrb_state *mrb, mrb_value x)
{
  mrb_value y = mrb_get_arg1(mrb);
  mrb_float div, mod;
  mrb_value a, b;

  flodivmod(mrb, mrb_float(x), mrb_as_float(mrb, y), &div, &mod);
  if (!FIXABLE_FLOAT(div))
    a = mrb_float_value(mrb, div);
  else
    a = mrb_int_value(mrb, (mrb_int)div);
  b = mrb_float_value(mrb, mod);
  return mrb_assoc_new(mrb, a, b);
}
#endif

/* 15.2.8.3.2 */
/*
 * call-seq:
 *   int == other  ->  true or false
 *
 * Return <code>true</code> if <code>int</code> equals <code>other</code>
 * numerically.
 *
 *   1 == 2      #=> false
 *   1 == 1.0    #=> true
 */

static mrb_value
int_equal(mrb_state *mrb, mrb_value x)
{
  mrb_value y = mrb_get_arg1(mrb);

  switch (mrb_type(y)) {
  case MRB_TT_INTEGER:
    return mrb_bool_value(mrb_integer(x) == mrb_integer(y));
#ifndef MRB_NO_FLOAT
  case MRB_TT_FLOAT:
    return mrb_bool_value((mrb_float)mrb_integer(x) == mrb_float(y));
#endif
#ifdef MRB_USE_BIGINT
  case MRB_TT_BIGINT:
    return mrb_bool_value(mrb_bint_cmp(mrb, y, x) == 0);
#endif
#ifdef MRB_USE_RATIONAL
  case MRB_TT_RATIONAL:
    return mrb_bool_value(mrb_equal(mrb, y, x));
#endif
#ifdef MRB_USE_COMPLEX
  case MRB_TT_COMPLEX:
    return mrb_bool_value(mrb_equal(mrb, y, x));
#endif
  default:
    return mrb_false_value();
  }
}

/* 15.2.8.3.8 */
/*
 * call-seq:
 *   ~int  ->  integer
 *
 * One's complement: returns a number where each bit is flipped.
 *   ex.0---00001 (1)-> 1---11110 (-2)
 *   ex.0---00010 (2)-> 1---11101 (-3)
 *   ex.0---00100 (4)-> 1---11011 (-5)
 */

static mrb_value
int_rev(mrb_state *mrb, mrb_value num)
{
#ifdef MRB_USE_BIGINT
  if (mrb_bigint_p(num)) {
    return mrb_bint_rev(mrb, num);
  }
#endif
  mrb_int val = mrb_integer(num);
  return mrb_int_value(mrb, ~val);
}

#ifdef MRB_NO_FLOAT
#define bit_op(x,y,op1,op2) do {\
  return mrb_int_value(mrb, (mrb_integer(x) op2 mrb_integer(y)));\
} while(0)
#else
static mrb_value flo_and(mrb_state *mrb, mrb_value x);
static mrb_value flo_or(mrb_state *mrb, mrb_value x);
static mrb_value flo_xor(mrb_state *mrb, mrb_value x);
#define bit_op(x,y,op1,op2) do {\
  if (mrb_integer_p(y)) return mrb_int_value(mrb, (mrb_integer(x) op2 mrb_integer(y))); \
  return flo_ ## op1(mrb, mrb_float_value(mrb, (mrb_float)mrb_integer(x)));\
} while(0)
#endif

/* 15.2.8.3.9 */
/*
 * call-seq:
 *   int & integer  ->  integer_result
 *
 * Bitwise AND.
 */

static mrb_value
int_and(mrb_state *mrb, mrb_value x)
{
  mrb_value y = mrb_get_arg1(mrb);

#ifdef MRB_USE_BIGINT
  if (mrb_bigint_p(x)) {
    return mrb_bint_and(mrb, x, y);
  }
  if (mrb_bigint_p(y)) {
    return mrb_bint_and(mrb, mrb_as_bint(mrb, x), y);
  }
#endif
  bit_op(x, y, and, &);
}

/* 15.2.8.3.10 */
/*
 * call-seq:
 *   int | integer  ->  integer_result
 *
 * Bitwise OR.
 */

static mrb_value
int_or(mrb_state *mrb, mrb_value x)
{
  mrb_value y = mrb_get_arg1(mrb);

#ifdef MRB_USE_BIGINT
  if (mrb_bigint_p(x)) {
    return mrb_bint_or(mrb, x, y);
  }
  if (mrb_bigint_p(y)) {
    return mrb_bint_or(mrb, mrb_as_bint(mrb, x), y);
  }
#endif
  bit_op(x, y, or, |);
}

/* 15.2.8.3.11 */
/*
 * call-seq:
 *   int ^ integer  ->  integer_result
 *
 * Bitwise EXCLUSIVE OR.
 */

static mrb_value
int_xor(mrb_state *mrb, mrb_value x)
{
  mrb_value y = mrb_get_arg1(mrb);

#ifdef MRB_USE_BIGINT
  if (mrb_bigint_p(x)) {
    return mrb_bint_xor(mrb, x, y);
  }
  if (mrb_bigint_p(y)) {
    return mrb_bint_xor(mrb, mrb_as_bint(mrb, x), y);
  }
#endif
  bit_op(x, y, or, ^);
}

#define NUMERIC_SHIFT_WIDTH_MAX (MRB_INT_BIT-1)

mrb_bool
mrb_num_shift(mrb_state *mrb, mrb_int val, mrb_int width, mrb_int *num)
{
  if (width < 0) {              /* rshift */
    if (width == MRB_INT_MIN || -width >= NUMERIC_SHIFT_WIDTH_MAX) {
      if (val < 0) {
        *num = -1;
      }
      else {
        *num = 0;
      }
    }
    else {
      *num = val >> -width;
    }
  }
  else if (val > 0) {
    if ((width > NUMERIC_SHIFT_WIDTH_MAX) ||
        (val   > (MRB_INT_MAX >> width))) {
      return FALSE;
    }
    *num = val << width;
  }
  else {
    if ((width > NUMERIC_SHIFT_WIDTH_MAX) ||
        (val   < (MRB_INT_MIN >> width))) {
      return FALSE;
    }
    if (width == NUMERIC_SHIFT_WIDTH_MAX)
      *num = MRB_INT_MIN;
    else
      *num = val * ((mrb_int)1 << width);
  }
  return TRUE;
}

/* 15.2.8.3.12 */
/*
 * call-seq:
 *   int << count  ->  integer or float
 *
 * Shifts _int_ left _count_ positions (right if _count_ is negative).
 */

static mrb_value
int_lshift(mrb_state *mrb, mrb_value x)
{
  mrb_int width, val;

  width = mrb_as_int(mrb, mrb_get_arg1(mrb));
  if (width == 0) {
    return x;
  }
  if (width == MRB_INT_MIN) mrb_int_overflow(mrb, "bit shift");
#ifdef MRB_USE_BIGINT
  if (mrb_bigint_p(x)) {
    return mrb_bint_lshift(mrb, x, width);
  }
#endif
  val = mrb_integer(x);
  if (val == 0) return x;
  if (!mrb_num_shift(mrb, val, width, &val)) {
#ifdef MRB_USE_BIGINT
    return mrb_bint_lshift(mrb, mrb_bint_new_int(mrb, val), width);
#else
    mrb_int_overflow(mrb, "bit shift");
#endif
  }
  return mrb_int_value(mrb, val);
}

/* 15.2.8.3.13 */
/*
 * call-seq:
 *   int >> count  ->  integer or float
 *
 * Shifts _int_ right _count_ positions (left if _count_ is negative).
 */

static mrb_value
int_rshift(mrb_state *mrb, mrb_value x)
{
  mrb_int width, val;

  width = mrb_as_int(mrb, mrb_get_arg1(mrb));
  if (width == 0) {
    return x;
  }
  if (width == MRB_INT_MIN) mrb_int_overflow(mrb, "bit shift");
#ifdef MRB_USE_BIGINT
  if (mrb_bigint_p(x)) {
    return mrb_bint_rshift(mrb, x, width);
  }
#endif
  val = mrb_integer(x);
  if (val == 0) return x;
  if (!mrb_num_shift(mrb, val, -width, &val)) {
#ifdef MRB_USE_BIGINT
    return mrb_bint_rshift(mrb, mrb_bint_new_int(mrb, val), width);
#else
    mrb_int_overflow(mrb, "bit shift");
#endif
  }
  return mrb_int_value(mrb, val);
}

static mrb_value
prepare_int_rounding(mrb_state *mrb, mrb_value x)
{
  mrb_int nd = 0;
  size_t bytes;

  mrb_get_args(mrb, "|i", &nd);
  if (nd >= 0) {
    return mrb_nil_value();
  }
#ifdef MRB_USE_BIGINT
  if (mrb_bigint_p(x)) {
    bytes = mrb_bint_memsize(x);
  }
  else
#endif
    bytes = sizeof(mrb_int);
  if (-0.415241 * nd - 0.125 > bytes) {
    return mrb_undef_value();
  }
  return mrb_int_pow(mrb, mrb_fixnum_value(10), mrb_fixnum_value(-nd));
}

/* 15.2.8.3.14 Integer#ceil */
/*
 *  call-seq:
 *     int.ceil          ->  int
 *     int.ceil(ndigits) ->  int
 *
 *  Returns self.
 *
 *  When the precision (ndigits) is negative, the returned value is an integer
 *  with at least <code>ndigits.abs</code> trailing zeros.
 */
static mrb_value
int_ceil(mrb_state *mrb, mrb_value x)
{
  mrb_value f = prepare_int_rounding(mrb, x);
  if (mrb_undef_p(f)) return mrb_fixnum_value(0);
  if (mrb_nil_p(f)) return x;
#ifdef MRB_USE_BIGINT
  if (mrb_bigint_p(x)) {
    x = mrb_bint_add_d(mrb, x, f);
    return mrb_bint_sub(mrb, x, mrb_bint_mod(mrb, x, f));
  }
#endif
  mrb_int a = mrb_integer(x);
  mrb_int b = mrb_integer(f);
  mrb_int c = a % b;
  int neg = a < 0;
  a -= c;
  if (!neg) {
    if (mrb_int_add_overflow(a, b, &c)) {
#ifdef MRB_USE_BIGINT
      x = mrb_bint_new_int(mrb, a);
      return mrb_bint_add(mrb, x, f);
#else
      mrb_int_overflow(mrb, "ceil");
#endif
    }
    a = c;
  }
  return mrb_int_value(mrb, a);
}

/* 15.2.8.3.17 Integer#floor */
/*
 *  call-seq:
 *     int.floor          ->  int
 *     int.floor(ndigits) ->  int
 *
 *  Returns self.
 *
 *  When the precision (ndigits) is negative, the returned value is an integer
 *  with at least <code>ndigits.abs</code> trailing zeros.
 */
static mrb_value
int_floor(mrb_state *mrb, mrb_value x)
{
  mrb_value f = prepare_int_rounding(mrb, x);
  if (mrb_undef_p(f)) return mrb_fixnum_value(0);
  if (mrb_nil_p(f)) return x;
#ifdef MRB_USE_BIGINT
  if (mrb_bigint_p(x)) {
    return mrb_bint_sub(mrb, x, mrb_bint_mod(mrb, x, f));
  }
#endif
  mrb_int a = mrb_integer(x);
  mrb_int b = mrb_integer(f);
  mrb_int c = a % b;
  int neg = a < 0;
  a -= c;
  if (neg) {
    if (mrb_int_sub_overflow(a, b, &c)) {
#ifdef MRB_USE_BIGINT
      x = mrb_bint_new_int(mrb, a);
      return mrb_bint_sub(mrb, x, f);
#else
      mrb_int_overflow(mrb, "floor");
#endif
    }
    a = c;
  }
  return mrb_int_value(mrb, a);
}

/* 15.2.8.3.20 Integer#round */
/*
 *  call-seq:
 *     int.round          ->  int
 *     int.round(ndigits) ->  int
 *
 *  Returns self.
 *
 *  When the precision (ndigits) is negative, the returned value is an integer
 *  with at least <code>ndigits.abs</code> trailing zeros.
 */
static mrb_value
int_round(mrb_state *mrb, mrb_value x)
{
  mrb_value f = prepare_int_rounding(mrb, x);
  if (mrb_undef_p(f)) return mrb_fixnum_value(0);
  if (mrb_nil_p(f)) return x;
#ifdef MRB_USE_BIGINT
  if (mrb_bigint_p(x)) {
    mrb_value r = mrb_bint_mod(mrb, x, f);
    mrb_value n = mrb_bint_sub(mrb, x, r);
    mrb_value h = mrb_bigint_p(f) ? mrb_bint_rshift(mrb, f, 1) : mrb_int_value(mrb, mrb_integer(f)>>1);
    mrb_int cmp = mrb_bigint_p(r) ? mrb_bint_cmp(mrb, r, h) : (mrb_bigint_p(h) ? -mrb_bint_cmp(mrb, h, r) : (mrb_integer(r)-mrb_integer(h)));
    if ((cmp > 0) || (cmp == 0 && mrb_bint_cmp(mrb, x, mrb_fixnum_value(0)) > 0)) {
      n = mrb_as_bint(mrb, n);
      n = mrb_bint_add(mrb, n, f);
    }
    return n;
  }
#endif
  mrb_int a = mrb_integer(x);
  mrb_int b = mrb_integer(f);
  mrb_int c = a % b;
  a -= c;
  if (c < 0) {
    c = -c;
    if (b/2 < c) {
      if (mrb_int_sub_overflow(a, b, &c)) {
#ifdef MRB_USE_BIGINT
        x = mrb_bint_new_int(mrb, a);
        return mrb_bint_sub(mrb, x, f);
#else
        mrb_int_overflow(mrb, "round");
#endif
      }
    }
    a = c;
  }
  else {
    if (b/2 < c) {
      if (mrb_int_add_overflow(a, b, &c)) {
#ifdef MRB_USE_BIGINT
        x = mrb_bint_new_int(mrb, a);
        return mrb_bint_add(mrb, x, f);
#else
        mrb_int_overflow(mrb, "round");
#endif
      }
    }
    a = c;
  }
  return mrb_int_value(mrb, a);
}

/* 15.2.8.3.26 Integer#truncate */
/*
 *  call-seq:
 *     int.truncate          ->  int
 *     int.truncate(ndigits) ->  int
 *
 *  Returns self.
 *
 *  When the precision (ndigits) is negative, the returned value is an integer
 *  with at least <code>ndigits.abs</code> trailing zeros.
 */
static mrb_value
int_truncate(mrb_state *mrb, mrb_value x)
{
  mrb_value f = prepare_int_rounding(mrb, x);
  if (mrb_undef_p(f)) return mrb_fixnum_value(0);
  if (mrb_nil_p(f)) return x;
#ifdef MRB_USE_BIGINT
  if (mrb_bigint_p(x)) {
    mrb_value m = mrb_bint_mod(mrb, x, f);
    x = mrb_bint_sub_d(mrb, x, m);
    if (mrb_bint_cmp(mrb, x, mrb_fixnum_value(0)) < 0) {
      return mrb_bint_add(mrb, x, f);
    }
    return x;
  }
#endif
  mrb_int a = mrb_integer(x);
  mrb_int b = mrb_integer(f);
  return mrb_int_value(mrb, a - (a % b));
}

/* 15.2.8.3.23 */
/*
 *  call-seq:
 *     int.to_f  ->  float
 *
 *  Converts <i>int</i> to a <code>Float</code>.
 *
 */

#ifndef MRB_NO_FLOAT
static mrb_value
int_to_f(mrb_state *mrb, mrb_value num)
{
#ifdef MRB_USE_BIGINT
  if (mrb_bigint_p(num)) {
    return mrb_float_value(mrb, mrb_bint_as_float(mrb, num));
  }
#endif
  return mrb_float_value(mrb, (mrb_float)mrb_integer(num));
}

MRB_API mrb_value
mrb_float_to_integer(mrb_state *mrb, mrb_value x)
{
  if (!mrb_float_p(x)) {
    mrb_raise(mrb, E_TYPE_ERROR, "non float value");
  }
  mrb_float f = mrb_float(x);
  if (isinf(f) || isnan(f)) {
    mrb_raisef(mrb, E_RANGE_ERROR, "float %f out of range", f);
  }
  return flo_to_i(mrb, x);
}
#endif

mrb_value
mrb_int_add(mrb_state *mrb, mrb_value x, mrb_value y)
{
  mrb_int a;

  a = mrb_integer(x);
  if (mrb_integer_p(y)) {
    mrb_int b, c;

    if (a == 0) return y;
    b = mrb_integer(y);
    if (b == 0) return x;
    if (mrb_int_add_overflow(a, b, &c)) {
#ifdef MRB_USE_BIGINT
      x = mrb_bint_new_int(mrb, a);
      return mrb_bint_add(mrb, x, y);
#else
      mrb_int_overflow(mrb, "addition");
#endif
    }
    return mrb_int_value(mrb, c);
  }
  switch (mrb_type(y)) {
#ifdef MRB_USE_BIGINT
  case MRB_TT_BIGINT:
    return mrb_bint_add(mrb, y, x);
#endif
#ifdef MRB_USE_RATIONAL
  case MRB_TT_RATIONAL:
    return mrb_rational_add(mrb, y, x);
#endif
#ifdef MRB_USE_COMPLEX
  case MRB_TT_COMPLEX:
    return mrb_complex_add(mrb, y, x);
#endif
  default:
#ifdef MRB_NO_FLOAT
    mrb_raise(mrb, E_TYPE_ERROR, "non integer addition");
#else
    return mrb_float_value(mrb, (mrb_float)a + mrb_as_float(mrb, y));
#endif
  }
}

/* 15.2.8.3.3 */
/*
 * call-seq:
 *   int + numeric  ->  numeric_result
 *
 * Performs addition: the class of the resulting object depends on
 * the class of <code>numeric</code> and on the magnitude of the
 * result.
 */
static mrb_value
int_add(mrb_state *mrb, mrb_value self)
{
  mrb_value other = mrb_get_arg1(mrb);

#ifdef MRB_USE_BIGINT
  if (mrb_bigint_p(self)) {
    return mrb_bint_add(mrb, self, other);
  }
#endif
  return mrb_int_add(mrb, self, other);
}

mrb_value
mrb_int_sub(mrb_state *mrb, mrb_value x, mrb_value y)
{
  mrb_int a;

  a = mrb_integer(x);
  if (mrb_integer_p(y)) {
    mrb_int b, c;

    b = mrb_integer(y);
    if (mrb_int_sub_overflow(a, b, &c)) {
#ifdef MRB_USE_BIGINT
      x = mrb_bint_new_int(mrb, a);
      return mrb_bint_sub(mrb, x, y);
#else
      mrb_int_overflow(mrb, "subtraction");
#endif
    }
    return mrb_int_value(mrb, c);
  }
  switch (mrb_type(y)) {
#ifdef MRB_USE_BIGINT
  case MRB_TT_BIGINT:
    return mrb_bint_sub(mrb, mrb_bint_new_int(mrb, a), y);
#endif
#ifdef MRB_USE_RATIONAL
  case MRB_TT_RATIONAL:
    return mrb_rational_sub(mrb, mrb_rational_new(mrb, a, 1), y);
#endif
#ifdef MRB_USE_COMPLEX
  case MRB_TT_COMPLEX:
    return mrb_complex_sub(mrb, mrb_complex_new(mrb, (mrb_float)a, 0), y);
#endif
  default:
#ifdef MRB_NO_FLOAT
    mrb_raise(mrb, E_TYPE_ERROR, "non integer subtraction");
#else
    return mrb_float_value(mrb, (mrb_float)a - mrb_as_float(mrb, y));
#endif
  }
}

/* 15.2.8.3.4 */
/*
 * call-seq:
 *   int - numeric  ->  numeric
 *
 * Performs subtraction: the class of the resulting object depends on
 * the class of <code>numeric</code> and on the magnitude of the
 * result.
 */
static mrb_value
int_sub(mrb_state *mrb, mrb_value self)
{
  mrb_value other = mrb_get_arg1(mrb);

#ifdef MRB_USE_BIGINT
  if (mrb_bigint_p(self)) {
    return mrb_bint_sub(mrb, self, other);
  }
#endif
  return mrb_int_sub(mrb, self, other);
}

MRB_API char*
mrb_int_to_cstr(char *buf, size_t len, mrb_int n, mrb_int base)
{
  char *bufend = buf + len;
  char *b = bufend-1;

  if (base < 2 || 36 < base) return NULL;
  if (len < 2) return NULL;

  if (n == 0) {
    buf[0] = '0';
    buf[1] = '\0';
    return buf;
  }

  *b = '\0';
  if (n < 0) {
    do {
      if (b-- == buf) return NULL;
      *b = mrb_digitmap[-(n % base)];
    } while (n /= base);
    if (b-- == buf) return NULL;
    *b = '-';
  }
  else {
    do {
      if (b-- == buf) return NULL;
      *b = mrb_digitmap[(int)(n % base)];
    } while (n /= base);
  }
  return b;
}

MRB_API mrb_value
mrb_integer_to_str(mrb_state *mrb, mrb_value x, mrb_int base)
{
  char buf[MRB_INT_BIT+1];
  mrb_int val = mrb_integer(x);

  if (base < 2 || 36 < base) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "invalid radix %i", base);
  }
#ifdef MRB_USE_BIGINT
  if (mrb_bigint_p(x)) {
    return mrb_bint_to_s(mrb, x, base);
  }
#endif
  const char *p = mrb_int_to_cstr(buf, sizeof(buf), val, base);
  mrb_assert(p != NULL);
  mrb_value str = mrb_str_new_cstr(mrb, p);
  RSTR_SET_ASCII_FLAG(mrb_str_ptr(str));
  return str;
}

/* 15.2.8.3.25 */
/*
 *  call-seq:
 *     int.to_s(base=10)  ->  string
 *
 *  Returns a string containing the representation of <i>int</i> radix
 *  <i>base</i> (between 2 and 36).
 *
 *     12345.to_s       #=> "12345"
 *     12345.to_s(2)    #=> "11000000111001"
 *     12345.to_s(8)    #=> "30071"
 *     12345.to_s(10)   #=> "12345"
 *     12345.to_s(16)   #=> "3039"
 *     12345.to_s(36)   #=> "9ix"
 *
 */
static mrb_value
int_to_s(mrb_state *mrb, mrb_value self)
{
  mrb_int base;

  if (mrb_get_argc(mrb) > 0) {
    base = mrb_integer(mrb_get_arg1(mrb));
  }
  else {
    base = 10;
  }
  return mrb_integer_to_str(mrb, self, base);
}

/* compare two numbers: (1:0:-1; -2 for error) */
static mrb_int
cmpnum(mrb_state *mrb, mrb_value v1, mrb_value v2)
{
#ifdef MRB_USE_BIGINT
  if (mrb_bigint_p(v1)) {
    return mrb_bint_cmp(mrb, v1, v2);
  }
#endif

#ifdef MRB_NO_FLOAT
  mrb_int x, y;
#else
  mrb_float x, y;
#endif

#ifdef MRB_NO_FLOAT
  x = mrb_integer(v1);
#else
  x = mrb_as_float(mrb, v1);
#endif
  switch (mrb_type(v2)) {
  case MRB_TT_INTEGER:
#ifdef MRB_NO_FLOAT
    y = mrb_integer(v2);
#else
    y = (mrb_float)mrb_integer(v2);
#endif
    break;
#ifndef MRB_NO_FLOAT
  case MRB_TT_FLOAT:
    y = mrb_float(v2);
    break;
#ifdef MRB_USE_RATIONAL
  case MRB_TT_RATIONAL:
    y = mrb_as_float(mrb, v2);
    break;
#endif
#endif
  default:
    return -2;
  }
  if (x > y)
    return 1;
  else {
    if (x < y)
      return -1;
    return 0;
  }
}

static mrb_value
int_hash(mrb_state *mrb, mrb_value self)
{
#ifdef MRB_USE_BIGINT
  if (mrb_bigint_p(self)) {
    return mrb_bint_hash(mrb, self);
  }
#endif
  mrb_int n = mrb_integer(self);
  return mrb_int_value(mrb, mrb_byte_hash((uint8_t*)&n, sizeof(n)));
}

/* 15.2.8.3.1 */
/* 15.2.9.3.1 */
/*
 * call-seq:
 *     self.f <=> other.f    => -1, 0, +1, or nil
 *             <  => -1
 *             =  =>  0
 *             >  => +1
 *  Comparison---Returns -1, 0, or +1 depending on whether <i>int</i> is
 *  less than, equal to, or greater than <i>numeric</i>. This is the
 *  basis for the tests in <code>Comparable</code>. When the operands are
 *  not comparable, it returns nil instead of raising an exception.
 */
static mrb_value
num_cmp(mrb_state *mrb, mrb_value self)
{
  mrb_value other = mrb_get_arg1(mrb);
  mrb_int n;

  n = cmpnum(mrb, self, other);
  if (n == -2) return mrb_nil_value();
  return mrb_fixnum_value(n);
}

static mrb_noreturn void
cmperr(mrb_state *mrb, mrb_value v1, mrb_value v2)
{
  mrb_raisef(mrb, E_ARGUMENT_ERROR, "comparison of %t with %t failed", v1, v2);
}

static mrb_value
num_lt(mrb_state *mrb, mrb_value self)
{
  mrb_value other = mrb_get_arg1(mrb);
  mrb_int n;

  n = cmpnum(mrb, self, other);
  if (n == -2) cmperr(mrb, self, other);
  if (n < 0) return mrb_true_value();
  return mrb_false_value();
}

static mrb_value
num_le(mrb_state *mrb, mrb_value self)
{
  mrb_value other = mrb_get_arg1(mrb);
  mrb_int n;

  n = cmpnum(mrb, self, other);
  if (n == -2) cmperr(mrb, self, other);
  if (n <= 0) return mrb_true_value();
  return mrb_false_value();
}

static mrb_value
num_gt(mrb_state *mrb, mrb_value self)
{
  mrb_value other = mrb_get_arg1(mrb);
  mrb_int n;

  n = cmpnum(mrb, self, other);
  if (n == -2) cmperr(mrb, self, other);
  if (n > 0) return mrb_true_value();
  return mrb_false_value();
}

static mrb_value
num_ge(mrb_state *mrb, mrb_value self)
{
  mrb_value other = mrb_get_arg1(mrb);
  mrb_int n;

  n = cmpnum(mrb, self, other);
  if (n == -2) cmperr(mrb, self, other);
  if (n >= 0) return mrb_true_value();
  return mrb_false_value();
}

MRB_API mrb_int
mrb_cmp(mrb_state *mrb, mrb_value obj1, mrb_value obj2)
{
  mrb_value v;

  switch (mrb_type(obj1)) {
  case MRB_TT_INTEGER:
  case MRB_TT_FLOAT:
  case MRB_TT_BIGINT:
    return cmpnum(mrb, obj1, obj2);
  case MRB_TT_STRING:
    if (!mrb_string_p(obj2))
      return -2;
    return mrb_str_cmp(mrb, obj1, obj2);
  default:
    if (!mrb_respond_to(mrb, obj1, MRB_OPSYM(cmp))) return -2;
    v = mrb_funcall_argv(mrb, obj1, MRB_OPSYM(cmp), 1, &obj2);
    if (mrb_nil_p(v) || !mrb_integer_p(v))
      return -2;
    return mrb_integer(v);
  }
}

static mrb_value
num_finite_p(mrb_state *mrb, mrb_value self)
{
  return mrb_true_value();
}

static mrb_value
num_infinite_p(mrb_state *mrb, mrb_value self)
{
  return mrb_false_value();
}

#ifndef MRB_NO_FLOAT
static mrb_value
flo_hash(mrb_state *mrb, mrb_value flo)
{
  mrb_float f = mrb_float(flo);
  /* normalize -0.0 to 0.0 */
  if (f == 0) f = 0.0;
  return mrb_int_value(mrb, (mrb_int)mrb_byte_hash((uint8_t*)&f, sizeof(f)));
}
#endif

/* ------------------------------------------------------------------------*/
void
mrb_init_numeric(mrb_state *mrb)
{
  struct RClass *numeric, *integer;
#ifndef MRB_NO_FLOAT
  struct RClass *fl;
#endif

  /* Numeric Class */
  numeric = mrb_define_class_id(mrb, MRB_SYM(Numeric), mrb->object_class);                  /* 15.2.7 */
  mrb_define_method_id(mrb, numeric, MRB_SYM_Q(finite),  num_finite_p,    MRB_ARGS_NONE());
  mrb_define_method_id(mrb, numeric, MRB_SYM_Q(infinite),num_infinite_p,  MRB_ARGS_NONE());
  mrb_define_method_id(mrb, numeric, MRB_SYM_Q(eql),     num_eql,         MRB_ARGS_REQ(1)); /* 15.2.8.3.16 */

  /* Integer Class */
  mrb->integer_class = integer = mrb_define_class_id(mrb, MRB_SYM(Integer),  numeric);     /* 15.2.8 */
  MRB_SET_INSTANCE_TT(integer, MRB_TT_INTEGER);
  MRB_UNDEF_ALLOCATOR(integer);
  mrb_undef_class_method_id(mrb, integer, MRB_SYM(new));
  mrb_define_method_id(mrb, integer, MRB_OPSYM(pow),    int_pow,         MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, integer, MRB_OPSYM(cmp),    num_cmp,         MRB_ARGS_REQ(1)); /* 15.2.8.3.1  */
  mrb_define_method_id(mrb, integer, MRB_OPSYM(lt),     num_lt,          MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, integer, MRB_OPSYM(le),     num_le,          MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, integer, MRB_OPSYM(gt),     num_gt,          MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, integer, MRB_OPSYM(ge),     num_ge,          MRB_ARGS_REQ(1));

  mrb_define_method_id(mrb, integer, MRB_SYM(to_i),     int_to_i,        MRB_ARGS_NONE()); /* 15.2.8.3.24 */
  mrb_define_method_id(mrb, integer, MRB_SYM(to_int),   int_to_i,        MRB_ARGS_NONE());

  mrb_define_method_id(mrb, integer, MRB_OPSYM(add),    int_add,         MRB_ARGS_REQ(1)); /* 15.2.8.3.1 */
  mrb_define_method_id(mrb, integer, MRB_OPSYM(sub),    int_sub,         MRB_ARGS_REQ(1)); /* 15.2.8.3.2 */
  mrb_define_method_id(mrb, integer, MRB_OPSYM(mul),    int_mul,         MRB_ARGS_REQ(1)); /* 15.2.8.3.3 */
  mrb_define_method_id(mrb, integer, MRB_OPSYM(mod),    int_mod,         MRB_ARGS_REQ(1)); /* 15.2.8.3.5 */
  mrb_define_method_id(mrb, integer, MRB_OPSYM(div),    int_div,         MRB_ARGS_REQ(1)); /* 15.2.8.3.6 */
  mrb_define_method_id(mrb, integer, MRB_SYM(quo),      int_quo,         MRB_ARGS_REQ(1)); /* 15.2.7.4.5(x) */
  mrb_define_method_id(mrb, integer, MRB_SYM(div),      int_idiv,        MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, integer, MRB_OPSYM(eq),     int_equal,       MRB_ARGS_REQ(1)); /* 15.2.8.3.7 */
  mrb_define_method_id(mrb, integer, MRB_OPSYM(neg),    int_rev,         MRB_ARGS_NONE()); /* 15.2.8.3.8 */
  mrb_define_method_id(mrb, integer, MRB_OPSYM(and),    int_and,         MRB_ARGS_REQ(1)); /* 15.2.8.3.9 */
  mrb_define_method_id(mrb, integer, MRB_OPSYM(or),     int_or,          MRB_ARGS_REQ(1)); /* 15.2.8.3.10 */
  mrb_define_method_id(mrb, integer, MRB_OPSYM(xor),    int_xor,         MRB_ARGS_REQ(1)); /* 15.2.8.3.11 */
  mrb_define_method_id(mrb, integer, MRB_OPSYM(lshift), int_lshift,      MRB_ARGS_REQ(1)); /* 15.2.8.3.12 */
  mrb_define_method_id(mrb, integer, MRB_OPSYM(rshift), int_rshift,      MRB_ARGS_REQ(1)); /* 15.2.8.3.13 */
  mrb_define_method_id(mrb, integer, MRB_SYM(ceil),     int_ceil,        MRB_ARGS_OPT(1)); /* 15.2.8.3.14 */
  mrb_define_method_id(mrb, integer, MRB_SYM(floor),    int_floor,       MRB_ARGS_OPT(1)); /* 15.2.8.3.17 */
  mrb_define_method_id(mrb, integer, MRB_SYM(round),    int_round,       MRB_ARGS_OPT(1)); /* 15.2.8.3.20 */
  mrb_define_method_id(mrb, integer, MRB_SYM(truncate), int_truncate,    MRB_ARGS_OPT(1)); /* 15.2.8.3.26 */
  mrb_define_method_id(mrb, integer, MRB_SYM(hash),     int_hash,        MRB_ARGS_NONE()); /* 15.2.8.3.18 */
#ifndef MRB_NO_FLOAT
  mrb_define_method_id(mrb, integer, MRB_SYM(to_f),     int_to_f,        MRB_ARGS_NONE()); /* 15.2.8.3.23 */
#endif
  mrb_define_method_id(mrb, integer, MRB_SYM(to_s),     int_to_s,        MRB_ARGS_OPT(1)); /* 15.2.8.3.25 */
  mrb_define_method_id(mrb, integer, MRB_SYM(inspect),  int_to_s,        MRB_ARGS_OPT(1));
  mrb_define_method_id(mrb, integer, MRB_SYM(divmod),   int_divmod,      MRB_ARGS_REQ(1)); /* 15.2.8.3.30(x) */
  mrb_define_method_id(mrb, integer, MRB_SYM(__coerce_step_counter), coerce_step_counter, MRB_ARGS_REQ(2));

  /* Fixnum Class for compatibility */
  mrb_define_const_id(mrb, mrb->object_class, MRB_SYM(Fixnum), mrb_obj_value(integer));

#ifndef MRB_NO_FLOAT
  /* Float Class */
  mrb->float_class = fl = mrb_define_class_id(mrb, MRB_SYM(Float), numeric);               /* 15.2.9 */
  MRB_SET_INSTANCE_TT(fl, MRB_TT_FLOAT);
  MRB_UNDEF_ALLOCATOR(fl);
  mrb_undef_class_method(mrb,  fl, "new");
  mrb_define_method_id(mrb, fl,      MRB_OPSYM(pow),     flo_pow,        MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, fl,      MRB_OPSYM(div),     flo_div,        MRB_ARGS_REQ(1)); /* 15.2.9.3.6 */
  mrb_define_method_id(mrb, fl,      MRB_SYM(quo),       flo_div,        MRB_ARGS_REQ(1)); /* 15.2.7.4.5(x) */
  mrb_define_method_id(mrb, fl,      MRB_SYM(div),       flo_idiv,       MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, fl,      MRB_OPSYM(add),     flo_add,        MRB_ARGS_REQ(1)); /* 15.2.9.3.3 */
  mrb_define_method_id(mrb, fl,      MRB_OPSYM(sub),     flo_sub,        MRB_ARGS_REQ(1)); /* 15.2.9.3.4 */
  mrb_define_method_id(mrb, fl,      MRB_OPSYM(mul),     flo_mul,        MRB_ARGS_REQ(1)); /* 15.2.9.3.5 */
  mrb_define_method_id(mrb, fl,      MRB_OPSYM(mod),     flo_mod,        MRB_ARGS_REQ(1)); /* 15.2.9.3.7 */
  mrb_define_method_id(mrb, fl,      MRB_OPSYM(cmp),     num_cmp,        MRB_ARGS_REQ(1)); /* 15.2.9.3.1 */
  mrb_define_method_id(mrb, fl,      MRB_OPSYM(lt),      num_lt,         MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, fl,      MRB_OPSYM(le),      num_le,         MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, fl,      MRB_OPSYM(gt),      num_gt,         MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, fl,      MRB_OPSYM(ge),      num_ge,         MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, fl,      MRB_OPSYM(eq),      flo_eq,         MRB_ARGS_REQ(1)); /* 15.2.9.3.2 */
  mrb_define_method_id(mrb, fl,      MRB_OPSYM(neg),     flo_rev,        MRB_ARGS_NONE());
  mrb_define_method_id(mrb, fl,      MRB_OPSYM(and),     flo_and,        MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, fl,      MRB_OPSYM(or),      flo_or,         MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, fl,      MRB_OPSYM(xor),     flo_xor,        MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, fl,      MRB_OPSYM(rshift),  flo_rshift,     MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, fl,      MRB_OPSYM(lshift),  flo_lshift,     MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, fl,      MRB_SYM(ceil),      flo_ceil,       MRB_ARGS_OPT(1)); /* 15.2.9.3.8  */
  mrb_define_method_id(mrb, fl,      MRB_SYM_Q(finite),  flo_finite_p,   MRB_ARGS_NONE()); /* 15.2.9.3.9  */
  mrb_define_method_id(mrb, fl,      MRB_SYM(floor),     flo_floor,      MRB_ARGS_OPT(1)); /* 15.2.9.3.10 */
  mrb_define_method_id(mrb, fl,      MRB_SYM_Q(infinite),flo_infinite_p, MRB_ARGS_NONE()); /* 15.2.9.3.11 */
  mrb_define_method_id(mrb, fl,      MRB_SYM(round),     flo_round,      MRB_ARGS_OPT(1)); /* 15.2.9.3.12 */
  mrb_define_method_id(mrb, fl,      MRB_SYM(to_f),      flo_to_f,       MRB_ARGS_NONE()); /* 15.2.9.3.13 */
  mrb_define_method_id(mrb, fl,      MRB_SYM(to_i),      flo_to_i,       MRB_ARGS_NONE()); /* 15.2.9.3.14 */
  mrb_define_method_id(mrb, fl,      MRB_SYM(truncate),  flo_truncate,   MRB_ARGS_OPT(1)); /* 15.2.9.3.15 */
  mrb_define_method_id(mrb, fl,      MRB_SYM(divmod),    flo_divmod,     MRB_ARGS_REQ(1));

  mrb_define_method_id(mrb, fl,      MRB_SYM(to_s),      flo_to_s,       MRB_ARGS_NONE()); /* 15.2.9.3.16(x) */
  mrb_define_method_id(mrb, fl,      MRB_SYM(inspect),   flo_to_s,       MRB_ARGS_NONE());
  mrb_define_method_id(mrb, fl,      MRB_SYM_Q(nan),     flo_nan_p,      MRB_ARGS_NONE());
  mrb_define_method_id(mrb, fl,      MRB_SYM(abs),       flo_abs,        MRB_ARGS_NONE()); /* 15.2.7.4.3 */
  mrb_define_method_id(mrb, fl,      MRB_SYM(hash),      flo_hash,       MRB_ARGS_NONE());

#ifdef INFINITY
  mrb_define_const_id(mrb, fl, MRB_SYM(INFINITY), mrb_float_value(mrb, INFINITY));
#endif
#ifdef NAN
  mrb_define_const_id(mrb, fl, MRB_SYM(NAN), mrb_float_value(mrb, NAN));
#endif
#endif
}

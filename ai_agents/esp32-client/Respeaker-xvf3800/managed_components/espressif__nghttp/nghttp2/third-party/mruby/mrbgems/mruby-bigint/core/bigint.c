/**
** @file mruby/bigint.c - Multi-precision Integer
**
** See Copyright Notice in mruby.h
*/

#include <mruby.h>
#include <mruby/object.h>
#include <mruby/numeric.h>
#include <mruby/array.h>
#include <mruby/string.h>
#include <mruby/internal.h>
#include <string.h>
#include "bigint.h"

#define DIG_SIZE (MPZ_DIG_SIZE)
#define DIG_BASE (1ULL << DIG_SIZE)
#define DIG_MASK (DIG_BASE - 1)
#define HIGH(x) ((x) >> DIG_SIZE)
#define LOW(x)  ((x) & DIG_MASK)

#define iabs(x) (((x)>0)?(x):(-x))
#define imax(x,y) (((x)>(y))?(x):(y))
#define imin(x,y) (((x)<(y))?(x):(y))
#define dg(x,i) (((size_t)i < (x)->sz)?(x)->p[i]:0)

static void
mpz_init(mrb_state *mrb, mpz_t *s)
{
  s->p = NULL;
  s->sn=0;
  s->sz=0;
}

static void
mpz_realloc(mrb_state *mrb, mpz_t *x, size_t size)
{
  if (x->sz < size) {
    x->p=(mp_limb*)mrb_realloc(mrb, x->p, size*sizeof(mp_limb));
    for (size_t i=x->sz; i<size; i++)
      x->p[i] = 0;
    x->sz = size;
  }
}

static void
mpz_set(mrb_state *mrb, mpz_t *y, mpz_t *x)
{
  size_t i, k = x->sz;

  mpz_realloc(mrb, y, k);
  for (i=0;i < k; i++)
    y->p[i] = x->p[i];

  for (;i<y->sz;i++)
    y->p[i] = 0;

  y->sn = x->sn;
}

static void
mpz_init_set(mrb_state *mrb, mpz_t *s, mpz_t *t)
{
  mpz_init(mrb, s);
  mpz_set(mrb, s, t);
}

static void
mpz_set_int(mrb_state *mrb, mpz_t *y, mrb_int v)
{
  mrb_uint u;

  if (v == 0) {
    y->sn=0;
    u = 0;
  }
  else if (v > 0) {
    y->sn = 1;
    u = v;
  }
  else /* if (v < 0) */ {
    y->sn = -1;
    if (v == MRB_INT_MIN) u = v;
    else u = -v;
  }
#if MRB_INT_BIT > DIG_SIZE
  if ((u & ~DIG_MASK) != 0) {
    mpz_realloc(mrb, y, 2);
    y->p[1] = (mp_limb)HIGH(u);
    y->p[0] = (mp_limb)LOW(u);
  }
  else
#endif
  {
    mpz_realloc(mrb, y, 1);
    y->p[0] = (mp_limb)u;
  }
}

static void
mpz_set_uint64(mrb_state *mrb, mpz_t *y, uint64_t u)
{
  const size_t len = sizeof(uint64_t) / sizeof(mp_limb);

  y->sn = (u != 0);
  mpz_realloc(mrb, y, len);
  for (size_t i=0; i<len; i++) {
    y->p[i++] = (mp_limb)LOW(u);
    u >>= DIG_SIZE;
  }
}

#ifdef MRB_INT32
static void
mpz_set_int64(mrb_state *mrb, mpz_t *y, int64_t v)
{
  uint64_t u;

  if (v < 0) {
    if (v == INT64_MIN) u = v;
    else u = -v;
  }
  else {
    u = v;
  }
  mpz_set_uint64(mrb, y, u);
  if (v < 0) {
    y->sn = -1;
  }
}
#endif

static void
mpz_init_set_int(mrb_state *mrb, mpz_t *y, mrb_int v)
{
  mpz_init(mrb, y);
  mpz_set_int(mrb, y, v);
}

static void
mpz_clear(mrb_state *mrb, mpz_t *s)
{
  if (s->p) mrb_free(mrb, s->p);
  s->p = NULL;
  s->sn = 0;
  s->sz = 0;
}

static void
mpz_move(mrb_state *mrb, mpz_t *y, mpz_t *x)
{
  mpz_clear(mrb, y);
  y->sn = x->sn;
  y->sz = x->sz;
  y->p = x->p;
  x->p = NULL;
  x->sn = 0;
  x->sz = 0;
}

static size_t
digits(mpz_t *x)
{
  size_t i;

  if (x->sz == 0) return 0;
  for (i = x->sz - 1; x->p[i] == 0; i--)
    if (i == 0) break;
  return i+1;
}

static void
trim(mpz_t *x)
{
  while (x->sz && x->p[x->sz-1] == 0) {
    x->sz--;
  }
}

/* z = x + y, without regard for sign */
static void
uadd(mrb_state *mrb, mpz_t *z, mpz_t *x, mpz_t *y)
{
  if (y->sz < x->sz) {
    mpz_t *t;                   /* swap x,y */
    t=x; x=y; y=t;
  }

  /* now y->sz >= x->sz */
  mpz_realloc(mrb, z, y->sz+1);

  mp_dbl_limb c = 0;
  size_t i;
  for (i=0; i<x->sz; i++) {
    c += (mp_dbl_limb)y->p[i] + (mp_dbl_limb)x->p[i];
    z->p[i] = LOW(c);
    c >>= DIG_SIZE;
  }
  for (;i<y->sz; i++) {
    c += y->p[i];
    z->p[i] = LOW(c);
    c >>= DIG_SIZE;
  }
  z->p[y->sz] = (mp_limb)c;
  trim(z);
}

/* z = y - x, ignoring sign */
/* precondition: abs(y) >= abs(x) */
static void
usub(mrb_state *mrb, mpz_t *z, mpz_t *y, mpz_t *x)
{
  mpz_realloc(mrb, z, (size_t)(y->sz));
  mp_dbl_limb_signed b = 0;
  size_t i;
  for (i=0;i<x->sz;i++) {
    b += (mp_dbl_limb_signed)y->p[i];
    b -= (mp_dbl_limb_signed)x->p[i];
    z->p[i] = LOW(b);
    b = HIGH(b);
  }
  for (;i<y->sz; i++) {
    b += y->p[i];
    z->p[i] = LOW(b);
    b = HIGH(b);
  }
  z->sz = digits(z);
}

/* compare abs(x) and abs(y) */
static int
ucmp(mpz_t *y, mpz_t *x)
{
  if (y->sz < x->sz) return -1;
  if (y->sz > x->sz) return 1;
  if (x->sz == 0) return 0;
  for (size_t i=x->sz-1;; i--) {
    mp_limb a = y->p[i];
    mp_limb b = x->p[i];
    if (a > b) return 1;
    if (a < b) return -1;
    if (i == 0) break;
  }
  return 0;
}

static int
uzero(mpz_t *x)
{
  for (size_t i=0; i < x->sz; i++)
    if (x->p[i] != 0)
      return 0;
  return 1;
}

static void
zero(mpz_t *x)
{
  x->sn=0;
  if (x->p) {
    x->sz=1;
    x->p[0]=0;
  }
  else {
    x->sz=0;
  }
}

/* z = x + y */
static void
mpz_add(mrb_state *mrb, mpz_t *zz, mpz_t *x, mpz_t *y)
{
  int mg;
  mpz_t z;

  if (x->sn == 0) {
    mpz_set(mrb, zz, y);
    return;
  }
  if (y->sn == 0) {
    mpz_set(mrb, zz, x);
    return;
  }
  mpz_init(mrb, &z);

  if (x->sn > 0 && y->sn > 0) {
    uadd(mrb, &z, x, y);
    z.sn = 1;
  }
  else if (x->sn < 0 && y->sn < 0) {
    uadd(mrb, &z, x, y);
    z.sn = -1;
  }
  else {
    /* signs differ */
    if ((mg = ucmp(x,y)) == 0) {
      zero(&z);
    }
    else if (mg > 0) {  /* abs(y) < abs(x) */
      usub(mrb, &z, x, y);
      z.sn = (x->sn > 0 && y->sn < 0) ? 1 : (-1);
    }
    else { /* abs(y) > abs(x) */
      usub(mrb, &z, y, x);
      z.sn = (x->sn < 0 && y->sn > 0) ? 1 : (-1);
    }
  }
  trim(&z);
  mpz_move(mrb, zz, &z);
}

/* z = x - y  -- just use mpz_add - I'm lazy */
static void
mpz_sub(mrb_state *mrb, mpz_t *z, mpz_t *x, mpz_t *y)
{
  mpz_t u;

  mpz_init(mrb, &u);
  mpz_set(mrb, &u, y);
  u.sn = -(u.sn);
  mpz_add(mrb, z, x, &u);
  mpz_clear(mrb, &u);
}

/* x = y - n */
static void
mpz_sub_int(mrb_state *mrb, mpz_t *x, mpz_t *y, mrb_int n)
{
  mpz_t z;

  mpz_init_set_int(mrb, &z, n);
  mpz_sub(mrb, x, y, &z);
  mpz_clear(mrb, &z);
}

/* w = u * v */
static void
mpz_mul(mrb_state *mrb, mpz_t *ww, mpz_t *u, mpz_t *v)
{
  size_t i, j;
  mpz_t w;

  if (uzero(u) || uzero(v)) {
    mpz_set_int(mrb, ww, 0);
    return;
  }
  mpz_init(mrb, &w);
  mpz_realloc(mrb, &w, u->sz + v->sz);
  for (j=0; j < u->sz; j++) {
    mp_dbl_limb cc = (mp_limb)0;
    mp_limb u0 = u->p[j];
    if (u0 == 0) continue;
    for (i=0; i < v->sz; i++) {
      mp_limb v0 = v->p[i];
      if (v0 == 0) continue;
      cc += (mp_dbl_limb)w.p[i+j] + (mp_dbl_limb)u0 * (mp_dbl_limb)v0;
      w.p[i+j] = LOW(cc);
      cc = HIGH(cc);
    }
    if (cc) {
      w.p[i+j] = (mp_limb)cc;
    }
  }
  w.sn = u->sn * v->sn;
  trim(&w);
  mpz_move(mrb, ww, &w);
}

static void
mpz_mul_int(mrb_state *mrb, mpz_t *x, mpz_t *y, mrb_int n)
{
  if (n == 0) {
    zero(x);
    return;
  }

  mpz_t z;

  mpz_init_set_int(mrb, &z, n);
  mpz_mul(mrb, x, y, &z);
  mpz_clear(mrb, &z);
}

/* number of leading zero bits in digit */
static int
lzb(mp_limb x)
{
  if (x == 0) return 0;
#if (defined(__GNUC__) || __has_builtin(__builtin_clz))
  if (sizeof(mp_limb) == sizeof(int64_t))
    return __builtin_clzll(x);
  else if (sizeof(mp_limb) == sizeof(int32_t))
    return __builtin_clz(x);
#endif

  int j=0;

  for (mp_limb i = ((mp_limb)1 << (DIG_SIZE-1)); i && !(x&i); j++,i>>=1)
    ;
  return j;
}

/* c1 = a>>n */
/* n must be < DIG_SIZE */
static void
urshift(mrb_state *mrb, mpz_t *c1, mpz_t *a, size_t n)
{
  mrb_assert(n < DIG_SIZE);

  if (n == 0)
    mpz_set(mrb, c1, a);
  else if (uzero(a)) {
    mpz_set_int(mrb, c1, 0);
  }
  else {
    mpz_t c;
    mp_limb cc = 0;
    mp_dbl_limb rm = (((mp_dbl_limb)1<<n) - 1);

    mpz_init(mrb, &c);
    mpz_realloc(mrb, &c, a->sz);
    for (size_t i=a->sz-1;; i--) {
      c.p[i] = ((a->p[i] >> n) | cc) & DIG_MASK;
      cc = (a->p[i] & rm) << (DIG_SIZE - n);
      if (i == 0) break;
    }
    trim(&c);
    mpz_move(mrb, c1, &c);
  }
}

/* c1 = a<<n */
/* n must be < DIG_SIZE */
static void
ulshift(mrb_state *mrb, mpz_t *c1, mpz_t *a, size_t n)
{
  mrb_assert(n < DIG_SIZE);
  if (n == 0)
    mpz_set(mrb, c1, a);
  else if (uzero(a)) {
    mpz_set_int(mrb, c1, 0);
  }
  else {
    mp_limb cc = 0;
    mpz_t c;
    mp_limb rm = (((mp_dbl_limb)1<<n) - 1) << (DIG_SIZE-n);

    mpz_init(mrb, &c);
    mpz_realloc(mrb, &c, a->sz+1);

    size_t i;
    for (i=0; i<a->sz; i++) {
      c.p[i] = ((a->p[i] << n) | cc) & DIG_MASK;
      cc = (a->p[i] & rm) >> (DIG_SIZE-n);
    }
    c.p[i] = cc;
    trim(&c);
    mpz_move(mrb, c1, &c);
  }
}

/* internal routine to compute x/y and x%y ignoring signs */
/* qq = xx/yy; rr = xx%yy */
static void
udiv(mrb_state *mrb, mpz_t *qq, mpz_t *rr, mpz_t *xx, mpz_t *yy)
{
  /* simple cases */
  int cmp = ucmp(xx, yy);
  if (cmp == 0) {
    mpz_set_int(mrb, qq, 1);
    zero(rr);
    return;
  }
  else if (cmp < 0) {
    zero(qq);
    mpz_set(mrb, rr, xx);
    return;
  }

  mpz_t q, x, y;
  size_t i;

  mrb_assert(!uzero(yy));       /* divided by zero */
  mpz_init(mrb, &q);
  mpz_init(mrb, &x);
  mpz_init(mrb, &y);
  mpz_realloc(mrb, &x, xx->sz+1);
  size_t yd = digits(yy);
  size_t ns = lzb(yy->p[yd-1]);
  ulshift(mrb, &x, xx, ns);
  ulshift(mrb, &y, yy, ns);
  size_t xd = digits(&x);
  mpz_realloc(mrb, &q, xd);
  mp_dbl_limb z = y.p[yd-1];
  if (xd>=yd) {
    for (size_t j=xd-yd;; j--) {
      mp_dbl_limb_signed b=0;
      mp_dbl_limb qhat;

      if (j+yd == xd)
        qhat = x.p[j+yd-1] / z;
      else
        qhat = (((mp_dbl_limb)x.p[j+yd] << DIG_SIZE) + x.p[j+yd-1]) / z;
      if (qhat) {
        for (i=0; i<yd; i++) {
          mp_dbl_limb zz = qhat * y.p[i];
          mp_dbl_limb_signed u = LOW(b)+x.p[i+j]-LOW(zz);
          x.p[i+j] = LOW(u);
          b = HIGH(b) - HIGH(zz) + HIGH(u);
        }
        b += x.p[i+j];
      }
      for (; b!=0; qhat--) {
        mp_dbl_limb c = 0;
        for (i=0; i<yd; i++) {
          c += (mp_dbl_limb)x.p[i+j] + (mp_dbl_limb)y.p[i];
          x.p[i+j] = LOW(c);
          c = HIGH(c);
        }
        b += c;
      }
      q.p[j] = (mp_limb)qhat;
      if (j == 0) break;
    }
  }
  x.sz = yy->sz;
  urshift(mrb, rr, &x, ns);
  trim(&q);
  mpz_move(mrb, qq, &q);
  mpz_clear(mrb, &x);
  mpz_clear(mrb, &y);
}

static void
mpz_mdiv(mrb_state *mrb, mpz_t *q, mpz_t *x, mpz_t *y)
{
  mpz_t r;
  short sn1 = x->sn, sn2 = y->sn, qsign;

  if (uzero(x)) {
    mpz_init_set_int(mrb, q, 0);
    return;
  }
  mpz_init(mrb, &r);
  udiv(mrb, q, &r, x, y);
  qsign = q->sn = sn1*sn2;
  if (uzero(q))
    q->sn = 0;
  /* now if r != 0 and q < 0 we need to round q towards -inf */
  if (!uzero(&r) && qsign < 0)
    mpz_sub_int(mrb, q, q, 1);
  mpz_clear(mrb, &r);
}

static void
mpz_mmod(mrb_state *mrb, mpz_t *r, mpz_t *x, mpz_t *y)
{
  mpz_t q;
  short sn1 = x->sn, sn2 = y->sn, sn3;

  mpz_init(mrb, &q);
  if (sn1 == 0) {
    zero(r);
    return;
  }
  udiv(mrb, &q, r, x, y);
  mpz_clear(mrb, &q);
  if (uzero(r)) {
    r->sn = 0;
    return;
  }
  sn3 = sn1*sn2;
  if (sn3 > 0)
    r->sn = sn1;
  else if (sn1 < 0 && sn2 > 0) {
    r->sn = 1;
    mpz_sub(mrb, r, y, r);
  }
  else {
    r->sn = 1;
    mpz_add(mrb, r, y, r);
  }
}

static void
mpz_mdivmod(mrb_state *mrb, mpz_t *q, mpz_t *r, mpz_t *x, mpz_t *y)
{
  short sn1 = x->sn, sn2 = y->sn, qsign;

  if (sn1 == 0) {
    zero(q);
    zero(r);
    return;
  }
  udiv(mrb, q, r, x, y);
  qsign = q->sn = sn1*sn2;
  if (uzero(r)) {
    /* q != 0, since q=r=0 would mean x=0, which was tested above */
    r->sn = 0;
    return;
  }
  if (q->sn > 0)
    r->sn = sn1;
  else if (sn1 < 0 && sn2 > 0) {
    r->sn = 1;
    mpz_sub(mrb, r, y, r);
  }
  else {
    r->sn = 1;
    mpz_add(mrb, r, y, r);
  }
  if (uzero(q))
    q->sn = 0;
  /* now if r != 0 and q < 0 we need to round q towards -inf */
  if (!uzero(r) && qsign < 0)
    mpz_sub_int(mrb, q, q, 1);
}

static void
mpz_mod(mrb_state *mrb, mpz_t *r, mpz_t *x, mpz_t *y)
{
  mpz_t q;
  short sn = x->sn;
  mpz_init(mrb, &q);
  if (x->sn == 0) {
    zero(r);
    return;
  }
  udiv(mrb, &q, r, x, y);
  r->sn = sn;
  if (uzero(r))
    r->sn = 0;
  mpz_clear(mrb, &q);
}

static mrb_int
mpz_cmp(mrb_state *mrb, mpz_t *x, mpz_t *y)
{
  int abscmp;
  if (x->sn < 0 && y->sn > 0)
    return (-1);
  if (x->sn > 0 && y->sn < 0)
    return 1;
  abscmp=ucmp(x, y);
  if (x->sn >=0 && y->sn >=0)
    return abscmp;
  return (-abscmp);          // if (x->sn <=0 && y->sn <=0)
}

/* 2<=base<=36 - this overestimates the optimal value, which is OK */
static size_t
mpz_sizeinbase(mpz_t *x, mrb_int base)
{
  size_t i, j;

  size_t bits = digits(x) * DIG_SIZE;
  mrb_assert(2 <= base && base <= 36);

  if (x->sz == 0) return 0;
  for (j=0,i=1; i<=(size_t)base; i*=2,j++)
    ;
  return bits/(j-1)+1;
}

static int
mpz_init_set_str(mrb_state *mrb, mpz_t *x, const char *s, mrb_int len, mrb_int base)
{
  int retval = 0;
  mpz_t t, m, bb;
  short sn;
  uint8_t k;
  mpz_init(mrb, x);
  mpz_init_set_int(mrb, &m, 1);
  mpz_init(mrb, &t);
  zero(x);
  if (*s == '-') {
    sn = -1; s++;
  }
  else
    sn = 1;
  mpz_init_set_int(mrb, &bb, base);
  for (mrb_int i = len-1; i>=0; i--) {
    if (s[i]=='_') continue;
    if (s[i] >= '0' && s[i] <= '9')
      k = (uint8_t)s[i] - (uint8_t)'0';
    else if (s[i] >= 'A' && s[i] <= 'Z')
      k = (uint8_t)s[i] - (uint8_t)'A'+10;
    else if (s[i] >= 'a' && s[i] <= 'z')
      k = (uint8_t)s[i] - (uint8_t)'a'+10;
    else {
      retval = (-1);
      break;
    }
    if (k >= base) {
      retval = (-1);
      break;
    }
    mpz_mul_int(mrb, &t, &m, (mrb_int)k);
    mpz_add(mrb, x, x, &t);
    mpz_mul(mrb, &m, &m, &bb);
  }
  x->sn = sn;
  mpz_clear(mrb, &m);
  mpz_clear(mrb, &bb);
  mpz_clear(mrb, &t);
  return retval;
}

static char*
mpz_get_str(mrb_state *mrb, char *s, mrb_int sz, mrb_int base, mpz_t *x)
{
  mrb_assert(2 <= base && base <= 36);
  if (uzero(x)) {
    *s='0';
    *(s+1)='\0';
    return s;
  }

  char *ps = s;
  char *se = s+sz;
  int xlen = (int)digits(x);
  mp_limb *t = (mp_limb*)mrb_malloc(mrb, xlen*sizeof(mp_limb));
  mp_limb *tend = t + xlen;
  memcpy(t, x->p, xlen*sizeof(mp_limb));
  mp_limb b2 = (mp_limb)base;
  const int blim = (sizeof(mp_limb)<4)?(base<=10?4:3):(base<=10?9:5);
  for (int i=1; i<blim; i++) {
    b2 *= (mp_limb)base;
  }

  for (;;) {
    mp_limb *d = tend;
    mp_dbl_limb a = 0;
    while (--d >= t) {
      mp_limb d0 = *d;
      a = (a<<DIG_SIZE) | d0;
      *d = (mp_limb)(a / b2);
      a %= b2;
    }

    // convert to character
    for (int i=0; i<blim; i++) {
      mp_limb a0 = (mp_limb)(a % base);
      if (a0 < 10) a0 += '0';
      else a0 += 'a' - 10;
      if (s == se) break;
      *s++ = (char)a0;
      a /= base;
    }

    // check if number is zero
    for (d = t; d < tend; d++) {
      if (*d != 0) break;
    }
    if (d == tend) goto done;
  }

 done:
  while (ps<s && s[-1]=='0') s--;
  mrb_free(mrb, t);
  if (x->sn < 0) {
    *s++ = '-';
  }

  /* reverse string */
  for (char *u = ps,*v=s-1; u < v; u++,v--) {
    char temp = *u;
    *u = *v;
    *v = temp;
  }
  *s = '\0'; /* null termination */
  return ps;
}

static int
mpz_get_int(mpz_t *y, mrb_int *v)
{
  if (uzero(y)) {
    *v = 0;
    return TRUE;
  }

  mp_dbl_limb i = 0;
  mp_limb *d = y->p + y->sz;

  while (d-- > y->p) {
    if (HIGH(i) != 0) {
      /* will overflow */
      return FALSE;
    }
    i = (i << DIG_SIZE) | *d;
  }
  if (i > MRB_INT_MAX) {
    /* overflow */
    return FALSE;
  }
  if (y->sn < 0) {
    *v = -(mrb_int)i;
  }
  else {
    *v = (mrb_int)i;
  }
  return TRUE;
}

static void
mpz_mul_2exp(mrb_state *mrb, mpz_t *z, mpz_t *x, mrb_int e)
{
  if (e==0)
    mpz_set(mrb, z, x);
  else {
    short sn = x->sn;
    size_t digs = e / DIG_SIZE;
    size_t bs = e % DIG_SIZE;
    mpz_t y;

    mpz_init(mrb, &y);
    mpz_realloc(mrb, &y, x->sz+digs);
    for (size_t i=0;i<x->sz;i++)
      y.p[i+digs] = x->p[i];
    if (bs) {
      ulshift(mrb, z, &y, bs);
      mpz_clear(mrb, &y);
    }
    else {
      mpz_move(mrb, z, &y);
    }
    z->sn = sn;
  }
}

static void
mpz_div_2exp(mrb_state *mrb, mpz_t *z, mpz_t *x, mrb_int e)
{
  short sn = x->sn;
  if (e==0)
    mpz_set(mrb, z, x);
  else {
    size_t digs = e / DIG_SIZE;
    size_t bs = e % DIG_SIZE;
    mpz_t y;

    mpz_init(mrb, &y);
    mpz_realloc(mrb, &y, x->sz-digs);
    for (size_t i=0; i < x->sz-digs; i++)
      y.p[i] = x->p[i+digs];
    if (bs) {
      urshift(mrb, z, &y, bs);
      mpz_clear(mrb, &y);
    }
    else {
      mpz_move(mrb, z, &y);
    }
    if (uzero(z))
      z->sn = 0;
    else {
      z->sn = sn;
    }
  }
}

static void
mpz_neg(mrb_state *mrb, mpz_t *x, mpz_t *y)
{
  if (x!=y)
    mpz_set(mrb, x, y);
  x->sn = -(y->sn);
}

static void
mpz_and(mrb_state *mrb, mpz_t *z, mpz_t *x, mpz_t *y) /* not the most efficient way to do this */
{
  size_t sz = imin(x->sz, y->sz);

  mpz_realloc(mrb, z, sz);
  for (size_t i=0; i < sz; i++)
    z->p[i] = x->p[i] & y->p[i];
  if (x->sn < 0 && y->sn < 0)
    z->sn = (-1);
  else
    z->sn = 1;
  if (uzero(z))
    z->sn = 0;
}

static void
mpz_or(mrb_state *mrb, mpz_t *z, mpz_t *x, mpz_t *y)  /* not the most efficient way to do this */
{
  size_t i;
  size_t sz = imax(x->sz, y->sz);

  mpz_realloc(mrb, z, sz);
  for (i=0; i < sz; i++)
    z->p[i] = dg(x,i) | dg(y,i);
  if (x->sn < 0 || y->sn < 0)
    z->sn = (-1);
  else
    z->sn = 1;
  if (uzero(z))
    z->sn = 0;
}

static void
mpz_xor(mrb_state *mrb, mpz_t *z, mpz_t *x, mpz_t *y)  /* not the most efficient way to do this */
{
  size_t i;

  size_t sz = imax(x->sz, y->sz);
  mpz_realloc(mrb, z, sz);
  for (i=0; i < sz; i++)
    z->p[i] = dg(x,i) ^ dg(y,i);
  if ((x->sn <= 0 && y->sn > 0) || (x->sn > 0 && y->sn <=0))
    z->sn = (-1);
  else
    z->sn = 1;
  if (uzero(z))
    z->sn = 0;
}

static void
mpz_pow(mrb_state *mrb, mpz_t *zz, mpz_t *x, mrb_int e)
{
  mpz_t t;
  mrb_uint mask = 1ULL<<(sizeof(mrb_int)*8-1);

  if (e==0) {
    mpz_set_int(mrb, zz, 1L);
    return;
  }

  mpz_init(mrb, &t);
  mpz_set(mrb, &t, x);
  for (;!(mask &e); mask>>=1)
    ;
  mask>>=1;
  for (;mask!=0; mask>>=1) {
    mpz_mul(mrb, &t, &t, &t);
    if (e & mask)
      mpz_mul(mrb, &t, &t, x);
  }
  mpz_move(mrb, zz, &t);
}

static void
mpz_powm(mrb_state *mrb, mpz_t *zz, mpz_t *x, mpz_t *ex, mpz_t *n)
{
  mpz_t t, b;

  if (uzero(ex)) {
    mpz_set_int(mrb, zz, 1);
    return;
  }

  if (ex->sn < 0) {
    return;
  }

  mpz_init_set_int(mrb, &t, 1);
  mpz_init_set(mrb, &b, x);

  size_t len = digits(ex);
  for (size_t i=0; i<len; i++) {
    mp_limb e = ex->p[i];
    for (size_t j=0; j<sizeof(mp_limb)*8; j++) {
      if ((e & 1) == 1) {
        mpz_mul(mrb, &t, &t, &b);
        mpz_mod(mrb, &t, &t, n);
      }
      e >>= 1;
      mpz_mul(mrb, &b, &b, &b);
      mpz_mod(mrb, &b, &b, n);
    }
  }
  mpz_move(mrb, zz, &t);
  mpz_clear(mrb, &b);
}

static void
mpz_powm_i(mrb_state *mrb, mpz_t *zz, mpz_t *x, mrb_int ex, mpz_t *n)
{
  mpz_t t, b;

  if (ex == 0) {
    mpz_set_int(mrb, zz, 1);
    return;
  }

  if (ex < 0) {
    return;
  }

  mpz_init_set_int(mrb, &t, 1);
  mpz_init_set(mrb, &b, x);

  while (ex > 0) {
    if ((ex & 1) == 1) {
      mpz_mul(mrb, &t, &t, &b);
      mpz_mod(mrb, &t, &t, n);
    }
    ex >>= 1;
    mpz_mul(mrb, &b, &b, &b);
    mpz_mod(mrb, &b, &b, n);
  }
  mpz_move(mrb, zz, &t);
  mpz_clear(mrb, &b);
}

/* --- mruby functions --- */
static struct RBigint*
bint_new(mrb_state *mrb)
{
  struct RBigint *b = MRB_OBJ_ALLOC(mrb, MRB_TT_BIGINT, mrb->integer_class);
  mpz_init(mrb, &b->mp);
  return b;
}

static struct RBigint*
bint_new_int(mrb_state *mrb, mrb_int x)
{
  struct RBigint *b = MRB_OBJ_ALLOC(mrb, MRB_TT_BIGINT, mrb->integer_class);
  mpz_init_set_int(mrb, &b->mp, x);
  return b;
}

mrb_value
mrb_bint_new_int(mrb_state *mrb, mrb_int x)
{
  struct RBigint *b = bint_new_int(mrb, x);
  return mrb_obj_value(b);
}

#ifdef MRB_INT32
mrb_value
mrb_bint_new_int64(mrb_state *mrb, int64_t x)
{
  struct RBigint *b = bint_new(mrb);
  mpz_init(mrb, &b->mp);
  mpz_set_int64(mrb, &b->mp, x);
  return mrb_obj_value(b);
}
#endif

mrb_value
mrb_bint_new_uint64(mrb_state *mrb, uint64_t x)
{
  struct RBigint *b = bint_new(mrb);
  mpz_init(mrb, &b->mp);
  mpz_set_uint64(mrb, &b->mp, x);
  return mrb_obj_value(b);
}

mrb_value
mrb_bint_new_str(mrb_state *mrb, const char *x, mrb_int len, mrb_int base)
{
  struct RBigint *b = MRB_OBJ_ALLOC(mrb, MRB_TT_BIGINT, mrb->integer_class);
  int sn = 1;
  if (base < 0) {
    base = -base;
    sn = -1;
  }
  mrb_assert(2 <= base && base <= 36);
  mpz_init_set_str(mrb, &b->mp, x, len, base);
  if (sn < 0) {
    b->mp.sn = sn;
  }
  return mrb_obj_value(b);
}

static mrb_value
bint_norm(mrb_state *mrb, struct RBigint *b)
{
  mrb_int i;

  if (mpz_get_int(&b->mp, &i)) {
    return mrb_int_value(mrb, i);
  }
  return mrb_obj_value(b);
}

void
mrb_gc_free_bint(mrb_state *mrb, struct RBasic *x)
{
  struct RBigint *b = (struct RBigint*)x;
  mpz_clear(mrb, &b->mp);
}

#ifndef MRB_NO_FLOAT
mrb_value
mrb_bint_new_float(mrb_state *mrb, mrb_float x)
{
  /* x should not be NaN nor Infinity */
  mrb_assert(x == x && x != x * 0.5);

  int sn;
  if (x < 0.0) {
    x = -x;
    sn = -1;
  }
  else {
    sn = 1;
  }
  if (x < 1.0) {
    return mrb_fixnum_value(0);
  }

  struct RBigint *bint = bint_new(mrb);
  mpz_t *r = &bint->mp;
  r->sn = sn;

  mrb_float b = (double)DIG_BASE;
  mrb_float bi = 1.0 / b;
  size_t rn;
  mp_limb *rp;
  mp_limb f;

  for (rn = 1; x >= b; rn++)
    x *= bi;

  mpz_realloc(mrb, r, rn);
  rp = r->p;
  for (size_t i=rn-1;;i--) {
    f = LOW((mp_limb)x);
    x -= f;
    mrb_assert(x < 1.0);
    rp[i] = f;
    if (i == 0) break;
  }
  return bint_norm(mrb, bint);
}

mrb_float
mrb_bint_as_float(mrb_state *mrb, mrb_value self)
{
  struct RBigint *b = RBIGINT(self);
  mpz_t *i = &b->mp;
  mp_limb *d = i->p + i->sz;
  mrb_float val = 0;

  while (d-- > i->p) {
    val = val * DIG_BASE + *d;
  }

  if (i->sn < 0) {
    val = -val;
  }
  return val;
}
#endif

mrb_value
mrb_as_bint(mrb_state *mrb, mrb_value x)
{
  if (mrb_bigint_p(x)) return x;
  return mrb_bint_new_int(mrb, mrb_as_int(mrb, x));
}

mrb_int
mrb_bint_as_int(mrb_state *mrb, mrb_value x)
{
  struct RBigint *b = RBIGINT(x);
  mrb_int i;

  if (!mpz_get_int(&b->mp, &i)) {
    mrb_raise(mrb, E_RANGE_ERROR, "integer out of range");
  }
  return i;
}

#ifdef MRB_INT32
int64_t
mrb_bint_as_int64(mrb_state *mrb, mrb_value x)
{
  struct RBigint *b = RBIGINT(x);
  mpz_t *m = &b->mp;
  uint64_t u = 0;
  size_t len = digits(m);

  if (len*sizeof(mp_limb) > sizeof(uint64_t)) {
  out_of_range:
    mrb_raise(mrb, E_RANGE_ERROR, "integer out of range");
  }
  for (size_t i=len-1; ; i--) {
    u <<= DIG_SIZE;
    u |= m->p[i];
    if (i==0) break;
  }
  if (u > INT64_MAX) goto out_of_range;
  if (m->sn < 0) return -(int64_t)u;
  return (int64_t)u;
}
#endif

uint64_t
mrb_bint_as_uint64(mrb_state *mrb, mrb_value x)
{
  struct RBigint *b = RBIGINT(x);
  mpz_t *m = &b->mp;
  uint64_t u = 0;
  size_t len = digits(m);

  if (m->sn < 0 || len*sizeof(mp_limb) > sizeof(uint64_t)) {
    mrb_raise(mrb, E_RANGE_ERROR, "integer out of range");
  }
  for (size_t i=len-1; ; i--) {
    u <<= DIG_SIZE;
    u |= m->p[i];
    if (i==0) break;
  }
  return u;
}

/* unnormalize version of mrb_bint_add */
mrb_value
mrb_bint_add_d(mrb_state *mrb, mrb_value x, mrb_value y)
{
  y = mrb_as_bint(mrb, y);
  struct RBigint *b = RBIGINT(x);
  struct RBigint *b2 = RBIGINT(y);
  struct RBigint *b3 = bint_new(mrb);
  mpz_add(mrb, &b3->mp, &b->mp, &b2->mp);
  return mrb_obj_value(b3);
}

mrb_value
mrb_bint_add(mrb_state *mrb, mrb_value x, mrb_value y)
{
#ifndef MRB_NO_FLOAT
  if (mrb_float_p(y)) {
    mrb_float v1 = mrb_bint_as_float(mrb, x);
    mrb_float v2 = mrb_float(y);
    return mrb_float_value(mrb,v1+v2);
  }
#endif
  x = mrb_bint_add_d(mrb, x, y);
  return bint_norm(mrb, RBIGINT(x));
}

/* unnormalize version of mrb_bint_sub */
mrb_value
mrb_bint_sub_d(mrb_state *mrb, mrb_value x, mrb_value y)
{
  y = mrb_as_bint(mrb, y);
  struct RBigint *b = RBIGINT(x);
  struct RBigint *b2 = RBIGINT(y);
  struct RBigint *b3 = bint_new(mrb);
  mpz_sub(mrb, &b3->mp, &b->mp, &b2->mp);
  return mrb_obj_value(b3);
}

mrb_value
mrb_bint_sub(mrb_state *mrb, mrb_value x, mrb_value y)
{
#ifndef MRB_NO_FLOAT
  if (mrb_float_p(y)) {
    mrb_float v1 = mrb_bint_as_float(mrb, x);
    mrb_float v2 = mrb_float(y);
    return mrb_float_value(mrb,v1-v2);
  }
#endif
  x = mrb_bint_sub_d(mrb, x, y);
  return bint_norm(mrb, RBIGINT(x));
}

mrb_value
mrb_bint_mul(mrb_state *mrb, mrb_value x, mrb_value y)
{
#ifndef MRB_NO_FLOAT
  if (mrb_float_p(y)) {
    mrb_float v1 = mrb_bint_as_float(mrb, x);
    mrb_float v2 = mrb_float(y);
    return mrb_float_value(mrb,v1*v2);
  }
#endif
  y = mrb_as_bint(mrb, y);
  struct RBigint *b = RBIGINT(x);
  struct RBigint *b2 = RBIGINT(y);
  struct RBigint *b3 = bint_new(mrb);
  mpz_mul(mrb, &b3->mp, &b->mp, &b2->mp);
  return bint_norm(mrb, b3);
}

mrb_value
mrb_bint_div(mrb_state *mrb, mrb_value x, mrb_value y)
{
#ifndef MRB_NO_FLOAT
  if (mrb_float_p(y)) {
    mrb_float v1 = mrb_bint_as_float(mrb, x);
    mrb_float v2 = mrb_float(y);
    return mrb_float_value(mrb,v1*v2);
  }
#endif
  if (mrb_integer_p(y) && mrb_integer(y) == 0) {
    mrb_int_zerodiv(mrb);
  }
  y = mrb_as_bint(mrb, y);
  struct RBigint *b = RBIGINT(x);
  struct RBigint *b2 = RBIGINT(y);
  struct RBigint *b3 = bint_new(mrb);
  if (b2->mp.sn == 0 || uzero(&b2->mp)) {
    mrb_int_zerodiv(mrb);
  }
  mpz_mdiv(mrb, &b3->mp, &b->mp, &b2->mp);
  return bint_norm(mrb, b3);
}

mrb_value
mrb_bint_add_ii(mrb_state *mrb, mrb_int x, mrb_int y)
{
  struct RBigint *b = bint_new(mrb);
  mpz_t z1, z2;

  mpz_init_set_int(mrb, &z1, x);
  mpz_init_set_int(mrb, &z2, y);
  mpz_add(mrb, &b->mp, &z1, &z2);
  mpz_clear(mrb, &z1);
  mpz_clear(mrb, &z2);
  return bint_norm(mrb, b);
}

mrb_value
mrb_bint_sub_ii(mrb_state *mrb, mrb_int x, mrb_int y)
{
  struct RBigint *b = bint_new(mrb);
  mpz_t z1, z2;

  mpz_init_set_int(mrb, &z1, x);
  mpz_init_set_int(mrb, &z2, y);
  mpz_sub(mrb, &b->mp, &z1, &z2);
  mpz_clear(mrb, &z1);
  mpz_clear(mrb, &z2);
  return bint_norm(mrb, b);
}

mrb_value
mrb_bint_mul_ii(mrb_state *mrb, mrb_int x, mrb_int y)
{
  struct RBigint *b = bint_new(mrb);
  mpz_t z1, z2;

  mpz_init_set_int(mrb, &z1, x);
  mpz_init_set_int(mrb, &z2, y);
  mpz_mul(mrb, &b->mp, &z1, &z2);
  mpz_clear(mrb, &z1);
  mpz_clear(mrb, &z2);
  return bint_norm(mrb, b);
}

mrb_value
mrb_bint_mod(mrb_state *mrb, mrb_value x, mrb_value y)
{
#ifndef MRB_NO_FLOAT
  if (mrb_float_p(y)) {
    mrb_float v1 = mrb_bint_as_float(mrb, x);
    mrb_float v2 = mrb_float(y);
    return mrb_float_value(mrb, fmod(v1, v2));
  }
#endif
  if (mrb_integer_p(y) && mrb_integer(y) == 0) {
    mrb_int_zerodiv(mrb);
  }
  y = mrb_as_bint(mrb, y);
  struct RBigint *b = RBIGINT(x);
  struct RBigint *b2 = RBIGINT(y);
  struct RBigint *b3 = bint_new(mrb);
  if (b2->mp.sn == 0 || uzero(&b2->mp)) {
    mrb_int_zerodiv(mrb);
  }
  mpz_mmod(mrb, &b3->mp, &b->mp, &b2->mp);
  return bint_norm(mrb, b3);
}

mrb_value
mrb_bint_rem(mrb_state *mrb, mrb_value x, mrb_value y)
{
  /* called from mrbgems/mruby-numeric-ext/src/numeric_ext.c */
  /* y should not be float */
  if (mrb_integer_p(y) && mrb_integer(y) == 0) {
    mrb_int_zerodiv(mrb);
  }
  y = mrb_as_bint(mrb, y);
  struct RBigint *b = RBIGINT(x);
  struct RBigint *b2 = RBIGINT(y);
  struct RBigint *b3 = bint_new(mrb);
  if (b2->mp.sn == 0 || uzero(&b2->mp)) {
    mrb_int_zerodiv(mrb);
  }
  mpz_mod(mrb, &b3->mp, &b->mp, &b2->mp);
  return bint_norm(mrb, b3);
}

mrb_value
mrb_bint_divmod(mrb_state *mrb, mrb_value x, mrb_value y)
{
  /* called from src/numeric.c */
  /* y should not be float */
  if (mrb_integer_p(y) && mrb_integer(y) == 0) {
    mrb_int_zerodiv(mrb);
  }
  y = mrb_as_bint(mrb, y);
  struct RBigint *b = RBIGINT(x);
  struct RBigint *b2 = RBIGINT(y);
  struct RBigint *b3 = bint_new(mrb);
  struct RBigint *b4 = bint_new(mrb);
  if (b2->mp.sn == 0 || uzero(&b2->mp)) {
    mrb_int_zerodiv(mrb);
  }
  mpz_mdivmod(mrb, &b3->mp, &b4->mp, &b->mp, &b2->mp);
  x = bint_norm(mrb, b3);
  y = bint_norm(mrb, b4);
  return mrb_assoc_new(mrb, x, y);
}

mrb_int
mrb_bint_cmp(mrb_state *mrb, mrb_value x, mrb_value y)
{
#ifndef MRB_NO_FLOAT
  if (mrb_float_p(y)) {
    mrb_float v1 = mrb_bint_as_float(mrb, x);
    mrb_float v2 = mrb_float(y);
    if (v1 == v2) return 0;
    if (v1 > v2)  return 1;
    return -1;
  }
#endif
  struct RBigint *b = RBIGINT(x);
  if (!mrb_bigint_p(y)) {
    if (!mrb_integer_p(y)) return -2; /* type mismatch */

    mrb_int i1, i2 = mrb_integer(y);
    if (mpz_get_int(&b->mp, &i1)) {
      if (i1 == i2) return 0;
      if (i1 > i2) return 1;
      return -1;
    }
    if (b->mp.sn > 0) return 1;
    return -1;
  }
  struct RBigint *b2 = RBIGINT(y);
  return mpz_cmp(mrb, &b->mp, &b2->mp);
}

mrb_value
mrb_bint_pow(mrb_state *mrb, mrb_value x, mrb_value y)
{
  struct RBigint *b = RBIGINT(x);
  switch (mrb_type(y)) {
  case MRB_TT_INTEGER:
    {
      struct RBigint *b3 = bint_new(mrb);
      mpz_pow(mrb, &b3->mp, &b->mp, mrb_integer(y));
      return mrb_obj_value(b3);
    }
  case MRB_TT_BIGINT:
    mrb_raise(mrb, E_TYPE_ERROR, "too big power");
  default:
    mrb_raisef(mrb, E_TYPE_ERROR, "%v cannot be convert to integer", y);
  }
  return mrb_nil_value();
}

mrb_value
mrb_bint_powm(mrb_state *mrb, mrb_value x, mrb_value exp, mrb_value mod)
{
  struct RBigint *b = RBIGINT(x);
  struct RBigint *b2, *b3;

  if (mrb_bigint_p(mod)) {
    b2 = RBIGINT(mod);
    if (uzero(&b2->mp)) mrb_int_zerodiv(mrb);
  }
  else {
    mrb_int m = mrb_integer(mod);
    if (m == 0) mrb_int_zerodiv(mrb);
    b2 = bint_new_int(mrb, m);
  }
  b3 = bint_new(mrb);
  if (mrb_bigint_p(exp)) {
    struct RBigint *be = RBIGINT(exp);
    if (be->mp.sn < 0) {
      mrb_raise(mrb, E_ARGUMENT_ERROR, "int.pow(n,m): n must be positive");
    }
    mpz_powm(mrb, &b3->mp, &b->mp, &be->mp, &b2->mp);
  }
  else {
    mrb_int e = mrb_integer(exp);
    if (e < 0) {
      mrb_raise(mrb, E_ARGUMENT_ERROR, "int.pow(n,m): n must be positive");
    }
    mpz_powm_i(mrb, &b3->mp, &b->mp, e, &b2->mp);
  }
  return bint_norm(mrb, b3);
}

mrb_value
mrb_bint_to_s(mrb_state *mrb, mrb_value x, mrb_int base)
{
  struct RBigint *b = RBIGINT(x);

  if (b->mp.sz == 0) return mrb_str_new_lit(mrb, "0");

  size_t len = mpz_sizeinbase(&b->mp, (int)base);
  if (MRB_INT_MAX-2 < len) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "too long string from Integer");
  }
  mrb_value str = mrb_str_new(mrb, NULL, len+2);
  mpz_get_str(mrb, RSTRING_PTR(str), len, base, &b->mp);
  RSTR_SET_LEN(RSTRING(str), strlen(RSTRING_PTR(str)));
  return str;
}

mrb_value
mrb_bint_and(mrb_state *mrb, mrb_value x, mrb_value y)
{
  struct RBigint *b1 = RBIGINT(x);
  struct RBigint *b3 = bint_new(mrb);

#ifndef MRB_NO_FLOAT
  if (mrb_float_p(y)) {
    mpz_t z;
    mpz_init_set_int(mrb, &z, (mrb_int)mrb_float(y));
    mpz_and(mrb, &b3->mp, &b1->mp, &z);
    mpz_clear(mrb, &z);
    return bint_norm(mrb, b3);
  }
#endif
  y = mrb_as_bint(mrb, y);
  struct RBigint *b2 = RBIGINT(y);
  mpz_and(mrb, &b3->mp, &b1->mp, &b2->mp);
  return bint_norm(mrb, b3);
}

mrb_value
mrb_bint_or(mrb_state *mrb, mrb_value x, mrb_value y)
{
  struct RBigint *b1 = RBIGINT(x);
  struct RBigint *b3 = bint_new(mrb);

#ifndef MRB_NO_FLOAT
  if (mrb_float_p(y)) {
    mpz_t z;
    mpz_init_set_int(mrb, &z, (mrb_int)mrb_float(y));
    mpz_or(mrb, &b3->mp, &b1->mp, &z);
    mpz_clear(mrb, &z);
    return bint_norm(mrb, b3);
  }
#endif
  y = mrb_as_bint(mrb, y);
  struct RBigint *b2 = RBIGINT(y);
  mpz_or(mrb, &b3->mp, &b1->mp, &b2->mp);
  return bint_norm(mrb, b3);
}

mrb_value
mrb_bint_xor(mrb_state *mrb, mrb_value x, mrb_value y)
{
  struct RBigint *b3 = bint_new(mrb);
  struct RBigint *b1 = RBIGINT(x);

#ifndef MRB_NO_FLOAT
  if (mrb_float_p(y)) {
    mpz_t z;
    mpz_init_set_int(mrb, &z, (mrb_int)mrb_float(y));
    mpz_xor(mrb, &b3->mp, &b1->mp, &z);
    mpz_clear(mrb, &z);
    return bint_norm(mrb, b3);
  }
#endif
  y = mrb_as_bint(mrb, y);
  struct RBigint *b2 = RBIGINT(y);
  mpz_xor(mrb, &b3->mp, &b1->mp, &b2->mp);
  return bint_norm(mrb, b3);
}

mrb_value
mrb_bint_rev(mrb_state *mrb, mrb_value x)
{
  struct RBigint *b1 = RBIGINT(x);
  struct RBigint *b2 = bint_new(mrb);

  mpz_neg(mrb, &b2->mp, &b1->mp);
  mpz_sub_int(mrb, &b2->mp, &b2->mp, 1);
  return bint_norm(mrb, b2);
}

mrb_value
mrb_bint_lshift(mrb_state *mrb, mrb_value x, mrb_int width)
{
  struct RBigint *b = RBIGINT(x);
  struct RBigint *b2 = bint_new(mrb);
  if (width < 0) {
    mpz_div_2exp(mrb, &b2->mp, &b->mp, -width);
  }
  else {
    mpz_mul_2exp(mrb, &b2->mp, &b->mp, width);
  }
  return bint_norm(mrb, b2);
}

mrb_value
mrb_bint_rshift(mrb_state *mrb, mrb_value x, mrb_int width)
{
  struct RBigint *b = RBIGINT(x);
  struct RBigint *b2 = bint_new(mrb);
  if (width < 0) {
    mpz_mul_2exp(mrb, &b2->mp, &b->mp, -width);
  }
  else {
    mpz_div_2exp(mrb, &b2->mp, &b->mp, width);
  }
  return bint_norm(mrb, b2);
}

void
mrb_bint_copy(mrb_state *mrb, mrb_value x, mrb_value y)
{
  struct RBigint *b = RBIGINT(x);
  struct RBigint *b2 = RBIGINT(y);
  mpz_init_set(mrb, &b->mp, &b2->mp);
}

size_t
mrb_bint_memsize(mrb_value x)
{
  struct RBigint *b = RBIGINT(x);
  return b->mp.sz * sizeof(mp_limb);
}

mrb_value
mrb_bint_hash(mrb_state *mrb, mrb_value x)
{
  struct RBigint *b = RBIGINT(x);
  uint32_t hash = mrb_byte_hash((uint8_t*)b->mp.p, b->mp.sz);
  hash = mrb_byte_hash_step((uint8_t*)&b->mp.sn, sizeof(b->mp.sn), hash);
  return mrb_int_value(mrb, hash);
}

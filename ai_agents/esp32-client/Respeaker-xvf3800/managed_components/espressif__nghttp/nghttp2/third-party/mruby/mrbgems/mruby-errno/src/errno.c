#include "mruby.h"
#include "mruby/array.h"
#include "mruby/class.h"
#include "mruby/error.h"
#include "mruby/hash.h"
#include "mruby/numeric.h"
#include "mruby/string.h"
#include "mruby/variable.h"
#include "mruby/internal.h"
#include <mruby/presym.h>
#include <errno.h>
#include <string.h>

static const struct {
#ifdef MRB_NO_PRESYM
#define itsdefined(name, sym)   { #name, name },
  const char *name;
#else
#define itsdefined(name, sym)   { sym, name },
  mrb_sym sym;
#endif
  int eno;
} e2c[] = {
#define itsnotdefined(name, sym)
#include "known_errors_def.cstub"
#undef itsdefined
#undef itsnotdefined
};

static const struct {
#ifdef MRB_NO_PRESYM
#define itsnotdefined(name, sym)   { #name },
  const char *name;
#else
#define itsnotdefined(name, sym)   { sym },
  mrb_sym sym;
#endif
} noe2c[] = {
#define itsdefined(name, sym)
#include "known_errors_def.cstub"
#undef itsdefined
#undef itsnotdefined
};

#ifdef MRB_NO_PRESYM
#define ENTRY_SYM(e)    mrb_intern_static(mrb, (e).name, strlen((e).name))
#else
#define ENTRY_SYM(e)    (e).sym
#endif

#define E2C_LEN         (sizeof(e2c) / sizeof(e2c[0]))
#define NOE2C_LEN       (sizeof(noe2c) / sizeof(noe2c[0]))

static struct RClass*
mrb_errno_define_exxx(mrb_state *mrb, mrb_sym name, int eno)
{
  struct RClass *errno_module = mrb_module_get_id(mrb, MRB_SYM(Errno));

  if (mrb_const_defined_at(mrb, mrb_obj_value(errno_module), name)) {
    mrb_value v = mrb_const_get(mrb, mrb_obj_value(errno_module), name);

    if (mrb_class_p(v)) {
      return mrb_class_ptr(v);
    }
  }

  struct RClass *sce_class = mrb_exc_get_id(mrb, MRB_SYM(SystemCallError));
  struct RClass *e = mrb_define_class_under_id(mrb, errno_module, name, sce_class);
  mrb_define_const_id(mrb, e, MRB_SYM(Errno), mrb_fixnum_value(eno));

  return e;
}

#ifndef MRB_NO_PRESYM
typedef mrb_sym sym_ref;
#define sym_ref_init(mrb, id) (id)
#define errno_name_matched_p(errentry, ref) ((errentry).sym == *(ref))

#else
typedef struct {
  const char *name;
  size_t len;
} sym_ref;

static sym_ref
sym_ref_init(mrb_state *mrb, mrb_sym id)
{
  mrb_int len = 0;
  const char *name = mrb_sym_name_len(mrb, id, &len);
  sym_ref ename = { name, (size_t)len };
  return ename;
}

#define errno_name_matched_p(errentry, ref) errno_name_matched_p_0((errentry).name, (ref))
static mrb_bool
errno_name_matched_p_0(const char *name, const sym_ref *ref)
{
  if (ref->len == strlen(name) && memcmp(ref->name, name, ref->len) == 0) {
    return TRUE;
  }
  else {
    return FALSE;
  }
}
#endif // MRB_NO_PRESYM

static mrb_bool
ary_included_in_head(mrb_state *mrb, mrb_value ary, mrb_value obj, mrb_ssize head)
{
  const mrb_value *p = RARRAY_PTR(ary);

  for (; head > 0; head--, p++) {
    if (mrb_obj_eq(mrb, obj, *p)) {
      return TRUE;
    }
  }

  return FALSE;
}

static mrb_value
mrb_errno_defined_p(mrb_state *mrb, mrb_value self)
{
  mrb_sym name;
  mrb_get_args(mrb, "n", &name);
  const sym_ref ref = sym_ref_init(mrb, name);

  for (size_t i = 0; i < E2C_LEN; i++) {
    if (errno_name_matched_p(e2c[i], &ref)) {
      return mrb_true_value();
    }
  }

  for (size_t i = 0; i < NOE2C_LEN; i++) {
    if (errno_name_matched_p(noe2c[i], &ref)) {
      return mrb_true_value();
    }
  }

  return mrb_false_value();
}

static mrb_value
mrb_errno_define(mrb_state *mrb, mrb_value self)
{
  mrb_sym name;
  mrb_get_args(mrb, "n", &name);
  const sym_ref ref = sym_ref_init(mrb, name);

  for (size_t i = 0; i < E2C_LEN; i++) {
    if (errno_name_matched_p(e2c[i], &ref)) {
      return mrb_obj_value(mrb_errno_define_exxx(mrb, name, e2c[i].eno));
    }
  }

  for (size_t i = 0; i < NOE2C_LEN; i++) {
    if (errno_name_matched_p(noe2c[i], &ref)) {
      struct RClass *errno_module = mrb_module_get_id(mrb, MRB_SYM(Errno));
      return mrb_obj_value(mrb_class_get_under_id(mrb, errno_module, MRB_SYM(NOERROR)));
    }
  }

  return mrb_nil_value();
}

static mrb_value
mrb_errno_list(mrb_state *mrb, mrb_value self)
{
  mrb_value list;
  mrb_get_args(mrb, "A", &list);

  mrb_ary_modify(mrb, mrb_ary_ptr(list));
  mrb_ssize head = RARRAY_LEN(list);

  for (size_t i = 0; i < E2C_LEN; i++) {
    mrb_value id = mrb_symbol_value(ENTRY_SYM(e2c[i]));
    if (!ary_included_in_head(mrb, list, id, head)) {
      mrb_ary_push(mrb, list, id);
    }
  }

  for (size_t i = 0; i < NOE2C_LEN; i++) {
    mrb_value id = mrb_symbol_value(ENTRY_SYM(noe2c[i]));
    if (!ary_included_in_head(mrb, list, id, head)) {
      mrb_ary_push(mrb, list, id);
    }
  }

  return list;
}

static void
mrb_sce_init(mrb_state *mrb, mrb_value self, mrb_value m, mrb_value no)
{
  mrb_value str;
  char buf[20];

  if (!mrb_nil_p(no)) {
    size_t i;
    int n = (int)mrb_as_int(mrb, no);

    for (i=0; i < E2C_LEN; i++) {
      if (e2c[i].eno == n) {
        mrb_basic_ptr(self)->c = mrb_errno_define_exxx(mrb, ENTRY_SYM(e2c[i]), e2c[i].eno);
        str = mrb_str_new_cstr(mrb, strerror(n));
        break;
      }
    }
    if (i == E2C_LEN) {
      mrb_iv_set(mrb, self, MRB_SYM(errno), mrb_fixnum_value(n));
      str = mrb_str_new_cstr(mrb, "Unknown error: ");
      char *bp = mrb_int_to_cstr(buf, sizeof(buf), n, 10);
      mrb_str_cat2(mrb, str, bp);
    }
  }
  else {
    str = mrb_str_new_cstr(mrb, "unknown error");
  }
  if (!mrb_nil_p(m)) {
    mrb_str_cat2(mrb, str, " - ");
    mrb_str_append(mrb, str, m);
  }
  mrb_exc_mesg_set(mrb, mrb_exc_ptr(self), str);
}

static mrb_value
mrb_exxx_init(mrb_state *mrb, mrb_value self)
{
  mrb_value m = mrb_nil_value();

  mrb_get_args(mrb, "|S", &m);
  mrb_sce_init(mrb, self, m, mrb_nil_value());
  return self;
}

static mrb_value
mrb_sce_init_m(mrb_state *mrb, mrb_value self)
{
  if (mrb_class(mrb, self) != mrb_exc_get_id(mrb, MRB_SYM(SystemCallError))) {
    return mrb_exxx_init(mrb, self);
  }

  mrb_value m, n;

  if (mrb_get_args(mrb, "o|o", &m, &n) == 1) {
    if (mrb_fixnum_p(m)) {
      n = m;
      m = mrb_nil_value();
    }
    else {
      n = mrb_nil_value();
    }
  }
  mrb_sce_init(mrb, self, m, n);
  return self;
}

static mrb_value
mrb_sce_errno(mrb_state *mrb, mrb_value self)
{
  struct RClass *c;
  mrb_sym sym;

  c = mrb_class(mrb, self);
  sym = MRB_SYM(Errno);
  if (mrb_const_defined_at(mrb, mrb_obj_value(c), sym)) {
    return mrb_const_get(mrb, mrb_obj_value(c), sym);
  }
  else {
    sym = MRB_SYM(errno);
    return mrb_attr_get(mrb, self, sym);
  }
}

static mrb_value
mrb_sce_sys_fail(mrb_state *mrb, mrb_value cls)
{
  struct RClass *sce;
  mrb_value msg, no;
  mrb_int argc;

  mrb->c->ci->mid = 0;
  sce = mrb_class_ptr(cls);
  argc = mrb_get_args(mrb, "o|S", &no, &msg);

  struct RBasic* e = mrb_obj_alloc(mrb, MRB_TT_EXCEPTION, sce);
  mrb_value exc = mrb_obj_value(e);
  if (argc == 1) {
    msg = mrb_nil_value();
  }
  exc = mrb_obj_value(e);
  mrb_sce_init(mrb, exc, msg, no);
  mrb_exc_raise(mrb, exc);
  return mrb_nil_value();  /* NOTREACHED */
}

void
mrb_mruby_errno_gem_init(mrb_state *mrb)
{
  struct RClass *e, *eno, *sce;

  sce = mrb_define_class(mrb, "SystemCallError", E_STANDARD_ERROR);
  mrb_define_class_method(mrb, sce, "_sys_fail", mrb_sce_sys_fail, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, sce, "errno", mrb_sce_errno, MRB_ARGS_NONE());
  mrb_define_method(mrb, sce, "initialize", mrb_sce_init_m, MRB_ARGS_ARG(1, 1));

  eno = mrb_define_module_id(mrb, MRB_SYM(Errno));
  mrb_define_class_method(mrb, eno, "__errno_defined?", mrb_errno_defined_p, MRB_ARGS_REQ(1));
  mrb_define_class_method(mrb, eno, "__errno_define", mrb_errno_define, MRB_ARGS_REQ(1));
  mrb_define_class_method(mrb, eno, "__errno_list", mrb_errno_list, MRB_ARGS_REQ(1));

  e = mrb_define_class_under_id(mrb, eno, MRB_SYM(NOERROR), sce);
  mrb_define_const_id(mrb, e, MRB_SYM(Errno), mrb_fixnum_value(0));
  //mrb_define_method(mrb, e, "===", mrb_exxx_cmp, MRB_ARGS_REQ(1));

  // Pre-allocation for Errno::ENOMEM only
  mrb_errno_define_exxx(mrb, MRB_SYM(ENOMEM), ENOMEM);
}

void
mrb_mruby_errno_gem_final(mrb_state *mrb)
{
}

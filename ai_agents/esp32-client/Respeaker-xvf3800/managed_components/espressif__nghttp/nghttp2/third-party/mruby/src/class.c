/*
** class.c - Class class
**
** See Copyright Notice in mruby.h
*/

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/hash.h>
#include <mruby/class.h>
#include <mruby/numeric.h>
#include <mruby/proc.h>
#include <mruby/string.h>
#include <mruby/variable.h>
#include <mruby/error.h>
#include <mruby/data.h>
#include <mruby/istruct.h>
#include <mruby/opcode.h>
#include <mruby/internal.h>
#include <mruby/presym.h>

union mt_ptr {
  struct RProc *proc;
  mrb_func_t func;
};

#define MT_KEY_P(k) (((k)>>2) != 0)
#define MT_FUNC_P 1
#define MT_NOARG_P 2
#define MT_EMPTY 0
#define MT_DELETED 1

#define MT_KEY(sym, flags) ((sym)<<2|(flags))
#define MT_FLAGS(func_p, noarg_p) ((func_p)?MT_FUNC_P:0)|((noarg_p)?MT_NOARG_P:0)
#define MT_KEY_SYM(k) ((k)>>2)
#define MT_KEY_FLG(k) ((k)&3)

/* method table structure */
typedef struct mt_tbl {
  int size;
  int alloc;
  union mt_ptr *ptr;
} mt_tbl;

#ifdef MRB_USE_INLINE_METHOD_CACHE
#define MT_INLINE_CACHE_SIZE 256
static uint8_t mt_cache[MT_INLINE_CACHE_SIZE];
#endif

/* Creates the method table. */
static mt_tbl*
mt_new(mrb_state *mrb)
{
  mt_tbl *t;

  t = (mt_tbl*)mrb_malloc(mrb, sizeof(mt_tbl));
  t->size = 0;
  t->alloc = 0;
  t->ptr = NULL;

  return t;
}

static void mt_put(mrb_state *mrb, mt_tbl *t, mrb_sym sym, mrb_sym flags, union mt_ptr ptr);

static void
mt_rehash(mrb_state *mrb, mt_tbl *t)
{
  int old_alloc = t->alloc;
  int new_alloc = old_alloc+8;
  union mt_ptr *old_ptr = t->ptr;

  khash_power2(new_alloc);
  if (old_alloc == new_alloc) return;

  t->ptr = (union mt_ptr*)mrb_calloc(mrb, sizeof(union mt_ptr)+sizeof(mrb_sym), new_alloc);
  t->alloc = new_alloc;
  t->size = 0;
  if (old_alloc == 0) return;

  mrb_sym *keys = (mrb_sym*)&old_ptr[old_alloc];
  union mt_ptr *vals = old_ptr;
  for (int i = 0; i < old_alloc; i++) {
    mrb_sym key = keys[i];
    if (MT_KEY_P(key)) {
      mt_put(mrb, t, MT_KEY_SYM(key), MT_KEY_FLG(key), vals[i]);
    }
  }
  mrb_free(mrb, old_ptr);
}

#define slot_empty_p(slot) ((slot)->key == 0 && (slot)->func_p == 0)

/* Set the value for the symbol in the method table. */
static void
mt_put(mrb_state *mrb, mt_tbl *t, mrb_sym sym, mrb_sym flags, union mt_ptr ptr)
{
  int hash, pos, start, dpos = -1;

  if (t->alloc == 0) {
    mt_rehash(mrb, t);
  }

  mrb_sym *keys = (mrb_sym*)&t->ptr[t->alloc];
  union mt_ptr *vals = t->ptr;
  hash = kh_int_hash_func(mrb, sym);
  start = pos = hash & (t->alloc-1);
  for (;;) {
    mrb_sym key = keys[pos];
    if (MT_KEY_SYM(key) == sym) {
    value_set:
      keys[pos] = MT_KEY(sym, flags);
      vals[pos] = ptr;
      return;
    }
    else if (key == MT_EMPTY) {
      t->size++;
      goto value_set;
    }
    else if (key == MT_DELETED && dpos < 0) {
      dpos = pos;
    }
    pos = (pos+1) & (t->alloc-1);
    if (pos == start) {         /* not found */
      if (dpos > 0) {
        t->size++;
        pos = dpos;
        goto value_set;
      }
      /* no room */
      mt_rehash(mrb, t);
      start = pos = hash & (t->alloc-1);
      keys = (mrb_sym*)&t->ptr[t->alloc];
      vals = t->ptr;
    }
  }
}

/* Get a value for a symbol from the method table. */
static mrb_sym
mt_get(mrb_state *mrb, mt_tbl *t, mrb_sym sym, union mt_ptr *pp)
{
  int hash, pos, start;

  if (t == NULL) return 0;
  if (t->alloc == 0) return 0;
  if (t->size == 0) return 0;

  mrb_sym *keys = (mrb_sym*)&t->ptr[t->alloc];
  union mt_ptr *vals = t->ptr;
  hash = kh_int_hash_func(mrb, sym);
#ifdef MRB_USE_INLINE_METHOD_CACHE
  int cpos = (hash^(uintptr_t)t) % MT_INLINE_CACHE_SIZE;
  pos = mt_cache[cpos];
  if (pos < t->alloc) {
    mrb_sym key = keys[pos];
    if (key) {
      if (MT_KEY_SYM(key) == sym) {
        *pp = vals[pos];
        return key;
      }
    }
  }
#endif
  start = pos = hash & (t->alloc-1);
  for (;;) {
    mrb_sym key = keys[pos];
    if (MT_KEY_SYM(key) == sym) {
      *pp = vals[pos];
#ifdef MRB_USE_INLINE_METHOD_CACHE
      if (pos < 0x100) {
        mt_cache[cpos] = pos;
      }
#endif
      return key;
    }
    else if (key == MT_EMPTY) {
      return 0;
    }
    pos = (pos+1) & (t->alloc-1);
    if (pos == start) {         /* not found */
      return 0;
    }
  }
}

/* Deletes the value for the symbol from the method table. */
static mrb_bool
mt_del(mrb_state *mrb, mt_tbl *t, mrb_sym sym)
{
  int hash, pos, start;

  if (t == NULL) return FALSE;
  if (t->alloc == 0) return  FALSE;
  if (t->size == 0) return FALSE;

  mrb_sym *keys = (mrb_sym*)&t->ptr[t->alloc];
  hash = kh_int_hash_func(mrb, sym);
  start = pos = hash & (t->alloc-1);
  for (;;) {
    mrb_sym key = keys[pos];
    if (MT_KEY_SYM(key) == sym) {
      t->size--;
      keys[pos] = MT_DELETED;
      return TRUE;
    }
    else if (key == MT_EMPTY) {
      return FALSE;
    }
    pos = (pos+1) & (t->alloc-1);
    if (pos == start) {         /* not found */
      return FALSE;
    }
  }
}

/* Copy the method table. */
static struct mt_tbl*
mt_copy(mrb_state *mrb, mt_tbl *t)
{
  mt_tbl *t2;

  if (t == NULL) return NULL;
  if (t->alloc == 0) return NULL;
  if (t->size == 0) return NULL;

  t2 = mt_new(mrb);
  mrb_sym *keys = (mrb_sym*)&t->ptr[t->alloc];
  union mt_ptr *vals = t->ptr;
  for (int i=0; i<t->alloc; i++) {
    if (MT_KEY_P(keys[i])) {
      mt_put(mrb, t2, MT_KEY_SYM(keys[i]), MT_KEY_FLG(keys[i]), vals[i]);
    }
  }
  return t2;
}

/* Free memory of the method table. */
static void
mt_free(mrb_state *mrb, mt_tbl *t)
{
  mrb_free(mrb, t->ptr);
  mrb_free(mrb, t);
}

static inline mrb_method_t
create_method_value(mrb_state *mrb, mrb_sym key, union mt_ptr val)
{
  mrb_method_t m;

  if (key & MT_FUNC_P) {
    MRB_METHOD_FROM_FUNC(m, val.func);
  }
  else {
    MRB_METHOD_FROM_PROC(m, val.proc);
  }
  if (key & MT_NOARG_P) {
    MRB_METHOD_NOARG_SET(m);
  }
  return m;
}

MRB_API void
mrb_mt_foreach(mrb_state *mrb, struct RClass *c, mrb_mt_foreach_func *fn, void *p)
{
  mt_tbl *t = c->mt;

  if (t == NULL) return;
  if (t->alloc == 0) return;
  if (t->size == 0) return;

  mrb_sym *keys = (mrb_sym*)&t->ptr[t->alloc];
  union mt_ptr *vals = t->ptr;
  for (int i=0; i<t->alloc; i++) {
    mrb_sym key = keys[i];
    if (MT_KEY_SYM(key)) {
      if (fn(mrb, MT_KEY_SYM(key), create_method_value(mrb, key, vals[i]), p) != 0)
        return;
    }
  }
  return;
}

void
mrb_gc_mark_mt(mrb_state *mrb, struct RClass *c)
{
  mt_tbl *t = c->mt;

  if (t == NULL) return;
  if (t->alloc == 0) return;
  if (t->size == 0) return;

  mrb_sym *keys = (mrb_sym*)&t->ptr[t->alloc];
  union mt_ptr *vals = t->ptr;
  for (int i=0; i<t->alloc; i++) {
    if (MT_KEY_P(keys[i]) && (keys[i] & MT_FUNC_P) == 0) { /* Proc pointer */
      struct RProc *p = vals[i].proc;
      mrb_gc_mark(mrb, (struct RBasic*)p);
    }
  }
  return;
}

size_t
mrb_gc_mark_mt_size(mrb_state *mrb, struct RClass *c)
{
  struct mt_tbl *h = c->mt;

  if (!h) return 0;
  return (size_t)h->size;
}

size_t
mrb_class_mt_memsize(mrb_state *mrb, struct RClass *c)
{
  struct mt_tbl *h = c->mt;

  if (!h) return 0;
  return sizeof(struct mt_tbl) + (size_t)h->size * sizeof(mrb_method_t);
}

void
mrb_gc_free_mt(mrb_state *mrb, struct RClass *c)
{
  if (c->mt) mt_free(mrb, c->mt);
}

void
mrb_class_name_class(mrb_state *mrb, struct RClass *outer, struct RClass *c, mrb_sym id)
{
  mrb_value name;
  mrb_sym nsym = MRB_SYM(__classname__);

  if (mrb_obj_iv_defined(mrb, (struct RObject*)c, nsym)) return;
  if (outer == NULL || outer == mrb->object_class) {
    name = mrb_symbol_value(id);
  }
  else {
    name = mrb_class_path(mrb, outer);
    if (mrb_nil_p(name)) {      /* unnamed outer class */
      if (outer != mrb->object_class && outer != c) {
        mrb_obj_iv_set_force(mrb, (struct RObject*)c, MRB_SYM(__outer__),
                             mrb_obj_value(outer));
      }
      return;
    }
    else {
      mrb_int len;
      const char *n = mrb_sym_name_len(mrb, id, &len);

      mrb_str_cat_lit(mrb, name, "::");
      mrb_str_cat(mrb, name, n, len);
    }
  }
  mrb_obj_iv_set_force(mrb, (struct RObject*)c, nsym, name);
}

mrb_bool
mrb_const_name_p(mrb_state *mrb, const char *name, mrb_int len)
{
  return len > 0 && ISUPPER(name[0]) && mrb_ident_p(name+1, len-1);
}

static void
setup_class(mrb_state *mrb, struct RClass *outer, struct RClass *c, mrb_sym id)
{
  mrb_class_name_class(mrb, outer, c, id);
  mrb_obj_iv_set(mrb, (struct RObject*)outer, id, mrb_obj_value(c));
}

#define make_metaclass(mrb, c) prepare_singleton_class((mrb), (struct RBasic*)(c))

static void
prepare_singleton_class(mrb_state *mrb, struct RBasic *o)
{
  struct RClass *sc, *c;

  mrb_assert(o->c);
  if (o->c->tt == MRB_TT_SCLASS) return;
  sc = MRB_OBJ_ALLOC(mrb, MRB_TT_SCLASS, mrb->class_class);
  sc->flags |= MRB_FL_CLASS_IS_INHERITED;
  sc->mt = NULL;
  sc->iv = NULL;
  if (o->tt == MRB_TT_CLASS) {
    c = (struct RClass*)o;
    if (!c->super) {
      sc->super = mrb->class_class;
    }
    else {
      sc->super = c->super->c;
    }
  }
  else if (o->tt == MRB_TT_SCLASS) {
    c = (struct RClass*)o;
    while (c->super->tt == MRB_TT_ICLASS)
      c = c->super;
    make_metaclass(mrb, c->super);
    sc->super = c->super->c;
  }
  else {
    sc->super = o->c;
    prepare_singleton_class(mrb, (struct RBasic*)sc);
  }
  o->c = sc;
  mrb_field_write_barrier(mrb, (struct RBasic*)o, (struct RBasic*)sc);
  mrb_obj_iv_set(mrb, (struct RObject*)sc, MRB_SYM(__attached__), mrb_obj_value(o));
  sc->flags |= o->flags & MRB_FL_OBJ_IS_FROZEN;
}

static mrb_value
class_name_str(mrb_state *mrb, struct RClass* c)
{
  mrb_value path = mrb_class_path(mrb, c);
  if (mrb_nil_p(path)) {
    path = c->tt == MRB_TT_MODULE ? mrb_str_new_lit(mrb, "#<Module:") :
                                    mrb_str_new_lit(mrb, "#<Class:");
    mrb_str_cat_str(mrb, path, mrb_ptr_to_str(mrb, c));
    mrb_str_cat_lit(mrb, path, ">");
  }
  return path;
}

static struct RClass*
class_from_sym(mrb_state *mrb, struct RClass *klass, mrb_sym id)
{
  mrb_value c = mrb_const_get(mrb, mrb_obj_value(klass), id);

  mrb_check_type(mrb, c, MRB_TT_CLASS);
  return mrb_class_ptr(c);
}

static struct RClass*
module_from_sym(mrb_state *mrb, struct RClass *klass, mrb_sym id)
{
  mrb_value c = mrb_const_get(mrb, mrb_obj_value(klass), id);

  mrb_check_type(mrb, c, MRB_TT_MODULE);
  return mrb_class_ptr(c);
}

static mrb_bool
class_ptr_p(mrb_value obj)
{
  switch (mrb_type(obj)) {
  case MRB_TT_CLASS:
  case MRB_TT_SCLASS:
  case MRB_TT_MODULE:
    return TRUE;
  default:
    return FALSE;
  }
}

static void
check_if_class_or_module(mrb_state *mrb, mrb_value obj)
{
  if (!class_ptr_p(obj)) {
    mrb_raisef(mrb, E_TYPE_ERROR, "%!v is not a class/module", obj);
  }
}

static struct RClass*
define_module(mrb_state *mrb, mrb_sym name, struct RClass *outer)
{
  struct RClass *m;

  if (mrb_const_defined_at(mrb, mrb_obj_value(outer), name)) {
    return module_from_sym(mrb, outer, name);
  }
  m = mrb_module_new(mrb);
  setup_class(mrb, outer, m, name);

  return m;
}

MRB_API struct RClass*
mrb_define_module_id(mrb_state *mrb, mrb_sym name)
{
  return define_module(mrb, name, mrb->object_class);
}

MRB_API struct RClass*
mrb_define_module(mrb_state *mrb, const char *name)
{
  return define_module(mrb, mrb_intern_cstr(mrb, name), mrb->object_class);
}

struct RClass*
mrb_vm_define_module(mrb_state *mrb, mrb_value outer, mrb_sym id)
{
  check_if_class_or_module(mrb, outer);
  if (mrb_const_defined_at(mrb, outer, id)) {
    mrb_value old = mrb_const_get(mrb, outer, id);

    if (!mrb_module_p(old)) {
      mrb_raisef(mrb, E_TYPE_ERROR, "%!v is not a module", old);
    }
    return mrb_class_ptr(old);
  }
  return define_module(mrb, id, mrb_class_ptr(outer));
}

MRB_API struct RClass*
mrb_define_module_under_id(mrb_state *mrb, struct RClass *outer, mrb_sym name)
{
  struct RClass * c = define_module(mrb, name, outer);

  setup_class(mrb, outer, c, name);
  return c;
}

MRB_API struct RClass*
mrb_define_module_under(mrb_state *mrb, struct RClass *outer, const char *name)
{
  mrb_sym id = mrb_intern_cstr(mrb, name);
  struct RClass * c = define_module(mrb, id, outer);

  setup_class(mrb, outer, c, id);
  return c;
}

static struct RClass*
find_origin(struct RClass *c)
{
  MRB_CLASS_ORIGIN(c);
  return c;
}

static struct RClass*
define_class(mrb_state *mrb, mrb_sym name, struct RClass *super, struct RClass *outer)
{
  struct RClass * c;

  if (mrb_const_defined_at(mrb, mrb_obj_value(outer), name)) {
    c = class_from_sym(mrb, outer, name);
    MRB_CLASS_ORIGIN(c);
    if (super && mrb_class_real(c->super) != super) {
      mrb_raisef(mrb, E_TYPE_ERROR, "superclass mismatch for Class %n (%C not %C)",
                 name, c->super, super);
    }
    return c;
  }

  c = mrb_class_new(mrb, super);
  setup_class(mrb, outer, c, name);

  return c;
}

MRB_API struct RClass*
mrb_define_class_id(mrb_state *mrb, mrb_sym name, struct RClass *super)
{
  if (!super) {
    mrb_warn(mrb, "no super class for '%n', Object assumed", name);
  }
  return define_class(mrb, name, super, mrb->object_class);
}

MRB_API struct RClass*
mrb_define_class(mrb_state *mrb, const char *name, struct RClass *super)
{
  return mrb_define_class_id(mrb, mrb_intern_cstr(mrb, name), super);
}

static mrb_value mrb_do_nothing(mrb_state *mrb, mrb_value);
#ifndef MRB_NO_METHOD_CACHE
static void mc_clear(mrb_state *mrb);
static void mc_clear_by_id(mrb_state *mrb, mrb_sym mid);
#else
#define mc_clear(mrb)
#define mc_clear_by_id(mrb,mid)
#endif

static void
mrb_class_inherited(mrb_state *mrb, struct RClass *super, struct RClass *klass)
{
  mrb_value s;
  mrb_sym mid;

  if (!super)
    super = mrb->object_class;
  super->flags |= MRB_FL_CLASS_IS_INHERITED;
  s = mrb_obj_value(super);
  mid = MRB_SYM(inherited);
  if (!mrb_func_basic_p(mrb, s, mid, mrb_do_nothing)) {
    mrb_value c = mrb_obj_value(klass);
    mrb_funcall_argv(mrb, s, mid, 1, &c);
  }
}

struct RClass*
mrb_vm_define_class(mrb_state *mrb, mrb_value outer, mrb_value super, mrb_sym id)
{
  struct RClass *s;
  struct RClass *c;

  if (!mrb_nil_p(super)) {
    if (!mrb_class_p(super)) {
      mrb_raisef(mrb, E_TYPE_ERROR, "superclass must be a Class (%!v given)", super);
    }
    s = mrb_class_ptr(super);
  }
  else {
    s = NULL;
  }
  check_if_class_or_module(mrb, outer);
  if (mrb_const_defined_at(mrb, outer, id)) {
    mrb_value old = mrb_const_get(mrb, outer, id);

    if (!mrb_class_p(old)) {
      mrb_raisef(mrb, E_TYPE_ERROR, "%!v is not a class", old);
    }
    c = mrb_class_ptr(old);
    if (s) {
      /* check super class */
      if (mrb_class_real(c->super) != s) {
        mrb_raisef(mrb, E_TYPE_ERROR, "superclass mismatch for class %v", old);
      }
    }
    return c;
  }
  c = define_class(mrb, id, s, mrb_class_ptr(outer));
  mrb_class_inherited(mrb, mrb_class_real(c->super), c);

  return c;
}

MRB_API mrb_bool
mrb_class_defined(mrb_state *mrb, const char *name)
{
  mrb_sym sym = mrb_intern_check_cstr(mrb, name);
  if (!sym) return FALSE;
  return mrb_const_defined(mrb, mrb_obj_value(mrb->object_class), sym);
}

MRB_API mrb_bool
mrb_class_defined_id(mrb_state *mrb, mrb_sym name)
{
  return mrb_const_defined(mrb, mrb_obj_value(mrb->object_class), name);
}

MRB_API mrb_bool
mrb_class_defined_under(mrb_state *mrb, struct RClass *outer, const char *name)
{
  mrb_sym sym = mrb_intern_check_cstr(mrb, name);
  if (!sym) return FALSE;
  return mrb_const_defined_at(mrb, mrb_obj_value(outer), sym);
}

MRB_API mrb_bool
mrb_class_defined_under_id(mrb_state *mrb, struct RClass *outer, mrb_sym name)
{
  return mrb_const_defined_at(mrb, mrb_obj_value(outer), name);
}

MRB_API struct RClass*
mrb_class_get_under(mrb_state *mrb, struct RClass *outer, const char *name)
{
  return class_from_sym(mrb, outer, mrb_intern_cstr(mrb, name));
}

MRB_API struct RClass*
mrb_class_get_under_id(mrb_state *mrb, struct RClass *outer, mrb_sym name)
{
  return class_from_sym(mrb, outer, name);
}

MRB_API struct RClass*
mrb_class_get(mrb_state *mrb, const char *name)
{
  return mrb_class_get_under(mrb, mrb->object_class, name);
}

MRB_API struct RClass*
mrb_class_get_id(mrb_state *mrb, mrb_sym name)
{
  return mrb_class_get_under_id(mrb, mrb->object_class, name);
}

MRB_API struct RClass*
mrb_exc_get_id(mrb_state *mrb, mrb_sym name)
{
  struct RClass *exc, *e;
  mrb_value c = mrb_const_get(mrb, mrb_obj_value(mrb->object_class), name);

  if (!mrb_class_p(c)) {
    mrb_raise(mrb, E_EXCEPTION, "exception corrupted");
  }
  exc = e = mrb_class_ptr(c);

  while (e) {
    if (e == E_EXCEPTION)
      return exc;
    e = e->super;
  }
  return E_EXCEPTION;
}

MRB_API struct RClass*
mrb_module_get_under(mrb_state *mrb, struct RClass *outer, const char *name)
{
  return module_from_sym(mrb, outer, mrb_intern_cstr(mrb, name));
}

MRB_API struct RClass*
mrb_module_get_under_id(mrb_state *mrb, struct RClass *outer, mrb_sym name)
{
  return module_from_sym(mrb, outer, name);
}

MRB_API struct RClass*
mrb_module_get(mrb_state *mrb, const char *name)
{
  return mrb_module_get_under(mrb, mrb->object_class, name);
}

MRB_API struct RClass*
mrb_module_get_id(mrb_state *mrb, mrb_sym name)
{
  return mrb_module_get_under_id(mrb, mrb->object_class, name);
}

/*!
 * Defines a class under the namespace of \a outer.
 * \param outer  a class which contains the new class.
 * \param name     name of the new class
 * \param super  a class from which the new class will derive.
 *               NULL means \c Object class.
 * \return the created class
 * \throw TypeError if the constant name \a name is already taken but
 *                  the constant is not a \c Class.
 * \throw NameError if the class is already defined but the class can not
 *                  be reopened because its superclass is not \a super.
 * \post top-level constant named \a name refers the returned class.
 *
 * \note if a class named \a name is already defined and its superclass is
 *       \a super, the function just returns the defined class.
 */
MRB_API struct RClass*
mrb_define_class_under_id(mrb_state *mrb, struct RClass *outer, mrb_sym name, struct RClass *super)
{
  struct RClass * c;

#if 0
  if (!super) {
    mrb_warn(mrb, "no super class for '%C::%n', Object assumed", outer, id);
  }
#endif
  c = define_class(mrb, name, super, outer);
  setup_class(mrb, outer, c, name);
  return c;
}

MRB_API struct RClass*
mrb_define_class_under(mrb_state *mrb, struct RClass *outer, const char *name, struct RClass *super)
{
  return mrb_define_class_under_id(mrb, outer, mrb_intern_cstr(mrb, name), super);
}

MRB_API void
mrb_define_method_raw(mrb_state *mrb, struct RClass *c, mrb_sym mid, mrb_method_t m)
{
  mt_tbl *h;
  union mt_ptr ptr;

  MRB_CLASS_ORIGIN(c);
  h = c->mt;
  if (c->tt == MRB_TT_SCLASS && mrb_frozen_p(c)) {
    mrb_value v = mrb_iv_get(mrb, mrb_obj_value(c), MRB_SYM(__attached__));
    mrb_check_frozen_value(mrb, v);
  }
  else {
    mrb_check_frozen(mrb, c);
  }
  if (!h) h = c->mt = mt_new(mrb);
  if (MRB_METHOD_PROC_P(m)) {
    struct RProc *p = MRB_METHOD_PROC(m);

    ptr.proc = p;
    if (p) {
      if (p->color != MRB_GC_RED) {
        p->flags |= MRB_PROC_SCOPE;
        p->c = NULL;
        mrb_field_write_barrier(mrb, (struct RBasic*)c, (struct RBasic*)p);
        if (!MRB_PROC_ENV_P(p)) {
          MRB_PROC_SET_TARGET_CLASS(p, c);
        }
      }
      else {
        mrb_assert(MRB_FROZEN_P(p) && MRB_PROC_SCOPE_P(p));
        mrb_assert(p->c == NULL && p->upper == NULL && p->e.target_class == NULL);
      }
    }
  }
  else {
    ptr.func = MRB_METHOD_FUNC(m);
  }
  mt_put(mrb, h, mid, MT_FLAGS(MRB_METHOD_FUNC_P(m), MRB_METHOD_NOARG_P(m)), ptr);
  mc_clear_by_id(mrb, mid);
}

MRB_API void
mrb_define_method_id(mrb_state *mrb, struct RClass *c, mrb_sym mid, mrb_func_t func, mrb_aspec aspec)
{
  mrb_method_t m;
  int ai = mrb_gc_arena_save(mrb);

  MRB_METHOD_FROM_FUNC(m, func);
#ifndef MRB_USE_METHOD_T_STRUCT
  mrb_assert(MRB_METHOD_FUNC(m) == func);
#endif
  if (aspec == MRB_ARGS_NONE()) {
    MRB_METHOD_NOARG_SET(m);
  }
  mrb_define_method_raw(mrb, c, mid, m);
  mrb_gc_arena_restore(mrb, ai);
}

MRB_API void
mrb_define_method(mrb_state *mrb, struct RClass *c, const char *name, mrb_func_t func, mrb_aspec aspec)
{
  mrb_define_method_id(mrb, c, mrb_intern_cstr(mrb, name), func, aspec);
}

/* a function to raise NotImplementedError with current method name */
MRB_API void
mrb_notimplement(mrb_state *mrb)
{
  mrb_callinfo *ci = mrb->c->ci;

  if (ci->mid) {
    mrb_raisef(mrb, E_NOTIMP_ERROR, "%n() function is unimplemented on this machine", ci->mid);
  }
}

/* a function to be replacement of unimplemented method */
MRB_API mrb_value
mrb_notimplement_m(mrb_state *mrb, mrb_value self)
{
  mrb_notimplement(mrb);
  /* not reached */
  return mrb_nil_value();
}

static void
ensure_class_type(mrb_state *mrb, mrb_value val)
{
  if (!class_ptr_p(val)) {
    mrb_raisef(mrb, E_TYPE_ERROR, "%v is not class/module", val);
  }
}

#define to_sym(mrb, ss) mrb_obj_to_sym(mrb, ss)

MRB_API mrb_int
mrb_get_argc(mrb_state *mrb)
{
  mrb_int argc = mrb->c->ci->n;

  if (argc == 15) {
    struct RArray *a = mrb_ary_ptr(mrb->c->ci->stack[1]);

    argc = ARY_LEN(a);
  }
  return argc;
}

MRB_API const mrb_value*
mrb_get_argv(mrb_state *mrb)
{
  mrb_int argc = mrb->c->ci->n;
  mrb_value *array_argv = mrb->c->ci->stack + 1;
  if (argc == 15) {
    struct RArray *a = mrb_ary_ptr(*array_argv);

    array_argv = ARY_PTR(a);
  }
  return array_argv;
}

MRB_API mrb_value
mrb_get_arg1(mrb_state *mrb)
{
  mrb_callinfo *ci = mrb->c->ci;
  mrb_int argc = ci->n;
  mrb_value *array_argv = ci->stack + 1;
  if (argc == 15) {
    struct RArray *a = mrb_ary_ptr(*array_argv);

    argc = ARY_LEN(a);
    array_argv = ARY_PTR(a);
  }
  if (argc == 0 && ci->nk == 15) {
    mrb_int n = ci->n;
    if (n == 15) n = 1;
    return ci->stack[n+1];      /* kwhash next to positional arguments */
  }
  if (argc != 1) {
    mrb_argnum_error(mrb, argc, 1, 1);
  }
  return array_argv[0];
}

MRB_API mrb_bool
mrb_block_given_p(mrb_state *mrb)
{
  mrb_callinfo *ci = mrb->c->ci;
  mrb_value b = ci->stack[mrb_ci_bidx(ci)];

  return !mrb_nil_p(b);
}

#define GET_ARG(_type) (ptr ? ((_type)(*ptr++)) : va_arg((*ap), _type))

static mrb_int
get_args_v(mrb_state *mrb, mrb_args_format format, void** ptr, va_list *ap)
{
  const char *fmt = format;
  char c;
  mrb_int i = 0;
  mrb_callinfo *ci = mrb->c->ci;
  mrb_int argc = ci->n;
  const mrb_value *argv = ci->stack+1;
  mrb_bool argv_on_stack;
  mrb_bool opt = FALSE;
  mrb_bool opt_skip = TRUE;
  const mrb_value *pickarg = NULL; /* arguments currently being processed */
  mrb_value kdict = mrb_nil_value();
  mrb_bool reqkarg = FALSE;
  int argc_min = 0, argc_max = 0;

  while ((c = *fmt++)) {
    switch (c) {
    case '|':
      opt = TRUE;
      break;
    case '*':
      opt_skip = FALSE;
      argc_max = -1;
      if (!reqkarg) reqkarg = strchr(fmt, ':') ? TRUE : FALSE;
      goto check_exit;
    case '!':
    case '+':
      break;
    case ':':
      reqkarg = TRUE;
      /* fall through */
    case '&': case '?':
      if (opt) opt_skip = FALSE;
      break;
    default:
      if (!opt) argc_min++;
      argc_max++;
      break;
    }
  }

 check_exit:
  if (!reqkarg && ci->nk > 0) {
    mrb_assert(ci->nk == 15);
    kdict = ci->stack[mrb_ci_bidx(ci)-1];
    if (mrb_hash_p(kdict) && mrb_hash_size(mrb, kdict) > 0) {
      if (argc < 14) {
        ci->n++;
        argc++;    /* include kdict in normal arguments */
      }
      else {
        /* 14+1 == 15 so pack first */
        if (argc == 14) {
          /* pack arguments and kdict */
          ci->stack[1] = mrb_ary_new_from_values(mrb, argc+1, &ci->stack[1]);
          argc = ci->n = 15;
        }
        else {
          /* push kdict to packed arguments */
          mrb_ary_push(mrb, ci->stack[1], kdict);
        }
        ci->stack[2] = ci->stack[mrb_ci_bidx(ci)];
      }
      ci->nk = 0;
    }
  }
  if (reqkarg && ci->nk > 0) {
    kdict = ci->stack[mrb_ci_bidx(ci)-1];
    mrb_assert(ci->nk == 15);
    mrb_assert(mrb_hash_p(kdict));
  }

  argv_on_stack = argc < 15;
  if (!argv_on_stack) {
    struct RArray *a = mrb_ary_ptr(*argv);
    argv = ARY_PTR(a);
    argc = ARY_LEN(a);
  }

  opt = FALSE;
  i = 0;
  while ((c = *format++)) {
    mrb_bool altmode = FALSE;
    mrb_bool needmodify = FALSE;

    for (; *format; format++) {
      switch (*format) {
      case '!':
        if (altmode) goto modifier_exit; /* not accept for multiple '!' */
        altmode = TRUE;
        break;
      case '+':
        if (needmodify) goto modifier_exit; /* not accept for multiple '+' */
        needmodify = TRUE;
        break;
      default:
        goto modifier_exit;
      }
    }

  modifier_exit:
    switch (c) {
    case '|': case '*': case '&': case '?': case ':':
      if (needmodify) {
      bad_needmodify:
        mrb_raisef(mrb, E_ARGUMENT_ERROR, "wrong `%c+` modified specifier`", c);
      }
      break;
    default:
      if (i < argc) {
        pickarg = &argv[i++];
        if (needmodify && !mrb_nil_p(*pickarg)) {
          mrb_check_frozen_value(mrb, *pickarg);
        }
      }
      else {
        if (opt) {
          pickarg = NULL;
        }
        else {
          mrb_argnum_error(mrb, argc, argc_min, argc_max);
        }
      }
      break;
    }

    switch (c) {
    case 'o':
    case 'C':
    case 'S':
    case 'A':
    case 'H':
      {
        mrb_value *p;

        p = GET_ARG(mrb_value*);
        if (pickarg) {
          if (!(altmode && mrb_nil_p(*pickarg))) {
            switch (c) {
            case 'C': ensure_class_type(mrb, *pickarg); break;
            case 'S': mrb_ensure_string_type(mrb, *pickarg); break;
            case 'A': mrb_ensure_array_type(mrb, *pickarg); break;
            case 'H': mrb_ensure_hash_type(mrb, *pickarg); break;
            }
          }
          *p = *pickarg;
        }
      }
      break;
    case 'c':
      {
        struct RClass **p;

        p = GET_ARG(struct RClass**);
        if (pickarg) {
          if (altmode && mrb_nil_p(*pickarg)) {
            *p = NULL;
          }
          else {
            ensure_class_type(mrb, *pickarg);
            *p = mrb_class_ptr(*pickarg);
          }
        }
      }
      break;
    case 's':
      {
        const char **ps = NULL;
        mrb_int *pl = NULL;

        ps = GET_ARG(const char**);
        pl = GET_ARG(mrb_int*);
        if (needmodify) goto bad_needmodify;
        if (pickarg) {
          if (altmode && mrb_nil_p(*pickarg)) {
            *ps = NULL;
            *pl = 0;
          }
          else {
            mrb_ensure_string_type(mrb, *pickarg);
            *ps = RSTRING_PTR(*pickarg);
            *pl = RSTRING_LEN(*pickarg);
          }
        }
      }
      break;
    case 'z':
      {
        const char **ps;

        ps = GET_ARG(const char**);
        if (needmodify) goto bad_needmodify;
        if (pickarg) {
          if (altmode && mrb_nil_p(*pickarg)) {
            *ps = NULL;
          }
          else {
            mrb_ensure_string_type(mrb, *pickarg);
            *ps = RSTRING_CSTR(mrb, *pickarg);
          }
        }
      }
      break;
    case 'a':
      {
        struct RArray *a;
        const mrb_value **pb;
        mrb_int *pl;

        pb = GET_ARG(const mrb_value**);
        pl = GET_ARG(mrb_int*);
        if (needmodify) goto bad_needmodify;
        if (pickarg) {
          if (altmode && mrb_nil_p(*pickarg)) {
            *pb = NULL;
            *pl = 0;
          }
          else {
            mrb_ensure_array_type(mrb, *pickarg);
            a = mrb_ary_ptr(*pickarg);
            *pb = ARY_PTR(a);
            *pl = ARY_LEN(a);
          }
        }
      }
      break;
#ifndef MRB_NO_FLOAT
    case 'f':
      {
        mrb_float *p;

        p = GET_ARG(mrb_float*);
        if (pickarg) {
          *p = mrb_as_float(mrb, *pickarg);
        }
      }
      break;
#endif
    case 'i':
      {
        mrb_int *p;

        p = GET_ARG(mrb_int*);
        if (pickarg) {
          *p = mrb_as_int(mrb, *pickarg);
        }
      }
      break;
    case 'b':
      {
        mrb_bool *boolp = GET_ARG(mrb_bool*);

        if (pickarg) {
          *boolp = mrb_test(*pickarg);
        }
      }
      break;
    case 'n':
      {
        mrb_sym *symp;

        symp = GET_ARG(mrb_sym*);
        if (pickarg) {
          *symp = to_sym(mrb, *pickarg);
        }
      }
      break;
    case 'd':
      {
        void** datap;
        struct mrb_data_type const* type;

        datap = GET_ARG(void**);
        type = GET_ARG(struct mrb_data_type const*);
        if (pickarg) {
          if (altmode && mrb_nil_p(*pickarg)) {
            *datap = NULL;
          }
          else {
            *datap = mrb_data_get_ptr(mrb, *pickarg, type);
          }
        }
      }
      break;

    case '&':
      {
        mrb_value *p, *bp;

        p = GET_ARG(mrb_value*);
        bp = ci->stack + mrb_ci_bidx(ci);
        if (altmode && mrb_nil_p(*bp)) {
          mrb_raise(mrb, E_ARGUMENT_ERROR, "no block given");
        }
        *p = *bp;
      }
      break;
    case '|':
      if (opt_skip && i == argc) goto finish;
      opt = TRUE;
      break;
    case '?':
      {
        mrb_bool *p;

        p = GET_ARG(mrb_bool*);
        *p = pickarg ? TRUE : FALSE;
      }
      break;

    case '*':
      {
        const mrb_value **var;
        mrb_int *pl;
        mrb_bool nocopy = (altmode || !argv_on_stack) ? TRUE : FALSE;

        var = GET_ARG(const mrb_value**);
        pl = GET_ARG(mrb_int*);
        if (argc > i) {
          *pl = argc-i;
          if (*pl > 0) {
            if (nocopy) {
              *var = argv+i;
            }
            else {
              mrb_value args = mrb_ary_new_from_values(mrb, *pl, argv+i);
              RARRAY(args)->c = NULL;
              *var = RARRAY_PTR(args);
            }
          }
          i = argc;
        }
        else {
          *pl = 0;
          *var = NULL;
        }
      }
      break;

    case ':':
      {
        mrb_value ksrc = mrb_hash_p(kdict) ? mrb_hash_dup(mrb, kdict) : mrb_hash_new(mrb);
        const mrb_kwargs *kwargs = GET_ARG(const mrb_kwargs*);
        mrb_value *rest;

        if (kwargs == NULL) {
          rest = NULL;
        }
        else {
          mrb_int kwnum = kwargs->num;
          mrb_int required = kwargs->required;
          const mrb_sym *kname = kwargs->table;
          mrb_value *values = kwargs->values;
          mrb_int j;
          const mrb_int keyword_max = 40;

          mrb_assert(kwnum >= 0);
          mrb_assert(required >= 0);
          if (kwnum > keyword_max || required > kwnum) {
            mrb_raise(mrb, E_ARGUMENT_ERROR, "keyword number is too large");
          }

          for (j = required; j > 0; j--, kname++, values++) {
            mrb_value k = mrb_symbol_value(*kname);
            if (!mrb_hash_key_p(mrb, ksrc, k)) {
              mrb_raisef(mrb, E_ARGUMENT_ERROR, "missing keyword: %n", *kname);
            }
            *values = mrb_hash_delete_key(mrb, ksrc, k);
            mrb_gc_protect(mrb, *values);
          }

          for (j = kwnum - required; j > 0; j--, kname++, values++) {
            mrb_value k = mrb_symbol_value(*kname);
            if (mrb_hash_key_p(mrb, ksrc, k)) {
              *values = mrb_hash_delete_key(mrb, ksrc, k);
              mrb_gc_protect(mrb, *values);
            }
            else {
              *values = mrb_undef_value();
            }
          }

          rest = kwargs->rest;
        }

        if (rest) {
          *rest = ksrc;
        }
        else if (!mrb_hash_empty_p(mrb, ksrc)) {
          ksrc = mrb_hash_keys(mrb, ksrc);
          ksrc = RARRAY_PTR(ksrc)[0];
          mrb_raisef(mrb, E_ARGUMENT_ERROR, "unknown keyword: %v", ksrc);
        }
      }
      break;

    default:
      mrb_raisef(mrb, E_ARGUMENT_ERROR, "invalid argument specifier %c", c);
      break;
    }
  }

  if (!c && argc > i) {
    mrb_argnum_error(mrb, argc, argc_min, argc_max);
  }

finish:
  return i;
}

/*
  retrieve arguments from mrb_state.

  mrb_get_args(mrb, format, ...)

  returns number of arguments parsed.

  format specifiers:

    string  mruby type     C type                 note
    ----------------------------------------------------------------------------------------------
    o:      Object         [mrb_value]
    C:      Class/Module   [mrb_value]            when ! follows, the value may be nil
    S:      String         [mrb_value]            when ! follows, the value may be nil
    A:      Array          [mrb_value]            when ! follows, the value may be nil
    H:      Hash           [mrb_value]            when ! follows, the value may be nil
    s:      String         [const char*,mrb_int]  Receive two arguments; s! gives (NULL,0) for nil
    z:      String         [const char*]          NUL terminated string; z! gives NULL for nil
    a:      Array          [const mrb_value*,mrb_int] Receive two arguments; a! gives (NULL,0) for nil
    c:      Class/Module   [struct RClass*]       c! gives NULL for nil
    f:      Integer/Float  [mrb_float]
    i:      Integer/Float  [mrb_int]
    b:      boolean        [mrb_bool]
    n:      String/Symbol  [mrb_sym]
    d:      data           [void*,mrb_data_type const] 2nd argument will be used to check data type so it won't be modified; when ! follows, the value may be nil
    &:      block          [mrb_value]            &! raises exception if no block given
    *:      rest argument  [const mrb_value*,mrb_int] The rest of the arguments as an array; *! avoid copy of the stack
    |:      optional                              Following arguments are optional
    ?:      optional given [mrb_bool]             true if preceding argument (optional) is given
    ':':    keyword args   [mrb_kwargs const]     Get keyword arguments

  format modifiers:

    string  note
    ----------------------------------------------------------------------------------------------
    !:      Switch to the alternate mode; The behaviour changes depending on the specifier
    +:      Request a not frozen object; However, except nil value
 */
MRB_API mrb_int
mrb_get_args(mrb_state *mrb, mrb_args_format format, ...)
{
  va_list ap;
  va_start(ap, format);
  mrb_int rc = get_args_v(mrb, format, NULL, &ap);
  va_end(ap);
  return rc;
}

MRB_API mrb_int
mrb_get_args_a(mrb_state *mrb, mrb_args_format format, void **args)
{
  return get_args_v(mrb, format, args, NULL);
}

static struct RClass*
boot_defclass(mrb_state *mrb, struct RClass *super)
{
  struct RClass *c;

  c = MRB_OBJ_ALLOC(mrb, MRB_TT_CLASS, mrb->class_class);
  if (super) {
    c->super = super;
    mrb_field_write_barrier(mrb, (struct RBasic*)c, (struct RBasic*)super);
    c->flags |= MRB_FL_CLASS_IS_INHERITED;
  }
  else {
    c->super = mrb->object_class;
  }
  c->mt = mt_new(mrb);
  return c;
}

static void
boot_initmod(mrb_state *mrb, struct RClass *mod)
{
  if (!mod->mt) {
    mod->mt = mt_new(mrb);
  }
}

static struct RClass*
include_class_new(mrb_state *mrb, struct RClass *m, struct RClass *super)
{
  struct RClass *ic = MRB_OBJ_ALLOC(mrb, MRB_TT_ICLASS, mrb->class_class);
  if (m->tt == MRB_TT_ICLASS) {
    m = m->c;
  }
  MRB_CLASS_ORIGIN(m);
  ic->iv = m->iv;
  ic->mt = m->mt;
  ic->super = super;
  if (m->tt == MRB_TT_ICLASS) {
    ic->c = m->c;
  }
  else {
    ic->c = m;
  }
  return ic;
}

static int
include_module_at(mrb_state *mrb, struct RClass *c, struct RClass *ins_pos, struct RClass *m, int search_super)
{
  struct RClass *p, *ic;
  void *klass_mt = find_origin(c)->mt;

  while (m) {
    int original_seen = FALSE;
    int superclass_seen = FALSE;

    if (c == ins_pos) original_seen = TRUE;
    if (m->flags & MRB_FL_CLASS_IS_PREPENDED)
      goto skip;

    if (klass_mt && klass_mt == m->mt)
      return -1;

    p = c->super;
    while (p) {
      if (c == p) original_seen = TRUE;
      if (p->tt == MRB_TT_ICLASS) {
        if (p->mt == m->mt) {
          if (!superclass_seen && original_seen) {
            ins_pos = p; /* move insert point */
          }
          goto skip;
        }
      }
      else if (p->tt == MRB_TT_CLASS) {
        if (!search_super) break;
        superclass_seen = TRUE;
      }
      p = p->super;
    }

    ic = include_class_new(mrb, m, ins_pos->super);
    m->flags |= MRB_FL_CLASS_IS_INHERITED;
    ins_pos->super = ic;
    mrb_field_write_barrier(mrb, (struct RBasic*)ins_pos, (struct RBasic*)ic);
    ins_pos = ic;
  skip:
    m = m->super;
  }
  mc_clear(mrb);
  return 0;
}

static int
fix_include_module(mrb_state *mrb, struct RBasic *obj, void *data)
{
  struct RClass **m = (struct RClass**)data;

  if (obj->tt == MRB_TT_ICLASS && obj->c == m[0] && !MRB_FLAG_TEST(obj, MRB_FL_CLASS_IS_ORIGIN)) {
    struct RClass *ic = (struct RClass*)obj;
    include_module_at(mrb, ic, ic, m[1], 1);
  }
  return MRB_EACH_OBJ_OK;
}

MRB_API void
mrb_include_module(mrb_state *mrb, struct RClass *c, struct RClass *m)
{
  mrb_check_frozen(mrb, c);
  if (include_module_at(mrb, c, find_origin(c), m, 1) < 0) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "cyclic include detected");
  }
  if (c->tt == MRB_TT_MODULE && (c->flags & MRB_FL_CLASS_IS_INHERITED)) {
    struct RClass *data[2];
    data[0] = c;
    data[1] = m;
    mrb_objspace_each_objects(mrb, fix_include_module, data);
  }
}

static int
fix_prepend_module(mrb_state *mrb, struct RBasic *obj, void *data)
{
  struct RClass **m = (struct RClass**)data;
  struct RClass *c = (struct RClass*)obj;

  if (c->tt == MRB_TT_CLASS || c->tt == MRB_TT_MODULE) {
    struct RClass *p = c->super;
    struct RClass *ins_pos = c;
    while (p) {
      if (c == m[0]) break;
      if (p == m[0]->super->c) {
        ins_pos = c;
      }
      if (p->tt == MRB_TT_CLASS) break;
      if (p->c == m[0]) {
        include_module_at(mrb, ins_pos, ins_pos, m[1], 0);
        break;
      }
      c = p;
      p = p->super;
    }
  }
  return MRB_EACH_OBJ_OK;
}

MRB_API void
mrb_prepend_module(mrb_state *mrb, struct RClass *c, struct RClass *m)
{
  struct RClass *origin;

  mrb_check_frozen(mrb, c);
  if (!(c->flags & MRB_FL_CLASS_IS_PREPENDED)) {
    origin = MRB_OBJ_ALLOC(mrb, MRB_TT_ICLASS, c);
    origin->flags |= MRB_FL_CLASS_IS_ORIGIN | MRB_FL_CLASS_IS_INHERITED;
    origin->super = c->super;
    c->super = origin;
    origin->mt = c->mt;
    c->mt = NULL;
    origin->iv = c->iv;
    mrb_field_write_barrier(mrb, (struct RBasic*)c, (struct RBasic*)origin);
    c->flags |= MRB_FL_CLASS_IS_PREPENDED;
  }
  if (include_module_at(mrb, c, c, m, 0) < 0) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "cyclic prepend detected");
  }
  if (c->tt == MRB_TT_MODULE &&
      (c->flags & (MRB_FL_CLASS_IS_INHERITED|MRB_FL_CLASS_IS_PREPENDED))) {
    struct RClass *data[2];
    data[0] = c;
    data[1] = m;
    mrb_objspace_each_objects(mrb, fix_prepend_module, data);
  }
}

static mrb_value
mrb_mod_prepend_features(mrb_state *mrb, mrb_value mod)
{
  struct RClass *c;

  mrb_check_type(mrb, mod, MRB_TT_MODULE);
  mrb_get_args(mrb, "c", &c);
  mrb_prepend_module(mrb, c, mrb_class_ptr(mod));
  return mod;
}

static mrb_value
mrb_mod_append_features(mrb_state *mrb, mrb_value mod)
{
  struct RClass *c;

  mrb_check_type(mrb, mod, MRB_TT_MODULE);
  mrb_get_args(mrb, "c", &c);
  mrb_include_module(mrb, c, mrb_class_ptr(mod));
  return mod;
}

/* 15.2.2.4.28 */
/*
 *  call-seq:
 *     mod.include?(module)    -> true or false
 *
 *  Returns <code>true</code> if <i>module</i> is included in
 *  <i>mod</i> or one of <i>mod</i>'s ancestors.
 *
 *     module A
 *     end
 *     class B
 *       include A
 *     end
 *     class C < B
 *     end
 *     B.include?(A)   #=> true
 *     C.include?(A)   #=> true
 *     A.include?(A)   #=> false
 */
static mrb_value
mrb_mod_include_p(mrb_state *mrb, mrb_value mod)
{
  mrb_value mod2;
  struct RClass *c = mrb_class_ptr(mod);

  mrb_get_args(mrb, "C", &mod2);
  mrb_check_type(mrb, mod2, MRB_TT_MODULE);

  while (c) {
    if (c->tt == MRB_TT_ICLASS) {
      if (c->c == mrb_class_ptr(mod2)) return mrb_true_value();
    }
    c = c->super;
  }
  return mrb_false_value();
}

static mrb_value
mrb_mod_ancestors(mrb_state *mrb, mrb_value self)
{
  mrb_value result;
  struct RClass *c = mrb_class_ptr(self);
  result = mrb_ary_new(mrb);
  while (c) {
    if (c->tt == MRB_TT_ICLASS) {
      mrb_ary_push(mrb, result, mrb_obj_value(c->c));
    }
    else if (!(c->flags & MRB_FL_CLASS_IS_PREPENDED)) {
      mrb_ary_push(mrb, result, mrb_obj_value(c));
    }
    c = c->super;
  }

  return result;
}

static mrb_value
mrb_mod_extend_object(mrb_state *mrb, mrb_value mod)
{
  mrb_value obj = mrb_get_arg1(mrb);

  mrb_check_type(mrb, mod, MRB_TT_MODULE);
  mrb_include_module(mrb, mrb_class_ptr(mrb_singleton_class(mrb, obj)), mrb_class_ptr(mod));
  return mod;
}

static mrb_value
mrb_mod_initialize(mrb_state *mrb, mrb_value mod)
{
  mrb_value b;
  struct RClass *m = mrb_class_ptr(mod);
  boot_initmod(mrb, m); /* bootstrap a newly initialized module */
  mrb_get_args(mrb, "|&", &b);
  if (!mrb_nil_p(b)) {
    mrb_yield_with_class(mrb, b, 1, &mod, mod, m);
  }
  return mod;
}

static mrb_value
mrb_mod_dummy_visibility(mrb_state *mrb, mrb_value mod)
{
  return mod;
}

MRB_API struct RClass*
mrb_singleton_class_ptr(mrb_state *mrb, mrb_value v)
{
  struct RBasic *obj;

  switch (mrb_type(v)) {
  case MRB_TT_FALSE:
    if (mrb_nil_p(v))
      return mrb->nil_class;
    return mrb->false_class;
  case MRB_TT_TRUE:
    return mrb->true_class;
  case MRB_TT_CPTR:
  case MRB_TT_SYMBOL:
  case MRB_TT_INTEGER:
#ifndef MRB_NO_FLOAT
  case MRB_TT_FLOAT:
#endif
    return NULL;
  default:
    break;
  }
  obj = mrb_basic_ptr(v);
  if (obj->c == NULL) return NULL;
  prepare_singleton_class(mrb, obj);
  return obj->c;
}

MRB_API mrb_value
mrb_singleton_class(mrb_state *mrb, mrb_value v)
{
  struct RClass *c = mrb_singleton_class_ptr(mrb, v);

  if (c == NULL) {
    mrb_raise(mrb, E_TYPE_ERROR, "can't define singleton");
  }
  return mrb_obj_value(c);
}

MRB_API void
mrb_define_singleton_method(mrb_state *mrb, struct RObject *o, const char *name, mrb_func_t func, mrb_aspec aspec)
{
  prepare_singleton_class(mrb, (struct RBasic*)o);
  mrb_define_method_id(mrb, o->c, mrb_intern_cstr(mrb, name), func, aspec);
}

MRB_API void
mrb_define_singleton_method_id(mrb_state *mrb, struct RObject *o, mrb_sym name, mrb_func_t func, mrb_aspec aspec)
{
  prepare_singleton_class(mrb, (struct RBasic*)o);
  mrb_define_method_id(mrb, o->c, name, func, aspec);
}

MRB_API void
mrb_define_class_method(mrb_state *mrb, struct RClass *c, const char *name, mrb_func_t func, mrb_aspec aspec)
{
  mrb_define_singleton_method(mrb, (struct RObject*)c, name, func, aspec);
}

MRB_API void
mrb_define_class_method_id(mrb_state *mrb, struct RClass *c, mrb_sym name, mrb_func_t func, mrb_aspec aspec)
{
  mrb_define_singleton_method_id(mrb, (struct RObject*)c, name, func, aspec);
}

MRB_API void
mrb_define_module_function_id(mrb_state *mrb, struct RClass *c, mrb_sym name, mrb_func_t func, mrb_aspec aspec)
{
  mrb_define_class_method_id(mrb, c, name, func, aspec);
  mrb_define_method_id(mrb, c, name, func, aspec);
}

MRB_API void
mrb_define_module_function(mrb_state *mrb, struct RClass *c, const char *name, mrb_func_t func, mrb_aspec aspec)
{
  mrb_define_module_function_id(mrb, c, mrb_intern_cstr(mrb, name), func, aspec);
}

#ifndef MRB_NO_METHOD_CACHE
/* clear whole method cache table */
static void
mc_clear(mrb_state *mrb)
{
  static const struct mrb_cache_entry ce_zero ={0};

  for (int i=0; i<MRB_METHOD_CACHE_SIZE; i++) {
    mrb->cache[i] = ce_zero;
  }
}

/* clear method cache for a class */
void
mrb_mc_clear_by_class(mrb_state *mrb, struct RClass *c)
{
  struct mrb_cache_entry *mc = mrb->cache;

  for (int i=0; i<MRB_METHOD_CACHE_SIZE; mc++,i++) {
    if (mc->c == c || mc->c0 == c) mc->c = NULL;
  }
}

static void
mc_clear_by_id(mrb_state *mrb, mrb_sym id)
{
  struct mrb_cache_entry *mc = mrb->cache;

  for (int i=0; i<MRB_METHOD_CACHE_SIZE; mc++,i++) {
    if (mc->mid == id) mc->c = NULL;
  }
}
#endif

mrb_method_t
mrb_vm_find_method(mrb_state *mrb, struct RClass *c, struct RClass **cp, mrb_sym mid)
{
  mrb_method_t m;
#ifndef MRB_NO_METHOD_CACHE
  struct RClass *oc = c;
  int h = kh_int_hash_func(mrb, ((intptr_t)oc) ^ mid) & (MRB_METHOD_CACHE_SIZE-1);
  struct mrb_cache_entry *mc = &mrb->cache[h];

  if (mc->c == c && mc->mid == mid) {
    *cp = mc->c0;
    return mc->m;
  }
#endif

  while (c) {
    mt_tbl *h = c->mt;

    if (h) {
      union mt_ptr ptr;
      mrb_sym ret = mt_get(mrb, h, mid, &ptr);
      if (ret) {
        if (ptr.proc == 0) break;
        *cp = c;
        m =  create_method_value(mrb, ret, ptr);
#ifndef MRB_NO_METHOD_CACHE
        mc->c = oc;
        mc->c0 = c;
        mc->mid = mid;
        mc->m = m;
#endif
        return m;
      }
    }
    c = c->super;
  }
  MRB_METHOD_FROM_PROC(m, NULL);
  return m;                  /* no method */
}

MRB_API mrb_method_t
mrb_method_search_vm(mrb_state *mrb, struct RClass **cp, mrb_sym mid)
{
  return mrb_vm_find_method(mrb, *cp, cp, mid);
}

MRB_API mrb_method_t
mrb_method_search(mrb_state *mrb, struct RClass* c, mrb_sym mid)
{
  mrb_method_t m;

  m = mrb_method_search_vm(mrb, &c, mid);
  if (MRB_METHOD_UNDEF_P(m)) {
    mrb_name_error(mrb, mid, "undefined method '%n' for class %C", mid, c);
  }
  return m;
}

#define ONSTACK_ALLOC_MAX 32

static mrb_sym
prepare_name_common(mrb_state *mrb, mrb_sym sym, const char *prefix, const char *suffix)
{
  char onstack[ONSTACK_ALLOC_MAX];
  mrb_int sym_len;
  const char *sym_str = mrb_sym_name_len(mrb, sym, &sym_len);
  size_t prefix_len = prefix ? strlen(prefix) : 0;
  size_t suffix_len = suffix ? strlen(suffix) : 0;
  size_t name_len = sym_len + prefix_len + suffix_len;
  char *buf = name_len > sizeof(onstack) ? (char*)mrb_alloca(mrb, name_len) : onstack;
  char *p = buf;

  if (prefix_len > 0) {
    memcpy(p, prefix, prefix_len);
    p += prefix_len;
  }

  memcpy(p, sym_str, sym_len);
  p += sym_len;

  if (suffix_len > 0) {
    memcpy(p, suffix, suffix_len);
    p += suffix_len;
  }

  return mrb_intern(mrb, buf, name_len);
}

static mrb_value
prepare_ivar_name(mrb_state *mrb, mrb_sym sym)
{
  sym = prepare_name_common(mrb, sym, "@", NULL);
  mrb_iv_name_sym_check(mrb, sym);
  return mrb_symbol_value(sym);
}

static mrb_sym
prepare_writer_name(mrb_state *mrb, mrb_sym sym)
{
  return prepare_name_common(mrb, sym, NULL, "=");
}

static mrb_value
mod_attr_define(mrb_state *mrb, mrb_value mod, mrb_value (*accessor)(mrb_state *, mrb_value), mrb_sym (*access_name)(mrb_state *, mrb_sym))
{
  struct RClass *c = mrb_class_ptr(mod);
  const mrb_value *argv;
  mrb_int argc;
  int ai;

  mrb_get_args(mrb, "*", &argv, &argc);
  ai = mrb_gc_arena_save(mrb);
  for (int i=0; i<argc; i++) {
    mrb_value name;
    mrb_sym method;
    struct RProc *p;
    mrb_method_t m;

    method = to_sym(mrb, argv[i]);
    name = prepare_ivar_name(mrb, method);
    if (access_name) {
      method = access_name(mrb, method);
    }

    p = mrb_proc_new_cfunc_with_env(mrb, accessor, 1, &name);
    MRB_METHOD_FROM_PROC(m, p);
    mrb_define_method_raw(mrb, c, method, m);
    mrb_gc_arena_restore(mrb, ai);
  }
  return mrb_nil_value();
}

static mrb_value
attr_reader(mrb_state *mrb, mrb_value obj)
{
  mrb_value name = mrb_proc_cfunc_env_get(mrb, 0);
  return mrb_iv_get(mrb, obj, to_sym(mrb, name));
}

static mrb_value
mrb_mod_attr_reader(mrb_state *mrb, mrb_value mod)
{
  return mod_attr_define(mrb, mod, attr_reader, NULL);
}

static mrb_value
attr_writer(mrb_state *mrb, mrb_value obj)
{
  mrb_value name = mrb_proc_cfunc_env_get(mrb, 0);
  mrb_value val = mrb_get_arg1(mrb);

  mrb_iv_set(mrb, obj, to_sym(mrb, name), val);
  return val;
}

static mrb_value
mrb_mod_attr_writer(mrb_state *mrb, mrb_value mod)
{
  return mod_attr_define(mrb, mod, attr_writer, prepare_writer_name);
}

static mrb_value
mrb_instance_alloc(mrb_state *mrb, mrb_value cv)
{
  struct RClass *c = mrb_class_ptr(cv);
  struct RObject *o;
  enum mrb_vtype ttype = MRB_INSTANCE_TT(c);

  if (c->tt == MRB_TT_SCLASS)
    mrb_raise(mrb, E_TYPE_ERROR, "can't create instance of singleton class");

  if (c == mrb->nil_class || c == mrb->false_class) {
    mrb_assert(ttype == 0);
  }
  else if (ttype == 0) {
    ttype = MRB_TT_OBJECT;
  }
  if (MRB_UNDEF_ALLOCATOR_P(c)) {
    mrb_raisef(mrb, E_TYPE_ERROR, "allocator undefined for %v", cv);
  }
  if (ttype <= MRB_TT_CPTR) {
    mrb_raisef(mrb, E_TYPE_ERROR, "can't create instance of %v", cv);
  }
  o = (struct RObject*)mrb_obj_alloc(mrb, ttype, c);
  return mrb_obj_value(o);
}

/*
 *  call-seq:
 *     class.new(args, ...)    ->  obj
 *
 *  Creates a new object of <i>class</i>'s class, then
 *  invokes that object's <code>initialize</code> method,
 *  passing it <i>args</i>. This is the method that ends
 *  up getting called whenever an object is constructed using
 *  `.new`.
 *
 */

mrb_value
mrb_instance_new(mrb_state *mrb, mrb_value cv)
{
  mrb_value obj, blk;
  const mrb_value *argv;
  mrb_int argc;
  mrb_sym init;

  mrb_get_args(mrb, "*!&", &argv, &argc, &blk);
  obj = mrb_instance_alloc(mrb, cv);
  init = MRB_SYM(initialize);
  if (!mrb_func_basic_p(mrb, obj, init, mrb_do_nothing)) {
    mrb_funcall_with_block(mrb, obj, init, argc, argv, blk);
  }
  return obj;
}

MRB_API mrb_value
mrb_obj_new(mrb_state *mrb, struct RClass *c, mrb_int argc, const mrb_value *argv)
{
  mrb_value obj;
  mrb_sym mid;

  obj = mrb_instance_alloc(mrb, mrb_obj_value(c));
  mid = MRB_SYM(initialize);
  if (!mrb_func_basic_p(mrb, obj, mid, mrb_do_nothing)) {
    mrb_funcall_argv(mrb, obj, mid, argc, argv);
  }
  return obj;
}

static mrb_value
mrb_class_initialize(mrb_state *mrb, mrb_value obj)
{
  mrb_value a, b;
  struct RClass *c = mrb_class_ptr(obj);

  if (c->iv) {
    mrb_raise(mrb, E_TYPE_ERROR, "already initialized class");
  }
  mrb_get_args(mrb, "|C&", &a, &b);
  if (!mrb_nil_p(b)) {
    mrb_yield_with_class(mrb, b, 1, &obj, obj, c);
  }
  return obj;
}

static mrb_value
mrb_class_new_class(mrb_state *mrb, mrb_value cv)
{
  mrb_int n;
  mrb_value super, blk;
  mrb_value new_class;
  mrb_sym mid;

  n = mrb_get_args(mrb, "|C&", &super, &blk);
  if (n == 0) {
    super = mrb_obj_value(mrb->object_class);
  }
  new_class = mrb_obj_value(mrb_class_new(mrb, mrb_class_ptr(super)));
  mid = MRB_SYM(initialize);
  if (mrb_func_basic_p(mrb, new_class, mid, mrb_class_initialize)) {
    mrb_class_initialize(mrb, new_class);
  }
  else {
    mrb_funcall_with_block(mrb, new_class, mid, n, &super, blk);
  }
  mrb_class_inherited(mrb, mrb_class_ptr(super), mrb_class_ptr(new_class));
  return new_class;
}

static mrb_value
mrb_class_superclass(mrb_state *mrb, mrb_value klass)
{
  struct RClass *c;

  c = mrb_class_ptr(klass);
  c = find_origin(c)->super;
  while (c && c->tt == MRB_TT_ICLASS) {
    c = find_origin(c)->super;
  }
  if (!c) return mrb_nil_value();
  return mrb_obj_value(c);
}

static mrb_value
mrb_do_nothing(mrb_state *mrb, mrb_value cv)
{
  return mrb_nil_value();
}

static mrb_value
mrb_bob_not(mrb_state *mrb, mrb_value cv)
{
  return mrb_bool_value(!mrb_test(cv));
}

/* 15.3.1.3.1  */
/* 15.3.1.3.10 */
/* 15.3.1.3.11 */
/*
 *  call-seq:
 *     obj == other        -> true or false
 *     obj.equal?(other)   -> true or false
 *     obj.eql?(other)     -> true or false
 *
 *  Equality---At the <code>Object</code> level, <code>==</code> returns
 *  <code>true</code> only if <i>obj</i> and <i>other</i> are the
 *  same object. Typically, this method is overridden in descendant
 *  classes to provide class-specific meaning.
 *
 *  Unlike <code>==</code>, the <code>equal?</code> method should never be
 *  overridden by subclasses: it is used to determine object identity
 *  (that is, <code>a.equal?(b)</code> iff <code>a</code> is the same
 *  object as <code>b</code>).
 *
 *  The <code>eql?</code> method returns <code>true</code> if
 *  <i>obj</i> and <i>anObject</i> have the same value. Used by
 *  <code>Hash</code> to test members for equality.  For objects of
 *  class <code>Object</code>, <code>eql?</code> is synonymous with
 *  <code>==</code>. Subclasses normally continue this tradition, but
 *  there are exceptions. <code>Numeric</code> types, for example,
 *  perform type conversion across <code>==</code>, but not across
 *  <code>eql?</code>, so:
 *
 *     1 == 1.0     #=> true
 *     1.eql? 1.0   #=> false
 */
mrb_value
mrb_obj_equal_m(mrb_state *mrb, mrb_value self)
{
  mrb_value arg = mrb_get_arg1(mrb);

  return mrb_bool_value(mrb_obj_equal(mrb, self, arg));
}

MRB_API mrb_bool
mrb_obj_respond_to(mrb_state *mrb, struct RClass* c, mrb_sym mid)
{
  mrb_method_t m;

  m = mrb_method_search_vm(mrb, &c, mid);
  if (MRB_METHOD_UNDEF_P(m)) {
    return FALSE;
  }
  return TRUE;
}

MRB_API mrb_bool
mrb_respond_to(mrb_state *mrb, mrb_value obj, mrb_sym mid)
{
  return mrb_obj_respond_to(mrb, mrb_class(mrb, obj), mid);
}

MRB_API mrb_value
mrb_class_path(mrb_state *mrb, struct RClass *c)
{
  mrb_value path;
  mrb_sym nsym = MRB_SYM(__classname__);

  path = mrb_obj_iv_get(mrb, (struct RObject*)c, nsym);
  if (mrb_nil_p(path)) {
    /* no name (yet) */
    return mrb_class_find_path(mrb, c);
  }
  else if (mrb_symbol_p(path)) {
    /* toplevel class/module */
    return mrb_sym_str(mrb, mrb_symbol(path));
  }
  return mrb_str_dup(mrb, path);
}

MRB_API struct RClass*
mrb_class_real(struct RClass* cl)
{
  if (cl == 0) return NULL;
  while ((cl->tt == MRB_TT_SCLASS) || (cl->tt == MRB_TT_ICLASS)) {
    cl = cl->super;
    if (cl == 0) return NULL;
  }
  return cl;
}

MRB_API const char*
mrb_class_name(mrb_state *mrb, struct RClass* c)
{
  mrb_value name;

  if (c == NULL) return NULL;
  name = class_name_str(mrb, c);
  return RSTRING_PTR(name);
}

MRB_API const char*
mrb_obj_classname(mrb_state *mrb, mrb_value obj)
{
  return mrb_class_name(mrb, mrb_obj_class(mrb, obj));
}

/*!
 * Ensures a class can be derived from super.
 *
 * \param super a reference to an object.
 * \exception TypeError if \a super is not a Class or \a super is a singleton class.
 */
static void
mrb_check_inheritable(mrb_state *mrb, struct RClass *super)
{
  if (super->tt != MRB_TT_CLASS) {
    mrb_raisef(mrb, E_TYPE_ERROR, "superclass must be a Class (%C given)", super);
  }
  if (super->tt == MRB_TT_SCLASS) {
    mrb_raise(mrb, E_TYPE_ERROR, "can't make subclass of singleton class");
  }
  if (super == mrb->class_class) {
    mrb_raise(mrb, E_TYPE_ERROR, "can't make subclass of Class");
  }
}

/*!
 * Creates a new class.
 * \param super     a class from which the new class derives.
 * \exception TypeError \a super is not inheritable.
 * \exception TypeError \a super is the Class class.
 */
MRB_API struct RClass*
mrb_class_new(mrb_state *mrb, struct RClass *super)
{
  struct RClass *c;

  if (super) {
    mrb_check_inheritable(mrb, super);
  }
  c = boot_defclass(mrb, super);
  if (super) {
    MRB_SET_INSTANCE_TT(c, MRB_INSTANCE_TT(super));
    c->flags |= super->flags & MRB_FL_UNDEF_ALLOCATE;
  }
  make_metaclass(mrb, c);

  return c;
}

/*!
 * Creates a new module.
 */
MRB_API struct RClass*
mrb_module_new(mrb_state *mrb)
{
  struct RClass *m = MRB_OBJ_ALLOC(mrb, MRB_TT_MODULE, mrb->module_class);
  boot_initmod(mrb, m);
  return m;
}

/*
 *  call-seq:
 *     obj.class    => class
 *
 *  Returns the class of <i>obj</i>, now preferred over
 *  <code>Object#type</code>, as an object's type in Ruby is only
 *  loosely tied to that object's class. This method must always be
 *  called with an explicit receiver, as <code>class</code> is also a
 *  reserved word in Ruby.
 *
 *     1.class      #=> Integer
 *     self.class   #=> Object
 */

MRB_API struct RClass*
mrb_obj_class(mrb_state *mrb, mrb_value obj)
{
  return mrb_class_real(mrb_class(mrb, obj));
}

MRB_API void
mrb_alias_method(mrb_state *mrb, struct RClass *c, mrb_sym a, mrb_sym b)
{
  if (a == b) return;
  mrb_method_t m = mrb_method_search(mrb, c, b);

  if (!MRB_METHOD_CFUNC_P(m)) {
    struct RProc *p = MRB_METHOD_PROC(m);
    if (!MRB_PROC_CFUNC_P(p) && !MRB_PROC_ALIAS_P(p)) {
      struct RProc *pnew = MRB_OBJ_ALLOC(mrb, MRB_TT_PROC, mrb->proc_class);

      pnew->body.mid = b;
      pnew->upper = p;
      pnew->e.env = NULL;
      pnew->flags |= MRB_PROC_ALIAS;
      MRB_METHOD_FROM_PROC(m, pnew);
    }
  }
  mrb_define_method_raw(mrb, c, a, m);
}

/*!
 * Defines an alias of a method.
 * \param mrb    the mruby state
 * \param klass  the class which the original method belongs to
 * \param name1  a new name for the method
 * \param name2  the original name of the method
 */
MRB_API void
mrb_define_alias(mrb_state *mrb, struct RClass *klass, const char *name1, const char *name2)
{
  mrb_alias_method(mrb, klass, mrb_intern_cstr(mrb, name1), mrb_intern_cstr(mrb, name2));
}

MRB_API void
mrb_define_alias_id(mrb_state *mrb, struct RClass *klass, mrb_sym a, mrb_sym b)
{
  mrb_alias_method(mrb, klass, a, b);
}

/*
 * call-seq:
 *   mod.to_s   -> string
 *
 * Return a string representing this module or class. For basic
 * classes and modules, this is the name. For singletons, we
 * show information on the thing we're attached to as well.
 */

mrb_value
mrb_mod_to_s(mrb_state *mrb, mrb_value klass)
{
  if (mrb_sclass_p(klass)) {
    mrb_value v = mrb_iv_get(mrb, klass, MRB_SYM(__attached__));
    mrb_value str = mrb_str_new_lit(mrb, "#<Class:");

    if (class_ptr_p(v)) {
      mrb_str_cat_str(mrb, str, mrb_inspect(mrb, v));
    }
    else {
      mrb_str_cat_str(mrb, str, mrb_any_to_s(mrb, v));
    }
    return mrb_str_cat_lit(mrb, str, ">");
  }
  else {
    return class_name_str(mrb, mrb_class_ptr(klass));
  }
}

static mrb_value
mrb_mod_alias(mrb_state *mrb, mrb_value mod)
{
  struct RClass *c = mrb_class_ptr(mod);
  mrb_sym new_name, old_name;

  mrb_get_args(mrb, "nn", &new_name, &old_name);
  mrb_alias_method(mrb, c, new_name, old_name);
  mrb_method_added(mrb, c, new_name);
  return mod;
}

static void
undef_method(mrb_state *mrb, struct RClass *c, mrb_sym a)
{
  mrb_method_t m;

  MRB_METHOD_FROM_PROC(m, NULL);
  mrb_define_method_raw(mrb, c, a, m);
}

MRB_API void
mrb_undef_method_id(mrb_state *mrb, struct RClass *c, mrb_sym a)
{
  if (!mrb_obj_respond_to(mrb, c, a)) {
    mrb_name_error(mrb, a, "undefined method '%n' for class '%C'", a, c);
  }
  undef_method(mrb, c, a);
}

MRB_API void
mrb_undef_method(mrb_state *mrb, struct RClass *c, const char *name)
{
  undef_method(mrb, c, mrb_intern_cstr(mrb, name));
}

MRB_API void
mrb_undef_class_method_id(mrb_state *mrb, struct RClass *c, mrb_sym name)
{
  mrb_undef_method_id(mrb,  mrb_class_ptr(mrb_singleton_class(mrb, mrb_obj_value(c))), name);
}

MRB_API void
mrb_undef_class_method(mrb_state *mrb, struct RClass *c, const char *name)
{
  mrb_undef_method(mrb,  mrb_class_ptr(mrb_singleton_class(mrb, mrb_obj_value(c))), name);
}

MRB_API void
mrb_remove_method(mrb_state *mrb, struct RClass *c, mrb_sym mid)
{
  mt_tbl *h;

  MRB_CLASS_ORIGIN(c);
  h = c->mt;

  if (h && mt_del(mrb, h, mid)) {
    mc_clear_by_id(mrb, mid);
    return;
  }
  mrb_name_error(mrb, mid, "method '%n' not defined in %C", mid, c);
}

static mrb_value
mrb_mod_undef(mrb_state *mrb, mrb_value mod)
{
  struct RClass *c = mrb_class_ptr(mod);
  mrb_int argc;
  const mrb_value *argv;

  mrb_get_args(mrb, "*", &argv, &argc);
  while (argc--) {
    mrb_undef_method_id(mrb, c, to_sym(mrb, *argv));
    argv++;
  }
  return mrb_nil_value();
}

static void
check_const_name_sym(mrb_state *mrb, mrb_sym id)
{
  mrb_int len;
  const char *name = mrb_sym_name_len(mrb, id, &len);
  if (!mrb_const_name_p(mrb, name, len)) {
    mrb_name_error(mrb, id, "wrong constant name %n", id);
  }
}

static mrb_value
mrb_mod_const_defined(mrb_state *mrb, mrb_value mod)
{
  mrb_sym id;
  mrb_bool inherit = TRUE;

  mrb_get_args(mrb, "n|b", &id, &inherit);
  check_const_name_sym(mrb, id);
  if (inherit) {
    return mrb_bool_value(mrb_const_defined(mrb, mod, id));
  }
  return mrb_bool_value(mrb_const_defined_at(mrb, mod, id));
}

static mrb_value
mrb_const_get_sym(mrb_state *mrb, mrb_value mod, mrb_sym id)
{
  check_const_name_sym(mrb, id);
  return mrb_const_get(mrb, mod, id);
}

static mrb_value
mrb_mod_const_get(mrb_state *mrb, mrb_value mod)
{
  mrb_value path = mrb_get_arg1(mrb);
  mrb_sym id;
  char *ptr;
  mrb_int off, end, len;

  if (mrb_symbol_p(path)) {
    /* const get with symbol */
    id = mrb_symbol(path);
    return mrb_const_get_sym(mrb, mod, id);
  }

  /* const get with class path string */
  mrb_ensure_string_type(mrb, path);
  ptr = RSTRING_PTR(path);
  len = RSTRING_LEN(path);
  off = 0;

  while (off < len) {
    end = mrb_str_index_lit(mrb, path, "::", off);
    end = (end == -1) ? len : end;
    id = mrb_intern(mrb, ptr+off, end-off);
    mod = mrb_const_get_sym(mrb, mod, id);
    if (end == len)
      off = end;
    else {
      off = end + 2;
      if (off == len) {         /* trailing "::" */
        mrb_name_error(mrb, id, "wrong constant name '%v'", path);
      }
    }
  }

  return mod;
}

static mrb_value
mrb_mod_const_set(mrb_state *mrb, mrb_value mod)
{
  mrb_sym id;
  mrb_value value;

  mrb_get_args(mrb, "no", &id, &value);
  check_const_name_sym(mrb, id);
  mrb_const_set(mrb, mod, id, value);
  return value;
}

static mrb_value
mrb_mod_remove_const(mrb_state *mrb, mrb_value mod)
{
  mrb_sym id;
  mrb_value val;

  mrb_get_args(mrb, "n", &id);
  check_const_name_sym(mrb, id);
  val = mrb_iv_remove(mrb, mod, id);
  if (mrb_undef_p(val)) {
    mrb_name_error(mrb, id, "constant %n not defined", id);
  }
  return val;
}

mrb_value
mrb_const_missing(mrb_state *mrb, mrb_value mod, mrb_sym sym)
{
  if (mrb_class_real(mrb_class_ptr(mod)) != mrb->object_class) {
    mrb_name_error(mrb, sym, "uninitialized constant %v::%n", mod, sym);
  }
  else {
    mrb_name_error(mrb, sym, "uninitialized constant %n", sym);
  }
  /* not reached */
  return mrb_nil_value();
}

mrb_value
mrb_mod_const_missing(mrb_state *mrb, mrb_value mod)
{
  mrb_sym sym;

  mrb_get_args(mrb, "n", &sym);
  mrb->c->ci->mid = 0;
  return mrb_const_missing(mrb, mod, sym);
}

/* 15.2.2.4.34 */
/*
 *  call-seq:
 *     mod.method_defined?(symbol)    -> true or false
 *
 *  Returns +true+ if the named method is defined by
 *  _mod_ (or its included modules and, if _mod_ is a class,
 *  its ancestors). Public and protected methods are matched.
 *
 *     module A
 *       def method1()  end
 *     end
 *     class B
 *       def method2()  end
 *     end
 *     class C < B
 *       include A
 *       def method3()  end
 *     end
 *
 *     A.method_defined? :method1    #=> true
 *     C.method_defined? "method1"   #=> true
 *     C.method_defined? "method2"   #=> true
 *     C.method_defined? "method3"   #=> true
 *     C.method_defined? "method4"   #=> false
 */

static mrb_value
mrb_mod_method_defined(mrb_state *mrb, mrb_value mod)
{
  mrb_sym id;

  mrb_get_args(mrb, "n", &id);
  return mrb_bool_value(mrb_obj_respond_to(mrb, mrb_class_ptr(mod), id));
}

void
mrb_method_added(mrb_state *mrb, struct RClass *c, mrb_sym mid)
{
  mrb_sym added;
  mrb_value recv = mrb_obj_value(c);

  if (c->tt == MRB_TT_SCLASS) {
    added = MRB_SYM(singleton_method_added);
    recv = mrb_iv_get(mrb, recv, MRB_SYM(__attached__));
  }
  else {
    added = MRB_SYM(method_added);
  }
  if (!mrb_func_basic_p(mrb, recv, added, mrb_do_nothing)) {
    mrb_value sym = mrb_symbol_value(mid);
    mrb_funcall_argv(mrb, recv, added, 1, &sym);
  }
}

mrb_value
mrb_mod_define_method_m(mrb_state *mrb, struct RClass *c)
{
  struct RProc *p;
  mrb_method_t m;
  mrb_sym mid;
  mrb_value proc = mrb_undef_value();
  mrb_value blk;

  mrb_get_args(mrb, "n|o&", &mid, &proc, &blk);
  switch (mrb_type(proc)) {
    case MRB_TT_PROC:
      blk = proc;
      break;
    case MRB_TT_UNDEF:
      /* ignored */
      break;
    default:
      mrb_raisef(mrb, E_TYPE_ERROR, "wrong argument type %T (expected Proc)", proc);
      break;
  }
  if (mrb_nil_p(blk)) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "no block given");
  }
  p = MRB_OBJ_ALLOC(mrb, MRB_TT_PROC, mrb->proc_class);
  mrb_proc_copy(mrb, p, mrb_proc_ptr(blk));
  p->flags |= MRB_PROC_STRICT;
  MRB_METHOD_FROM_PROC(m, p);
  mrb_define_method_raw(mrb, c, mid, m);
  mrb_method_added(mrb, c, mid);
  return mrb_symbol_value(mid);
}

static mrb_value
mod_define_method(mrb_state *mrb, mrb_value self)
{
  return mrb_mod_define_method_m(mrb, mrb_class_ptr(self));
}

static mrb_value
top_define_method(mrb_state *mrb, mrb_value self)
{
  return mrb_mod_define_method_m(mrb, mrb->object_class);
}

static mrb_value
mrb_mod_eqq(mrb_state *mrb, mrb_value mod)
{
  mrb_value obj = mrb_get_arg1(mrb);
  mrb_bool eqq;

  eqq = mrb_obj_is_kind_of(mrb, obj, mrb_class_ptr(mod));

  return mrb_bool_value(eqq);
}

static mrb_value
mrb_mod_dup(mrb_state *mrb, mrb_value self)
{
  mrb_value mod = mrb_obj_clone(mrb, self);
  MRB_UNSET_FROZEN_FLAG(mrb_obj_ptr(mod));
  return mod;
}

static mrb_value
mrb_mod_module_function(mrb_state *mrb, mrb_value mod)
{
  const mrb_value *argv;
  mrb_int argc;
  mrb_sym mid;
  mrb_method_t m;
  struct RClass *rclass;
  int ai;

  mrb_check_type(mrb, mod, MRB_TT_MODULE);

  mrb_get_args(mrb, "*", &argv, &argc);
  if (argc == 0) {
    /* set MODFUNC SCOPE if implemented */
    return mod;
  }

  /* set PRIVATE method visibility if implemented */
  /* mrb_mod_dummy_visibility(mrb, mod); */

  for (int i=0; i<argc; i++) {
    mrb_check_type(mrb, argv[i], MRB_TT_SYMBOL);

    mid = mrb_symbol(argv[i]);
    rclass = mrb_class_ptr(mod);
    m = mrb_method_search(mrb, rclass, mid);

    prepare_singleton_class(mrb, (struct RBasic*)rclass);
    ai = mrb_gc_arena_save(mrb);
    mrb_define_method_raw(mrb, rclass->c, mid, m);
    mrb_gc_arena_restore(mrb, ai);
  }

  return mod;
}

static struct RClass*
mrb_singleton_class_clone(mrb_state *mrb, mrb_value obj)
{
  struct RClass *klass = mrb_basic_ptr(obj)->c;

  if (klass->tt != MRB_TT_SCLASS)
    return klass;
  else {
    /* copy singleton(unnamed) class */
    struct RClass *clone = (struct RClass*)mrb_obj_alloc(mrb, klass->tt, mrb->class_class);

    switch (mrb_type(obj)) {
    case MRB_TT_CLASS:
    case MRB_TT_SCLASS:
      break;
    default:
      clone->c = mrb_singleton_class_clone(mrb, mrb_obj_value(klass));
      break;
    }
    clone->super = klass->super;
    if (klass->iv) {
      mrb_iv_copy(mrb, mrb_obj_value(clone), mrb_obj_value(klass));
      mrb_obj_iv_set(mrb, (struct RObject*)clone, MRB_SYM(__attached__), obj);
    }
    if (klass->mt) {
      clone->mt = mt_copy(mrb, klass->mt);
    }
    else {
      clone->mt = mt_new(mrb);
    }
    clone->tt = MRB_TT_SCLASS;
    return clone;
  }
}

static void
copy_class(mrb_state *mrb, mrb_value dst, mrb_value src)
{
  struct RClass *dc = mrb_class_ptr(dst);
  struct RClass *sc = mrb_class_ptr(src);
  /* if the origin is not the same as the class, then the origin and
     the current class need to be copied */
  if (sc->flags & MRB_FL_CLASS_IS_PREPENDED) {
    struct RClass *c0 = sc->super;
    struct RClass *c1 = dc;

    /* copy prepended iclasses */
    while (!(c0->flags & MRB_FL_CLASS_IS_ORIGIN)) {
      c1->super = mrb_class_ptr(mrb_obj_dup(mrb, mrb_obj_value(c0)));
      c1 = c1->super;
      c0 = c0->super;
    }
    c1->super = mrb_class_ptr(mrb_obj_dup(mrb, mrb_obj_value(c0)));
    c1->super->flags |= MRB_FL_CLASS_IS_ORIGIN;
  }
  if (sc->mt) {
    if (sc->tt == MRB_TT_ICLASS && !(sc->flags & MRB_FL_CLASS_IS_ORIGIN)) {
      dc->mt = sc->mt;
    }
    else {
      dc->mt = mt_copy(mrb, sc->mt);
    }
  }
  dc->super = sc->super;
  dc->flags = sc->flags;
  dc->flags &= ~MRB_FL_OBJ_IS_FROZEN;
}

/* 15.3.1.3.16 */
static mrb_value
mrb_obj_init_copy(mrb_state *mrb, mrb_value self)
{
  mrb_value orig = mrb_get_arg1(mrb);

  if (mrb_obj_equal(mrb, self, orig)) return self;
  if ((mrb_type(self) != mrb_type(orig)) || (mrb_obj_class(mrb, self) != mrb_obj_class(mrb, orig))) {
      mrb_raise(mrb, E_TYPE_ERROR, "initialize_copy should take same class object");
  }
  return self;
}

static void
init_copy(mrb_state *mrb, mrb_value dest, mrb_value obj)
{
  mrb_assert((mrb_type(dest) == mrb_type(obj)));
  switch (mrb_type(obj)) {
    case MRB_TT_ICLASS:
      copy_class(mrb, dest, obj);
      return;
    case MRB_TT_CLASS:
    case MRB_TT_MODULE:
      copy_class(mrb, dest, obj);
      mrb_iv_copy(mrb, dest, obj);
      mrb_iv_remove(mrb, dest, MRB_SYM(__classname__));
      break;
    case MRB_TT_OBJECT:
    case MRB_TT_SCLASS:
    case MRB_TT_HASH:
    case MRB_TT_CDATA:
    case MRB_TT_EXCEPTION:
      mrb_iv_copy(mrb, dest, obj);
      break;
    case MRB_TT_ISTRUCT:
      mrb_istruct_copy(dest, obj);
      break;
#ifdef MRB_USE_BIGINT
    case MRB_TT_BIGINT:
      mrb_bint_copy(mrb, dest, obj);
      break;
#endif
#ifdef MRB_USE_RATIONAL
    case MRB_TT_RATIONAL:
      mrb_rational_copy(mrb, dest, obj);
      break;
#endif
#ifdef MRB_USE_COMPLEX
    case MRB_TT_COMPLEX:
      mrb_complex_copy(mrb, dest, obj);
      break;
#endif

    default:
      break;
  }
  if (!mrb_func_basic_p(mrb, dest, MRB_SYM(initialize_copy), mrb_obj_init_copy)) {
    mrb_funcall_argv(mrb, dest, MRB_SYM(initialize_copy), 1, &obj);
  }
}

/* 15.3.1.3.8  */
/*
 *  call-seq:
 *     obj.clone -> an_object
 *
 *  Produces a shallow copy of <i>obj</i>---the instance variables of
 *  <i>obj</i> are copied, but not the objects they reference. Copies
 *  the frozen state of <i>obj</i>. See also the discussion
 *  under <code>Object#dup</code>.
 *
 *     class Klass
 *        attr_accessor :str
 *     end
 *     s1 = Klass.new      #=> #<Klass:0x401b3a38>
 *     s1.str = "Hello"    #=> "Hello"
 *     s2 = s1.clone       #=> #<Klass:0x401b3998 @str="Hello">
 *     s2.str[1,4] = "i"   #=> "i"
 *     s1.inspect          #=> "#<Klass:0x401b3a38 @str=\"Hi\">"
 *     s2.inspect          #=> "#<Klass:0x401b3998 @str=\"Hi\">"
 *
 *  This method may have class-specific behavior.  If so, that
 *  behavior will be documented under the #+initialize_copy+ method of
 *  the class.
 *
 *  Some Class(True False Nil Symbol Integer Float) Object  cannot clone.
 */
MRB_API mrb_value
mrb_obj_clone(mrb_state *mrb, mrb_value self)
{
  struct RObject *p;
  mrb_value clone;

  if (mrb_immediate_p(self)) {
    return self;
  }
  if (mrb_sclass_p(self)) {
    mrb_raise(mrb, E_TYPE_ERROR, "can't clone singleton class");
  }
  p = (struct RObject*)mrb_obj_alloc(mrb, mrb_type(self), mrb_obj_class(mrb, self));
  p->c = mrb_singleton_class_clone(mrb, self);
  mrb_field_write_barrier(mrb, (struct RBasic*)p, (struct RBasic*)p->c);
  clone = mrb_obj_value(p);
  init_copy(mrb, clone, self);
  p->flags |= mrb_obj_ptr(self)->flags & MRB_FL_OBJ_IS_FROZEN;

  return clone;
}

/* 15.3.1.3.9  */
/*
 *  call-seq:
 *     obj.dup -> an_object
 *
 *  Produces a shallow copy of <i>obj</i>---the instance variables of
 *  <i>obj</i> are copied, but not the objects they reference.
 *  <code>dup</code> copies the frozen state of <i>obj</i>. See also
 *  the discussion under <code>Object#clone</code>. In general,
 *  <code>clone</code> and <code>dup</code> may have different semantics
 *  in descendant classes. While <code>clone</code> is used to duplicate
 *  an object, including its internal state, <code>dup</code> typically
 *  uses the class of the descendant object to create the new instance.
 *
 *  This method may have class-specific behavior.  If so, that
 *  behavior will be documented under the #+initialize_copy+ method of
 *  the class.
 */

MRB_API mrb_value
mrb_obj_dup(mrb_state *mrb, mrb_value obj)
{
  struct RBasic *p;
  mrb_value dup;

  if (mrb_immediate_p(obj)) {
    return obj;
  }
  if (mrb_sclass_p(obj)) {
    mrb_raise(mrb, E_TYPE_ERROR, "can't dup singleton class");
  }
  p = mrb_obj_alloc(mrb, mrb_type(obj), mrb_obj_class(mrb, obj));
  dup = mrb_obj_value(p);
  init_copy(mrb, dup, obj);

  return dup;
}

/* implementation of __id__ */
mrb_value mrb_obj_id_m(mrb_state *mrb, mrb_value self);

mrb_noreturn void
mrb_method_missing(mrb_state *mrb, mrb_sym name, mrb_value self, mrb_value args)
{
  mrb_no_method_error(mrb, name, args, "undefined method '%n'", name);
}

/* 15.3.1.3.30 */
/*
 *  call-seq:
 *     obj.method_missing(symbol [, *args] )   -> result
 *
 *  Invoked by Ruby when <i>obj</i> is sent a message it cannot handle.
 *  <i>symbol</i> is the symbol for the method called, and <i>args</i>
 *  are any arguments that were passed to it. By default, the interpreter
 *  raises an error when this method is called. However, it is possible
 *  to override the method to provide more dynamic behavior.
 *  If it is decided that a particular method should not be handled, then
 *  <i>super</i> should be called, so that ancestors can pick up the
 *  missing method.
 *  The example below creates
 *  a class <code>Roman</code>, which responds to methods with names
 *  consisting of roman numerals, returning the corresponding integer
 *  values.
 *
 *     class Roman
 *       def romanToInt(str)
 *         # ...
 *       end
 *       def method_missing(sym)
 *         str = sym.to_s
 *         romanToInt(str)
 *       end
 *     end
 *
 *     r = Roman.new
 *     r.iv      #=> 4
 *     r.xxiii   #=> 23
 *     r.mm      #=> 2000
 */
mrb_value
mrb_obj_missing(mrb_state *mrb, mrb_value mod)
{
  mrb_sym name;
  const mrb_value *a;
  mrb_int alen;

  mrb->c->ci->mid = 0;
  mrb_get_args(mrb, "n*!", &name, &a, &alen);
  mrb_method_missing(mrb, name, mod, mrb_ary_new_from_values(mrb, alen, a));
  /* not reached */
  return mrb_nil_value();
}

static mrb_value
inspect_main(mrb_state *mrb, mrb_value mod)
{
  return mrb_str_new_lit(mrb, "main");
}

static const mrb_code new_iseq[] = {
  OP_ENTER, 0x0, 0x10, 0x3,  // OP_ENTER     0:0:1:0:0:1:1
  OP_LOADSELF, 4,            // OP_LOADSELF  R4
  OP_SEND, 4, 0, 0,          // OP_SEND      R4  :allocate  n=0
  OP_MOVE, 0, 4,             // OP_MOVE      R0  R4
  OP_MOVE, 4, 3,             // OP_MOVE      R4  R3 (&)
  OP_MOVE, 3, 2,             // OP_MOVE      R3  R2 (**)
  OP_MOVE, 2, 1,             // OP_MOVE      R2  R1 (*)
  OP_SSENDB, 1, 1, 255,      // OP_SSENDB    R1  :initialize n=*|nk=*
  OP_RETURN, 0               // OP_RETURN    R0
};

MRB_PRESYM_DEFINE_VAR_AND_INITER(new_syms, 2, MRB_SYM(allocate), MRB_SYM(initialize))

static const mrb_irep new_irep = {
  4, 6, 0, MRB_IREP_STATIC,
  new_iseq, NULL, new_syms, NULL, NULL, NULL,
  sizeof(new_iseq), 0, 2, 0, 0,
};

static const struct RProc new_proc = {
  NULL, NULL, MRB_TT_PROC, MRB_GC_RED, MRB_FL_OBJ_IS_FROZEN | MRB_PROC_SCOPE | MRB_PROC_STRICT,
  { &new_irep }, NULL, { NULL }
};

static void
init_class_new(mrb_state *mrb, struct RClass *cls)
{
  mrb_method_t m;

  MRB_PRESYM_INIT_SYMBOLS(mrb, new_syms);
  MRB_METHOD_FROM_PROC(m, &new_proc);
  mrb_define_method_raw(mrb, cls, MRB_SYM(new), m);
}

void
mrb_init_class(mrb_state *mrb)
{
  struct RClass *bob;           /* BasicObject */
  struct RClass *obj;           /* Object */
  struct RClass *mod;           /* Module */
  struct RClass *cls;           /* Class */

  /* boot class hierarchy */
  bob = boot_defclass(mrb, 0);
  obj = boot_defclass(mrb, bob); mrb->object_class = obj;
  mod = boot_defclass(mrb, obj); mrb->module_class = mod;/* obj -> mod */
  cls = boot_defclass(mrb, mod); mrb->class_class = cls; /* obj -> cls */
  /* fix-up loose ends */
  bob->c = obj->c = mod->c = cls->c = cls;
  make_metaclass(mrb, bob);
  make_metaclass(mrb, obj);
  make_metaclass(mrb, mod);
  make_metaclass(mrb, cls);

  /* name basic classes */
  mrb_define_const_id(mrb, bob, MRB_SYM(BasicObject), mrb_obj_value(bob));
  mrb_define_const_id(mrb, obj, MRB_SYM(Object),      mrb_obj_value(obj));
  mrb_define_const_id(mrb, obj, MRB_SYM(Module),      mrb_obj_value(mod));
  mrb_define_const_id(mrb, obj, MRB_SYM(Class),       mrb_obj_value(cls));

  /* name each classes */
  mrb_class_name_class(mrb, NULL, bob, MRB_SYM(BasicObject));
  mrb_class_name_class(mrb, NULL, obj, MRB_SYM(Object)); /* 15.2.1 */
  mrb_class_name_class(mrb, NULL, mod, MRB_SYM(Module)); /* 15.2.2 */
  mrb_class_name_class(mrb, NULL, cls, MRB_SYM(Class));  /* 15.2.3 */

  MRB_SET_INSTANCE_TT(cls, MRB_TT_CLASS);
  mrb_define_method_id(mrb, bob, MRB_SYM(initialize),              mrb_do_nothing,           MRB_ARGS_NONE());
  mrb_define_method_id(mrb, bob, MRB_OPSYM(not),                   mrb_bob_not,              MRB_ARGS_NONE());
  mrb_define_method_id(mrb, bob, MRB_OPSYM(eq),                    mrb_obj_equal_m,          MRB_ARGS_REQ(1)); /* 15.3.1.3.1  */
  mrb_define_method_id(mrb, bob, MRB_SYM(__id__),                  mrb_obj_id_m,             MRB_ARGS_NONE()); /* 15.3.1.3.4  */
  mrb_define_method_id(mrb, bob, MRB_SYM(__send__),                mrb_f_send,               MRB_ARGS_REQ(1)|MRB_ARGS_REST()|MRB_ARGS_BLOCK());  /* 15.3.1.3.5  */
  mrb_define_method_id(mrb, bob, MRB_SYM_Q(equal),                 mrb_obj_equal_m,          MRB_ARGS_REQ(1)); /* 15.3.1.3.11 */
  mrb_define_method_id(mrb, bob, MRB_SYM(instance_eval),           mrb_obj_instance_eval,    MRB_ARGS_OPT(1)|MRB_ARGS_BLOCK());  /* 15.3.1.3.18 */
  mrb_define_method_id(mrb, bob, MRB_SYM(singleton_method_added),  mrb_do_nothing,           MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, bob, MRB_SYM(method_missing),          mrb_obj_missing,          MRB_ARGS_ANY());  /* 15.3.1.3.30 */

  mrb_define_class_method_id(mrb, cls, MRB_SYM(new),               mrb_class_new_class,      MRB_ARGS_OPT(1)|MRB_ARGS_BLOCK());
  mrb_define_method_id(mrb, cls, MRB_SYM(allocate),                mrb_instance_alloc,       MRB_ARGS_NONE());
  mrb_define_method_id(mrb, cls, MRB_SYM(superclass),              mrb_class_superclass,     MRB_ARGS_NONE()); /* 15.2.3.3.4 */
  mrb_define_method_id(mrb, cls, MRB_SYM(initialize),              mrb_class_initialize,     MRB_ARGS_OPT(1)); /* 15.2.3.3.1 */
  mrb_define_method_id(mrb, cls, MRB_SYM(inherited),               mrb_do_nothing,           MRB_ARGS_REQ(1));

  init_class_new(mrb, cls);

  MRB_SET_INSTANCE_TT(mod, MRB_TT_MODULE);
  mrb_define_method_id(mrb, mod, MRB_SYM(extend_object),           mrb_mod_extend_object,    MRB_ARGS_REQ(1)); /* 15.2.2.4.25 */
  mrb_define_method_id(mrb, mod, MRB_SYM(extended),                mrb_do_nothing,           MRB_ARGS_REQ(1)); /* 15.2.2.4.26 */
  mrb_define_method_id(mrb, mod, MRB_SYM(prepended),               mrb_do_nothing,           MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, mod, MRB_SYM(prepend_features),        mrb_mod_prepend_features, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, mod, MRB_SYM_Q(include),               mrb_mod_include_p,        MRB_ARGS_REQ(1)); /* 15.2.2.4.28 */
  mrb_define_method_id(mrb, mod, MRB_SYM(append_features),         mrb_mod_append_features,  MRB_ARGS_REQ(1)); /* 15.2.2.4.10 */
  mrb_define_method_id(mrb, mod, MRB_SYM(class_eval),              mrb_mod_module_eval,      MRB_ARGS_ANY());  /* 15.2.2.4.15 */
  mrb_define_method_id(mrb, mod, MRB_SYM(included),                mrb_do_nothing,           MRB_ARGS_REQ(1)); /* 15.2.2.4.29 */
  mrb_define_method_id(mrb, mod, MRB_SYM(initialize),              mrb_mod_initialize,       MRB_ARGS_NONE()); /* 15.2.2.4.31 */
  mrb_define_method_id(mrb, mod, MRB_SYM(module_eval),             mrb_mod_module_eval,      MRB_ARGS_ANY());  /* 15.2.2.4.35 */
  mrb_define_method_id(mrb, mod, MRB_SYM(module_function),         mrb_mod_module_function,  MRB_ARGS_ANY());
  mrb_define_method_id(mrb, mod, MRB_SYM(private),                 mrb_mod_dummy_visibility, MRB_ARGS_ANY());  /* 15.2.2.4.36 */
  mrb_define_method_id(mrb, mod, MRB_SYM(protected),               mrb_mod_dummy_visibility, MRB_ARGS_ANY());  /* 15.2.2.4.37 */
  mrb_define_method_id(mrb, mod, MRB_SYM(public),                  mrb_mod_dummy_visibility, MRB_ARGS_ANY());  /* 15.2.2.4.38 */
  mrb_define_method_id(mrb, mod, MRB_SYM(attr_reader),             mrb_mod_attr_reader,      MRB_ARGS_ANY());  /* 15.2.2.4.13 */
  mrb_define_method_id(mrb, mod, MRB_SYM(attr_writer),             mrb_mod_attr_writer,      MRB_ARGS_ANY());  /* 15.2.2.4.14 */
  mrb_define_method_id(mrb, mod, MRB_SYM(to_s),                    mrb_mod_to_s,             MRB_ARGS_NONE());
  mrb_define_method_id(mrb, mod, MRB_SYM(inspect),                 mrb_mod_to_s,             MRB_ARGS_NONE());
  mrb_define_method_id(mrb, mod, MRB_SYM(alias_method),            mrb_mod_alias,            MRB_ARGS_ANY());  /* 15.2.2.4.8 */
  mrb_define_method_id(mrb, mod, MRB_SYM(ancestors),               mrb_mod_ancestors,        MRB_ARGS_NONE()); /* 15.2.2.4.9 */
  mrb_define_method_id(mrb, mod, MRB_SYM(undef_method),            mrb_mod_undef,            MRB_ARGS_ANY());  /* 15.2.2.4.41 */
  mrb_define_method_id(mrb, mod, MRB_SYM_Q(const_defined),         mrb_mod_const_defined,    MRB_ARGS_ARG(1,1)); /* 15.2.2.4.20 */
  mrb_define_method_id(mrb, mod, MRB_SYM(const_get),               mrb_mod_const_get,        MRB_ARGS_REQ(1)); /* 15.2.2.4.21 */
  mrb_define_method_id(mrb, mod, MRB_SYM(const_set),               mrb_mod_const_set,        MRB_ARGS_REQ(2)); /* 15.2.2.4.23 */
  mrb_define_method_id(mrb, mod, MRB_SYM(remove_const),            mrb_mod_remove_const,     MRB_ARGS_REQ(1)); /* 15.2.2.4.40 */
  mrb_define_method_id(mrb, mod, MRB_SYM(const_missing),           mrb_mod_const_missing,    MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, mod, MRB_SYM_Q(method_defined),        mrb_mod_method_defined,   MRB_ARGS_REQ(1)); /* 15.2.2.4.34 */
  mrb_define_method_id(mrb, mod, MRB_SYM(define_method),           mod_define_method,        MRB_ARGS_ARG(1,1));
  mrb_define_method_id(mrb, mod, MRB_OPSYM(eqq),                   mrb_mod_eqq,              MRB_ARGS_REQ(1)); /* 15.2.2.4.7 */
  mrb_define_method_id(mrb, mod, MRB_SYM(dup),                     mrb_mod_dup,              MRB_ARGS_NONE());
  mrb_define_method_id(mrb, mod, MRB_SYM(method_added),            mrb_do_nothing,           MRB_ARGS_REQ(1));

  mrb_undef_method_id(mrb, cls, MRB_SYM(append_features));
  mrb_undef_method_id(mrb, cls, MRB_SYM(prepend_features));
  mrb_undef_method_id(mrb, cls, MRB_SYM(extend_object));
  mrb_undef_method_id(mrb, cls, MRB_SYM(module_function));

  mrb->top_self = MRB_OBJ_ALLOC(mrb, MRB_TT_OBJECT, mrb->object_class);
  mrb_define_singleton_method_id(mrb, mrb->top_self, MRB_SYM(inspect), inspect_main, MRB_ARGS_NONE());
  mrb_define_singleton_method_id(mrb, mrb->top_self, MRB_SYM(to_s), inspect_main, MRB_ARGS_NONE());
  mrb_define_singleton_method_id(mrb, mrb->top_self, MRB_SYM(define_method), top_define_method, MRB_ARGS_ARG(1,1));
}

/*
** data.c - Data class
**
** See Copyright Notice in mruby.h
*/

#include <string.h>
#include <mruby.h>
#include <mruby/array.h>
#include <mruby/string.h>
#include <mruby/class.h>
#include <mruby/variable.h>
#include <mruby/hash.h>
#include <mruby/proc.h>
#include <mruby/internal.h>
#include <mruby/presym.h>

#define RDATA_LEN(st) RARRAY_LEN(st)
#define RDATA_PTR(st) RARRAY_PTR(st)

#define data_p(o) (mrb_type(o) == MRB_TT_STRUCT)

static struct RClass *
data_class(mrb_state *mrb)
{
  return mrb_class_get_id(mrb, MRB_SYM(Data));
}

static void
data_corrupted(mrb_state *mrb)
{
  mrb_raise(mrb, E_TYPE_ERROR, "corrupted data");
}

static mrb_value
data_s_members(mrb_state *mrb, struct RClass *c)
{
  struct RClass* sclass = data_class(mrb);
  mrb_value mem;

  for (;;) {
    mem = mrb_iv_get(mrb, mrb_obj_value(c), MRB_SYM(__members__));
    if (!mrb_nil_p(mem)) {
      if (!mrb_array_p(mem)) {
        data_corrupted(mrb);
      }
      return mem;
    }
    c = c->super;
    if (c == sclass || c == 0) {
      mrb_raise(mrb, E_TYPE_ERROR, "uninitialized data");
    }
  }
}

static mrb_value
data_members(mrb_state *mrb, mrb_value obj)
{
  if (!data_p(obj) || RDATA_LEN(obj) == 0) {
    data_corrupted(mrb);
  }
  mrb_value members = data_s_members(mrb, mrb_obj_class(mrb, obj));
  if (RDATA_LEN(obj) != RARRAY_LEN(members)) {
    mrb_raisef(mrb, E_TYPE_ERROR,
               "data size differs (%i required %i given)",
               RARRAY_LEN(members), RDATA_LEN(obj));
  }
  return members;
}

static mrb_value
mrb_data_s_members(mrb_state *mrb, mrb_value klass)
{
  mrb_value members = data_s_members(mrb, mrb_class_ptr(klass));
  return mrb_ary_new_from_values(mrb, RARRAY_LEN(members), RARRAY_PTR(members));
}

/*
 *  call-seq:
 *     data.members    -> array
 *
 *  Returns an array of strings representing the names of the instance
 *  variables.
 *
 *     Customer = Data.define(:name, :address, :zip)
 *     joe = Customer.new("Joe Smith", "123 Maple, Anytown NC", 12345)
 *     joe.members   #=> [:name, :address, :zip]
 */

static mrb_value
mrb_data_members(mrb_state *mrb, mrb_value obj)
{
  return mrb_data_s_members(mrb, mrb_obj_value(mrb_obj_class(mrb, obj)));
}

static mrb_value
data_ref(mrb_state *mrb, mrb_value obj, mrb_int i)
{
  mrb_int len = RDATA_LEN(obj);
  mrb_value *ptr = RDATA_PTR(obj);

  if (!ptr || len <= i)
    return mrb_nil_value();
  return ptr[i];
}

static mrb_value
mrb_data_ref(mrb_state *mrb, mrb_value obj)
{
  mrb_int argc = mrb_get_argc(mrb);
  if (argc != 0) {
    mrb_argnum_error(mrb, argc, 0, 0);
  }
  mrb_int i = mrb_integer(mrb_proc_cfunc_env_get(mrb, 0));
  return data_ref(mrb, obj, i);
}

static mrb_value
data_ref_0(mrb_state *mrb, mrb_value obj)
{
  return data_ref(mrb, obj, 0);
}

static mrb_value
data_ref_1(mrb_state *mrb, mrb_value obj)
{
  return data_ref(mrb, obj, 1);
}

static mrb_value
data_ref_2(mrb_state *mrb, mrb_value obj)
{
  return data_ref(mrb, obj, 2);
}

static mrb_value
data_ref_3(mrb_state *mrb, mrb_value obj)
{
  return data_ref(mrb, obj, 3);
}

#define DATA_DIRECT_REF_MAX 4

static mrb_func_t aref[DATA_DIRECT_REF_MAX] = {
  data_ref_0,
  data_ref_1,
  data_ref_2,
  data_ref_3,
};

static void
make_data_define_accessors(mrb_state *mrb, mrb_value members, struct RClass *c)
{
  const mrb_value *ptr_members = RARRAY_PTR(members);
  mrb_int len = RARRAY_LEN(members);
  int ai = mrb_gc_arena_save(mrb);

  for (mrb_int i=0; i<len; i++) {
    mrb_sym id = mrb_symbol(ptr_members[i]);

    if (i < DATA_DIRECT_REF_MAX) {
      mrb_define_method_id(mrb, c, id, aref[i], MRB_ARGS_NONE());
    }
    else {
      mrb_method_t m;
      mrb_value at = mrb_fixnum_value(i);
      struct RProc *aref = mrb_proc_new_cfunc_with_env(mrb, mrb_data_ref, 1, &at);
      MRB_METHOD_FROM_PROC(m, aref);
      mrb_define_method_raw(mrb, c, id, m);
      mrb_gc_arena_restore(mrb, ai);
    }
  }
}

static mrb_value mrb_data_initialize(mrb_state *mrb, mrb_value self);

static mrb_value
mrb_data_new(mrb_state *mrb, mrb_value self)
{
  struct RClass *c = mrb_class_ptr(self);
  mrb_value members = data_s_members(mrb, c);
  mrb_value *vals;

  mrb_int n = RARRAY_LEN(members);
  mrb_value *mems = RARRAY_PTR(members);
  if (mrb->c->ci->nk > 0) {
    mrb_value tmp = mrb_str_new(mrb, NULL, sizeof(mrb_sym)*n);
    mrb_sym *knames = (mrb_sym*)RSTRING_PTR(tmp);
    mrb_value m = mrb_ary_new_capa(mrb, n);
    vals = RARRAY_PTR(m);
    for (mrb_int i=0; i<n; i++) {
      knames[i] = mrb_symbol(mems[i]);
    }
    const mrb_kwargs kw = {n, n, knames, vals, NULL};
    mrb_get_args(mrb, ":", &kw);
  }
  else {
    mrb_int argc;
    mrb_get_args(mrb, "*", &vals, &argc);
    if (n != argc) {
      mrb_raise(mrb, E_ARGUMENT_ERROR, "wrong number of arguments");
    }
  }

  struct RArray* p = MRB_OBJ_ALLOC(mrb, MRB_TT_STRUCT, c);
  mrb_value data = mrb_obj_value(p);
  if (!mrb_func_basic_p(mrb, data, MRB_SYM(initialize), mrb_data_initialize)) {
    /* overridden initialize - create hash and call initialize explicitly */
    mrb_value hash = mrb_hash_new_capa(mrb, n);
    for (mrb_int i=0; i<n; i++) {
      mrb_hash_set(mrb, hash, mems[i], vals[i]);
    }
    mrb_funcall_argv(mrb, data, MRB_SYM(initialize), 1, &hash);
  }
  else {
    /* default initialize - skip calling initialize */
    mrb_ary_resize(mrb, data, n);
    for (mrb_int i = 0; i < n; i++) {
      mrb_ary_set(mrb, data, i, vals[i]);
    }
  }
  mrb_obj_freeze(mrb, data);
  return data;
}

static mrb_value
make_data_class(mrb_state *mrb, mrb_value members, struct RClass *klass)
{
  struct RClass *c = mrb_class_new(mrb, klass);
  MRB_SET_INSTANCE_TT(c, MRB_TT_STRUCT);
  MRB_DEFINE_ALLOCATOR(c);
  mrb_value data = mrb_obj_value(c);
  mrb_iv_set(mrb, data, MRB_SYM(__members__), members);

  mrb_undef_class_method(mrb, c, "define");
  mrb_define_class_method_id(mrb, c, MRB_SYM(new), mrb_data_new, MRB_ARGS_ANY());
  mrb_define_class_method_id(mrb, c, MRB_SYM(members), mrb_data_s_members, MRB_ARGS_NONE());
  /* RSTRUCT(data)->basic.c->super = c->c; */
  make_data_define_accessors(mrb, members, c);
  return data;
}

/*
 *  call-seq:
 *     DataClass.new(arg, ...)             -> obj
 *
 *  <code>Data::define</code> returns a new <code>Class</code> object,
 *  which can then be used to create specific instances of the new
 *  data structure. The number of actual parameters must be
 *  equal to the number of attributes defined for this class.
 *  Passing too many or too less parameters will raise an
 *  <code>ArgumentError</code>.
 *
 *  The remaining methods listed in this section (class and instance)
 *  are defined for this generated class.
 *
 *     # Create a structure named by its constant
 *     Customer = Data.define(:name, :address)  #=> Customer
 *     Customer.new("Dave", "123 Main")         #=> #<data name="Dave", address="123 Main">
 */
static mrb_value
mrb_data_s_def(mrb_state *mrb, mrb_value klass)
{
  mrb_value rest;
  mrb_value b, data;
  mrb_sym id;
  const mrb_value *argv;
  mrb_int argc;

  mrb_get_args(mrb, "*&", &argv, &argc, &b);
  rest = mrb_ary_new_from_values(mrb, argc, argv);
  for (mrb_int i=0; i<argc; i++) {
    id = mrb_obj_to_sym(mrb, RARRAY_PTR(rest)[i]);
    mrb_ary_set(mrb, rest, i, mrb_symbol_value(id));
  }
  /* check member duplication */
  mrb_int len = RARRAY_LEN(rest);
  mrb_value *p = RARRAY_PTR(rest);
  for (mrb_int i=0; i<len; i++) {
    mrb_sym sym = mrb_symbol(p[i]);
    for (mrb_int j=i+1; j<len; j++) {
      if (sym == mrb_symbol(p[j])) {
        mrb_raisef(mrb, E_ARGUMENT_ERROR, "duplicate member: %n", sym);
      }
    }
  }
  data = make_data_class(mrb, rest, mrb_class_ptr(klass));
  if (!mrb_nil_p(b)) {
    mrb_yield_with_class(mrb, b, 1, &data, data, mrb_class_ptr(data));
  }
  return data;
}

static mrb_value
mrb_data_initialize(mrb_state *mrb, mrb_value self)
{
  mrb_value members = data_members(mrb, self);

  mrb_int n = RARRAY_LEN(members);
  mrb_value hash;
  mrb_get_args(mrb, "H", &hash);
  if (mrb_hash_size(mrb, hash) != n) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "wrong number of arguments");
  }
  mrb_ary_resize(mrb, self, n);

  mrb_value *mems = RARRAY_PTR(members);
  for (mrb_int i = 0; i < n; i++) {
    if (!mrb_hash_key_p(mrb, hash, mems[i])) {
      mrb_raisef(mrb, E_ARGUMENT_ERROR, "undefined data member %v", mems[i]);
    }
    mrb_ary_set(mrb, self, i, mrb_hash_get(mrb, hash, mems[i]));
  }
  mrb_obj_freeze(mrb, self);
  return self;
}

static mrb_value
mrb_data_init_copy(mrb_state *mrb, mrb_value copy)
{
  mrb_value s = mrb_get_arg1(mrb);

  if (mrb_obj_equal(mrb, copy, s)) return copy;
  if (!mrb_obj_is_instance_of(mrb, s, mrb_obj_class(mrb, copy))) {
    mrb_raise(mrb, E_TYPE_ERROR, "wrong argument class");
  }
  if (!data_p(s)) {
    data_corrupted(mrb);
  }
  mrb_ary_replace(mrb, copy, s);
  mrb_obj_freeze(mrb, copy);
  return copy;
}

/*
 *  call-seq:
 *     data == other_data     -> true or false
 *
 *  Equality---Returns <code>true</code> if <i>other_data</i> is
 *  equal to this one: they must be of the same class as generated by
 *  <code>Data::define</code>, and all values of must be equal
 *  (according to <code>Object#==</code>).
 *
 *     Customer = Data.define(:name, :address, :zip)
 *     joe  = Customer.new("Joe Smith", "123 Maple, Anytown NC", 12345)
 *     joe2 = Customer.new("Joe Smith", "123 Maple, Anytown NC", 12345)
 *     jane = Customer.new("Jane Doe", "456 Elm, Anytown NC", 12345)
 *     joe == joe2   #=> true
 *     joe == jane   #=> false
 */

static mrb_value
mrb_data_equal(mrb_state *mrb, mrb_value s)
{
  mrb_value s2 = mrb_get_arg1(mrb);
  mrb_value *ptr, *ptr2;
  mrb_int len;

  if (mrb_obj_equal(mrb, s, s2)) {
    return mrb_true_value();
  }
  if (mrb_obj_class(mrb, s) != mrb_obj_class(mrb, s2)) {
    return mrb_false_value();
  }
  if (RDATA_LEN(s) != RDATA_LEN(s2)) {
    return mrb_false_value();
  }
  ptr = RDATA_PTR(s);
  ptr2 = RDATA_PTR(s2);
  len = RDATA_LEN(s);
  for (mrb_int i=0; i<len; i++) {
    if (!mrb_equal(mrb, ptr[i], ptr2[i])) {
      return mrb_false_value();
    }
  }

  return mrb_true_value();
}

/*
 * call-seq:
 *   data.eql?(other)   -> true or false
 *
 * Two structures are equal if they are the same object, or if all their
 * fields are equal (using <code>Object#eql?</code>).
 */
static mrb_value
mrb_data_eql(mrb_state *mrb, mrb_value s)
{
  mrb_value s2 = mrb_get_arg1(mrb);
  mrb_value *ptr, *ptr2;
  mrb_int len;

  if (mrb_obj_equal(mrb, s, s2)) {
    return mrb_true_value();
  }
  if (mrb_obj_class(mrb, s) != mrb_obj_class(mrb, s2)) {
    return mrb_false_value();
  }
  if (RDATA_LEN(s) != RDATA_LEN(s2)) {
    return mrb_false_value();
  }
  ptr = RDATA_PTR(s);
  ptr2 = RDATA_PTR(s2);
  len = RDATA_LEN(s);
  for (mrb_int i=0; i<len; i++) {
    if (!mrb_eql(mrb, ptr[i], ptr2[i])) {
      return mrb_false_value();
    }
  }

  return mrb_true_value();
}

/*
 * call-seq:
 *    data.to_h -> hash
 *
 * Create a hash from member names and values.
 */
static mrb_value
mrb_data_to_h(mrb_state *mrb, mrb_value self)
{
  mrb_value members, ret;
  mrb_value *mems;

  members = data_members(mrb, self);
  mems = RARRAY_PTR(members);

  ret = mrb_hash_new_capa(mrb, RARRAY_LEN(members));
  mrb_int len = RARRAY_LEN(members);
  for (mrb_int i=0; i<len; i++) {
    mrb_hash_set(mrb, ret, mems[i], RARRAY_PTR(self)[i]);
  }

  return ret;
}

/*
 * call-seq:
 *    data.to_s    -> string
 *    data.inspect -> string
 *
 * Returns a string representation of Data
 */
static mrb_value
mrb_data_to_s(mrb_state *mrb, mrb_value self)
{
  mrb_value members, ret, cname;
  mrb_value *mems;
  mrb_int mlen;

  members = data_members(mrb, self);
  mlen = RARRAY_LEN(members);
  mems = RARRAY_PTR(members);
  ret = mrb_str_new_lit(mrb, "#<data ");
  int ai = mrb_gc_arena_save(mrb);
  cname = mrb_class_path(mrb, mrb_class_real(mrb_class(mrb, self)));
  if (!mrb_nil_p(cname)) {
    mrb_str_cat_str(mrb, ret, cname);
    mrb_str_cat_lit(mrb, ret, " ");
  }
  for (mrb_int i=0; i<mlen; i++) {
    mrb_int len;
    const char *name = mrb_sym_name_len(mrb, mrb_symbol(mems[i]), &len);
    if (i>0) mrb_str_cat_lit(mrb, ret, ", ");
    mrb_str_cat(mrb, ret, name, len);
    mrb_str_cat_lit(mrb, ret, "=");
    mrb_str_cat_str(mrb, ret, mrb_inspect(mrb, RARRAY_PTR(self)[i]));
    mrb_gc_arena_restore(mrb, ai);
  }
  mrb_str_cat_lit(mrb, ret, ">");

  return ret;
}

/*
 *  A <code>Data</code> is a convenient way to bundle a number of
 *  attributes together, using accessor methods, without having to write
 *  an explicit class.
 *
 *  The <code>Data</code> class is a generator of specific classes,
 *  each one of which is defined to hold a set of variables and their
 *  accessors. In these examples, we'll call the generated class
 *  "<i>Customer</i>Class," and we'll show an example instance of that
 *  class as "<i>Customer</i>Inst."
 *
 *  In the descriptions that follow, the parameter <i>symbol</i> refers
 *  to a symbol (such as <code>:name</code>).
 */
void
mrb_mruby_data_gem_init(mrb_state* mrb)
{
  struct RClass *d;
  d = mrb_define_class(mrb, "Data",  mrb->object_class);
  MRB_SET_INSTANCE_TT(d, MRB_TT_STRUCT);
  MRB_UNDEF_ALLOCATOR(d);

  mrb_undef_class_method(mrb, d, "new");
  mrb_define_class_method(mrb, d, "define",          mrb_data_s_def,      MRB_ARGS_ANY());

  mrb_define_method(mrb, d,       "==",              mrb_data_equal,      MRB_ARGS_REQ(1));
  mrb_define_method(mrb, d,       "members",         mrb_data_members,    MRB_ARGS_NONE());
  mrb_define_method(mrb, d,       "initialize",      mrb_data_initialize, MRB_ARGS_ANY());
  mrb_define_method(mrb, d,       "initialize_copy", mrb_data_init_copy,  MRB_ARGS_ANY());
  mrb_define_method(mrb, d,       "eql?",            mrb_data_eql,        MRB_ARGS_REQ(1));

  mrb_define_method(mrb, d,       "to_h",            mrb_data_to_h,       MRB_ARGS_NONE());
  mrb_define_method(mrb, d,       "to_s",            mrb_data_to_s,       MRB_ARGS_NONE());
  mrb_define_method(mrb, d,       "inspect",         mrb_data_to_s,       MRB_ARGS_NONE());
}

void
mrb_mruby_data_gem_final(mrb_state* mrb)
{
}

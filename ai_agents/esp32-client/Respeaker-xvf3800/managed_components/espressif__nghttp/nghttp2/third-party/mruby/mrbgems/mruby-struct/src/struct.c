/*
** struct.c - Struct class
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
#include <mruby/range.h>
#include <mruby/proc.h>
#include <mruby/internal.h>
#include <mruby/presym.h>

#define RSTRUCT_LEN(st) RARRAY_LEN(st)
#define RSTRUCT_PTR(st) RARRAY_PTR(st)

#define mrb_struct_p(o) (mrb_type(o) == MRB_TT_STRUCT)

static struct RClass *
struct_class(mrb_state *mrb)
{
  return mrb_class_get_id(mrb, MRB_SYM(Struct));
}

static void
struct_corrupted(mrb_state *mrb)
{
  mrb_raise(mrb, E_TYPE_ERROR, "corrupted struct");
}

static mrb_value
struct_s_members(mrb_state *mrb, struct RClass *c)
{
  struct RClass* sclass = struct_class(mrb);
  mrb_value members;

  for (;;) {
    members = mrb_iv_get(mrb, mrb_obj_value(c), MRB_SYM(__members__));
    if (!mrb_nil_p(members)) {
      if (!mrb_array_p(members)) {
        struct_corrupted(mrb);
      }
      return members;
    }
    c = c->super;
    if (c == sclass || c == 0) {
      mrb_raise(mrb, E_TYPE_ERROR, "uninitialized struct");
    }
  }
}

static mrb_value
struct_members(mrb_state *mrb, mrb_value s)
{
  if (!mrb_struct_p(s)) {
    struct_corrupted(mrb);
  }
  mrb_value members = struct_s_members(mrb, mrb_obj_class(mrb, s));
  mrb_int len = RSTRUCT_LEN(s);
  mrb_int mlen = RARRAY_LEN(members);
  if (len > 0 && len != mlen) {
    mrb_raisef(mrb, E_TYPE_ERROR,
               "struct size differs (%i required %i given)",
               mlen, len);
  }
  return members;
}

static mrb_value
mrb_struct_s_members_m(mrb_state *mrb, mrb_value klass)
{
  mrb_value members, ary;

  members = struct_s_members(mrb, mrb_class_ptr(klass));
  ary = mrb_ary_new_capa(mrb, RARRAY_LEN(members));
  mrb_ary_replace(mrb, ary, members);
  return ary;
}

#define mrb_struct_modify(mrb,s) mrb_check_frozen((mrb), mrb_basic_ptr(s))

/* 15.2.18.4.6  */
/*
 *  call-seq:
 *     struct.members    -> array
 *
 *  Returns an array of strings representing the names of the instance
 *  variables.
 *
 *     Customer = Struct.new(:name, :address, :zip)
 *     joe = Customer.new("Joe Smith", "123 Maple, Anytown NC", 12345)
 *     joe.members   #=> [:name, :address, :zip]
 */

static mrb_value
mrb_struct_members(mrb_state *mrb, mrb_value obj)
{
  return mrb_struct_s_members_m(mrb, mrb_obj_value(mrb_obj_class(mrb, obj)));
}

static mrb_int
num_members(mrb_state *mrb, mrb_value self)
{
  mrb_value members = struct_members(mrb, self);
  return RARRAY_LEN(members);
}

static mrb_value
mrb_struct_ref(mrb_state *mrb, mrb_value obj)
{
  mrb_int argc = mrb_get_argc(mrb);
  if (argc != 0) {
    mrb_argnum_error(mrb, argc, 0, 0);
  }
  mrb_int i = mrb_integer(mrb_proc_cfunc_env_get(mrb, 0));
  mrb_int len = num_members(mrb, obj);
  mrb_value *ptr = RSTRUCT_PTR(obj);

  if (!ptr || len <= i) return mrb_nil_value();
  return ptr[i];
}

static mrb_sym
mrb_id_attrset(mrb_state *mrb, mrb_sym id)
{
#define ONSTACK_ALLOC_MAX 32
#define ONSTACK_STRLEN_MAX (ONSTACK_ALLOC_MAX - 1) /* '=' character */

  const char *name;
  char *buf;
  mrb_int len;
  mrb_sym mid;
  char onstack[ONSTACK_ALLOC_MAX];

  name = mrb_sym_name_len(mrb, id, &len);
  if (len > ONSTACK_STRLEN_MAX) {
    buf = (char*)mrb_malloc(mrb, (size_t)len+1);
  }
  else {
    buf = onstack;
  }
  memcpy(buf, name, (size_t)len);
  buf[len] = '=';

  mid = mrb_intern(mrb, buf, len+1);
  if (buf != onstack) {
    mrb_free(mrb, buf);
  }
  return mid;
}

static mrb_value
mrb_struct_set_m(mrb_state *mrb, mrb_value obj)
{
  mrb_int i = mrb_integer(mrb_proc_cfunc_env_get(mrb, 0));
  mrb_value val = mrb_get_arg1(mrb);

  mrb_ary_set(mrb, obj, i, val);
  return val;
}

static void
make_struct_define_accessors(mrb_state *mrb, mrb_value members, struct RClass *c)
{
  const mrb_value *ptr_members = RARRAY_PTR(members);
  mrb_int i;
  mrb_int len = RARRAY_LEN(members);
  int ai = mrb_gc_arena_save(mrb);

  for (i=0; i<len; i++) {
    mrb_sym id = mrb_symbol(ptr_members[i]);
    mrb_method_t m;
    mrb_value at = mrb_fixnum_value(i);
    struct RProc *aref = mrb_proc_new_cfunc_with_env(mrb, mrb_struct_ref, 1, &at);
    struct RProc *aset = mrb_proc_new_cfunc_with_env(mrb, mrb_struct_set_m, 1, &at);
    MRB_METHOD_FROM_PROC(m, aref);
    mrb_define_method_raw(mrb, c, id, m);
    MRB_METHOD_FROM_PROC(m, aset);
    mrb_define_method_raw(mrb, c, mrb_id_attrset(mrb, id), m);
    mrb_gc_arena_restore(mrb, ai);
  }
}

static mrb_value
make_struct(mrb_state *mrb, mrb_value name, mrb_value members, struct RClass *klass)
{
  mrb_value nstr;
  mrb_sym id;
  struct RClass *c;

  if (mrb_nil_p(name)) {
    c = mrb_class_new(mrb, klass);
  }
  else {
    /* old style: should we warn? */
    mrb_ensure_string_type(mrb, name);
    id = mrb_obj_to_sym(mrb, name);
    if (!mrb_const_name_p(mrb, RSTRING_PTR(name), RSTRING_LEN(name))) {
      mrb_name_error(mrb, id, "identifier %v needs to be constant", name);
    }
    if (mrb_const_defined_at(mrb, mrb_obj_value(klass), id)) {
      mrb_warn(mrb, "redefining constant Struct::%v", name);
      mrb_const_remove(mrb, mrb_obj_value(klass), id);
    }
    c = mrb_define_class_under_id(mrb, klass, id, klass);
  }
  MRB_SET_INSTANCE_TT(c, MRB_TT_STRUCT);
  MRB_DEFINE_ALLOCATOR(c);
  nstr = mrb_obj_value(c);
  mrb_iv_set(mrb, nstr, MRB_SYM(__members__), members);

  mrb_define_class_method_id(mrb, c, MRB_SYM(new), mrb_instance_new, MRB_ARGS_ANY());
  mrb_define_class_method_id(mrb, c, MRB_OPSYM(aref), mrb_instance_new, MRB_ARGS_ANY());
  mrb_define_class_method_id(mrb, c, MRB_SYM(members), mrb_struct_s_members_m, MRB_ARGS_NONE());
  /* RSTRUCT(nstr)->basic.c->super = c->c; */
  make_struct_define_accessors(mrb, members, c);
  return nstr;
}

/* 15.2.18.3.1  */
/*
 *  call-seq:
 *     Struct.new( [aString] [, aSym]+> )    -> StructClass
 *     StructClass.new(arg, ...)             -> obj
 *     StructClass[arg, ...]                 -> obj
 *
 *  Creates a new class, named by <i>aString</i>, containing accessor
 *  methods for the given symbols. If the name <i>aString</i> is
 *  omitted, an anonymous structure class will be created. Otherwise,
 *  the name of this struct will appear as a constant in class
 *  <code>Struct</code>, so it must be unique for all
 *  <code>Struct</code>s in the system and should start with a capital
 *  letter. Assigning a structure class to a constant effectively gives
 *  the class the name of the constant.
 *
 *  <code>Struct::new</code> returns a new <code>Class</code> object,
 *  which can then be used to create specific instances of the new
 *  structure. The number of actual parameters must be
 *  less than or equal to the number of attributes defined for this
 *  class; unset parameters default to <code>nil</code>.  Passing too many
 *  parameters will raise an <code>ArgumentError</code>.
 *
 *  The remaining methods listed in this section (class and instance)
 *  are defined for this generated class.
 *
 *     # Create a structure with a name in Struct
 *     Struct.new("Customer", :name, :address)    #=> Struct::Customer
 *     Struct::Customer.new("Dave", "123 Main")   #=> #<struct Struct::Customer name="Dave", address="123 Main">
 *
 *     # Create a structure named by its constant
 *     Customer = Struct.new(:name, :address)     #=> Customer
 *     Customer.new("Dave", "123 Main")           #=> #<struct Customer name="Dave", address="123 Main">
 */
static mrb_value
mrb_struct_s_def(mrb_state *mrb, mrb_value klass)
{
  mrb_value name = mrb_nil_value();
  const mrb_value *pargv;
  mrb_int argcnt;
  mrb_value b, st;
  const mrb_value *argv;
  mrb_int argc;

  mrb_get_args(mrb, "*&", &argv, &argc, &b);
  if (argc == 0) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "wrong number of arguments (given 0, expected 1+)");
  }
  pargv = argv;
  argcnt = argc;
  if (argc > 0 && !mrb_symbol_p(argv[0])) {
    /* 1stArgument:!symbol -> name=argv[0] rest=argv[0..n] */
    name = argv[0];
    pargv++;
    argcnt--;
  }
  mrb_value members = mrb_ary_new_from_values(mrb, argcnt, pargv);
  for (mrb_int i=0; i<argcnt; i++) {
    mrb_sym sym = mrb_obj_to_sym(mrb, RARRAY_PTR(members)[i]);
    mrb_ary_set(mrb, members, i, mrb_symbol_value(sym));
  }
  /* check member duplication */
  mrb_int len = RARRAY_LEN(members);
  mrb_value *p = RARRAY_PTR(members);
  for (mrb_int i=0; i<len; i++) {
    mrb_sym sym = mrb_symbol(p[i]);
    for (mrb_int j=i+1; j<len; j++) {
      if (sym == mrb_symbol(p[j])) {
        mrb_raisef(mrb, E_ARGUMENT_ERROR, "duplicate member: %n", sym);
      }
    }
  }
  st = make_struct(mrb, name, members, mrb_class_ptr(klass));
  if (!mrb_nil_p(b)) {
    mrb_yield_with_class(mrb, b, 1, &st, st, mrb_class_ptr(st));
  }
  return st;
}

/* 15.2.18.4.8  */
/*
 */
static mrb_value
mrb_struct_initialize_withArg(mrb_state *mrb, mrb_int argc, const mrb_value *argv, mrb_value self)
{
  mrb_int i, n;

  n = num_members(mrb, self);
  if (n < argc) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "struct size differs");
  }

  for (i = 0; i < argc; i++) {
    mrb_ary_set(mrb, self, i, argv[i]);
  }
  for (i = argc; i < n; i++) {
    mrb_ary_set(mrb, self, i, mrb_nil_value());
  }
  return self;
}

static mrb_value
mrb_struct_initialize(mrb_state *mrb, mrb_value self)
{
  const mrb_value *argv;
  mrb_int argc;

  mrb_get_args(mrb, "*", &argv, &argc);
  return mrb_struct_initialize_withArg(mrb, argc, argv, self);
}

/* 15.2.18.4.9  */
/* :nodoc: */
static mrb_value
mrb_struct_init_copy(mrb_state *mrb, mrb_value copy)
{
  mrb_value s = mrb_get_arg1(mrb);

  if (mrb_obj_equal(mrb, copy, s)) return copy;
  if (!mrb_obj_is_instance_of(mrb, s, mrb_obj_class(mrb, copy))) {
    mrb_raise(mrb, E_TYPE_ERROR, "wrong argument class");
  }
  if (!mrb_struct_p(s)) {
    struct_corrupted(mrb);
  }
  mrb_ary_replace(mrb, copy, s);
  return copy;
}

static mrb_value
struct_aref_sym(mrb_state *mrb, mrb_value obj, mrb_sym id)
{
  mrb_value members, *ptr;
  const mrb_value *ptr_members;
  mrb_int i, len, plen;

  members = struct_members(mrb, obj);
  ptr_members = RARRAY_PTR(members);
  len = RARRAY_LEN(members);
  ptr = RSTRUCT_PTR(obj);
  plen = RARRAY_LEN(obj);
  for (i=0; i<len; i++) {
    mrb_value slot = ptr_members[i];
    if (mrb_symbol_p(slot) && mrb_symbol(slot) == id) {
      if (i < plen) return ptr[i];
      return mrb_nil_value();
    }
  }
  mrb_name_error(mrb, id, "no member '%n' in struct", id);
  return mrb_nil_value();       /* not reached */
}

static mrb_int
struct_index(mrb_state *mrb, mrb_int i, mrb_int len)
{
  mrb_int idx = i < 0 ? len + i : i;

  if (idx < 0)
    mrb_raisef(mrb, E_INDEX_ERROR,
               "offset %i too small for struct(size:%i)", i, len);
  if (len <= idx)
    mrb_raisef(mrb, E_INDEX_ERROR,
               "offset %i too large for struct(size:%i)", i, len);
  return idx;
}

static mrb_value
struct_aref_int(mrb_state *mrb, mrb_value s, mrb_int i)
{
  mrb_int idx = struct_index(mrb, i, num_members(mrb, s));
  if (idx >= RSTRUCT_LEN(s)) return mrb_nil_value();
  return RSTRUCT_PTR(s)[idx];
}

/* 15.2.18.4.2  */
/*
 *  call-seq:
 *     struct[symbol]    -> anObject
 *     struct[fixnum]    -> anObject
 *
 *  Attribute Reference---Returns the value of the instance variable
 *  named by <i>symbol</i>, or indexed (0..length-1) by
 *  <i>fixnum</i>. Will raise <code>NameError</code> if the named
 *  variable does not exist, or <code>IndexError</code> if the index is
 *  out of range.
 *
 *     Customer = Struct.new(:name, :address, :zip)
 *     joe = Customer.new("Joe Smith", "123 Maple, Anytown NC", 12345)
 *
 *     joe["name"]   #=> "Joe Smith"
 *     joe[:name]    #=> "Joe Smith"
 *     joe[0]        #=> "Joe Smith"
 */
static mrb_value
mrb_struct_aref(mrb_state *mrb, mrb_value s)
{
  mrb_value idx = mrb_get_arg1(mrb);

  if (mrb_string_p(idx)) {
    mrb_sym sym = mrb_intern_str(mrb, idx);
    idx = mrb_symbol_value(sym);
  }
  if (mrb_symbol_p(idx)) {
    return struct_aref_sym(mrb, s, mrb_symbol(idx));
  }
  return struct_aref_int(mrb, s, mrb_as_int(mrb, idx));
}

static mrb_value
mrb_struct_aset_sym(mrb_state *mrb, mrb_value s, mrb_sym id, mrb_value val)
{
  mrb_value members;
  const mrb_value *ptr_members;
  mrb_int i, len;

  members = struct_members(mrb, s);
  len = RARRAY_LEN(members);
  ptr_members = RARRAY_PTR(members);
  for (i=0; i<len; i++) {
    if (mrb_symbol(ptr_members[i]) == id) {
      mrb_ary_set(mrb, s, i, val);
      return val;
    }
  }
  mrb_name_error(mrb, id, "no member '%n' in struct", id);
  return val;                   /* not reach */
}

/* 15.2.18.4.3  */
/*
 *  call-seq:
 *     struct[symbol] = obj    -> obj
 *     struct[fixnum] = obj    -> obj
 *
 *  Attribute Assignment---Assigns to the instance variable named by
 *  <i>symbol</i> or <i>fixnum</i> the value <i>obj</i> and
 *  returns it. Will raise a <code>NameError</code> if the named
 *  variable does not exist, or an <code>IndexError</code> if the index
 *  is out of range.
 *
 *     Customer = Struct.new(:name, :address, :zip)
 *     joe = Customer.new("Joe Smith", "123 Maple, Anytown NC", 12345)
 *
 *     joe["name"] = "Luke"
 *     joe[:zip]   = "90210"
 *
 *     joe.name   #=> "Luke"
 *     joe.zip    #=> "90210"
 */

static mrb_value
mrb_struct_aset(mrb_state *mrb, mrb_value s)
{
  mrb_int i;
  mrb_value idx;
  mrb_value val;

  mrb_get_args(mrb, "oo", &idx, &val);

  if (mrb_string_p(idx)) {
    mrb_sym sym = mrb_intern_str(mrb, idx);
    idx = mrb_symbol_value(sym);
  }
  if (mrb_symbol_p(idx)) {
    return mrb_struct_aset_sym(mrb, s, mrb_symbol(idx), val);
  }

  i = struct_index(mrb, mrb_as_int(mrb, idx), num_members(mrb, s));
  mrb_ary_set(mrb, s, i, val);
  return val;
}

/* 15.2.18.4.1  */
/*
 *  call-seq:
 *     struct == other_struct     -> true or false
 *
 *  Equality---Returns <code>true</code> if <i>other_struct</i> is
 *  equal to this one: they must be of the same class as generated by
 *  <code>Struct::new</code>, and the values of all instance variables
 *  must be equal (according to <code>Object#==</code>).
 *
 *     Customer = Struct.new(:name, :address, :zip)
 *     joe   = Customer.new("Joe Smith", "123 Maple, Anytown NC", 12345)
 *     joejr = Customer.new("Joe Smith", "123 Maple, Anytown NC", 12345)
 *     jane  = Customer.new("Jane Doe", "456 Elm, Anytown NC", 12345)
 *     joe == joejr   #=> true
 *     joe == jane    #=> false
 */

static mrb_value
mrb_struct_equal(mrb_state *mrb, mrb_value s)
{
  mrb_value s2 = mrb_get_arg1(mrb);
  mrb_value *ptr, *ptr2;
  mrb_int i, len;

  if (mrb_obj_equal(mrb, s, s2)) {
    return mrb_true_value();
  }
  if (mrb_obj_class(mrb, s) != mrb_obj_class(mrb, s2)) {
    return mrb_false_value();
  }
  if (RSTRUCT_LEN(s) != RSTRUCT_LEN(s2)) {
    return mrb_false_value();
  }
  ptr = RSTRUCT_PTR(s);
  ptr2 = RSTRUCT_PTR(s2);
  len = RSTRUCT_LEN(s);
  for (i=0; i<len; i++) {
    if (!mrb_equal(mrb, ptr[i], ptr2[i])) {
      return mrb_false_value();
    }
  }

  return mrb_true_value();
}

/* 15.2.18.4.12(x)  */
/*
 * call-seq:
 *   struct.eql?(other)   -> true or false
 *
 * Two structures are equal if they are the same object, or if all their
 * fields are equal (using <code>eql?</code>).
 */
static mrb_value
mrb_struct_eql(mrb_state *mrb, mrb_value s)
{
  mrb_value s2 = mrb_get_arg1(mrb);
  mrb_value *ptr, *ptr2;
  mrb_int i, len;

  if (mrb_obj_equal(mrb, s, s2)) {
    return mrb_true_value();
  }
  if (mrb_obj_class(mrb, s) != mrb_obj_class(mrb, s2)) {
    return mrb_false_value();
  }
  if (RSTRUCT_LEN(s) != RSTRUCT_LEN(s2)) {
    return mrb_false_value();
  }
  ptr = RSTRUCT_PTR(s);
  ptr2 = RSTRUCT_PTR(s2);
  len = RSTRUCT_LEN(s);
  for (i=0; i<len; i++) {
    if (!mrb_eql(mrb, ptr[i], ptr2[i])) {
      return mrb_false_value();
    }
  }

  return mrb_true_value();
}

/*
 * call-seq:
 *    struct.length   -> Integer
 *    struct.size     -> Integer
 *
 * Returns number of struct members.
 */
static mrb_value
mrb_struct_len(mrb_state *mrb, mrb_value self)
{
  return mrb_fixnum_value(RSTRUCT_LEN(self));
}

/*
 * call-seq:
 *    struct.to_a    -> array
 *    struct.values  -> array
 *
 * Create an array from struct values.
 */
static mrb_value
mrb_struct_to_a(mrb_state *mrb, mrb_value self)
{
  return mrb_ary_new_from_values(mrb, RSTRUCT_LEN(self), RSTRUCT_PTR(self));
}

/*
 * call-seq:
 *    struct.to_h -> hash
 *
 * Create a hash from member names and struct values.
 */
static mrb_value
mrb_struct_to_h(mrb_state *mrb, mrb_value self)
{
  mrb_value members, ret;
  mrb_int i;

  members = struct_members(mrb, self);
  ret = mrb_hash_new_capa(mrb, RARRAY_LEN(members));

  for (i = 0; i < RARRAY_LEN(members); i++) {
    mrb_hash_set(mrb, ret, RARRAY_PTR(members)[i], mrb_ary_ref(mrb, self, i));
  }

  return ret;
}

static mrb_value
mrb_struct_values_at(mrb_state *mrb, mrb_value self)
{
  mrb_int argc;
  const mrb_value *argv;

  mrb_get_args(mrb, "*", &argv, &argc);

  return mrb_get_values_at(mrb, self, RSTRUCT_LEN(self), argc, argv, struct_aref_int);
}

/*
 * call-seq:
 *    struct.to_s    -> string
 *    struct.inspect -> string
 *
 * Returns a string representation of Data
 */
static mrb_value
mrb_struct_to_s(mrb_state *mrb, mrb_value self)
{
  mrb_value members, ret, cname;
  mrb_value *mems;
  mrb_int mlen;

  mrb->c->ci->mid = MRB_SYM(inspect);
  ret = mrb_str_new_lit(mrb, "#<struct ");
  int ai = mrb_gc_arena_save(mrb);
  cname = mrb_class_path(mrb, mrb_class_real(mrb_class(mrb, self)));
  if (!mrb_nil_p(cname)) {
    mrb_str_cat_str(mrb, ret, cname);
    mrb_str_cat_lit(mrb, ret, " ");
  }
  if (mrb_inspect_recursive_p(mrb, self)) {
    mrb_str_cat_lit(mrb, ret, "...>");
    return ret;
  }
  members = struct_members(mrb, self);
  mlen = RARRAY_LEN(members);
  mems = RARRAY_PTR(members);
  for (mrb_int i=0; i<mlen; i++) {
    mrb_int len;
    const char *name = mrb_sym_name_len(mrb, mrb_symbol(mems[i]), &len);
    if (i>0) mrb_str_cat_lit(mrb, ret, ", ");
    mrb_str_cat(mrb, ret, name, len);
    mrb_str_cat_lit(mrb, ret, "=");
    mrb_str_cat_str(mrb, ret, mrb_inspect(mrb, mrb_ary_ref(mrb, self, i)));
    mrb_gc_arena_restore(mrb, ai);
  }
  mrb_str_cat_lit(mrb, ret, ">");

  return ret;
}

/*
 *  A <code>Struct</code> is a convenient way to bundle a number of
 *  attributes together, using accessor methods, without having to write
 *  an explicit class.
 *
 *  The <code>Struct</code> class is a generator of specific classes,
 *  each one of which is defined to hold a set of variables and their
 *  accessors. In these examples, we'll call the generated class
 *  "<i>Customer</i>Class," and we'll show an example instance of that
 *  class as "<i>Customer</i>Inst."
 *
 *  In the descriptions that follow, the parameter <i>symbol</i> refers
 *  to a symbol, which is either a quoted string or a
 *  <code>Symbol</code> (such as <code>:name</code>).
 */
void
mrb_mruby_struct_gem_init(mrb_state* mrb)
{
  struct RClass *st;
  st = mrb_define_class(mrb, "Struct",  mrb->object_class);
  MRB_SET_INSTANCE_TT(st, MRB_TT_STRUCT);
  MRB_UNDEF_ALLOCATOR(st);

  mrb_define_class_method(mrb, st, "new",             mrb_struct_s_def,       MRB_ARGS_ANY());  /* 15.2.18.3.1  */

  mrb_define_method(mrb, st,       "==",              mrb_struct_equal,       MRB_ARGS_REQ(1)); /* 15.2.18.4.1  */
  mrb_define_method(mrb, st,       "[]",              mrb_struct_aref,        MRB_ARGS_REQ(1)); /* 15.2.18.4.2  */
  mrb_define_method(mrb, st,       "[]=",             mrb_struct_aset,        MRB_ARGS_REQ(2)); /* 15.2.18.4.3  */
  mrb_define_method(mrb, st,       "members",         mrb_struct_members,     MRB_ARGS_NONE()); /* 15.2.18.4.6  */
  mrb_define_method(mrb, st,       "initialize",      mrb_struct_initialize,  MRB_ARGS_ANY());  /* 15.2.18.4.8  */
  mrb_define_method(mrb, st,       "initialize_copy", mrb_struct_init_copy,   MRB_ARGS_REQ(1)); /* 15.2.18.4.9  */
  mrb_define_method(mrb, st,       "eql?",            mrb_struct_eql,         MRB_ARGS_REQ(1)); /* 15.2.18.4.12(x)  */
  mrb_define_method(mrb, st,       "to_s",            mrb_struct_to_s,        MRB_ARGS_NONE()); /* 15.2.18.4.11(x) */
  mrb_define_method(mrb, st,       "inspect",         mrb_struct_to_s,        MRB_ARGS_NONE()); /* 15.2.18.4.10(x) */

  mrb_define_method(mrb, st,       "size",            mrb_struct_len,         MRB_ARGS_NONE());
  mrb_define_method(mrb, st,       "length",          mrb_struct_len,         MRB_ARGS_NONE());
  mrb_define_method(mrb, st,       "to_a",            mrb_struct_to_a,        MRB_ARGS_NONE());
  mrb_define_method(mrb, st,       "values",          mrb_struct_to_a,        MRB_ARGS_NONE());
  mrb_define_method(mrb, st,       "to_h",            mrb_struct_to_h,        MRB_ARGS_NONE());
  mrb_define_method(mrb, st,       "values_at",       mrb_struct_values_at,   MRB_ARGS_ANY());
}

void
mrb_mruby_struct_gem_final(mrb_state* mrb)
{
}

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/class.h>
#include <mruby/hash.h>
#include <mruby/proc.h>
#include <mruby/variable.h>
#include <mruby/presym.h>
#include <mruby/opcode.h>
#include <mruby/debug.h>
#include <mruby/internal.h>

#define BINDING_UPPER_DEFAULT  20
#define BINDING_UPPER_MINIMUM  10
#define BINDING_UPPER_MAXIMUM 100

#ifndef MRB_BINDING_UPPER_MAX
# define BINDING_UPPER_MAX BINDING_UPPER_DEFAULT
#else
# if (MRB_BINDING_UPPER_MAX) > BINDING_UPPER_MAXIMUM
#  define BINDING_UPPER_MAX BINDING_UPPER_MAXIMUM
# elif (MRB_BINDING_UPPER_MAX) < BINDING_UPPER_MINIMUM
#  define BINDING_UPPER_MAX BINDING_UPPER_MINIMUM
# else
#  define BINDING_UPPER_MAX MRB_BINDING_UPPER_MAX
# endif
#endif

static mrb_int
binding_extract_pc(mrb_state *mrb, mrb_value binding)
{
  mrb_value obj = mrb_iv_get(mrb, binding, MRB_SYM(pc));
  if (mrb_nil_p(obj)) {
    return -1;
  }
  else {
    mrb_check_type(mrb, obj, MRB_TT_INTEGER);
    return mrb_int(mrb, obj);
  }
}

const struct RProc *
mrb_binding_extract_proc(mrb_state *mrb, mrb_value binding)
{
  mrb_value obj = mrb_iv_get(mrb, binding, MRB_SYM(proc));
  mrb_check_type(mrb, obj, MRB_TT_PROC);
  return mrb_proc_ptr(obj);
}

struct REnv *
mrb_binding_extract_env(mrb_state *mrb, mrb_value binding)
{
  mrb_value obj = mrb_iv_get(mrb, binding, MRB_SYM(env));
  if (mrb_nil_p(obj)) {
    return NULL;
  }
  else {
    mrb_check_type(mrb, obj, MRB_TT_ENV);
    return (struct REnv*)mrb_obj_ptr(obj);
  }
}

static mrb_irep *
binding_irep_new_lvspace(mrb_state *mrb)
{
  static const mrb_code iseq_dummy[] = { OP_RETURN, 0 };

  mrb_irep *irep = mrb_add_irep(mrb);
  irep->flags = MRB_ISEQ_NO_FREE;
  irep->iseq = iseq_dummy;
  irep->ilen = sizeof(iseq_dummy) / sizeof(iseq_dummy[0]);
  irep->lv = (mrb_sym*)mrb_calloc(mrb, 1, sizeof(mrb_sym)); /* initial allocation for dummy */
  irep->nlocals = 1;
  irep->nregs = 1;
  return irep;
}

static struct RProc *
binding_proc_new_lvspace(mrb_state *mrb, const struct RProc *upper, struct REnv *env)
{
  struct RProc *lvspace = MRB_OBJ_ALLOC(mrb, MRB_TT_PROC, mrb->proc_class);
  lvspace->body.irep = binding_irep_new_lvspace(mrb);
  lvspace->upper = upper;
  if (env && env->tt == MRB_TT_ENV) {
    lvspace->e.env = env;
    lvspace->flags |= MRB_PROC_ENVSET;
  }
  return lvspace;
}

static struct REnv *
binding_env_new_lvspace(mrb_state *mrb, const struct REnv *e)
{
  struct REnv *env = MRB_OBJ_ALLOC(mrb, MRB_TT_ENV, NULL);
  mrb_value *stacks = (mrb_value*)mrb_calloc(mrb, 1, sizeof(mrb_value));
  env->cxt = e ? e->cxt : mrb->c;
  env->mid = 0;
  env->stack = stacks;
  if (e && e->stack && MRB_ENV_LEN(e) > 0) {
    env->stack[0] = e->stack[0];
  }
  else {
    env->stack[0] = mrb_nil_value();
  }
  env->flags = MRB_ENV_CLOSED;
  MRB_ENV_SET_LEN(env, 1);
  return env;
}

static void
binding_check_proc_upper_count(mrb_state *mrb, const struct RProc *proc)
{
  for (size_t count = 0; proc && !MRB_PROC_CFUNC_P(proc); proc = proc->upper) {
    count++;
    if (count > BINDING_UPPER_MAX) {
      mrb_raise(mrb, E_RUNTIME_ERROR,
                "too many upper procs for local variables (mruby limitation; maximum is " MRB_STRINGIZE(BINDING_UPPER_MAX) ")");
    }
    if (MRB_PROC_SCOPE_P(proc)) break;
  }
}

mrb_bool
mrb_binding_p(mrb_state *mrb, mrb_value obj)
{
  if (!mrb_obj_is_kind_of(mrb, obj, mrb_class_get_id(mrb, MRB_SYM(Binding)))) return FALSE;
  if (mrb_type(obj) != MRB_TT_OBJECT) return FALSE;
  if (!mrb_obj_iv_defined(mrb, mrb_obj_ptr(obj), MRB_SYM(proc))) return FALSE;
  if (!mrb_obj_iv_defined(mrb, mrb_obj_ptr(obj), MRB_SYM(recv))) return FALSE;
  if (!mrb_obj_iv_defined(mrb, mrb_obj_ptr(obj), MRB_SYM(env))) return FALSE;
  return TRUE;
}

static void
binding_type_ensure(mrb_state *mrb, mrb_value obj)
{
  if (mrb_binding_p(mrb, obj)) return;
  mrb_raise(mrb, E_TYPE_ERROR, "not a binding");
}

static struct RProc*
binding_wrap_lvspace(mrb_state *mrb, const struct RProc *proc, struct REnv **envp)
{
  /*
   * local variable space: It is a space to hold the top-level variable of
   * binding.eval and binding.local_variable_set.
   */

  struct RProc *lvspace = binding_proc_new_lvspace(mrb, proc, *envp);
  *envp = binding_env_new_lvspace(mrb, *envp);
  return lvspace;
}

static mrb_value
binding_initialize_copy(mrb_state *mrb, mrb_value binding)
{
  mrb_value src = mrb_get_arg1(mrb);
  binding_type_ensure(mrb, src);
  const struct RProc *src_proc = mrb_binding_extract_proc(mrb, src);
  struct REnv *src_env = mrb_binding_extract_env(mrb, src);

  mrb_check_frozen(mrb, mrb_obj_ptr(binding));

  struct RProc *lvspace;
  struct REnv *env;
  if (MRB_ENV_LEN(src_env) < 2) {
    /* when local variables of src are self only */
    env = src_proc->e.env;
    lvspace = binding_wrap_lvspace(mrb, src_proc->upper, &env);
  }
  else {
    binding_check_proc_upper_count(mrb, src_proc);

    env = src_env;
    lvspace = binding_wrap_lvspace(mrb, src_proc, &env);

    // The reason for using the mrb_obj_iv_set_force() function is to allow local
    // variables to be modified even if src is frozen. This behavior is CRuby imitation.
    src_proc = binding_wrap_lvspace(mrb, src_proc, &src_env);
    struct RObject *o = mrb_obj_ptr(src);
    mrb_obj_iv_set_force(mrb, o, MRB_SYM(proc), mrb_obj_value((struct RProc*)src_proc));
    mrb_obj_iv_set_force(mrb, o, MRB_SYM(env), mrb_obj_value(src_env));
  }
  mrb_iv_set(mrb, binding, MRB_SYM(proc), mrb_obj_value(lvspace));
  mrb_iv_set(mrb, binding, MRB_SYM(env), mrb_obj_value(env));

  return binding;
}

static void
binding_local_variable_name_check(mrb_state *mrb, mrb_sym id)
{
  if (id == 0) {
  badname:
    mrb_raisef(mrb, E_NAME_ERROR, "wrong local variable name %!n for binding", id);
  }

  mrb_int len;
  const char *name = mrb_sym_name_len(mrb, id, &len);
  if (len == 0) {
    goto badname;
  }

  if (ISASCII(*name) && !(*name == '_' || ISLOWER(*name))) {
    goto badname;
  }
  len--;
  name++;

  for (; len > 0; len--, name++) {
    if (ISASCII(*name) && !(*name == '_' || ISALNUM(*name))) {
      goto badname;
    }
  }
}

static mrb_value *
binding_local_variable_search(mrb_state *mrb, const struct RProc *proc, struct REnv *env, mrb_sym varname)
{
  binding_local_variable_name_check(mrb, varname);

  while (proc) {
    if (MRB_PROC_CFUNC_P(proc)) break;

    const mrb_irep *irep = proc->body.irep;
    const mrb_sym *lv;
    if (irep && (lv = irep->lv)) {
      for (int i = 0; i + 1 < irep->nlocals; i++, lv++) {
        if (varname == *lv) {
          return (env && MRB_ENV_LEN(env) > i) ? &env->stack[i + 1] : NULL;
        }
      }
    }

    if (MRB_PROC_SCOPE_P(proc)) break;
    env = MRB_PROC_ENV(proc);
    proc = proc->upper;
  }

  return NULL;
}

/*
 * call-seq:
 *  local_variable_defined?(symbol) -> bool
 */
static mrb_value
binding_local_variable_defined_p(mrb_state *mrb, mrb_value self)
{
  mrb_sym varname;
  mrb_get_args(mrb, "n", &varname);

  const struct RProc *proc = mrb_binding_extract_proc(mrb, self);
  struct REnv *env = mrb_binding_extract_env(mrb, self);
  mrb_value *e = binding_local_variable_search(mrb, proc, env, varname);
  if (e) {
    return mrb_true_value();
  }
  else {
    return mrb_false_value();
  }
}

/*
 * call-seq:
 *  local_variable_get(symbol) -> object
 */
static mrb_value
binding_local_variable_get(mrb_state *mrb, mrb_value self)
{
  mrb_sym varname;
  mrb_get_args(mrb, "n", &varname);

  const struct RProc *proc = mrb_binding_extract_proc(mrb, self);
  struct REnv *env = mrb_binding_extract_env(mrb, self);
  mrb_value *e = binding_local_variable_search(mrb, proc, env, varname);
  if (!e) {
    mrb_raisef(mrb, E_NAME_ERROR, "local variable %!n is not defined", varname);
  }

  return *e;
}

static mrb_value
binding_local_variable_set(mrb_state *mrb, mrb_value self)
{
  mrb_sym varname;
  mrb_value obj;
  mrb_get_args(mrb, "no", &varname, &obj);

  const struct RProc *proc = mrb_binding_extract_proc(mrb, self);
  struct REnv *env = mrb_binding_extract_env(mrb, self);
  mrb_value *e = binding_local_variable_search(mrb, proc, env, varname);
  if (e) {
    *e = obj;
    if (!mrb_immediate_p(obj)) {
      mrb_field_write_barrier(mrb, (struct RBasic*)env, (struct RBasic*)mrb_obj_ptr(obj));
    }
  }
  else {
    mrb_proc_merge_lvar(mrb, (mrb_irep*)proc->body.irep, env, 1, &varname, &obj);
  }

  return obj;
}

static mrb_value
binding_local_variables(mrb_state *mrb, mrb_value self)
{
  const struct RProc *proc = mrb_proc_ptr(mrb_iv_get(mrb, self, MRB_SYM(proc)));
  return mrb_proc_local_variables(mrb, proc);
}

static mrb_value
binding_receiver(mrb_state *mrb, mrb_value self)
{
  return mrb_iv_get(mrb, self, MRB_SYM(recv));
}

/*
 * call-seq:
 *  source_location -> [String, Integer]
 */
static mrb_value
binding_source_location(mrb_state *mrb, mrb_value self)
{
  if (mrb_iv_defined(mrb, self, MRB_SYM(source_location))) {
    return mrb_iv_get(mrb, self, MRB_SYM(source_location));
  }

  mrb_value srcloc;
  const struct RProc *proc = mrb_binding_extract_proc(mrb, self);
  if (!proc || MRB_PROC_CFUNC_P(proc) ||
      !proc->upper || MRB_PROC_CFUNC_P(proc->upper)) {
    srcloc = mrb_nil_value();
  }
  else {
    const mrb_irep *irep = proc->upper->body.irep;
    mrb_int pc = binding_extract_pc(mrb, self);
    if (pc < 0) {
      srcloc = mrb_nil_value();
    }
    else {
      const char *fname;
      int32_t line;

      if (!mrb_debug_get_position(mrb, irep, (uint32_t)pc, &line, &fname)) {
        srcloc  = mrb_nil_value();
      }
      else {
        srcloc = mrb_assoc_new(mrb, mrb_str_new_cstr(mrb, fname), mrb_fixnum_value(line));
      }
    }
  }

  if (!mrb_frozen_p(mrb_obj_ptr(self))) {
    mrb_iv_set(mrb, self, MRB_SYM(source_location), srcloc);
  }
  return srcloc;
}

mrb_value
mrb_binding_new(mrb_state *mrb, const struct RProc *proc, mrb_value recv, struct REnv *env)
{
  struct RObject *binding = MRB_OBJ_ALLOC(mrb, MRB_TT_OBJECT, mrb_class_get_id(mrb, MRB_SYM(Binding)));

  if (proc && !MRB_PROC_CFUNC_P(proc)) {
    const mrb_irep *irep = proc->body.irep;
    mrb_obj_iv_set(mrb, binding, MRB_SYM(pc), mrb_fixnum_value(mrb->c->ci[-1].pc - irep->iseq - 1 /* step back */));
  }
  proc = binding_wrap_lvspace(mrb, proc, &env);

  mrb_obj_iv_set(mrb, binding, MRB_SYM(proc), mrb_obj_value((void*)proc));
  mrb_obj_iv_set(mrb, binding, MRB_SYM(recv), recv);
  mrb_obj_iv_set(mrb, binding, MRB_SYM(env), mrb_obj_value(env));

  return mrb_obj_value(binding);
}

static mrb_value
mrb_f_binding(mrb_state *mrb, mrb_value self)
{
  struct RProc *proc;
  struct REnv *env;

  if (mrb->c->ci->cci != 0) {
  caller_err:
    mrb_raise(mrb, E_RUNTIME_ERROR, "Cannot create Binding object for non-Ruby caller");
  }
  proc = (struct RProc*)mrb_proc_get_caller(mrb, &env);
  if (!env || MRB_PROC_CFUNC_P(proc)) {
    goto caller_err;
  }
  return mrb_binding_new(mrb, proc, self, env);
}

void
mrb_mruby_binding_gem_init(mrb_state *mrb)
{
  struct RClass *binding = mrb_define_class(mrb, "Binding", mrb->object_class);
  MRB_SET_INSTANCE_TT(binding, MRB_TT_OBJECT);
  MRB_UNDEF_ALLOCATOR(binding);
  mrb_undef_class_method(mrb, binding, "new");
  mrb_undef_class_method(mrb, binding, "allocate");

  mrb_define_method(mrb, mrb->kernel_module, "binding", mrb_f_binding, MRB_ARGS_NONE());

  mrb_define_method(mrb, binding, "initialize_copy", binding_initialize_copy, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, binding, "local_variable_defined?", binding_local_variable_defined_p, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, binding, "local_variable_get", binding_local_variable_get, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, binding, "local_variable_set", binding_local_variable_set, MRB_ARGS_REQ(2));
  mrb_define_method(mrb, binding, "local_variables", binding_local_variables, MRB_ARGS_NONE());
  mrb_define_method(mrb, binding, "receiver", binding_receiver, MRB_ARGS_NONE());
  mrb_define_method(mrb, binding, "source_location", binding_source_location, MRB_ARGS_NONE());
  mrb_define_method(mrb, binding, "inspect", mrb_any_to_s, MRB_ARGS_NONE());
}

void
mrb_mruby_binding_gem_final(mrb_state *mrb)
{
}

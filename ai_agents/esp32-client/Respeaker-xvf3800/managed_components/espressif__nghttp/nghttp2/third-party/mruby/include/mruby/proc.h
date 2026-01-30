/**
** @file mruby/proc.h - Proc class
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_PROC_H
#define MRUBY_PROC_H

#include "common.h"
#include <mruby/irep.h>

/**
 * Proc class
 */
MRB_BEGIN_DECL

struct REnv {
  MRB_OBJECT_HEADER;
  mrb_value *stack;
  struct mrb_context *cxt;
  mrb_sym mid;
};

/* flags (21bits): 1(close):1(touched):1(heap):8(cioff/bidx):8(stack_len) */
#define MRB_ENV_SET_LEN(e,len) ((e)->flags = (((e)->flags & ~0xff)|((unsigned int)(len) & 0xff)))
#define MRB_ENV_LEN(e) ((mrb_int)((e)->flags & 0xff))
#define MRB_ENV_CLOSED (1<<20)
#define MRB_ENV_CLOSE(e) ((e)->flags |= MRB_ENV_CLOSED)
#define MRB_ENV_ONSTACK_P(e) (((e)->flags & MRB_ENV_CLOSED) == 0)
#define MRB_ENV_BIDX(e) (((e)->flags >> 8) & 0xff)
#define MRB_ENV_SET_BIDX(e,idx) ((e)->flags = (((e)->flags & ~(0xff<<8))|((unsigned int)(idx) & 0xff)<<8))

/*
 * Returns TRUE on success.
 * If the function fails:
 * * Returns FALSE if noraise is TRUE.
 * * Raises a NoMemoryError exception if noraise is FALSE.
 */
mrb_bool mrb_env_unshare(mrb_state*, struct REnv*, mrb_bool noraise);

struct RProc {
  MRB_OBJECT_HEADER;
  union {
    const mrb_irep *irep;
    mrb_func_t func;
    mrb_sym mid;
  } body;
  const struct RProc *upper;
  union {
    struct RClass *target_class;
    struct REnv *env;
  } e;
};

/* aspec access */
#define MRB_ASPEC_REQ(a)          (((a) >> 18) & 0x1f)
#define MRB_ASPEC_OPT(a)          (((a) >> 13) & 0x1f)
#define MRB_ASPEC_REST(a)         (((a) >> 12) & 0x1)
#define MRB_ASPEC_POST(a)         (((a) >> 7) & 0x1f)
#define MRB_ASPEC_KEY(a)          (((a) >> 2) & 0x1f)
#define MRB_ASPEC_KDICT(a)        (((a) >> 1) & 0x1)
#define MRB_ASPEC_BLOCK(a)        ((a) & 1)

#define MRB_PROC_CFUNC_FL 128
#define MRB_PROC_CFUNC_P(p) (((p)->flags & MRB_PROC_CFUNC_FL) != 0)
#define MRB_PROC_CFUNC(p) (p)->body.func
#define MRB_PROC_STRICT 256
#define MRB_PROC_STRICT_P(p) (((p)->flags & MRB_PROC_STRICT) != 0)
#define MRB_PROC_ORPHAN 512
#define MRB_PROC_ORPHAN_P(p) (((p)->flags & MRB_PROC_ORPHAN) != 0)
#define MRB_PROC_ENVSET 1024
#define MRB_PROC_ENV_P(p) (((p)->flags & MRB_PROC_ENVSET) != 0)
#define MRB_PROC_ENV(p) (MRB_PROC_ENV_P(p) ? (p)->e.env : NULL)
#define MRB_PROC_TARGET_CLASS(p) (MRB_PROC_ENV_P(p) ? (p)->e.env->c : (p)->e.target_class)
#define MRB_PROC_SET_TARGET_CLASS(p,tc) do {\
  if (MRB_PROC_ENV_P(p)) {\
    (p)->e.env->c = (tc);\
    mrb_field_write_barrier(mrb, (struct RBasic*)(p)->e.env, (struct RBasic*)(tc));\
  }\
  else {\
    (p)->e.target_class = (tc);\
    mrb_field_write_barrier(mrb, (struct RBasic*)p, (struct RBasic*)(tc));\
  }\
} while (0)
#define MRB_PROC_SCOPE 2048
#define MRB_PROC_SCOPE_P(p) (((p)->flags & MRB_PROC_SCOPE) != 0)
#define MRB_PROC_NOARG 4096 /* for MRB_PROC_CFUNC_FL, it would be something like MRB_ARGS_NONE() or MRB_METHOD_NOARG_FL */
#define MRB_PROC_NOARG_P(p) (((p)->flags & MRB_PROC_NOARG) != 0)
#define MRB_PROC_ALIAS 8192
#define MRB_PROC_ALIAS_P(p) (((p)->flags & MRB_PROC_ALIAS) != 0)

#define mrb_proc_ptr(v)    ((struct RProc*)(mrb_ptr(v)))

struct RProc *mrb_proc_new(mrb_state*, const mrb_irep*);
MRB_API struct RProc *mrb_proc_new_cfunc(mrb_state*, mrb_func_t);
MRB_API struct RProc *mrb_closure_new_cfunc(mrb_state *mrb, mrb_func_t func, int nlocals);

/* following functions are defined in mruby-proc-ext so please include it when using */
MRB_API struct RProc *mrb_proc_new_cfunc_with_env(mrb_state *mrb, mrb_func_t func, mrb_int argc, const mrb_value *argv);
MRB_API mrb_value mrb_proc_cfunc_env_get(mrb_state *mrb, mrb_int idx);
/* old name */
#define mrb_cfunc_env_get(mrb, idx) mrb_proc_cfunc_env_get(mrb, idx)

#define MRB_METHOD_FUNC_FL 1
#define MRB_METHOD_NOARG_FL 2

#ifndef MRB_USE_METHOD_T_STRUCT

#define MRB_METHOD_FUNC_P(m) (((uintptr_t)(m))&MRB_METHOD_FUNC_FL)
#define MRB_METHOD_NOARG_P(m) ((((uintptr_t)(m))&MRB_METHOD_NOARG_FL)?1:0)
#define MRB_METHOD_NOARG_SET(m) ((m)=(mrb_method_t)(((uintptr_t)(m))|MRB_METHOD_NOARG_FL))
#define MRB_METHOD_FUNC(m) ((mrb_func_t)((uintptr_t)(m)>>2))
#define MRB_METHOD_FROM_FUNC(m,fn) ((m)=(mrb_method_t)((((uintptr_t)(fn))<<2)|MRB_METHOD_FUNC_FL))
#define MRB_METHOD_FROM_PROC(m,pr) ((m)=(mrb_method_t)(pr))
#define MRB_METHOD_PROC_P(m) (!MRB_METHOD_FUNC_P(m))
#define MRB_METHOD_PROC(m) ((struct RProc*)(m))
#define MRB_METHOD_UNDEF_P(m) ((m)==0)

#else

#define MRB_METHOD_FUNC_P(m) ((m).flags&MRB_METHOD_FUNC_FL)
#define MRB_METHOD_NOARG_P(m) (((m).flags&MRB_METHOD_NOARG_FL)?1:0)
#define MRB_METHOD_FUNC(m) ((m).func)
#define MRB_METHOD_NOARG_SET(m) do{(m).flags|=MRB_METHOD_NOARG_FL;}while(0)
#define MRB_METHOD_FROM_FUNC(m,fn) do{(m).flags=MRB_METHOD_FUNC_FL;(m).func=(fn);}while(0)
#define MRB_METHOD_FROM_PROC(m,pr) do{(m).flags=0;(m).proc=(struct RProc*)(pr);}while(0)
#define MRB_METHOD_PROC_P(m) (!MRB_METHOD_FUNC_P(m))
#define MRB_METHOD_PROC(m) ((m).proc)
#define MRB_METHOD_UNDEF_P(m) ((m).proc==NULL)

#endif /* MRB_USE_METHOD_T_STRUCT */

#define MRB_METHOD_CFUNC_P(m) (MRB_METHOD_FUNC_P(m)?TRUE:(MRB_METHOD_PROC(m)?(MRB_PROC_CFUNC_P(MRB_METHOD_PROC(m))):FALSE))
#define MRB_METHOD_CFUNC(m) (MRB_METHOD_FUNC_P(m)?MRB_METHOD_FUNC(m):((MRB_METHOD_PROC(m)&&MRB_PROC_CFUNC_P(MRB_METHOD_PROC(m)))?MRB_PROC_CFUNC(MRB_METHOD_PROC(m)):NULL))


#include <mruby/khash.h>

MRB_API mrb_value mrb_load_proc(mrb_state *mrb, const struct RProc *proc);

/**
 *  It can be used to isolate top-level scopes referenced by blocks generated by
 *  `mrb_load_string_cxt()` or similar called before entering the mruby VM (e.g. from `main()`).
 *  In that case, the `ci` parameter should be `mrb->c->cibase`.
 *
 *      #include <mruby.h>
 *      #include <mruby/compile.h>
 *      #include <mruby/proc.h>
 *
 *      int
 *      main(int argc, char **argv)
 *      {
 *        mrb_state *mrb;
 *        mrb_ccontext *cxt;
 *        mrb_value blk, ret;
 *
 *        mrb = mrb_open();
 *        cxt = mrb_ccontext_new(mrb);
 *        blk = mrb_load_string_cxt(mrb, "x, y, z = 1, 2, 3; proc { [x, y, z] }", cxt);
 *        mrb_vm_ci_env_clear(mrb, mrb->c->cibase);
 *        mrb_load_string_cxt(mrb, "x, y, z = 4, 5, 6", cxt);
 *        ret = mrb_funcall(mrb, blk, "call", 0);
 *        mrb_p(mrb, ret);  // => [1, 2, 3]
 *                          // => [4, 5, 6] if `mrb_vm_ci_env_clear()` is commented out
 *        mrb_ccontext_free(mrb, cxt);
 *        mrb_close(mrb);
 *
 *        return 0;
 *      }
 *
 *  The top-level local variable names stored in `mrb_ccontext` are retained.
 *  Use also `mrb_ccontext_cleanup_local_variables()` at the same time, if necessary.
 */
MRB_API void mrb_vm_ci_env_clear(mrb_state *mrb, mrb_callinfo *ci);

void mrb_vm_ci_proc_set(mrb_callinfo *ci, const struct RProc *p);
struct RClass * mrb_vm_ci_target_class(const mrb_callinfo *ci);
void mrb_vm_ci_target_class_set(mrb_callinfo *ci, struct RClass *tc);
struct REnv * mrb_vm_ci_env(const mrb_callinfo *ci);
void mrb_vm_ci_env_set(mrb_callinfo *ci, struct REnv *e);

MRB_END_DECL

#endif  /* MRUBY_PROC_H */

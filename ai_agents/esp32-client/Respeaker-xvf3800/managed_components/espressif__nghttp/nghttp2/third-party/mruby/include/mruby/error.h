/**
** @file mruby/error.h - Exception class
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_ERROR_H
#define MRUBY_ERROR_H

#include "common.h"

/**
 * mruby error handling.
 */
MRB_BEGIN_DECL

struct RException {
  MRB_OBJECT_HEADER;
  struct iv_tbl *iv;
  struct RObject *mesg;         // NULL or probably RString
  struct RObject *backtrace;    // NULL, RArray or RData
};

/* error that should terminate execution */
#define MRB_EXC_EXIT 65536
#define MRB_EXC_EXIT_P(e) ((e)->flags & MRB_EXC_EXIT)
/* retrieve status value from exc; need <mruby/variable.h> and <mruby/presym.h> */
#define MRB_EXC_EXIT_STATUS(mrb,e) ((int)mrb_as_int((mrb),mrb_obj_iv_get((mrb),(e),MRB_SYM(status))))
/* exit with SystemExit status */
#define MRB_EXC_CHECK_EXIT(mrb,e) do {if (MRB_EXC_EXIT_P(e)) exit(MRB_EXC_EXIT_STATUS((mrb),(e)));} while (0)

#define mrb_exc_ptr(v) ((struct RException*)mrb_ptr(v))

MRB_API mrb_noreturn void mrb_sys_fail(mrb_state *mrb, const char *mesg);
MRB_API mrb_value mrb_exc_new_str(mrb_state *mrb, struct RClass* c, mrb_value str);
#define mrb_exc_new_lit(mrb, c, lit) mrb_exc_new_str(mrb, c, mrb_str_new_lit(mrb, lit))
MRB_API mrb_noreturn void mrb_no_method_error(mrb_state *mrb, mrb_sym id, mrb_value args, const char *fmt, ...);

#if defined(MRB_64BIT) || defined(MRB_USE_FLOAT32) || defined(MRB_NAN_BOXING) || defined(MRB_WORD_BOXING)
#undef MRB_USE_RBREAK_VALUE_UNION
#else
#define MRB_USE_RBREAK_VALUE_UNION 1
#endif

/*
 *  flags:
 *      0..7:   enum mrb_vtype (only when defined MRB_USE_RBREAK_VALUE_UNION)
 *      8..10:  RBREAK_TAGs in src/vm.c (otherwise, set to 0)
 */
struct RBreak {
  MRB_OBJECT_HEADER;
  uintptr_t ci_break_index; // The top-level ci index to break. One before the return destination.
#ifndef MRB_USE_RBREAK_VALUE_UNION
  mrb_value val;
#else
  union mrb_value_union value;
#endif
};

#ifndef MRB_USE_RBREAK_VALUE_UNION
#define mrb_break_value_get(brk) ((brk)->val)
#define mrb_break_value_set(brk, v) ((brk)->val = v)
#else
#define RBREAK_VALUE_TT_MASK ((1 << 8) - 1)
static inline mrb_value
mrb_break_value_get(struct RBreak *brk)
{
  mrb_value val;
  val.value = brk->value;
  val.tt = (enum mrb_vtype)(brk->flags & RBREAK_VALUE_TT_MASK);
  return val;
}
static inline void
mrb_break_value_set(struct RBreak *brk, mrb_value val)
{
  brk->value = val.value;
  brk->flags &= ~RBREAK_VALUE_TT_MASK;
  brk->flags |= val.tt;
}
#endif  /* MRB_USE_RBREAK_VALUE_UNION */

/**
 * Error check
 *
 */
/* clear error status in the mrb_state structure */
MRB_API void mrb_clear_error(mrb_state *mrb);
/* returns TRUE if error in the previous call; internally calls mrb_clear_error() */
MRB_API mrb_bool mrb_check_error(mrb_state *mrb);

/**
 * Protect
 *
 */
typedef mrb_value mrb_protect_error_func(mrb_state *mrb, void *userdata);
MRB_API mrb_value mrb_protect_error(mrb_state *mrb, mrb_protect_error_func *body, void *userdata, mrb_bool *error);

/**
 * Protect (takes mrb_value for body argument)
 *
 * Implemented in the mruby-error mrbgem
 */
MRB_API mrb_value mrb_protect(mrb_state *mrb, mrb_func_t body, mrb_value data, mrb_bool *state);

/**
 * Ensure
 *
 * Implemented in the mruby-error mrbgem
 */
MRB_API mrb_value mrb_ensure(mrb_state *mrb, mrb_func_t body, mrb_value b_data,
                             mrb_func_t ensure, mrb_value e_data);

/**
 * Rescue
 *
 * Implemented in the mruby-error mrbgem
 */
MRB_API mrb_value mrb_rescue(mrb_state *mrb, mrb_func_t body, mrb_value b_data,
                             mrb_func_t rescue, mrb_value r_data);

/**
 * Rescue exception
 *
 * Implemented in the mruby-error mrbgem
 */
MRB_API mrb_value mrb_rescue_exceptions(mrb_state *mrb, mrb_func_t body, mrb_value b_data,
                                        mrb_func_t rescue, mrb_value r_data,
                                        mrb_int len, struct RClass **classes);

MRB_END_DECL

#endif  /* MRUBY_ERROR_H */

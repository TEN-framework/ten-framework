#include <mruby.h>
#include <mruby/class.h>
#include <mruby/variable.h>
#include <mruby/error.h>
#include <mruby/proc.h>
#include <mruby/opcode.h>
#include <mruby/presym.h>

MRB_PRESYM_DEFINE_VAR_AND_INITER(catch_syms, 3, MRB_SYM(Object), MRB_SYM(new), MRB_SYM(call))
/*
 *  def catch(r1 = Object.new, &r2)
 *    r2.call(r1)
 *  end
 */
static const mrb_code catch_iseq[] = {
  OP_ENTER,    0x00, 0x20, 0x01,     // 000 ENTER         0:1:0:0:0:0:1 (0x2001)
  OP_JMP,      0x00, 0x06,           // 004 JMP           013

  // copy for block parameter "tag" when method argument are given
  OP_MOVE,     0x03, 0x01,           // 007 MOVE          R3      R1
  OP_JMP,      0x00, 0x0a,           // 010 JMP           023

  // create a tag for default parameter
  OP_GETCONST, 0x03, 0x00,           // 013 GETCONST      R3      Object
  OP_SEND,     0x03, 0x01, 0x00,     // 016 SEND          R3      :new    n=0
  OP_MOVE,     0x01, 0x03,           // 020 MOVE          R1      R3

  // to save on the stack, block variables are used as is
  OP_SEND,     0x02, 0x02, 0x01,     // 023 SEND          R2      :call   n=1
  OP_RETURN,   0x02,                 // 027 RETURN        R2
};
static const mrb_irep catch_irep = {
  3,5,0,
  MRB_IREP_STATIC,catch_iseq,
  NULL,catch_syms,NULL,
  NULL,
  NULL,
  sizeof(catch_iseq),0,3,0,0
};
static const struct RProc catch_proc = {
  NULL, NULL, MRB_TT_PROC, MRB_GC_RED, MRB_FL_OBJ_IS_FROZEN | MRB_PROC_SCOPE | MRB_PROC_STRICT,
  { &catch_irep }, NULL, { NULL }
};

static uintptr_t
find_catcher(mrb_state *mrb, mrb_value tag)
{
  const mrb_callinfo *ci = mrb->c->ci - 1; // skip ownself throw
  ptrdiff_t n = ci - mrb->c->cibase;

  for (; n > 0; n--, ci--) {
    const mrb_value *arg1 = ci->stack + 1;
    if (ci->proc == &catch_proc && mrb_obj_eq(mrb, *arg1, tag)) {
      return (uintptr_t)n;
    }
  }

  return 0;
}

static mrb_value
throw_m(mrb_state *mrb, mrb_value self)
{
  mrb_value tag, obj;
  if (mrb_get_args(mrb, "o|o", &tag, &obj) == 1) {
    obj = mrb_nil_value();
  }

  uintptr_t ci_index = find_catcher(mrb, tag);
  if (ci_index > 0) {
    struct RBreak *b = MRB_OBJ_ALLOC(mrb, MRB_TT_BREAK, NULL);
    mrb_break_value_set(b, obj);
    b->ci_break_index = ci_index; /* Back to the caller directly */
    mrb_exc_raise(mrb, mrb_obj_value(b));
  }
  else {
    mrb_value argv[2] = {tag, obj};
    mrb_exc_raise(mrb, mrb_obj_new(mrb, mrb_exc_get_id(mrb, MRB_ERROR_SYM(UncaughtThrowError)), 2, argv));
  }
  /* not reached */
  return mrb_nil_value();
}

void
mrb_mruby_catch_gem_init(mrb_state *mrb)
{
  mrb_method_t m;

  MRB_PRESYM_INIT_SYMBOLS(mrb, catch_syms);
  MRB_METHOD_FROM_PROC(m, &catch_proc);
  mrb_define_method_raw(mrb, mrb->kernel_module, MRB_SYM(catch), m);

  mrb_define_method(mrb, mrb->kernel_module, "throw", throw_m, MRB_ARGS_ARG(1,1));
}

void
mrb_mruby_catch_gem_final(mrb_state *mrb)
{
}

#include <stdlib.h>
#include <mruby.h>
#include <mruby/error.h>
#include <mruby/variable.h>
#include <mruby/presym.h>

#ifndef EXIT_SUCCESS
# define EXIT_SUCCESS 0
#endif

#ifndef EXIT_FAILURE
# define EXIT_FAILURE 1
#endif

static int
get_status(mrb_state *mrb)
{
  mrb_value status = mrb_true_value();

  mrb_get_args(mrb, "|o", &status);
  if (mrb_true_p(status)) return EXIT_SUCCESS;
  if (mrb_false_p(status)) return EXIT_FAILURE;
  return (int)mrb_as_int(mrb, status);
}

/*
 *  call-seq:
 *     exit(status=true)
 *
 *  Initiates the termination of the Ruby script by raising the
 *  SystemExit exception. This exception may be caught. The
 *  optional parameter is used to return a status code to the invoking
 *  environment.
 *
 *  +true+ and +false+ of _status_ means success and failure
 *  respectively.  The interpretation of other integer values are
 *  system dependent.
 *
 *     exit(0)
 */
static mrb_value
f_exit(mrb_state *mrb, mrb_value self)
{
  int status = get_status(mrb);
  mrb_value exc = mrb_obj_new(mrb, mrb_exc_get_id(mrb, MRB_SYM(SystemExit)), 0, NULL);
  struct RException *e = mrb_exc_ptr(exc);
  e->flags |= MRB_EXC_EXIT;
  mrb_iv_set(mrb, exc, MRB_SYM(status), mrb_int_value(mrb, (mrb_int)status));
  mrb_exc_raise(mrb, exc);
  /* not reached */
  return mrb_nil_value();
}

/*
 *  call-seq:
 *     exit!(status=true)
 *
 *  Exits the process immediately. No exit handlers are run.
 *  <em>status</em> is returned to the underlying system as the
 *  exit status.
 *
 *     exit!(0)
 */
static mrb_value
f_exit_bang(mrb_state *mrb, mrb_value self)
{
  exit(get_status(mrb));
  /* not reached */
  return mrb_nil_value();
}

void
mrb_mruby_exit_gem_init(mrb_state* mrb)
{
  mrb_define_class_id(mrb, MRB_SYM(SystemExit), E_EXCEPTION);
  mrb_define_method_id(mrb, mrb->kernel_module, MRB_SYM(exit), f_exit, MRB_ARGS_OPT(1));
  mrb_define_method_id(mrb, mrb->kernel_module, MRB_SYM_B(exit), f_exit_bang, MRB_ARGS_OPT(1));
}

void
mrb_mruby_exit_gem_final(mrb_state* mrb)
{
}

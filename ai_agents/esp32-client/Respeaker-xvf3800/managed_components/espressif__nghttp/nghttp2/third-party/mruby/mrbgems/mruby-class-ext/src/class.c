#include "mruby.h"
#include "mruby/class.h"
#include "mruby/string.h"
#include "mruby/array.h"
#include "mruby/proc.h"
#include "mruby/variable.h"
#include "mruby/presym.h"

static mrb_value
mod_name(mrb_state *mrb, mrb_value self)
{
  mrb_value name =  mrb_class_path(mrb, mrb_class_ptr(self));
  if (mrb_string_p(name)) {
    MRB_SET_FROZEN_FLAG(mrb_basic_ptr(name));
  }
  return name;
}

static mrb_value
mod_singleton_class_p(mrb_state *mrb, mrb_value self)
{
  return mrb_bool_value(mrb_sclass_p(self));
}

/*
 *  call-seq:
 *     module_exec(arg...) {|var...| block } -> obj
 *     class_exec(arg...) {|var...| block } -> obj
 *
 * Evaluates the given block in the context of the
 * class/module. The method defined in the block will belong
 * to the receiver. Any arguments passed to the method will be
 * passed to the block. This can be used if the block needs to
 * access instance variables.
 *
 *     class Thing
 *     end
 *     Thing.class_exec{
 *       def hello() "Hello there!" end
 *     }
 *     puts Thing.new.hello()
 */

static mrb_value
mod_module_exec(mrb_state *mrb, mrb_value self)
{
  const mrb_value *argv;
  mrb_int argc;
  mrb_value blk;
  struct RClass *c;

  mrb_get_args(mrb, "*&!", &argv, &argc, &blk);

  c = mrb_class_ptr(self);
  if (mrb->c->ci->cci > 0) {
    return mrb_yield_with_class(mrb, blk, argc, argv, self, c);
  }
  mrb_vm_ci_target_class_set(mrb->c->ci, c);
  return mrb_yield_cont(mrb, blk, self, argc, argv);
}

struct subclass_args {
  struct RClass *c;
  mrb_value ary;
};

static int
add_subclasses(mrb_state *mrb, struct RBasic *obj, void *data)
{
  struct subclass_args *args = (struct subclass_args*)data;
  if (obj->tt == MRB_TT_CLASS) {
    struct RClass *c = (struct RClass*)obj;
    if (mrb_class_real(c->super) == args->c) {
      mrb_ary_push(mrb, args->ary, mrb_obj_value(obj));
    }
  }
  return MRB_EACH_OBJ_OK;
}

/*
 *  call-seq:
 *     subclasses -> array
 *
 *  Returns an array of classes where the receiver is the
 *  direct superclass of the class, excluding singleton classes.
 *  The order of the returned array is not defined.
 *
 *     class A; end
 *     class B < A; end
 *     class C < B; end
 *     class D < A; end
 *
 *     A.subclasses        #=> [D, B]
 *     B.subclasses        #=> [C]
 *     C.subclasses        #=> []
 */
static mrb_value
class_subclasses(mrb_state *mrb, mrb_value self)
{
  struct RClass *c;
  mrb_value ary;

  c = mrb_class_ptr(self);
  ary = mrb_ary_new(mrb);

  if (c->flags & MRB_FL_CLASS_IS_INHERITED) {
    struct subclass_args arg = {c, ary};
    mrb_objspace_each_objects(mrb, add_subclasses, &arg);
  }
  return ary;
}

/*
 *  call-seq:
 *     attached_object -> object
 *
 *  Returns the object for which the receiver is the singleton class.
 *  Raises an TypeError if the class is not a singleton class.
 *
 *     class Foo; end
 *
 *     Foo.singleton_class.attached_object        #=> Foo
 *     Foo.attached_object                        #=> TypeError: not a singleton class
 *     Foo.new.singleton_class.attached_object    #=> #<Foo:0x000000010491a370>
 *     TrueClass.attached_object                  #=> TypeError: not a singleton class
 *     NilClass.attached_object                   #=> TypeError: not a singleton class
 */
static mrb_value
class_attached_object(mrb_state *mrb, mrb_value self)
{
  struct RClass *c;

  c = mrb_class_ptr(self);
  if (c->tt != MRB_TT_SCLASS) {
    mrb_raise(mrb, E_TYPE_ERROR, "not a singleton class");
  }
  return mrb_obj_iv_get(mrb, (struct RObject*)c, MRB_SYM(__attached__));
}

void
mrb_mruby_class_ext_gem_init(mrb_state *mrb)
{
  struct RClass *mod = mrb->module_class;

  mrb_define_method(mrb, mod, "name", mod_name, MRB_ARGS_NONE());
  mrb_define_method(mrb, mod, "singleton_class?", mod_singleton_class_p, MRB_ARGS_NONE());
  mrb_define_method(mrb, mod, "module_exec", mod_module_exec, MRB_ARGS_ANY()|MRB_ARGS_BLOCK());
  mrb_define_method(mrb, mod, "class_exec", mod_module_exec, MRB_ARGS_ANY()|MRB_ARGS_BLOCK());

  struct RClass *cls = mrb->class_class;
  mrb_define_method(mrb, cls, "subclasses", class_subclasses, MRB_ARGS_NONE());
  mrb_define_method(mrb, cls, "attached_object", class_attached_object, MRB_ARGS_NONE());
}

void
mrb_mruby_class_ext_gem_final(mrb_state *mrb)
{
}

#include <mruby.h>
#include <mruby/string.h>
#include <mruby/class.h>
#include <mruby/data.h>
#include <stdio.h>

static void mrb_foo_free(mrb_state *mrb, void *ptr) {
  /* custom destructor */
  mrb_free(mrb, ptr);
}
struct mrb_data_type mrb_foo_type = { "Foo", mrb_foo_free };

struct Foo {
  int bar;
  char baz[32];
};

static mrb_value
mrb_foo_initialize(mrb_state *mrb, mrb_value self)
{
  struct Foo *f;

  f = (struct Foo*)mrb_malloc(mrb, sizeof(struct Foo));
  f->bar = 0;

  DATA_PTR(self) = f;
  DATA_TYPE(self) = &mrb_foo_type;

  return self;
}

static mrb_value
mrb_foo_get_bar(mrb_state *mrb, mrb_value self)
{
  struct Foo *f;

  f = (struct Foo*)mrb_data_get_ptr(mrb, self, &mrb_foo_type);
  if (f == NULL) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "uninitialized data");
  }

  return mrb_fixnum_value(f->bar);
}

static mrb_value
mrb_foo_set_bar(mrb_state *mrb, mrb_value self)
{
  struct Foo *f;
  int v;

  f = (struct Foo*)mrb_data_get_ptr(mrb, self, &mrb_foo_type);
  if (f == NULL) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "uninitialized data");
  }

  mrb_get_args(mrb, "i", &v);

  f->bar = v;

  return mrb_fixnum_value(f->bar);
}

void
mrb_cdata_extension_example_gem_init(mrb_state* mrb) {
  struct RClass *class_foo;

  class_foo = mrb_define_class(mrb, "Foo", mrb->object_class);
  MRB_SET_INSTANCE_TT(class_foo, MRB_TT_CDATA);
  mrb_define_method(mrb, class_foo, "initialize", mrb_foo_initialize, MRB_ARGS_NONE());
  mrb_define_method(mrb, class_foo, "bar", mrb_foo_get_bar, MRB_ARGS_NONE());
  mrb_define_method(mrb, class_foo, "bar=", mrb_foo_set_bar, MRB_ARGS_REQ(1));
}

void
mrb_cdata_extension_example_gem_final(mrb_state* mrb) {
  /* gem finalizer */
}

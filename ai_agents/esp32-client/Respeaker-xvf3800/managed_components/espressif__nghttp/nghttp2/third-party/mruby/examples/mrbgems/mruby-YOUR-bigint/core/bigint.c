/*
 * If placed under the "mruby/examples/mrbgems/mruby-YOUR-bigint" directory,
 * this file is available under the Creative Commons Zero License (CC0).
 * Note that file is incomplete.
 *
 * TODO: If this file is copied and another implementation is written,
 * remove this comment block from the copied file.
 */

#include <mruby.h>
#include <mruby/numeric.h>

/*
 * The "mruby/internal.h" file should be placed after the other mruby header files.
 */
#include <mruby/internal.h>

/*
 * The "mruby/presym.h" file is placed at the end of the mruby header file.
 */
#include <mruby/presym.h>

/*
 * Define your own struct RBigint.
 *
 * - Object type must be MRB_TT_BIGINT.
 * - If the structure is named RBigint, MRB_OBJ_ALLOC() can be used as is.
 */
struct RBigint {
  /*
   * Put MRB_OBJECT_HEADER before the first member of the structure.
   */
  MRB_OBJECT_HEADER;

  /*
   * Up to 3 words can be freely configured.
   */
  size_t len;
  size_t capa;
  uintptr_t *num;
};

/*
 * Assert with mrb_static_assert_object_size() that the entire structure is within 6 words.
 */
mrb_static_assert_object_size(struct RBigint);

/*
 * The lower 16 bits of the object flags (`obj->flags`) can be used freely by the GEM author.
 */
#define MY_BIGINT_NEGATIVE_FLAG 1
#define MY_BIGINT_NEGATIVE_P(obj) ((obj)->flags & MY_BIGINT_NEGATIVE_FLAG)

/*
 * Implement the functions declared in `#ifdef MRUBY_USE_BIGINT ... #endif` in the "mruby/internal.h" file.
 */

mrb_value
mrb_bint_new_int(mrb_state *mrb, mrb_int x)
{
  struct RBigint *obj = MRB_OBJ_ALLOC(mrb, MRB_TT_BIGINT, mrb->integer_class);

  ...

  return mrb_obj_value(obj);
}

/*
 * The implementation function continues...
 */

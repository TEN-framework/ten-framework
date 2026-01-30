/*
** allocf.c - default memory allocation function
**
** See Copyright Notice in mruby.h
*/

#include <stdlib.h>
#include "mruby.h"

/* This function serves as the default memory allocation function and accepts four arguments:
 *
 * - `mrb`: An instance of `mrb_state`. It's important to note that for the initial allocation (used to allocate the `mrb_state` itself), `mrb` is set to NULL.
 * - `p`: The previous pointer to the memory region. For memory allocation, this parameter is NULL.
 * - `size`: The new size of the memory region to be returned.
 * - `ud`: User data, represented as a `void*`, which is passed to the `mrb_state`.
 */

void*
mrb_default_allocf(mrb_state *mrb, void *p, size_t size, void *ud)
{
  if (size == 0) {
    /* `free(NULL)` should be no-op */
    free(p);
    return NULL;
  }
  else {
    /* `ralloc(NULL, size)` works as `malloc(size)` */
    return realloc(p, size);
  }
}

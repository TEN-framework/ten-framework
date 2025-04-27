//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
// This file is modified from
// https://github.com/ianlancetaylor/libbacktrace [BSD license]
//
#include "ten_utils/ten_config.h"

#include "include_internal/ten_utils/backtrace/sort.h"

#include <stddef.h>
#include <sys/types.h>

/**
 * @file The GNU glibc version of qsort allocates memory, which we must not do
 * if we are invoked by a signal handler. So provide our own sort.
 */

static void swap(char *a, char *b, size_t size) {
  for (size_t i = 0; i < size; i++, a++, b++) {
    char t = *a;
    *a = *b;
    *b = t;
  }
}

void backtrace_sort(void *base_arg, size_t count, size_t size,
                    int (*compar)(const void *, const void *)) {
  char *base = (char *)base_arg;

tail_recurse:
  if (count < 2) {
    return;
  }

  // The symbol table and DWARF tables, which is all we use this routine for,
  // tend to be roughly sorted. Pick the middle element in the array as our
  // pivot point, so that we are more likely to cut the array in half for each
  // recursion step.
  swap(base, base + ((count / 2) * size), size);

  size_t mid = 0;
  for (size_t i = 1; i < count; i++) {
    if ((*compar)(base, base + (i * size)) > 0) {
      ++mid;
      if (i != mid) {
        swap(base + (mid * size), base + (i * size), size);
      }
    }
  }

  if (mid > 0) {
    swap(base, base + (mid * size), size);
  }

  // Recurse with the smaller array, loop with the larger one. That ensures that
  // our maximum stack depth is log count.
  if (2 * mid < count) {
    backtrace_sort(base, mid, size, compar);
    base += (mid + 1) * size;
    count -= mid + 1;
    goto tail_recurse;
  } else {
    backtrace_sort(base + ((mid + 1) * size), count - (mid + 1), size, compar);
    count = mid;
    goto tail_recurse;
  }
}

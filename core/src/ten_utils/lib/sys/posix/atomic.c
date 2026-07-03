//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/lib/atomic.h"

#include <stdbool.h>

// IMPORTANT: This file is used in the TEN backtrace module, so it must NOT use
// TEN_ASSERT or any other mechanisms that might trigger backtrace dumps, as
// this would create circular dependencies and potential infinite recursion.

// TODO(Wei): Consider reducing the memory ordering constraints from
// __ATOMIC_SEQ_CST (sequential consistency, strongest but most expensive)
// to a lighter mode like __ATOMIC_ACQUIRE/__ATOMIC_RELEASE where appropriate
// for better performance.

int64_t ten_atomic_fetch_add(volatile ten_atomic_t *a, int64_t v) {
  return __atomic_fetch_add(a, v, __ATOMIC_SEQ_CST);
}

int64_t ten_atomic_fetch_and(volatile ten_atomic_t *a, int64_t v) {
  return __atomic_fetch_and(a, v, __ATOMIC_SEQ_CST);
}

int64_t ten_atomic_fetch_or(volatile ten_atomic_t *a, int64_t v) {
  return __atomic_fetch_or(a, v, __ATOMIC_SEQ_CST);
}

int64_t ten_atomic_add_fetch(volatile ten_atomic_t *a, int64_t v) {
  return __atomic_add_fetch(a, v, __ATOMIC_SEQ_CST);
}

int64_t ten_atomic_and_fetch(volatile ten_atomic_t *a, int64_t v) {
  return __atomic_and_fetch(a, v, __ATOMIC_SEQ_CST);
}

int64_t ten_atomic_fetch_sub(volatile ten_atomic_t *a, int64_t v) {
  return __atomic_fetch_sub(a, v, __ATOMIC_SEQ_CST);
}

int64_t ten_atomic_sub_fetch(volatile ten_atomic_t *a, int64_t v) {
  return __atomic_sub_fetch(a, v, __ATOMIC_SEQ_CST);
}

int64_t ten_atomic_or_fetch(volatile ten_atomic_t *a, int64_t v) {
  return __atomic_or_fetch(a, v, __ATOMIC_SEQ_CST);
}

int64_t ten_atomic_test_set(volatile ten_atomic_t *a, int64_t v) {
  return __atomic_exchange_n(a, v, __ATOMIC_SEQ_CST);
}

int ten_atomic_bool_compare_swap(volatile ten_atomic_t *a, int64_t comp,
                                 int64_t xchg) {
  return __atomic_compare_exchange_n(a, &comp, xchg, false, __ATOMIC_SEQ_CST,
                                     __ATOMIC_SEQ_CST);
}

int64_t ten_atomic_inc_if_non_zero(volatile ten_atomic_t *a) {
  int64_t r = ten_atomic_load(a);

  for (;;) {
    if (r == 0) {
      return r;
    }

    if (__atomic_compare_exchange_n(a, &r, r + 1, false, __ATOMIC_SEQ_CST,
                                    __ATOMIC_SEQ_CST)) {
      return r;
    }
  }
}

int64_t ten_atomic_dec_if_non_zero(volatile ten_atomic_t *a) {
  int64_t r = ten_atomic_load(a);

  for (;;) {
    if (r == 0) {
      return r;
    }

    if (__atomic_compare_exchange_n(a, &r, r - 1, false, __ATOMIC_SEQ_CST,
                                    __ATOMIC_SEQ_CST)) {
      return r;
    }
  }
}

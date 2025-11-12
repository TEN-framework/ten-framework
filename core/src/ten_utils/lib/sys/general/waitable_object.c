//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/lib/waitable_object.h"

#include <memory.h>
#include <stdint.h>

#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/cond.h"
#include "ten_utils/lib/mutex.h"
#include "ten_utils/lib/waitable_number.h"

typedef struct ten_waitable_object_t {
  union {
    void *obj;
    volatile int64_t num;
  } v, p;
  int (*compare)(const void *l, const void *r);
  ten_cond_t *cond;
  ten_mutex_t *lock;
} ten_waitable_number_t, ten_waitable_object_t;

static void ten_waitable_object_free(ten_waitable_object_t *obj) {
  if (obj) {
    if (obj->lock) {
      ten_mutex_destroy(obj->lock);
      obj->lock = NULL;
    }

    if (obj->cond) {
      ten_cond_destroy(obj->cond);
      obj->cond = NULL;
    }

    ten_free(obj);
  }
}

static ten_waitable_number_t *ten_waitable_object_alloc() {
  ten_waitable_object_t *obj = NULL;

  obj = (ten_waitable_object_t *)ten_malloc(sizeof(*obj));
  if (!obj) {
    goto error;
  }

  memset(obj, 0, sizeof(*obj));

  obj->cond = ten_cond_create();
  if (!obj->cond) {
    goto error;
  }

  obj->lock = ten_mutex_create();
  if (!obj->lock) {
    goto error;
  }

  return obj;

error:
  ten_waitable_object_free(obj);
  return NULL;
}

static int ten_waitable_object_valid(ten_waitable_object_t *obj) {
  return (obj && obj->cond && obj->lock) ? 1 : 0;
}

static int ten_number_is_equal(void *arg) {
  ten_waitable_object_t *obj = (ten_waitable_object_t *)arg;
  return obj->v.num == obj->p.num ? 1 : 0;
}

static int ten_number_is_not_equal(void *arg) {
  ten_waitable_object_t *obj = (ten_waitable_object_t *)arg;
  return obj->v.num == obj->p.num ? 0 : 1;
}

static int ten_obj_is_equal(void *arg) {
  ten_waitable_object_t *obj = (ten_waitable_object_t *)arg;
  return obj->compare(obj->v.obj, obj->p.obj);
}

static int ten_obj_is_not_equal(void *arg) {
  ten_waitable_object_t *obj = (ten_waitable_object_t *)arg;
  return !obj->compare(obj->v.obj, obj->p.obj);
}

ten_waitable_number_t *ten_waitable_number_create(int64_t init_value) {
  ten_waitable_object_t *obj = ten_waitable_object_alloc();
  if (!obj) {
    return NULL;
  }

  obj->v.num = init_value;
  return obj;
}

void ten_waitable_number_destroy(ten_waitable_number_t *number) {
  ten_waitable_object_free(number);
}

void ten_waitable_number_increase(ten_waitable_number_t *number,
                                  int64_t value) {
  if (!ten_waitable_object_valid(number)) {
    return;
  }

  if (value == 0) {
    return;
  }

  ten_mutex_lock(number->lock);
  number->v.num += value;
  ten_cond_broadcast(number->cond);
  ten_mutex_unlock(number->lock);
}

void ten_waitable_number_decrease(ten_waitable_number_t *number,
                                  int64_t value) {
  if (!ten_waitable_object_valid(number)) {
    return;
  }

  if (value == 0) {
    return;
  }

  ten_mutex_lock(number->lock);
  number->v.num -= value;
  ten_cond_broadcast(number->cond);
  ten_mutex_unlock(number->lock);
}

void ten_waitable_number_multiply(ten_waitable_number_t *number,
                                  int64_t value) {
  if (!ten_waitable_object_valid(number)) {
    return;
  }

  if (value == 1) {
    return;
  }

  ten_mutex_lock(number->lock);
  number->v.num *= value;
  ten_cond_broadcast(number->cond);
  ten_mutex_unlock(number->lock);
}

void ten_waitable_number_divide(ten_waitable_number_t *number, int64_t value) {
  if (!ten_waitable_object_valid(number)) {
    return;
  }

  if (value == 1) {
    return;
  }

  ten_mutex_lock(number->lock);
  number->v.num /= value;
  ten_cond_broadcast(number->cond);
  ten_mutex_unlock(number->lock);
}

void ten_waitable_number_set(ten_waitable_number_t *number, int64_t value) {
  if (!ten_waitable_object_valid(number)) {
    return;
  }

  ten_mutex_lock(number->lock);
  if (number->v.num != value) {
    number->v.num = value;
    ten_cond_broadcast(number->cond);
  }
  ten_mutex_unlock(number->lock);
}

int64_t ten_waitable_number_get(ten_waitable_number_t *number) {
  int64_t ret = 0;

  if (!ten_waitable_object_valid(number)) {
    return 0;
  }

  ten_mutex_lock(number->lock);
  ret = number->v.num;
  ten_mutex_unlock(number->lock);

  return ret;
}

int ten_waitable_number_wait_until(ten_waitable_number_t *number, int64_t value,
                                   int timeout) {
  int ret = -1;

  if (!ten_waitable_object_valid(number)) {
    return -1;
  }

  ten_mutex_lock(number->lock);
  number->p.num = value;
  ret = ten_cond_wait_while(number->cond, number->lock, ten_number_is_not_equal,
                            number, timeout);
  number->p.num = 0;
  ten_mutex_unlock(number->lock);
  return ret;
}

int ten_waitable_number_wait_while(ten_waitable_number_t *number, int64_t value,
                                   int timeout) {
  int ret = -1;

  if (!ten_waitable_object_valid(number)) {
    return -1;
  }

  ten_mutex_lock(number->lock);
  number->p.num = value;
  ret = ten_cond_wait_while(number->cond, number->lock, ten_number_is_equal,
                            number, timeout);
  number->p.num = 0;
  ten_mutex_unlock(number->lock);
  return ret;
}

ten_waitable_object_t *ten_waitable_object_create(void *init_value) {
  ten_waitable_object_t *obj = ten_waitable_object_alloc();
  if (!obj) {
    return NULL;
  }

  obj->v.obj = init_value;
  return obj;
}

void ten_waitable_object_destroy(ten_waitable_object_t *obj) {
  ten_waitable_object_free(obj);
}

void ten_waitable_object_set(ten_waitable_object_t *obj, void *value) {
  if (!ten_waitable_object_valid(obj)) {
    return;
  }

  ten_mutex_lock(obj->lock);
  obj->v.obj = value;
  ten_cond_broadcast(obj->cond);
  ten_mutex_unlock(obj->lock);
}

void ten_waitable_object_update(ten_waitable_object_t *obj) {
  if (!ten_waitable_object_valid(obj)) {
    return;
  }

  ten_mutex_lock(obj->lock);
  ten_cond_broadcast(obj->cond);
  ten_mutex_unlock(obj->lock);
}

void *ten_waitable_object_get(ten_waitable_object_t *obj) {
  void *ret = 0;

  if (!ten_waitable_object_valid(obj)) {
    return NULL;
  }

  ten_mutex_lock(obj->lock);
  ret = obj->v.obj;
  ten_mutex_unlock(obj->lock);

  return ret;
}

int ten_waitable_object_wait_until(ten_waitable_object_t *obj,
                                   int (*compare)(const void *l, const void *r),
                                   int timeout) {
  int ret = -1;

  if (!ten_waitable_object_valid(obj)) {
    return -1;
  }

  ten_mutex_lock(obj->lock);
  obj->compare = compare;
  ret = ten_cond_wait_while(obj->cond, obj->lock, ten_obj_is_not_equal, obj,
                            timeout);
  obj->compare = NULL;
  ten_mutex_unlock(obj->lock);
  return ret;
}

int ten_waitable_object_wait_while(ten_waitable_object_t *obj,
                                   int (*compare)(const void *l, const void *r),
                                   int timeout) {
  int ret = -1;

  if (!ten_waitable_object_valid(obj)) {
    return -1;
  }

  ten_mutex_lock(obj->lock);
  obj->compare = compare;
  ret =
      ten_cond_wait_while(obj->cond, obj->lock, ten_obj_is_equal, obj, timeout);
  obj->compare = NULL;
  ten_mutex_unlock(obj->lock);
  return ret;
}
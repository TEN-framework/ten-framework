//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/io/runloop.h"

#include <stdlib.h>
#include <string.h>

#include "ten_utils/io/general/loops/runloop.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/macro/memory.h"

typedef struct ten_runloop_bare_t {
  ten_runloop_common_t common;
} ten_runloop_bare_t;

typedef struct ten_runloop_async_bare_t {
  ten_runloop_async_common_t common;
  void (*notify_callback)(ten_runloop_async_t *);
  void (*close_callback)(ten_runloop_async_t *);
} ten_runloop_async_bare_t;

typedef struct ten_runloop_timer_bare_t {
  ten_runloop_timer_common_t common;

  bool initted;
  void (*notify_callback)(ten_runloop_timer_t *, void *);
  void (*stop_callback)(ten_runloop_timer_t *, void *);
  void (*close_callback)(ten_runloop_timer_t *, void *);
} ten_runloop_timer_bare_t;

static void ten_runloop_bare_destroy(ten_runloop_t *loop);
static void ten_runloop_bare_run(ten_runloop_t *loop);
static void *ten_runloop_bare_get_raw(ten_runloop_t *loop);
static void ten_runloop_bare_close(ten_runloop_t *loop);
static void ten_runloop_bare_stop(ten_runloop_t *loop);
static int ten_runloop_async_bare_init(ten_runloop_async_t *base,
                                       ten_runloop_t *loop,
                                       void (*callback)(ten_runloop_async_t *));
static void ten_runloop_async_bare_close(
    ten_runloop_async_t *base, void (*close_cb)(ten_runloop_async_t *));
static int ten_runloop_async_bare_notify(ten_runloop_async_t *base);
static void ten_runloop_async_bare_destroy(ten_runloop_async_t *base);
static int ten_runloop_bare_alive(ten_runloop_t *loop);

ten_runloop_common_t *ten_runloop_create_bare_common(TEN_UNUSED void *raw) {
  ten_runloop_bare_t *impl =
      (ten_runloop_bare_t *)TEN_MALLOC(sizeof(ten_runloop_bare_t));
  TEN_ASSERT(impl, "Failed to allocate memory.");
  if (!impl) {
    return NULL;
  }

  memset(impl, 0, sizeof(ten_runloop_bare_t));

  impl->common.base.impl = ten_strdup(TEN_RUNLOOP_BARE);

  impl->common.destroy = ten_runloop_bare_destroy;
  impl->common.run = ten_runloop_bare_run;
  impl->common.get_raw = ten_runloop_bare_get_raw;
  impl->common.close = ten_runloop_bare_close;
  impl->common.stop = ten_runloop_bare_stop;
  impl->common.alive = ten_runloop_bare_alive;

  return &impl->common;
}

ten_runloop_common_t *ten_runloop_create_bare(void) {
  return ten_runloop_create_bare_common(NULL);
}

ten_runloop_common_t *ten_runloop_attach_bare(void *raw) {
  return ten_runloop_create_bare_common(raw);
}

ten_runloop_async_common_t *ten_runloop_async_create_bare(void) {
  ten_runloop_async_bare_t *impl =
      (ten_runloop_async_bare_t *)TEN_MALLOC(sizeof(ten_runloop_async_bare_t));
  TEN_ASSERT(impl, "Failed to allocate memory.");
  if (!impl) {
    return NULL;
  }

  memset(impl, 0, sizeof(ten_runloop_async_bare_t));

  impl->common.base.impl = ten_strdup(TEN_RUNLOOP_BARE);
  impl->common.init = ten_runloop_async_bare_init;
  impl->common.close = ten_runloop_async_bare_close;
  impl->common.destroy = ten_runloop_async_bare_destroy;
  impl->common.notify = ten_runloop_async_bare_notify;

  return &impl->common;
}

static void ten_runloop_bare_destroy(ten_runloop_t *loop) {
  ten_runloop_bare_t *impl = (ten_runloop_bare_t *)loop;

  if (!loop || strcmp(loop->impl, TEN_RUNLOOP_BARE) != 0) {
    return;
  }

  ten_sanitizer_thread_check_deinit(&loop->thread_check);

  TEN_FREE(impl->common.base.impl);
  TEN_FREE(impl);
}

static void ten_runloop_bare_run(ten_runloop_t *loop) {
  ten_runloop_bare_t *impl = (ten_runloop_bare_t *)loop;

  if (!loop || strcmp(loop->impl, TEN_RUNLOOP_BARE) != 0) {
    return;
  }

  // no-op.
}

static void *ten_runloop_bare_get_raw(ten_runloop_t *loop) {
  ten_runloop_bare_t *impl = (ten_runloop_bare_t *)loop;

  if (!loop || strcmp(loop->impl, TEN_RUNLOOP_BARE) != 0) {
    return NULL;
  }

  return NULL;
}

static void ten_runloop_bare_close(ten_runloop_t *loop) {
  ten_runloop_bare_t *impl = (ten_runloop_bare_t *)loop;

  if (!loop || strcmp(loop->impl, TEN_RUNLOOP_BARE) != 0) {
    return;
  }
}

static void ten_runloop_bare_stop(ten_runloop_t *loop) {
  ten_runloop_bare_t *impl = (ten_runloop_bare_t *)loop;

  if (!loop || strcmp(loop->impl, TEN_RUNLOOP_BARE) != 0) {
    return;
  }

  // In bare runloop, the runloop is stopped completely here, call the
  // on_stopped callback if the user registered one before.
  if (impl->common.on_stopped) {
    impl->common.on_stopped((ten_runloop_t *)impl,
                            impl->common.on_stopped_data);
  }
}

static int ten_runloop_bare_alive(ten_runloop_t *loop) {
  ten_runloop_bare_t *impl = (ten_runloop_bare_t *)loop;

  if (!loop || strcmp(loop->impl, TEN_RUNLOOP_BARE) != 0) {
    return 0;
  }

  return 1;
}

static int ten_runloop_async_bare_init(
    ten_runloop_async_t *base, ten_runloop_t *loop,
    void (*notify_callback)(ten_runloop_async_t *)) {
  ten_runloop_async_bare_t *async_impl = (ten_runloop_async_bare_t *)base;
  ten_runloop_bare_t *loop_impl = (ten_runloop_bare_t *)loop;

  if (!base || strcmp(base->impl, TEN_RUNLOOP_BARE) != 0) {
    return -1;
  }

  if (!loop || strcmp(loop->impl, TEN_RUNLOOP_BARE) != 0) {
    return -1;
  }

  async_impl->notify_callback = notify_callback;

  return 0;
}

static void ten_runloop_async_bare_close(
    ten_runloop_async_t *base, void (*close_cb)(ten_runloop_async_t *)) {
  ten_runloop_async_bare_t *async_impl = (ten_runloop_async_bare_t *)base;

  if (!base || strcmp(base->impl, TEN_RUNLOOP_BARE) != 0) {
    return;
  }

  close_cb(base);
}

static void ten_runloop_async_bare_destroy(ten_runloop_async_t *base) {
  ten_runloop_async_bare_t *async_impl = (ten_runloop_async_bare_t *)base;

  if (!base || strcmp(base->impl, TEN_RUNLOOP_BARE) != 0) {
    return;
  }

  ten_sanitizer_thread_check_deinit(&base->thread_check);

  TEN_FREE(async_impl->common.base.impl);
  TEN_FREE(async_impl);
}

static int ten_runloop_async_bare_notify(ten_runloop_async_t *base) {
  ten_runloop_async_bare_t *async_impl = (ten_runloop_async_bare_t *)base;

  if (!base || strcmp(base->impl, TEN_RUNLOOP_BARE) != 0) {
    return -1;
  }

  return 0;
}

static int ten_runloop_timer_bare_start(
    ten_runloop_timer_t *base, ten_runloop_t *loop,
    void (*notify_callback)(ten_runloop_timer_t *, void *)) {
  ten_runloop_timer_bare_t *timer_impl = (ten_runloop_timer_bare_t *)base;
  ten_runloop_bare_t *loop_impl = (ten_runloop_bare_t *)loop;

  if (!base || strcmp(base->impl, TEN_RUNLOOP_BARE) != 0) {
    return -1;
  }

  if (!loop || strcmp(loop->impl, TEN_RUNLOOP_BARE) != 0) {
    return -1;
  }

  timer_impl->notify_callback = notify_callback;
  if (timer_impl->initted == false) {
    timer_impl->initted = true;
  }

  return 0;
}

static void ten_runloop_timer_bare_stop(ten_runloop_timer_t *base,
                                        void (*stop_cb)(ten_runloop_timer_t *,
                                                        void *)) {
  ten_runloop_timer_bare_t *timer_impl = (ten_runloop_timer_bare_t *)base;

  if (!base || strcmp(base->impl, TEN_RUNLOOP_BARE) != 0) {
    return;
  }

  timer_impl->stop_callback = stop_cb;

  if (timer_impl->stop_callback) {
    timer_impl->stop_callback(&timer_impl->common.base,
                              timer_impl->common.stop_data);
  }
}

static void ten_runloop_timer_bare_close(ten_runloop_timer_t *base,
                                         void (*close_cb)(ten_runloop_timer_t *,
                                                          void *)) {
  ten_runloop_timer_bare_t *timer_impl = (ten_runloop_timer_bare_t *)base;

  if (!base || strcmp(base->impl, TEN_RUNLOOP_BARE) != 0) {
    return;
  }

  close_cb(base, NULL);
}

static void ten_runloop_timer_bare_destroy(ten_runloop_timer_t *base) {
  ten_runloop_timer_bare_t *timer_impl = (ten_runloop_timer_bare_t *)base;

  if (!base || strcmp(base->impl, TEN_RUNLOOP_BARE) != 0) {
    return;
  }

  ten_sanitizer_thread_check_deinit(&base->thread_check);

  TEN_FREE(timer_impl->common.base.impl);
  TEN_FREE(timer_impl);
}

ten_runloop_timer_common_t *ten_runloop_timer_create_bare(void) {
  ten_runloop_timer_bare_t *impl =
      (ten_runloop_timer_bare_t *)TEN_MALLOC(sizeof(ten_runloop_timer_bare_t));
  TEN_ASSERT(impl, "Failed to allocate memory.");
  if (!impl) {
    return NULL;
  }

  memset(impl, 0, sizeof(ten_runloop_timer_bare_t));

  impl->initted = false;
  impl->common.base.impl = ten_strdup(TEN_RUNLOOP_BARE);
  impl->common.start = ten_runloop_timer_bare_start;
  impl->common.stop = ten_runloop_timer_bare_stop;
  impl->common.close = ten_runloop_timer_bare_close;
  impl->common.destroy = ten_runloop_timer_bare_destroy;

  return &impl->common;
}

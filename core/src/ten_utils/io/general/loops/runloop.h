//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "include_internal/ten_utils/io/runloop.h"
#include "ten_utils/container/list.h"
#include "ten_utils/io/runloop.h"
#include "ten_utils/lib/mutex.h"

typedef struct ten_runloop_common_t {
  ten_runloop_t base;

  uint64_t state;
  int destroying;
  ten_list_t tasks;
  ten_mutex_t *lock;
  ten_runloop_async_t *task_available_signal;

  bool attach_other;

  void (*destroy)(ten_runloop_t *);
  void (*run)(ten_runloop_t *);
  void (*close)(ten_runloop_t *);
  void (*stop)(ten_runloop_t *);
  void *(*get_raw)(ten_runloop_t *);
  int (*alive)(ten_runloop_t *);

  /**
   * @brief The callback function which will be called when the whole stop()
   * operations are fully completed. This enables the runloop users to perform
   * some actions that can only be triggered when the runloop stops completely.
   */
  ten_runloop_on_stopped_func_t on_stopped;
  void *on_stopped_data;
} ten_runloop_common_t;

typedef struct ten_runloop_async_common_t {
  ten_runloop_async_t base;

  int (*init)(ten_runloop_async_t *, ten_runloop_t *,
              void (*)(ten_runloop_async_t *));
  void (*close)(ten_runloop_async_t *, void (*)(ten_runloop_async_t *));
  void (*destroy)(ten_runloop_async_t *);
  int (*notify)(ten_runloop_async_t *);
} ten_runloop_async_common_t;

typedef struct ten_runloop_timer_common_t {
  ten_runloop_timer_t base;

  void *start_data;
  void *stop_data;
  void *close_data;

  int (*start)(ten_runloop_timer_t *, ten_runloop_t *,
               void (*)(ten_runloop_timer_t *, void *));
  void (*stop)(ten_runloop_timer_t *, void (*)(ten_runloop_timer_t *, void *));
  void (*close)(ten_runloop_timer_t *, void (*)(ten_runloop_timer_t *, void *));
  void (*destroy)(ten_runloop_timer_t *);
} ten_runloop_timer_common_t;

typedef struct ten_runloop_task_t {
  ten_listnode_t node;

  void (*func)(void *, void *);
  void *from;
  void *arg;
} ten_runloop_task_t;

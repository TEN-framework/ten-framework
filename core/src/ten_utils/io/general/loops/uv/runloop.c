//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/io/runloop.h"

#include <string.h>
#include <uv.h>

#include "include_internal/ten_utils/io/general/transport/backend/uv/stream/migrate.h"
#include "ten_utils/container/list.h"
#include "ten_utils/io/general/loops/runloop.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/mutex.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/field.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/macro/memory.h"
#include "ten_utils/sanitizer/thread_check.h"

// Runloop creates its own 'async' in the below 'migrate'.
typedef struct ten_migrate_task_t {
  ten_migrate_t *migrate;
} ten_migrate_task_t;

typedef struct ten_runloop_uv_t {
  ten_runloop_common_t common;

  uv_loop_t *uv_loop;

  // Protect the following 'migrate_tasks'.
  ten_mutex_t *migrate_task_lock;
  // The type of items is the above 'ten_migrate_task_t'.
  ten_list_t migrate_tasks;

  // Start to create 'async' in 'ten_migrate_task_t' in the 'destination'
  // thread/runloop.
  uv_async_t migrate_start_async;
} ten_runloop_uv_t;

typedef enum TEN_RUNLOOP_ASYNC_UV_STATE {
  TEN_RUNLOOP_ASYNC_UV_STATE_INIT,
  TEN_RUNLOOP_ASYNC_UV_STATE_CLOSING,
  TEN_RUNLOOP_ASYNC_UV_STATE_CLOSED,
} TEN_RUNLOOP_ASYNC_UV_STATE;

typedef struct ten_runloop_async_uv_t {
  ten_runloop_async_common_t common;

  TEN_RUNLOOP_ASYNC_UV_STATE state;
  uv_async_t uv_async;

  void (*notify_callback)(ten_runloop_async_t *);
  void (*close_callback)(ten_runloop_async_t *);
} ten_runloop_async_uv_t;

typedef struct ten_runloop_timer_uv_t {
  ten_runloop_timer_common_t common;

  uv_timer_t uv_timer;
  bool initted;

  void (*notify_callback)(ten_runloop_timer_t *, void *);
  void (*stop_callback)(ten_runloop_timer_t *, void *);
  void (*close_callback)(ten_runloop_timer_t *, void *);
} ten_runloop_timer_uv_t;

static void ten_runloop_uv_destroy(ten_runloop_t *loop);
static void ten_runloop_uv_run(ten_runloop_t *loop);
static void *ten_runloop_uv_get_raw(ten_runloop_t *loop);
static void ten_runloop_uv_close(ten_runloop_t *loop);
static void ten_runloop_uv_stop(ten_runloop_t *loop);
static int ten_runloop_async_uv_init(ten_runloop_async_t *base,
                                     ten_runloop_t *loop,
                                     void (*callback)(ten_runloop_async_t *));
static void ten_runloop_async_uv_close(ten_runloop_async_t *base,
                                       void (*close_cb)(ten_runloop_async_t *));
static int ten_runloop_async_uv_notify(ten_runloop_async_t *base);
static void ten_runloop_async_uv_destroy(ten_runloop_async_t *base);
static int ten_runloop_uv_alive(ten_runloop_t *loop);

// Start to create 'async' in 'ten_migrate_task_t'.
static void migrate_start_async_callback(uv_async_t *migrate_start_async) {
  TEN_ASSERT(migrate_start_async, "Invalid argument.");

  ten_runloop_uv_t *to_loop_impl = migrate_start_async->data;
  TEN_ASSERT(to_loop_impl &&
                 ten_runloop_check_integrity(&to_loop_impl->common.base, true),
             "Invalid argument.");

  ten_list_t tasks = TEN_LIST_INIT_VAL;

  {
    // Get all migration tasks at once.

    TEN_UNUSED int rc = ten_mutex_lock(to_loop_impl->migrate_task_lock);
    TEN_ASSERT(!rc, "Failed to lock.");

    ten_list_swap(&to_loop_impl->migrate_tasks, &tasks);

    rc = ten_mutex_unlock(to_loop_impl->migrate_task_lock);
    TEN_ASSERT(!rc, "Failed to unlock.");
  }

  // Handle each migration task one by one.
  ten_list_foreach (&tasks, iter) {
    ten_migrate_task_t *task = ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(task, "Invalid argument.");

    {
      // Initialize the sufficient 'async' resources in the migration task, and
      // all the 'dst_xxx' 'async' resources should belong to the 'to'
      // thread/runloop.

      // Ensure the 'async' is created in the correct (belonging) thread.
      ten_runloop_t *to_runloop = task->migrate->to;
      TEN_ASSERT(to_runloop && ten_runloop_check_integrity(to_runloop, true),
                 "Invalid argument.");

      TEN_UNUSED int rc =
          uv_async_init(migrate_start_async->loop, &task->migrate->dst_prepare,
                        migration_dst_prepare);
      TEN_ASSERT(!rc, "uv_async_init() failed: %d", rc);

      rc = uv_async_init(migrate_start_async->loop,
                         &task->migrate->dst_migration, migration_dst_start);
      TEN_ASSERT(!rc, "uv_async_init() failed: %d", rc);
    }

    ten_stream_migrate_uv_stage2(task->migrate);
  }

  ten_list_clear(&tasks);
}

static void free_task(void *task) { TEN_FREE(task); }

void ten_migrate_task_create_and_insert(ten_migrate_t *migrate) {
  TEN_ASSERT(migrate, "Invalid argument.");

  ten_runloop_uv_t *to_runloop = (ten_runloop_uv_t *)(migrate->to);
  TEN_ASSERT(to_runloop &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 // thread-check: This function is intended to be called outside
                 // of 'to_runloop' thread.
                 ten_runloop_check_integrity(&to_runloop->common.base, false),
             "Invalid argument.");

  ten_migrate_task_t *task =
      (ten_migrate_task_t *)TEN_MALLOC(sizeof(ten_migrate_task_t));
  TEN_ASSERT(task, "Failed to allocate memory.");

  task->migrate = migrate;

  {
    TEN_UNUSED int rc = ten_mutex_lock(to_runloop->migrate_task_lock);
    TEN_ASSERT(!rc, "Failed to lock.");

    ten_list_push_ptr_back(&to_runloop->migrate_tasks, task, free_task);

    rc = ten_mutex_unlock(to_runloop->migrate_task_lock);
    TEN_ASSERT(!rc, "Failed to unlock.");
  }

  // Kick 'to_runloop', so that the later operations are in the 'to'
  // thread.
  TEN_UNUSED int rc = uv_async_send(&to_runloop->migrate_start_async);
  TEN_ASSERT(!rc, "uv_async_send() failed: %d", rc);
}

/**
 * @brief Create sufficient resources to migrate 'stream' from one
 * thread/runloop to another. libuv is not a thread-safe library, so a 'stream'
 * must be migrated to the thread where uses it.
 */
static void ten_runloop_create_uv_migration_resource(ten_runloop_uv_t *impl) {
  TEN_ASSERT(impl, "Invalid argument.");

  impl->migrate_task_lock = ten_mutex_create();
  TEN_ASSERT(impl->migrate_task_lock, "Should not happen.");

  ten_list_init(&impl->migrate_tasks);

  TEN_UNUSED int rc = uv_async_init(impl->uv_loop, &impl->migrate_start_async,
                                    migrate_start_async_callback);
  TEN_ASSERT(!rc, "uv_async_init() failed: %d", rc);

  impl->migrate_start_async.data = impl;
}

static ten_runloop_common_t *ten_runloop_create_uv_common(void *raw) {
  ten_runloop_uv_t *impl =
      (ten_runloop_uv_t *)TEN_MALLOC(sizeof(ten_runloop_uv_t));
  if (!impl) {
    TEN_ASSERT(0, "Failed to allocate memory.");
    return NULL;
  }

  memset(impl, 0, sizeof(ten_runloop_uv_t));

  impl->common.base.impl = ten_strdup(TEN_RUNLOOP_UV);
  if (raw) {
    impl->uv_loop = raw;
  } else {
    impl->uv_loop = TEN_MALLOC(sizeof(uv_loop_t));
    TEN_ASSERT(impl->uv_loop, "Failed to allocate memory.");

    TEN_UNUSED int rc = uv_loop_init(impl->uv_loop);
    TEN_ASSERT(!rc, "uv_loop_init() failed: %d", rc);
  }

  ten_runloop_create_uv_migration_resource(impl);

  impl->common.destroy = ten_runloop_uv_destroy;
  impl->common.run = ten_runloop_uv_run;
  impl->common.get_raw = ten_runloop_uv_get_raw;
  impl->common.stop = ten_runloop_uv_stop;
  impl->common.alive = ten_runloop_uv_alive;

  return &impl->common;
}

ten_runloop_common_t *ten_runloop_create_uv(void) {
  return ten_runloop_create_uv_common(NULL);
}

ten_runloop_common_t *ten_runloop_attach_uv(void *raw) {
  return ten_runloop_create_uv_common(raw);
}

ten_runloop_async_common_t *ten_runloop_async_create_uv(void) {
  ten_runloop_async_uv_t *impl =
      (ten_runloop_async_uv_t *)TEN_MALLOC(sizeof(ten_runloop_async_uv_t));
  if (!impl) {
    TEN_ASSERT(0, "Failed to allocate memory.");
    return NULL;
  }

  memset(impl, 0, sizeof(ten_runloop_async_uv_t));

  impl->state = TEN_RUNLOOP_ASYNC_UV_STATE_INIT;

  impl->common.base.impl = ten_strdup(TEN_RUNLOOP_UV);
  impl->common.init = ten_runloop_async_uv_init;
  impl->common.close = ten_runloop_async_uv_close;
  impl->common.destroy = ten_runloop_async_uv_destroy;
  impl->common.notify = ten_runloop_async_uv_notify;

  return &impl->common;
}

static void ten_runloop_uv_destroy(ten_runloop_t *loop) {
  ten_runloop_uv_t *impl = (ten_runloop_uv_t *)loop;
  TEN_ASSERT(impl &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 // thread-check: This function is intended to be called in any
                 // threads.
                 ten_runloop_check_integrity(loop, false),
             "Invalid argument.");

  if (!loop || strcmp(loop->impl, TEN_RUNLOOP_UV) != 0) {
    return;
  }

  ten_sanitizer_thread_check_deinit(&loop->thread_check);

  TEN_FREE(impl->common.base.impl);
  TEN_FREE(impl);
}

/**
 * @brief Runs the libuv event loop until it's stopped.
 *
 * This function starts the libuv event loop and blocks until the loop is
 * stopped. After the loop is stopped, it cleans up resources by closing the
 * loop and freeing memory. If an `on_stopped` callback was registered, it will
 * be called after the loop has completely stopped.
 *
 * @param loop The runloop to run.
 */
static void ten_runloop_uv_run(ten_runloop_t *loop) {
  ten_runloop_uv_t *impl = (ten_runloop_uv_t *)loop;
  TEN_ASSERT(impl, "Invalid argument.");
  TEN_ASSERT(ten_runloop_check_integrity(loop, true), "Invalid argument.");

  if (!loop || strcmp(loop->impl, TEN_RUNLOOP_UV) != 0) {
    return;
  }

  // Run the libuv event loop until there are no more active handles or
  // requests.
  uv_run(impl->uv_loop, UV_RUN_DEFAULT);

  // Only free the loop if it wasn't attached from an external source.
  if (!impl->common.attach_other) {
    // Must call 'uv_loop_close()' after 'uv_run()' to release the internal
    // libuv loop resources, and check the return value of 'uv_loop_close' to
    // ensure that all resources relevant to the runloop are released.
    TEN_UNUSED int rc = uv_loop_close(impl->uv_loop);
    if (rc) {
      TEN_ASSERT(rc == UV_EBUSY, "uv_loop_close() failed: %d", rc);

      // Attempting to close a loop with active resources is problematic
      // because:
      // - It leads to resource leaks.
      // - It can cause undefined behavior if those resources are accessed
      //   later.
      // - It indicates a bug in the application's cleanup logic.
      TEN_ASSERT(0, "Runloop is destroyed when it holds alive resources.");
    }

    TEN_FREE(impl->uv_loop);
  }

  // The runloop is stopped completely, call the on_stopped callback if the
  // user registered one before.
  if (impl->common.on_stopped) {
    impl->common.on_stopped((ten_runloop_t *)impl,
                            impl->common.on_stopped_data);
  }
}

static void *ten_runloop_uv_get_raw(ten_runloop_t *loop) {
  ten_runloop_uv_t *impl = (ten_runloop_uv_t *)loop;
  TEN_ASSERT(impl &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 // thread-check: Refer to ten_runloop_get_raw(), this function
                 // is intended to be called in any threads.
                 ten_runloop_check_integrity(loop, false),
             "Invalid argument.");

  if (!loop || strcmp(loop->impl, TEN_RUNLOOP_UV) != 0) {
    return NULL;
  }

  return impl->uv_loop;
}

/**
 * @brief Callback invoked when the migration async handle is fully closed.
 *
 * This function is called after the migration async handle has been closed
 * as part of the runloop shutdown sequence. It performs different actions
 * based on whether the runloop was created internally or attached from an
 * external source:
 *
 * 1. For internally created runloops (!attach_other): Calls `uv_stop()` to
 *    stop the event loop, which will cause `uv_run()` to return.
 * 2. For externally attached runloops (attach_other): Does not stop the
 *    underlying loop (as we don't own it), but calls the `on_stopped` callback
 *    if one was registered.
 *
 * Finally, it cleans up migration-related resources by destroying the mutex
 * and clearing the migration tasks list.
 *
 * @param handle Pointer to the uv_handle_t (the migration async handle)
 *               that has been closed.
 */
static void ten_runloop_uv_migration_start_async_closed(uv_handle_t *handle) {
  TEN_ASSERT(handle, "Invalid argument.");

  ten_runloop_uv_t *impl = (ten_runloop_uv_t *)handle->data;
  TEN_ASSERT(impl, "Invalid argument.");
  TEN_ASSERT(ten_runloop_check_integrity(&impl->common.base, true),
             "Invalid argument.");

  if (!impl->common.attach_other) {
    // For internally created runloops, we need to stop the event loop which
    // will cause `uv_run()` to return in `ten_runloop_uv_run()`.
    uv_stop(impl->uv_loop);
  } else {
    // For externally attached runloops, we don't stop the loop (as we don't own
    // it), but we do call the `on_stopped` callback if one was registered.
    if (impl->common.on_stopped) {
      impl->common.on_stopped((ten_runloop_t *)impl,
                              impl->common.on_stopped_data);
    }
  }

  // Clean up migration-related resources.
  ten_mutex_destroy(impl->migrate_task_lock);
  ten_list_clear(&impl->migrate_tasks);
}

/**
 * @brief Stops the libuv-based runloop implementation.
 *
 * This function initiates the shutdown sequence for a libuv-based runloop.
 * It closes the migration async handle, which will trigger the
 * `ten_runloop_uv_migration_start_async_closed` callback when the handle
 * is fully closed. That callback will then either:
 * 1. Call `uv_stop()` if this is a runloop we created, or
 * 2. Call the `on_stopped` callback if this is an attached runloop.
 *
 * @param loop The runloop to stop.
 */
static void ten_runloop_uv_stop(ten_runloop_t *loop) {
  if (!loop || strcmp(loop->impl, TEN_RUNLOOP_UV) != 0) {
    return;
  }

  ten_runloop_uv_t *impl = (ten_runloop_uv_t *)loop;
  TEN_ASSERT(impl, "Invalid argument.");
  TEN_ASSERT(ten_runloop_check_integrity(loop, true), "Invalid argument.");

  // Close migration relevant resources.
  // This will trigger `ten_runloop_uv_migration_start_async_closed` when
  // complete, which will either stop the loop or call the `on_stopped`
  // callback.
  uv_close((uv_handle_t *)&impl->migrate_start_async,
           ten_runloop_uv_migration_start_async_closed);
}

static int ten_runloop_uv_alive(ten_runloop_t *loop) {
  ten_runloop_uv_t *impl = (ten_runloop_uv_t *)loop;
  TEN_ASSERT(impl && ten_runloop_check_integrity(loop, true),
             "Invalid argument.");

  if (!loop || strcmp(loop->impl, TEN_RUNLOOP_UV) != 0) {
    return 0;
  }

  return uv_loop_alive(impl->uv_loop);
}

static void uv_async_callback(uv_async_t *async) {
  if (!async) {
    TEN_ASSERT(0, "Invalid argument.");
    return;
  }

  ten_runloop_async_uv_t *async_impl =
      (ten_runloop_async_uv_t *)CONTAINER_OF_FROM_FIELD(
          async, ten_runloop_async_uv_t, uv_async);
  TEN_ASSERT(async_impl, "Invalid argument.");

  if (async_impl->notify_callback) {
    async_impl->notify_callback(&async_impl->common.base);
  }
}

static int ten_runloop_async_uv_init(
    ten_runloop_async_t *base, ten_runloop_t *loop,
    void (*notify_callback)(ten_runloop_async_t *)) {
  ten_runloop_async_uv_t *async_impl = (ten_runloop_async_uv_t *)base;
  ten_runloop_uv_t *loop_impl = (ten_runloop_uv_t *)loop;

  if (!base || strcmp(base->impl, TEN_RUNLOOP_UV) != 0) {
    return -1;
  }

  if (!loop || strcmp(loop->impl, TEN_RUNLOOP_UV) != 0) {
    return -1;
  }

  TEN_ASSERT(base && ten_runloop_async_check_integrity(base, true),
             "Invalid argument.");
  TEN_ASSERT(loop && ten_runloop_check_integrity(loop, true),
             "Invalid argument.");

  async_impl->notify_callback = notify_callback;
  int rc = uv_async_init(loop_impl->uv_loop, &async_impl->uv_async,
                         uv_async_callback);
  TEN_ASSERT(!rc, "uv_async_init() failed: %d", rc);

  return rc;
}

static void uv_async_closed(uv_handle_t *handle) {
  if (!handle) {
    TEN_ASSERT(0, "Invalid argument.");
    return;
  }

  ten_runloop_async_uv_t *async_impl =
      (ten_runloop_async_uv_t *)CONTAINER_OF_FROM_FIELD(
          handle, ten_runloop_async_uv_t, uv_async);
  TEN_ASSERT(async_impl, "Invalid argument.");

  async_impl->state = TEN_RUNLOOP_ASYNC_UV_STATE_CLOSED;

  if (!async_impl->close_callback) {
    return;
  }

  async_impl->common.base.loop = NULL;
  async_impl->close_callback(&async_impl->common.base);
}

static void ten_runloop_async_uv_close(
    ten_runloop_async_t *base, void (*close_cb)(ten_runloop_async_t *)) {
  if (!base) {
    TEN_ASSERT(0, "Invalid argument.");
    return;
  }

  if (strcmp(base->impl, TEN_RUNLOOP_UV) != 0) {
    TEN_ASSERT(0, "Invalid argument.");
    return;
  }

  TEN_ASSERT(ten_runloop_async_check_integrity(base, true),
             "Invalid argument.");

  ten_runloop_async_uv_t *async_impl = (ten_runloop_async_uv_t *)base;

  async_impl->close_callback = close_cb;
  async_impl->state = TEN_RUNLOOP_ASYNC_UV_STATE_CLOSING;

  uv_close((uv_handle_t *)&async_impl->uv_async, uv_async_closed);
}

static void ten_runloop_async_uv_destroy(ten_runloop_async_t *base) {
  if (!base) {
    TEN_ASSERT(0, "Invalid argument.");
    return;
  }

  if (strcmp(base->impl, TEN_RUNLOOP_UV) != 0) {
    TEN_ASSERT(0, "Invalid argument.");
    return;
  }

  TEN_ASSERT(ten_runloop_async_check_integrity(base, true),
             "Invalid argument.");

  ten_runloop_async_uv_t *async_impl = (ten_runloop_async_uv_t *)base;

  ten_sanitizer_thread_check_deinit(&base->thread_check);

  async_impl->common.init = NULL;
  async_impl->common.destroy = NULL;
  async_impl->common.close = NULL;
  async_impl->common.notify = NULL;

  async_impl->common.base.loop = NULL;
  async_impl->common.base.data = NULL;

  TEN_FREE(async_impl->common.base.impl);

  async_impl->state = TEN_RUNLOOP_ASYNC_UV_STATE_INIT;
  async_impl->notify_callback = NULL;
  async_impl->close_callback = NULL;

  TEN_FREE(async_impl);
}

static int ten_runloop_async_uv_notify(ten_runloop_async_t *base) {
  if (!base) {
    TEN_ASSERT(0, "Invalid argument.");
    return -1;
  }

  if (strcmp(base->impl, TEN_RUNLOOP_UV) != 0) {
    TEN_ASSERT(0, "Invalid argument.");
    return -1;
  }

  TEN_ASSERT(
      // TEN_NOLINTNEXTLINE(thread-check)
      // thread-check: This function is intended to be called in any
      // threads.
      ten_runloop_async_check_integrity(base, false), "Invalid argument.");

  ten_runloop_async_uv_t *async_impl = (ten_runloop_async_uv_t *)base;

  if (async_impl->state >= TEN_RUNLOOP_ASYNC_UV_STATE_CLOSING) {
    TEN_ASSERT(0, "The async is closing.");
    return -1;
  }

  int rc = uv_async_send(&async_impl->uv_async);
  TEN_ASSERT(!rc, "uv_async_send() failed: %d", rc);

  return rc;
}

static void uv_timer_callback(uv_timer_t *handle) {
  if (!handle) {
    TEN_ASSERT(0, "Invalid argument.");
    return;
  }

  ten_runloop_timer_uv_t *timer_impl =
      (ten_runloop_timer_uv_t *)CONTAINER_OF_FROM_FIELD(
          handle, ten_runloop_timer_uv_t, uv_timer);
  TEN_ASSERT(timer_impl, "Invalid argument.");

  if (timer_impl->notify_callback) {
    timer_impl->notify_callback(&timer_impl->common.base,
                                timer_impl->common.start_data);
  }
}

static int ten_runloop_timer_uv_start(
    ten_runloop_timer_t *base, ten_runloop_t *loop,
    void (*notify_callback)(ten_runloop_timer_t *, void *)) {
  ten_runloop_timer_uv_t *timer_impl = (ten_runloop_timer_uv_t *)base;
  ten_runloop_uv_t *loop_impl = (ten_runloop_uv_t *)loop;

  if (!base || strcmp(base->impl, TEN_RUNLOOP_UV) != 0) {
    return -1;
  }

  if (!loop || strcmp(loop->impl, TEN_RUNLOOP_UV) != 0) {
    return -1;
  }

  TEN_ASSERT(ten_runloop_timer_check_integrity(base, true),
             "Invalid argument.");
  TEN_ASSERT(loop && ten_runloop_check_integrity(loop, true),
             "Invalid argument.");

  timer_impl->notify_callback = notify_callback;
  if (timer_impl->initted == false) {
    int rc = uv_timer_init(loop_impl->uv_loop, &timer_impl->uv_timer);
    if (rc != 0) {
      TEN_ASSERT(!rc, "uv_timer_init() failed: %d", rc);
      return -1;
    }
    timer_impl->initted = true;
  }

  int rc = uv_timer_start(&timer_impl->uv_timer, uv_timer_callback,
                          base->timeout, base->periodic);
  TEN_ASSERT(!rc, "uv_timer_start() failed: %d", rc);

  return rc;
}

static void uv_timer_closed(uv_handle_t *handle) {
  if (!handle) {
    TEN_ASSERT(0, "Invalid argument.");
    return;
  }

  ten_runloop_timer_uv_t *timer_impl =
      (ten_runloop_timer_uv_t *)CONTAINER_OF_FROM_FIELD(
          handle, ten_runloop_timer_uv_t, uv_timer);
  TEN_ASSERT(timer_impl, "Invalid argument.");

  if (!timer_impl->close_callback) {
    return;
  }

  timer_impl->common.base.loop = NULL;
  if (timer_impl->close_callback) {
    timer_impl->close_callback(&timer_impl->common.base,
                               timer_impl->common.close_data);
  }
}

/**
 * @brief Stops a libuv-based timer.
 *
 * This function stops a running timer by calling `uv_timer_stop()` on the
 * underlying libuv timer handle. After stopping the timer, it invokes the
 * provided stop callback with the timer and the stored stop data.
 *
 * @param base The timer to stop.
 * @param stop_cb Callback function to be called after the timer is stopped.
 *                Can be NULL if no callback is needed.
 */
static void ten_runloop_timer_uv_stop(ten_runloop_timer_t *base,
                                      void (*stop_cb)(ten_runloop_timer_t *,
                                                      void *)) {
  TEN_ASSERT(base, "Invalid argument.");
  TEN_ASSERT(ten_runloop_timer_check_integrity(base, true),
             "Invalid argument.");

  if (!base || strcmp(base->impl, TEN_RUNLOOP_UV) != 0) {
    return;
  }

  ten_runloop_timer_uv_t *timer_impl = (ten_runloop_timer_uv_t *)base;
  timer_impl->stop_callback = stop_cb;

  TEN_UNUSED int rc = uv_timer_stop(&timer_impl->uv_timer);
  TEN_ASSERT(!rc, "uv_timer_stop() failed: %d", rc);

  if (timer_impl->stop_callback) {
    timer_impl->stop_callback(&timer_impl->common.base,
                              timer_impl->common.stop_data);
  }
}

/**
 * @brief Closes a libuv-based timer.
 *
 * This function initiates the asynchronous closing of a timer by calling
 * `uv_close()` on the underlying libuv timer handle. The actual callback
 * (`uv_timer_closed`) will be invoked by libuv when the handle is fully closed,
 * which will then call the provided `close_cb` with the timer and the stored
 * close data.
 *
 * Note: This function does not immediately destroy the timer. The timer will be
 * properly cleaned up when the close callback is invoked.
 *
 * @param base The timer to close.
 * @param close_cb Callback function to be called after the timer is fully
 * closed. Can be NULL if no callback is needed.
 */
static void ten_runloop_timer_uv_close(ten_runloop_timer_t *base,
                                       void (*close_cb)(ten_runloop_timer_t *,
                                                        void *)) {
  TEN_ASSERT(base, "Invalid argument.");
  TEN_ASSERT(ten_runloop_timer_check_integrity(base, true),
             "Invalid argument.");

  if (!base || strcmp(base->impl, TEN_RUNLOOP_UV) != 0) {
    return;
  }

  ten_runloop_timer_uv_t *timer_impl = (ten_runloop_timer_uv_t *)base;
  timer_impl->close_callback = close_cb;

  uv_close((uv_handle_t *)&timer_impl->uv_timer, uv_timer_closed);
}

static void ten_runloop_timer_uv_destroy(ten_runloop_timer_t *base) {
  ten_runloop_timer_uv_t *timer_impl = (ten_runloop_timer_uv_t *)base;

  if (!base || strcmp(base->impl, TEN_RUNLOOP_UV) != 0) {
    return;
  }

  TEN_ASSERT(ten_runloop_timer_check_integrity(base, true),
             "Invalid argument.");

  ten_sanitizer_thread_check_deinit(&base->thread_check);

  TEN_FREE(timer_impl->common.base.impl);
  TEN_FREE(timer_impl);
}

ten_runloop_timer_common_t *ten_runloop_timer_create_uv(void) {
  ten_runloop_timer_uv_t *impl =
      (ten_runloop_timer_uv_t *)TEN_MALLOC(sizeof(ten_runloop_timer_uv_t));
  TEN_ASSERT(impl, "Failed to allocate memory.");
  if (!impl) {
    return NULL;
  }

  memset(impl, 0, sizeof(ten_runloop_timer_uv_t));

  impl->initted = false;

  impl->common.base.impl = ten_strdup(TEN_RUNLOOP_UV);
  impl->common.start = ten_runloop_timer_uv_start;
  impl->common.stop = ten_runloop_timer_uv_stop;
  impl->common.close = ten_runloop_timer_uv_close;
  impl->common.destroy = ten_runloop_timer_uv_destroy;

  return &impl->common;
}

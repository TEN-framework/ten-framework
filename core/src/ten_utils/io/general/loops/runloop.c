//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/io/runloop.h"

#include <stdint.h>
#include <stdlib.h>

#include "ten_utils/container/list.h"
#include "ten_utils/io/general/loops/runloop.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/atomic.h"
#include "ten_utils/lib/mutex.h"
#include "ten_utils/lib/thread_local.h"
#include "ten_utils/lib/thread_once.h"
#include "ten_utils/log/log.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/field.h"
#include "ten_utils/macro/mark.h"

static ten_thread_once_t runloop_once = TEN_THREAD_ONCE_INIT;
static ten_thread_key_t runloop_key = kInvalidTlsKey;

static void setup_runloop_callback(void) {
  runloop_key = ten_thread_key_create();
}

static int set_self(ten_runloop_t *self) {
  ten_thread_key_t key = runloop_key;

  if (key == kInvalidTlsKey) {
    TEN_LOGE("Failed to set the runloop pointer to the thread local storage.");
    return -1;
  }

  return ten_thread_set_key(key, self);
}

static ten_runloop_t *get_self(void) {
  ten_thread_key_t key = runloop_key;

  if (key == kInvalidTlsKey) {
    TEN_LOGE(
        "Failed to get the runloop pointer from the thread local storage.");
    return NULL;
  }

  return ten_thread_get_key(key);
}

#if defined(TEN_USE_LIBUV)
extern ten_runloop_common_t *ten_runloop_create_uv(void);
extern ten_runloop_common_t *ten_runloop_attach_uv(void *);
extern ten_runloop_async_common_t *ten_runloop_async_create_uv(void);
extern ten_runloop_timer_common_t *ten_runloop_timer_create_uv(void);
#endif

#if defined(TEN_USE_LIBEVENT)
extern ten_runloop_common_t *ten_runloop_create_event(void);
extern ten_runloop_common_t *ten_runloop_attach_event(void *);
extern ten_runloop_async_common_t *ten_runloop_async_create_event(void);
extern ten_runloop_timer_common_t *ten_runloop_timer_create_event(void);
#endif

#if defined(TEN_USE_BARE_RUNLOOP)
extern ten_runloop_common_t *ten_runloop_create_bare(void);
extern ten_runloop_common_t *ten_runloop_attach_bare(void *);
extern ten_runloop_async_common_t *ten_runloop_async_create_bare(void);
extern ten_runloop_timer_common_t *ten_runloop_timer_create_bare(void);
#endif

typedef struct runloop_factory_t {
  const char *impl;
  ten_runloop_common_t *(*create_runloop)(void);
  ten_runloop_common_t *(*attach)(void *raw);
  ten_runloop_async_common_t *(*create_async)(void);
  ten_runloop_timer_common_t *(*create_timer)(void);
} runloop_factory_t;

static const runloop_factory_t runloop_factory[] = {
#if defined(TEN_USE_LIBUV)
    // libuv is the default runloop, put it in the first element.
    {
        TEN_RUNLOOP_UV,
        ten_runloop_create_uv,
        ten_runloop_attach_uv,
        ten_runloop_async_create_uv,
        ten_runloop_timer_create_uv,
    },
#endif
#if defined(TEN_USE_LIBEVENT)
    {
        TEN_RUNLOOP_EVENT2,
        ten_runloop_create_event,
        ten_runloop_attach_event,
        ten_runloop_async_create_event,
        ten_runloop_timer_create_event,
    },
#endif
#if defined(TEN_USE_BARE_RUNLOOP)
    {
        TEN_RUNLOOP_BARE,
        ten_runloop_create_bare,
        ten_runloop_attach_bare,
        ten_runloop_async_create_bare,
        ten_runloop_timer_create_bare,
    },
#endif
    {
        NULL,
        NULL,
        NULL,
        NULL,
    }};

#define RUNLOOP_FACTORY_SIZE \
  (sizeof(runloop_factory) / sizeof(runloop_factory[0]))

static const char *get_default_impl(void) { return runloop_factory[0].impl; }

bool ten_runloop_check_integrity(ten_runloop_t *self, bool check_thread) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_RUNLOOP_SIGNATURE) {
    return false;
  }

  if (check_thread &&
      !ten_sanitizer_thread_check_do_check(&self->thread_check)) {
    return false;
  }

  return true;
}

bool ten_runloop_async_check_integrity(ten_runloop_async_t *self,
                                       bool check_thread) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_RUNLOOP_ASYNC_SIGNATURE) {
    return false;
  }

  if (check_thread &&
      !ten_sanitizer_thread_check_do_check(&self->thread_check)) {
    return false;
  }

  return true;
}

bool ten_runloop_timer_check_integrity(ten_runloop_timer_t *self,
                                       bool check_thread) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_RUNLOOP_TIMER_SIGNATURE) {
    return false;
  }

  if (check_thread &&
      !ten_sanitizer_thread_check_do_check(&self->thread_check)) {
    return false;
  }

  return true;
}

static const runloop_factory_t *get_runloop_factory(const char *name) {
  if (!name) {
    return NULL;
  }

  for (int i = 0; i < RUNLOOP_FACTORY_SIZE; i++) {
    if (!runloop_factory[i].impl) {
      continue;
    }

    if (strcmp(name, runloop_factory[i].impl) != 0) {
      continue;
    }

    return &runloop_factory[i];
  }

  return NULL;
}

static void process_remaining_tasks_safe(ten_runloop_common_t *loop) {
  TEN_ASSERT(loop, "Invalid argument.");
  TEN_ASSERT(ten_runloop_check_integrity(&loop->base, true),
             "Invalid argument.");

  while (!ten_list_is_empty(&loop->tasks)) {
    ten_listnode_t *itor = ten_list_pop_front(&loop->tasks);
    TEN_ASSERT(itor, "Invalid argument.");

    ten_runloop_task_t *task = (ten_runloop_task_t *)CONTAINER_OF_FROM_FIELD(
        itor, ten_runloop_task_t, node);

    if (task->func) {
      TEN_UNUSED bool rc = ten_mutex_unlock(loop->lock);
      TEN_ASSERT(!rc, "Failed to unlock.");

      task->func(task->from, task->arg);

      rc = ten_mutex_lock(loop->lock);
      TEN_ASSERT(!rc, "Failed to lock.");
    }

    TEN_FREE(task);
  }
}

static void flush_remaining_tasks(ten_runloop_common_t *impl) {
  TEN_ASSERT(impl, "Invalid argument.");
  TEN_ASSERT(ten_runloop_check_integrity(&impl->base, true),
             "Invalid argument.");

  TEN_UNUSED bool rc = ten_mutex_lock(impl->lock);
  TEN_ASSERT(!rc, "Failed to lock.");

  process_remaining_tasks_safe(impl);

  rc = ten_mutex_unlock(impl->lock);
  TEN_ASSERT(!rc, "Failed to unlock.");
}

static void task_available_callback(ten_runloop_async_t *async) {
  TEN_ASSERT(async, "Invalid argument.");
  TEN_ASSERT(ten_runloop_async_check_integrity(async, true),
             "Invalid argument.");

  ten_runloop_common_t *impl = (ten_runloop_common_t *)async->data;
  TEN_ASSERT(impl, "Invalid argument.");
  TEN_ASSERT(ten_runloop_check_integrity(&impl->base, true),
             "Invalid argument.");

  flush_remaining_tasks(impl);
}

/**
 * @brief Callback invoked when the `task_available_signal` async handle is
 * fully closed.
 *
 * This function is called as part of the runloop shutdown sequence after the
 * `task_available_signal` async handle has been closed.
 *
 * @param async Pointer to the async handle that has been closed.
 */
static void task_available_signal_closed(ten_runloop_async_t *async) {
  TEN_ASSERT(async, "Invalid argument.");
  TEN_ASSERT(ten_runloop_async_check_integrity(async, true),
             "Invalid argument.");

  ten_runloop_common_t *impl = (ten_runloop_common_t *)async->data;
  TEN_ASSERT(impl, "Invalid argument.");
  TEN_ASSERT(ten_runloop_check_integrity(&impl->base, true),
             "Invalid argument.");

  // After the signal is closed, we can ensure that no more new tasks will be
  // added to the task queue, so we can safely consume all remaining tasks.
  task_available_callback(async);

  // Verify all tasks have been processed. If this assertion fails, it indicates
  // a bug where tasks were added after processing or the processing logic
  // failed.
  TEN_ASSERT(ten_list_is_empty(&impl->tasks),
             "Task queue should be empty after processing remaining tasks.");

  // Call the implementation-specific stop function if available.
  if (impl->stop) {
    impl->stop(&impl->base);
  }

  // Clean up resources.
  ten_runloop_async_destroy(async);
  impl->task_available_signal = NULL;
}

static void runloop_init(ten_runloop_common_t *impl, bool attached) {
  TEN_ASSERT(impl, "Invalid argument.");

  ten_signature_set(&impl->base.signature, TEN_RUNLOOP_SIGNATURE);
  ten_sanitizer_thread_check_init_with_current_thread(&impl->base.thread_check);

  ten_atomic_store((ten_atomic_t *)&impl->state, TEN_RUNLOOP_STATE_IDLE);
  impl->attach_other = attached;
  ten_list_init(&impl->tasks);
  impl->lock = ten_mutex_create();
  impl->task_available_signal = ten_runloop_async_create(impl->base.impl);
  impl->task_available_signal->data = impl;
  ten_runloop_async_init(impl->task_available_signal, &impl->base,
                         task_available_callback);
}

ten_runloop_t *ten_runloop_create(const char *type) {
  const char *name = type ? type : get_default_impl();
  ten_runloop_common_t *impl = NULL;
  const runloop_factory_t *factory = NULL;

  ten_thread_once(&runloop_once, setup_runloop_callback);

  factory = get_runloop_factory(name);
  if (!factory || !factory->create_runloop) {
    return NULL;
  }

  impl = factory->create_runloop();
  TEN_ASSERT(impl, "Failed to create %s runloop implementation.", name);
  if (!impl) {
    return NULL;
  }

  runloop_init(impl, false);

  return &impl->base;
}

void ten_runloop_destroy(ten_runloop_t *loop) {
  TEN_ASSERT(loop, "Invalid argument.");
  TEN_ASSERT(
      // TEN_NOLINTNEXTLINE(thread-check)
      // thread-check: destroying might be occurred in any threads.
      ten_runloop_check_integrity(loop, false), "Invalid argument.");

  ten_runloop_common_t *impl = (ten_runloop_common_t *)loop;
  if (!loop) {
    return;
  }

  TEN_ASSERT(!impl->task_available_signal, "Should not happen.");

  ten_mutex_destroy(impl->lock);
  impl->lock = NULL;

  if (impl->destroy) {
    impl->destroy(&impl->base);
  }
}

ten_runloop_t *ten_runloop_current(void) { return get_self(); }

/**
 * @brief Attaches to an existing runloop with a "raw loop pointer"
 *
 * This function creates a ten_runloop_t wrapper around an existing loop
 * implementation (like libuv, libevent, etc.) that was created elsewhere.
 * The runloop will not take ownership of the raw loop, so the caller is
 * responsible for its lifecycle.
 *
 * @param type The implementation type of the runloop (e.g., "uv", "event2").
 *             If NULL, uses the default implementation.
 * @param raw The "raw loop pointer" to attach to (e.g., uv_loop_t* for "uv").
 *            Must not be NULL and must match the specified type.
 * @return The attached runloop wrapper, or NULL if attachment failed.
 *
 * @note Be careful that |raw| must be of the correct type specified by |type|,
 *       otherwise undefined behavior may occur.
 */
ten_runloop_t *ten_runloop_attach(const char *type, void *raw) {
  TEN_ASSERT(raw, "Raw loop pointer cannot be NULL");

  const char *name = type ? type : get_default_impl();
  ten_runloop_common_t *impl = NULL;
  const runloop_factory_t *factory = NULL;

  ten_thread_once(&runloop_once, setup_runloop_callback);

  factory = get_runloop_factory(name);
  if (!factory || !factory->attach) {
    TEN_LOGE("No attach function found for runloop type: %s", name);
    return NULL;
  }

  impl = factory->attach(raw);
  if (!impl) {
    TEN_LOGE("Failed to attach to raw loop of type: %s", name);
    return NULL;
  }

  runloop_init(impl, true);

  return &impl->base;
}

bool ten_runloop_is_attached(ten_runloop_t *loop) {
  ten_runloop_common_t *impl = (ten_runloop_common_t *)loop;
  TEN_ASSERT(impl && ten_runloop_check_integrity(loop, true),
             "Invalid argument.");

  return impl->attach_other;
}

void *ten_runloop_get_raw(ten_runloop_t *loop) {
  ten_runloop_common_t *impl = (ten_runloop_common_t *)loop;
  TEN_ASSERT(impl &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 // thread-check: This function is intended to be called in any
                 // threads.
                 ten_runloop_check_integrity(loop, false),
             "Invalid argument.");

  void *raw = NULL;

  ten_mutex_lock(impl->lock);

  if (!loop || !impl->get_raw) {
    goto done;
  }
  raw = impl->get_raw(loop);

done:
  ten_mutex_unlock(impl->lock);
  return raw;
}

void ten_runloop_run(ten_runloop_t *loop) {
  TEN_ASSERT(loop && ten_runloop_check_integrity(loop, true),
             "Invalid argument.");

  ten_runloop_common_t *impl = (ten_runloop_common_t *)loop;

  // If the underlying is created separately, it will start running by itself,
  // so we do _not_ need to enable it to run here.
  if (!loop || !impl->run || impl->attach_other) {
    return;
  }

  set_self(loop);
  ten_atomic_store((ten_atomic_t *)&impl->state, TEN_RUNLOOP_STATE_RUNNING);

  impl->run(loop);

  ten_atomic_store((ten_atomic_t *)&impl->state, TEN_RUNLOOP_STATE_IDLE);
  set_self(NULL);
}

/**
 * @brief This function is used to 'release' the resources occupied by the
 * runloop internally, therefore, we should _not_ close the runloop before it
 * stops.
 */
void ten_runloop_close(ten_runloop_t *loop) {
  TEN_ASSERT(loop && ten_runloop_check_integrity(loop, true),
             "Invalid argument.");

  ten_runloop_common_t *impl = (ten_runloop_common_t *)loop;

  if (!loop || !impl->close) {
    return;
  }
  impl->close(loop);
}

/**
 * @brief Stops the runloop and initiates the shutdown sequence.
 *
 * This function stops the runloop by:
 * 1. Setting the 'destroying' flag to prevent new tasks from being processed.
 * 2. Closing the task_available_signal async handle, which will trigger
 *    `task_available_signal_closed` callback when fully closed.
 *
 * The `task_available_signal_closed` callback will:
 * - Process any remaining tasks in the queue.
 * - Call the implementation-specific stop function.
 * - Destroy the async handle.
 *
 * @param loop The runloop to stop. Must be called from the runloop's thread.
 */
void ten_runloop_stop(ten_runloop_t *loop) {
  TEN_ASSERT(loop, "Invalid argument.");
  TEN_ASSERT(ten_runloop_check_integrity(loop, true), "Invalid argument.");

  ten_runloop_common_t *impl = (ten_runloop_common_t *)loop;
  if (!loop || !impl->stop) {
    return;
  }

  // Lock to safely update the destroying flag.
  TEN_UNUSED int rc = ten_mutex_lock(impl->lock);
  TEN_ASSERT(!rc, "Failed to lock.");

  impl->destroying = 1;

  rc = ten_mutex_unlock(impl->lock);
  TEN_ASSERT(!rc, "Failed to unlock.");

  // Close the async handle, which will trigger `task_available_signal_closed`
  // when fully closed.
  ten_runloop_async_close(impl->task_available_signal,
                          task_available_signal_closed);
}

void ten_runloop_set_on_stopped(ten_runloop_t *loop,
                                ten_runloop_on_stopped_func_t on_stopped,
                                void *on_stopped_data) {
  TEN_ASSERT(loop && ten_runloop_check_integrity(loop, true),
             "Invalid argument.");

  ten_runloop_common_t *impl = (ten_runloop_common_t *)loop;
  impl->on_stopped = on_stopped;
  impl->on_stopped_data = on_stopped_data;
}

int ten_runloop_alive(ten_runloop_t *loop) {
  TEN_ASSERT(loop && ten_runloop_check_integrity(loop, true),
             "Invalid argument.");

  ten_runloop_common_t *impl = (ten_runloop_common_t *)loop;

  if (!impl || !impl->alive) {
    return 0;
  }

  return impl->alive(loop);
}

ten_runloop_async_t *ten_runloop_async_create(const char *type) {
  const char *name = type ? type : get_default_impl();
  ten_runloop_async_common_t *impl = NULL;
  const runloop_factory_t *factory = NULL;

  ten_thread_once(&runloop_once, setup_runloop_callback);

  factory = get_runloop_factory(name);
  if (!factory || !factory->create_async) {
    return NULL;
  }

  impl = factory->create_async();
  TEN_ASSERT(impl, "Failed to create %s async.", name);
  if (!impl) {
    return NULL;
  }

  impl->base.loop = NULL;

  ten_signature_set(&impl->base.signature, TEN_RUNLOOP_ASYNC_SIGNATURE);
  ten_sanitizer_thread_check_init_with_current_thread(&impl->base.thread_check);

  return &impl->base;
}

void ten_runloop_async_close(ten_runloop_async_t *async,
                             void (*close_cb)(ten_runloop_async_t *)) {
  TEN_ASSERT(async, "Invalid argument.");
  TEN_ASSERT(ten_runloop_async_check_integrity(async, true),
             "Invalid argument.");

  ten_runloop_async_common_t *impl = (ten_runloop_async_common_t *)async;

  if (!async || !impl->close) {
    return;
  }

  impl->close(async, close_cb);
}

void ten_runloop_async_destroy(ten_runloop_async_t *async) {
  TEN_ASSERT(async, "Invalid argument.");
  TEN_ASSERT(ten_runloop_async_check_integrity(async, true),
             "Invalid argument.");

  ten_runloop_async_common_t *impl = (ten_runloop_async_common_t *)async;

  if (!async || !impl->destroy) {
    return;
  }

  impl->destroy(async);
}

int ten_runloop_async_notify(ten_runloop_async_t *async) {
  TEN_ASSERT(async, "Invalid argument.");
  TEN_ASSERT(
      // TEN_NOLINTNEXTLINE(thread-check)
      // thread-check: This function is intended to be called in any
      // threads.
      ten_runloop_async_check_integrity(async, false), "Invalid argument.");

  ten_runloop_async_common_t *impl = (ten_runloop_async_common_t *)async;

  if (!async || !impl->notify) {
    return -1;
  }

  return impl->notify(async);
}

int ten_runloop_async_init(ten_runloop_async_t *async, ten_runloop_t *loop,
                           void (*callback)(ten_runloop_async_t *)) {
  TEN_ASSERT(async, "Invalid argument.");
  TEN_ASSERT(ten_runloop_async_check_integrity(async, true),
             "Invalid argument.");
  TEN_ASSERT(loop && ten_runloop_check_integrity(loop, true),
             "Invalid argument.");

  ten_runloop_async_common_t *impl = (ten_runloop_async_common_t *)async;
  int ret = -1;

  if (!async || !impl->init) {
    return -1;
  }

  if (impl->base.loop) {
    return -1;
  }

  if (strcmp(async->impl, loop->impl) != 0) {
    return -1;
  }

  ret = impl->init(async, loop, callback);
  if (ret == 0) {
    async->loop = loop;
  }

  return ret;
}

static int ten_runloop_post_task_at(ten_runloop_t *loop,
                                    void (*task_cb)(void *, void *), void *from,
                                    void *arg, int front) {
  TEN_ASSERT(loop, "Invalid argument.");
  TEN_ASSERT(
      // TEN_NOLINTNEXTLINE(thread-check)
      // thread-check: This function is intended to be called in any
      // threads.
      ten_runloop_check_integrity(loop, false), "Invalid argument.");

  ten_runloop_common_t *impl = (ten_runloop_common_t *)loop;
  ten_runloop_task_t *task = NULL;
  int needs_notify = 0;

  int result = 0;

  if (!loop || !task_cb) {
    TEN_ASSERT(0, "Invalid argument.");
    result = -1;
    goto error;
  }

  task = (ten_runloop_task_t *)TEN_MALLOC(sizeof(ten_runloop_task_t));
  if (!task) {
    TEN_ASSERT(0, "Failed to allocate memory.");
    result = -1;
    goto error;
  }

  memset(task, 0, sizeof(ten_runloop_task_t));
  task->func = task_cb;
  task->from = from;
  task->arg = arg;

  TEN_UNUSED bool rc = ten_mutex_lock(impl->lock);
  TEN_ASSERT(!rc, "Failed to lock.");

  if (impl->destroying) {
    // The runloop has started to close, so we do _not_ add any more new tasks
    // into it.
    result = -2;
    goto leave_and_error;
  }

#if defined(_DEBUG)
  // Add some random delays in debug mode to test different timings.
  ten_random_sleep_range_ms(0, 20);
#endif

  needs_notify = ten_list_is_empty(&impl->tasks) ? 1 : 0;
  if (front) {
    ten_list_push_front(&impl->tasks, &task->node);
  } else {
    ten_list_push_back(&impl->tasks, &task->node);
  }

  if (needs_notify) {
    ten_runloop_async_notify(impl->task_available_signal);
  }

  rc = ten_mutex_unlock(impl->lock);
  TEN_ASSERT(!rc, "Failed to unlock.");

#if defined(_DEBUG)
  // Add some random delays in debug mode to test different timings.
  ten_random_sleep_range_ms(0, 20);
#endif

  return result;

leave_and_error:
  rc = ten_mutex_unlock(impl->lock);
  TEN_ASSERT(!rc, "Failed to unlock.");

error:
  if (task) {
    TEN_FREE(task);
  }

  return result;
}

int ten_runloop_post_task_front(ten_runloop_t *loop,
                                ten_runloop_task_func_t task_cb, void *from,
                                void *arg) {
  TEN_ASSERT(loop &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 // thread-check: This function is intended to be called in any
                 // threads.
                 ten_runloop_check_integrity(loop, false),
             "Invalid argument.");
  return ten_runloop_post_task_at(loop, task_cb, from, arg, 1);
}

int ten_runloop_post_task_tail(ten_runloop_t *loop,
                               ten_runloop_task_func_t task_cb, void *from,
                               void *arg) {
  TEN_ASSERT(loop &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 // thread-check: This function is intended to be called in any
                 // threads.
                 ten_runloop_check_integrity(loop, false),
             "Invalid argument.");
  return ten_runloop_post_task_at(loop, task_cb, from, arg, 0);
}

size_t ten_runloop_task_queue_size(ten_runloop_t *loop) {
  TEN_ASSERT(loop &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 // thread-check: This function is intended to be called in any
                 // threads.
                 ten_runloop_check_integrity(loop, false),
             "Invalid argument.");

  ten_runloop_common_t *impl = (ten_runloop_common_t *)loop;

  if (!loop) {
    return -1;
  }

  ten_mutex_lock(impl->lock);
  size_t size = ten_list_size(&impl->tasks);
  ten_mutex_unlock(impl->lock);

  return size;
}

void ten_runloop_flush_task(ten_runloop_t *loop) {
  TEN_ASSERT(loop &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 // thread-check: This function is intended to be called in any
                 // threads.
                 ten_runloop_check_integrity(loop, false),
             "Invalid argument.");

  flush_remaining_tasks((ten_runloop_common_t *)loop);
}

ten_runloop_timer_t *ten_runloop_timer_create(const char *type,
                                              uint64_t timeout,
                                              uint64_t periodic) {
  const char *name = type ? type : get_default_impl();
  ten_runloop_timer_common_t *impl = NULL;
  const runloop_factory_t *factory = NULL;

  ten_thread_once(&runloop_once, setup_runloop_callback);

  factory = get_runloop_factory(name);
  if (!factory || !factory->create_timer) {
    return NULL;
  }

  impl = factory->create_timer();
  TEN_ASSERT(impl, "Failed to create %s timer.", name);
  if (!impl) {
    return NULL;
  }

  ten_signature_set(&impl->base.signature, TEN_RUNLOOP_TIMER_SIGNATURE);
  ten_sanitizer_thread_check_init_with_current_thread(&impl->base.thread_check);

  impl->base.loop = NULL;
  impl->base.timeout = timeout;
  impl->base.periodic = periodic;

  return &impl->base;
}

int ten_runloop_timer_set_timeout(ten_runloop_timer_t *timer, uint64_t timeout,
                                  uint64_t periodic) {
  TEN_ASSERT(timer, "Invalid argument.");
  TEN_ASSERT(ten_runloop_timer_check_integrity(timer, true),
             "Invalid argument.");

  ten_runloop_timer_common_t *impl = (ten_runloop_timer_common_t *)timer;

  if (!timer) {
    return -1;
  }

  impl->base.timeout = timeout;
  impl->base.periodic = periodic;
  return 0;
}

void ten_runloop_timer_stop(ten_runloop_timer_t *timer,
                            void (*stop_cb)(ten_runloop_timer_t *, void *),
                            void *arg) {
  TEN_ASSERT(timer, "Invalid argument.");
  TEN_ASSERT(ten_runloop_timer_check_integrity(timer, true),
             "Invalid argument.");

  if (!timer) {
    return;
  }

  ten_runloop_timer_common_t *impl = (ten_runloop_timer_common_t *)timer;
  if (!impl->stop) {
    return;
  }

  impl->stop_data = arg;
  impl->stop(timer, stop_cb);
}

void ten_runloop_timer_close(ten_runloop_timer_t *timer,
                             void (*close_cb)(ten_runloop_timer_t *, void *),
                             void *arg) {
  TEN_ASSERT(timer, "Invalid argument.");
  TEN_ASSERT(ten_runloop_timer_check_integrity(timer, true),
             "Invalid argument.");

  if (!timer) {
    return;
  }

  ten_runloop_timer_common_t *impl = (ten_runloop_timer_common_t *)timer;
  if (!impl->close) {
    return;
  }

  impl->close_data = arg;
  impl->close(timer, close_cb);
}

void ten_runloop_timer_destroy(ten_runloop_timer_t *timer) {
  TEN_ASSERT(timer, "Invalid argument.");
  TEN_ASSERT(ten_runloop_timer_check_integrity(timer, true),
             "Invalid argument.");

  ten_runloop_timer_common_t *impl = (ten_runloop_timer_common_t *)timer;

  if (!timer || !impl->destroy) {
    return;
  }

  impl->destroy(timer);
}

int ten_runloop_timer_start(ten_runloop_timer_t *timer, ten_runloop_t *loop,
                            void (*callback)(ten_runloop_timer_t *, void *),
                            void *arg) {
  TEN_ASSERT(timer, "Invalid argument.");
  TEN_ASSERT(ten_runloop_timer_check_integrity(timer, true),
             "Invalid argument.");
  TEN_ASSERT(loop && ten_runloop_check_integrity(loop, true),
             "Invalid argument.");

  ten_runloop_timer_common_t *impl = (ten_runloop_timer_common_t *)timer;
  int ret = -1;

  if (!timer || !impl->start) {
    return -1;
  }

  if (strcmp(timer->impl, loop->impl) != 0) {
    return -1;
  }

  impl->start_data = arg;
  ret = impl->start(timer, loop, callback);
  if (ret == 0) {
    timer->loop = loop;
  }

  return ret;
}
